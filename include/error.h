#ifndef ERROR_H
#define ERROR_H

/** Sets the static error code for netc, and returns the current errno. */
#define netc_error(reason) (netc_errno_reason = reason, errno)

/** The syscall the `errno` error code has originated from. */
int netc_errno_reason;

/** ERROR CODES */
#define KEVENT      1 /** kevent syscall */
#define SOCKET      2 /** socket syscall */
#define POLL        3 /** polling syscalls (epoll, kqueue) */
#define BIND        4 /** bind syscall */
#define LISTEN      5 /** listen syscall */
#define ACCEPT      6 /** accept syscall */
#define SERVSEND    7 /** server send syscall */
#define SERVRECV    8 /** server receive syscall */
#define CLOSE       9 /** close syscall */
#define FCNTL      10 /** fcntl syscall */
#define CONNECT    11 /** connect syscall */
#define CLNTSEND   12 /** client send syscall */
#define CLNTRECV   13 /** client receive syscall */
#define SOCKOPT    14 /** socket options */

#endif // ERROR_H