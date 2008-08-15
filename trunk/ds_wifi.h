#ifndef __DS_WIFI_H__
#define __DS_WIFI_H__

#include "nds.h"
#include <nds/arm9/console.h> //basic print funcionality
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/dir.h>
#include <fat.h>

#include <dswifi9.h>

#define printf iprintf

#define TEXTLEFT (u16*)BG_MAP_RAM(1)
#define TEXTRIGHT (u16*)BG_MAP_RAM(3)
typedef enum{STYLE_OPEN = 0, STYLE_SPLIT} style;

#define defaultDrawString(x,y,s) mDrawString((u16*)BG_MAP_RAM(1), x, y, s)
#define defaultClearScreen() mClearScreen((u16*)BG_MAP_RAM(1))


#define HELLO_MSG   "Hello there. Type away.\r\n"
#define SERVER_PORT 21

typedef struct{
	int x;
	int y;
	int w;
	int h;
	u16 *map;
	u16 *textMap;
}ds_box;

typedef struct{
	char *ssid;
	char *wepkey;
}saved_ap;
/* extern the functions devined by server.o */

extern int server_loop(void *argv);
extern int init_server(struct sockaddr_in *my_addr, int port);

void mClearScreen(u16* map);
void mDrawString(u16* map, int x, int y, char *s);
int connectToAp(int autoconnect);

#endif

