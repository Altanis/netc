#ifndef ERROR_H
#define ERROR_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <errno.h>
#endif

#include <stdio.h>
#include <stdarg.h>

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
#define EVCREATE        1     /** kqueue, epoll_create1, and WSACreateEvent syscalls */
#define SOCKET_C        2     /** socket creation syscall */
#define POLL_FD         3     /** kevent, epoll_wait, and WSAPoll syscalls */
#define EVENT_SELECT    4     /** WSAEventSelect syscall */
#define NETWORK_EVENT   5     /** WSAEnumNetworkEvents syscall */
#define WSA_WAIT        6     /** WSAWaitForMultipleEvents syscall */
#define BIND            6     /** bind syscall */
#define LISTEN          7     /** listen syscall */
#define ACCEPT          8     /** accept syscall */
#define BADSEND         9     /** send syscall */
#define BADRECV        10     /** receive syscall */
#define CLOSE          11     /** close and closesocket syscalls */
#define FD_CTL         12     /** ioctl/fcntl syscall */
#define CONNECT        13     /** connect syscall */
#define HANGUP         14     /** unexpected socket hangup */
#define INETPTON       15     /** inet_pton syscall */
#define WSA_STARTUP    16     /** WSAStartup() */

/** Writes the error to a buffer. */
void netc_strerror(char *buffer);
/** Prints the error to a file stream. */
void netc_perror(const char *message, ...);

#endif // ERROR_H