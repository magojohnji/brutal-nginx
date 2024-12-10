
# Brutal Nginx

 Using [TCP Brutal](https://github.com/apernet/tcp-brutal) congestion control algorithm in NGINX 

# NGINX Configuration

Add this to `/etc/nginx/nginx.conf`

```
load_module /path/to/ngx_http_tcp_brutal_module.so;
```


```
server {
  listen 8080;

  root /var/www/html;

  # Enable tcp brutal
  tcp_brutal on;
    
  location / {
    # Send rate in bytes per second
    tcp_brutal_rate 1048576;
  }
}
```

# Build

1. Start developement container (Optional)

```
docker run -it -v ./:/data -p 8080:8080 debian:latest /bin/bash
```

2. Install packages

```
apt update
apt install build-essential nginx unzip git wget curl libpcre3-dev zlib1g-dev
```

3. Copy moudle source code

```
mkdir ~/nginx-brutal
cd ~/nginx-brutal
cp /data/config .
cp /data/ngx_http_tcp_brutal_module.c .
```

4. Downlaod NGINX source code

```
cd ~
wget http://nginx.org/download/nginx-1.18.0.tar.gz
tar -xf nginx-1.18.0.tar.gz
cd ~/nginx-1.18.0
```

5. Compile
```
./configure --with-compat --add-dynamic-module=/root/nginx-brutal
make modules
```

6. Compiled binary is in `./objs/ngx_http_tcp_brutal_module.so`

# Build Rocky Linux

1. start build container
```
docker pull rockylinux:9.3.20231119
docker run -it  rockylinux:9.3.20231119 /bin/bash
```

2. packages
```
yum update -y
yum install -y gcc git wget pcre-devel zlib-devel
```

3. donwload source 
```
docker cp brutal-nginx 62aa4a134191:/root
wget https://nginx.org/download/nginx-1.20.1.tar.gz
tar xf nginx-1.20.1.tar.gz && cd nginx-1.20.1
./configure --with-compat --add-dynamic-module=/root/brutal-nginx
make modules
```

4. copy binary module
```
ls -lhat ./objs/ngx_http_tcp_brutal_module.so
docker cp 62aa4a134191:/root/nginx-1.20.1/objs/ngx_http_tcp_brutal_module.so .
```
