#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "parser.h"
#include "ftp.h"
#include "ftpcmd.h"

#ifdef PSP_ADHOC
#include "psp_adhoc.h"
#endif

static FILE *logfile;
connection *connections[MAX_SOCKETS];


int openDataConnectionPASV(connection *c, transfer *t);
int openDataConnectionPORT(connection *c, transfer *t);

void ftpOpenLog() {
	logfile = fopen("ftp.log", "a");
}

void ftpLog(char *s) {
	char tmp[1024];
	char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	struct tm *times;
	time_t time_value;
	int write_size;
	printf("logging %s",s);
	if(logfile) {
		time(&time_value);
		times = localtime(&time_value);
		write_size = sprintf(tmp, "%s %.2d %.2d:%.2d:%.2d %.1007s",
			   	months[times->tm_mon], times->tm_mday, times->tm_hour, times->tm_min, times->tm_sec, s) ;
		printf("%s",tmp);
		fwrite(tmp, sizeof(char), write_size, logfile);
	}
}

void ftpCloseLog() {
	fclose(logfile);
}



void printConnections() {
	int i;
	for(i=0; i < FD_SETSIZE; i++) {
		if(FD_ISSET(i, &savedset_read) || FD_ISSET(i, &savedset_write)) {
			if(connections[i] != NULL) {
				printConnection(connections[i]);
			}
		}
	}
}

void printConnection(connection *c) {
	if(c == NULL)
		return;
	int i = c->sock;
	printf("[%d (%p)] rset:%d wset:%d type:%d parent:%p child:%p tx:%p\n", 
		i, c, (int)FD_ISSET(i, &savedset_read), (int)FD_ISSET(i, &savedset_write), c->ctype, c->parent, c->child, c->tx);
}

void setTXDirection(connection *c, tx_direction d) {
	c->direction = d;
	switch(c->direction) {
		case GET: {
			FD_SET(c->sock, &savedset_read);
			break;
		}
		case PUT: {
			FD_SET(c->sock, &savedset_write);
			break;
		}
		default: break;
	}
}

/**
 *	I dont care what you do but get rid of you from select
 */
void removeFromFdset(int socket) {
	FD_CLR(socket, &savedset_read);
	FD_CLR(socket, &savedset_write);
}

/**
 *  Returns a TCP listening socket
 */
connection *initMasterListen(struct sockaddr_in *my_addr, int port) {
#ifndef __PSP__
#ifndef __DS__
	int on = 1;
#endif
#endif
	int ret;
	connection *master_sock = malloc(sizeof(connection));
	if(master_sock == NULL) {
		printf("failed malloc\n");
		return NULL;
	}
	master_sock->ctype = MASTER_LISTEN;
	master_sock->path = NULL;
	master_sock->child = NULL;
	master_sock->tx = NULL;
	master_sock->parent = NULL;
	master_sock->root = NULL;
	master_sock->user = NULL;
	master_sock->buffer = NULL;


	/* create socket */
	master_sock->sock = socket(AF_INET, SOCK_STREAM, 0);
	if(master_sock->sock < 0) {
		printf("socket() failure. %d\n", master_sock->sock);
		free(master_sock);
		return NULL;
	}
#ifndef __PSP__
#ifndef __DS__
	if (setsockopt(master_sock->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		printf("setting reuse socket failed");
	}
#endif
#endif
	/* set sockaddr parameter and bind() */
	memset(my_addr, 0, sizeof(struct sockaddr_in));
	my_addr->sin_family = AF_INET;  /* this is IP socket */
	my_addr->sin_addr.s_addr = INADDR_ANY; /* use my IP address */
	my_addr->sin_port = htons(port);  /* use my port */
	if(bind(master_sock->sock, (struct sockaddr*)my_addr, sizeof(struct sockaddr_in)) < 0) {
		printf("bind() failure. %s\n",strerror(errno));
		shutdown(master_sock->sock, 2);
#ifndef __DS__
		close(master_sock->sock);
#endif
		free(master_sock);
		return NULL;
	}

	/* listen() */
	if( (ret = listen(master_sock->sock, MAX_CLIENTS)) < 0) {
		printf("listen() failure %s.\n",strerror(errno));
		shutdown(master_sock->sock, 2);
#ifndef __DS__
		close(master_sock->sock);
#endif
		free(master_sock);
		return NULL;
	}

	connections[master_sock->sock] = master_sock;
	printf("New Connection %p\n",master_sock);

	return master_sock;
}

void closeConnection(connection *c) {
	if(c->child)
		closeConnection(c->child);
	if(c->tx) {
		closeDataConnection(c->tx);
	}
	if(c->parent) {
		((connection *)(c->parent))->child = NULL;
	}
	if(c->sock > -1) {
		removeFromFdset(c->sock);
		connections[c->sock] = NULL;
		shutdown(c->sock, 2);
#ifndef __DS__
		close(c->sock);
#endif
	}
	printf("Connection closed %p\n",c);
	free(c->user);
	free(c->root);
	free(c->path);
	free(c->buffer);
	free(c);
}

/**
 * Close sockets, Cleanup storage and free alloc'd data
 */
int closeDataConnection(transfer *t) {
	if(t->file != NULL) {
		fclose(t->file);
	}
	free(t);
	printf("Closing down a Data Connection\n");
	return 1;
}

connection *openConnection(int socket, sock_type type, connection *parent) {
	connection *con;
	if( (con = malloc(sizeof(connection))) == NULL) {
		printf("failed malloc\n");
		return NULL;
	}
	con->path = copyString("/");
	con->root = copyString("/");

	printf("New Connection %p\n",con);

	con->sock = socket;
	con->ctype = type;
	con->f_type = ASCII;
	con->parent = NULL;
	if(parent != NULL) {
		con->ip[0] = parent->ip[0];
		con->ip[1] = parent->ip[1];
		con->ip[2] = parent->ip[2];
		con->ip[3] = parent->ip[3];
		if(parent->ctype != MASTER_LISTEN) {
			con->parent = parent;
			parent->child = con;
		}
		else {
			con->parent = NULL;
		}
	}
	else {
	}
	con->port = 0;
	con->buffer_pos = 0;
	con->buffer_size = 1024;
	con->child = NULL;
	con->tx = NULL;
	con->login_status = 0;
	con->renamepath = NULL;
	con->user = NULL;
	if( (con->buffer = calloc(1025, sizeof(char))) == NULL) {
		printf("failed malloc\n");
		free(con->path);
		free(con);
		return NULL;
	}

	if(socket > -1) {
		connections[con->sock] = con;
	}

	return con;
}

/**
 * Open listening socked or connect to a client
 */
int openDataConnection(connection *c, transfer *t) {
	char tmp[16];
	int port;
	int ret=-1, i=0;
	memset(&t->my_addr, 0, sizeof(struct sockaddr_in));
	t->my_addr.sin_family = AF_INET;  /* this is IP socket */
	if( (t->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { /* create the socket */
		printf("socket() failure. %s\n",strerror(errno));
		closeConnection(c);
		return -1;
	}
	if(t->mode == PASV) {
		t->my_addr.sin_addr.s_addr = INADDR_ANY; /* use my IP address */
		ret = 0;
		while(ret != 1) {
			port = (rand()%(65534-1024)) + 1024;
			t->my_addr.sin_port = htons(port);
			c->port = port;
			i++;
			if(i>3) {
				ret=0;
				shutdown(t->sock, 2);
#ifndef __DS__
				close(t->sock);
#endif
				break;
			}
			ret = openDataConnectionPASV(c, t);
		}
		return ret;
	}
	else {
		sprintf(tmp, "%d.%d.%d.%d",c->ip[0],c->ip[1],c->ip[2],c->ip[3]);
		printf("PORT connecting to %s:%d\n",tmp,c->port);
		t->my_addr.sin_addr.s_addr = inet_addr(tmp);
		t->my_addr.sin_port = htons(c->port);
		memset(t->my_addr.sin_zero, '\0', sizeof(t->my_addr.sin_zero));
		return openDataConnectionPORT(c, t);
	}
}

/**
 *  Create and listen on a socket
 */
int openDataConnectionPASV(connection *c, transfer *t) {
	int ret;
	if(bind(t->sock, (struct sockaddr*)&t->my_addr, sizeof(struct sockaddr_in)) < 0) {
		printf("bind() failure. %s\n",strerror(errno));
		return 0;
	}
	if( (ret = listen(t->sock, 1)) < 0) {
		printf("listen() failure. %s\n",strerror(errno));
		return 0;
	}
	FD_SET(t->sock, &savedset_read);
	return 1;
}

/**
 *  Create and connect on a socket
 */
int openDataConnectionPORT(connection *c, transfer *t) {
	int ret;
	if( (ret = connect(t->sock, (struct sockaddr *) &t->my_addr, sizeof(t->my_addr))) < 0) {
		printf("connect() failure. %s\n",strerror(errno));
		return 0;
	}
	return 1;
}

connection *createClient(char *host, int port) {
#ifdef __DS__
	struct hostent *myhost;
#elif __PSP__
	struct hostent *myhost;
#else
	struct addrinfo *result;
#endif
	int ret;
	connection *c = openConnection(-1, USER_CONTROL, NULL);
	memset(&c->my_addr, 0, sizeof(struct sockaddr_in));
	c->my_addr.sin_family = AF_INET;  /* this is IP socket */
	if( (c->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { /* create the socket */
		printf("socket() failure. %s\n",strerror(errno));
		closeConnection(c);
		return NULL;
	}
#ifdef __DS__
	myhost = gethostbyname( host );
	if(!myhost) {
		closeConnection(c);
		return NULL;
	}
	c->my_addr.sin_addr.s_addr = *( (unsigned long *)(myhost->h_addr_list[0]) );
#elif __PSP__
	myhost = gethostbyname( host );
	if(!myhost) {
		closeConnection(c);
		return NULL;
	}
	c->my_addr.sin_addr.s_addr = *( (unsigned long *)(myhost->h_addr_list[0]) );
#else
	if(getaddrinfo(host, NULL, NULL, &result) == -1) {
		printf("addrinfo error %s",strerror(errno));
	}
	c->my_addr.sin_addr.s_addr = ((struct sockaddr_in *)(result->ai_addr))->sin_addr.s_addr;
	freeaddrinfo(result);
#endif
	c->my_addr.sin_port = htons(port);
	c->port = port;
	memset(c->my_addr.sin_zero, '\0', sizeof(c->my_addr.sin_zero));
	if( (ret = connect(c->sock, (struct sockaddr *) &c->my_addr, sizeof(c->my_addr))) < 0) {
		printf("connect() failure. %s\n",strerror(errno));
		closeConnection(c);
		return NULL;
	}
	return c;
}

/**
 * create new transfer structure
 * char *s can be NULL in PASV mode
 */
transfer *createTransfer(connection *c, sock_mode mode, char *s) {
	int success = 0;
	int l1,l2,l3,l4,p1,p2;
	c->tx = malloc(sizeof(transfer));
	transfer *t = c->tx;
	if(t == NULL) {
		printf("failed malloc\n");
		closeConnection(c);
		return NULL;
	}
	t->file = NULL;
	t->mode = mode;
	t->tx_progress = 0;
	t->size = 0;
	t->connected = 0;
	if(mode == PORT) {
		sscanf(s, "%d,%d,%d,%d,%d,%d",&l1,&l2,&l3,&l4,&p1,&p2);
		c->port = 0;
		c->port |= (p1 << 8 | p2);
		t->my_addr.sin_addr.s_addr = 0;
		c->ip[0] = l1;
		c->ip[1] = l2;
		c->ip[2] = l3;
		c->ip[3] = l4;
	}
	success = openDataConnection(c,t);

	if (!success) {
		printf("not successful in creating transfer\n");
		shutdown(t->sock, 2);
#ifndef __DS__
		close(t->sock);
#endif
		t->sock = -1;
		c->sock = -1;
	}
	else {
		c->sock = t->sock;
		connections[c->sock] = c;
		if(mode == PORT) 
			t->connected = 1;
	}
	/* PASV's should add socket to the select and wait to do an accept() */
	return t;
}

int transferData(connection *c) {
	int size = 0, tx_size = 0;
	unsigned char tx_buffer[MAX_TX_BUFFER];
	if(c->direction == PUT) {
		if(c->tx->file == NULL || c->tx->tx_progress == c->tx->size) {
			if(((connection *)(c->parent))->user)
				blockingSendString( ((connection *)(c->parent)), "226 Closing data connection.\r\n");
			closeConnection(c);
			return 0;
		}
		if(c->f_type == ASCII) 
			size = fread(tx_buffer, sizeof(char), MAX_TX, c->tx->file);
		else
			size = fread(tx_buffer, sizeof(unsigned char), MAX_TX, c->tx->file);
#ifdef __DS__
		if(size == -1) {
			printf("Read error %s\n",strerror(errno));
			return -1;
		}
#endif
		tx_size = send(c->sock, tx_buffer, size, 0);
		if(tx_size == -1)
			tx_size = 0;
#ifdef DEBUG
		tx_buffer[tx_size] = '\0';
		printf("%s",tx_buffer);
#endif
		if(size != tx_size) {
			fseek(c->tx->file, tx_size - size, SEEK_CUR);
		}
		c->tx->tx_progress += tx_size;
		printf("read %d sent %d (%d/%d bytes)\n", size, tx_size, c->tx->tx_progress, c->tx->size);
		return tx_size;
	}
	else{
		tx_size = recv(c->sock, tx_buffer, MAX_TX, 0);
		printf("recieved %d (%d bytes)\n", tx_size, c->tx->tx_progress);
		if(tx_size == 0) {
			if(((connection *)(c->parent))->user)
				blockingSendString( ((connection *)(c->parent)), "226 Closing data connection.\r\n");
			closeConnection(c);
			return 0;
		}
		if(c->f_type == ASCII)
			size = fwrite(tx_buffer, sizeof(char),tx_size,c->tx->file);
		else
			size = fwrite(tx_buffer, sizeof(unsigned char),tx_size,c->tx->file);

		c->tx->tx_progress += size;
		return size;
	}
}

int handleSocket(connection *c) {
	int size;
	tokens *toks;
	char *carragereturn;
	int new_sock;
	int parser_status;
	struct sockaddr_in client;
#ifdef __PSP__
	socklen_t c_size = sizeof(client);
#elif __DS__
	int c_size = sizeof(client);
#else
	size_t c_size = sizeof(client);
#endif
	if(c == NULL) {
		return -1;
	}
#ifdef DEBUG
	printf("handle socket called\n");
	printConnection(c);
#endif
	switch(c->ctype) {
		/*
		 * accept DONE
		 * create connection structure DONE
		 * add to fdset DONE
		 */
		case MASTER_LISTEN: {
			new_sock = accept(c->sock, (struct sockaddr *) &client, &c_size);
			if(new_sock < 0) {
				printf( "accept error %s\n", strerror(errno));
				return 0;
			}
#ifdef __DS__
			ioctl(new_sock, FIONBIO, &block);
#endif
			openConnection(new_sock, USER_CONTROL, c);
			printf("new user connection %p\n",connections[new_sock]);
			FD_SET(new_sock, &savedset_read);
#ifdef __DS__
			blockingSendString(connections[new_sock], "220 GambleFTPD NDS\r\n");
#elif __PSP__
			blockingSendString(connections[new_sock], "220 GambleFTPD PSP\r\n");
#else
			blockingSendString(connections[new_sock], "220 GambleFTPD PC\r\n");
#endif
			break;
		}
		/*
	   	 * read DONE
   		 * tokinize DONE
		 * parse parseExec
		 * do something parseExec
		 *  create transfer structure?
		 *  open new data connection/pasv_listen?
		 *  USER,PASS etc.
		 */
		case USER_CONTROL: {
			size = recv(c->sock, &c->buffer[c->buffer_pos], 1024, 0);
			if(size == 0) {
				closeConnection(c);
				break;
			}
			if(size + c->buffer_pos + 1023 > c->buffer_size) {
				if( (c->buffer = realloc(c->buffer, c->buffer_size * 2)) == NULL) {
					printf("failed malloc\n");
					closeConnection(c);
					return -1;
				}
			}
			if( (carragereturn = strstr(c->buffer, "\r\n")) != NULL ) {
				*carragereturn = '\0';
#ifdef DEBUG
				printf("parsing complete command %s\n",c->buffer);
#endif
				toks = getTokens(c->buffer);
#ifdef DEBUG
				printf("parsed %d strings\n",toks->size);
#endif
			 	parser_status = parseExec(toks, c);
#ifdef DEBUG
				printf("executed '%s'\n",c->buffer);
#endif
				if(!parser_status) {
					printf("Umm parser broke on tokens\n");
					printTokens(toks);
				}
				deleteTokens(toks);
				/* check to see if the client quit */
				if(parser_status != 2) {
					c->buffer_pos = 0;
					free(c->buffer);
					if( (c->buffer = calloc(1025, sizeof(char))) == NULL) {
						printf("failed malloc\n");
						closeConnection(c);
						return -1;
					}
				}
			}
			break;
		}
		case PASV_LISTEN: {
			/*
			 * accept
			 * create transfer structure
			 * add new sock to transfer
			 * close pasv_listen socket
			 */
			new_sock = accept(c->sock, (struct sockaddr *) &client, &c_size);
			if(new_sock < 0) {
				printf( "accept error %s\n", strerror(errno));
				return 0;
			}
#ifdef __DS__
			ioctl(new_sock, FIONBIO, &block);
#endif
			/* now connected change from PASV_SOCK to a TRANSFER */
			size = c->tx->sock;
			c->tx->sock = new_sock;
			c->sock = new_sock;

			FD_CLR(size, &savedset_read);
			shutdown(size, 2);
#ifndef __DS__
			close(size);
#endif
			connections[size] = NULL;
			connections[new_sock] = c;
			c->tx->connected = 1;
			c->ctype = TRANSFER;
			printf("new data connection %p\n",connections[new_sock]);
			break;
		}
		case TRANSFER: {
			/*
			 * fread/fwrite(file) based on direction.
			 *  send data over socket?
			 *  recieve data over socket?
			 *  update progress with size transfered
			 *  close transfer socket on completion
			 */
			if(c->tx->connected == 0)
				break;
			transferData(c);
			return 1;
		}
		default: return 0;
	}
	return 1;
}
