/**
opline是编译生成的单条指令，所有的指令组合成了zend_op_array
除了指令集合，zend_op_array保存了很多编译生成的关键数据，比如字面量存储区就在zend_op_array中。
对于ZendVM而言，zend_op_array就是可执行数据，每个PHP脚本都会被编译未独立的zend_op_array结构。
*/
// file: zend_compile.h
struct _zend_op_array{
  // common是普通函数或类成员方法对应的opcodes快速访问时使用的字段，后面分析PHP函数实现时会详细讲
  ...
  // 引用计数？？？
  uint32_t  *refcount;
  
  uint32_t  this_var;
  
  uint32_t  last;
  // opcode指令数组
  zend_op *opcodes;
  // PHP代码里的变量数：op_type = IS_CV 的变量
  int last_var;
  // 临时变量数：op_type = IS_TMP_VAR | IS_VAR 的变量
  uint32_t T;
  // PHP变量名数组：在ast编译期间配合 last_var 来确定各个变量的编号
  zend_string **vars;
  // 静态变量符号表:通过 static声明的
  HashTable *static_variables;
  ...
  // 字面量的数量
  int last_literal;
  // 字面量（常量）数组，都是在PHP代码定义的一些值
  zval *literals;
  // 运行时缓存的大小
  int cache_size;
  // 运行时缓存，主要用来缓存一些操作数以便快速获取数据
  void **run_time_cache;
  
  void *reserved[ZEND_MAX_RESERVER_RESOURCE];
}
