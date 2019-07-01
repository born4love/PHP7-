#### Zend Thread Safe

多线程环境下（在windows 环境或者使用多线程的 unix 操作系统环境下 apache等 web 服务器会运行在多线程模式下）全局变量防止冲突的解决方案。

#### 线程安全资源管理器 (TSRM)

```C
// D:\projects\php-src\Zend\zend_globals_macros.h

BEGIN_EXTERN_C()  // extern "C" { 表示使用 C 语言的编译和链接规则

/* Compiler 编译期间的全局变量*/
#ifdef ZTS
# define CG(v) ZEND_TSRMG(compiler_globals_id, zend_compiler_globals *, v)
#else
# define CG(v) (compiler_globals.v)
extern ZEND_API struct _zend_compiler_globals compiler_globals;
#endif
ZEND_API int zendparse(void);


/* Executor 执行期间的全局变量*/
#ifdef ZTS
# define EG(v) ZEND_TSRMG(executor_globals_id, zend_executor_globals *, v)
#else
# define EG(v) (executor_globals.v)
extern ZEND_API zend_executor_globals executor_globals;
#endif

/* Language Scanner 语法扫描期间的全局变量*/
#ifdef ZTS
# define LANG_SCNG(v) ZEND_TSRMG(language_scanner_globals_id, zend_php_scanner_globals *, v)
extern ZEND_API ts_rsrc_id language_scanner_globals_id;
#else
# define LANG_SCNG(v) (language_scanner_globals.v)
extern ZEND_API zend_php_scanner_globals language_scanner_globals;
#endif


/* INI Scanner 配置文件扫描期间的全局变量*/
#ifdef ZTS
# define INI_SCNG(v) ZEND_TSRMG(ini_scanner_globals_id, zend_ini_scanner_globals *, v)
extern ZEND_API ts_rsrc_id ini_scanner_globals_id;
#else
# define INI_SCNG(v) (ini_scanner_globals.v)
extern ZEND_API zend_ini_scanner_globals ini_scanner_globals;
#endif

END_EXTERN_C()  // }
```

如果在线程安全模式下，就使用 ZEND_TSRMG 宏获取全局变量的值，否则，公用全局变量

#### ZEND_TSRMG

```C
// D:\projects\php-src\Zend\zend.h

L67:  #define ZEND_TSRMG TSRMG_STATIC

```

```C
// D:\projects\php-src\TSRM\TSRM.h

L163: #define TSRM_SHUFFLE_RSRC_ID(rsrc_id)		((rsrc_id)+1)
L164: #define TSRM_UNSHUFFLE_RSRC_ID(rsrc_id)		((rsrc_id)-1)

L171: #define TSRMG_STATIC(id, type, element)	(TSRMG_BULK_STATIC(id, type)->element)
L172: #define TSRMG_BULK_STATIC(id, type)	((type) (*((void ***) TSRMLS_CACHE))[TSRM_UNSHUFFLE_RSRC_ID(id)])
L173: #define TSRMLS_CACHE_EXTERN() extern TSRM_TLS void *TSRMLS_CACHE;
L174: #define TSRMLS_CACHE_DEFINE() TSRM_TLS void *TSRMLS_CACHE = NULL;
L175: #if ZEND_DEBUG
L176: #define TSRMLS_CACHE_UPDATE() TSRMLS_CACHE = tsrm_get_ls_cache()
L177: #else
L178: #define TSRMLS_CACHE_UPDATE() if (!TSRMLS_CACHE) TSRMLS_CACHE = tsrm_get_ls_cache()
L179: #endif
L180: #define TSRMLS_CACHE _tsrm_ls_cache

```

因此 ZEND_TSRMG 宏展开后结果是：

```C
_tsrm_ls_cache = tsrm_get_ls_cache()

TSRMG_STATIC(id, type, element)	((type) (*((void ***) tsrm_get_ls_cache()))[TSRM_UNSHUFFLE_RSRC_ID(id)])->element)
```

tsrm_get_ls_cache()方法的定义在 TSRM.c

```C
// D:\projects\php-src\TSRM\TSRM.c L:801

TSRM_API void *tsrm_get_ls_cache(void)
{
	return tsrm_tls_get();
}
```

tsrm_tls_get() 方法的定义也在这个文件：

```C
// D:\projects\php-src\TSRM\TSRM.c L:96
#if defined(PTHREADS)
/* Thread local storage */
static pthread_key_t tls_key;
# define tsrm_tls_set(what)		pthread_setspecific(tls_key, (void*)(what))
# define tsrm_tls_get()			pthread_getspecific(tls_key)

#elif defined(TSRM_ST)
static int tls_key;
# define tsrm_tls_set(what)		st_thread_setspecific(tls_key, (void*)(what))
# define tsrm_tls_get()			st_thread_getspecific(tls_key)

#elif defined(TSRM_WIN32)
static DWORD tls_key;
# define tsrm_tls_set(what)		TlsSetValue(tls_key, (void*)(what))
# define tsrm_tls_get()			TlsGetValue(tls_key)

#elif defined(BETHREADS)
static int32 tls_key;
# define tsrm_tls_set(what)		tls_set(tls_key, (void*)(what))
# define tsrm_tls_get()			(tsrm_tls_entry*)tls_get(tls_key)

#else
# define tsrm_tls_set(what)
# define tsrm_tls_get()			NULL
# warning tsrm_set_interpreter_context is probably broken on this platform
#endif
```

这里增加了判断逻辑，分别针对 PTHREADS TSRM_ST TSRM_WIN32 BETHREADS 四种不同的线程支持情况，使用相应的系统调用函数定义 tsrm_tls_set() 和 tsrm_tls_get()方法.
以 pthread_setspecific为例，set方法将当前线程指定的值绑定到 tls_key 指定的键上，get 方法则是通过指定的key值取得这个值.

参考 [pthread_setspecific doc](https://linux.die.net/man/3/pthread_getspecific)


