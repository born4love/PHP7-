### 面向对象

类的结构定义
```c
// zend_types.h
typedef struct _zend_class_entry     zend_class_entry;
// zend.h
struct _zend_class_entry {
	// 类的类型：内部类 ZEND_INTERNAL_CLASS(1)、用户自定义类 ZEND_USER_CLASS(2)
	char type;
	// 类名，PHP类不区分大小写，统一为小写
	zend_string *name;
	// 父类
	struct _zend_class_entry *parent;
	// 引用 ??
	int refcount;
	// 类掩码，入普通类、抽奖类、接口，以及别的含义
	uint32_t ce_flags;
	// 普通属性数，包括 public private
	int default_properties_count;
	// 静态属性数
	int default_static_members_count;
	// 普通属性值数组
	zval *default_properties_table;
	// 静态属性值数组
	zval *default_static_members_table;
	zval *static_members_table;
	// 成员方法符号表
	HashTable function_table;
	// 成员属性基本信息哈希表，key为成员名，value为zend_property_info
	HashTable properties_info;
	// 常量符号表
	HashTable constants_table;

	// 构造函数
	union _zend_function *constructor;
	union _zend_function *destructor;
	union _zend_function *clone;
	union _zend_function *__get;
	union _zend_function *__set;
	union _zend_function *__unset;
	union _zend_function *__isset;
	union _zend_function *__call;
	union _zend_function *__callstatic;
	union _zend_function *__tostring;
	union _zend_function *__debugInfo;
	union _zend_function *serialize_func;
	union _zend_function *unserialize_func;

	zend_class_iterator_funcs iterator_funcs;

	/* handlers */
	zend_object* (*create_object)(zend_class_entry *class_type);
	zend_object_iterator *(*get_iterator)(zend_class_entry *ce, zval *object, int by_ref);
	int (*interface_gets_implemented)(zend_class_entry *iface, zend_class_entry *class_type); /* a class implements this interface */
	union _zend_function *(*get_static_method)(zend_class_entry *ce, zend_string* method);

	/* serializer callbacks */
	int (*serialize)(zval *object, unsigned char **buffer, size_t *buf_len, zend_serialize_data *data);
	int (*unserialize)(zval *object, zend_class_entry *ce, const unsigned char *buf, size_t buf_len, zend_unserialize_data *data);

	uint32_t num_interfaces;
	uint32_t num_traits;
	zend_class_entry **interfaces;

	zend_class_entry **traits;
	zend_trait_alias **trait_aliases;
	zend_trait_precedence **trait_precedences;

	union {
		struct {
			zend_string *filename;
			uint32_t line_start;
			uint32_t line_end;
			zend_string *doc_comment;
		} user;
		struct {
			const struct _zend_function_entry *builtin_functions;
			struct _zend_module_entry *module;
		} internal;
	} info;
};
```
