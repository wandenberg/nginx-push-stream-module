#!/bin/bash

TAG="$1"
NGINX_VERSION="$2"
PREFIX="nginx-push-stream-module"

if [[ -z "$TAG" || -z "$NGINX_VERSION" ]]
then
    echo "Usage: $0 <tag> <nginx_version>"
    echo "Example: $0 master 0.7.67"
    exit 1
fi

CONFIGURE_OPTIONS="\
--with-http_stub_status_module \
--add-module=nginx-push-stream-module"

(chmod 755 *sh && \
./pack.sh $TAG && \
cd build && \
wget -N -c http://sysoev.ru/nginx/nginx-${NGINX_VERSION}.tar.gz && \
rm -rf nginx-${NGINX_VERSION} && \
tar -xzvf nginx-${NGINX_VERSION}.tar.gz && \
cd nginx-$NGINX_VERSION && \
tar -xzvf ../$PREFIX-$TAG.tar.gz && \
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
