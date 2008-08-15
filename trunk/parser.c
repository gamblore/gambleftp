#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "parser.h"
#include "ftpcmd.h"
#include "filelist.h"
#include "ftp.h"

#ifdef __PSP__
#include <psppower.h>
#elif __DS__
#include "ds_wifi.h"
#endif

char *mergeTokens(char **argv, int argc) {
	int i;
	int length = 0;
	char *ret;
	for(i = 0; i < argc; i++) {
		length += strlen(argv[i]);
	}
	if( (ret = malloc(length+argc+1)) == NULL) {
		printf("failed malloc.\n");
		return NULL;
	}
	*ret = 0;
	for(i = 0; i < argc; i++) {
		if(i != 0) 
			strcat(ret, " ");
		strcat(ret, argv[i]);
	}
	return ret;
}

void notImplemented(connection *c) {
	blockingSendString(c, "502 Not Implemented\r\n");
}
void syntaxError(connection *c) {
	blockingSendString(c, "501 Syntax error in parameters or arguments.\r\n");
}

/**
 *	Executes based on tokens and a connection
 */
int parseExec(tokens *t, connection *c) {
	int ret;
#ifdef __PSP__
	int battery_life = 0;
#elif __DS__
	TransferRegion volatile * ipc;
#endif

	struct stat stats;
	FILE *password_file;
	char tmp[2048];
	char tmppass[128];
	char *help_message = "214 The following commands are recognized (* =>'s unimplemented):\r\n\
 CWD     XCWD    CDUP    XCUP    SMNT*   QUIT    PORT    PASV\r\n\
 EPRT*    EPSV*    ALLO   RNFR    RNTO    DELE    MDTM    RMD\r\n\
 XRMD    MKD     XMKD    PWD     XPWD    SIZE    SYST    HELP\r\n\
 NOOP    FEAT*    OPTS*    AUTH*   CCC*    CONF*   ENC*    MIC*\r\n\
 PBSZ*   PROT*   TYPE    STRU*    MODE    RETR    STOR    STOU\r\n\
 APPE    REST    ABOR    USER    PASS    ACCT*   REIN   LIST\r\n\
 NLST    STAT    SITE\r\n";
	char *ascii = "ASCII";
	char *binary = "BINARY";
	connection *c_tx = NULL;
	char *cmd = t->token[0];
	char **argv = &t->token[1];
	char *merged = mergeTokens(argv, t->size-1);

	/* int argc = t->size - 1; */
	/* basic control */
	if(strcmp(cmd, "USER") == 0) {
		if(c->login_status < 2) {
			ret = strlen(argv[0]);
			if(ret > 19) {
				/* too long */
				syntaxError(c);
			}
			else {
				c->login_status = 1;
				free(c->user);
				c->user = copyString(argv[0]);
				sprintf(tmp, "331 Password required for %s\r\n",c->user);
				blockingSendString(c, tmp);
			}
		}
		else {
			blockingSendString(c, "403 Authorization Required\r\n");
		}
		free(merged);
		return 1;
	}
	else if(strcmp(cmd, "PASS") == 0) {
		if(c->login_status == 1) {
			if(strcmp(c->user,"anonymous") == 0) {
				c->login_status = 2;
				blockingSendString(c, "230 User logged in.\r\n");
				/* if we have a pub folder use it for anon */
				if(stat("/pub", &stats) == 0) {
					free(c->root);
					c->root = copyString("/pub");
				}
			}
			else {
				/* no user control installed */
				if( (password_file = fopen("passwd", "r")) == NULL) {
					blockingSendString(c, "230 User logged in.\r\n");
					c->login_status = 2;
					free(merged);
					return 1;
				}
				while(fscanf(password_file, "%s %s",tmp,tmppass) != EOF) {
					if( (strcmp(tmp,c->user) == 0) && (strcmp(tmppass, argv[0]) == 0)) {
						c->login_status = 2;
						blockingSendString(c, "230 User logged in.\r\n");
						fclose(password_file);
						free(merged);
						return 1;
					}
				}
				fclose(password_file);
				blockingSendString(c, "403 Authorization Required\r\n");
				free(c->user);
				c->user = NULL;
				c->login_status = 0;
			}
		}
		else {
			blockingSendString(c, "403 Authorization Required\r\n");
		}
		free(merged);
		return 1;
	}
	else if(strcmp(cmd, "HELP") == 0) {
		blockingSendString(c, help_message);
		free(merged);
		return 1;
	}
	else if(strcmp(cmd, "NOOP") == 0) {
		blockingSendString(c, "200 NOOP command successful\r\n");
		free(merged);
		return 1;
	}
	else if(strcmp(cmd, "FEAT") == 0) {
		notImplemented(c);
	}
	else if(strcmp(cmd, "SYST") == 0) {
#ifdef __PSP__
		blockingSendString(c, "215 UNIX Type: PS\r\n");
#elif __DS__
		blockingSendString(c, "215 UNIX Type: ND\r\n");
#else
		blockingSendString(c, "215 UNIX Type: L8\r\n");
#endif
		free(merged);
		return 1;
	}
	else if(strcmp(cmd, "QUIT") == 0) {
		printf("User quit %p\n",c);
		blockingSendString(c, "221 Goodbye.\r\n");
		closeConnection(c);
		free(merged);
		return 2;
	}
	/*check that your logged in for the rest of the commands*/
	if(c->login_status != 2){
		blockingSendString(c, "530 Not logged in.\r\n");
		free(merged);
		return 1;
	}
	else if(strcmp(cmd, "CWD") == 0 || strcmp(cmd, "XCWD") == 0) {
		if(changeDirectory(c, merged)) {
			sprintf(tmp, "250 CWD command successful\r\n");
		}
		else {
			sprintf(tmp, "550 %s: Directory does not exist\r\n",merged);
		}
		blockingSendString(c, tmp);
	}
	else if(strcmp(cmd, "ACCT") == 0) {
		notImplemented(c);
	}
	else if(strcmp(cmd, "SMNT") == 0) {
		notImplemented(c);
	}
	else if(strcmp(cmd, "STRU") == 0) {
		if(argv[0]){
			switch(argv[0][0]){
				case 'F': blockingSendString(c, "200 Structure set to F\r\n");break;
				default: notImplemented(c);break;
			}
		}
	}
	else if(strcmp(cmd, "CDUP") == 0 || strcmp(cmd, "XCUP") == 0) {
		ret = changeDirectoryUp(c,c->path);
		sprintf(tmp, "200 Directory changed to %s\r\n",c->path);
		blockingSendString(c, tmp);
	}
	else if(strcmp(cmd, "REIN") == 0) {
		blockingSendString(c, "220 Reinitializing.\r\n");
		c->login_status = 0;
	}
	/* file control */
	else if(strcmp(cmd, "PORT") == 0) {
		if(argv[0]) {
			if(!c->child) {
				c_tx = openConnection(-1, TRANSFER, c);
				createTransfer(c_tx, PORT, argv[0]);
			}
			else {
				c_tx = c->child;
			}
			if(c_tx->sock == -1) {
				blockingSendString(c, "425 Can't open data connection.\r\n");
				closeConnection(c_tx);
			}
			else {
				blockingSendString(c, "200 PORT Succesfull\r\n");
			}
		}
		else {
			syntaxError(c);
			
		}
	}
	else if(strcmp(cmd, "PASV") == 0) {
		if(!c->child) {
			c_tx = openConnection(-1, PASV_LISTEN, c);
			createTransfer(c_tx, PASV, NULL);
		}
		else {
			c_tx = c->child;
		}
		if(c_tx->sock == -1) {
			blockingSendString(c, "425 Server error\r\n");
			closeConnection(c_tx);
		}
		else {
			sprintf(tmp, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).\r\n",
						c->ip[0],c->ip[1],c->ip[2],c->ip[3],(unsigned char)(c_tx->port >> 8), (unsigned char)c_tx->port);
			blockingSendString(c, tmp);
		}
	}
	else if(strcmp(cmd, "TYPE") == 0) {
		if(argv[0]){
			sprintf(tmp, "200 Type set to %c\r\n", argv[0][0]);
			switch(argv[0][0]){
				case 'I': c->f_type = BINARY; blockingSendString(c, tmp);break;
				case 'A': c->f_type = ASCII; blockingSendString(c, tmp);break;
				default: c->f_type = ASCII; blockingSendString(c, "500 Not Implemented\r\n");break;
			}
		}
	}
	else if(strcmp(cmd, "STRU") == 0) {
		if(argv[0]){
			switch(argv[0][0]){
				case 'F': blockingSendString(c, "200 Structure set to F\r\n");break;
				default: notImplemented(c);break;
			}
		}
	}
	else if(strcmp(cmd, "MODE") == 0) {
		if(argv[0]){
			switch(argv[0][0]){
				case 'S': blockingSendString(c, "200 Mode set to S\r\n");break;
				default:  notImplemented(c);break;
			}
		}
	}
	else if(strcmp(cmd, "RNFR") == 0) {
		if(argv[0]){
			if(isFile(c, merged)) {
				c->renamepath = copyString(merged);
				blockingSendString(c, "350 Requested file action pending further information.\r\n");
			}
			else {
				blockingSendString(c, "550 Requested action not taken.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "RNTO") == 0) {
		if(argv[0]){
			if(c->renamepath) {
				ret = ftpRename(c, c->renamepath, merged);
				if(ret) {
					blockingSendString(c, "250 Requested file action okay, completed.\r\n");
				}
				else {
					blockingSendString(c, "553 Requested action not taken.\r\n");
				}
				free(c->renamepath);
				c->renamepath = NULL;
			}
			else {
				blockingSendString(c, "503 Bad sequence of commands.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "RETR") == 0) {
		if(argv[0]) {
			printf("sending %s\n",merged);
			if(isFile(c, merged)) {
				if(c->child && ((connection *)(c->child))->tx->connected) {
					blockingSendString(c, "125 Data connection already open; transfer starting.\r\n");
					ftpSend(((connection *)(c->child)), merged);
				}
				else {
					blockingSendString(c, "425 Can't open data connection.\r\n");
				}
			}
			else {
				blockingSendString(c, "550 Requested action not taken.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "STOR") == 0) {
		if(argv[0]) {
			if(c->child) {
				if(((connection *)(c->child))->tx->connected) {
					if(ftpRecv(c->child, merged, 0)) {
						if(c->f_type == BINARY) {
							sprintf(tmp, "150 Opening %s mode data connection for %s\r\n", binary, merged);
						}
						else {
							sprintf(tmp, "150 Opening %s mode data connection for %s\r\n", ascii, merged);
						}
						blockingSendString(c, tmp);
					}
					else {
						blockingSendString(c, "550 Requested action not taken.\r\n");
					}
				}
				else {
					blockingSendString(c, "425 Can't open data connection. Not Connected.\r\n");
				}
			}
			else {
				blockingSendString(c, "425 Can't open data connection. Too fast.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "STOU") == 0) {
		if(argv[0]) {
			syntaxError(c);
		}
		if(c->child) {
			if(((connection *)(c->child))->tx->connected) {
				if(ftpRecv(c->child, NULL, 0)) {
					if(c->f_type == BINARY) {
						sprintf(tmp, "150 Opening %s mode data connection for data\r\n", binary);
					}
					else {
						sprintf(tmp, "150 Opening %s mode data connection for data\r\n", ascii);
					}
					blockingSendString(c, tmp);
				}
				else {
					blockingSendString(c, "550 Requested action not taken.\r\n");
				}
			}
			else {
				blockingSendString(c, "425 Can't open data connection.\r\n");
			}
		}
		else {
			blockingSendString(c, "425 Can't open data connection.\r\n");
		}
	}
	else if(strcmp(cmd, "APPE") == 0) {
		if(argv[0]) {
			if(c->child) {
				if(((connection *)(c->child))->tx->connected) {
					if(ftpRecv(c->child, merged, 1)){
						if(c->f_type == BINARY) {
							sprintf(tmp, "150 Opening %s mode data connection for %s\r\n", binary, argv[0]);
						}
						else {
							sprintf(tmp, "150 Opening %s mode data connection for %s\r\n", ascii, argv[0]);
						}
						blockingSendString(c, tmp);
					}
					else {
						blockingSendString(c, "550 Requested action not taken.\r\n");
					}
				}
				else {
					blockingSendString(c, "425 Can't open data connection.\r\n");
				}
			}
			else {
				blockingSendString(c, "425 Can't open data connection.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "ALLO") == 0) {
		blockingSendString(c, "200 NOOP command successful\r\n");
	}
	else if(strcmp(cmd, "SIZE") == 0) {
		if(argv[0]) {
			if(isFile(c, merged)) {
				ret = getSize(c, merged);		
				sprintf(tmp, "213 %d\r\n", ret);
				blockingSendString(c, tmp);
			}
			else {
				blockingSendString(c, "550 Requested action not taken.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "MDTM") == 0) {
		if(argv[0]) {
			if(isFile(c, merged)) {
				if(getModtime(c, merged, tmppass)) {
					sprintf(tmp, "213 %s\r\n", tmppass);
					blockingSendString(c, tmp);		
				}
				else {
					blockingSendString(c, "550 Requested action not taken.\r\n");
				}
			}
			else {
				blockingSendString(c, "550 Requested action not taken.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "REST") == 0) {
		if(argv[0]) {
			if(c->child && ((connection *)(c->child))->tx) {
				if(((connection *)(c->child))->tx->file) {
					blockingSendString(c, "503 Bad sequence of commands.\r\n");
				}
				else if( (sscanf(argv[0], "%d", &((connection *)(c->child))->tx->tx_progress)) == 1) {
					blockingSendString(c, "350 Requested file action pending further information.\r\n");
				}
				else {
					syntaxError(c);
				}
			}
			else {
				blockingSendString(c, "503 Bad sequence of commands.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "ABOR") == 0) {
		if(c->child) {
			if(((connection *)(c->child))->tx->connected && ((connection *)(c->child))->tx->tx_progress > 0) {
				blockingSendString(c, "426 Connection closed; transfer aborted.\r\n");
			}
			closeConnection(c->child);
		}
		blockingSendString(c, "226 Closing data connection.\r\n");
	}
	else if(strcmp(cmd, "DELE") == 0) {
		if(argv[0]) {
			if(isFile(c, merged)) {
				if(deleteFile(c, merged)) {
					blockingSendString(c, "250 DELE command successful\r\n");
				}
				else {
					blockingSendString(c, "550 Requested action not taken.\r\n");
				}
			}
			else {
				blockingSendString(c, "550 Requested action not taken.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "RMD") == 0 || strcmp(cmd, "XRMD") == 0) {
		if(argv[0]) {
			if(isFile(c, merged)) {
				if(deleteFolder(c, merged)) {
						blockingSendString(c, "250 RMD command successful\r\n");
				}
				else {
					if(errno == ENOTEMPTY){
						sprintf(tmp, "550 %s: Directory not empty\r\n",argv[0]);
						blockingSendString(c, tmp);
					}
					else {
						blockingSendString(c, "550 Requested action not taken.\r\n");
					}
				}
			}
			else {
				blockingSendString(c, "550 Requested action not taken.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "MKD") == 0 || strcmp(cmd, "XMKD") == 0) {
		if(argv[0]) {
			if(createFolder(c, merged)) {
				sprintf(tmp, "257 \"%s\" - Directory successfully created\r\n", argv[0]);
				blockingSendString(c, tmp);
			}
			else {
				blockingSendString(c, "550 Requested action not taken.\r\n");
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "PWD") == 0 || strcmp(cmd, "XPWD") == 0) {
		sprintf(tmp, "257 \"%s\" is the current directory\r\n", c->path);
		blockingSendString(c, tmp);
	}
	else if(strcmp(cmd, "LIST") == 0) {
		if(c->child) {
			if(((connection *)(c->child))->tx->connected) {
				blockingSendString(c, "150 Opening ASCII mode data connection for file list\r\n");
				ftpList(c, c->child, argv[0], 1);
				blockingSendString(c, "226 Closing data connection.\r\n");
			}
			else {
				blockingSendString(c, "425 Can't open data connection.\r\n");
			}
			closeConnection(c->child);
		}
		else {
			blockingSendString(c, "425 Can't open data connection.\r\n");
		}
	}
	else if(strcmp(cmd, "NLST") == 0) {
		if(c->child) {
			if(((connection *)(c->child))->tx->connected) {
				blockingSendString(c, "150 Opening ASCII mode data connection for file list\r\n");
				ftpList(c, c->child, argv[0], 0);
				blockingSendString(c, "226 Closing data connection.\r\n");
			}
			else {
				blockingSendString(c, "425 Can't open data connection.\r\n");
			}
			closeConnection(c->child);
		}
		else {
			blockingSendString(c, "425 Can't open data connection.\r\n");
		}
	}
	else if(strcmp(cmd, "SITE") == 0) {
		if(argv[0]) {
			if(strcmp(argv[0],"BATT") == 0) {
#ifdef __PSP__
				battery_life = scePowerGetBatteryLifeTime();
				sprintf(tmp,"200 Life %d%% (%02dh%02dm)\r\n",
						scePowerGetBatteryLifePercent(), battery_life/60, battery_life-(battery_life/60*60));
				blockingSendString(c, tmp);
#elif __DS__
				ipc = getIPC();
				sprintf(tmp,"200 LIFE %d\r\n", (short)ipc->battery);
				blockingSendString(c, tmp);
#else
				notImplemented(c);
#endif
			}
			else {
				notImplemented(c);
			}
		}
		else {
			syntaxError(c);
		}
	}
	else if(strcmp(cmd, "STAT") == 0) {
		if(argv[0]) {
			if(isFile(c, merged)) {
				ftpList(c, c, argv[0], 1);
				blockingSendString(c, "200 STAT Command okay.\r\n");
			}
			else {
				sprintf(tmp, "450 %s: No such file or directory\r\n",argv[0]);
				blockingSendString(c, tmp);
			}
		}
		else {
			if(c->child) {
				sprintf(tmp, "Connected\r\n Logged in\r\n TYPE: %s, STRUcture: File, Mode: Stream\r\n Data Connection Open %d bytes transfered\r\n", 
						((c->f_type == ASCII) ? ascii : binary), ((connection *)(c->child))->tx->tx_progress);
			}
			else {
				sprintf(tmp, "Connected\r\n Logged in\r\n TYPE: %s, STRUcture: File, Mode: Stream\r\n No data connection\r\n", 
						((c->f_type == ASCII) ? ascii : binary));
			}
			blockingSendString(c, tmp);
		}
	}
	else {
		free(merged);
		return 0;
	}
	free(merged);
	return 1;
}

void printTokens(tokens *t) {
	int i;
	printf("token size %d\n", t->size);
	for(i=0; i < t->size; i++) {
		printf("%s ",t->token[i]);
	}
	printf("\n");
}

filelist *parseList(char *data) {
	int i;
	char *filename;
	char *crlf;
	char *merged;
	int isdir;
	tokens *tmptokens;
	filelist *fl = createFilelist();
	while((crlf = strstr(data, "\r\n"))) {
		isdir = 0;
		/* get rid of crlf, get tokens, then set up the loop again*/
		crlf[0] = '\0';
		crlf[0] = '\0';
		tmptokens = getTokens(data);
		data = crlf + 2;
		if(tmptokens->token[0][0] == 'd' && tmptokens->size > 1) {
			isdir = 1;
		}
		/* for NLST not used as its nice to get size etc in the listing */
		filename = tmptokens->token[tmptokens->size - 1];
		if(tmptokens->size == 1) {
			merged = mergeTokens(tmptokens->token, tmptokens->size);
			addFile(fl, filename, 0, isdir);
		}
		else {
			for(i = 0; i < tmptokens->size; i++) {
				if( (strlen(tmptokens->token[i]) == 5 && tmptokens->token[i][2] == ':') ||  (i > 4 && strlen(tmptokens->token[i]) == 4))
					break;
			}
			if(i == tmptokens->size) {
				printf("crap i dont know how to parse the list\n");
				i = 7;
			}
			i++;
			merged = mergeTokens(tmptokens->token+i, tmptokens->size - i);
			addFile(fl, merged, atoi(tmptokens->token[4]), isdir);
		}
		free(merged);
		deleteTokens(tmptokens);
	}
	return fl;
}

void deleteTokens(tokens *t) {
	int i;
	for(i=0; i < t->size; i++) {
		free(t->token[i]);
	}
	free(t->token);
	free(t);
}

tokens *getTokens(char* s) {
	int i = 0;
	int j = 0;
	int k = 0;
	char *past_pointer;
	int len = strlen(s);
	tokens *ret;
#ifdef DEBUG
	printf("PARSER: %s received\n", s);
#endif
	if ( (ret = malloc(sizeof(tokens))) == NULL) {
		printf("failed malloc\n");
		return NULL;
	}
	ret->size = 0;
	ret->max_size = 2;
	/* start with 2 and realloc if needed */
	if ( (ret->token = calloc(2, sizeof(char*))) == NULL) {
		printf("failed malloc\n");
		deleteTokens(ret);
		return NULL;
	}
	/* while whitespace */
	for(i=0; i < len; i++) {
		j = 0;
#ifdef DEBUG
		printf("PARSER: parsing for whitespace\n");
#endif
		for(; ( s[i] <= 32 || s[i] == 127 ) && i < len; i++ );
		if(i >= len) break;
		/* mark token start */
		past_pointer = &s[i];
#ifdef DEBUG
		printf("PARSER: starting here %s\n",past_pointer);
#endif

		/* find token end */
		for(; s[i] >32 && s[i] != 127 && i < len; i++, j++);

		if(j > 0) {
			if(ret->size == ret->max_size) {
#ifdef DEBUG
				printf("PARSER: needed more space size: %d max: %d\n", ret->size, ret->max_size);
#endif
				if( (ret->token = realloc(ret->token, sizeof(char*) * ret->max_size * 2)) == NULL) {
					printf("failed tokenizer realloc\n");
					deleteTokens(ret);
					return NULL;
				}
				ret->max_size = ret->max_size * 2;
#ifdef DEBUG
				printf("PARSER: new_max: %d\n", ret->max_size);
#endif

				/* set to null so we can free it without thinking twice */
				for(k=ret->size; k < ret->max_size; k++) {
					ret->token[k] = NULL;
				}
				
			}
#ifdef DEBUG
			printf("PARSER: getting %d bytes for token %d\n", j, ret->size);
#endif
			if( (ret->token[ret->size] = malloc(sizeof(char) * j + 1)) == NULL) {
				printf("failed malloc\n");
				deleteTokens(ret);
				return NULL;
			}
			memcpy(ret->token[ret->size], past_pointer, j);
			/* NULL terminate the token for safety */
			ret->token[ret->size][j] = '\0';
#ifdef DEBUG
			printf("PARSER: got '%s'\n",ret->token[ret->size]);
#endif
			ret->size++;
		}
	}

	return ret;
}

