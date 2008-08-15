#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>

#include "clientcmd.h"
#include "ftpcmd.h"
#include "parser.h"
#include "ftp.h"
#include "filelist.h"
#include "platform.h"

#ifdef PSP_ADHOC
#include "psp_adhoc.h"
#endif

/**
 * Parse and use saved_host functionality
 */
void readToNewLine(FILE *s) {
	int c = 0;
	while( (c = fgetc(s)) != EOF && c != '\n');
}

void deleteSavedHost(saved_host *h) {
	int i;
	if(h) {
		for(i = 0; i < 4; i++) {
			free(h->host[i]);
		}
	}
	free(h);
}

void deleteSavedHosts(saved_host *hosts) {
	if(hosts == NULL)
		return;
	if(hosts->next != NULL)
		deleteSavedHosts(hosts->next);

	deleteSavedHost(hosts);
}

saved_host *createHost(char *user,char *pass, char *host, char *port) {
	saved_host *ret;
	if( (ret = malloc(sizeof(saved_host))) == NULL) {
		printf("failed malloc\n");
		return NULL;
	}
	ret->host[0] = copyString(user);
	ret->host[1] = copyString(pass);
	ret->host[2] = copyString(host);
	ret->host[3] = copyString(port);
	ret->next = NULL;
	return ret;
}

saved_host *getSavedHosts() {
	char tmp[1024];
	int read = 0;
	int readpos = 0;
	saved_host *ret = NULL;
	saved_host *current = NULL;
	char *us;
	char *pa;
	char *ho;
	char *po;

	FILE *hosts = fopen("hosts.cfg","r");
	if(hosts == NULL) {
		return NULL;
	}
	current = ret;
	while( (read = fread(tmp+readpos, sizeof(char), 1, hosts))) {
		if(readpos > 1023)
			printf("your breaking things. Fuck off.\n");
		if(tmp[readpos] == '\n') {
			printf("GetHosts Got a line %d\n",readpos);
			tmp[readpos] = '\0';
			us = tmp;
			
			pa = strstr(us,":");
			*pa = '\0';
			pa++;
			
			ho = strstr(pa,"@");
			*ho = '\0';
			ho++;

			po = strstr(ho,":");
			*po = '\0';
			po++;
			printf("GetHosts user: %s pass: %s host: %s port: %s\n",us,pa,ho,po);
			if(current == NULL) {
				current = createHost(us,pa,ho,po);
				ret = current;
			}
			else{
				current->next = createHost(us,pa,ho,po);
				current = current->next;
			}
			readpos = 0;
		}
		else
			readpos++;
	}
	fclose(hosts);
	printf("GetHosts done\n");
	return ret;
}

void addHost(char *user, char *pass, char *host, char *port) {
	int found = 0;
	int state = 0;
	char tmp[128];
	char *values[4];
	char next[4];
	int read = 0;
	int c = 0;
	FILE *hosts;
	values[0] = user;
	next[0] = ':';
	values[1] = pass;
	next[1] = '@';
	values[2] = host;
	next[2] = ':';
	values[3] = port;
	next[3] = '\n';

	hosts = fopen("hosts.cfg","r");
	if(hosts == NULL) {
		printf("Cannot open hosts.cfg\n");
		hosts = fopen("hosts.cfg","a");
		fclose(hosts);
		hosts = fopen("hosts.cfg","r");
		if(hosts == NULL)
			return;
	}
	while( (c = fgetc(hosts)) != EOF) {
		tmp[read++] = c;
		if(c == next[state]) {
			tmp[read-1] = '\0';
			printf("Host state: %d %s vs %s\n",state,values[state], tmp);
			if(strcmp(values[state], tmp) != 0) {
				read = 0;
				state = 0;
				readToNewLine(hosts);
				continue;
			}
			if(state == 3) {
				read = 0;
				state = 0;
				found = 1;
				break;
			}
			else {
				read = 0;
				state++;
			}
		}
	}
	fclose(hosts);
	if(!found) {
		hosts = fopen("hosts.cfg","a");
		fprintf(hosts,"%s:%s@%s:%s\n",user,pass,host,port);
		fclose(hosts);
	}
}

/** 
 * Check if the path is a directory
 */
int pathIsDir(char *path) {
	struct stat stats;
	if(stat(path, &stats) != 0) {
		return 0;
	}
	if(S_ISDIR(stats.st_mode)) {
		return 1;
	}
	else {
		return 0;
	}
}

/**
 * Receive a LIST command from a server force blocking tell you receive it all.
 */
char *blockingRecvList(connection *c) {
	int read = 0,i;
	int buffer_size = 2048;
	char *buffer;
	if( (buffer = malloc(2048)) == NULL) {
		printf("failed malloc\n");
		closeConnection(c);
		return NULL;
	}
	buffer[0] = '\0';
	fd_set readset;
	fd_set saved;
	FD_ZERO(&readset);
	FD_SET(c->sock, &readset);
	saved = readset;
	while(1) {
		readset = saved;
		i = select(c->sock+2,&readset,NULL,NULL,0);
		if(i == -1)
			break;
		if(FD_ISSET(c->sock,&readset)) {
			if(read + 1023 > buffer_size) {
				if( (buffer = realloc(buffer, buffer_size * 2)) == NULL) {
					printf("failed realloc\n");
					closeConnection(c);
					return NULL;
				}
				buffer_size *= 2;
			}
			i = recv(c->sock, buffer+read, 1024, 0);
			read += i;
			buffer[read] = '\0';
			if(i == 0) {
				closeConnection(c);
				return buffer;
			}
		}
		else
			continue;
	}
	return NULL;
}

/**
 * Login to the server
 */
int clientLogin(connection *c, char *user, char *pass) {
	char *tmp;
	if( (tmp = malloc(1024)) == NULL) {
		printf("malloc failed.\n");
		return 0;
	}
	int ret_code = 0;
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 220) {
		free(tmp);
		return 0;
	}
	sprintf(tmp,"USER %s\r\n",user);
	printf(tmp);
	blockingSendString(c,tmp);
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 331) {
		free(tmp);
		return 0;
	}
	sprintf(tmp,"PASS %s\r\n",pass);
	printf(tmp);
	blockingSendString(c,tmp);
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 230) {
		free(tmp);
		return 0;
	}
	printf("logged in\n");
	free(tmp);
	return 1;
}

/**
 * connect to the server
 * check if the users will use a saved host
 */
connection *clientConnect() {
	char *host, *port, *user, *pass;
	int connected;
	connection *c = NULL;
	/* list hosts */
	saved_host *saved = platformListHosts();

	if(saved) {
		printf("using saved %s@%s\n",saved->host[0],saved->host[2]);
		c = createClient(saved->host[2], atoi(saved->host[3]));
		if(c == NULL) {
			deleteSavedHost(saved);
			return NULL;
		}
		if(!clientLogin(c, saved->host[0],saved->host[1])) {
			printf("Unable to logon\n");
			deleteSavedHost(saved);
			return NULL;
		}
		deleteSavedHost(saved);
	}
	else {
		host = copyString(getKeyboard(64,4,"hostname: ", ""));
		port = copyString(getKeyboard(5,4,"port: ", "21"));

		printf("connecting\n");
		c = createClient(host, atoi(port));
		if(c == NULL) {
			return NULL;
		}
		user = copyString(getKeyboard(64,4,"username: ", "anonymous"));
		pass = copyString(getKeyboard(64,4,"password: ", NULL));
		if( (connected = clientLogin(c, user,pass)) ) {
			addHost(user,pass,host,port);
		}
		free(user);
		free(pass);
		free(host);
		free(port);
		if(!connected) {
			closeConnection(c);
			return NULL;
		}
	}
	return c;
}

/**
 * Quit from the FTP Server
 */
void clientQuit(connection *c) {
	char tmp[128];
	blockingSendString(c,"QUIT\r\n");
	if(blockingRecvString(c,tmp) == 221)
		printf("quit ok\n");
	else 
		printf("quit not ok\n");
	closeConnection(c);
}

/**
 * Open up a passive connection to the server. Client only supports this method.
 */
int clientPasv(connection *c) {
	char tmp[1024];
	char *tmpparen;
	int ret_code;
	tokens *tmptokens;
	blockingSendString(c, "PASV\r\n");
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 227) {
		printf("Server not opening passive port (%d)\n", ret_code);
		return 0;
	}
	tmptokens = getTokens(tmp);
	tmpparen = strchr(tmptokens->token[4]+1, ')');
	tmpparen[0] = '\0';

	c->child = openConnection(-1, TRANSFER, c);
	((connection *)(c->child))->tx = createTransfer(c->child, PORT, &tmptokens->token[4][1]);
	deleteTokens(tmptokens);

	return 1;
}

/**
 * Handle a transfer tell it is finished. Calls back to update the screen with new progress
 */
void clientTransfer(connection *c) {
	char tmp[15];
	int done = 0;
	fd_set saved_read;
	fd_set saved_write;
	fd_set read;
	fd_set write;
	FD_ZERO(&saved_read);
	FD_ZERO(&saved_write);
	read = saved_read;
	write = saved_write;
	time_t current = time(NULL);
	time_t oldtime = time(NULL) - 1;
	int timediff = 0;
	int oldprogress = 0;
	int progress_change;
	struct timeval timer = {0, 500};

	if(c == NULL)
		return;

	if(c->direction == PUT)
		FD_SET(c->sock, &saved_write);
	else
		FD_SET(c->sock, &saved_read);

	oldprogress = c->tx->tx_progress;
	while(!done) {
		read = saved_read;
		write = saved_write;
		if( (select(c->sock+2,&read, &write, NULL, &timer)) == -1) {
			printf("transfer select error\n");
			closeConnection(c);
			return;
		}
		if(FD_ISSET(c->sock, &read) || FD_ISSET(c->sock, &write)) {
			done = !transferData(c); /* returns 0 on tx finish */
			if(!done) {
				if( (current = time(NULL)) > oldtime) {
					progress_change = c->tx->tx_progress - oldprogress;
					timediff = current - oldtime;
					oldprogress = c->tx->tx_progress;
					oldtime = current;
					sprintf(tmp, "%d KB/s", progress_change / 1024 / timediff);
					platformProgress(c->tx->tx_progress*100/c->tx->size, tmp);
				}
				printf("%d of %d transfered\n",c->tx->tx_progress, c->tx->size);
			}
		}
	}
}

/**
 * Various request to send to the server.
 */
void clientRetr(connection *c, char *file, int remotesize) {
	int ret_code;
	char tmp[1024] = "";
	char cmdbuf[1024];
	struct stat stats;
	int go_ahead = 1;
	
	if(strlen(c->root) != 1)
		strcat(tmp,c->root);
	strcat(tmp,"/");
	strcat(tmp,file);
	printf("local file will be %s\n",tmp);

	if(stat(tmp,&stats) == 0) {
		printf("file %s exists\n",tmp);
		if(remotesize == 0)
			go_ahead = 1;
		else if(remotesize > (int)stats.st_size)
			go_ahead = platformOverwrite(tmp,1);
		else
			go_ahead = platformOverwrite(tmp,0);
	}
	if(go_ahead == 3)
		return;

	printf("Opening PASV");
	clientPasv(c);
	((connection *)(c->child))->tx->size = remotesize;
	((connection *)(c->child))->direction = GET;
	((connection *)(c->child))->f_type = BINARY;

	if(go_ahead) {
		if(go_ahead == 1) {
			printf("overwriting\n");
			((connection *)(c->child))->tx->file = fopen(tmp,"wb");
		}
		else if(go_ahead == 2) {
			printf("appending\n");
			((connection *)(c->child))->tx->file = fopen(tmp,"ab");
			sprintf(cmdbuf, "REST %d\r\n",(int)stats.st_size);
			printf("tx: %s\n",cmdbuf);
			blockingSendString(c, cmdbuf);
			((connection *)(c->child))->tx->tx_progress = (int)stats.st_size;
			blockingRecvString(c, cmdbuf); /* i dont care what it is... just get it out of the buffer */
		}
	}

	/* setup the connection */
	sprintf(tmp, "RETR %s\r\n", file);
	printf("sent retr request for %s\n",file);
	blockingSendString(c, tmp);
	ret_code = blockingRecvString(c, tmp);
	if(ret_code == 125){
		clientTransfer(c->child);
	}
	/* read off transfer finished 226 */
	ret_code = blockingRecvString(c,tmp);
	if(ret_code != 226)
		printf("transfer failed\n");
}

void clientStor(connection *c, char *file, int remotesize) {
	char cmdbuf[1024];
	struct stat stats;
	int go_ahead = 3;
	int ret_code;
	char tmp[1024] = "";

	if(strlen(c->root) != 1)
		strcat(tmp,c->root);
	strcat(tmp,"/");
	strcat(tmp,file);

	if(stat(tmp,&stats) == 0) {
		if(remotesize == 0)
			go_ahead = 1;
		else if(remotesize >= (int)stats.st_size)
			go_ahead = platformOverwrite(tmp,0);
		else
			go_ahead = platformOverwrite(tmp,1);
	}
	if(go_ahead == 3)
		return;

	printf("Opening PASV");
	clientPasv(c);
	((connection *)(c->child))->tx->size = (int)stats.st_size;
	((connection *)(c->child))->direction = PUT;
	((connection *)(c->child))->f_type = BINARY;
	if(go_ahead) {
		((connection *)(c->child))->tx->file = fopen(tmp,"rb");
		if(go_ahead == 1) {
			printf("overwriting\n");
		}
		else if(go_ahead == 2) {
			printf("appending\n");
			fseek(((connection *)(c->child))->tx->file, remotesize, SEEK_CUR);
			sprintf(cmdbuf, "REST %d\r\n",remotesize);
			printf("tx: %s\n",cmdbuf);
			blockingSendString(c, cmdbuf);
			((connection *)(c->child))->tx->tx_progress = remotesize;
			blockingRecvString(c, cmdbuf); /* i dont care what it is... just get it out of the buffer */
		}
	}

	sprintf(tmp, "STOR %s\r\n", file);
	printf("sent stor request for %s\n",file);
	blockingSendString(c, tmp);
	ret_code = blockingRecvString(c, tmp);
	if(ret_code == 150){
		clientTransfer(c->child);
	}
	/* read off transfer finished 226 */
	ret_code = blockingRecvString(c,tmp);
	if(ret_code != 226)
		printf("transfer failed\n");
}

int clientRemoteDelete(connection *c, char *name, int isDir) {
	int ret_code;
	char tmp[1024];
	if(isDir) {
		sprintf(tmp, "RMD %s\r\n", name);
	}
	else {
		sprintf(tmp, "DELE %s\r\n", name);
	}
	printf("sent Delete request for %s\n",name);
	blockingSendString(c, tmp);
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 250) {
		return 0;
	}
	return 1;
}

int clientLocalDelete(connection *c, char *name, int isDir) {
	char tmp[1024] = "";

	if(strlen(c->root) != 1)
		strcat(tmp,c->root);
	strcat(tmp,"/");
	strcat(tmp,name);

	printf("Deleting %s\n",tmp);
	if(isDir) {
#ifdef __DS__
		return (unlink(tmp) == 0);
#else
		return (rmdir(tmp) == 0);
#endif
	}
	else {
		return (unlink(tmp) == 0);
	}
}

int clientLocalMakeDirectory(connection *c, char *name) {
	char tmp[1024] = "";

	if(strlen(c->root) != 1)
		strcat(tmp,c->root);
	strcat(tmp,"/");
	strcat(tmp,name);

	printf("Make directory %s\n",tmp);
	return (mkdir(tmp, S_IRWXU) == 0);
}

int clientRemoteMakeDirectory(connection *c, char *name) {
	int ret_code;
	char tmp[1024];
	sprintf(tmp, "MKD %s\r\n", name);
	printf("sent make directory request for %s\n",name);
	blockingSendString(c, tmp);
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 257) {
		return 0;
	}
	return 1;
}

int clientLocalRename(connection *c, char *from, char *to) {
	char tmp[1024] = "";
	char tmpto[1024] = "";

	if(strlen(c->root) != 1)
		strcat(tmp,c->root);
	strcat(tmp,"/");
	strcat(tmp,from);

	if(strlen(c->root) != 1)
		strcat(tmpto,c->root);
	strcat(tmpto,"/");
	strcat(tmpto,to);

	printf("Rename %s to %s\n",tmp,tmpto);
	return rename(tmp, tmpto);
}

int clientRemoteRename(connection *c, char *from, char *to) {
	int ret_code;
	char tmp[1024];
	sprintf(tmp, "RNFR %s\r\n", from);
	printf("sent rename from request for %s\n",from);
	blockingSendString(c, tmp);
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 350) {
		return 0;
	}
	sprintf(tmp, "RNTO %s\r\n", to);
	printf("sent rename to request for %s\n",to);
	blockingSendString(c, tmp);
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 250) {
		return 0;
	}
	return 1;
}


int clientChangeRemoteDir(connection *c, char *path) {
	char tmp[128];
	char newpath[1024];
	char tosend[1029];
	int ret_code = 0;
	int path_len = strlen(path);
	if(path_len > 1 && path[path_len-2] == '.' && path[path_len-1] == '.') {
		sprintf(tosend, "CDUP\r\n");
		blockingSendString(c, tosend);
		ret_code = blockingRecvString(c, tmp);
		changeDirectoryUp(c, c->path);
		return 1;
	}
	if(path[0] == '/') {
		memcpy(newpath,path,strlen(path)+1);
	}
	else {
		path_len = strlen(c->path);
		memcpy(newpath, c->path, path_len);
		if(path_len == 1)
			path_len--;
		newpath[path_len] = '/';
		memcpy(newpath+path_len+1,path,strlen(path)+1); /*get null terminator*/
	}
	sprintf(tosend, "CWD %s\r\n",newpath);
	blockingSendString(c, tosend);
	ret_code = blockingRecvString(c, tmp);
	if(ret_code == 250) {
		free(c->path);
		c->path = copyString(newpath);
		return 1;
	}
	else {
		printf("No access to %s\n",path);
	}

	return 0;
}

int clientChangeLocalDir(connection *c, char *path) {
	char tmp[1024];
	int root_len = 0;
	if(strcmp(path,"..") == 0) {
		changeDirectoryUp(c, c->root);
		return 1;
	}
	else {
		if(path[0] == '/') {
			memcpy(tmp,path,strlen(path)+1);
		}
		else {
			root_len = strlen(c->root);
			memcpy(tmp, c->root, root_len);
			if(root_len == 1)
				root_len--;
			tmp[root_len] = '/';
			memcpy(tmp+root_len+1,path,strlen(path)+1); /*get null terminator*/
		}
		if(pathIsDir(tmp)) {
			free(c->root);
			c->root = copyString(tmp);
			return 1;
		}
		else {
			return 0;
		}
	}
}

/**
 * Populate an internal structure with LIST data from the server
 */
filelist *getRemoteList(connection *c) {
	int ret_code;
	char *listings;
	char tmp[1024];
	filelist *fl;
	
	if(!clientPasv(c)){
		return NULL;
	}
	
	blockingSendString(c, "LIST -aL\r\n");

	/* get opened */
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 150) {
		printf("transfer isnt starting\n");
		return NULL;
	}

	/* get the list */
	listings = blockingRecvList(c->child);
	fl = parseList(listings);
	free(listings);

	/* get closed closeConnection called from RecvList*/
	ret_code = blockingRecvString(c, tmp);
	if(ret_code != 226) {
		printf("transfer didnt complete ok %d\n",ret_code);
		deleteFilelist(fl);
		fl = NULL;
	}
	sortFilelist(fl);
	return fl;
}

filelist *getLocalList(connection *c) {
#ifdef __DS__
	char filename[MAXPATHLEN];
#else
	struct dirent *direntry;
	int filelength;
	char *fullpath;
	char *filename;
#endif
	int pathlength;
	DIR *dir;
	struct stat stats;
	filelist *fl = createFilelist();
	char *pathname = c->root;

	pathlength = strlen(pathname);
	dir = opendir(pathname);
#ifdef DEBUG
	printf("listing %s\n",pathname);
#endif
	if(dir == NULL) {
		/*error not a dir or something*/
		printf("unable to open %s (%s)\n",pathname,strerror(errno));
		return 0;
	}

#ifdef __DS__
	while (dirnext(dir, filename, &stats) == 0) {
#else
#ifdef __PSP__
	fullpath = malloc( (pathlength + FILENAME_MAX + 2) * sizeof(char));
#else
	fullpath = malloc( (pathlength + NAME_MAX + 2) * sizeof(char));
#endif
	memcpy(fullpath, pathname, pathlength);
	if(pathlength != 1) {
		fullpath[pathlength] = '/';
	}
	while ( (direntry = readdir(dir)) ) {
/* psp .. has issues */
#ifdef __PSP__
		if(strcmp(direntry->d_name, "..") == 0) {
			addFile(fl, "..", 4096, 1);
			continue;
		}
#endif
		filelength = strlen(direntry->d_name);
		if(pathlength != 1) {
			memcpy(fullpath+pathlength+1, direntry->d_name, filelength + 1);
		}
		else {
			memcpy(fullpath+pathlength, direntry->d_name, filelength + 1);
		}
		stat(fullpath, &stats);
		filename = direntry->d_name;
		/*printf("Getting stats on %s (%x)\n",fullpath, stats.st_mode);*/
#endif
		addFile(fl, filename, (int)stats.st_size, S_ISDIR(stats.st_mode));
	}
#ifndef __DS__
	free(fullpath);
#endif
	closedir(dir);
	sortFilelist(fl);
	return fl;
}

#ifndef __PSP__
#ifndef __DS__

/**
 * THIS IS A CRAPPY LINUX PORT. THIS IS NOT WORKING AND PROBLY NEVER WILL.
 * THIS IS JUST A SIMPLE EXAMPLE OR STARTING POINT FOR PORTING TO OTHER
 * PLATFORMS.
 */
char keyboardText[1024];

void platformProgress(int perc, char *speed) {
	printf("(%d) %s\n",perc,speed);
}

saved_host *platformListHosts() {
	return NULL;
}

int platformOverwrite(char *s,int resume) {
	return 1;
}

int platformStor(connection *c, char *tmp, int filesize) {
	char cmdbuf[1024];
	struct stat stats;
	int go_ahead = 1;
	if(stat(tmp,&stats) == 0) {
		if(filesize >= (int)stats.st_size)
			go_ahead = platformOverwrite(tmp,0);
		else
			go_ahead = platformOverwrite(tmp,1);
	}
	if(go_ahead == 3)
		return 0;

	printf("Opening PASV\n");
	clientPasv(c);
	((connection *)(c->child))->tx->size = (int)stats.st_size;
	((connection *)(c->child))->direction = PUT;
	((connection *)(c->child))->f_type = BINARY;
	if(go_ahead) {
		((connection *)(c->child))->tx->file = fopen(tmp,"rb");
		if(go_ahead == 1) {
			printf("overwriting\n");
		}
		else if(go_ahead == 2) {
			printf("appending\n");
			fseek(((connection *)(c->child))->tx->file, filesize, SEEK_CUR);
			sprintf(cmdbuf, "REST %d\r\n",filesize);
			printf("tx: %s\n",cmdbuf);
			blockingSendString(c, cmdbuf);
			((connection *)(c->child))->tx->tx_progress = filesize;
			blockingRecvString(c, cmdbuf); /* i dont care what it is... just get it out of the buffer */
		}
	}
	return 1;
}

int platformRetr(connection *c, char *tmp, int filesize) {
	char cmdbuf[1024];
	struct stat stats;
	int go_ahead = 1;
	if(stat(tmp,&stats) == 0) {
		printf("file %s exists\n",tmp);
		if(filesize > (int)stats.st_size)
			go_ahead = platformOverwrite(tmp,1);
		else
			go_ahead = platformOverwrite(tmp,0);
	}
	if(go_ahead == 3)
		return 0;

	printf("Opening PASV\n");
	clientPasv(c);
	((connection *)(c->child))->tx->size = filesize;
	((connection *)(c->child))->direction = GET;
	((connection *)(c->child))->f_type = BINARY;
	if(go_ahead) {
		if(go_ahead == 1) {
			printf("overwriting %s\n",tmp);
			((connection *)(c->child))->tx->file = fopen(tmp,"wb");
		}
		else if(go_ahead == 2) {
			printf("appending\n");
			((connection *)(c->child))->tx->file = fopen(tmp,"ab");
			sprintf(cmdbuf, "REST %d\r\n",(int)stats.st_size);
			printf("tx: %s\n",cmdbuf);
			blockingSendString(c, cmdbuf);
			((connection *)(c->child))->tx->tx_progress = (int)stats.st_size;
			blockingRecvString(c, cmdbuf); /* i dont care what it is... just get it out of the buffer */
		}
	}
	return 1;
}

char *getKeyboard(int max, int column, char *pretext, char *current) {
	printf("%s",pretext);
	fflush(stdout);
	return gets(keyboardText);
}

void client_test(char *host, int port) {
	int done = 0;
	char buf[1024];
	tokens *t;
	filelist *fl_left;
	filelist *fl_right;
	connection *c = createClient(host, port);
	if(c == NULL) {
		printf("Unable to connect\n");
		return;
	}
	if( (clientLogin(c, "anonymous","stuff") != 1)) {
		printf("login failed\n");
		return;
	}
	clientChangeLocalDir(c,"tmp");
	while(!done) {
		fl_left = getLocalList(c);
		fl_right = getRemoteList(c);
		printf("---LEFT---\n");
		printFilelist(fl_left);
		printf("---RIGHT---\n");
		printFilelist(fl_right);
		deleteFilelist(fl_left);
		deleteFilelist(fl_right);

		gets(buf);
		t = getTokens(buf);
		if(strcmp(t->token[0], "cd") == 0)
			clientChangeRemoteDir(c,t->token[1]);
		else if(strcmp(t->token[0], "lcd") == 0)
			clientChangeLocalDir(c,t->token[1]);
		else if(strcmp(t->token[0], "get") == 0)
			clientRetr(c,t->token[1],0);
		else if(strcmp(t->token[0], "put") == 0)
			clientStor(c,t->token[1],0);
		else if(strcmp(t->token[0], "quit") == 0)
			done = 1;
		deleteTokens(t);
	}
	clientQuit(c);
}
#endif
#endif

