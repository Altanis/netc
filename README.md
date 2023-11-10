# netc

A cross-platform networking library for TCP, UDP, HTTP, and WS sockets. Written in C.

## Prerequisites:
This library requires OpenSSL for SHA1 functionality.

## Features:

- [X] TCP/UDP Server and Client
- [X] Blocking and Non-Blocking Mode
- [X] HTTP/1.1 Abstraction
    - [X] Server
        - [X] Routing
        - [X] Query String Decoding
        - [X] URL Percent Encoding/Decoding
        - [X] Wildcards
        - [X] Chunked Encoding
        - [X] Binary Data
        - [X] Keep Alive
            - [ ] Timeout 
        - [ ] Compression/Decompression
    - [X] Client
        - [X] URL Percent Encoding/Decoding
        - [X] Query String Encoding
        - [X] Chunked Encoding
        - [X] Binary Data
        - [X] Keep Alive
        - [ ] Compression/Decompression
- [ ] WebSocket Abstraction
    - [x] Multi-Framed Messages
    - [x] Masked Messages
    - [ ] Protocols
    - [ ] Ping/Pong
    - [ ] Permessage-Deflate support
- [ ] Threadpool
- [ ] SSL/TLS Support (for HTTP/1.1 and WS Client)
<!-- In my opinion, there is no point of providing SSL suport for servers due to reverse proxies providing them. -->
- [ ] Linux/Windows Support
- [ ] Buffering
- [ ] HTTP/2 Abstractions
- [ ] HTTP/3 and QUIC Abstractions

## Usage:

For usage and documentations, please [refer to this folder for documentation and usage guide](https://github.com/Altanis/netc/tree/main/docs).
