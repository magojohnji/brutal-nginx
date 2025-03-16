## Brutal Nginx

vps需要先安装 [TCP Brutal](https://github.com/apernet/tcp-brutal)

[brutal_nginx的博客](https://blog.smallparking.eu.org/posts/brutal-nginx/)

## nginx 配置

支持 tcp_brutal 配置在 http,server 上

支持 tcp_brutal_rate 配置在http,server,location上

```conf
http {

# Enable tcp brutal
  tcp_brutal on;
  tcp_brutal_rate 1048576;

server {
  listen 8080;

  root /var/www/html;
    tcp_brutal_rate 1048576;    
  location / {
    # Send rate in bytes per second
    tcp_brutal_rate 1048576;
  }
}

server {
  listen 8099;
  tcp_brutal off; # not brutal
  ... 
}
}
```

## 安装 Nginx

二进制在github action上

## 日志

在info 级别的error_log 会有日志提示

```
2024/12/18 08:44:12 [info] 7763#7763: *1 Brutal TCP flow control is enabled for request from a.b.c.d with rate 1048576 bytes/s, client: a.b.c.d, server: localhost, request: "GET / HTTP/1.1", host: "a.b.c.d:8099"
```
