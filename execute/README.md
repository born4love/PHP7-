### 执行流程
zend_execute_data是ZendVM执行过程中非常重要的一个数据结构，它主要有两个作用：记录运行时信息、分配动态变量内存。
```c
ZEND_API void zend_execute(zend_op_array *op_array, zval *return_value)
{
  zend_execute_data * execute_data;
  ...
  // 分配zend_execute_data
  execute_data = zend_vm_stack_push_call_frame(ZEND_CALL_TOP_CODE,
    (zend_function*)op_array, 0, zend_get_called_scope(EG(current_execute_data)), 
    zend_get_this_object(EG(current_execute_data))
  );
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
  通过zend_vm_stack_push_call_frame()方法分配一块用于当前作用域的内存空间，也就是zend_execute_data,分配时会根据zend_op_array->last_var、           zend_op_array->T计算出动态变量区的内存大小，随zend_execute_data一起分配。
  在分配zend_execute_data时，传入了一个ZEND_CALL_TOP_CODE参数，这个值是用来标识执行器的调用类型的，因为PHP主代码执行、调用用户自定义函数、
  调用内部函数、include/require/eval的执行流程都是类似的，都会分配zend_execute_data，因此，zend_execute_data通过This这个成员，将调用类型
  保存下来，This类型是zval，具体保存在This.u1.v.reserved中。
```c
typedef enum _zend_call_kind {
  ZEND_CALL_NESTED_FUNCTION,  /* 用户自定义函数 */
  ZEND_CALL_NESTED_CODE,      /* include/require/eval */
  ZEND_CALL_TOP_FUNCTION,     /* 调用内部函数 */
  ZEND_CALL_TOP_CODE,         /* 调用主脚本 */
}zend_call_kind;
```
#### 初始化zend_execute_data
这一步主要是初始化zend_execute_data中的一些成员，比如opline、return_value。另外还有一个比较重要的操作：zend_attach_symbol_table(),这是
一个哈希表，用于存储全局变量。需要注意的是，主代码中所有CV变量都是全局变量，尽管这些变量对于主代码而言是局部变量，但是对于其他函数，它们就是
全局变量，可以在函数中通过global关键字访问。
```c
// file: zend_execute.c
static zend_always_inline void i_init_execute_data(zend_execute_data *execute_data, zend_op_array *op_array, zval *return_value)
{
  EX(opline) = op_array->opcodes;
  EX(call) = NULL;
  EX(return_value) = return_value;
  
  if(UNEXPECTED(EX(symbol_table) != NULL)) {
    ...
    // 添加全局变量
    zend_attach_symbol_table(execute_data);
  }else {
    ...
  }
  EX_LOAD_RUN_TIME_CACHE(op_array);
  EX_LOAD_LITERALS(op_array);
  
  EG(current_execute_data) = execute_data;
  ZEND_VM_INTERRUPT_CHECK();
}
```
经过这一步的处理后，zend_execute_data->opline指向了第一条指令，同时将zend_execute_data->literals指向了zend_op_array->literals，便于快速
访问，完成这些设置之后，最后将EG(current_execute_data)指向zend_execute_data。现在，执行前的准备工作都已经完成了。
#### 执行
ZendVM的执行调度器就是一个while循环，在这个循环中依次调用Opline指令的handler，然后依据handler的返回决定下一步的动作。执行调度器为
zend_execute_ex，这是函数指针，默认为execute_ex()，可以通过扩展进行覆盖。GCC低于4.8的情况下，这个调度器展开后如下：
```c
ZEND_API void execute_ex(zend_execute_data *ex)
{
  zend_execute_data *execute_data = ex;
  while(1) {
    int ret;
    // 执行当前指令
    if(UNEXPECTED((ret = ((opcode_handler_t)execute_data->opline->handler)(execute_data)) != 0)) {
      if(EXPECTED(ret > 0)){
        execute_data = EG(current_execute_data);
      }else{
        return;
      }
    }
  }
}
```
执行的第一条opcode为ZEND_ASSIGN，即$a = 123，该指令由ZEND_ASSIGN_SPEC_CV_CONST_HANDLER()处理，首先根据操作数1,2取出赋值的变量与变量值，其中
变量值为CONST类型，保存在literals中，通过EX_CONSTANT(opline->op2)获取它的值。$a为CV变量，分配在Zend_execute_data动态变量区，通过
_get_zval_ptr_cv_undef_BP_VAR_W()取到这个变量的地址，然后将变量值复制到CV变量即可。

```c
static int ZEND_ASSIGN_SPEC_CV_CONST_HANDLER(zend_execute_data *execute_data)
{
  // USE OPLINE
  const zend_op *opline = execute_data->opline;
  ...
  value = EX_CONSTANT(opline->op2);
  variable_ptr = _get_zval_ptr_cv_undef_BP_VAR_W(execute_data, opline->op1.var);
  ...
  // 赋值复制
  value = zend_assign_to_variable(variable_ptr, value, IS_CONST);
  ...
  // ZEND_VM_NEXT_OPCODE_CHECK_EXCEPTION()
  excute_data->opline = execute_data->opline + 1;
  return 0;
}

```
  执行完成后会更新opline,指向下一条指令，然后返回调度器。这里会有几个不同的动作，调度器根据不同的返回值决定下一个动作。
  ```c
  #define ZEND_VM_CONTINUE()  return 0
  #define ZEND_VM_ENTER() return 1
  #define ZEND_VM_LEAVE() return 2
  #define ZEND_VM_RETURN() return -1
  ```
  
  ZEND_VM_CONTINUE()表示继续执行下一条opcode；ZEND_VM_ENTER()/ZEND_VM_LEAVE()是调用函数时的动作，普通模式下ZEND_VM_ENTER()实际上就是返回1，
  然后execute_ex()会将execute_data切换到被调用函数的结构上。对应的，在函数调用完成后，ZEND_VM_LEAVE()会return 2，再将execute_data切换至原来的
  结构：ZEND_VM_RETURN()表示执行完成，返回-1给execute_ex(),比如exit,这时候execute_ex()会退出执行。
    执行流程的最后一条指令是ZEND_RETURN，这条执行会有几个非常关键的处理，1)设置返回值，zend_execute()执行时传入了一个return_value，这个过程相当
  于赋值，ZendVM会把返回值赋值给这个地址；2)清理动态变量区，对zend_execute_data上的CV、VAR、TMP_VAR进行清理，需要注意的是，这里的清理不包括全局
  变量，也就是主脚本、include的调用并不会在这一步清理全局变量（即主代码里的“局部变量”）进行清理；3)切换至调用前的zend_execute_data，相当于汇编中
  的ret指令的作用。
  ```c
  static ZEND_OPCODE_HANDLER_RET ZEND_FASTCALL zend_leave_helper_SPEC(ZEND_OPCODE_HANDLER_ARGS)
  {
    zend_execute_data *old_execute_data;
    // 获取zend_execute_data的call_info
    uint32_t call_info = EX_CALL_INFO();
    
    // 调用用户自定义函数
    if(EXPECTED(ZEND_CALL_KIND_EX(call_info)) == ZEND_CALL_NESTED_FUCTION)) {
      ...
    }
    if((EXPECTED(ZEND_CALL_KIND_EX(call_info) & ZEND_CALL_TOP) == 0)) {
      ...
    } else {
      if (ZEND_CALL_KIND_EX(call_info) == ZEND_CALL_TOP_FUNCTION) {
        ...
      } else /* if (call_kind == ZEND_CALL_TOP_CODE) */ {
        zend_array *symbol_table = EX(symbol_table);
        
        zend_detach_symbol_table(execute_data);
        old_execute_data = EX(prev_execute_data);
        ...
        // 还原 zend_execute_data
        EG(current_execute_data) = EX(prev_execute_data);
      }
      ZEND_VM_RETURN();
    }
  }
  ```
#### 释放zend_execute_data
  在所有的指令执行完成后，将调用zend_vm_stack_free_call_frame()释放zend_execute_data.为了避免频繁的申请、释放zend_execute_data，这里会对
  zend_execute_data进行缓存，释放时并不是立即归还底层的内存系统（即Zend内存池），而是暂时保留，下次使用时直接分配，缓存位置为EG(vm_stack)。
