### Opcache
`Opcache --- 一个用于缓存opcodes以提升PHP性能的Zend扩展，它的前身是Zend Optimizer Plus(简称 O+)，目前已经是PHP
非常重要的组成部分，它对于PHP性能的提升有着非常显著的作用。`

Opcache最主要的功能是缓存PHP代码编译生成的opcodes指令，工作原理比较容易理解：开启Opcache后，它将PHP的编译函数
zend_compile_file()替换为persistent_compile_file()，接管PHP代码的编译过程，当新的请求到达时，将调用
persistent_compile_file()进行编译。此时，Opcache首先检查是否有该文件的缓存，如果有则取出，然后进行一系列的验证，
如果最终发现缓存可用则直接返回，进入执行流程，如果没有缓存或者缓存已经失效，则重新调用系统默认的编译器进行编译，然后
将编译后的结果缓存下来，供下次使用。流程如下图

![image](https://github.com/deanisty/PHP7-internal-dissect/blob/master/images/opcache.png)

1. 初始化
Opcache是一个Zend扩展，在PHP的module startup阶段将调用Zend扩展的startup方法进行启动，Opcache定义的startup方法为accel_startup()，此时Opcache将
覆盖一些ZendVM的函数句柄，其中最重要的一个就是上面提到的zend_compile_file()，具体过程如下：

```c
// file: https://github.com/deanisty/php-src/blob/a394e1554c233c8ff6d6ab5d33ab79457b59522a/ext/opcache/ZendAccelerator.c#L2751
static int accel_startup(zend_extension *extension)
{
  ...
  // 分配共享内存
  switch (zend_shared_alloc_startup(ZCG(accel_directives).memory_consumption)) {
    case ALLOC_SUCCESS:
      // 初始化，并分配一个zend_accel_shared_globals结构
      if(zend_accel_init_shm() == FAILURE){
        accel_startup_ok = 0;
        return FAILURE;
      }
      break;
     ...
  }
  ...
  // 覆盖编译函数：zend_compile_file
  accelerator_orig_compile_file = zend_compile_file;
  zend_compile_file = persistent_compile_file;
  ...
}
```
同时，该过程还会分配一个zend_accel_shared_globals结构，这个结构通过共享内存分配，进程间共享，它保存着所有的opcodes缓存信息，以及一些统计数据，
ZCSG()宏操作的就是这个结构。这里暂时不关心其具体结构，只看hash这个成员，它就是存储opcodes缓存的地方。zend_accel_shared_globals->hash是一个哈希
表，但并不是PHP内核的HashTable，而是单独实现的一个，它的结构为zend_accel_hash:

```c
typedef struct _zend_accel_hash {
  zend_accel_hash_entry  **hash_table;
  zend_accel_hash_entry  *hash_entries;
  uint32_t               num_entries;       // 已有元素数
  uint32_t               max_num_entries;   // 总容量
  uint32_t               num_direct_entries;
}zend_accel_hash;
```

其中hash_table用于保存具体的value，它是一个数组，根据hash_value%max_num_entries索引存储位置，zend_accel_hash_entry为具体的存储元素，这是一个单
联表，用于解决哈希冲突；max_num_entries为最大元素数，也就是可以换成的文件数，这个值可以通过php.ini配置opcache.max_accelerated_files指定，默认值
为2000，但是实际max_num_entries并不是直接使用这个配置，而是根据配置值选择了一个固定规格，所有规格如下：

```c
static uint prime_number[] = 
{5,11,19,53,107,223,...,3907,...,524521,1048793};
```

实际选择的是可以满足opcache.max_accelerated_files的最小的一个，比如默认值2000最终使用的是3907，也就是可以缓存3907个文件，超过了这个值Opcache将不
再缓存。

2. 缓存的获取过程
编译一个脚本调用的是zend_compile_file(),此时将由Opcache的persistent_compile_file()处理，首先介绍Opcache中比较重要的几个配置：
* opcache.validate_timestamps:是否开启缓存有效期验证，默认值为1，表示开启，开启之后每隔opcache.revalidate_freq秒检查一次文件是否更新了；如果不开
启则不会检查，脚本修改了只能重启服务才能生效。opache.revalidate_freq默认为2s.
* opcache.revalidate_path:验证文件路径，默认值为0，表示关闭。默认情况下，opcodes缓存并不是通过完整的文件路径名称进行索引的，而是通过一个根据文件名
、当前所在目录、include_path生成的key，因此当编译的文件实际已经不存在了但是缓存还在的时候，就会使用已经失效的缓存，如果开启这个选项，将通过完整的文
件路径检索缓存，并且检查文件是否存在，而不再使用那个key。

```c
zend_op_array *persistent_compile_file(zend_file_handle *file_handle, int type)
{
  zend_persistent_script *persistent_script = NULL;
  ...
  //1) 获取缓存
    // 如果没有开启opcache.revalidate_path，则先根据key获取缓存
    if(!ZCG(accel_directives).revalidate_path) {
      key = accel_make_persistent_key(file_handle->filename, strlen(file_handle->filename), &key_length);
      if(!key) {
        return accelerator_orig_compile_file(file_handle, type);
      }
      // 获取缓存
      persistent_script = zend_accel_hash_str_find(&ZCSG(hash), key, key_length);
    }
    if(!persistent_script) {
      // 如果取不到缓存或开启了opcache.revalidate_path，则根据实际的文件路径查找缓存
      ...
      bucket = zend_accel_hash_find_entry(&ZCSG(hash), file_handle->opened_path);
      if(bucket) {
        persitent_scritp = (zend_persistent_script *)bucket->data;
        ...
      }
    }
    ...
    //2) 检查脚本是否更新过，开启opcache.validate_timestamps时
    if(persistent_script && ZCG(accel_directives).validate_timestamps) {
      // 每隔opcache.revalidate_freq秒检查一次文件是否更新，如果更新了则缓存失效
      if(validate_timestamp_and_record(persistent_script, file_handler) == FAILURE) 
      {
        persistent_script = NULL;
      }
    }
    // 校验缓存数据是否合法：根据Adler-32算法，类似crc的一个算法
    if(persistent_script && ZCG(accel_directives).consistency_checks && persistent_script->dynamic_members.hits%ZCG(accel_directives).consistency_checks == 0) {
      unsigned int checksum = zend_accel_script_checksum(persistent_script);
      // 检查校验和
      if(checksum != persistent_script->dynamic_members.checksum) {
        ...
        persistent_script = NULL;
      }
    }
    ...
    //3) 返回缓存或重新编译
    if(!persistent_script) { // 无缓存可用
      ...
      // 调用ZendVM默认的编译器进行编译
      persistent_script = opcache_compile_file(file_handler, type, key, key ? key_length : 0, &op_array);
      if(persistent_script) {
        // 将编译结果缓存
        persistent_script = cache_script_in_shared_memory(persistent_script, key, key ? key_length : 0, &from_shared_memory);
      }
      ...
    } else { // 有缓存
      ...
    }
    ...
    return zend_accel_load_script(persistent_script, from_shared_memory);
}
```

3. 缓存的生成
  如果没有缓存或者缓存失效了，则需要重新编译并缓存结果，其中编译过程由opcache_compile_file()完成，编译完成后调用cache_script_in_shared_memory()
进行缓存。首先看一下Opcache中缓存的结构zend_persistent_script，缓存的数据并不是仅仅是zend_op_array，还有函数、类的符号表:
```c
typedef struct _zend_persistent_script {
  zend_string *full_path; // 完整的脚本文件路径
  zend_op_array main_op_array; // 编译生成的zend_op_array
  HashTable function_table;
  HashTable class_table;
  ...
  accel_time_t timestamp; // 脚本更新时间
  zend_bool corrupted;
  zend_bool is_phar;
  
  void *mem; // zend_persistent_script内存的地址
  size_t size; // 共享内存的大小
  ...
  struct zend_persistent_script_dynamic_members {
    time_t last_used; // 上次使用时间
    zend_ulong hits; // 缓存命中次数
    unsigned int memory_consumption;
    unsigned int checksum; // 缓存的校验和
    time_t revalidate;
  } dynamic_members;
} zend_persistent_script;
```

PHP脚本在调用compile_file()编译完成后，将分配一个zend_persistent_script结构，然后将编译生成的数据转移到zend_persistent_script结构中。

```c
static zend_persistent_script *opcache_compile_file(zend_file_handle *file_handle, int type, char *key, unsigned int key_length, zend_op_array **op_array_p) {
  ...
  // 分配一个新的zend_persistent_script结构
  new_persistent_script = create_persistent_script();
  ...
  // 编译前替换CG(function_table)、EG(class_table)
  CG(function_table) = &ZCG(function_table);
  EG(class_table) = CG(class_table) = &new_persistent_script->class_table;
  ZVAL_UNDEF(&EG(user_error_handler));
  ...
  zend_try {
    ...
    // 调用compile_file()编译
    op_array = *op_array_p = accelerator_orig_compile_file(file_handle, type);
  } zend_catch {
  
  }
  ...
  // 将函数符号表转移到new_persistent_script->function_table
  zend_accel_move_user_functions(&ZCG(function_table), &new_persistent_script->function_table);
  new_persistent_script->main_op_array = *op_array;
  
  efree(op_array);
  ...
  return new_persistent_script;
}
```

此时生成的zend_persistent_script并不在共享内存上，调用cache_script_in_shared_memory()进行缓存时会重新复制到共享内存上，以便供其他进程使用。最终
保存到共享内存上的数据有：zend_persistent_script结构、脚本路径名称、脚本中定义的类、脚本中定义的函数、脚本的zend_op_array，内存结构如图。

![image](https://github.com/deanisty/PHP7-internal-dissect/blob/master/images/opcache1.png)

