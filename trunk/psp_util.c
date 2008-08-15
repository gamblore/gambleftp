#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include "psp_wifi.h"
#include "psp_text.h"
#include "psp_util.h"

char keyboardText[1024];

void drawGU();

/* Exit callback */
int exit_callback(int arg1, int arg2, void *common) {
	int status;
	SceUID callbackid;
	SceKernelCallbackInfo cbInfo;
	pspSdkReferCallbackStatusByName("Exit Callback", &callbackid, &cbInfo);
	printf("callback exec id: %d\n", callbackid);
	if( (status = sceUtilityNetconfGetStatus()) > 0) {
		printf("Shutting down NetConf Utility\n");
		sceUtilityNetconfShutdownStart();
		while( (status = sceUtilityNetconfGetStatus()) != 0) {
			printf("  Status %d\n", status);
			sceKernelDelayThread(100*100);
		}
	}
	if( (status = sceUtilityOskGetStatus()) > 0) {
		printf("Shutting down OSK Utility\n");
		sceUtilityOskShutdownStart();
		while( (status = sceUtilityOskGetStatus()) != 0) {
			printf("  Status %d\n", status);
			sceKernelDelayThread(100*100);
		}
	}
	if( (status = sceUtilityGameSharingGetStatus()) > 0) {
		printf("Shutting down GameSharing Utility\n");
		sceUtilityGameSharingShutdownStart();
		while( (status = sceUtilityGameSharingGetStatus()) != 0) {
			printf("  Status %d\n", status);
			sceKernelDelayThread(100*100);
		}
	}
	if( (status = sceUtilityMsgDialogGetStatus()) > 0) {
		printf("Shutting down MsgDialog Utility\n");
		sceUtilityMsgDialogAbort();
		sceUtilityMsgDialogShutdownStart();
		while( (status = sceUtilityMsgDialogGetStatus()) != 0) {
			printf("  Status %d\n", status);
			sceKernelDelayThread(100*100);
		}	
	}
	printf("status %d Now Exiting...\n", status);
#if (_PSP_FW_VERSION >= 200) 
	running = 0;
	sceKernelExitGame();
#else
	sceKernelExitGame();
#endif

	return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp) {
	int cbid;
	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	printf("callback created id: %d\n",cbid);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void) {
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, 0);
	if(thid >= 0)
		sceKernelStartThread(thid, 0, 0);

	printf("callback creating thread %d\n",thid);

	return thid;
}

void DeleteCallbacks() {
	SceUID callbackid;
	SceKernelCallbackInfo cbInfo;
	pspSdkReferCallbackStatusByName("Exit Callback", &callbackid, &cbInfo);
	sceKernelDeleteCallback(callbackid);
	printf("callback deleted id: %d\n",callbackid);
	sceKernelTerminateDeleteThread(cbInfo.threadId);
}

/**
 *  All sceUtilities except MsgDialog don't like it when you exit im them.
 */
void ExitCallbackEnable() {
	sceImposeSetHomePopup(1);
}
void ExitCallbackDisable() {
	sceImposeSetHomePopup(0);
}

/**
 * Keyboard helper functions. 
 *
 * Since the psp exists supports many languages text input is in unicode.
 * Two string casting functions are required for getKeyboard.
 */
void asciiToUnicode(char *from, unsigned short *to) {
	int c;
	while( (c = *from) ) {
		*to = (unsigned short)c;
		from++;
		to++;
	}
	*to = '\0';
}

void unicodeToAscii(unsigned short *from, char *to) {
	int c;
	while( (c = *from) ) {
		*to = (char)c;
		to++;
		from++;
	}
	*to = '\0';
}

/**
 * Discribed in platform.h is one function that is required to use the client.
 */
char *getKeyboard(int max, int column, char *pretext, char *current) {
	SceUtilityOskData data;
	SceUtilityOskParams params;
	unsigned short in[1024];
	unsigned short out[1024];
	unsigned short desc[128];
	int done = 0;
	
	if(pretext)
		asciiToUnicode(pretext,desc);
	else
		*desc = '\0';
	if(current)
		asciiToUnicode(current,in);
	else
		*in = '\0';
	*out = '\0';

	memset(&data, 0, sizeof(SceUtilityOskData));
	data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT; // Use system default for text input
	data.lines = 1;
	data.unk_24 = 1;
	if(strcmp(pretext, "port: ") == 0) 
		data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_LATIN_DIGIT;
	else 
		data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL; // Allow all input types
	data.desc = desc;
	data.intext = in;
	data.outtextlength = 1024;
	data.outtextlimit = max; // Limit input to 32 characters
	data.outtext = out;

	memset(&params, 0, sizeof(params));
	params.base.size = sizeof(params);
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &params.base.language);
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &params.base.buttonSwap);
	params.base.graphicsThread = 17;
	params.base.accessThread = 19;
	params.base.fontThread = 18;
	params.base.soundThread = 16;
	params.datacount = 1;
	params.data = &data;

	ExitCallbackDisable();
	printf("starting OSK\n");
	sceUtilityOskInitStart(&params);

	while(!done) {
		drawGU();
		switch(sceUtilityOskGetStatus()) {
			case PSP_UTILITY_DIALOG_INIT:
				break;

			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityOskUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityOskShutdownStart();
				break;

			case PSP_UTILITY_DIALOG_FINISHED:
				break;

			case PSP_UTILITY_DIALOG_NONE:
				done = 1;
				break;

			default :
				break;
		}

		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}
	ExitCallbackEnable();
	printf("exiting OSK\n");
	unicodeToAscii(out, keyboardText);
	return keyboardText;
}

/**
 * Setup a Utility paramater structure.
 */
void ConfigureDialog(pspUtilityMsgDialogParams *dialog, size_t dialog_size) {
	memset(dialog, 0, dialog_size);

	dialog->base.size = dialog_size;
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,
			&dialog->base.language); // Prompt language
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN,
			&dialog->base.buttonSwap); // X/O button swap

	dialog->base.graphicsThread = 0x11;
	dialog->base.accessThread = 0x13;
	dialog->base.fontThread = 0x12;
	dialog->base.soundThread = 0x10;
}

/**
 * Displays a string using the nice PSP Utility functions and can even 
 * request user input in yes, no or cancel style.
 *
 * returns result
 *		1 Yes selected
 *		0 No selected
 *		-1 User canceled
 */
int ShowMessageDialog(const char *message, int enableYesno) {
	pspUtilityMsgDialogParams dialog;
	int done = 0;

	ConfigureDialog(&dialog, sizeof(dialog));
	dialog.mode = PSP_UTILITY_MSGDIALOG_MODE_TEXT;
	dialog.options = PSP_UTILITY_MSGDIALOG_OPTION_TEXT;

	if(enableYesno)
		dialog.options |= PSP_UTILITY_MSGDIALOG_OPTION_YESNO_BUTTONS|PSP_UTILITY_MSGDIALOG_OPTION_DEFAULT_NO;

	strcpy(dialog.message, message);

	sceUtilityMsgDialogInitStart(&dialog);

	while(!done) {
		drawGU();
		switch(sceUtilityMsgDialogGetStatus()) {

			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityMsgDialogUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityMsgDialogShutdownStart();
				break;
			case PSP_UTILITY_DIALOG_NONE:
				done = 1;
		}
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}
	if(enableYesno) {
		if(dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_YES)
			return 1;
		else if(dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_NO)
			return 0;
		else
			return -1;
	}
	else 
		return 1;
}

/* Connect to an access point */
int connectToAp(int config) {
	int err;
	int state;
	int stateLast = -1;

	ExitCallbackDisable();
	/* Connect using the first profile */
	err = sceNetApctlConnect(config);
	if (err != 0) {
		printf(MODULE_NAME ": sceNetApctlConnect returns %08X\n", err);
		return 0;
	}

	printf(MODULE_NAME ": Connecting...\n");
	while (1) {
		drawGU();
		err = sceNetApctlGetState(&state);
		if (err != 0)
		{
			printf(MODULE_NAME ": sceNetApctlGetState returns $%x\n", err);
			break;
		}
		if (state != stateLast)
		{
			if (state == 6)
				printf("  starting Wpa Supplicant\n");
			else
				printf("  connection state %d of 4\n", state);
			stateLast = state;
		}
		if (state == 4)
			break;  // connected with static IP

		// wait a little before polling again
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
		sceKernelDelayThread(1*100); // .1ms
	}
	printf(MODULE_NAME ": Connected!\n");

	ExitCallbackEnable();
	if(err != 0) {
		return 0;
	}

	return 1;
}

/**
 * Connects as 10.0.1.1 if master .2 if client to ap ssid
 *
 */
int manualNetConf(int master,char *ssid) {
	int yes = 1;
	int no = 0;
	sceUtilitySetNetParam(PSP_NETPARAM_NAME, "FTPServer");
	sceUtilitySetNetParam(PSP_NETPARAM_SSID, "ssid");
	if(master) {
		sceUtilitySetNetParam(PSP_NETPARAM_IP, "10.0.1.1");
		sceUtilitySetNetParam(PSP_NETPARAM_PRIMARYDNS, "10.0.1.1");
	}
	else {
		sceUtilitySetNetParam(PSP_NETPARAM_IP, "10.0.1.2");
		sceUtilitySetNetParam(PSP_NETPARAM_PRIMARYDNS, "10.0.1.2");
	}
	sceUtilitySetNetParam(PSP_NETPARAM_SECURE, &no);
	sceUtilitySetNetParam(PSP_NETPARAM_WEPKEY, "");
	sceUtilitySetNetParam(PSP_NETPARAM_IS_STATIC_IP, &yes);
	sceUtilitySetNetParam(PSP_NETPARAM_NETMASK, "255.255.255.0");
	sceUtilitySetNetParam(PSP_NETPARAM_ROUTE, "10.0.0.0");
	sceUtilitySetNetParam(PSP_NETPARAM_MANUAL_DNS, &yes);
	sceUtilitySetNetParam(PSP_NETPARAM_PRIMARYDNS, "0.0.0.0");
	sceUtilitySetNetParam(PSP_NETPARAM_PROXY_USER, "");
	sceUtilitySetNetParam(PSP_NETPARAM_PROXY_PASS, "");
	sceUtilitySetNetParam(PSP_NETPARAM_USE_PROXY, &no);
	sceUtilitySetNetParam(PSP_NETPARAM_PROXY_SERVER, "0.0.0.0");
	sceUtilitySetNetParam(PSP_NETPARAM_PROXY_PORT, &yes);
	sceUtilitySetNetParam(PSP_NETPARAM_UNKNOWN1, &no);
	return connectToAp(0);
}

/**
 * Inialization functions for infrastructure mode 
 */
void netInit(void) {
	sceNetInit(128*1024, 42, 4*1024, 42, 4*1024);
	sceNetInetInit();
	sceNetApctlInit(0x8000, 48);
}

void INetTerm(void) {
	sceNetApctlTerm();
	sceNetInetTerm();
	sceNetTerm();
}

/**
 * Launch the gameshare Utility. Sends this executable to another PSP. Requires
 * Adhoc to be initalized first.
 */
int gsDialog() {
	int done = 0;

	sceNetAdhocMatchingInit(32*1024);

	pspUtilityGameSharingParams params;

	memset(&params, 0, sizeof(params));
	params.base.size = sizeof(params);
	params.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
	params.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
	params.base.graphicsThread = 17;
	params.base.accessThread = 19;
	params.base.fontThread = 18;
	params.base.soundThread = 16;

	int fd = sceIoOpen("EBOOT.PBP", PSP_O_RDONLY, 0777);
	params.datasize = sceIoLseek32(fd, 0, PSP_SEEK_END);
	unsigned char *fileBuffer = memalign(64, params.datasize);
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, fileBuffer, params.datasize);
	sceIoClose(fd);

	/* Manually patch the PARAM.SFO in the EBOOT */
	fileBuffer[276] = 0x57;
	/* And add a custom filename */
	strncpy((char *) &fileBuffer[320], "PSPFTP Shared", 127);

	memcpy(&params.name, "GameShar", 8);

	params.mode = PSP_UTILITY_GAMESHARING_MODE_MULTIPLE;
	params.datatype = PSP_UTILITY_GAMESHARING_DATA_TYPE_MEMORY;

	params.data = fileBuffer;

	ExitCallbackDisable();
	sceUtilityGameSharingInitStart(&params);

	while(!done) {
		drawGU();

		switch(sceUtilityGameSharingGetStatus()) {
			case PSP_UTILITY_DIALOG_NONE:
				sceNetAdhocMatchingTerm();
				done = 1;
				break;

			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityGameSharingUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityGameSharingShutdownStart();
				break;

			case PSP_UTILITY_DIALOG_FINISHED:
				break;

			default:
				break;
		}
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}
	free(fileBuffer);
	ExitCallbackEnable();
	return 1;
}

/**
 * Setup and shutdown functions for Adhoc systems
 */
void adhocTerm(void) {
	sceNetAdhocctlDisconnect();
	sceNetAdhocctlTerm();
	sceNetAdhocTerm();
	sceNetTerm();
	sceUtilityUnloadNetModule(PSP_NET_MODULE_ADHOC);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

void adhocInit(void) {
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_ADHOC);
	sceNetInit(128*1024, 42, 4*1024, 42, 4*1024);
	sceNetAdhocInit();

	struct productStruct gameProduct;
	gameProduct.unknown = 2;
	memcpy(gameProduct.product, "000000001", 9);
	sceNetAdhocctlInit(32*1024, 0x20, &gameProduct);
}

/**
 * Launchs the adhoc helper.
 */
int adhocDialog(char *adhoc_name) {
	int done = 0;

	pspUtilityNetconfData data;

	memset(&data, 0, sizeof(data));
	data.base.size = sizeof(data);
	data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
	data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
	data.base.graphicsThread = 17;
	data.base.accessThread = 19;
	data.base.fontThread = 18;
	data.base.soundThread = 16;
	data.action = PSP_NETCONF_ACTION_CONNECT_ADHOC;

	struct pspUtilityNetconfAdhoc adhocparam;
	memset(&adhocparam, 0, sizeof(adhocparam));
	memcpy(&adhocparam.name, adhoc_name, 8);
	adhocparam.timeout = 60;
	data.adhocparam = &adhocparam;

	ExitCallbackDisable();
	sceUtilityNetconfInitStart(&data);

	while(!done) {
		drawGU();
		switch(sceUtilityNetconfGetStatus()) {
			case PSP_UTILITY_DIALOG_NONE:
				done = 1;
				break;

			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityNetconfUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityNetconfShutdownStart();
				break;

			case PSP_UTILITY_DIALOG_FINISHED:
				done = 1;
				break;

			default:
				break;
		}
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}
	ExitCallbackEnable();
	return 1;
}

/**
 * Use the OS network connect dialog to connect to an AP.
 * EXIT CALLBACK WILL BE DISABLED AS YOU CAN NOT CANCEL THIS REMOTELY
 */
int netDialog() {
	int done = 0;
	pspUtilityNetconfData data;

	memset(&data, 0, sizeof(data));
	data.base.size = sizeof(data);
	data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
	data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
	data.base.graphicsThread = 17;
	data.base.accessThread = 19;
	data.base.fontThread = 18;
	data.base.soundThread = 16;
	data.action = PSP_NETCONF_ACTION_CONNECTAP;

	struct pspUtilityNetconfAdhoc adhocparam;
	memset(&adhocparam, 0, sizeof(adhocparam));
	data.adhocparam = &adhocparam;

	ExitCallbackDisable();
	sceUtilityNetconfInitStart(&data);

	while(!done) {
		drawGU();

		switch(sceUtilityNetconfGetStatus()) {
			case PSP_UTILITY_DIALOG_NONE:
				done = 1;
				break;

			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityNetconfUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityNetconfShutdownStart();
				break;

			case PSP_UTILITY_DIALOG_FINISHED:
				done = 1;
				break;

			default:
				break;
		}
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}
	ExitCallbackEnable();
	return 1;
}

#ifdef PSP_ADHOC
unsigned char *adhocGetMacs() {
	int i = 0;
	unsigned char *macs;
	int length;
	struct SceNetAdhocctlPeerInfo *this2, *ptr2;

	sceNetAdhocctlScan();
	sceNetAdhocctlGetPeerList(&length, NULL);
	this2 = malloc(length);
	if(this2 == NULL)
		return NULL;
	macs = malloc(length*6);
	if(macs == NULL) {
		free(this2);
		return NULL;
	}
	i = sceNetAdhocctlGetPeerList(&length, this2);
	printf("--scan Peers %d--\n",i);
	for(ptr2 = this2; ptr2 != NULL; ptr2 = ptr2->next){
		printf("nickname: %.128s mac: %x:%x:%x:%x:%x:%x\n",ptr2->nickname, ptr2->mac[0], ptr2->mac[1], ptr2->mac[2], ptr2->mac[3], ptr2->mac[4], ptr2->mac[5]);
		memcpy(macs+(i++)*6,ptr2->mac,6);
		sceNetAdhocctlGetNameByAddr(ptr2->mac, adhoc_nick);
	}
	free(this2);
	free(macs);
	return NULL;
}
#endif

#ifdef PSP_ADHOC
int joined = 0;
int matchingId = -1;
int adhoc_state = -1;
void matchingCallback(int unk1, int event, unsigned char *mac, int optLen, void *optData) {
	// This is called when there is an event, dont seem to be able to call
	// procs from here otherwise it causes a crash

	if(event==PSP_ADHOC_MATCHING_EVENT_JOIN) {
		printf("Match Connected\n");
		joined = 1;
		memcpy(adhoc_mac,mac,6);
		if(matchingId > -1)
			sceNetAdhocMatchingSelectTarget(matchingId, adhoc_mac, 0, 0);
	}
	else if(event==PSP_ADHOC_MATCHING_EVENT_DISCONNECT) {
		printf("Match Disconnected\n");
		joined = 0;
		memset(adhoc_mac, 0, 6);
	}
	adhoc_state = event;
}
#endif

/**
 * Setups adhoc and joints a network name
 * Gamesharing uses "GameShar"
 *
 * You can see my failed adhoc attempts.
 */
void setupAdhoc(char *adhoc_name) {
	adhocInit();
	adhocDialog(adhoc_name);
#ifdef PSP_ADHOC
	int err;
	if(adhoc == 2) {
		sceGuDisplay(GU_FALSE);
		sceGuTerm();

		pspDebugScreenInit();
	}
	if(strcmp(adhoc_name,"FTPServe") == 0) {
		sceNetAdhocMatchingInit(0x20000);
		matchingId = sceNetAdhocMatchingCreate( PSP_ADHOC_MATCHING_MODE_PTP, 2, 0x22b,
				0x800, 0x2dc6c0, 0x5b8d80, 3, 0x7a120, matchingCallback);
		pspDebugScreenPrintf("matchingId %d\n",matchingId);
		err = sceNetAdhocMatchingStart(matchingId, 0x10, 0x2000, 0x10, 0x2000, 5+1, "hello");
		pspDebugScreenPrintf("matching start: %d\n",err);
		if(adhoc == 2) {
			err = sceNetAdhocMatchingSelectTarget(matchingId, adhoc_mac, 0, 0);
			pspDebugScreenPrintf("select target: %d\n",err);
		}
	}
#endif
}

/**
 * Loads INet modules
 */
void setupINet() {
#if ( _PSP_FW_VERSION < 200 )
		if(pspSdkLoadInetModules() < 0) {
			printf("Error, could not load inet modules\n");
			sceKernelSleepThread();
		}
		if((err = pspSdkInetInit())) {
			printf(MODULE_NAME ": Error, could not initialise the network %08X\n", err);
			return 0;
		}
#else
		sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
		sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
		netInit();
#endif
	}

#ifdef PSP_ADHOC
void asciiToHex(char *from, unsigned char *to) {
	int i;
	unsigned char tmp;
	for(i = 0; i < 12; i += 2) {
		tmp = from[i] - 48;
		if(tmp > 9) 
			tmp = from[i] - 65 + 10;
		if(tmp > 15) 
			tmp = from[i] - 97 + 10;
		to[i/2] = tmp << 4;
		tmp = from[i+1] - 48;
		if(tmp > 9)
			tmp = from[i+1] - 65 + 10;
		if(tmp > 15)
			tmp = from[i+1] - 97 + 10;
		to[i/2] |= tmp;
	}
}
#endif

