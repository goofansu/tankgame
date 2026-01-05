#!/usr/bin/env bash
set -e

mkdir -p build-web
cd build-web

emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DPZ_WEB_BUILD=ON

cmake --build . --parallel
