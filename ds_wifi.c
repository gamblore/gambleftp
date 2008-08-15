/*
 * NDS specific code for FTP server and client
 * Adam Metcalf
 * (c) 2008
 *
 */

#include "../../ds_wifi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parser.h"
#include "frametiles.h"
#include "ftpcmd.h"
#include "clientcmd.h"
#include "keyboard.h"


#define		VCOUNT		(*((u16 volatile *) 0x04000006))

#define ECHO_ON  0
#define ECHO_OFF 1
#define MAX_TEXT 32

static int wifi_inited = 0;
char keyboardText[MAX_TEXT] = "";

/**
 * loads keyboardText with keyboard text
 */
char *getKeyboard(int max, int column, char *pretext, char *current) {
	char tmp[32] = "";
	int i;
	u16 *map = (u16*)BG_MAP_RAM(1);
	videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE); /* 3 text 1 rotation */
	if(current)
		strcpy(keyboardText, current);
	else
		strcpy(keyboardText, "");
	/* make sure the screen is not touched before taking input */
	while(1) {
		scanKeys();
		i = keysHeld();
		if(!(i & KEY_TOUCH)) 
			break;
	}
	while(1) {
		scanKeys();
		if(processKeyboard(keyboardText, max, ECHO_ON) == '\n')
			break;
		for(i = 0; i < 32; i++) {
			map[i + column * 32] = 15 << 12 | (u16)(' ') ;
		}
		sprintf(tmp, "%s%s", pretext, keyboardText);
		defaultDrawString(1,column,tmp);
		while(VCOUNT>192); // wait for vblank
		while(VCOUNT<192);
	}
	videoSetMode(MODE_0_2D | DISPLAY_BG1_ACTIVE | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE); /* 3 text 1 rotation */
	return keyboardText;
}

/**
 * deletes my graphics object
 */
void clearBox(ds_box *b) {
	int i,j;
	if(!b)
		return;
	int x = b->x;
	int y = b->y;
	int w = b->w;
	int h = b->h;
	for(j = y; j < y+h; j++) {
		for(i = x; i < x+w; i++) {
			b->map[i + j * 32] = 0;
			b->textMap[i + j *32] = 15 << 12 | (u16)(' ');
		}
	}
	free(b);
	BG2_Y0 = 0;
}

/**
 * makes a nice display box. Uses palettes loaded at boot time. Uses the tile
 * engine to make it look really good.
 */
ds_box *makeBox(u16 *mapMemory, u16 *txtMap, int x, int y, int w, int h, char *s) {
	int i,j;
	ds_box *b;
	if( (b = malloc(sizeof(ds_box))) == NULL) {
		printf("failed malloc");
		return NULL;
	}
	b->x = x;
	b->y = y;
	b->w = w;
	b->h = h;
	b->map = mapMemory;
	b->textMap = txtMap;

	BG2_Y0 = 4;
	for(j = y; j < y+h; j++) {
		for(i = x; i < x+w; i++) {
			if(j==y) {
				if(i==x) 
					b->map[i + j * 32] = 5;
				else if(i == w + x - 1)
					b->map[i + j * 32] = 5 | FLIPHOR;
				else
					b->map[i + j * 32] = 6;
			}
			else if(j == h + y - 1) {
				if(i==x) 
					b->map[i + j * 32] = 5 | FLIPVER;
				else if(i == w + x - 1)
					b->map[i + j * 32] = 5 | FLIPVER | FLIPHOR;
				else
					b->map[i + j * 32] = 6 | FLIPVER;
			}
			else {
				if(i==x)
					b->map[i + j * 32] = 4;
				else if(i == w + x - 1)
					b->map[i + j * 32] = 4 | FLIPHOR;
				else
					b->map[i + j * 32] = 0;
			}
		}	
	}
	mDrawString(b->textMap, x+1, y+1, s);
	return b;
}

/**
 * Tests if the box contains the touchPosition
 */
int inBox(ds_box *b, touchPosition touch) {
	if(!b)
		return 0;
	if(touch.px >= b->x * 8 && touch.px <= (b->x + b->w) * 8
			&& touch.py >= b->y * 8 && touch.py <= (b->y + b->h) * 8)
		return 1;
	return 0;
}

/**
 * Users select what to do with the selected file. A filename is passed in to give
 * the user an idea of what they are working with.
 *
 * returns an index of the option selected. 
 *
 * TODO should have defined these instead of just using random arbitrary ints.
 */
int launchMenu(char *filename) {
	int i;
	int ret = -1;
	touchPosition touch_down;
	ds_box *boxes[5];
	mClearScreen(TEXTLEFT);
	mDrawString(TEXTLEFT,1,1,filename);

	boxes[0] = makeBox((u16*)BG_MAP_RAM(0),TEXTLEFT,7,3,18,2, "    Transfer");
	boxes[1] = makeBox((u16*)BG_MAP_RAM(0),TEXTLEFT,7,7,18,2, "  Create Folder");
	boxes[2] = makeBox((u16*)BG_MAP_RAM(0),TEXTLEFT,7,11,18,2,"     Rename");
	boxes[3] = makeBox((u16*)BG_MAP_RAM(0),TEXTLEFT,7,15,18,2,"     Delete");
	boxes[4] = makeBox((u16*)BG_MAP_RAM(0),TEXTLEFT,7,19,18,2,"     Cancel");

	while(ret == -1) {
		scanKeys();
		i = keysDown();
		if(i & KEY_TOUCH) {
			touch_down = touchReadXY();
			for(i = 0; i < 5; i++) {
				if(inBox(boxes[i], touch_down))
					ret = i;
			}
		}
	}

	for(i = 0; i < 5; i++)
		clearBox(boxes[i]);
	return ret;
}

/**
 * A basic confirmation screen given to the user when he is about to overwrite
 * a file either localy or remotly.
 *
 * params 
 *	filename
 *	Whether the user is displayed the option to resume.
 *
 * returns user input
 *		1 overwrite
 *		2 resume
 *		3 cancel
 */
int platformOverwrite(char *tmp, int resume) {
	int i;
	int ret = 0;
	touchPosition touch_down;
	ds_box *boxes[3];
	mClearScreen(TEXTLEFT);
	mDrawString(TEXTLEFT,1,1,tmp);
	boxes[0] = makeBox((u16*)BG_MAP_RAM(0),TEXTLEFT,8,4,16,2,"  Overwrite");
	if(resume) {
		boxes[1] = makeBox((u16*)BG_MAP_RAM(0),TEXTLEFT,8,8,16,2,"    Resume ");
	}
	else {
		boxes[1] = NULL;
	}
	boxes[2] = makeBox((u16*)BG_MAP_RAM(0),TEXTLEFT,8,12,16,2,"    Cancel ");

	while(!ret) {
		scanKeys();
		i = keysDown();
		if(i & KEY_TOUCH) {
			touch_down = touchReadXY();
			for(i = 0; i < 3; i++) {
				if(inBox(boxes[i], touch_down))
					ret = i + 1;
			}
		}
	}

	for(i = 0; i < 3; i++)
		clearBox(boxes[i]);
	return ret;
}

/* Dswifi helper functions */
/* wifi timer function, to update internals of sgIP */
/* turned to 30 ms because 50 was too slow.*/
void Timer_50ms(void) {
   Wifi_Timer(30);
}

/* notification function to send fifo message to arm7 */
void arm9_synctoarm7() { /* send fifo message */
   REG_IPC_FIFO_TX=0x87654321;
}

/* interrupt handler to receive fifo messages from arm7 */
void arm9_fifo() { /* check incoming fifo messages */
   u32 value = REG_IPC_FIFO_RX;
   if(value == 0x87654321) Wifi_Sync();
}

/**
 * initializes the FIFO between this and the ARM7
 * then turns on scan mode.
 */
void initWifi() {
	if(!wifi_inited) {
		wifi_inited = 1;
		// send fifo message to initialize the arm7 wifi
		REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR; // enable & clear FIFO

		u32 Wifi_pass= Wifi_Init(WIFIINIT_OPTION_USEHEAP_512);
		REG_IPC_FIFO_TX=0x12345678;
		REG_IPC_FIFO_TX=Wifi_pass;

		*((volatile u16 *)0x0400010E) = 0; // disable timer3

		irqSet(IRQ_TIMER3, Timer_50ms); // setup timer IRQ
		irqEnable(IRQ_TIMER3);
		irqSet(IRQ_FIFO_NOT_EMPTY, arm9_fifo); // setup fifo IRQ
		irqEnable(IRQ_FIFO_NOT_EMPTY);

		REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_RECV_IRQ; // enable FIFO IRQ

		Wifi_SetSyncHandler(arm9_synctoarm7); // tell wifi lib to use our handler to notify arm7

		// set timer3
		*((volatile u16 *)0x0400010C) = -6553; // 6553.1 * 256 cycles = ~50ms;
		*((volatile u16 *)0x0400010E) = 0x00C2; // enable, irq, 1/256 clock

		while(Wifi_CheckInit()==0) { // wait for arm7 to be initted successfully
			while(VCOUNT>192); // wait for vblank
			while(VCOUNT<192);
		}
		/* start scanning mode */
		Wifi_ScanMode();
	}
}

/**
 * DS doesnt have a nice network config screen so i added a shared host screen
 *
 * These functions deal with creation, deletion and selection of the access points.
 *
 * This is very similar to the saved host api. I wish I made a common "config parser"
 * but that would just make more work as this works just fine.
 */
void deleteAccessPoints(saved_ap **aps) {
	int i;
	if(aps == NULL)
		return;
	for(i = 0; i < 22 && aps[i] != NULL; i++) {
		free(aps[i]->ssid);
		free(aps[i]->wepkey);
		free(aps[i]);
	}
	free(aps);
}

saved_ap *createAP(char *ssid, char *wepkey) {
	saved_ap *ret;
	if( (ret = malloc(sizeof(saved_ap))) == NULL) {
		printf("malloc failed\n");
		return NULL;
	}
	ret->ssid = copyString(ssid);
	ret->wepkey = copyString(wepkey);
	return ret;
}

/**
 * forms a ap: wepkey: array
 */
saved_ap **getAccessPoints() {
	char tmp[1024] = "";
	char *ap;
	char *wep;
	int read;
	int readpos = 0;
	int numap = 0;
	saved_ap **ret;
	FILE *apcfg = fopen("ap.cfg","r");
	if(apcfg == NULL) {
		return NULL;
	}
	if( (ret = calloc(22, sizeof(saved_ap *))) == NULL) {
		printf("calloc failed\n");
		fclose(apcfg);
		return NULL;
	}
	while( (read = fread(tmp+readpos, sizeof(char), 1, apcfg))) {
		if(readpos > 1023)
			printf("your breaking things. Fuck off.\n");
		if(tmp[readpos] == '\n') {
			tmp[readpos] = '\0';
			ap = tmp+3;
			wep = strstr(tmp,"wep:");
			wep[-1] = '\0';
			wep += 4;
			readpos = 0;
			ret[numap++] = createAP(ap,wep);
		}
		else
			readpos++;
	}
	fclose(apcfg);
	return ret;
}

void addAP(Wifi_AccessPoint *apdata, char *wepkey) {
	FILE *ap;
	saved_ap ** aps;
	if(apdata == NULL)
		return;
	aps = getAccessPoints();
	if(aps){
		int i;
		for(i = 0; i < 22 && aps[i] != NULL; i++) {
			if(strcmp(apdata->ssid, aps[i]->ssid) == 0) {
				deleteAccessPoints(aps);
				return;
			}
		}
		deleteAccessPoints(aps);
	}

	/* never seen this before */
	ap = fopen("ap.cfg","a");
	printf("saving %s\n",apdata->ssid);
	if(apdata->flags & WFLAG_APDATA_WEP && wepkey)
		fprintf(ap, "ap:%s wep:%s\n",apdata->ssid, wepkey);
	else
		fprintf(ap, "ap:%s wep:\n",apdata->ssid);

	fclose(ap);
}

int checkSavedAP() {
	int i,j;
	int numap;
	Wifi_AccessPoint apdata;
	saved_ap **tmpap = getAccessPoints();
	if(tmpap) {
		numap = Wifi_GetNumAP();
		for(i = 0; i < numap; i++) {
			if(Wifi_GetAPData(i, &apdata) == -1) {
				printf("cannot get apdata\n");
				return -1;
			}
			for(j = 0; j < 22 && tmpap[j] != NULL; j++) {
				if(strcmp(apdata.ssid,tmpap[j]->ssid) == 0) {
					strcpy(keyboardText, tmpap[j]->wepkey);
					deleteAccessPoints(tmpap);
					return i;
				}
			}
		}
		deleteAccessPoints(tmpap);
	}
	return -1;
}

/**
 * Helpers for drawing to the tilesystem
 *  ClearScreen clears the screen to ' '
 *  DrawString renders the screen at the coordinates 
 */

void mClearScreen(u16* map) {
	int i;
	for(i = 0; i < 32 * 64; i++) {
		map[i] = 15 << 12 | (u16)(' ');
	}
}

void mDrawString(u16* map, int x, int y, char *s) {
	int i;
	if(!s)
		return;
	int len = strlen(s);
	for(i = 0; i < len; i++) {
		map[x + i + y * 32] = 15 << 12 | (u16)(s[i]) ;
	}
}

/**
 * Fills the screen with a filelist uses the ds_window to find rendering points
 *
 * if int full is true the size will not be calculated from the file list entries.
 * This is usefull to make generic lists.
 */
int populateWindow(ds_window *w, int full) {
	filelist *fl = w->fl;
	u16 *map = w->map;
	int i,j;
	char tmp[32];
	listelm *l;
	mClearScreen(map);
	/* set offset to 0 */
	if(w->base < 0)
		w->base = 0;
	if(map == TEXTLEFT)
		BG2_Y0 = w->top * 8; 
	else
		BG3_Y0 = w->top * 8;

	for(i=0; i + w->base < fl->size && i <62; i++) {
		l = fl->data[i+w->base];
		memset(tmp,'\0',32);
		if(full) {
			strncpy(tmp, l->filename, 30);
		}
		else {
			strncpy(tmp, l->filename, 10);
		}
		if(l->filesize != 0) {
			if(!l->isDir){
				if(l->filesize > 1024*1024) {
					sprintf(tmp+9, " %dM", (l->filesize+1)/(1024*1024));
				}
				else if(l->filesize > 1024) {
					sprintf(tmp+9, " %dK", (l->filesize+1)/1024);
				}
				else {
					sprintf(tmp+9, " %d", (l->filesize+1));
				}
				for(j=0;j<11;j++) {
					if(tmp[j] == '\0')
						tmp[j] = ' ';
				}
			}
			else {
				if(!full)
					tmp[10] = '\0';
			}

		}
		mDrawString(map,1,i+1,tmp);
	}
	return w->base;
}

/**
 * Gets a list element from a touch position relative to position on the screen
 */
listelm *getSelected(ds_window *w, touchPosition touch, int *selected) {
	listelm *tmplist;
	*selected = w->top + (touch.py / 8);
	if(*selected > 0 && *selected < w->fl->size + 1) {
		tmplist = getListAt(w->fl, *selected + w->base - 1);
		if(tmplist) {
			printf("%d (%s)\n",*selected, tmplist->filename);
		}
		return tmplist;
	}
	*selected = 0;
	return NULL;
}

/**
 * Switches between styles of screen 
 *
 * STYLE_OPEN there is no bar in the middle
 * STYLE_SPLIT there is a bar in the middle splitting the display into two windows
 */
void changeBackground(u16 *mapMemory, style s) {
	int i;
	mClearScreen(TEXTLEFT);
	mClearScreen(TEXTRIGHT);

	if(s == STYLE_OPEN) {
		BG3_X0 = 0;
		for(i = 0; i < 24; i++) {
			if(i == 0) {
				mapMemory[15 + i * 32] = 3;
				mapMemory[16 + i * 32] = 3;
				continue;
			}
			if(i == 23) {
				mapMemory[15 + i * 32] = 3 | 1 << 11;
				mapMemory[16 + i * 32] = 3 | 1 << 11;
				continue;
			}
			mapMemory[15 + i * 32] = 0;
			mapMemory[16 + i * 32] = 0;
		}
	}
	else if(s == STYLE_SPLIT) {
		BG3_X0 = 128;
		for(i = 0; i < 24; i++) {
			if(i == 0) {
				mapMemory[15 + i * 32] = 2 | 1 << 10;
				mapMemory[16 + i * 32] = 2;
				continue;
			}
			if(i == 23) {
				mapMemory[15 + i * 32] = 2 | 1 << 11 | 1 << 10;
				mapMemory[16 + i * 32] = 2 | 1 << 11;
				continue;
			}
			mapMemory[15 + i * 32] = 1 | 1 << 10;
			mapMemory[16 + i * 32] = 1;
		}
	}
}

/**
 * generic scrollable list that uses iPod Touch navigation ideas. 
 *
 * returns index when a item is double tapped.
 */
int scrollList(filelist *fl) {
	int i;
	int moved;
	int direction = -1;
	int dy = 0;
	int selected = 0;
	int prevselected = -1;
	listelm *tmplist;
	touchPosition touch;
	touchPosition touch_down = {0,0,0,0};
	ds_window *w = createDSWindow(TEXTLEFT,fl);
	changeBackground((u16*)BG_MAP_RAM(0), STYLE_OPEN);
	populateWindow(w,1);
	do {
		scanKeys();
	} while(keysHeld() || keysDown());
	while(1) {
		scanKeys();
		i = keysDown();
		if(i & KEY_TOUCH) {
			touch_down = touchReadXY();
			w->speed = 0;
			moved = 0;
			do{
				scanKeys();
				if(!keysHeld() & KEY_TOUCH)
					break;
				touch = touchReadXY();
				if(touch.x == 0 || touch.y == 0)
					break;
				if(w->speed != 4)
					w->speed++;
				dy = touch_down.py - touch.py;
				if(dy < -16 || dy > 16) {
					moved = 1;
					touch_down = touch;
					/*moving up */
					if(dy < -16) {
						if(direction != 0)
							w->speed = 1;

						direction = 0;
						if(w->fl->size > 22)
							w->top -= w->speed;
						if(w->top < 0) {
							if(w->base == 0)
								w->top = 0;
							else {
								w->top = 64 - 24;
								w->base -= 64 - 24;
							}
							populateWindow(w,1);
						}
						/* if base isnt 0 change the base */
					}
					else { /* moving down */
						if(direction != 1)
							w->speed = 1;

						direction = 1;
						if(w->fl->size - w->base - w->top > 22) {
							w->top += w->speed;
						}
						if(w->top > (64-24)) {
							w->top = 0;
							w->base += 64 - 24;
							populateWindow(w,1);
						}
					}
				}
				BG2_Y0 = w->top * 8;
				BG3_Y0 = w->top * 8;
				scanKeys();
			}while(keysHeld() & KEY_TOUCH);
			if(moved != 0) {
				continue;
			}
			
			tmplist = getSelected(w, touch_down, &selected);
			if(tmplist) {
				if(prevselected == selected) {
					printf("changing to %s\n",tmplist->filename);
					w->top = 0;
					populateWindow(w,1);
					prevselected = -1;
					tmplist = NULL;
					deleteDSWindow(w);
					return selected - 1;
				}
				else {
					/*do nothing*/
					prevselected = selected;
				}
			}
		}
		else if(i & KEY_B) {
			deleteDSWindow(w);
			return -1;
		}
	}
	deleteDSWindow(w);
	return -1;
}

/**
 * waits for a connection to an Access Point be fully established
 *
 * If connection is established the access point is stored in the saved AP database
 */
int connectLoop(Wifi_AccessPoint *apdata, char *wep) {
	int i = -1;
	int prev_i = -2;
	while(1) {
		i=Wifi_AssocStatus(); // check status
		if(i==ASSOCSTATUS_SEARCHING && prev_i != i) {
			printf("Searching for Access Point\n");
		}
		if(i==ASSOCSTATUS_AUTHENTICATING && prev_i != i) {
			printf("Authenticating\n");
		}
		if(i==ASSOCSTATUS_ASSOCIATING && prev_i != i) {
			printf("Associating\n");
		}
		if(i==ASSOCSTATUS_ACQUIRINGDHCP && prev_i != i) {
			printf("Acquiring DHCP\n");
		}
		if(i==ASSOCSTATUS_ASSOCIATED && prev_i != i) {
			printf("Connected successfully!\n");
			addAP(apdata, wep);
			return 1;
		}
		if(i==ASSOCSTATUS_CANNOTCONNECT) {
			printf("Could not connect!\n");
			return 0;
		}
		prev_i = i;
	}
}

/**
 * Requriment of the client implementation. Displays a list of previously connected hosts.
 * User can select a host or choose to return NULL and will be prompted for connection
 * information
 */
saved_host *platformListHosts() {
	saved_host *hosts;
	saved_host *tmph; 
	int selected = -1;
	int i;
	saved_host *ret = NULL;
	char tmp[1024];
	hosts = getSavedHosts();
	if(hosts == NULL)
		return NULL;
	filelist *fl = createFilelist();
	if(fl == NULL) {
		deleteSavedHosts(hosts);
		return NULL;
	}
	for(tmph = hosts; tmph != NULL; tmph = tmph->next) {
		sprintf(tmp,"%s@%s:%s", tmph->host[0],tmph->host[2],tmph->host[3]);
		tmp[30] = '\0';
		addFile(fl,tmp,0,0);
	}
	if(fl->size == 0) {
		deleteSavedHosts(hosts);
		deleteFilelist(fl);
		return NULL;
	}

	iprintf("scrolling\n");
	defaultDrawString(1,3,"scrolling");
	selected = scrollList(fl);
	iprintf("selected %d\n",selected);

	if(selected == -1)
		return NULL;
	
	for(tmph = hosts, i = 0; i < selected && tmph->next != NULL; i++, tmph = tmph->next);
	if(tmph)
		ret = createHost(tmph->host[0],tmph->host[1],tmph->host[2],tmph->host[3]);
	deleteSavedHosts(hosts);
	/* deleteFilelist(fl); done in scroll list */
	defaultClearScreen();
	return ret;
}

/**
 * lists availible access points in the area. WEP or WPA is conncated to SSID's
 * to see if there are any open APs
 */
int listAp() {
	int i;
	int selected = -1;
	int numap;
	Wifi_AccessPoint apdata;
	filelist *fl;
	char tmp[32];
	tmp[31] = '\0';

	defaultClearScreen();
	numap = Wifi_GetNumAP();
	fl = createFilelist();
	for(i = 0; i < numap; i++) {
		if(Wifi_GetAPData(i, &apdata) == -1) {
			printf("cannot get apdata\n");
			defaultClearScreen();
			defaultDrawString(1,1,"Cannot get apdata");
			deleteFilelist(fl);
			return 0;
		}
		strncpy(tmp, apdata.ssid, 30);
		if(apdata.ssid_len == 0) {
			strcpy(tmp, "{empty ssid}");
		}
		if(apdata.flags & WFLAG_APDATA_WPA) {
			strcat(tmp," WPA");
		}
		else if(apdata.flags & WFLAG_APDATA_WEP) {
			strcat(tmp," WEP");
		}
		addFile(fl,tmp,0,0);
	}
	if(numap == 0) {
		printf("zero ap's\n");
		deleteFilelist(fl);
		return -1;
	}
	while(selected < 0 || selected > numap + 1) {
		selected = scrollList(fl);
		if(selected == -1)
			return -1;
		defaultClearScreen();
		Wifi_GetAPData(selected, &apdata);
		defaultDrawString(1, 1, apdata.ssid);
		printf("selected '%s'\n",apdata.ssid);
		if(apdata.flags & WFLAG_APDATA_WPA) {
			printf("wpa not supported\n");
			return -1;
		}
		return selected;
	}
	return -1;
}

/**
 * Requests webkeys if requried then alerts dswifi to connect. The connect loop is then called
 * until the it either gives up or connects.
 */
int connectToAp(int autoconnect) {
	int i, j;
	Wifi_AccessPoint apdata;
	char tmp[64];
	unsigned char wepkey[] =
	"penismightier";
	if(!wifi_inited) {
		return -1;
	}
	
	/* simple WFC connect:*/
	if(autoconnect) {
		printf("Connecting via WFC data\n");
		Wifi_AutoConnect(); // request connect
	}
	else {
		i = checkSavedAP();
		if(i == -1)
			i = listAp();
		else
			autoconnect = 1;
		if(i == -1)
			return 0;
		j = Wifi_GetAPData(i, &apdata);

		if(j != 0) {
			printf("wifi_getAPData returned %d\n",j);
			defaultClearScreen();
			defaultDrawString(1,1,"Cannot get apdata");
			return 0;
		}
		sprintf(tmp,"connect to %s",apdata.ssid);
		defaultClearScreen();
		defaultDrawString(1,1, tmp);
	    defaultDrawString(1,2, "Press A or Touch to connect");
		defaultDrawString(1,3, "Press B to search again");
		while(1) {
			scanKeys();
			i = keysDown();
			if(i & (KEY_TOUCH | KEY_A)) {
				Wifi_DisconnectAP();
				/*request dhcp */
				Wifi_SetIP(0,0,0,0,0);
				if(apdata.flags & WFLAG_APDATA_WEP) {
					if(!autoconnect) {
						defaultClearScreen();
						defaultDrawString(1,1, tmp);
						defaultDrawString(1,2, "WEP key required");
						getKeyboard(32,4,"WEPKEY: ", NULL);
					}
					Wifi_ConnectAP(&apdata, WEPMODE_128BIT, 0, (unsigned char *)keyboardText);
				}
				else
					Wifi_ConnectAP(&apdata, WEPMODE_NONE, 0, wepkey);
				return connectLoop(&apdata, keyboardText);
			}
			else if(i & KEY_B)
				return 0;
		}
	}
	return connectLoop(NULL, NULL);
}

/**
 * Client requirement. Displays progress to the screen using DrawString
 */
void platformProgress(int perc, char *speedbuffer) {
	char tmp[128];

	sprintf(tmp,"(%.2d%%) %s", perc, speedbuffer);
	mClearScreen(TEXTLEFT);
	mDrawString(TEXTLEFT,9,10,tmp);
}

/**
 * All things to do with the client are here. It handles user input and triggers
 * clientcmd.h functions. To interact with a FTP server.
 *
 * returns status of exit.
 *		1 continue start from beginning
 *		0 exit completly
 */
int client() {
	int i;
	int ret;
	int column = -1;
	int prevcolumn = -1;
	int direction = -1;
	int dy = 0;
	int selected = 0;
	int prevselected = 0;
	int moved;
	int menu = -1;
	char *tmpchar;
	touchPosition touch;
	touchPosition touch_down = {0,0,0,0};
	ds_window *w_left = createDSWindow(TEXTLEFT,NULL);
	ds_window *w_right = createDSWindow(TEXTRIGHT,NULL);
	ds_window *w = NULL;

	listelm *tmplist;
	listelm *tmpfile;

	connection *c = clientConnect();
	if(c == NULL) {
		printf("failed to connect\n");
		return 1;
	}

	w_left->fl = getLocalList(c);
	w_right->fl = getRemoteList(c);

	changeBackground((u16*)BG_MAP_RAM(0), STYLE_SPLIT);
	
	populateWindow(w_left,0);
	populateWindow(w_right,0);
	
	while(1) {
		menu = -1;
		scanKeys();
		i = keysDown();
		if(i & KEY_TOUCH) {
			touch_down = touchReadXY();
			w_left->speed = 0;
			w_right->speed = 0;
			moved = 0;
			column = (touch_down.px > (256/2));
			if(column == 0)
				w = w_left;
			else
				w = w_right;
			do{
				scanKeys();
				if(!keysHeld() & KEY_TOUCH)
					break;
				touch = touchReadXY();
				if(touch.x == 0 || touch.y == 0)
					break;
				if(w->speed != 4)
					w->speed++;
				dy = touch_down.py - touch.py;
				if(dy < -16 || dy > 16) {
					moved = 1;
					touch_down = touch;
					/*moving up */
					if(dy < -16) {
						if(direction != 0)
							w->speed = 1;

						direction = 0;
						if(w->fl->size > 22)
							w->top -= w->speed;
						if(w->top < 0) {
							if(w->base == 0)
								w->top = 0;
							else {
								w->top = 64 - 24;
								w->base -= 64 - 24;
							}
							populateWindow(w,0);
						}
						/* if base isnt 0 change the base */
					}
					else { /* moving down */
						if(direction != 1)
							w->speed = 1;

						direction = 1;
						if(w->fl->size - w->base - w->top > 22) {
							w->top += w->speed;
						}
						if(w->top > (64-24)) {
							w->top = 0;
							w->base += 64 - 24;
							populateWindow(w,0);
						}
					}
				}
				if(w == w_left) {
					BG2_Y0 = w->top * 8;
				}
				else {
					BG3_Y0 = w->top * 8;
				}
				scanKeys();
			}while(keysHeld() & KEY_TOUCH);
			if(moved != 0) {
				continue;
			}
			
			tmplist = getSelected(w, touch_down, &selected);
			if(tmplist) {
				if(prevselected == selected && prevcolumn == column) {
					prevcolumn = -1;
					if(tmplist->isDir) {
						printf("changing to %s\n",tmplist->filename);
						if(w == w_left) {
							clientChangeLocalDir(c,tmplist->filename);
							deleteFilelist(w->fl);
							w->fl = getLocalList(c);
						}
						else {
							clientChangeRemoteDir(c,tmplist->filename);
							deleteFilelist(w->fl);
							w->fl = getRemoteList(c);
						}
						w->top = 0;
						populateWindow(w, 0);
						prevselected = -1;
						tmplist = NULL;
						menu = -1;
					}
					else {
						changeBackground((u16*)BG_MAP_RAM(0), STYLE_OPEN);
						printf("launching menu\n");
						menu = launchMenu(tmplist->filename);
						printf("menu item: %d\n",menu);
					}
					if(menu > -1) {
						if(menu == 0) {
							if(w == w_left) {
								tmpfile = findFile(w_right->fl, tmplist->filename);
								if(tmpfile) {
									printf("file exists '%s' size %d\n",tmpfile->filename, tmpfile->filesize);
									clientStor(c,tmplist->filename,tmpfile->filesize);
								}
								else
									clientStor(c,tmplist->filename,0);
								deleteFilelist(w_right->fl);
								w_right->fl = getRemoteList(c);
							}
							else {
								tmpfile = findFile(w_right->fl, tmplist->filename);
								if(tmpfile) {
									printf("file exists '%s' size %d\n",tmpfile->filename, tmpfile->filesize);
									clientRetr(c,tmplist->filename,tmpfile->filesize);
								}
								else
									clientRetr(c,tmplist->filename,0);
								deleteFilelist(w_left->fl);
								w_left->fl = getLocalList(c);
							}
						}
						else if(menu == 1) { /* create folder */
							tmpchar = copyString(getKeyboard(128,4,"folder name: ", NULL));
							if(strlen(tmpchar)){
								if(w == w_left) {
									clientLocalMakeDirectory(c, tmpchar);
									deleteFilelist(w->fl);
									w->fl = getLocalList(c);
								}
								else {
									clientRemoteMakeDirectory(c, tmpchar);
									deleteFilelist(w->fl);
									w->fl = getRemoteList(c);
								}
							}
							free(tmpchar);
						}
						if(menu == 2) { /* rename file */
							tmpchar = copyString(getKeyboard(128,4,"new name: ", tmplist->filename));
							if(strlen(tmpchar)){
								if(w == w_left) {
									ret = clientLocalRename(c,tmplist->filename, tmpchar);
									deleteFilelist(w->fl);
									w->fl = getLocalList(c);
								}
								else {
									clientRemoteRename(c,tmplist->filename, tmpchar);
									deleteFilelist(w->fl);
									w->fl = getRemoteList(c);
								}
							}
							free(tmpchar);
						}
						else if(menu == 3) { /* delete file */
							if(w == w_left) {
								ret = clientLocalDelete(c,tmplist->filename, tmplist->isDir);
								deleteFilelist(w->fl);
								w->fl = getLocalList(c);
							}
							else {
								clientRemoteDelete(c,tmplist->filename, tmplist->isDir);
								deleteFilelist(w->fl);
								w->fl = getRemoteList(c);
							}
						}
					}
					changeBackground((u16*)BG_MAP_RAM(0), STYLE_SPLIT);
					populateWindow(w_left, 0);
					populateWindow(w_right, 0);
				}
				else {
					/*do nothing*/
					prevselected = selected;
					prevcolumn = column;
				}
			}
		}
		else if(i & KEY_X) {
			printf("quiting ftp\n");
			clientQuit(c);
			break;
		}
	}
	deleteDSWindow(w_left);
	deleteDSWindow(w_right);
	mClearScreen(TEXTLEFT);
	mClearScreen(TEXTRIGHT);
	changeBackground((u16*)BG_MAP_RAM(0), STYLE_OPEN);
	return 1;
}

/**
 * Initialize and call either server or client
 */
int main(void) {
	struct sockaddr_in my_addr;  /* my server address info */
	struct in_addr s;
	int server_sock;
	int i;
	int connect_type = 0;
	char *my_ip;

	lcdSwap();
	defaultExceptionHandler();
	videoSetMode(MODE_0_2D | DISPLAY_BG1_ACTIVE | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE); /* 2 text 2 rotation */
	videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);	/* sub bg 0 will be used to print text */
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	irqInit(); 
	irqEnable(IRQ_VBLANK);
	/*irqSet(IRQ_VBLANK, blankHandler);
	  irqEnable(IRQ_VBLANK);
	*/

	initKeyboard(); /* uses map base(2) and tile base(3) */
	BG_PALETTE[0] = 1 << 15 | RGB15(0, 0, 0); /* nothing */
	BG_PALETTE[1] = RGB15(8, 15, 18); /* border color */
	BG_PALETTE[2] = RGB15(4, 8, 9); /* border outline */
	BG_PALETTE[3] = RGB15(15, 15, 15); /* grey */
	BG_PALETTE[255] = RGB15(31, 31, 31);
	BG_PALETTE_SUB[255] = RGB15(31,31,31);	/* by default font will be rendered with color 255 */

	u8* tileMemory = (u8*)BG_TILE_RAM(1);
	u16* mapMemory = (u16*)BG_MAP_RAM(0);
	u16* textMapMemoryleft = (u16*)BG_MAP_RAM(1);
	u16* textMapMemoryright = (u16*)BG_MAP_RAM(3);

	/* clear the text area */
	for(i = 0; i < 32 * 64; i++) {
		textMapMemoryleft[i] = 15 << 12 | (u16)(' ');
		textMapMemoryright[i] = 15 << 12 | (u16)(' ');
	}
	

	SUB_BG0_CR = BG_MAP_BASE(31); /* init text on sub bg0 */
	BG3_CR = BG_32x64 | BG_MAP_BASE(3) | BG_TILE_BASE(2) | BG_PRIORITY(3); /* init text on bg3 */
	BG2_CR = BG_32x64 | BG_MAP_BASE(1) | BG_TILE_BASE(2) | BG_PRIORITY(2); /* init text on bg2 */
	BG1_CR = BG_32x32 | BG_COLOR_256 | BG_MAP_BASE(0) | BG_TILE_BASE(1) | BG_PRIORITY(1); /* init graphics on bg2 */


	/* load the font into main and sub memory blocks for both screens printf will be handled by sub block */
	consoleInitDefault((u16*)SCREEN_BASE_BLOCK(1), (u16*)CHAR_BASE_BLOCK(2), 16);
	consoleInitDefault((u16*)SCREEN_BASE_BLOCK_SUB(31), (u16*)CHAR_BASE_BLOCK_SUB(0), 16);

	//mDrawString(textMapMemory, 1,1, "Hello Welcome to the server");
	defaultDrawString(1,1,"Welcome to FTP");
	mDrawString(TEXTRIGHT,1,2,"Loading.");

	/* drop our frame in the tile memory */
	swiCopy(frametiles, tileMemory, 32*7);

	/* populate the map */
	for(i = 0; i < 32 * 32; i++)
		mapMemory[i] = framemap[i];

	/* init the wifi interupt handlers and data */
	initWifi();
	mDrawString(TEXTRIGHT,1,2,"Loading...");

	/* init the filesystem set sd card as default */
	if(!fatInitDefault()) {
		printf("wtf lib fat sucks\n");
	}
	scanKeys();
	i = keysDown();
	if( (i & KEY_L) && (i &  KEY_R) ) {
		unlink("ap.cfg");
		unlink("hosts.cfg");
	}
	i = 0;
	while(i != 1) {
		/* connect to the wifi network */
		defaultClearScreen();
		mClearScreen(TEXTRIGHT);
		defaultDrawString(1,1,"Tap the screen to choose an AP");
		defaultDrawString(1,2,"Or press A to autoconnect");

		while(1) {
			scanKeys();
			i = keysDown();
			if(i & KEY_A) {
				connect_type = 0;
				break;
			}
			else if(i & KEY_TOUCH) {
				connect_type = 1;
				break;
			}
		}

		if(connect_type == 0) {
			defaultDrawString(1,4,"AutoConnecting");
			i = connectToAp(1);

		}
		else {
			defaultDrawString(1,4,"Launching Manual Connecter");
			i = connectToAp(0);
		}
	}
	defaultClearScreen();


	s.s_addr = Wifi_GetIP();
	my_ip = inet_ntoa( s ); 
	printf("To start server press A.\nTo start client press B.\n");
	defaultDrawString(1,1,"To start server press A.");
	defaultDrawString(1,2,"To start client press Y.");
	while(1) {
		scanKeys();
		i = keysDown();
		if(i & KEY_A) {
			connect_type = 0;
			break;
		}
		else if(i & KEY_Y) {
			connect_type = 1;
			break;
		}
	}
	mClearScreen(TEXTLEFT);
	mClearScreen(TEXTRIGHT);
	if(connect_type == 0) {
		defaultDrawString(1,1,my_ip);
		/*printf("Listening on %d.%d.%d.%d\n",(char)my_ip >> 24, (char)my_ip >> 16, (char)my_ip >> 8, (char)my_ip);*/
		printf("Listening on \n%s:%d\n",my_ip, SERVER_PORT);

		/* Create a TCP socket */
		connection *master = initMasterListen(&my_addr, SERVER_PORT);
		sscanf(my_ip,"%d.%d.%d.%d",&master->ip[0],&master->ip[1],&master->ip[2],&master->ip[3]);
		server_sock = master->sock;
		printf("Created Socket.\n");

		server_loop((void *)&server_sock);
	}
	else {
		while(client());
	}
	printf("Done!\n");
	return 0;
}
