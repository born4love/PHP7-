#### 成员属性

  类的变量成员叫属性。属性声明由关键字public,protected,private开头，然后跟一个普通的变量声明来组成。属性中的变量可以初始化，但是初始化的值必须是固定不
变的值，不可以是变量，初始化值在编译阶段就可以得到其值，而不依赖于运行时的信息才能求值，比如 public $time = time()，这样定义一个属性就会触发语法错误。
  成员属性分为两类：普通属性、静态属性。静态属性为各对象共享，与常量类似，而普通成员属性则是各对象独享。与常量的存储方式不同，成员属性的初始值并不是直接
以属性名作为索引的哈希表存储的，而是通过数组保存的，普通属性、静态属性各有一个数组分别存储，即default_properties_table、default_static_members_table
。另外，zend_class_entry中还有两个成员用于记录这两个数组的长度：default_properties_count、default_static_members_count。
  如果使用数组存储，那么访问成员属性时是如何找到的呢？
  普通成员属性的存储与局部变量的实现相同，它们分布在zend_object上，通过相对zend_object的内存偏移进行访问，各属性的内存偏移在编译时分配，普通成员属性的
存取都是根据这个内存offset完成的，静态属性相对简单，直接根据数组下标访问。
  实际上，default_properties_table和default_static_members_table数组只是用来存储属性值的，并不是保存属性信息的，这里说的属性信息包括属性的访问权限
  （public,protected,private）、属性名、静态属性值的存储下标、非静态属性的内存offset等，这些信息通过zend_property_info结构存储，该结构通过
  zend_class_entry->properties_info符号表存储，这是一个哈希表，key就是属性名。
  
  ```c
  typedef struct _zend_propertiy_info {
    uint32_t offset; //普通成员变量的内存偏移量，静态成员变量的数组索引
    uint32_t flags; // 属性掩码,如 public protected private 及是否是静态属性
    zend_string *name; // 属性名，并不是原始属性名
    zend_string *doc_comment; 
    zend_class_entry *ce; //所属类
  } zend_property_info;
  ```
  
  * name : 属性名，并不是原始属性名，private会在原始属性名前加上类名，protected则会加上*作为前缀
  * offset : 普通成员属性的内存偏移值， 静态成员属性在default_static_members_table数组索引
  * flag : 属性标识，有两个含义 --- 一是区分是否为静态，静态、非静态属性的结构都存储在这一个符号表中；而是属性权限，即public、private、protected
  
  
  ```c
  // flag 标识位
  #define ZEND_ACC_PUBLIC 0x100
  #define ZEND_ACC_PROTECTED 0x200
  #define ZEND_ACC_PRIVATE 0x400
  
  #define ZEND_ACC_STATIC 0x01
  ```
  
    在编译时，成员属性将根据属性类型按照属性定义的先后顺序分配一个对应的offset，用于运行时索引属性的存储位置。读取成员属性分为两步：
   *  根据属性名从zend_class_entry.properties_info中索引到属性的zend_property_info结构
   *  根据zend_property_info->offset获取具体的属性值，其中静态成员属性通过zend_class_entry.default_static_members_table[offset]获取，普通成员
   属性则通过((char*)zend_object)+offset获取。
