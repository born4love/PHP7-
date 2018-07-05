### Opcache
`Opcache --- 一个用于缓存opcodes以提升PHP性能的Zend扩展，它的前身是Zend Optimizer Plus(简称 O+)，目前已经是PHP
非常重要的组成部分，它对于PHP性能的提升有着非常显著的作用。`

Opcache最主要的功能是缓存PHP代码编译生成的opcodes指令，工作原理比较容易理解：开启Opcache后，它将PHP的编译函数
zend_compile_file()替换为persistent_compile_file()，接管PHP代码的编译过程，当新的请求到达时，将调用
persistent_compile_file()进行编译。此时，Opcache首先检查是否有该文件的缓存，如果有则取出，然后进行一系列的验证，
如果最终发现缓存可用则直接返回，进入执行流程，如果没有缓存或者缓存已经失效，则重新调用系统默认的编译器进行编译，然后
将编译后的结果缓存下来，供下次使用。流程如下图

![image](https://github.com/deanisty/PHP7-internal-dissect/blob/master/images/opcache.png)
