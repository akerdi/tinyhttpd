#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

void error_die(char* err) {
  perror(err);
  exit(1);
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

  exit(EXIT_SUCCESS);
}