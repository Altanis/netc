#ifndef ERROR_H
#define ERROR_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <sys/errno.h>
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

/** syscalls */
#define EVCREATE        1     /** kqueue and epoll_create1 syscalls */
#define SOCKET          2     /** socket syscall */
#define POLL_FD         3     /** kevent, epoll_wait, and epoll_ctl syscalls */
#define BIND            4     /** bind syscall */
#define LISTEN          5     /** listen syscall */
#define ACCEPT          6     /** accept syscall */
#define BADSEND         7     /** send syscall */
#define BADRECV         8     /** receive syscall */
#define CLOSE           9     /** close syscall */
#define FCNTL          10     /** fcntl syscall */
#define CONNECT        11     /** connect syscall */
#define SOCKOPT        12     /** socket options */
#define HANGUP         13     /** unexpected socket hangup */
#define INETPTON       14     /** inet_pton syscall */
#define WSASTARTUP     15     /** WSAStartup() */

/** Prints the current error. */
void netc_perror();

#endif // ERROR_H