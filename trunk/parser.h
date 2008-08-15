#ifndef __PARSER_H__
#define __PARSER_H__
#include "ftp.h"
#include "filelist.h"

typedef struct{
	char **token;
	int size;
	int max_size;
}tokens;

/**
 *  Fills ret[] with strings of the commands from string s
 *  ret is pointer that will be malloced to fit the data properly
 *
 *  returns number of tokens
 */
tokens *getTokens(char* s);

char *mergeTokens(char **argv, int argc);
filelist *parseList(char *s);
/**
 *	Free all allocated memory assosiated with the command
 */
void deleteTokens(tokens *t);
void printTokens(tokens *t);

int parseExec(tokens *t, connection *c);

#endif

