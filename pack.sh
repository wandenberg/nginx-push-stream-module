#!/bin/bash

TAG="$1"
PREFIX="nginx-push-stream-module"

if [[ -z "$TAG" ]]
then
    echo "Usage: $0 <tag>"
    exit 1
fi

mkdir -p build
git archive --format=tar --prefix=$PREFIX/ $TAG src config | gzip > build/$PREFIX-$TAG.tar.gz

echo "Package generated: build/$PREFIX-$TAG.tar.gz"
