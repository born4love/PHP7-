## PHP_FUNCTION(floor)

>php --version : PHP 7.0.12

>位置：php-src/ext/standard/math.c

```PHP
<?php
/data/php7/test/test.php

$a = 100 * 19.9;

var_dump($a);

$fa = floor($a);

var_dump($fa);

// output:
// float(1990)
// float(1989)
```

本文档致力于研究这个问题

#### 相关知识点

* zend_execute_data https://github.com/deanisty/PHP7-internal-dissect/blob/master/_datastructure/zend_execute_data.h
* zend_string       https://github.com/deanisty/PHP7-internal-dissect/blob/master/_datastructure/zend_string.md
* php float types   http://php.net/manual/en/language.types.float.php

## GDB

#### 寻找合适位置设置断点

```shell
root@56d745620b72:/data/php7/test# php -dvld.active=1 test.php
Finding entry points
Branch analysis from position: 0
1 jumps found. (Code = 62) Position 1 = -2
filename:       /data/php7/test/test.php
function name:  (null)
number of ops:  12
compiled vars:  !0 = $a, !1 = $fa
line     #* E I O op                           fetch          ext  return  operands
-------------------------------------------------------------------------------------
   3     0  E >   ASSIGN                                                   !0, 1990
   4     1        INIT_FCALL                                               'var_dump'
         2        SEND_VAR                                                 !0
         3        DO_ICALL                                                 
   5     4        INIT_FCALL                                               'floor'
         5        SEND_VAR                                                 !0
         6        DO_ICALL                                         $4      
         7        ASSIGN                                                   !1, $4
   6     8        INIT_FCALL                                               'var_dump'
         9        SEND_VAR                                                 !1
        10        DO_ICALL                                                 
        11      > RETURN                                                   1

branch: #  0; line:     3-    6; sop:     0; eop:    11; out0:  -2
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
// php-src/Zend/zend_compile.h

#define EX(element)  ((execute_data)->element)

// php-src/Zend/zend_vm_execute.h

static ZEND_OPCODE_HANDLER_RET ZEND_FASTCALL ZEND_DO_ICALL_SPEC_HANDLER(ZEND_OPCODE_HANDLER_ARGS)
{
	USE_OPLINE
	zend_execute_data *call = EX(call); // 从全局 execute_data 中获取当前需要执行的 zend_excute_data 结构体
	zend_function *fbc = call->func;  // 从结构体中获取需要调用的函数 也就是我们需要运行的 floor 函数
	zval *ret;

	SAVE_OPLINE();
	EX(call) = call->prev_execute_data; // 调用指针移动到上一个结构体

	call->prev_execute_data = execute_data; // 指向全局结构体
	EG(current_execute_data) = call; 

	ret = EX_VAR(opline->result.var);
	ZVAL_NULL(ret);
	Z_VAR_FLAGS_P(ret) = 0;

	fbc->internal_function.handler(call, ret);  // 调用内部函数的 handler 执行函数

...

	EG(current_execute_data) = call->prev_execute_data;
	zend_vm_stack_free_args(call);
	zend_vm_stack_free_call_frame(call);

	if (!RETURN_VALUE_USED(opline)) {
		zval_ptr_dtor(EX_VAR(opline->result.var));
	}

...
	ZEND_VM_INTERRUPT_CHECK();
	ZEND_VM_NEXT_OPCODE();
}

```
该方法具体流程不用细看，直接设置断点 

```shell
root@56d745620b72:/data/php7# gdb php
(gdb)b ZEND_DO_ICALL_SPEC_HANDLER
(gdb)run test/test_floor.php
```

查看堆栈

```
(gdb) bt
#0  ZEND_DO_ICALL_SPEC_HANDLER () at /data/php7/php-src-PHP-7.0.12/Zend/zend_vm_execute.h:572
#1  0x00005555558b2218 in execute_ex (ex=0x7ffff7014030) at /data/php7/php-src-PHP-7.0.12/Zend/zend_vm_execute.h:414
#2  0x00005555558b22fc in zend_execute (op_array=0x7ffff7083000, return_value=0x0) at /data/php7/php-src-PHP-7.0.12/Zend/zend_vm_execute.h:458
#3  0x000055555585492f in zend_execute_scripts (type=8, retval=0x0, file_count=3) at /data/php7/php-src-PHP-7.0.12/Zend/zend.c:1427
#4  0x00005555557c46d6 in php_execute_script (primary_file=0x7fffffffd390) at /data/php7/php-src-PHP-7.0.12/main/main.c:2494
#5  0x000055555591a133 in do_cli (argc=2, argv=0x555555d29af0) at /data/php7/php-src-PHP-7.0.12/sapi/cli/php_cli.c:974
#6  0x000055555591b242 in main (argc=2, argv=0x555555d29af0) at /data/php7/php-src-PHP-7.0.12/sapi/cli/php_cli.c:1344

```

堆栈中显示 ZEND_DO_ICALL_SPEC_HANDLER 方法并未接收任何参数
断点处第一行执行了从全局 execute_data 中获取 call 成员

```shell
Breakpoint 1, ZEND_DO_ICALL_SPEC_HANDLER () at /data/php7/php-src-PHP-7.0.12/Zend/zend_vm_execute.h:572
572		zend_execute_data *call = EX(call);
```
单步执行

```shell
(gdb) n
573		zend_function *fbc = call->func;
(gdb) 
576		SAVE_OPLINE();
(gdb) 
577		EX(call) = call->prev_execute_data;
(gdb) 
579		call->prev_execute_data = execute_data;
(gdb) 
580		EG(current_execute_data) = call;
(gdb) n
582		ret = EX_VAR(opline->result.var);
(gdb) 
583		ZVAL_NULL(ret);
(gdb) 
584		Z_VAR_FLAGS_P(ret) = 0;
```

打印 ret 都是空，此变量传递给被调用的方法作为返回值的存储地址

```shell
(gdb) p ret
$1 = (zval *) 0x7ffff70140c0
(gdb) p *ret
$2 = {value = {lval = 0, dval = 0, counted = 0x0, str = 0x0, arr = 0x0, obj = 0x0, res = 0x0, ref = 0x0, ast = 0x0, zv = 0x0, ptr = 0x0, ce = 0x0, func = 0x0, ww = {w1 = 0, w2 = 0}}, u1 = {v = {type = 1 '\001', type_flags = 0 '\000', 
      const_flags = 0 '\000', reserved = 0 '\000'}, type_info = 1}, u2 = {var_flags = 0, next = 0, cache_slot = 0, lineno = 0, num_args = 0, fe_pos = 0, fe_iter_idx = 0}}
(gdb) 
    
# ZVAL_NULL(ret); 将 ret 重新初始化为空用于存储返回值
```

再执行

```shell
(gdb) n
688		fbc->internal_function.handler(call, ret);

# 查看执行的函数类型 
# internal_function 成员表示当前函数是内部函数 
# 真正执行的函数经过 zend 解析在 internal_function->handler 成员中

(gdb) ptype fbc
type = union _zend_function {
    zend_uchar type;
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

(gdb) ptype fbc->internal_function 
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
    void (*handler)(zend_execute_data *, zval *);
    struct _zend_module_entry *module;
    void *reserved[4];
}
(gdb) 
```

查看 fbc->internal_function.handler

```shell
(gdb) p fbc->internal_function.handler 
$4 = (void (*)(zend_execute_data *, zval *)) 0x5555557669e4 <zif_floor>
(gdb) 
    
# 可以看到 handler 地址处存储的是 zif_floor 方法名
# 同时可以从 fbc->internal_function->function_name 获取原始方法名

(gdb) p fbc->internal_function->function_name 
$17 = (zend_string *) 0x555555d56540
(gdb) 

# function_name 的地址在 0x555555d56540 是一个 zend_string
# 使用 Examining Memory 方法打印出变量值
(gdb) x/s call->func->common->function_name->val
0xc689c8:	 "floor"
(gdb) 

# 可以看到这里执行的函数名是 floor 但是真正执行的 handler 是 zif_floor 函数

```

为了看清楚 zif_floor 内部进行了哪些操作，我们设置一个断点

```shell
(gdb) b zif_floor 
Breakpoint 2 at 0x5c0a1b: file /usr/local/php7/php-src/ext/standard/math.c, line 340.
(gdb) n

Breakpoint 2, zif_floor (execute_data=0x7ffff661e0e0, return_value=0x7ffff661e0b0) at /usr/local/php7/php-src/ext/standard/math.c:340
340		ZEND_PARSE_PARAMETERS_START(1, 1)
(gdb) 
```

查看当前栈帧

```shell
// info frame
(gdb) info f
Stack level 0, frame at 0x7fffffffac90:
 rip = 0x5c0a1b in zif_floor (/usr/local/php7/php-src/ext/standard/math.c:340); saved rip 0x73495c
 called by frame at 0x7ffffffface0
 source language c.
 Arglist at 0x7fffffffac80, args: execute_data=0x7ffff661e0e0, return_value=0x7ffff661e0b0
 Locals at 0x7fffffffac80, Previous frame's sp is 0x7fffffffac90
 Saved registers:
  rbp at 0x7fffffffac80, rip at 0x7fffffffac88
```

查看 zif_floor 参数

```shell
(gdb) n
352			Z_PARAM_ZVAL(value)
(gdb) p value
$1 = (zval *) 0x0
(gdb) n
353		ZEND_PARSE_PARAMETERS_END();
(gdb) p value
$2 = (zval *) 0x7ffff7014160
(gdb) p *value
$3 = {value = {lval = 4656466928003448831, dval = 1989.9999999999998, counted = 0x409f17ffffffffff, str = 0x409f17ffffffffff, arr = 0x409f17ffffffffff, obj = 0x409f17ffffffffff, res = 0x409f17ffffffffff, ref = 0x409f17ffffffffff, 
    ast = 0x409f17ffffffffff, zv = 0x409f17ffffffffff, ptr = 0x409f17ffffffffff, ce = 0x409f17ffffffffff, func = 0x409f17ffffffffff, ww = {w1 = 4294967295, w2 = 1084168191}}, u1 = {v = {type = 5 '\005', type_flags = 0 '\000', 
      const_flags = 0 '\000', reserved = 0 '\000'}, type_info = 5}, u2 = {var_flags = 0, next = 0, cache_slot = 0, lineno = 0, num_args = 0, fe_pos = 0, fe_iter_idx = 0}}
(gdb) 
```

很遗憾，floor 函数接收唯一的参数 value ，而它是一个浮点型的值，所以在 zval 结构中使用 zval->value->dval 来存储，但是我们发现在 floor 函数真正处理
之前入参的值已经是 `1989.9999999999998` 了，也就是说这个值已经不对了，因此，输出 1989 不是 floor 函数的问题。

#### 揭晓谜底了

其实 19.9 这个小数在代码编译阶段就已经转成二进制的浮点数形式，但是二进制表示小数有个问题，就是有些十进制可以精确表示的小数，转成二进制之后就无法精确
表示，为什么呢，因为大部分计算机系统的浮点数表示法都遵循 IEEE 754 标准，这个表示方法的具体内容可以看这个[IEEE standard 754](https://steve.hollasch.net/cgindex/coding/ieeefloat.html) ，所以二进制和十进制之间表示小数根本就无法完全精确，所以任何小数变成二进制之后都可能
存在精度缺失的问题。

可以查看这个[在线浮点数转二进制](https://www.binaryconvert.com/result_float.html?decimal=049057046057)工具，随便输两个小数试试，有些是无法精确
还原为十进制的。




