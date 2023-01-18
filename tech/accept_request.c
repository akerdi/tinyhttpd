#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

void error_die(char* err) {
  perror(err);
  exit(1);
}

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

void accept_request(int client) {
  char buf[1024];
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

  if (*port == 0) {
    int name_len = sizeof(name);
    if (getsockname(httpd, &name, &name_len) < 0) error_die("getsockname");
    // 得到可用的port后回填到参数中
    *port = ntohs(name.sin_port);
  }
  // 启动监听动作
  if (listen(httpd, 5) < 0) error_die("listen");

  return httpd;
}

int main() {
  int server_sock = -1;
  u_short port = 0;
  // 启动服务
  server_sock = startup(&port);
  printf("server start at port: %d\n", port);

  int client_sock = -1;
  struct sockaddr_in name;
  int name_len = sizeof(name);
  while (1) {
    // accept会阻塞等待客户端的连接
    client_sock = accept(server_sock, &name, &name_len);
    if (client_sock < 0) error_die("accept");
    // 连接后的业务处理逻辑
    accept_request(client_sock);
  }

  exit(EXIT_SUCCESS);
}