#### ZTS API 

讲述PHP7 线程安全实现原理

> 参考文章 [PHP和线程](https://iliubang.cn/php/2017/10/12/php%E5%92%8C%E7%BA%BF%E7%A8%8B.html)

```C
//API 源代码目录
// D:\projects\php-src\TSRM\TSRM.c
```

##### 结构体

```C
// 每个线程安全资源存储的结构体
// 保存了线程id 和 当前线程的资源 storage 以及资源的数量
struct _tsrm_tls_entry {
	void **storage;
	int count;
	THREAD_T thread_id;
	tsrm_tls_entry *next; // 链表
};

// 保存资源类型的结构体
typedef struct {
	size_t size; // 资源的大小
	ts_allocate_ctor ctor; // 初始化回调
	ts_allocate_dtor dtor; // 销毁回调
	int done; 
} tsrm_resource_type;

/* The memory manager table 资源表 */
static tsrm_tls_entry	**tsrm_tls_table=NULL;
static int				tsrm_tls_table_size; // 表的大小
static ts_rsrc_id		id_count; // 资源数量+1

/* The resource sizes table 资源类型表 */
static tsrm_resource_type	*resource_types_table=NULL;
static int					resource_types_table_size; // 类型表的大小
```

##### tsrm_startup

```C

/* Startup TSRM (call once for the entire process) */
// TSRM初始化函数 在一个单独的PHP进程生命周期中仅执行一次
TSRM_API int tsrm_startup(int expected_threads, int expected_resources, int debug_level, char *debug_filename)
{
// 根据当前系统支持的线程处理方式 调用相应的系统调用初始化
#if defined(GNUPTH)
	pth_init();
#elif defined(PTHREADS)
	pthread_key_create( &tls_key, 0 );  // 初始化 thread local storage key 为0
#elif defined(TSRM_ST)
	st_init();
	st_key_create(&tls_key, 0);
#elif defined(TSRM_WIN32)
	tls_key = TlsAlloc();
#elif defined(BETHREADS)
	tls_key = tls_allocate();
#endif

	tsrm_error_file = stderr;
	tsrm_error_set(debug_level, debug_filename);
	tsrm_tls_table_size = expected_threads; // 线程安全资源表的大小，在初始化过程中为1
  // 初始化资源表 分配堆空间
  // 类型为 tsrm_tls_entry 二级指针 类似一个指针数组 
	tsrm_tls_table = (tsrm_tls_entry **) calloc(tsrm_tls_table_size, sizeof(tsrm_tls_entry *)); 
  
	if (!tsrm_tls_table) {
		TSRM_ERROR((TSRM_ERROR_LEVEL_ERROR, "Unable to allocate TLS table"));
		return 0;
	}
  // 资源数量 初始化期间为 0
	id_count=0;
  // 资源类型表 与资源表大小一致
	resource_types_table_size = expected_resources;
  // 资源类型表分配内存空间 并初始化
	resource_types_table = (tsrm_resource_type *) calloc(resource_types_table_size, sizeof(tsrm_resource_type));
	if (!resource_types_table) {
		TSRM_ERROR((TSRM_ERROR_LEVEL_ERROR, "Unable to allocate resource types table"));
		free(tsrm_tls_table);
		tsrm_tls_table = NULL;
		return 0;
	}
  // 初始化互斥锁 在生成新的资源id时使用 避免多个线程同时操作
	tsmm_mutex = tsrm_mutex_alloc();

	tsrm_new_thread_begin_handler = tsrm_new_thread_end_handler = NULL;

	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Started up TSRM, %d expected threads, %d expected resources", expected_threads, expected_resources));
	return 1;
}
```

##### ts_allocate_id

```C
/* allocates a new thread-safe-resource id */
// 分配一个新的线程安全资源id，增加新资源时用到
TSRM_API ts_rsrc_id ts_allocate_id(ts_rsrc_id *rsrc_id, size_t size, ts_allocate_ctor ctor, ts_allocate_dtor dtor)
{
	int i;

	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Obtaining a new resource id, %d bytes", size));
  // 当前线程获取互斥锁 准备操作资源表
	tsrm_mutex_lock(tsmm_mutex);

	/* obtain a resource id  获取资源id*/
  // id_count 默认为0 TSRM_SHUFFLE_RSRC_ID 宏将其值加1，id_count 此时为1
  // rsrc_id 表示当前资源 id
	*rsrc_id = TSRM_SHUFFLE_RSRC_ID(id_count++);
	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Obtained resource id %d", *rsrc_id));

	/* store the new resource type in the resource sizes table */
  // 随着资源的增加 类型表也要扩容 详见 realloc 方法的定义
	if (resource_types_table_size < id_count) {
		resource_types_table = (tsrm_resource_type *) realloc(resource_types_table, sizeof(tsrm_resource_type)*id_count);
		if (!resource_types_table) {
			tsrm_mutex_unlock(tsmm_mutex);
			TSRM_ERROR((TSRM_ERROR_LEVEL_ERROR, "Unable to allocate storage for resource"));
			*rsrc_id = 0;
			return 0;
		}
		resource_types_table_size = id_count;
	}
  // TSRM_UNSHUFFLE_RSRC_ID 宏会将 rsrc_id 的值减一获得当前资源的id
  // 保存资源类型 size:资源的大小 ctor:资源初始化函数 dtor:资源析构函数 done:资源是否使用完毕
	resource_types_table[TSRM_UNSHUFFLE_RSRC_ID(*rsrc_id)].size = size;
	resource_types_table[TSRM_UNSHUFFLE_RSRC_ID(*rsrc_id)].ctor = ctor;
	resource_types_table[TSRM_UNSHUFFLE_RSRC_ID(*rsrc_id)].dtor = dtor;
	resource_types_table[TSRM_UNSHUFFLE_RSRC_ID(*rsrc_id)].done = 0;

	/* enlarge the arrays for the already active threads */
	// 每个线程的资源表都为新增加的资源id创建内存空间
	for (i=0; i<tsrm_tls_table_size; i++) {
	        // 遍历资源表中所有条目
		tsrm_tls_entry *p = tsrm_tls_table[i];

		while (p) {
		        // 当前资源数量小于资源总数
			if (p->count < id_count) {
				int j;
				// 调整资源空间 如果不足则重新分配 保证内存连续
				p->storage = (void *) realloc(p->storage, sizeof(void *)*id_count);
				for (j=p->count; j<id_count; j++) {
					// 每个资源分配size指定的内存空间 并调用对应的初始化函数初始化
					p->storage[j] = (void *) malloc(resource_types_table[j].size);
					if (resource_types_table[j].ctor) {
						resource_types_table[j].ctor(p->storage[j]);
					}
				}
				// 当前线程表的资源总量
				p->count = id_count;
			}
			// 处理下一个线程资源表
			p = p->next;
		}
	}
	// 处理完成 释放锁
	tsrm_mutex_unlock(tsmm_mutex);

	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Successfully allocated new resource id %d", *rsrc_id));
	return *rsrc_id;
}
```


##### tsrm_shutdown

```C
/* Shutdown TSRM (call once for the entire process) */
// TSRM关闭API，在PHP进程生命周期中仅执行一次
TSRM_API void tsrm_shutdown(void)
{
	int i;

	if (tsrm_tls_table) {
		for (i=0; i<tsrm_tls_table_size; i++) {
			tsrm_tls_entry *p = tsrm_tls_table[i], *next_p;

			while (p) {
				int j;

				next_p = p->next;
				for (j=0; j<p->count; j++) {
					if (p->storage[j]) {
						if (resource_types_table && !resource_types_table[j].done && resource_types_table[j].dtor) {
							resource_types_table[j].dtor(p->storage[j]);
						}
						free(p->storage[j]);
					}
				}
				free(p->storage);
				free(p);
				p = next_p;
			}
		}
		free(tsrm_tls_table);
		tsrm_tls_table = NULL;
	}
	if (resource_types_table) {
		free(resource_types_table);
		resource_types_table=NULL;
	}
	tsrm_mutex_free(tsmm_mutex);
	tsmm_mutex = NULL;
	TSRM_ERROR((TSRM_ERROR_LEVEL_CORE, "Shutdown TSRM"));
	if (tsrm_error_file!=stderr) {
		fclose(tsrm_error_file);
	}
#if defined(GNUPTH)
	pth_kill();
#elif defined(PTHREADS)
	pthread_setspecific(tls_key, 0);
	pthread_key_delete(tls_key);
#elif defined(TSRM_WIN32)
	TlsFree(tls_key);
#endif
}
```
