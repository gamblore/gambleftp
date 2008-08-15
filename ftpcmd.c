#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include "parser.h"
#include "filelist.h"
#include "ftpcmd.h"
#include "ftp.h"

#ifdef PSP_ADHOC
#include "psp_adhoc.h"
#endif

/**
 * fill the first bit
 */
static char getFileMode (mode_t mode) {
	/* These are the most common, so test for them first.  */
	if (S_ISREG (mode))
		return '-';
	if (S_ISDIR (mode))
		return 'd';

	/* Other letters standardized by POSIX 1003.1-2004.  */
	if (S_ISBLK (mode))
		return 'b';
	if (S_ISCHR (mode))
		return 'c';
	if (S_ISLNK (mode))
		return 'l';
	if (S_ISFIFO (mode))
		return 'p';

	return '?';
}

void getFileModeString(mode_t mode, char *buf) {
	buf[0] = getFileMode(mode);
	buf[1] = mode & S_IRUSR ? 'r' : '-';
	buf[2] = mode & S_IWUSR ? 'w' : '-';
	buf[3] = (mode & S_ISUID
			? (mode & S_IXUSR ? 's' : 'S')
			: (mode & S_IXUSR ? 'x' : '-'));
	buf[4] = mode & S_IRGRP ? 'r' : '-';
	buf[5] = mode & S_IWGRP ? 'w' : '-';
	buf[6] = (mode & S_ISGID
			? (mode & S_IXGRP ? 's' : 'S')
			: (mode & S_IXGRP ? 'x' : '-'));
	buf[7] = mode & S_IROTH ? 'r' : '-';
	buf[8] = mode & S_IWOTH ? 'w' : '-';
	buf[9] = (mode & S_IXOTH ? 'x' : '-');
	buf[10] = ' ';
	buf[11] = '\0';
}

int blockingRecvString(connection *c, char *string) {
	char return_code[4];
	int read = 0,i;
	string[0] = '\0';
	while(!strstr(string,"\r\n")) {
		i = recv(c->sock, string+read,1,0);
		if(i == -1)
			continue;
		if(i == 0) {
			return 0;
		}
		read += i;
		string[read] = '\0';
	}
	strncpy(return_code, string, 3);
	return_code[3] = '\0';
#ifdef DEBUG
	printf("rx: %s",string);
#endif
	return atoi(return_code);
}

int blockingSendString(connection *c, char *s){
	int sent_bytes = 0;
	int sent;
	int size = strlen(s);
	while(sent_bytes != size) {
		sent = send(c->sock, s + sent_bytes, size - sent_bytes, 0);
		sent_bytes += sent;
		if(sent == 0) {
			printf("Server not responding\n");
			return 0;
		}
	}
	return 1;
}

char *copyString(char *s) {
	char *ret;
	int len;
	if(s == NULL)
		return NULL;
	len = strlen(s) + 1;
	if( (ret = malloc(sizeof(char) * len)) == NULL) {
	    printf("failed malloc\n");
		return NULL;
	}
	memcpy(ret, s, len);
	return ret;
}

int getAbsolutePath(connection *c, char *dest, char *filename) {
	dest[0] = '\0';
	if(filename == NULL)
		return 0;
	if(strlen(c->root) != 1) {
		/*printf("adding root %s\n",c->root);*/
		strcat(dest,c->root);
	}
	if(filename[0] == '/') {
		strcat(dest,filename);
		return 1;
	}
	else {
		if(strlen(c->path) != 1)
			strcat(dest,c->path);
		strcat(dest,"/");
		strcat(dest,filename);
		return 1;
	}
}
int isFile(connection *c, char *s) {
	char filetmp[1024];
	getAbsolutePath(c, filetmp, s);
#ifdef __DS__
	return (!access(filetmp, F_OK));
#else
	struct stat stats;
	return (!stat(filetmp, &stats));
#endif
}

int getSize(connection *c, char *s) {
	struct stat stats;
	char filetmp[1024];
	getAbsolutePath(c, filetmp, s);
	if(stat(filetmp, &stats) == 0)
		return stats.st_size;
	else 
		return 0;
}

int getModtime(connection *c, char *file, char *buf) {
	struct tm *stdtime;
	struct stat stats;
	char filetmp[1024];
	getAbsolutePath(c, filetmp, file);
	if(stat(filetmp, &stats) == 0) {
		stdtime = localtime(&stats.st_mtime);
		sprintf(buf, "%04d%02d%02d%02d%02d%02d",
			   	1900 + stdtime->tm_year, stdtime->tm_mon+1, stdtime->tm_mday, 
				stdtime->tm_hour, stdtime->tm_min, stdtime->tm_sec);
	}
	else {
		return 0;
	}
	return 1;
}

int ftpRename(connection *c, char *from, char *to) {
	char tmp[1024];
	char filetmpfrom[1024];
	getAbsolutePath(c, filetmpfrom, from);
	char filetmpto[1024];
	getAbsolutePath(c, filetmpto, to);
	sprintf(tmp, "%s Renaming %s to %s\n", c->user, from, to);
	ftpLog(tmp);
	return (!rename(filetmpfrom, filetmpto));
}

void getAscTime(time_t mtime, char *mon, int *day, char *yeartime) {
	struct tm *stdtime;
	char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	int curyear;
	time_t curtime = time(NULL);
	stdtime = localtime(&curtime);
	curyear = stdtime->tm_year;

	/* get the access time in the struct */
	stdtime = localtime(&mtime);
	*day = stdtime->tm_mday;
	memcpy(mon, months[stdtime->tm_mon], 4);
	if(curyear == stdtime->tm_year) {
		sprintf(yeartime, "%.2d:%.2d", stdtime->tm_hour, stdtime->tm_min);
	}
	else {
		sprintf(yeartime, "%d", 1900 + stdtime->tm_year);
	}
}

int ftpList(connection *c, connection *c_tx, char *path, int full) {
#ifdef __DS__
	char filename[MAXPATHLEN];
#else
	struct dirent *direntry;
	int filelength;
	char *fullpath;
	char *filename;
#endif
	int pathlength;
	char month[4];
	char yeartime[6];
	int day;
	char filetmp[1024];
	char entry_buffer[2048];
	char permisions[12];
	DIR *dir;
	struct stat stats;
	char *pathname = path;
	if(pathname == NULL || (strcmp(pathname, "-aL") == 0)){
		getAbsolutePath(c, filetmp, c->path);
		pathname = filetmp;
	}
	else {
		getAbsolutePath(c, filetmp, path);
		if(!isFile(c, filetmp)) {
			pathname = filetmp;
		}
	}
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
			if(full) {
				sprintf(entry_buffer, "drwxr-xr-x 22 0 0 4096 Jun 17 18:16 ..\r\n");
			}
			else {
				sprintf(entry_buffer, "..");
			}
			blockingSendString(c_tx, entry_buffer);
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
		if(full) {
			getFileModeString(stats.st_mode, permisions);
			getAscTime(stats.st_mtime, month, &day, yeartime);
			sprintf(entry_buffer, "%s%d %d %d %d %s %d %s %s\r\n",
					permisions, stats.st_nlink, stats.st_uid, stats.st_gid, (int)stats.st_size, month, day, yeartime, filename);
		}
		else {
			sprintf(entry_buffer, "%s\r\n", filename);
		}
		blockingSendString(c_tx, entry_buffer);
	}
#ifndef __DS__
	free(fullpath);
#endif
	closedir(dir);
	return 1;
}

int ftpSend(connection *c, char *file) {
	int i = 0;
	struct stat stats;
	char tmp[1024];
	char filetmp[1024];
	getAbsolutePath((connection *)c->parent, filetmp, file);
	if( ( stat(filetmp, &stats) ) != 0) {
		printf("unable to stat %s\n",filetmp);
	}
	printf("sending %d bytes\n",(int)stats.st_size - c->tx->tx_progress);
	c->tx->size = stats.st_size;
	switch(c->f_type) {
		case ASCII: c->tx->file = fopen(filetmp, "r");break;
		case BINARY: c->tx->file = fopen(filetmp, "rb");break;
		default: break;
	}
	if(c->tx->file == NULL) {
		return 0;
	}
	if(c->tx->tx_progress != 0) {
		i = fseek(c->tx->file, c->tx->tx_progress, SEEK_SET);
		printf("seeking %d %s\n",i,strerror(errno));
	}
	setTXDirection(c, PUT);
	sprintf(tmp, "%s Sending %s\n", ((connection *)(c->parent))->user, filetmp);
	ftpLog(tmp);
	return 1;
}

int ftpRecv(connection *c, char *file, int append) {
	char *path;
	char *filename;
	int pathlength = 0;
	char response[25];
	int i;
	char tmp[1024];
	char filetmp[1024];
	if(file == NULL) {
#ifdef DEBUG
		printf("creating random filename\n");
#endif
		getAbsolutePath((connection *)c->parent, filetmp, file);
		pathlength = strlen(filetmp);
		if( (path = malloc(pathlength + 9)) == NULL) {
			printf("malloc failed.\n");
			closeConnection(c);
		}
		memcpy(path, filetmp, pathlength);
		if(pathlength == 1) {
			filename = path;
		}
		else{
			filename = path + pathlength;
		}
		filename[0] = '/';
		do {
			for(i=1; i < 8; i++) {
				filename[i] = (char)((rand() % 26) + 97);
			}
			filename[8] = '\0';
#ifdef DEBUG
			printf("trying %s\n",path);
#endif
		} while(isFile(c, path));
		sprintf(response, "150 FILE: %s\r\n", filename + 1);
		blockingSendString(c->parent, response);
	}
	else {
		getAbsolutePath((connection *)c->parent, filetmp, file);
		path = filetmp;
		printf("opening %s\n",path);
	}
	if(append) {
		switch(c->f_type) {
			case ASCII: c->tx->file = fopen(path, "a");break;
			case BINARY: c->tx->file = fopen(path, "ab");break;
			default: break;
		}
	}
	else {
		switch(c->f_type) {
			case ASCII: c->tx->file = fopen(path, "w");break;
			case BINARY: c->tx->file = fopen(path, "wb");break;
			default: break;
		}
	}
	if(c->tx->file == NULL) {
		printf("cannot open %s\n",path);
		return 0;
	}
	if(file == NULL) {
		free(path);
	}
	if(c->tx->tx_progress != 0) {
		i = fseek(c->tx->file, c->tx->tx_progress, SEEK_SET);
		printf("seeking %d %d\n", i, c->tx->tx_progress);
	}
	sprintf(tmp, "%s Receiving %s\n", ((connection *)(c->parent))->user, filetmp);
	ftpLog(tmp);
	setTXDirection(c, GET);
	return 1;
}

int deleteFolder(connection *c, char *path) {
	char filetmp[1024];
	getAbsolutePath(c, filetmp, path);
	if(isFile(c, path)) {
#ifdef __DS__
		return (unlink(filetmp) == 0);
#else
		return (rmdir(filetmp) == 0);
#endif
	}
	else 
		return 0;
}

int deleteFile(connection *c, char *path) {
	char filetmp[1024];
	getAbsolutePath(c, filetmp, path);
	if(isFile(c, path))
		return (unlink(filetmp) == 0);
	else 
		return 0;
}

int createFolder(connection *c, char *path) {
	char filetmp[1024];
	getAbsolutePath(c, filetmp, path);
	return (mkdir(filetmp, S_IRWXU) == 0);
}

int changeDirectory(connection *c, char *path) {
	char filetmp[1024];
	char *tmp;
	DIR *dir;
	int path_len = strlen(path);
	if(path_len > 1 && path[path_len-2] == '.' && path[path_len-1] == '.') {
		changeDirectoryUp(c, c->path);
	}
	if(path != NULL) {
		tmp = c->path;
		getAbsolutePath(c, filetmp, path);
		if(strlen(c->root) > 1)
			c->path = copyString(filetmp + strlen(c->root));
		else 
			c->path = copyString(filetmp);
		dir = opendir (filetmp);
		if(dir == NULL) {
			/*error not a dir or something*/
			free(c->path);
			c->path = tmp;
			return 0;
		}
		else{
			free(tmp);
		}
		closedir(dir);
		return 1;
	}
	else {
		return 0;
	}
}

int changeDirectoryUp(connection *c, char *path) {
	/*int i;*/
	char *tmp = NULL;
	int len = strlen(path);
	if(len == 1)
		return 1;
	tmp = strrchr(path, '/');
	/*
	for(i=0; i < len; i++) {
		if(c->path[i] == '/' && i != len - 1) {
			tmp = &c->path[i];
		}
	}
	*/
	if(tmp == path){
		tmp = path + 1;
	}
	if(tmp != NULL)
		*tmp = '\0';
	return 1;
}

