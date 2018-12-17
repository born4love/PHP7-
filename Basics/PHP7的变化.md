
### PHP7相对PHP5的变化

* 抽象语法树
* Native TLS
* 指定函数参数、返回值类型
* zval结构的变化
* 异常处理
* Hash Table的变化
* 执行器
* 新的参数解析方式

####  抽象语法树
PHP7中增加了抽象语法树（AST），首先将PHP代码解析生成抽象语法树，然后将抽象语法树编译为ZendVM指令。抽象语法树的加入使得PHP的编译器和执行器很好的隔离开
，编译器不需要关心指令的生成规则，执行器同样不需要关心指令的语法规则。在此之前，PHP代码在语法解析阶段直接生成ZendVM指令，使得编译器和执行器耦合在一起
。

#### Native TLS
在PHP5.x 版本中，使用 TSRM_CC、TSRM_DC 这两个宏来实现线程安全。PHP中有很多变量需要在不同函数间共享，多线程的环境下不能简单的通过全局变量来实现，为了
适应多线程的应用环境，PHP提供了一个线程安全资源管理器，将全局资源进行了线程隔离，不同的线程之间互不干扰。

使用全局资源首先要获取本线程的资源池，这个过程比较占用时间，因此PHP5.x通过参数传递的方式将本线程的资源池传递给其他函数，避免重复查找。这种实现方式使得
几乎所有的函数都要加上接收资源的参数，也就是 TSRM_DC 宏所加的参数，然后调用其他函数时再把这个参数传下去，不仅容易遗漏，而且这种方式极不优雅。

PHP7中引入了 NativeTLS(线程局部存储)来保护线程的资源池，简单的讲就是通过 “_thread” 标识一个全局变量，这样这个全局变量就是线程独享的了，不同线程的修改
不会互相影响。

#### 指定函数参数、返回值类型
PHP7中可以指定函数的参数及返回值类型，例如：

```PHP
function foo(string $name): array 
{
  return [];
}
```

这个函数的参数必须为字符串，返回值必须为数组，否则会报 error 错误。

#### zval结构的变化
zval 是PHP中非常重要的一个结构，它是PHP变量的内部结构，也是PHP内核中应用最为普遍的一个结构。在PHP5.x中，zval结构是这样的：

```C
struct _zval_struct {
  /* variable information */
  zvalue_value value;   /* value */
  zend_uint refcount_gc;
  zend_uchar type; /* activie type */
  zend_uchar is_ref_gc;
}
```

type为类型，is_ref_gc标识变量是否为引用，value为变量的具体指，它是一个union，用来适配不同的变量类型：

```C
typedef union _zvalue_value {
  long lval;    /* long value */
  double dval;  /* double value */
  struct {
    char *val;
    int len;
  } str;
  HashTable *ht;   /* hash table value */
  zend_object_value obj;
  zend_ast *ast;
} zvalue_value;
```

zval中的refcount_gc变量记录的是变量的引用计数值，PHP通过这个值实现变量的自动回收。PHP5.x中引用计数在zval中而不是在 value中，这样一来，复制变量就
需要同时复制两个结构，zval和zvalue_value始终绑定在一起。PHP7将引用计数转移到了具体的value中，这样更合理。因为zval只是变量的载体，可以理解为变量
名，而value才是真正的值，这个改变使得PHP变量的复制、传递更加简洁、易懂。zval结构体的大小也从24B减少到了16B，这是PHP7能够降低系统资源占用的一个关键
所在。


#### 异常处理
PHP5.x中很多操作会直接抛出error错误，PHP7中将多数错误改为抛出异常，可以通过 try/catch 捕捉到。

#### Hash Table的变化
HashTable 即哈希表。是PHP中 array() 类型的内部实现结构，也是内核中使用非常频繁的一个结构，函数符号表、类符号表、常量符号表等都是通过 HashTable 
实现的。
PHP7中 HashTable 有非常大的变化， HashTable 结构的大小从 72B 减小到 56B，同时，数组元素 Bucket 结构也从 72B 减小到了 32B。

#### 执行器

