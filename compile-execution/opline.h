// opline 指令的结构:zend_op
struct _zend_op {
  const void *handler; // 指令执行handler
  znode_op op1; // 操作数1
  znode_op op2; // 操作数2
  znode_op result; // 返回值
  uint32_t extended_value;
  uint32_t lineno;
  zend_uchar opcode; // opcode 指令
  zend_uchar op1_type; // 操作数1类型
  zend_uchar op2_type; // 操作数2类型
  zend_uchar result_type; // 返回值类型
};

// zend_vm_opcode.h
#define ZEND_NOP  0
#define ZEND_ADD  1
#define ZEND_SUB  2
#define ZEND_MUL  3
#define ZEND_DIV  4
#define ZEND_MOD  5
#define ZEND_SL   6
#define ZEND_SR   7
...

// 操作数结构：znode_op，实际就是个32位整型
typedef union _znode_op {
  uint32_t  constant;
  uint32_t  var;
  uint32_t  num;
  uint32_t  opline_num;
  uint32_t  jmp_offset;
} znode_op;
