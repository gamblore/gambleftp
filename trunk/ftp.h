#ifndef __FTP_H__
#define __FTP_H__

#include <sys/socket.h>
#include <stdio.h>

#ifdef __DS__
#include <netinet/in.h>
#else
#include <sys/select.h>
#include <arpa/inet.h>
#endif

extern fd_set savedset_read;
extern fd_set savedset_write;
extern int block;

#define MAX_TX 8192
#define MAX_TX_BUFFER 8193
#define MAX_CLIENTS 5

#ifdef __DS__
#define MAX_SOCKETS 32
#else
#define MAX_SOCKETS 1024
#endif
typedef enum {ASCII = 0, BINARY} tx_type;
typedef enum {GET = 0, PUT} tx_direction;
typedef enum {PASV = 0, PORT} sock_mode;
typedef enum {MASTER_LISTEN = 0, USER_CONTROL, PASV_LISTEN, TRANSFER} sock_type;

typedef struct {
	struct sockaddr_in my_addr;
	int sock;
	int connected;
	sock_mode mode;
	FILE *file;
	int size;
	unsigned int tx_progress;
}transfer;

typedef struct {
	struct sockaddr_in my_addr;
	int sock;
	transfer *tx;
	void *parent;
	void *child;
	char *path;
	char *root;
	char *renamepath;
	char *user;
	char *buffer;
	int buffer_pos;
	int buffer_size;
	int login_status;
	int ip[4];
	int port;
	sock_type ctype;
	tx_direction direction;
	tx_type f_type;
}connection;

extern connection *connections[MAX_SOCKETS];

void ftpOpenLog();
void ftpLog(char *s);
void ftpCloseLog();

void printConnections();
void printConnection(connection *c);

void setTXDirection(connection *c, tx_direction d);
connection *initMasterListen(struct sockaddr_in *my_addr, int port);

connection *openConnection(int socket, sock_type type, connection *parrent);
void closeConnection(connection *c);

connection *createClient(char *host, int port);
transfer *createTransfer(connection *c, sock_mode mode, char *s);

int openDataConnection(connection *c, transfer *t);
int closeDataConnection(transfer *t);



int transferData(connection *c);
int handleSocket(connection *c);

#endif
