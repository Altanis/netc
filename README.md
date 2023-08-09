# netc

A UNIX compliant TCP wrapper which utilizes asynchronous polling. Written in C.

## Features:

- [X] TCP Server and Client
- [X] IPv6 Support
- [X] Asynchronous Callback System
- [X] Blocking and Non-Blocking Mode
- [ ] UDP Server and Client
- [ ] Windows Support
- [ ] HTTP/2 and WS abstractions
- [ ] SSL/TLS Support
- [ ] Socket Flag Support
- [ ] HTTP/3 and QUIC abstractions

Usage:

```c
#include "netc/server.h"
#include "netc/client.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

```
