/*****
 * Non Daemonizing server 
 * Author Adam Metcalf
 * (c) 2008
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ftp.h"

#ifdef __PSP__

#include <arpa/inet.h>
#include <sys/select.h>
#include "psp_wifi.h"

#elif __DS__

#include "../../ds_wifi.h"

#else

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include "clientcmd.h"

#define __SIGNALS
#define SERVER_PORT 5000

#endif

#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <netinet/in.h>


/* global set so the eval switch in connection handling can have access to it simply */
fd_set savedset_read;
fd_set savedset_write;
int block = 1;

#ifdef __SIGNALS
int sigterm=0, sigint=0;
void sighandler(int signo){
	if(signo == SIGTERM) sigterm = 1;
	if(signo == SIGINT) sigint = 1;
}
#endif

#ifdef __PSP__

#if ( _PSP_FW_VERSION >= 300 )
/* user module is not a thread */
int server_loop(void *argv){
#else
/* threads on the PSP take a size argument */
int server_loop(SceSize args, void *argv){
#endif
#else
int server_loop(void *argv){
#endif

	int i;
    fd_set rset;
    fd_set wset;

	int server_sock = *(int*)argv;
	
	FD_ZERO(&savedset_read);
    FD_SET(server_sock, &savedset_read);
#ifdef __DS__
	ioctl(server_sock,FIONBIO,&block);
#endif

	struct timeval tv;
	tv.tv_sec=1;
	tv.tv_usec=0;
	ftpOpenLog();

/*    savedset = set;*/
	printf("Starting Server\n");
#ifdef __PSP__
	while(running) {
		running = platformServerCheckExit();
#elif __DS__
	while(1) {
#else
	while(!sigint && !sigterm) {
#endif
		rset = savedset_read;
		wset = savedset_write;
		if(select(FD_SETSIZE, &rset, &wset, NULL, NULL) < 0) {
#ifdef __SIGNALS
			if(errno == ERESTART || errno == EINTR)
				break;
#endif
			printf( "select error %s\n", strerror(errno));
		}
#ifdef __PSP__
		platformServerUpdate();
#endif
#ifdef DEBUG
		printConnections();
		printf("\n");
#endif
		/*
		 * This prioritizes opening connections cause some servers jump the gun with retr's
		 * you end up with a pending accept() and a user connection saying TRANSFER now. 
		 * well lower socket wins right... well then my mom got scared said you're 
		 * movin' with your auntie and uncle in Bel-Air. I whistled for a cab and when
		 * it came near The license plate said fresh and it had dice in the mirror 
		 * If anything I could say that this cab was rare But I thought, 
		 * Nah, forget it. Yo home to Bel-Air!
		 */
		for(i = 0; i < FD_SETSIZE; i++) {
			if(FD_ISSET(i, &rset)) {
				if(connections[i] != NULL) {
					if(connections[i]->ctype == PASV_LISTEN || connections[i]->ctype == MASTER_LISTEN) {
						handleSocket(connections[i]);
						FD_CLR(i, &rset);
					}
				}
			}
		}
		for(i = 0; i < FD_SETSIZE; i++) {
			if(FD_ISSET(i, &rset)) {
				if(connections[i] != NULL) {
					handleSocket(connections[i]);
				}
			}
			else if(FD_ISSET(i, &wset)) {
				if(connections[i] != NULL) {
					handleSocket(connections[i]);
				}
			}
		}
	}
	printf("shutting down\n");
	for(i = 0; i < FD_SETSIZE; i++) {
		if(FD_ISSET(i, &savedset_read)) {
			closeConnection(connections[i]);
		}
		if(FD_ISSET(i, &savedset_write)) {
			closeConnection(connections[i]);
		}
	}
	ftpCloseLog();
	return 0;
}

#ifndef __PSP__
#ifndef __DS__
void client_test(char *host, int port);


int main(int argc, char **argv) {
	char *my_ip;
	struct sockaddr_in my_addr;  /* my server address info */
	struct sigaction sighand;

	sighand.sa_handler = sighandler;
	sigemptyset (&sighand.sa_mask);
	sighand.sa_flags = 0;
	sigaction(SIGINT, &sighand, NULL);
	sigaction(SIGTERM, &sighand, NULL);

	int i;
	for(i = 0; i < MAX_SOCKETS; i++) {
		connections[i] = NULL;
	}

	if(argc > 1) {
		client_test(argv[1], atoi(argv[2]));
		return 0;
	}
	
	struct addrinfo *result;
	if(getaddrinfo("gambloren", NULL, NULL, &result) == -1) {
		printf("addrinfo error %s",strerror(errno));
	}
	/*
	char hostname[80];
	struct hostent *hostent_ptr;
	gethostname (hostname, 80);
	hostent_ptr = gethostbyname(hostname);
	
	my_addr.sin_addr.s_addr = ((struct in_addr *)hostent_ptr->h_addr_list[0])->s_addr;
	for(i = 0; hostent_ptr->h_addr_list[i] != NULL ; i++) {
		if((((struct in_addr *)hostent_ptr->h_addr_list[i])->s_addr & 0xFF) == 0x7f)
			printf("i got the localaddress %lx\n", (unsigned long)((struct in_addr *)hostent_ptr->h_addr_list[i])->s_addr);
		else {
			printf("i got  %lx\n", (unsigned long)((struct in_addr *)hostent_ptr->h_addr_list[i])->s_addr);
			break;
		}
	}
	*/
	
	if((((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr & 0xFF) == 0x7f) {
		printf("i got the localaddress %x\n", ((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr & 0xFF);
		while(result->ai_next && (((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr & 0xFF) == 0x7f) {
			printf("still local %x\n", ((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr & 0xFF);

			result = result->ai_next;
		}
	}
	
	
	connection *master = initMasterListen(&my_addr, SERVER_PORT);
	if(!master) {
		fprintf(stderr,"Cannot use port 5000\n");
		return 1;
	}

	/*my_addr.sin_addr.s_addr = (unsigned long)((struct in_addr *)hostent_ptr->h_addr_list[i])->s_addr;*/
	printf("%lx\n",(unsigned long)((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr);
	my_addr.sin_addr.s_addr = ((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr;
	freeaddrinfo(result);
	/*
	my_addr.sin_addr.s_addr = ((struct in_addr *)hostent_ptr->h_addr_list[0])->s_addr;
	printf("host:%s 0x%lx\n",hostname,(unsigned long)my_addr.sin_addr.s_addr);
	*/

	my_ip = inet_ntoa(my_addr.sin_addr);
	master->port = SERVER_PORT;
	sscanf(my_ip,"%d.%d.%d.%d",&master->ip[0],&master->ip[1],&master->ip[2],&master->ip[3]);
	
	printf("Master listening on %s:%d\n", my_ip, ntohs(my_addr.sin_port));

	/* select loop */
	server_loop((void*)&master->sock);

	return 0;
}
#endif
#endif
