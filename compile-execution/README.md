## 第五章
编译
------------------------------------------------------------

zend_execute_data和zend_op_array的关系：

![image](https://github.com/born4love/PHP7-internal-dissect/blob/master/compile-execution/zend_execute_data.png)

执行
------------------------------------------------------------
ZendVM执行器由`指令处理handler`与`调度器`组成：指令处理handler是每条opcode定义的具体处理过程，
根据操作数的不同类型，每种opcode可定义25个handler，执行时，由执行器调用相应的handler完成指令的处理；
调度器负责控制指令的执行，以及执行器的上下文切换，由调度器发起handler的执行。
