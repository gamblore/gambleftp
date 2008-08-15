/**
 *
 * THIS DOES NOT WORK. 
 *
 * This is left in simply for educational purposes. It was an attempt to create an
 * example for PTP which currently has none around.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <pspsdk.h>
#include <pspwlan.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <pspnet_adhocmatching.h>
#include <pspnet_resolver.h>
#include <pspdebug.h>

#define printf pspDebugScreenPrintf

extern int adhoc;
extern unsigned char *adhoc_mac; /* this is my paired mac */
unsigned char mymac[8];
unsigned short adhoc_port = 0;
unsigned short adhoc_other_port = 0;




/* some hackey shit protocol will be adhoc_port to connect to */
int pspSocket(int domain, int type, int protocol) {
	if(adhoc) {
		if(adhoc_port == 0 || adhoc_other_port == 0)
			return 0;
		printf("Adhoc Socket port: %d otherport: %d\n",adhoc_port, adhoc_other_port);
		sceWlanGetEtherAddr(mymac);
		return sceNetAdhocPtpOpen(mymac, adhoc_port, adhoc_mac, adhoc_other_port, 8192, 60, 5, 0);
	}
	else {
		return socket(domain, type, protocol);
	}
}


int pspRecv(int s, void *buf, int len, int flags) {
	if(adhoc) {
		printf("Adhoc Recv socket: %d\n",s);
		/* 60 microsecond timeout nonblocking */
		if(sceNetAdhocPtpRecv(s, buf, &len, 60, 1) == 0)
			return len;
		else
			return -1;
	}
	else {
		return recv(s, buf, len, flags);
	}
}
int pspSend(int s, void *buf, int len, int flags) {
	if(adhoc) {
		printf("Adhoc Send socket: %d\n",s);
		if(sceNetAdhocPtpSend(s, buf, &len, 60, 1) == 0)
			return len;
		else
			return -1;
	}
	else {
		return send(s, buf, len, flags);
	}
}

int pspConnect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen) {
	int mysock;
	static unsigned short myport = 33000;
	if(adhoc) {
		sceWlanGetEtherAddr(mymac);
		mysock = sceNetAdhocPtpOpen(mymac, myport, adhoc_mac, adhoc_other_port, 8192, 60, 5, 0);
		printf("Adhoc connect socket: %d other_port: %d\n", sockfd, adhoc_other_port);
		myport++;
		return sceNetAdhocPtpConnect(mysock, 60, 1);
	}
	else {
		return connect(sockfd, serv_addr, addrlen);
	}
}

int pspBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	if(adhoc) {
		return 0;
	}
	else {
		return bind(sockfd, addr, addrlen);
	}
}

int pspListen(int sockfd, int backlog) {
	if(adhoc) {
		printf("Adhoc Listning on %d\n",adhoc_port);
		sceWlanGetEtherAddr(mymac);
		return sceNetAdhocPtpListen(mymac, adhoc_port, 8192, 60, 5, backlog, 0);
	}
	else {
		return listen(sockfd, backlog);
	}
}

int pspAccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	unsigned char tmpmac[8];
	unsigned short tmpport;
	static unsigned short myport = 12000;
	if(adhoc) {
		if( sceNetAdhocPtpAccept(sockfd, tmpmac, &tmpport, 60, 1) == 0) {
			printf("Adhoc Accepted port %d\n",tmpport);
			memcpy(adhoc_mac,tmpmac,6);
			myport++;
			return sceNetAdhocPtpOpen(mymac, myport, tmpmac, tmpport, 8192, 60, 5, 0);
		}
		else {
			return -1;
		}
	}
	else {
		return accept(sockfd, addr, addrlen);
	}
}

/* shutdown closes the connection close will just return */
int pspShutdown(int s, int how) {
	int ret;
	if(adhoc) {
		printf("Adhoc Shutdown\n");
		if( (ret = sceNetAdhocPtpClose(s, 0)) > -1)
			return 0;
		else
			return ret;
	}
	else
		return shutdown(s,how);
}
int pspClose(int fd) {
	if(adhoc)
		return 0;
	else
		return close(fd);
}


int pspSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	int result, length;
	fd_set return_read;
	fd_set return_write;
	fd_set return_execp;
	int blocking = 1;
	ptpStatStruct *this, *ptr;

	FD_ZERO(&return_read);
	FD_ZERO(&return_write);
	FD_ZERO(&return_execp);
	if(adhoc) {
		printf("Adhoc select\n");
		if(timeout == NULL) {
			blocking = 1;
		}
		do {
			result = sceNetAdhocGetPtpStat(&length, NULL);
			if (result < 0) {
				return -1;
			}
			if(result > 0) {
				/* there is data to deal with */
				blocking = 0;
			}
		} while(blocking);
		if(result == 0) {
			errno = EAGAIN;
			return -1;
		}
		this = malloc(length);
		if(this == NULL){
			return -1;
		}
		result = sceNetAdhocGetPtpStat(&length, this);
		if(result < 0){
			free(this);
			return -1;
		}
		else if(length == 0){
			free(this);
			return 0;
		}        
		else{
			for(ptr = this; ptr != NULL; ptr = ptr->next){
				printf("--socket: %d port: %d sent: %d recv: %d\n",ptr->ptpId, ptr->port, ptr->sentData, ptr->rcvdData);
				if(FD_ISSET(this->ptpId,readfds)) 
					FD_SET(this->ptpId,&return_read);
				else if(FD_ISSET(this->ptpId,writefds))
					FD_SET(this->ptpId,&return_write);
				else if(FD_ISSET(this->ptpId,exceptfds))
					FD_SET(this->ptpId,&return_execp);
			}
		}
		free(this);
		*readfds = return_read;
		*writefds = return_write;
		*exceptfds = return_execp;
		return length;
	}
	else {
		return select(nfds, readfds, writefds,exceptfds,timeout);
	}
}
