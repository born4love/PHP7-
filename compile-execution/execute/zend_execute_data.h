typedef struct _zend_execute_data zend_execute_data;
/**
ZendVM执行opcode指令前，首先会根据zend_op_array信息分配一个zend_execute_data结构，这个结构用来保存运行时信息，
包括当前执行的指令、局部变量、上下文调用信息等。
*/
struct _zend_execute_data {
  // 当前执行中的指令，等价于eip的作用，执行之初opline指向zend_op_array->opcodes指令集中的第一条指令，当执行完当前指令后，该值会指向下一条指令
  const zend_op     *opline;
  // 当前执行的上下文 ???
  zend_execute_data *call;
  // 返回值 执行完之后会把返回值设置到这个地址
  zval              *return_value;
  zend_function     *func;
  zval              This;
  zend_class_entry  *called_scope;
  // 调用上下文,当调用函数或者include时，会重新分配一个zend_execute_data，并把当前的zend_execute_data保存到被调函数的pre_execute_data
  // 调用完成后再设置回来，相当于C语言中的call/ret指令的作用
  zend_execute_data *prev_execute_data;
  // 全局符号表
  zend_array        *symbol_table;
  void              **run_time_cache;
  // 就是zend_op_array->literals
  zval              *literals;
}

/**
zend_execute_data还有一个重要的组成部分：动态变量区。
CV变量、临时变量按照编号依次分配在zend_execute_data结构的末尾，
根据具体的CV变量、临时变量数（即zend_op_array->last_var和zend_op_array->T）确定内存的大小。
*/
