FROM ubuntu:20.04
WORKDIR /app
COPY . /app

RUN apt-get update && \
    apt-get install -y g++ && \
    chown -R root:root /app
USER root

RUN g++ -g -o MyProxy MyProxy.cpp MyProxy.h MyLog.cpp MyLog.h MyHandle.cpp MyHandle.h cache.cpp cache.hpp response.cpp response.hpp connectInfo.hpp -lpthread
CMD ["./MyProxy"]
