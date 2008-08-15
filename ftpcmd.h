#ifndef __FTPCMD_H__
#define __FTPCMD_H__

#include "ftp.h"

#ifndef __DS__
#include <dirent.h>
#else
#include <sys/dir.h>
#include <fat.h>
#define DIR DIR_ITER
#define opendir diropen 
#define closedir dirclose
#endif

int blockingSendString(connection *c, char *s);
int blockingRecvString(connection *c, char *s);

char *copyString(char *s);

int isFile(connection *c, char *s);

int getSize(connection *c, char *s);

int getModtime(connection *c, char *file, char *buf);

int ftpSend(connection *c, char *file);

int ftpRecv(connection *c, char *file, int append);

int ftpRename(connection *c, char *from, char *to);

int ftpList(connection *c, connection *c_tx, char *path, int full);

int deleteFolder(connection *c, char *path);

int deleteFile(connection *c, char *path);

int createFolder(connection *c, char *path);

int changeDirectory(connection *c, char *path);

int changeDirectoryUp(connection *c, char *dest);

#endif

