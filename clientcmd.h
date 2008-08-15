#ifndef __CLIENTCMD_H__
#define __CLIENTCMD_H__
#include "ftp.h"
#include "filelist.h"

typedef struct{
	char *host[4];
	void *next;
}saved_host;

void readToNewLine(FILE *fp);

void deleteSavedHost(saved_host *h);

void deleteSavedHosts(saved_host *hosts);

saved_host *getSavedHosts();
saved_host *createHost(char *user,char *pass, char *host, char *port);

void addHost(char *user, char *pass, char *host, char *port);

char *blockingRecvList(connection *c);

connection *clientConnect();

int clientLogin(connection *c, char *user, char *pass);

void clientQuit(connection *c);

int clientPasv(connection *c);

void clientRetr(connection *c, char *file, int remotesize);
void clientStor(connection *c, char *file, int remotesize);

int clientRemoteDelete(connection *c, char *name, int isDir);
int clientLocalDelete(connection *c, char *name, int isDir);

int clientRemoteMakeDirectory(connection *c, char *name);
int clientLocalMakeDirectory(connection *c, char *name);

int clientRemoteRename(connection *c, char *from, char *to);
int clientLocalRename(connection *c, char *from, char *to);

int clientChangeRemoteDir(connection *c, char *path);
int clientChangeLocalDir(connection *c, char *path);

filelist *getRemoteList(connection *c);
filelist *getLocalList(connection *c);

#endif
