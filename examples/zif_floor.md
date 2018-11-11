## PHP_FUNCTION(floor)

>php --version : PHP 7.3.0-dev

>位置：php-src/ext/standard/math.c

```PHP
<?php
// php7/test/test_floor.php

$a = 100 * 19.9;
$fa = floor($a);

echo $fa;

// 1989
```

本文档致力于研究这个问题

#### 相关知识点

* zend_execute_data https://github.com/deanisty/PHP7-internal-dissect/blob/master/_datastructure/zend_execute_data.h
* zend_string     https://github.com/deanisty/PHP7-internal-dissect/blob/master/_datastructure/zend_string.md

## GDB

#### 启动 gdb 

```shell
gdb bin/php
```

#### 寻找合适位置设置断点

```shell
hostname-xxx:/usr/local/php7$bin/php -dvld.active=1 test/test_floor.php
Finding entry points
Branch analysis from position: 0
1 jumps found. (Code = 62) Position 1 = -2
filename:       /usr/local/php7/test/test_floor.php
function name:  (null)
number of ops:  8
compiled vars:  !0 = $a, !1 = $fa
line     #* E I O op                           fetch          ext  return  operands
-------------------------------------------------------------------------------------
   3     0  E >   ASSIGN                                                   !0, 1990
   5     1        INIT_FCALL                                               'floor'
         2        SEND_VAR                                                 !0
         3        DO_ICALL                                         $3      
         4        ASSIGN                                                   !1, $3
   7     5        CONCAT                                           ~5      !1, '%0A'
         6        ECHO                                                     ~5
   8     7      > RETURN                                                   1

branch: #  0; line:     3-    8; sop:     0; eop:     7; out0:  -2
path #1: 0,
```

>php-vld extension: https://github.com/derickr/vld
>打印出opcode执行顺序

根据执行流程可以看出调用 floor 方法的 opcode 是 INIT_FCALL，而执行 floor 的是 DO_ICALL 找到他们定义的地方 

```C
// php-src/Zend/zend_vm_opcodes.h
...
#define ZEND_INIT_FCALL_BY_NAME               59
#define ZEND_DO_FCALL                         60
#define ZEND_INIT_FCALL                       61
...

#define ZEND_INIT_DYNAMIC_CALL               128
#define ZEND_DO_ICALL                        129
#define ZEND_DO_UCALL                        130
#define ZEND_DO_FCALL_BY_NAME                131
...

// 这只是一个数值 代表 opcode 的编号，vld 扩展输出的 opcodes 在 zend 引擎编译期间会加上 ZEND_ 前缀
// 执行期间会根据这里定义的数值找到对应的执行 handler 方法
```
找到本例中执行 ZEND_DO_ICALL 的 handler 

```C
// php-src/Zend/zend_vm_execute.h
// 根据这个 handler 的名字大致可以看出它是调用内部函数并且需要返回值的  ICALL(internal call) return value used 
// 在它上面还有一个同胞 handler ： ZEND_DO_ICALL_SPEC_RETVAL_UNUSED_HANDLER

static ZEND_VM_HOT ZEND_OPCODE_HANDLER_RET ZEND_FASTCALL ZEND_DO_ICALL_SPEC_RETVAL_USED_HANDLER(ZEND_OPCODE_HANDLER_ARGS)
{
	USE_OPLINE
  // 获取传入参数 execute_data 的 call 成员 (表示即将被调用的 execute_data 类似 C 语言中的下一个执行栈) 
	zend_execute_data *call = EX(call); // EX 宏是用于访问 execute_data 的成员
  // 获取被调用上下文里的 func 成员 表示被调用的函数 这里指 zif_floor 方法
	zend_function *fbc = call->func;
	zval *ret;
	zval retval;

	SAVE_OPLINE();
  // 切换上下文操作 上一个上下文切换成即将执行
	EX(call) = call->prev_execute_data;

  // 被执行的上下文变成上一个
	call->prev_execute_data = execute_data;
  // 全局变量中的当前执行的上下文指向 call
	EG(current_execute_data) = call; // EG 宏就是用于访问 executor_globals 的某个成员

	ret = 1 ? EX_VAR(opline->result.var) : &retval;
	ZVAL_NULL(ret);

	fbc->internal_function.handler(call, ret);

#if ZEND_DEBUG
	if (!EG(exception) && call->func) {
		ZEND_ASSERT(!(call->func->common.fn_flags & ZEND_ACC_HAS_RETURN_TYPE) ||
			zend_verify_internal_return_type(call->func, ret));
		ZEND_ASSERT((call->func->common.fn_flags & ZEND_ACC_RETURN_REFERENCE)
			? Z_ISREF_P(ret) : !Z_ISREF_P(ret));
	}
#endif

	EG(current_execute_data) = execute_data;
	zend_vm_stack_free_args(call);
	zend_vm_stack_free_call_frame(call);

	if (!1) {
		zval_ptr_dtor(ret);
	}

	if (UNEXPECTED(EG(exception) != NULL)) {
		zend_rethrow_exception(execute_data);
		HANDLE_EXCEPTION();
	}

	ZEND_VM_SET_OPCODE(opline + 1);
	ZEND_VM_CONTINUE();
}

```

设置断点 

```shell
(gdb)run test/test_floor.php
(gdb)b ZEND_DO_ICALL_SPEC_RETVAL_USED_HANDLER
```

查看堆栈

```
(gdb) bt
#0  ZEND_DO_ICALL_SPEC_RETVAL_USED_HANDLER (execute_data=0x7ffff661e030) at /usr/local/php7/php-src/Zend/zend_vm_execute.h:673
#1  0x00000000007b32e7 in execute_ex (ex=0x7ffff661e030) at /usr/local/php7/php-src/Zend/zend_vm_execute.h:54789
#2  0x00000000007b33fa in zend_execute (op_array=0x7ffff667d300, return_value=0x0) at /usr/local/php7/php-src/Zend/zend_vm_execute.h:59987
#3  0x00000000006d22a1 in zend_execute_scripts (type=8, retval=0x0, file_count=3) at /usr/local/php7/php-src/Zend/zend.c:1566
#4  0x0000000000641521 in php_execute_script (primary_file=0x7fffffffe280) at /usr/local/php7/php-src/main/main.c:2467
#5  0x00000000007b5a8e in do_cli (argc=2, argv=0xc36a50) at /usr/local/php7/php-src/sapi/cli/php_cli.c:1011
#6  0x00000000007b689c in main (argc=2, argv=0xc36a50) at /usr/local/php7/php-src/sapi/cli/php_cli.c:1404

```

ZEND_DO_ICALL_SPEC_RETVAL_USED_HANDLER 方法接收的参数是 execute_data=0x7ffff661e030
表示本次要执行的上下文，通过 print 命令看出是 zend_execute_data 结构

```shell
(gdb) p execute_data 
$4 = (zend_execute_data *) 0x7ffff661e030
```
单步执行

```shell
(gdb) n
682		call->prev_execute_data = execute_data;
(gdb) 
683		EG(current_execute_data) = call;
(gdb) 
685		ret = 1 ? EX_VAR(opline->result.var) : &retval;
```

打印 ret 都是空，此变量传递给被调用的方法作为返回值的存储地址

```shell
(gdb) p ret
$12 = (zval *) 0x7ffff661e0b0
(gdb) p *ret
$13 = {value = {lval = 0, dval = 0, counted = 0x0, str = 0x0, arr = 0x0, obj = 0x0, res = 0x0, ref = 0x0, ast = 0x0, zv = 0x0, ptr = 0x0, ce = 0x0, func = 0x0, ww = {w1 = 0, w2 = 0}}, u1 = {v = {type = 0 '\000', type_flags = 0 '\000', u = {call_info = 0, extra = 0}}, 
    type_info = 0}, u2 = {next = 0, opline_num = 0, lineno = 0, num_args = 0, fe_pos = 0, fe_iter_idx = 0, access_flags = 0, property_guard = 0, extra = 0}}
    
# ZVAL_NULL(ret); 将 ret 重新初始化为空用于存储返回值
```

再执行

```shell
(gdb) n
688		fbc->internal_function.handler(call, ret);

# 查看执行的函数类型 
# internal_function 成员表示当前函数是内部函数 
# 真正执行的函数经过 zend 解析在 internal_function->handler 成员中

(gdb) ptype call->func
type = union _zend_function {
    zend_uchar type;
    uint32_t quick_arg_flags;
    struct {
        zend_uchar type;
        zend_uchar arg_flags[3];
        uint32_t fn_flags;
        zend_string *function_name;
        zend_class_entry *scope;
        union _zend_function *prototype;
        uint32_t num_args;
        uint32_t required_num_args;
        zend_arg_info *arg_info;
    } common;
    zend_op_array op_array;
    zend_internal_function internal_function;
} *
(gdb)

(gdb) ptype call->func->internal_function 
type = struct _zend_internal_function {
    zend_uchar type;
    zend_uchar arg_flags[3];
    uint32_t fn_flags;
    zend_string *function_name;
    zend_class_entry *scope;
    zend_function *prototype;
    uint32_t num_args;
    uint32_t required_num_args;
    zend_internal_arg_info *arg_info;
    zif_handler handler;
    struct _zend_module_entry *module;
    void *reserved[6];
}
(gdb)
```

查看 fbc->internal_function.handler

```shell
(gdb) p call->func->internal_function 
$21 = {type = 1 '\001', arg_flags = "\000\000", fn_flags = 256, function_name = 0xc689b0, scope = 0x0, prototype = 0x0, num_args = 1, required_num_args = 1, arg_info = 0x8ba798, handler = 0x5c0a02 <zif_floor>, module = 0xc61c10, reserved = {0x0, 0x0, 0x0, 0x0, 0x0, 
    0x0}}
    
# 可以看到 handler 地址处存储的是 zif_floor 方法

(gdb) p call->func->common->function_name
$22 = (zend_string *) 0xc689b0

# func 的函数名称的地址在 0xc689b0 是一个 zend_string
# 使用 Examining Memory 方法打印出变量值
(gdb) x/s call->func->common->function_name->val
0xc689c8:	 "floor"
(gdb) 

# 可以看到这里需要执行的函数名是 floor

(gdb) ptype zif_handler
type = void (*)(zend_execute_data *, zval *)

# zif_handler 的类型是一个函数指针，返回任意类型，接收两个参数，一个是执行的上下文信息，一个是返回值地址

```







