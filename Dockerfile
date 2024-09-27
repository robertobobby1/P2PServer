FROM alpine:latest AS build
RUN apk update && \
    apk add --no-cache \
        cmake=3.29.3-r0 make gcc g++

WORKDIR /P2PServer
COPY P2PServer/ ./P2PServer/
COPY CMakeLists.txt . 

WORKDIR /P2PServer/build
RUN cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build . --parallel 8



FROM alpine:3.17.0

RUN apk update && \
    apk add --no-cache \
    libstdc++=12.2.1_git20220924-r4

RUN addgroup -S p2pServer && adduser -S p2pServer -G p2pServer
USER p2pServer

COPY --chown=p2pServer:p2pServer --from=build \
    ./P2PServer/bin \
    ./app/

EXPOSE 3000

ENTRYPOINT ["./app/P2PServer"]
