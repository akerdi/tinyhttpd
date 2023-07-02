# 执行浏览器发送的命令

本章实现客户端发来的POST请求color.cgi, 该脚本由于有执行权限，所以`cgi = 1`, 调用函数`void(*execute_cgi)(const int, const char*, const char*, const char*)`, 判断过程为:

```c
  ...
    if ((st.st_mode & S_IFMT) == S_IFDIR) strcat(path, "/index.html");
+   if ((st.st_mode & S_IXUSR) ||
+       (st.st_mode & S_IXGRP) ||
+       (st.st_mode & S_IXOTH))
+       cgi = 1;
    if (cgi == 0) {
      serve_file(client, path);
    } else {
-     printf("execute_cgi...\n");
+     execute_cgi(client, path, method, query_string);
    }
  ...
```

`execute_cgi`函数主要是创建一个子进程，通过读取(`read`)pipe传递子进程执行的结果回主进程，主进程转发到客户端。开始之前，先获取完请求头所需信息，并及时发送状态码到客户端:

```c
void execute_cgi(const int client, const char* path, const char* method, const char* query_string) {
  ...
+ if (strcasecmp(method, "GET") == 0) {
+   while (read_count > 0 && strcmp(buf, "\n")) {
+     read_count = read_line(client, buf, sizeof(buf));
+   }
+ } else {
+   while (read_count > 0 && strcmp(buf, "\n")) {
+     read_count = read_line(client, buf, sizeof(buf));
+     // 由于不需要读取除了Content-Length之外的数据
+     // `Content-Length:`后接一个空格字符, 所以直接截取第15个字符为\0后比对
+     // 比对成功，则buf[16]开始就为长度数据
+     buf[15] = '\0';
+     if (strcmp(buf, "Content-Length:") == 0) {
+       content_length = atoi(&(buf[16]));
+     }
+   }
+   if (content_length == -1) {
+     bad_request(client);
+     return;
+   }
+ }
+ sprintf(buf, "HTTP/1.1 200 OK\r\n");
+ send(client, buf, strlen(buf), 0);
```

接下来创建pipe、创建子进程，并且实现子进程:

```c
void execute_cgi(const int client, const char* path, const char* method, const char* query_string) {
  ...
+ if (pipe(pipe_in) < 0) {
+   cannot_execute(client);
+   return;
+ }
+ if (pipe(pipe_out) < 0) {
+   cannot_execute(client);
+   return;
+ }
+ if ((pid = fork()) < 0) {
+   cannot_execute(client);
+   return;
+ }
+ if (pid == 0) {
+   close(pipe_in[1]);
+   close(pipe_out[0]);
+   char meth_env[512];
+   char query_env[512];
+   char length_env[512];
+   char body[1024];
+   char c = '\0';
+   int i = 0;
+   // 使用dup2(pipe_in[0], 0) 经测试无法获取到color环境变量
+   while (read(pipe_in[0], &c, 1) > 0) {
+     body[i] = c;
+     i++;
+   }
+   body[i] = '\0';
+   putenv(body);
+   close(pipe_in[0]);
+   dup2(pipe_out[1], 1);
+   sprintf(meth_env, "REQUEST_METHOD=%s", method);
+   putenv(meth_env);
+   if (strcmp(method, "GET") == 0) {
+     sprintf(query_env, "QUERY_STRING=%s", query_string);
+     putenv(query_env);
+   } else {
+     sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
+     putenv(length_env);
+   }
+   execl(path, path, NULL);
+   exit(0);
+ }
```

子进程使用`dup2`指令使控制台写句柄同步到`pipe_out[1]`中，通过read读取主进程传进来的数据，写到控制台环境变量中。(原来的`dup2(pipe_in[0], 0)`经测试无法读取到环境变量中)。

要停止子进程`read`的阻塞，要在主进程`write`结束后，关闭写段: `close(pipe_in[1])`。

下面来实现主进程的处理过程, 1. method=POST时，将body数据通过pipe_in[1]发送(`write`)到子进程；同时通过读取(`read`)pipe_out[0], 转发处理结果至浏览器:

```c
void execute_cgi(const int client, const char* path, const char* method, const char* query_string) {
  ...
+ else {
+   // 关闭不需要的信道
+   close(pipe_in[0]);
+   close(pipe_out[1]);
+   // 将浏览器发来的数据(\r\n后面的Body数据)写入到子进程
+   if (strcasecmp(method, "POST") == 0) {
+     for (int i = 0; i < content_length; i++) {
+       recv(client, &c, 1, 0);
+       write(pipe_in[1], &c, 1);
+     }
+   }
+   // close写端，让子进程的读端终止
+   close(pipe_in[1]);
+   // 将子进程数据转发至浏览器
+   while (read(pipe_out[0], &c, 1) > 0) {
+     send(client, &c, 1, 0);
+   }
+   // 关闭使用完的信道
+   close(pipe_out[0]);
+   // 执行waitpid 为子进程收拾进程数据，否则会成为僵尸进程
+   waitpid(pid, &status, 0);
+ }
}
```

以上通过pipe来实现进程间的通讯.

## 测试

由于cgi是早期产物, `color.cgi`文件改为bash脚本, 使用前修改下文件权限: `chmod +x color.cgi`

    gcc -o app execute_cgi.c
    ./app

当用户页面填写`red`点击按钮后，跳到新页面，并展示 [图片结果](../images/tech/execute_cgi_0.jpg)



