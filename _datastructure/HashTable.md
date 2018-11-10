### HashTable

>The HashTable structure serves many purposes in PHP and can be found everywhere, 
>a good understanding of it's functionality is a preqrequisite of being a good Hacker.

>引自官方文档 http://php.net/manual/en/internals2.variables.tables.php

PHP的 HashTable 结构用于各种用途并且在源码中随处可见，对哈希表实现原理的理解是作为一个内核开发人员的必备前提。

>The HashTable implemented by the engine is a standard HashTable, 
>that is to say, a key => value based store, where the keys are always strings, 
>whose hashes are calculated with a built in hashing algorithm zend_inline_hash_func(const char* key, uint length), 
>which results in good distribution, and reasonable usage.

内核的 HashTable 实现是一个标准的哈希表，也就是一个基于 key => value 结构的存储，key 始终是一个字符串，key 的哈希值由内置的哈希算法
zend_inline_hash_func(const char* key, uint length)计算，这个方法产生的哈希值具有很合理的分布。

#### HashTable 定义

```C
// Zend/zend_types.h
...
typedef struct _zend_array HashTable; // 使用 _zend_array 定义 HashTable

struct _zend_array {
	zend_refcounted_h gc;
	union {
		struct {
			ZEND_ENDIAN_LOHI_4(
				zend_uchar    flags,
				zend_uchar    nApplyCount,
				zend_uchar    nIteratorsCount,
				zend_uchar    reserve)
		} v;
		uint32_t flags;
	} u;
	uint32_t          nTableMask;
	Bucket           *arData;
	uint32_t          nNumUsed;
	uint32_t          nNumOfElements;
	uint32_t          nTableSize;
	uint32_t          nInternalPointer;
	zend_long         nNextFreeElement;
	dtor_func_t       pDestructor;
};

/*
 * HashTable Data Layout
 * =====================
 *
 *                 +=============================+
 *                 | HT_HASH(ht, ht->nTableMask) |
 *                 | ...                         |
 *                 | HT_HASH(ht, -1)             |
 *                 +-----------------------------+
 * ht->arData ---> | Bucket[0]                   |
 *                 | ...                         |
 *                 | Bucket[ht->nTableSize-1]    |
 *                 +=============================+
 */
...


```

