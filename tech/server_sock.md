# 创建Socket并监听端口

创建Socket:

```c
int startup(u_short* port) {
  int httpd = -1;
  // PF_INET 和AF_INET基本一致, 推荐写AF_INET
  // SOCK_STREAM 指TCP协议, SOCK_DGRAM 指UDP
  // 最后一位为保留位
  httpd = socket(AF_INET, SOCK_STREAM, 0);
  if (httpd == -1) error_die("socket");

  // 设置绑定数据
  struct sockaddr_in name;
  memset(&name, 0, sizeof(name));
  // 确定协议簇为IPV4
  name.sin_family = AF_INET;
  // 配置绑定地址为本机开放地址
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  // 配置传入的端口
  name.sin_port = htons(*port);
  // 绑定Socket和配置项
  if (bind(httpd, &name, sizeof(name)) < 0) error_die("bind");
  // 启动监听动作
  if (listen(httpd, 5) < 0) error_die("listen");

  return httpd;
}
```
h代表host主机、n代表net网络、l代表long长整型、s代表short短整形。

`htonl`/`htons`/`ntohl`/`ntohs` 是用于转换字节顺序的函数。计算机内一般是大端存储: `0x12345678`, 最左侧的`0x12`存储在高位；网络发送时，用的是小端模式。如:

```c
uint32_t a = 0x12345678;
printf("res: 0x%x\n", htonl(a)); // 0x78563412
```

在绑定过程，如果发现`*port == 0`, 则说明端口没有绑定成功，需要使用`int(*getsockname)(int, struct sockaddr*, int*)`函数请求系统给httpd分配端口

```c
  if (bind(httpd, &name, sizeof(name)) < 0) error_die("bind");

+ if (*port == 0) {
+   int name_len = sizeof(name);
+   if (getsockname(httpd, &name, &name_len) < 0) error_die("getsockname");
+   // 得到可用的port后回填到参数中
+   *port = ntohs(name.sin_port);
+ }
```

`int(*listen)(int socket, int backlog)`方法中，第二个填写为5，理由是未连接队列和已完成TCP握手队列数，上限是5，所以就填5了。

创建Socket直接使用方法`int(*socket)(int, int, int)`即完成, 接下来绑定(`bind`)使用的协议族(ipv4/ipv6)、端口、地址, 以及最后的发出监听(`listen`)。

    gcc -o server_sock server_sock.c
    ./server_sock // 打印出端口号说明成功

