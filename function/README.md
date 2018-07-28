### 第六章 函数

  PHP内核中，函数通过zend_function结构表示，而不是直接使用zend_op_array，这是因为PHP中还有一种特殊的函数：内部函数。zend_function实际是一个union，用来适配两种不同的函数，对于PHP脚本中定义的函数（以下称：用户自定义函数）而言，它就是zend_op_array。
  
  ```c
  // zend_compile.h
  union _zend_function {
    zend_uchar type;
    
    struct {
      zend_uchar type; /*never used*/
      zend_uchar arg_flags[3];
      // 方法标识：final/static/abstract、private/public/protected
      uint32_t fn_flags;
      zend_string *function_name;
      // 成员方法所属类，面向对象实现中用到
      zend_class_entry *scope;
      union _zend_function *prototype;
      // 参数数量
      uint32_t num_args;
      // 必传参数数量
      uint32_t required_num_args;
      // 参数信息
      zend_arg_info *arg_info;
    } common;
    // 用户自定义函数
    zend_op_array op_array;
    // 内部函数
    zend_internal_function internal_function;
  }
  
  typedef struct _zend_internal_function {
	/* Common elements */
	zend_uchar type;
	zend_uchar arg_flags[3]; /* bitset of arg_info.pass_by_reference */
	uint32_t fn_flags;
	zend_string* function_name;
	zend_class_entry *scope;
	zend_function *prototype;
	uint32_t num_args;
	uint32_t required_num_args;
	zend_internal_arg_info *arg_info;
	/* END of common elements */

	void (*handler)(INTERNAL_FUNCTION_PARAMETERS);
	struct _zend_module_entry *module;
	void *reserved[ZEND_MAX_RESERVED_RESOURCES];
} zend_internal_function;

typedef struct _zend_arg_info {
	// 参数名
	zend_string *name;
	// 如果参数/返回值是类，这里是类名
	zend_string *class_name;
	// 指定的参数类型，比如array $param_1
	zend_uchar type_hint;
	// 是否引用参数，参数前&
	zend_uchar pass_by_reference;
	// 是否允许为空
	zend_bool allow_null;
	// 是否可变参数
	zend_bool is_variadic
} zend_arg_info;
  ```
  
  zend_function结构中还有两个特殊的成员：type、common,他们都是zend_op_array、zend_internal_function接口中共有成员，用户快速获取函数的基本信息。type用于区分函数类型，common则是函数的名称、参数等信息，对于用户自定义函数而言，zend_function.type实际上取的是zend_function.op_array.type，zend_function.common.xxx等价于zend_function.op_array.xxx。
  
  函数编译完成后，会插入EG(function_table)符号表中，使用函数时根据函数名称在符号表中索引zend_function，从而获得编译后的函数指令，这也意味着函数的作用域是全局的，编译后可以在任意位置使用函数。
