#include <string.h>
#include <stdlib.h>
#include "filelist.h"
#include "ftpcmd.h"

#ifdef __DS__
#include "../../ds_wifi.h"
#elif __PSP__
#else
#endif

/**
 * Define a couple create and distroy functions for internal listing structures
 */
#ifdef __PSP__
psp_window *createPSPWindow(filelist *fl, location loc) {
	psp_window *w;
	if( (w = malloc(sizeof(psp_window))) == NULL)
		return NULL;

	w->fl = fl;
	w->loc = loc;
	w->active = 0;
	w->base = 0;
	w->selected = 0;
	return w;
}

void deletePSPWindow(psp_window *w) {
	deleteFilelist(w->fl);
	free(w);
}

#endif

#ifdef __DS__
ds_window *createDSWindow(u16 *map, filelist *fl) {
	ds_window *w;
	if( (w = malloc(sizeof(ds_window))) == NULL)
		return NULL;

	w->map = map;
	w->fl = fl;
	w->top = 0;
	w->base = 0;
	w->speed = 0;

	return w;
}

void deleteDSWindow(ds_window *w) {
	deleteFilelist(w->fl);
	free(w);
}
#endif

/**
 * Sort listings since servers dont have to sort it when they send
 */
int list_cmp(const void *va, const void *vb) {
	listelm *a = *(listelm**)va;
	listelm *b = *(listelm**)vb;
	if(strcmp(a->filename,"..") == 0)
		return -1;
	else if(strcmp(b->filename,"..") == 0)
	   return 1;
	else if(a->isDir && !b->isDir)
		return -1;
	else if(!a->isDir && b->isDir)
		return 1;
	else 
		return strcmp(a->filename,b->filename);
}

void sortFilelist(filelist *fl) {
	if(fl)
		qsort(fl->data, fl->size, sizeof(listelm *),list_cmp);
}

filelist *createFilelist() {
	filelist *p;
    if( (p = malloc(sizeof(filelist))) == NULL)
		return NULL;
	p->size = 0;
	if( (p->data = malloc(sizeof(listelm*) * 64)) == NULL) {
		free(p);
		return NULL;
	}
	p->max_size = 64;
	return p;
}

void deleteFilelist(filelist *fl) {
	int i;
	if(fl == NULL)
		return;
	for(i = 0; i < fl->size; i++) {
		free(fl->data[i]->filename);
		free(fl->data[i]);
	}
	free(fl->data);
	free(fl);
}

listelm *findFile(filelist *fl, char *filename) {
	int i;
	for(i = 0; i <fl->size; i++) {
		if(strcmp(filename, fl->data[i]->filename) == 0)
			return fl->data[i];
	}
	return NULL;
}

void addFile(filelist *fl, char *filename, int size, int isdir) {
	listelm *tmp;
	if(strcmp(filename, ".") == 0) /*no . in here */
		return;
	if(fl->size + 1 == fl->max_size) {
		if( (fl->data = realloc(fl->data, sizeof(listelm*) * fl->max_size * 2)) == NULL)
			return;
		fl->max_size *= 2;
	}
	if( (fl->data[fl->size++] = malloc(sizeof(listelm))) == NULL)
		return;
	tmp = fl->data[fl->size-1];
	tmp->filename = copyString(filename);
	tmp->filesize = size;
	tmp->isDir = isdir;
}

listelm *getListAt(filelist *fl, int pos) {
	if(pos > fl->size)
		return NULL;
	listelm *l = fl->data[pos];
	return l;
}

void printFilelist(filelist *fl) {
	int i;
	listelm *l;
	if(fl == NULL)
		return;
	for(i = 0; i < fl->size; i++) {
		l = fl->data[i];
		if(l->isDir)
			printf("%s\n",l->filename);
		else
			printf("%s (%d)\n",l->filename, l->filesize);
	}
}

