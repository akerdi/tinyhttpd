# 浏览器连接并打印请求头信息

这章目标是客户端连接，打印出客户端的请求头，并发送状态码200.

## 讲解HTTP传输协议

Header信息都是描述信息，包括请求method、HTTP版本、Content-Type、Content-Length和`\r\n`分隔符等信息。

当然`\r\n`之后的所有数据就是Body数据了。

### 请求

```
-------
|method|空格|url|空格|HTTP版本|\r\n
|Content-Length:${length}|\r\n
|\r\n
Body
-------
```

### 响应头

```
--------
|HTTP版本|空格|状态码|MSG|\r\n
|...\r\n
|\r\n
Body
--------
```

## accept等待客户端连接

使用`accept`等待客户端连接:

```c
int main() {
  ...
  printf("server start at port: %d\n", port);
  int client_sock = -1;
  struct sockaddr_in name;
  int name_len = sizeof(name);
+ while (1) {
+   / accept会阻塞等待客户端的连接
+   client_sock = accept(server_sock, &name, &name_len);
+   if (client_sock < 0) error_die("accept");
+   // 连接后的业务处理逻辑
+   accept_request(client_sock);
+ }
```

当客户端连接之后我们就到了处理业务逻辑的函数`void(*accept_request)(int client)`.

## 处理客户端业务逻辑

在处理之前，先实现读取一行请求头信息.

### 读取一行请求头信息

```c
int read_line(int client, char* buf, int size) {
  // 注意! char c 提前赋值，否则有可能默认是'\n'情况
  char c = '\0';
  int i = 0;
  int n = 0;

  while ((i < size - 1) && (c != '\n')) {
    n = recv(client, &c, 1, 0);
    if (n > 0) {
      if (c == '\r') {
        n = recv(client, &c, 1, MSG_PEEK);
        if (n > 0 && c == '\n') {
          recv(client, &c, 1, 0);
        } else c = '\n';
      }
      buf[i] = c;
      i++;
    } else c = '\n';
  }
  buf[i] = '\0';

  return i;
}
```

这里逻辑是遇到`\r` 就认为换行。

函数`int(*recv)(int socket, void* buffer, size_t length, int flags)`用于从socket中读取数据。buffer 为读取后保存内存；length 为读取大小；flags 为0代表读取，flags为MSG_PEEK 时代表查看但不读取。

> 另外特别注意`char c`需要提前赋值, Mac环境中，char 未赋值的话，大概率是'\n'，会导致返回`i = 0`, 对之后的业务有影响, 如返回信息到客户端非正常结束`curl: (56) Recv failure: Connection reset by peer`

### 依次读取头文件信息

```c
void accept_request(int client) {
  char buf[1024];
  // 让下方的while循环正常进入
  buf[0] = 'A'; buf[1] = '\0';
  int read_count = strlen(buf);

  while (read_count > 0 && strcmp(buf, "\n")) {
    read_count = read_line(client, buf, sizeof(buf));
    printf("header > %s", buf);
  }
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  send(client, &buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, &buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, &buf, strlen(buf), 0);
  sprintf(buf, "<HTML><TITLE>200 OK</TITLE>\r\n");
  send(client, &buf, strlen(buf), 0);
  sprintf(buf, "<BODY>Success!!!</BODY></HTML>\r\n");
  send(client, &buf, strlen(buf), 0);
  // 结束时，关闭client的连接
  close(client);
}
```

该方法通过调用read_line 打印出每一行请求头, 并在之后发送对应的信息。

## 测试

    gcc -o app client_sock.c
    ./app

浏览器打开对应端口, 当页面看到`Success!!!`时表明正确执行。同时也可以使用`curl http://localhost:${port} -v` 查看详细请求过程。

## 下一章

[发送文件到浏览器](./accept_request.md)