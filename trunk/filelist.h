#ifndef __FILELIST_H__
#define __FILELIST_H__

typedef enum {LEFT = 0, RIGHT} location;

typedef struct{
	char *filename;
	int filesize;
	int isDir;
}listelm;

typedef struct{
	int size;
	int max_size;
	listelm **data;
}filelist;

#ifdef __PSP__
typedef struct{
	filelist *fl;
	location loc;
	int base;
	int selected;
	int active;
}psp_window;

psp_window *createPSPWindow(filelist *fl, location l);

void deletePSPWindow(psp_window *w);
#endif

#ifdef __DS__
#include <nds/jtypes.h>
typedef struct{
	u16 *map;
	filelist *fl;
	int top;
	int base;
	int speed;
}ds_window;

ds_window *createDSWindow(u16 *map, filelist *fl);

void deleteDSWindow(ds_window *w);
#endif

void sortFilelist(filelist *fl);

filelist *createFilelist();

listelm *findFile(filelist *fl, char *f);
listelm *getListAt(filelist *fl, int pos);

void deleteFilelist(filelist *fl);

void addFile(filelist *fl, char *filename, int size, int isDir);

void printFilelist(filelist *fl);

#endif

