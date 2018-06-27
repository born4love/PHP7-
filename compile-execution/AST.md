### 抽象语法树

`抽奖语法树通过不同的节点表示具体的语法`
四类节点：

* 普通节点
* list节点
* 数据节点
* 声明节点

####  普通节点
(普通节点做为非叶子节点，通常用于某种语法的根节点，结构为zend_ast:)
```c
typedef struct _zend_ast  zend_ast;
struct _zend_ast {
  zend_ast_kind kind; // 节点类型
  zend_ast_attr attr; // Additional attribute
  unit32_t      lineno; // 行号
  zend_ast      *child[1]; // 子节点
}
```
zend_ast最重要的两部分就是节点的类型和子节点child，不同kind类型zend_ast的子节点数是不同的，节点类型：
[_zend_ast_kind](https://github.com/php/php-src/blob/a394e1554c233c8ff6d6ab5d33ab79457b59522a/Zend/zend_ast.h#L36)

####  list节点
```c
typedef struct _zend_ast_list {
  zend_ast_kind kind;
  zend_ast_attr attr;
  uint32_t      lineno;
  uint32_t      children;
  zend_ast      *child[1];
}zend_ast_list;
```

####  数据节点
```c
typedef struct _zend_ast_zval {
  zend_ast_kind kind;
  zend_ast_attr attr;
  zval          val;
}zend_ast_zval;
```

####  声明节点
这类节点用于函数、类、成员方法、闭包的表示。

------------------------------------------------------------
### 编译过程
  * 词法规则文件：https://github.com/deanisty/php-src/blob/master/Zend/zend_language_scanner.l
  * 语法规则文件：https://github.com/deanisty/php-src/blob/master/Zend/zend_language_parser.y
  
  ![image](https://github.com/born4love/PHP7-internal-dissect/blob/master/compile-execution/compile/PHP-compile.png)
