#ifndef ERROR_H
#define ERROR_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <sys/errno.h>
#endif

#include <stdio.h>

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
#define SOCKET_C        2   /** socket creation syscall */
#define POLL_FD         3     /** kevent, epoll_wait, and epoll_ctl syscalls */
#define BIND            4     /** bind syscall */
#define LISTEN          5     /** listen syscall */
#define ACCEPT          6     /** accept syscall */
#define BADSEND         7     /** send syscall */
#define BADRECV         8     /** receive syscall */
#define CLOSE           9     /** close syscall */
#define FD_CTL         10     /** ioctl/fcntl syscall */
#define CONNECT        11     /** connect syscall */
#define HANGUP         12     /** unexpected socket hangup */
#define INETPTON       13     /** inet_pton syscall */
#define WSASTARTUP     14     /** WSAStartup() */

/** Writes the error to a buffer. */
void netc_strerror(char* buffer);
/** Prints the error to a file stream. */
void netc_perror(const char* message, FILE* stream);

#endif // ERROR_H