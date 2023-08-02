#ifndef ERROR_H
#define ERROR_H

#ifdef _WIN32
#include <winsock2.h>
#else
#include <errno.h>
#endif

#ifdef _WIN32
/** Sets the error code for netc, and returns the current errno. */
#define netc_error(reason) (netc_errno_reason = reason, WSAGetLastError())
#else
/** Sets the error code for netc, and returns the current errno. */
#define netc_error(reason) (netc_errno_reason = reason, errno)
#endif

/** The syscall the `errno` error code has originated from. */
extern __thread int netc_errno_reason;

/** ERROR CODES */
#define EVCREATE    1     /** kqueue and epoll_create1 syscalls */
#define SOCKET      2     /** socket syscall */
#define POLL_FD     3     /** kevent, epoll_wait, and epoll_ctl syscalls */
#define BIND        4     /** bind syscall */
#define LISTEN      5     /** listen syscall */
#define ACCEPT      6     /** accept syscall */
#define SERVSEND    7     /** server send syscall */
#define SERVRECV    8     /** server receive syscall */
#define CLOSE       9     /** close syscall */
#define FCNTL      10     /** fcntl syscall */
#define CONNECT    11     /** connect syscall */
#define CLNTSEND   12     /** client send syscall */
#define CLNTRECV   13     /** client receive syscall */
#define SOCKOPT    14     /** socket options */
#define HANGUP     15     /** unexpected socket hangup */
#define INETPTON   16     /** inet_pton syscall */
#define BADRECV    17     /** socket closed when trying to recv() */

#endif // ERROR_H