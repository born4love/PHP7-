#### Zend Thread Safe

多线程环境下（apache或者嵌入式PHP环境）全局变量防止冲突的解决方案。

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
