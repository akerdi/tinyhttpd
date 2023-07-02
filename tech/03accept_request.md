# 发送文件到浏览器

本章目标是分析请求头相关信息，并最终发送文件至浏览器。

本章主要是解读函数`void(*accept_request)(int client)`.

## 解析请求头信息

删除上一章accept_request中的内容，改为解析请求头信息:

```c
void accept_request(int client) {
  ...
  // 从socket拿出第一行
+ read_count = read_line(client, buf, sizeof(buf));
+ i = 0; j = 0;
+ // 没有遇到空字符，并且i小于本身申请空间大小
+ while (!DEF_CHAR_ISSPACE(buf[j]) && (i < sizeof(method)-1)) {
+   method[i] = buf[j];
+   i++; j++;
+ }
+ method[i] = '\0';
+ if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
+   unimplemented(client);
+   return;
+ }
+ // 如果方法为POST 则表示需要处理动作
+ if (strcasecmp(method, "POST") == 0) cgi = 1;
+ // 忽略空格
+ while (DEF_CHAR_ISSPACE(buf[j]) && j < sizeof(buf)) j++;
+ // 读取url和method一样
+ i = 0;
+ while (!DEF_CHAR_ISSPACE(buf[j])  && (i < sizeof(url)-1) && j < sizeof(buf)) {
+   url[i] = buf[j];
+   i++; j++;
+ }
+ url[i] = '\0';
```

上面获得了请求头中method 和url 信息，接下来需要分解url 为`path`和`query_string`:

```c
void accept_request(int client) {
  ...
+ // 获取query_string
+ if (strcasecmp(method, "GET") == 0) {
+   // query_string 指针与url指针一样指向同一块地址
+   query_string = url;
+   // 让query_string 指针不断向后移动，直到遇到'?'或者'\0'时停止
+   while (*query_string != '?' && *query_string != '\0') query_string++;
+   if (*query_string == '?') {
+     // 通过设置'?'为\0, 那么url则全部都是path
+     // /search?q=tinyhttpd ->
+     // /search\0q=tinyhttpd
+     // ${url}\0${query_string}
+     // 这样一个内存空间被两个指针获得对应的数据
+     *query_string = '\0';
+     query_string++;
+     cgi = 1; // 表明该动作需要执行cgi
+   }
+ }
+ // 合并真实路径
+ sprintf(path, "tech/htdocs%s", url);
+ // 如果path 最后一个字符为'/', 说明需要补充index.html
+ if (path[strlen(path)-1] == '/') strcat(path, "index.html");
```

以上完成url分解为`path` 和`query_string`，并且为`path`填充业务需求。

接下来根据path查询是否有对应文件，没有则报告`404`，否则发送文件:

```c
void accept_request(int client) {
  ...
+ // 如果文件读取失败，则返回404文件不存在
+ // headers仍然需要全部读取完，否则客户端会认为非正常中断
+ if (stat(path, &st) < 0) {
+   while (read_count > 0 && strcmp(buf, "\n")) {
+     read_count = read_line(client, buf, sizeof(buf));
+   }
+   not_found(client, path);
+ } else {
+   // 判断如果文件描述为目录，则补充index.html
+   if ((st.st_mode & S_IFMT) == S_IFDIR) strcat(path, "/index.html");
+   if (cgi == 0) {
+     serve_file(client, path);
+   } else {
+     printf("execute_cgi...\n");
+   }
+ }
  // 结束时，关闭client的连接
  close(client);
}
```

数据已准备好，接下来就是实现发送文件函数`void(*serve_file)(const int, const FILE*)`:

```c
+void cat(const int client, const FILE* resource) {
+ char buf[1024];
+ fgets(buf, sizeof(buf), resource);
+ while (!feof(resource)) {
+   send(client, buf, strlen(buf), 0);
+   fgets(buf, sizeof(buf), resource);
+ }
+}

+void serve_file(const int client, const char* filepath) {
+ char buf[1024];
+ buf[0] = 'A'; buf[1] = '\0';
+ int read_count = strlen(buf);
+
+ while (read_count > 0 && strcmp(buf, "\n")) {
+   read_count = read_line(client, buf, 1024);
+ }
+ FILE* resource = fopen(filepath, "r");
+ if (!resource) {
+   not_found(client, filepath);
+ } else {
+   headers(client);
+   cat(client, resource);
+ }
+ fclose(resource);
+}
```

这两个方法还是比较比较简单，`fopen`文件，将文件内容读取并传输到浏览器。

这里的serve_file中读取(`read_line`)所有头文件是必要的，如果不读取，则前端发送过来的数据，没有人读取，同样会报错: `curl: (56) Recv failure: Connection reset by peer`; 或者浏览器控制台报错: `GET http://localhost:50781/ net::ERR_CONNECTION_RESET 200 (OK)`

## 测试

    gcc -o app accept_request.c
    ./app # 浏览器打开网页看见页面

> 如果网页没有正常展示，尝试执行`chmod 600 htdocs/index.html` 将index.html文件改为非执行权限

## 下一章

[执行浏览器发送的命令](./execute_cgi.md)
