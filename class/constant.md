7.1.1 常量

  PHP中可以把在类中始终保持不变的值定义为常量，在定义和使用常量的时候不需要使用$符号，常量的值必须是一个定值，比如布尔型，整型，字符串，数组，不能是
  变量、数学运算的结果或函数调用，它是只读的，无法进行赋值。类中的常量与PHP中的普通常量（define()或const定义的）含义是一样的，只不过类中的常量是属于
  类的，普通常量是全局的。
  
  ```c
  class my_function {
    // 定义一个常量
    const CONST_NAME = const_value;
    
    public function __construct()
    {
      // 在类内部访问常量
      self::CONST_NAME;
    }
  }
  
  // 在类的外部访问常量
  my_class:CONST_NAME;
  ```
  
 
    常量属于类维度的数据，对于类的所有实例化对象是没有任何差异的，这一点与成员属性不同。与普通常量相同，类的常量也通过哈希表存储，类的常量保存在
zend_class_entry->constants_table中。通常情况下访问常量是需要根据常量名索引，但是有些情况下会在编译阶段将常量直接替换为常量值使用，比如：

```c
// 示例1
echo my_class::A1;

class my_class {
  const A1 = 'hi';
}

// 示例2
class my_class {
  const A1 = 'hi';
}

echo my_class::A1;
```

  示例1在定义前使用，示例2在定义后使用。
  PHP中无论是变量、常量还是函数，都不需要提前声明，那么这两个会有什么不同呢？
  具体debug一下上面两个例子会发现：示例2编译生成的主要指令只有一个ZEND_ECHO，也就是直接输出值了，并没有涉及常量的查找，进一步查看发现它的操作数为
CONST变量，值为“hi”，也就是my_class::A1的值；而示例1首先执行的指令则是ZEND_FETCH_CONSTANT，查找常量，接着才是ZEND_ECHO。
  事实上这两种情况内核会有两种不同的处理方式：示例1这种情况会在运行时根据常量名称索引zend_class_entry_constant_table，取到常量值以后再执行echo；而
示例2则有另外一种处理方式，PHP代码的编译是顺序的，示例2在编译到echo my_class::A1这行时，首先会尝试索引下是否已编译了my_class，如果能在
CG(class_table)中找到，则进一步从类的constants_table中查找对应常量，找到的话则会复制其value替换常量，也就是将常量的检索提前到编译时，通过这种“预处
理”优化了耗时的检索过程，避免多次执行的重复检索，同时也可以利用opcode避免放到运行时重复检索常量。
  














