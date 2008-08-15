#ifndef __PSP_ADHOC_H__
#define __PSP_ADHOC_H__

#include <sys/select.h>
#include <pspsdk.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <pspnet_adhocmatching.h>
#include <pspnet_resolver.h>

#define send pspSend
#define recv pspRecv
#define select pspSelect
#define connect pspConnect
#define bind pspBind
#define listen pspListen
#define accept pspAccept
#define shutdown pspShutdown
#define close pspClose
#define socket pspSocket

extern unsigned short adhoc;
extern unsigned short adhoc_port;
extern unsigned short adhoc_other_port;

int pspRecv(int s, void *buf, size_t len, int flags);
int pspSend(int s, const void *buf, size_t len, int flags);
int pspSelect(int nfds, fd_set *readfds, fd_set *writefds,
		                  fd_set *exceptfds, struct timeval *timeout);
int pspConnect(int sockfd, const struct sockaddr *serv_addr,
		                   socklen_t addrlen);
int pspBind(int sockfd, const struct sockaddr *addr,
		                socklen_t addrlen);
int pspListen(int sockfd, int backlog);
int pspAccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int pspShutdown(int s, int how);
int pspClose(int fd);
int pspSocket(int domain, int type, int protocol);

#endif
