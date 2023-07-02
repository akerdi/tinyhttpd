#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEF_CHAR_ISSPACE(x) (x == ' ')
#define DEF_SEND_CLIENT send(client, buf, strlen(buf), 0)
#define DEF_SERVER_STRING "Server: tinyhttpd/0.0.1\r\n"

void serve_file(const int, const char*);
void cat(const int, const FILE*);
void execute_cgi(const int, const char*, const char*, const char*);

void unimplemented(const int);
void not_found(const int, const char*);
void headers(const int);
void bad_request(const int);
void cannot_execute(const int);

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
  int read_count = 0;

  char method[256];
  char url[512];
  char path[512];
  char *query_string;
  int i, j;
  int cgi = 0;
  struct stat st;

  read_count = read_line(client, buf, sizeof(buf));
  // 读取method
  i = 0; j = 0;
  while (!DEF_CHAR_ISSPACE(buf[j]) && (i < (sizeof(method)-1))) {
    method[i] = buf[j];
    i++; j++;
  }
  method[i] = '\0';
  if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
    unimplemented(client);
    return;
  }
  // 如果方法为POST 则表示
  if (strcasecmp(method, "POST") == 0) cgi = 1;
  // 忽略空格
  while (DEF_CHAR_ISSPACE(buf[j]) && j < sizeof(buf)) j++;

  // 读取url
  i = 0;
  while (!DEF_CHAR_ISSPACE(buf[j])  && (i < sizeof(url)-1) && j < sizeof(buf)) {
    url[i] = buf[j];
    i++; j++;
  }
  url[i] = '\0';

  // 获取query_string
  if (strcasecmp(method, "GET") == 0) {
    // query_string 指针与url指针一样指向同一块地址
    query_string = url;
    // 让query_string 指针不断向后移动，直到遇到'?'或者'\0'时停止
    while (*query_string != '?' && *query_string != '\0') query_string++;
    if (*query_string == '?') {
      // 通过设置'?'为\0, 那么url则全部都是path
      // /search?q=tinyhttpd ->
      // /search\0q=tinyhttpd
      // ${url}\0${query_string}
      // 这样一个内存空间被两个指针获得对应的数据
      *query_string = '\0';
      query_string++;
      cgi = 1; // 表明该动作需要执行cgi
    }
  }
  // 合并真实路径
  sprintf(path, "htdocs%s", url);
  // 如果path 最后一个字符为'/', 说明需要补充index.html
  if (path[strlen(path)-1] == '/') strcat(path, "index.html");
  // 如果文件读取失败，则返回404文件不存在
  // headers仍然需要全部读取完，否则客户端会认为非正常中断
  if (stat(path, &st) < 0) {
    while (read_count > 0 && strcmp(buf, "\n")) {
      read_count = read_line(client, buf, sizeof(buf));
    }
    not_found(client, path);
  } else {
    // 判断如果文件描述为目录，则补充index.html
    if ((st.st_mode & S_IFMT) == S_IFDIR) strcat(path, "/index.html");
    if ((st.st_mode & S_IXUSR) ||
        (st.st_mode & S_IXGRP) ||
        (st.st_mode & S_IXOTH))
        cgi = 1;
    if (cgi == 0) {
      serve_file(client, path);
    } else {
      execute_cgi(client, path, method, query_string);
    }
  }

  // 结束时，关闭client的连接
  close(client);
}

void execute_cgi(const int client, const char* path, const char* method, const char* query_string) {
  char buf[1024];
  buf[0] = 'A'; buf[1] = '\0';
  int read_count = strlen(buf);

  int pipe_in[2];
  int pipe_out[2];
  int content_length = -1;
  pid_t pid;
  int status;
  char c = '\0';

  if (strcasecmp(method, "GET") == 0) {
    while (read_count > 0 && strcmp(buf, "\n")) {
      read_count = read_line(client, buf, sizeof(buf));
    }
  } else {
    while (read_count > 0 && strcmp(buf, "\n")) {
      read_count = read_line(client, buf, sizeof(buf));
      // 由于不需要读取除了Content-Length之外的数据
      // `Content-Length:`后接一个空格字符, 所以直接截取第15个字符为\0后比对
      // 比对成功，则buf[16]开始就为长度数据
      buf[15] = '\0';
      if (strcmp(buf, "Content-Length:") == 0) {
        content_length = atoi(&(buf[16]));
      }
    }
    if (content_length == -1) {
      bad_request(client);
      return;
    }
  }
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  DEF_SEND_CLIENT;

  if (pipe(pipe_in) < 0) {
    cannot_execute(client);
    return;
  }
  if (pipe(pipe_out) < 0) {
    cannot_execute(client);
    return;
  }
  if ((pid = fork()) < 0) {
    cannot_execute(client);
    return;
  }
  if (pid == 0) {
    close(pipe_in[1]);
    close(pipe_out[0]);
    char meth_env[512];
    char query_env[512];
    char length_env[512];

    char body[1024];
    char c = '\0';
    int i = 0;
    // 使用dup2(pipe_in[0], 0) 经测试无法获取到color环境变量
    while (read(pipe_in[0], &c, 1) > 0) {
      body[i] = c;
      i++;
    }
    body[i] = '\0';
    putenv(body);
    close(pipe_in[0]);

    dup2(pipe_out[1], 1);

    sprintf(meth_env, "REQUEST_METHOD=%s", method);
    putenv(meth_env);
    if (strcmp(method, "GET") == 0) {
      sprintf(query_env, "QUERY_STRING=%s", query_string);
      putenv(query_env);
    } else {
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
      putenv(length_env);
    }
    execl(path, path, NULL);
    exit(0);
  } else {
    close(pipe_in[0]);
    close(pipe_out[1]);

    if (strcasecmp(method, "POST") == 0) {
      for (int i = 0; i < content_length; i++) {
        recv(client, &c, 1, 0);
        write(pipe_in[1], &c, 1);
      }
    }
    // close写端，让子进程的读端终止
    close(pipe_in[1]);
    while (read(pipe_out[0], &c, 1) > 0) {
      send(client, &c, 1, 0);
    }
    close(pipe_out[0]);
    // 执行waitpid 为子进程收拾进程数据，否则会成为僵尸进程
    waitpid(pid, &status, 0);
  }
}

void cat(const int client, const FILE* resource) {
  char buf[1024];
  fgets(buf, sizeof(buf), resource);
  while (!feof(resource)) {
    DEF_SEND_CLIENT;
    fgets(buf, sizeof(buf), resource);
  }
}

void serve_file(const int client, const char* filepath) {
  char buf[1024];
  buf[0] = 'A'; buf[1] = '\0';
  int read_count = strlen(buf);

  while (read_count > 0 && strcmp(buf, "\n")) {
    read_count = read_line(client, buf, 1024);
  }
  FILE* resource = fopen(filepath, "r");
  if (!resource) {
    not_found(client, filepath);
  } else {
    headers(client);
    cat(client, resource);
  }
  fclose(resource);
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

void not_found(const int client, const char* filepath) {
  char buf[1024];

  sprintf(buf, "HTTP/1.1 404 Not Found\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, DEF_SERVER_STRING);
  DEF_SEND_CLIENT;
  sprintf(buf, "Content-Type: text/html\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, "\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, "<P>Server not found resource:</P>\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, "<B>%s<B>\r\n", filepath);
  DEF_SEND_CLIENT;
}
void unimplemented(const int client) {
  char buf[1024];
  sprintf(buf, "HTTP/1.1 501 Method Not Implemented\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, DEF_SERVER_STRING);
  DEF_SEND_CLIENT;
  sprintf(buf, "Content-Type: text/html\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, "\r\n");
  DEF_SEND_CLIENT;
}

void headers(const int client) {
  char buf[1024];
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, DEF_SERVER_STRING);
  DEF_SEND_CLIENT;
  sprintf(buf, "Content-Type: text/html\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, "\r\n");
  DEF_SEND_CLIENT;
}

void cannot_execute(const int client) {
  char buf[1024];
  sprintf(buf, "HTTP/1.1 500 CAN NOT EXECUTE\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, DEF_SERVER_STRING);
  DEF_SEND_CLIENT;
  sprintf(buf, "Content-Type: text/html\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, "\r\n");
  DEF_SEND_CLIENT;
}
void bad_request(const int client) {
  char buf[1024];
  sprintf(buf, "HTTP/1.1 400 BAD REQUEST\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, DEF_SERVER_STRING);
  DEF_SEND_CLIENT;
  sprintf(buf, "Content-Type: text/html\r\n");
  DEF_SEND_CLIENT;
  sprintf(buf, "\r\n");
  DEF_SEND_CLIENT;
}