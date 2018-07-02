### 执行流程
zend_execute_data是ZendVM执行过程中非常重要的一个数据结构，它主要有两个作用：记录运行时信息、分配动态变量内存。
```c
ZEND_API void zend_execute(zend_op_array *op_array, zval *return_value)
{
  zend_execute_data * execute_data;
  ...
  // 分配zend_execute_data
  execute_data = zend_vm_stack_push_call_frame(ZEND_CALL_TOP_CODE,
    (zend_function*)op_array, 0, zend_get_called_scope(EG(current_execute_data)), zend_get_this_object(EG(current_execute_data)));
    ...
  // execute_data->prev_execute_data = EG(current_execute_data)
  EX(prev_execute_data) = EG(current_execute_data);
  // 初始化execute_data
  i_init_execute_data(execute_data, op_array, return_value);
  // 执行Opcode
  zend_execute_ex(execute_data);
  zend_vm_stack_free_call_frame(execute_data);
}
```

#### 分配zend_execute_data
  通过zend_vm_stack_push_call_frame()方法分配一块用于当前作用域的内存空间，也就是zend_execute_data,分配时会根据zend_op_array->last_var、zend_op_array->T计算出动态变量区的内存大小，随zend_execute_data一起分配。
  在分配zend_execute_data时，传入了一个ZEND_CALL_TOP_CODE参数，这个值是用来标识执行器的调用类型的，因为
#### 初始化zend_execute_data
#### 执行
#### 释放zend_execute_data
