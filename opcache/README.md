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
* opcache.validate_timestamps:
* opcache.revalidate_path:

3. 缓存的生成
