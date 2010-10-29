#!/bin/bash

TAG="$1"
NGINX_VERSION="$2"
PREFIX="nginx-push-stream-module"

CONFIGURE_OPTIONS="--without-select_module \
--without-poll_module \
--without-http_charset_module \
--without-http_ssi_module \
--without-http_auth_basic_module \
--without-http_autoindex_module \
--without-http_geo_module \
--without-http_map_module \
--without-http_referer_module \
--without-http_fastcgi_module \
--without-http_memcached_module \
--without-http_limit_zone_module \
--without-http_limit_req_module \
--without-http_empty_gif_module \
--without-http_browser_module \
--without-http_upstream_ip_hash_module \
--without-mail_pop3_module \
--without-mail_imap_module \
--without-mail_smtp_module \
--with-http_stub_status_module \
--add-module=nginx-push-stream-module"

if [[ -z "$TAG" || -z "$NGINX_VERSION" ]]
then
    echo "Usage: $0 <tag> <nginx_version>"
    exit 1
fi

(./pack.sh $TAG && \
cd build && \
rm -rf nginx-${NGINX_VERSION}* && \
wget http://sysoev.ru/nginx/nginx-${NGINX_VERSION}.tar.gz && \
tar xzvf nginx-${NGINX_VERSION}.tar.gz && \
cd nginx-$NGINX_VERSION && \
tar xzvf ../$PREFIX-$TAG.tar.gz && \
./configure $CONFIGURE_OPTIONS && \
make && \
echo "
##############################################################
Build generated: build/nginx-$NGINX_VERSION

Configure options used:
$CONFIGURE_OPTIONS

To finish the process:
cd build/nginx-$NGINX_VERSION
sudo make install") || \
(echo "There was a problem building the module" ; exit 1)
echo "##############################################################"
