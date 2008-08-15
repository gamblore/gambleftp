/*
 * PSP specific code for FTP server and client
 * Adam Metcalf
 * (C) 2008
 */
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include "psp_wifi.h"
#include "psp_text.h"
#include "psp_util.h"
#include "clientcmd.h"
#include "ftp.h"
#include "ftpcmd.h"
#include "platform.h"

/* macros to setup your thread in the OS on execution */
#if (_PSP_FW_VERSION >= 200) 
PSP_MODULE_INFO(MODULE_NAME, 0, 1, 1);
PSP_HEAP_SIZE_KB(20480);
#else
PSP_MODULE_INFO(MODULE_NAME, 0x1000, 1, 1);
PSP_MAIN_THREAD_ATTR(0);
#endif

void drawGU();

/* some global variables only possible because this is single threaded */
extern char *keyboardText;
int running = 1;
int adhoc = 0;

#ifdef PSP_ADHOC
unsigned char adhoc_mac[6];
char adhoc_nick[128];
unsigned char mymac[8];
char mynick[128];
#endif

/* A draw list used to send commands to the Gu */
unsigned int __attribute__((aligned(16))) list[262144];
/* A nice square drawn with two different colored triangles */
struct Vertex __attribute__((aligned(16))) vertices[2*3] =
{
	{0xffeeaa7f,1, 1,0}, // 0
	{0xffeeaa7f, 1, -1,0}, // 1
	{0xffeeaa7f, -1, -1,0}, // 2

	{0xffee557f, -1, -1,0}, // 0
	{0xffee557f, -1, 1,0}, // 2
	{0xffee557f,1, 1,0} // 3
};

/**
 * Initializes the Graphics unit on the psp.
 */
void setupGu() {
	sceGuInit();

	sceGuStart(GU_DIRECT,list);
	sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,(void*)0x88000,BUF_WIDTH);
	sceGuDepthBuffer((void*)0x110000,BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH/2),2048 - (SCR_HEIGHT/2));
	sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
	sceGuDepthRange(0xc350,0x2710);
	sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

	sceGuFinish();
	sceGuSync(0,0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
}

/**
 * This draws to the screen before the filelists
 *
 * On the psp it creates a line in between the two sides of the comander layout
 *
 */
void drawPSPClient() {
	struct Vertex *v;
	struct Vertex *v1, *v2, *v3;
	int i;
	sceGuStart(GU_DIRECT, list);
	{
		sceGuDisable(GU_TEXTURE_2D);
		v = sceGuGetMemory(sizeof(struct Vertex) * 3 * 2);

		for(i=0; i<2; i++) {
			v1 = &v[i*3];
			v2 = &v[i*3+1];
			v3 = &v[i*3+2];

			v1->color = 0xffeeaa7f;
			if(i==0){
				v1->x = 238;
				v1->y = 0;
				v1->z = 0.0f;
			}
			else {
				v1->x = 240;
				v1->y = 280;
				v1->z = 0.0f;
			}
			//v1->s = 238;
			//v1->t = 0;

			v2->color = 0xffeeaa7f;
			v2->x = 238;
			v2->y = 280;
			v2->z = 0.0f;
			//v2->s = 0;
			//v2->t = 0;
			
			v3->color = 0xffeeaa7f;
			v3->x = 240;
			v3->y = 0;
			v3->z = 0.0f;

		}
		sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2*3, 0, v);
	}
	sceGuFinish();
	sceGuSync(0,0);
}

/**
 * A simple draw function that rotates for the shape for following calls
 */
void drawGU() {
	static int val = 0;

	sceGuStart(GU_DIRECT, list);
	{
		sceGuDisable(GU_TEXTURE_2D);

		sceGuClearColor(0xff554433);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

		sceGumMatrixMode(GU_PROJECTION);
		sceGumLoadIdentity();
		sceGumPerspective(75.0f,16.0f/9.0f,0.5f,1000.0f);

		sceGumMatrixMode(GU_VIEW);
		sceGumLoadIdentity();

		sceGumMatrixMode(GU_MODEL);
		sceGumLoadIdentity();

		sceGumPushMatrix();
		sceGumLoadIdentity();
		ScePspFVector3 rot = {0,0, val * 1.3f * (M_PI/180.0f) };
		ScePspFVector3 pos = { 9.5f, -5.5f, -10.0f };
		sceGumTranslate(&pos);
		sceGumRotateXYZ(&rot);

		sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2*3, 0, vertices);
		sceGumPopMatrix();
	}
	sceGuFinish();
	sceGuSync(0,0);
	
	val++;
}

/**
 * Displays current transfer progress to the screen. Called from the clientcmd
 * transfer function
 *
 * params
 *	percentage completed
 *	current speed displayed
 */
void platformProgress(int perc, char *speedbuffer) {
	char tmp[128];

	drawGU();
	sprintf(tmp,"(%.2d%%) %s", perc, speedbuffer);
	sceGuStart(GU_DIRECT, list);
	//sceGuClear(GU_COLOR_BUFFER_BIT);

	initFontTexture();
	drawString(tmp,10*17,13*10,0xFFFFFFFF,0);

	sceGuFinish();
	sceGuSync(0,0);

	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
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
int platformOverwrite(char *filename, int canresume) {
	char tmp[1024];
	int ret;
	printf("asking to overwrite\n");
	if(canresume) {
		sprintf(tmp, "Resume the file %s\n",filename);
		ret = ShowMessageDialog(tmp, 1);
		if(ret == -1)
			return 3;
		else if(ret == 0)
			return 1;
		else
			return 2;
	}
	else {
		sprintf(tmp, "Overwrite the file %s\n",filename);
		ret = ShowMessageDialog(tmp, 1);
		if(ret == -1) /* back */
			return 3;
		else if(ret == 0) /* no */
			return 3;
		else
			return 1;
	}
}

/**
 * Part of the platform series of functions. This will list saved hosts for quick 
 * connections. 
 *
 * returns an address to a saved_host structure or NULL will get the user to input
 * the data using getKeyboard.
 */
saved_host *platformListHosts() {
	char tmp[128];
	int done = 0;
	int i;
	SceCtrlData pad;
	SceCtrlData lastpad;
	filelist *fl = createFilelist();
	saved_host *host;
	saved_host *hosts = getSavedHosts();
	saved_host *ret = NULL;
	addFile(fl, "       ----Help----", 0, 1);
	for(host = hosts; host != NULL; host = host->next) {
		sprintf(tmp, "%s@%s:%s",host->host[0],host->host[2],host->host[3]);
		printf("HostList Adding %s\n",tmp);
		addFile(fl, tmp, 0, 1);
	}
	if(fl->size == 0)
		return NULL;
	psp_window *w = createPSPWindow(fl, LEFT);
	w->active = 1;
	sceCtrlReadBufferPositive(&lastpad, 1);
	printf("HostList Draw loop entrance\n");
	while(!done) {

		drawGU();
		sceGuStart(GU_DIRECT, list);
		//sceGuClear(GU_COLOR_BUFFER_BIT);

		initFontTexture();
		drawPSPWindow(w,1);

		sceGuFinish();
		sceGuSync(0, 0);

		sceCtrlReadBufferPositive(&pad, 1);
		/* prevent the spaming of key strokes */
		if (pad.TimeStamp - lastpad.TimeStamp > 200000 && pad.Buttons != 0){
			if(pad.Buttons & PSP_CTRL_UP) {
				if(w->selected == 0)
					w->base = (w->base == 0 ? 0 : --w->base);
				else 
					w->selected = (w->selected == 0 ? 0 : --w->selected);
			}
			else if(pad.Buttons & PSP_CTRL_DOWN) {
				if(w->selected == 16 && w->base + w->selected + 1 < w->fl->size)
					w->base++;
				else if(w->base + w->selected + 1 < w->fl->size)
					w->selected++;
			}
			else if(pad.Buttons & PSP_CTRL_CROSS) {
				if(w->base + w->selected == 0) {
					ShowMessageDialog("In the list you are shown\nHit Cross to select a previous host\nHit Circle to enter connection details\n",0);
					continue;
				}
				else{
					w->selected--;
				}
				for(host = hosts, i = 0; i < w->base + w->selected && host != NULL; i++, host = host->next);
				if(host)
					ret = createHost(host->host[0],host->host[1],host->host[2],host->host[3]);
				done = 1;

			}
			else if(pad.Buttons & PSP_CTRL_CIRCLE) {
				ret = NULL;
				done = 1;
			}
			lastpad = pad;
		}
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}

	if(ret)
		printf("connecting to %s@%s\n", ret->host[0],ret->host[2]);

	deleteSavedHosts(hosts);
	deletePSPWindow(w);
	return ret;
}

/**
 * Do something when the server recieves a packet. Here i draw my little rotating
 * square.
 */
void platformServerUpdate() {
	drawGU();

	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

/**
 * Failed server exit callback not availible on every platform.
 * 
 * This is just a test.
 */
int platformServerCheckExit() {
	SceCtrlData pad;

	printf("Checking for Exit\n");
	sceCtrlReadBufferPositive(&pad, 1);
	if(pad.Buttons & PSP_CTRL_TRIANGLE)
		return (ShowMessageDialog("Shutdown Server?\n",1) != 1);
	else
		return 1;
}	

/**
 * Launches a file menu where users are able to choose what to do with the selected file
 *
 * returns index of the element selected.
 */
int launchMenu(psp_window *w, char *options) {
	SceCtrlData pad;
	SceCtrlData lastpad;
	int done = 0;

	w->active = 1;
	/* clear it */
	do {
		sceCtrlReadBufferPositive(&lastpad, 1);
	} while(lastpad.Buttons);
	while(!done) {
		sceGuStart(GU_DIRECT, list);
		sceGuClear(GU_COLOR_BUFFER_BIT);

		/*drawPSPClient();*/
		initFontTexture();
		drawPSPWindow(w,1);

		sceGuFinish();
		sceGuSync(0, 0);
	
		
		sceCtrlReadBufferPositive(&pad, 1);
		/* prevent the spaming of key strokes */
		if (pad.TimeStamp - lastpad.TimeStamp > 200000 && pad.Buttons != 0){
			if(pad.Buttons & PSP_CTRL_UP) {
				if(w->selected == 0)
					w->base = (w->base == 0 ? 0 : --w->base);
				else 
					w->selected = (w->selected == 0 ? 0 : --w->selected);
			}
			else if(pad.Buttons & PSP_CTRL_DOWN) {
				if(w->selected == 16 && w->base + w->selected + 1 < w->fl->size)
					w->base++;
				else if(w->base + w->selected + 1 < w->fl->size)
					w->selected++;
			}
			else if (pad.Buttons & PSP_CTRL_CIRCLE){
				return -1;
			}
			else if (pad.Buttons & PSP_CTRL_CROSS){
				return w->base + w->selected;
			}
			lastpad = pad;
		}

		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}
	return -1;
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
	char tmp[1024];
	char *tmpchar;
	int ret;
	listelm *tmplist;
	listelm *tmpfile;
	SceCtrlData pad;
	SceCtrlData lastpad;
	psp_window *w_options;
	psp_window *w_left;
	psp_window *w_right;
	psp_window *w = NULL;
	int done = 0;
	int menu;

	connection *c = clientConnect();
	if(c == NULL) {
		ShowMessageDialog("Unable to connect.\n",0);
		printf("failed to connect\n");
		return 1;
	}

	w_options = createPSPWindow(NULL,LEFT);
	w_options->fl = createFilelist();
	addFile(w_options->fl, "Transfer", 0, 1);
	addFile(w_options->fl, "Create Folder", 0, 1);
	addFile(w_options->fl, "Rename File", 0, 1);
	addFile(w_options->fl, "Delete", 0, 1);

	w_left = createPSPWindow(NULL,LEFT);
	w_right = createPSPWindow(NULL,RIGHT);
	w_left->fl = getLocalList(c);
	w_right->fl = getRemoteList(c);
	w = w_right;
	w->active = 1;

	sceCtrlReadBufferPositive(&lastpad, 1);
	while(!done) {
		menu = -1;

		sceGuStart(GU_DIRECT, list);
		{
			sceGuClearColor(0xff554433);
			sceGuClear(GU_COLOR_BUFFER_BIT);
		}
		sceGuFinish();
		sceGuSync(0,0);
		drawPSPClient();
		sceGuStart(GU_DIRECT, list);

		initFontTexture();
		drawPSPWindow(w_left,0);
		if(w_right->fl == NULL) {
			/* lost connection */
			sceGuFinish();
			sceGuSync(0, 0);
			return 1;
		}
		else {
			drawPSPWindow(w_right,0);
		}

		sceGuFinish();
		sceGuSync(0, 0);



		sceCtrlReadBufferPositive(&pad, 1);
		/* prevent the spaming of key strokes */
		if (pad.TimeStamp - lastpad.TimeStamp > 200000 && pad.Buttons != 0){
			if(pad.Buttons & PSP_CTRL_UP) {
				if(w->selected == 0)
					w->base = (w->base == 0 ? 0 : --w->base);
				else 
					w->selected = (w->selected == 0 ? 0 : --w->selected);
			}
			else if(pad.Buttons & PSP_CTRL_DOWN) {
				if(w->selected == 16 && w->base + w->selected + 1 < w->fl->size)
					w->base++;
				else if(w->base + w->selected + 1 < w->fl->size)
					w->selected++;
			}
			else if(pad.Buttons & PSP_CTRL_LEFT) {
				w_left->active = 1;
				w_right->active = 0;
				w = w_left;
			}
			else if(pad.Buttons & PSP_CTRL_RIGHT) {
				w_left->active = 0;
				w_right->active = 1;
				w = w_right;
			}
			else if (pad.Buttons & PSP_CTRL_CROSS) {
				tmplist = getListAt(w->fl, w->selected + w->base);
				if(tmplist) {
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
						w->selected = 0;
						w->base = 0;
						tmplist = NULL;
						menu = -1;
					}
					else {
						printf("launching menu\n");
						menu = launchMenu(w_options, tmplist->filename);
						printf("menu item: %d\n",menu);
						w_options->selected = 0;
					}
				}
				else {
					continue;
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
						sprintf(tmp, "Create folder '%s'?\n", tmpchar);
						if(strlen(tmpchar)){
							ret = ShowMessageDialog(tmp, 1);
							if(ret == 1) {
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
						}
						free(tmpchar);
					}
					if(menu == 2) { /* rename file */
						tmpchar = copyString(getKeyboard(128,4,"new name: ", tmplist->filename));
						if(strlen(tmpchar)){
							sprintf(tmp, "Rename '%s' to '%s'?\n",tmplist->filename, tmpchar);
							ret = ShowMessageDialog(tmp, 1);
							if(ret == 1) {
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
						}
						free(tmpchar);
					}
					else if(menu == 3) { /* delete file */
						sprintf(tmp, "Delete '%s'?\n",tmplist->filename);
						ret = ShowMessageDialog(tmp, 1);
						if(ret == 1) {
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
				}
				sceKernelDelayThread(100*100);
			}
			else if (pad.Buttons & PSP_CTRL_TRIANGLE) {
				ret = ShowMessageDialog("Quit connection?\n", 1);
				if(ret == 1) {
					clientQuit(c);
					done = 1;
				}
			}
			lastpad = pad;
		}
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();

	}
	deletePSPWindow(w_left);
	deletePSPWindow(w_right);
	return 1;
}

/**
 * Initalize and call either server or client
 */
int main(int argc, char **argv) {
	int ret;
	netData data;
	char tmp[128];
#if ( _PSP_FW_VERSION < 200 )
	SceUID thid;
	int err;
#endif
	char my_ip[32] = "000.000.000.000";
	SceCtrlData pad;
	struct sockaddr_in my_addr;  /* my server address info */
	int server_sock;
	int paddone = 0;

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	pspDebugScreenInit();

	pspDebugScreenPrintf("Starting up\n");

	SetupCallbacks();

	setupGu();
	drawGU();

	sceCtrlReadBufferPositive(&pad, 1);
	if( (pad.Buttons & PSP_CTRL_LTRIGGER) && (pad.Buttons & PSP_CTRL_RTRIGGER) ) {
		unlink("hosts.cfg");
	}
	/* this is for gameshared edition */
	if(argc > 1) {
		if(strcmp(argv[2], "GameShar") == 0) {
			adhoc = 2; /* two means client */
			sceGuTerm();
			pspDebugScreenInit();
			pspDebugScreenPrintf("testing chdir\n");
			sceKernelDelayThread(100*10000);
			pspDebugScreenPrintf("chdir returns %d\n", chdir("/"));
			sceKernelDelayThread(100*10000);
			pspDebugScreenPrintf("errno %d %s\n",errno, strerror(errno));
			setupGu();
#ifdef PSP_ADHOC
			pspDebugScreenPrintf("--Adhoc Negotiations--\n");

			asciiToHex(argv[1],adhoc_mac);
			setupAdhoc("FTPServe");

			pspDebugScreenPrintf("Adhoc Pairing with %s.\n",argv[1]);

			sceWlanGetEtherAddr(mymac);
			sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME, mynick, 32);
			/* master is listening on 5000 udp for a ssid to connect to */
			while(adhoc_state != PSP_ADHOC_MATCHING_EVENT_COMPLETE) {
				if(adhoc_state != prevstate) {
					pspDebugScreenPrintf("adhoc matching state is now %d\n", adhoc_state);
					prevstate = adhoc_state;
				}
			}
#endif
		}
	}

#ifdef GAMESHARE
	if(adhoc == 0) {
		ret = ShowMessageDialog("Gameshare the executable?\n", 1);

		if(ret == 1) {
			adhoc = 1;
			printf("--Gamesharing--\n");
			setupAdhoc("GameShar");
			printf("Gamesharing Adhoc setup.\n");
			gsDialog();
			printf("--Gamesharing Completed--\n");

			adhocTerm();
#ifdef PSP_ADHOC
			printf("--Adhoc--\n");
			setupAdhoc("FTPServe");
			printf("--Adhoc Completed--\n");
			sceWlanGetEtherAddr(mymac);
			while(adhoc_state != PSP_ADHOC_MATCHING_EVENT_COMPLETE) {
				if(adhoc_state != prevstate) {
					printf("adhoc matching state is now %d\n", adhoc_state);
					prevstate = adhoc_state;
				}
			}
			//printf("Listen %d\n", sceNetAdhocPtpListen(mymac, 21, 8192, 60, 5, 2, 0));
#endif
		}
	}
#endif
	setupINet();
	/* find out what they want to do */
	data.asUint = 0xBADF00D;
	memset(&data.asString[4], 0, 124);
	sceUtilityGetNetParam(1, PSP_NETPARAM_NAME, &data);
	sprintf(tmp, "Use '%s'?\n",data.asString);
	ret = ShowMessageDialog(tmp, 1);
	if(ret == 1)
		paddone = 2;
	else if(ret == 0)
		paddone = 1;
	else {
#if ( _PSP_FW_VERSION < 200)
		sceKernelExitDeleteThread(0);
#else
		sceKernelExitGame();
#endif
	}

	/* connect by dialog */
	if(paddone == 1) {
		netDialog();
	}
	else if(paddone == 2) { /* autoconnect */
		printf("autoconnecting");
		if(connectToAp(1) == 0) {
#if ( _PSP_FW_VERSION < 200)
			sceKernelExitDeleteThread(0);
#endif
			return 0;
		}
	}

	sceNetApctlGetInfo(8, my_ip);
	if(!adhoc && strcmp(my_ip,"000.000.000.000") == 0){
		ShowMessageDialog("Failed to connect to AP.", 0);
		sceKernelDelayThread(1000*1000); // 50ms
#if ( _PSP_FW_VERSION < 200)
		sceKernelExitDeleteThread(0);
#else
		sceKernelExitGame();
#endif
	}

	while(1) {
		if(adhoc == 0) {
			ret = -1;
			while(ret == -1) {
				ret = ShowMessageDialog("Launch the server?\n", 1);
				if(ret == 1) {
					ret = 1;
					break;
				}
				else
					ret = -1;

				ret = ShowMessageDialog("Launch the client?\n", 1);
				if(ret == 1) {
					ret = 0;
					break;
				}
				else
					ret = -1;
			}
		}
		else{
			ret = adhoc - 2;
		}
		/* if true we want server */
		if(ret) {
			connection *master = initMasterListen(&my_addr, SERVER_PORT);
			server_sock = master->sock;
			sscanf(my_ip,"%d.%d.%d.%d",&master->ip[0],&master->ip[1],&master->ip[2],&master->ip[3]);
			char buffer[50];
			sprintf(buffer,"listening on %s:%d",my_ip,SERVER_PORT);

			drawGU();
			sceGuStart(GU_DIRECT, list);
			//sceGuClear(GU_COLOR_BUFFER_BIT);
			initFontTexture();

			drawString(buffer,0,16,0xFFFFFFFF,0);

			sceGuFinish();
			sceGuSync(0, 0);
			sceDisplayWaitVblankStart();
			sceGuSwapBuffers();
#if ( _PSP_FW_VERSION >= 200 )
			server_loop((void *)&server_sock);
#else
			/* Create a user thread to do the real work */
			thid = sceKernelCreateThread("server_loop", server_loop, 0x18, 0x10000, PSP_THREAD_ATTR_USER, (void *)&server_sock);
			if(thid < 0) {
				printf("Error, could not create thread\n");
				sceKernelSleepThread();
			}
			sceKernelStartThread(thid, 0, NULL);
			sceKernelWaitThreadEnd(thid, 0);
#endif
		}
		else {
			/* we are doing the client */
			client();
		}
	}
	sceKernelExitGame();
	return 0;
}

#if ( _PSP_FW_VERSION >= 200 )
int module_stop(SceSize args, void *argp) {
	INetTerm();
	return 0;
}
#endif
