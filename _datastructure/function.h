typedef union _zend_function zend_function;
// zend_compile.h
union _zend_function {
  zend_uchar type;
  
  struct {
    zend_uchar type; /* never used */
    zend_uchar arg_flags[3];
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
};
