#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include "clientcmd.h"

/**
 * Get input from the user. I dont care how. Return in a static char array
 * calling the function again will clear the previous text. ftpcmd.h has
 * a nice copyString() function you can use with this to make dynamic pointers
 *
 * params
 *		max: integer the max size of the input the user is allowed
 *		column: users can specify where they would like the data to echo
 *		pretext: What would be displayed to the user
 *		current: any text that you would like to have filled in already
 *
 * returns pointer to the static array
 *
 * /example 
 *	 /get user input on port:21 max size is 5 characters
 *   /prefer to be displayed on column 4
 *	 getKeyboard(5, 4, "port:", "21")
 * /example
 */
char *getKeyboard(int max, int column, char *pretext, char *current);

/**
 * Display to the user a prompt of the current transfer progress
 * "My percent transfered is '%percent' tranfering at '$speed'\n"
 *
 * params
 *		int percent: an integer 0-100 of the current percentage transfered
 *		char *speed: a string "12KB/s" for example. Max speed is in MB/s
 *
 *	returns nothing
 */
void platformProgress(int percent, char *speed);

/**
 * Display to the user a prompt along the lines of 
 * "This will overwrite '$file'. Are you sure you want to do this?"
 *
 * params
 *		char *file: The pathname of the platform that is to be overwriten
 *		int canresume: if this is true you show the overwrite options 
 *			otherwise you hide that option from them
 * returns int action
 *		0 Something went wrong
 *		1 overwite
 *		2 resume
 *		3 cancel
 */
int platformOverwrite(char *file, int canresume);

/**
 * This is how you display the hosts previously connected to.
 *
 * It is up to you to determine how you would like to do this. This could simply
 * be console printf's of the lists and an a enter number. The choice is up to you.
 *
 * returns saved_host* to connct to
 *		NULL allow them to enter in custom information via getKeyboard
 */
saved_host *platformListHosts();

/**
 * Do what you want with this. Call the clientcmd functions to create
 * a graphical design of the client.
 *
 * returns status 
 *		0 if something when wrong
 *		1 if exited it a safe manner
 */
int client();

#endif

