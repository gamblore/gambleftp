#ifndef __PSP_WIFI_H__
#define __PSP_WIFI_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <pspctrl.h>
#include <pspwlan.h>
#include <psputility.h>
#include <psputility_netparam.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspimpose_driver.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <pspnet_adhocmatching.h>
#include <pspnet_resolver.h>
#include "psp_util.h"

#define MODULE_NAME "FTPServer"
//#define gu
//#define printf pspDebugScreenPrintf

#define SERVER_PORT 21

#if (_PSP_FW_VERSION >= 200)
extern int server_loop(void *argp);
#else
extern int server_loop(SceSize args, void *argp);
#endif
extern int init_server(struct sockaddr_in *my_addr, int port);

extern int running;

void platformServerUpdate();
int platformServerCheckExit();
int main(int argc, char **argv);

#endif

