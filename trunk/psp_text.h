#ifndef __PSP_TEXT_H__
#define __PSP_TEXT_H__
#include "filelist.h"

typedef struct {
	float s, t;
	unsigned int c;
	float x, y, z;
} VERT;

/**
 * when drawing strings call this in your begin to turn on the font texture
 */
void initFontTexture();
/**
 * Draw the string
 */
void drawString(const char* text, int x, int y, unsigned int color, int fw);
/**
 * Draw the window structure defined in filelist for the psp
 */
void drawPSPWindow(psp_window *w, int fulltext);
#endif

