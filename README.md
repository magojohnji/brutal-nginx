[中文](./zh.md)

## Brutal Nginx
The VPS needs to have TCP Brutal installed first.

## Nginx Configuration
The tcp_brutal directive is supported in the http block. The tcp_brutal_rate directive is supported in http, server, and location blocks.

```conf
http {

# Enable tcp brutal
  tcp_brutal on;
  tcp_brutal_rate 1048576;
}
server {
  listen 8080;

  root /var/www/html;
  tcp_brutal_rate 1048576;
  
  location / {
    # Send rate in bytes per second
    tcp_brutal_rate 1048576;
  }
}
```
## Installing Nginx
The binary is available on GitHub Actions.


## log

The error_log will have log messages at the info level.
```
2024/12/18 08:44:12 [info] 7763#7763: *1 Brutal TCP flow control is enabled for request from a.b.c.d with rate 1048576 bytes/s, client: a.b.c.d, server: localhost, request: "GET / HTTP/1.1", host: "a.b.c.d:8099"
```