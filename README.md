一个简单的 shell，要求见[OSH 2018 编写 shell 程序](https://osh-2018.github.io/2/shell/)。

实现的功能有：

* 跳过冗余的空格；
* 管道；
* 修改环境变量；
* 文件重定向；
* 带单、双引号的字符串去除引号之后作为参数（不进行变量展开和子 shell 执行，不支持形如`"init"ial`这样的字符串）。