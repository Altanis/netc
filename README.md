# netc

A UNIX compliant TCP wrapper which utilizes asynchronous polling. Written in C.

## Features:
- [x] TCP Server and Client
- [x] IPv6 Support
- [x] Asynchronous Callback System
- [x] Blocking vs Non-Blocking Mode
- [ ] HTTP and WS abstractions
- [ ] SSL/TLS Support
- [ ] Socket Flag Support
- [ ] WindowsAPI Support <!-- Who would even use Windows for a tcp server lmao -->

## Usage:
```c
#include "netc/server.h"
#include "netc/client.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

```