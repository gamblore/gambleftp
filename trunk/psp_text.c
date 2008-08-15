
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>

#include <string.h>
#include <stdio.h>

#include "font.c"
#include "psp_text.h"
#include "filelist.h"

static int fontwidthtab[128] = {
	10, 10, 10, 10,
	10, 10, 10, 10,
	10, 10, 10, 10,
	10, 10, 10, 10,

	10, 10, 10, 10,
	10, 10, 10, 10,
	10, 10, 10, 10,
	10, 10, 10, 10,

	10,  6,  8, 10, //   ! " #
	10, 10, 10,  6, // $ % & '
	10, 10, 10, 10, // ( ) * +
	6, 10,  6, 10, // , - . /

	10, 10, 10, 10, // 0 1 2 3
	10, 10, 10, 10, // 6 5 8 7
	10, 10,  6,  6, // 10 9 : ;
	10, 10, 10, 10, // < = > ?

	16, 10, 10, 10, // @ A B C
	10, 10, 10, 10, // D E F G
	10,  6,  8, 10, // H I J K
	8, 10, 10, 10, // L M N O

	10, 10, 10, 10, // P Q R S
	10, 10, 10, 12, // T U V W
	10, 10, 10, 10, // X Y Z [
	10, 10,  8, 10, // \ ] ^ _

	6,  8,  8,  8, // ` a b c
	8,  8,  6,  8, // d e f g
	8,  6,  6,  8, // h i j k
	6, 10,  8,  8, // l m n o

	8,  8,  8,  8, // p q r s
	8,  8,  8, 12, // t u v w
	8,  8,  8, 10, // x y z {
	8, 10,  8, 12  // | } ~
};

void initFontTexture() {
	/* enable drawString */
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	sceGuTexImage(0, 256, 128, 256, font);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuTexEnvColor(0x0);
	sceGuTexOffset(0.0f, 0.0f);
	sceGuTexScale(1.0f / 256.0f, 1.0f / 128.0f);
	sceGuTexWrap(GU_REPEAT, GU_REPEAT);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	/* end enable drawString */
}

/**
 *     This function draws a string on the screen 16 pixels high
 *     The chars are handled as sprites.
 *     It supportes colors and blending.
 *     The fontwidth can be selected with the parameter fw, if it is 0 the best width for each char is selected.
 */
void drawString(const char* text, int x, int y, unsigned int color, int fw) {
	int len = (int)strlen(text);
	if(!len) {
		return;
	}

	typedef struct {
		float s, t;
		unsigned int c;
		float x, y, z;
	} VERT;

	VERT* v = sceGuGetMemory(sizeof(VERT) * 2 * len);

	int i;
	for(i = 0; i < len; i++) {
		unsigned char c = (unsigned char)text[i];
		if(c < 32) {
			c = 0;
		} else if(c >= 128) {
			c = 0;
		}

		int tx = (c & 0x0F) << 4;
		int ty = (c & 0xF0);

		VERT* v0 = &v[i*2+0];
		VERT* v1 = &v[i*2+1];


		v0->s = (float)(tx + (fw ? ((16 - fw) >> 1) : ((16 - fontwidthtab[c]) >> 1)));
		v0->t = (float)(ty);
		v0->c = color;
		v0->x = (float)(x);
		v0->y = (float)(y);
		v0->z = 0.0f;

		v1->s = (float)(tx + 16 - (fw ? ((16 - fw) >> 1) : ((16 - fontwidthtab[c]) >> 1)));
		v1->t = (float)(ty + 16);
		v1->c = color;
		v1->x = (float)(x + (fw ? fw : fontwidthtab[c]));
		v1->y = (float)(y + 16);
		v1->z = 0.0f;

		x += (fw ? fw : fontwidthtab[c]);
	}

	sceGumDrawArray(GU_SPRITES,
			GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
			len * 2, 0, v);
}

/**
 * Calls drawstring until screen is filled or the filelist has run out
 */
void drawPSPWindow(psp_window *w, int fulltext) {
	char tmp[128];
	int i,j;
	int x = 1;
	listelm *l;
	if(w->loc == RIGHT)
		x = 241;
	
	/* 16 pixel height */
	/* 10 average pixel width */ 
	for(i = 0; w->base + i < w->fl->size; i++) {
		l = w->fl->data[i+w->base];
		memset(tmp,'\0',128);
		if(fulltext) {
			strncpy(tmp, l->filename, 48);
		}
		else {
			strncpy(tmp, l->filename, 19);
		}
		if(l->filesize != 0) {
			if(!l->isDir){
				if(l->filesize > 1024*1024) {
					sprintf(tmp+18, " %dM", (l->filesize)/(1024*1024));
				}
				else if(l->filesize > 1024) {
					sprintf(tmp+18, " %dK", (l->filesize)/1024);
				}
				else {
					sprintf(tmp+18, " %d", (l->filesize));
				}
				for(j=0;j<20;j++) {
					if(tmp[j] == '\0')
						tmp[j] = ' ';
				}
			}
			else {
				if(!fulltext)	
					tmp[20] = '\0';
			}
		}
		if(i == w->selected && w->active) {
			drawString(tmp,x,(i)*16,0xFFFFFF00,10);
		}
		else {
			drawString(tmp,x,(i)*16,0xFFFFFFFF,10);
		}
	}
}

