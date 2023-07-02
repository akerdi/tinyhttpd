# tinyhttpd源码解析

tinyhttpd 能帮助我们看懂网络请求中的过程，花费小半天时间就能完全读懂，完全是精品项目。只要跟着教程一步步实现，就能掌握Socket。

本文目的是读懂源码，理解C/S交互过程。

## 直接运行环境

```
平台: Mac/linux/Windows
IDE: VSCode
语言: C
编译器: GCC
编译工具: CMake
```

运行: `cp .vscode/launch.example.json .vscode/launch.json && cp .vscode/tasks.example.json .vscode/tasks.json` 然后按F5

浏览器打开对应端口, 如`http://localhost:51888`

在页面中填写颜色, 如red/blue

## 教程

+ [启动服务端Socket](./tech/01server_sock.md)
+ [浏览器连接并打印请求头信息](./tech/02client_sock.md)
+ [发送文件到浏览器](./tech/03accept_request.md)
+ [执行浏览器发送的命令](./tech/04execute_cgi.md)

## 请我喝杯咖啡

![wechat](./images/donate/wechatPay-8.jpeg)![alipay](./images/donate/aliPay-8.jpeg)

## 联系我

如果您有任何疑问都可以使用以下的方式联系到我:

+ 新建一个issue.
+ 发送邮件给我, `tianxiaoxin001gmail.com`