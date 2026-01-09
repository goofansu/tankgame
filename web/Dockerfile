FROM --platform=linux/amd64 emscripten/emsdk:3.1.51

RUN apt-get update && apt-get install -y rsync && rm -rf /var/lib/apt/lists/*

WORKDIR /src
