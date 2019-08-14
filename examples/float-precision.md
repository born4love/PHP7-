### PHP 的浮点数精度

```PHP

$a = 0.7;

$b = 0.1;

$c = floor(($a + $b) * 10);

echo $a, "\t", $b, "\t", $c, "\n";

// output :  0.7	0.1	7
```

#### 两个问题

* 为什么直接 echo 没有问题

* 为什么 执行运算之后有问题

##### 操作环境

本文档运行的所有例子均在 docker 环境下部署的 php 容器中

```SHELL
  
root@56d745620b72:/data/php7# cat /etc/issue
Ubuntu 18.04.2 LTS \n \l

root@56d745620b72:/data/php7# php -v
PHP 7.0.12 (cli) (built: Jun 25 2019 07:01:47) ( NTS DEBUG )
Copyright (c) 1997-2016 The PHP Group
Zend Engine v3.0.0, Copyright (c) 1998-2016 Zend Technologies
root@56d745620b72:/data/php7# 

```



##### 问题一

首先查看 echo 的 opcode 

测试文件：

```PHP
// test/test_echo.php
<?php

$a = 0.7;

echo $a;
```
运行 vld 扩展

```SHELL
root@56d745620b72:/data/php7# php -dvld.active=1 test/test_echo.php 
Finding entry points
Branch analysis from position: 0
1 jumps found. (Code = 62) Position 1 = -2
filename:       /data/php7/test/test_echo.php
function name:  (null)
number of ops:  3
compiled vars:  !0 = $a
line     #* E I O op                           fetch          ext  return  operands
-------------------------------------------------------------------------------------
   3     0  E >   ASSIGN                                                   !0, 0.7
   5     1        ECHO                                                     !0
         2      > RETURN                                                   1

branch: #  0; line:     3-    5; sop:     0; eop:     2; out0:  -2
path #1: 0,
```

可以看到，脚本运行后 首先执行了 ASSIGN 操作，将 0.7 赋值给第0个变量也就是 $a，然后执行了 ECHO 操作，输出了第0个变量的值，最后执行了 RETURN。
这里的操作都是 PHP 编译后的 opcode，每一个 opcode 在PHP内部都定义了一个唯一的数字编码，相应的源码文件为`php-src/Zend/zend_vm_opcodes.h`，
每个 opcode 都有 'ZEND_'前缀：

```PHP
...

#define ZEND_POST_DEC                         37
#define ZEND_ASSIGN                           38
#define ZEND_ASSIGN_REF                       39
#define ZEND_ECHO                             40
#define ZEND_JMP                              42
#define ZEND_JMPZ                             43
#define ZEND_JMPNZ                            44
#define ZEND_JMPZNZ                           45

...
```

opcode 在编译过程中会根据不同的参数类型分配不同的 handler ，在PHP内部的参数类型大致有五种：

```PHP
// file : zend_compile.h
#define IS_CONST    (1<<0)  // 1  常量，字面量 分配在只读内存区域
#define IS_TMP_VAR  (1<<1)  // 2  中间值、临时变量
#define IS_VAR      (1<<2)  // 4  函数返回值，非显示定义的PHP变量
#define IS_UNUSED   (1<<3)  // 8  操作数没有被使用
#define IS_CV       (1<<4)  // 16 脚本中$定义的变量
```
handler 定义的文件在这里：

```PHP
// php-src/Zend/zend_vm_def.h
ZEND_VM_HANDLER(40, ZEND_ECHO, CONST|TMPVAR|CV, ANY)
{
	USE_OPLINE
	zend_free_op free_op1;
	zval *z;

	SAVE_OPLINE();
	z = GET_OP1_ZVAL_PTR_UNDEF(BP_VAR_R);

	if (Z_TYPE_P(z) == IS_STRING) {
		zend_string *str = Z_STR_P(z);

		if (ZSTR_LEN(str) != 0) {
			zend_write(ZSTR_VAL(str), ZSTR_LEN(str));
		}
	} else {
		zend_string *str = _zval_get_string_func(z);

		if (ZSTR_LEN(str) != 0) {
			zend_write(ZSTR_VAL(str), ZSTR_LEN(str));
		} else if (OP1_TYPE == IS_CV && UNEXPECTED(Z_TYPE_P(z) == IS_UNDEF)) {
			GET_OP1_UNDEF_CV(z, BP_VAR_R);
		}
		zend_string_release(str);
	}

	FREE_OP1();
	ZEND_VM_NEXT_OPCODE_CHECK_EXCEPTION();
}

```

可以看到，这里就是定义 echo 的地方，在方法内部，先用 `GET_OP1_ZVAL_PTR_UNDEF` （不必深究，不是本次研究的重点）获取了要输出的变量，然后赋值给一个
zval 类型的指针，接下来判断这个变量是否是字符串，显然 0.7 是浮点数字不是字符串，所以走到  else 里的逻辑，通过 `_zval_get_string_func` 方法获取
变量 z 的字符串值并赋值给 zend_string 类型的指针 str,最后 zend_write 输出这个字符串的值。

这里最主要的操作就是把一个非字符串类型的变量转成了一个字符串，所以，使用 echo 或者其他任何形式的输出方式，最终都是要转成字符串类型的，所以我们主要
观察 `_zval_get_string_func`方法的行为

设置断点：

```SHELL
root@56d745620b72:/data/php7# gdb php
GNU gdb (Ubuntu 8.1-0ubuntu3) 8.1.0.20180409-git
Copyright (C) 2018 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
<http://www.gnu.org/software/gdb/documentation/>.
For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from php...done.
(gdb) b _zval_get_string_func
Breakpoint 1 at 0x2f17f9: file /data/php7/php-src-PHP-7.0.12/Zend/zend_operators.c, line 809.
(gdb)
```

gdb 输出了函数所在的文件位置，我们看看这个函数内部的逻辑：

```PHP
ZEND_API zend_string* ZEND_FASTCALL _zval_get_string_func(zval *op) /* {{{ */
{
try_again:
	switch (Z_TYPE_P(op)) {
		...
		case IS_LONG: {
			return zend_long_to_str(Z_LVAL_P(op));
		}
		case IS_DOUBLE: {
			return zend_strpprintf(0, "%.*G", (int) EG(precision), Z_DVAL_P(op));
		}
		...
}
```

因为 0.7 显然是 浮点型，PHP里的浮点型都是 double 类型，所以我们只看 IS_DOUBLE 部分，它执行了这样一个 zend_strpprint 方法，看着这个方法和 C 语言的
printf 很类似，第二个参数好像是 格式化字符串，我们定位到这个方法里看看：

```SHELL
(gdb) b zend_strpprintf
Breakpoint 2 at 0x32c8fc: file /data/php7/php-src-PHP-7.0.12/Zend/zend_exceptions.c, line 700.
(gdb) 
```

```C
// Zend/zend_exceptions.c
zend_string *zend_strpprintf(size_t max_len, const char *format, ...) /* {{{ */
{
	va_list arg;
	zend_string *str;

	va_start(arg, format);
	str = zend_vstrpprintf(max_len, format, arg);
	va_end(arg);
	return str;
}

```
这个函数看着很简单，接收一个最大长度参数 max_len，格式化参数 format, 然后是可变参数，方法内部使用了 C语言的 va_list,va_start,va_end 获取方法接收
的可变参数，然后又调用 zend_vstrpprintf 方法获取需要返回的字符串值，我们在这个函数上设置一个端点，看下这个函数：

```SHELL
(gdb) b zend_vstrpprintf
Function "zend_vstrpprintf" not defined.
Make breakpoint pending on future shared library load? (y or [n]) n
(gdb) 

```
很遗憾，这个函数不存在。

不过，我们打印一下这个函数的值：

```SHELL
(gdb) p zend_vstrpprintf
$8 = (zend_string *(*)(size_t, const char *, struct __va_list_tag *)) 0x0
(gdb) 
```

发现它指向一个函数！！！但是地址是 0x0，也就是说这个函数其实已经声明了但是没有赋值，那也就是说这个函数其实是在运行时赋值的，它被赋值为另外一个函数。
由于我们并没有真正执行脚本，所以PHP内核还没启动，所以运行时函数就没有被赋值。
其实这个函数是在内核启动函数 `zend_startup`中被赋值的，

```C
// php-src/Zend/zend.c
int zend_startup(zend_utility_functions *utility_functions, char **extensions) /* {{{ */
{
    ......
    zend_vstrpprintf = utility_functions->vstrpprintf_function;
    ......
}
```

而 zend_startup 在 main.c 文件中被调用：

```C
// php-src/main/main.c
/* {{{ php_module_startup
 */
int php_module_startup(sapi_module_struct *sf, zend_module_entry *additional_modules, uint num_additional_modules)
{
  zend_utility_functions zuf;
  ......
	zuf.vspprintf_function = vspprintf;
	zuf.vstrpprintf_function = vstrpprintf;
	zuf.getenv_function = sapi_getenv;
	zuf.resolve_path_function = php_resolve_path_for_zend;
	zend_startup(&zuf, NULL);
  ......
}
```
终于找到这个函数定义的地方了，就是 vstrpprintf 函数，它在这里定义

```C
// php-src/main/spprintf.c

PHPAPI zend_string *vstrpprintf(size_t max_len, const char *format, va_list ap) /* {{{ */
{
	smart_str buf = {0};

	xbuf_format_converter(&buf, 0, format, ap);

	if (!buf.s) {
		return ZSTR_EMPTY_ALLOC();
	}

	if (max_len && ZSTR_LEN(buf.s) > max_len) {
		ZSTR_LEN(buf.s) = max_len;
	}

	smart_str_0(&buf);
	return buf.s;
}
/* }}} */
```

捋一捋这个函数的调用流程：

首先是 在 php-src/main/spprintf.c 中定义了 vstrpprintf 函数，然后在 php_module_startup 中定义了 `zend_utility_functions zuf` 并把
vstrpprintf 赋值给 zuf->vstrpprintf_function 然后调用 zend_startup(&zuf, NULL) ，在 zend_startup 中 将
zend_vstrpprintf 赋值为 utility_functions->vstrpprintf_function，在赋值之前 zend_vstrpprintf 被声明为：
`zend_string *(*zend_vstrpprintf)(size_t max_len, const char *format, va_list ap);`

好了，在 vstrpprintf 函数中又调用了 xbuf_format_converter 函数，并把结果赋值给 buf 。

终于找到了最终执行 echo 函数的逻辑，就是 ‘xbuf_format_converter’，我们找到这个函数的定义：

```C
// php-src/main/spprintf.c
/*
 * Do format conversion placing the output in buffer
 */
static void xbuf_format_converter(void *xbuf, zend_bool is_char, const char *fmt, va_list ap) /* {{{ */
{
  ...
  
}
```

这个函数比较长，有很多判断，我们直接使用 gdb 调试，看看执行流程：

```SHELL

```

