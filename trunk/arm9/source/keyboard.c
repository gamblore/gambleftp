/*---------------------------------------------------------------------------------

	Keyboard Example 5
	Author: Headkaze
	Modifications: Adam Metcalf
		Changed to Main Background from Sub Background

---------------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <nds.h>
#include <nds/arm9/console.h>	//basic print funcionality
#include <nds/arm9/sound.h>	// sound functions

#include "keyboard_raw_lc_bin.h"
#include "keyboard_raw_uc_bin.h"
#include "keyboard_map_lc_bin.h"
#include "keyboard_map_uc_bin.h"
#include "keyboard_pal_bin.h"
#include "keyboard_pal_hl_bin.h"

#include "clickdown_bin.h"
#include "clickup_bin.h"

// comment out the line below to remove the custom pen jump detection code
#define USE_CUSTOM_PEN_JUMP_DETECTION_CODE	1

#define PEN_DOWN (~IPC->buttons & (1 << 6))

#define F_1	0x3c
#define F_2	0x3d
#define F_3	0x3e
#define F_4	0x3f
#define F_5	0x40
#define F_6	0x41
#define F_7	0x42
#define F_8	0x43
#define F_9	0x44
#define F10	0x45
#define F11	0x46
#define F12	0x47

#define EXT	0x1		// Exit

#define HOM	0x47	// Home
#define PGU	0x49	// Page Up
#define PGD	0x51	// Page Down
#define END	0x4f	// End

#define TAB	'\t'	// Tab

#define ESC	0x1b	// Escape
#define BSP	0x8		// Backspace
#define CAP	0x2		// Caps
#define RET	'\n'	// Enter
#define SHF	0x4		// Shift
#define	CTL	0x1d	// Ctrl
#define SPC	0x20	// Space
#define ALT	0x38	// Alt
#define NDS	0x4a	// DS
#define SCN	0x46	// Screen

#define CRU	0x48	// Cursor up
#define CRD	0x50	// Cursor down
#define CRL	0x4b	// Cursor Left
#define CRR	0x4d	// Cursor Right

#define INS	0x52	// Insert
#define DEL	0x53	// Delete

#define TILE_OFFSET_Y 13	// 13 tiles from the top

const unsigned char keyboard_Hit[10][32] = {
	{ 0x0,'1','1','2','2','3','3','4','4','5','5','6','6','7','7','8','8','9','9','0','0','-','-','=','=',BSP,BSP,BSP,BSP,HOM,HOM,0x0 },
	{ 0x0,'1','1','2','2','3','3','4','4','5','5','6','6','7','7','8','8','9','9','0','0','-','-','=','=',BSP,BSP,BSP,BSP,HOM,HOM,0x0 },
	{ 0x0,TAB,'q','q','w','w','e','e','r','r','t','t','y','y','u','u','i','i','o','o','p','p','[','[',']',']','\\','\\',0x0,PGU,PGU,0x0 },
	{ 0x0,TAB,'q','q','w','w','e','e','r','r','t','t','y','y','u','u','i','i','o','o','p','p','[','[',']',']','\\','\\',0x0,PGU,PGU,0x0 },
	{ 0x0,CAP,CAP,'a','a','s','s','d','d','f','f','g','g','h','h','j','j','k','k','l','l',';',';','\'','\'',RET,RET,RET,RET,PGD,PGD,0x0 },
	{ 0x0,CAP,CAP,'a','a','s','s','d','d','f','f','g','g','h','h','j','j','k','k','l','l',';',';','\'','\'',RET,RET,RET,RET,PGD,PGD,0x0 },
	{ 0x0,SHF,SHF,SHF,'z','z','x','x','c','c','v','v','b','b','n','n','m','m',',',',','.','.','/','/',SHF,SHF,SHF,CRU,CRU,END,END,0x0 },
	{ 0x0,SHF,SHF,SHF,'z','z','x','x','c','c','v','v','b','b','n','n','m','m',',',',','.','.','/','/',SHF,SHF,SHF,CRU,CRU,END,END,0x0 },
	{ 0x0,CTL,CTL,NDS,NDS,ALT,ALT,'`','`',SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,INS,INS,DEL,DEL,SCN,SCN,CRL,CRL,CRD,CRD,CRR,CRR,0x0 },
	{ 0x0,CTL,CTL,NDS,NDS,ALT,ALT,'`','`',SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,INS,INS,DEL,DEL,SCN,SCN,CRL,CRL,CRD,CRD,CRR,CRR,0x0 }
};

const unsigned char keyboard_Hit_Shift[10][32] = {
	{ 0x0,'!','!','@','@','#','#','$','$','%','%','^','^','&','&','*','*','(','(',')',')','_','_','+','+',BSP,BSP,BSP,BSP,HOM,HOM,0x0 },
	{ 0x0,'!','!','@','@','#','#','$','$','%','%','^','^','&','&','*','*','(','(',')',')','_','_','+','+',BSP,BSP,BSP,BSP,HOM,HOM,0x0 },
	{ 0x0,TAB,'Q','Q','W','W','E','E','R','R','T','T','Y','Y','U','U','I','I','O','O','P','P','{','{','}','}','|','|',0x0,PGU,PGU,0x0 },
	{ 0x0,TAB,'Q','Q','W','W','E','E','R','R','T','T','Y','Y','U','U','I','I','O','O','P','P','{','{','}','}','|','|',0x0,PGU,PGU,0x0 },
	{ 0x0,CAP,CAP,'A','A','S','S','D','D','F','F','G','G','H','H','J','J','K','K','L','L',':',':','"','"',RET,RET,RET,RET,PGD,PGD,0x0 },
	{ 0x0,CAP,CAP,'A','A','S','S','D','D','F','F','G','G','H','H','J','J','K','K','L','L',':',':','"','"',RET,RET,RET,RET,PGD,PGD,0x0 },
	{ 0x0,SHF,SHF,SHF,'Z','Z','X','X','C','C','V','V','B','B','N','N','M','M','<','<','>','>','?','?',SHF,SHF,SHF,CRU,CRU,END,END,0x0 },
	{ 0x0,SHF,SHF,SHF,'Z','Z','X','X','C','C','V','V','B','B','N','N','M','M','<','<','>','>','?','?',SHF,SHF,SHF,CRU,CRU,END,END,0x0 },
	{ 0x0,CTL,CTL,NDS,NDS,ALT,ALT,'~','~',SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,INS,INS,DEL,DEL,SCN,SCN,CRL,CRL,CRD,CRD,CRR,CRR,0x0 },
	{ 0x0,CTL,CTL,NDS,NDS,ALT,ALT,'~','~',SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,SPC,INS,INS,DEL,DEL,SCN,SCN,CRL,CRL,CRD,CRD,CRR,CRR,0x0 }
};


#define KB_NORMAL 0
#define KB_CAPS   1
#define KB_SHIFT  2

#define ECHO_ON	 0
#define ECHO_OFF 1

#define BSP	0x8		// Backspace
#define CAP	0x2		// Caps
#define RET	'\n'	// Enter
#define SHF	0x4		// Shift

static int g_dx=0;
static int g_dy=0;
static u16 g_MouseDown = false;
static int g_Mode = KB_NORMAL;
static int g_col = 0;

static unsigned int lasttilex=0, lasttiley=0;
static char lastkey = 0x0;

void setTile(uint16 *map, int x, int y, int pal)
{
	char c;
	int x2, y2;

	c = keyboard_Hit[y-TILE_OFFSET_Y][x];

	if(!c) return;

	map[(y*32)+x] &= ~(1 << 12);
	map[(y*32)+x] |= (pal << 12);

	x2 = x; y2 = y;
	while(keyboard_Hit[y2-TILE_OFFSET_Y][x2]==c)
	{
		map[(y2*32)+x2] &= ~(1 << 12);
		map[(y2*32)+x2] |= (pal << 12);

		x2 = x;
		while(keyboard_Hit[y2-TILE_OFFSET_Y][x2]==c) { map[(y2*32)+x2] &= ~(1 << 12); map[(y2*32)+x2] |= (pal << 12); x2++; }
		x2 = x;
		while(keyboard_Hit[y2-TILE_OFFSET_Y][x2]==c) { map[(y2*32)+x2] &= ~(1 << 12); map[(y2*32)+x2] |= (pal << 12); x2--; }

		x2 = x;
		y2++;
	}

	x2 = x; y2 = y;
	while(keyboard_Hit[y2-TILE_OFFSET_Y][x2]==c)
	{
		map[(y2*32)+x2] &= ~(1 << 12);
		map[(y2*32)+x2] |= (pal << 12);

		x2 = x;
		while(keyboard_Hit[y2-TILE_OFFSET_Y][x2]==c) { map[(y2*32)+x2] &= ~(1 << 12); map[(y2*32)+x2] |= (pal << 12); x2++; }
		x2 = x;
		while(keyboard_Hit[y2-TILE_OFFSET_Y][x2]==c) { map[(y2*32)+x2] &= ~(1 << 12); map[(y2*32)+x2] |= (pal << 12); x2--; }

		x2 = x;
		y2--;
	}
}

void initKeyboard()
{
	/*videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);*/

	BG0_CR = BG_COLOR_16 | BG_32x32 | BG_MAP_BASE(29) | BG_TILE_BASE(3);
	/*SUB_BG0_CR = BG_COLOR_16 | BG_32x32 | (29 << SCREEN_SHIFT) | (1 << CHAR_SHIFT);*/

	dmaCopy((uint16 *) keyboard_pal_bin, (uint16 *) BG_PALETTE, keyboard_pal_bin_size);
	dmaCopy((uint16 *) keyboard_pal_hl_bin, (uint16 *) &BG_PALETTE[16], keyboard_pal_hl_bin_size);
	dmaCopy((uint16 *) keyboard_map_lc_bin, (uint16 *) SCREEN_BASE_BLOCK(29), keyboard_map_lc_bin_size);
	dmaCopy((uint16 *) keyboard_raw_lc_bin, (uint16 *) CHAR_BASE_BLOCK(3), keyboard_raw_lc_bin_size);
	
	g_dx=0;
	g_dy=0;
	g_MouseDown = false;
	g_Mode = KB_NORMAL;
	g_col = 0;

	lasttilex=0, lasttiley=0;
	lastkey = 0x0;
}

// returns - last key pressed
// str - pointer to string to store output
// max - maximum number of characters
// echo - echo input to console
char processKeyboard(char* str, unsigned int max, unsigned int echo)
{
	touchPosition touchXY;
	
	//get the map
	uint16 *map = (uint16 *) SCREEN_BASE_BLOCK(29); 

	touchXY=touchReadXY();

        if(PEN_DOWN && !g_MouseDown)
	{
		g_dx = touchXY.px;
		g_dy = touchXY.py;
		g_MouseDown = TRUE;
    } else
	if(PEN_DOWN && g_MouseDown)
	{
		int i, j;
		
		i = touchXY.px;
		j = touchXY.py;

#ifdef USE_CUSTOM_PEN_JUMP_DETECTION_CODE
		int z1, z2;
		
		z1 = IPC->touchZ1;
		z2 = IPC->touchZ2;
	
		// This is checking z registers are not zero for pen jumping
		if(z1!=0 && z2!=0)
		{
#endif 
			g_dx = i;
			g_dy = j;

			unsigned int tilex, tiley;

			tilex = g_dx/8;
			tiley = g_dy/8;

			if(tilex>=1 && tilex<=30 && tiley>=13 && tiley<=23)
			{
				char c;

				if(g_Mode==KB_NORMAL)
					c = keyboard_Hit[tiley-TILE_OFFSET_Y][tilex];
				else
					c = keyboard_Hit_Shift[tiley-TILE_OFFSET_Y][tilex];

				//if(lastkey != c)				
				//	playGenericSound(clickdown_bin, clickdown_bin_size);

				setTile(map, lasttilex, lasttiley, 0);
				setTile(map, tilex, tiley, 1);
				lastkey = c; lasttilex = tilex; lasttiley = tiley;
			}
#ifdef USE_CUSTOM_PEN_JUMP_DETECTION_CODE
		} else g_MouseDown = FALSE;
#endif 
    } else
	if((!PEN_DOWN && g_MouseDown) || ((!PEN_DOWN && !g_MouseDown) && lastkey != 0)) {
		g_MouseDown = FALSE;
		char c, buf[2];

		unsigned int tilex, tiley;

		tilex = g_dx/8;
		tiley = g_dy/8;

		if(tilex>=1 && tilex<=30 && tiley>=13 && tiley<=23)
		{
			if(g_Mode==KB_NORMAL)
				c = keyboard_Hit[tiley-TILE_OFFSET_Y][tilex];
			else
				c = keyboard_Hit_Shift[tiley-TILE_OFFSET_Y][tilex];

			playGenericSound(clickup_bin, clickup_bin_size);
			setTile(map, lasttilex, lasttiley, 0);

			lastkey = 0; lasttilex = 0; lasttiley = 0;

			buf[0] = c;
			buf[1] = (char) NULL;

			if(c==RET) // Return
			{
				if(echo==ECHO_ON)
				{
					iprintf("\n");
					g_col = 0;
				}
			} else
			if(c==BSP) // Backspace
			{
				if(strlen(str)>0)
				{
					if(echo==ECHO_ON)
					{
						if(g_col == 0)
						{
							g_col = 31;
							iprintf("\x1b[1A\x1b[31C \x1b[1D");
						} else { g_col--; iprintf("\x1b[1D \x1b[1D"); }
					}
					str[strlen(str)-1] = (char) NULL;
				}
				
			} else
			if(c==CAP) // Caps
			{
				lasttilex = 0; lasttiley = 0;
				if(g_Mode==KB_NORMAL) {
					dmaCopy((uint16 *) keyboard_map_uc_bin, (uint16 *) SCREEN_BASE_BLOCK(29), keyboard_map_uc_bin_size);
					dmaCopy((uint16 *) keyboard_raw_uc_bin, (uint16 *) CHAR_BASE_BLOCK(3), keyboard_raw_uc_bin_size);

					map[(17*32)+1] |= (1 << 12);
					map[(17*32)+2] |= (1 << 12);
					map[(18*32)+1] |= (1 << 12);
					map[(18*32)+2] |= (1 << 12);

					g_Mode = KB_CAPS;
				} else {
					dmaCopy((uint16 *) keyboard_map_lc_bin, (uint16 *) SCREEN_BASE_BLOCK(29), keyboard_map_lc_bin_size);
					dmaCopy((uint16 *) keyboard_raw_lc_bin, (uint16 *) CHAR_BASE_BLOCK(3), keyboard_raw_lc_bin_size);
					g_Mode = KB_NORMAL;
				}
			} else
			if(c==SHF) // Shift
			{
				lasttilex = 0; lasttiley = 0;
				if(g_Mode==KB_NORMAL) {
					dmaCopy((uint16 *) keyboard_map_uc_bin, (uint16 *) SCREEN_BASE_BLOCK(29), keyboard_map_uc_bin_size);
					dmaCopy((uint16 *) keyboard_raw_uc_bin, (uint16 *) CHAR_BASE_BLOCK(3), keyboard_raw_uc_bin_size);

					map[(19*32)+1] |= (1 << 12);
					map[(19*32)+2] |= (1 << 12);
					map[(19*32)+3] |= (1 << 12);
					map[(20*32)+1] |= (1 << 12);
					map[(20*32)+2] |= (1 << 12);
					map[(20*32)+3] |= (1 << 12);

					map[(19*32)+24] |= (1 << 12);
					map[(19*32)+25] |= (1 << 12);
					map[(19*32)+26] |= (1 << 12);
					map[(20*32)+24] |= (1 << 12);
					map[(20*32)+25] |= (1 << 12);
					map[(20*32)+26] |= (1 << 12);
					g_Mode = KB_SHIFT;
				} else {
					dmaCopy((uint16 *) keyboard_map_lc_bin, (uint16 *) SCREEN_BASE_BLOCK(29), keyboard_map_lc_bin_size);
					dmaCopy((uint16 *) keyboard_raw_lc_bin, (uint16 *) CHAR_BASE_BLOCK(3), keyboard_raw_lc_bin_size);
					g_Mode = KB_NORMAL;
				}
			} else 
			{
				if(strlen(str)<max-1 && (c>=32 && c<=126)) {
					strcat(str, buf);
					if(echo==ECHO_ON)
					{
						iprintf("%c",c);
						g_col++;
						if(g_col == 33) g_col = 1;
					}
				}
				if(g_Mode == KB_SHIFT) // Undo Shift
				{
					dmaCopy((uint16 *) keyboard_map_lc_bin, (uint16 *) SCREEN_BASE_BLOCK(29), keyboard_map_lc_bin_size);
					dmaCopy((uint16 *) keyboard_raw_lc_bin, (uint16 *) CHAR_BASE_BLOCK(3), keyboard_raw_lc_bin_size);
					g_Mode = KB_NORMAL;
				}
			}
			return c;
		} else
		{
			setTile(map, lasttilex, lasttiley, 0);

			lastkey = 0; lasttilex = 0; lasttiley = 0;
		}
	}
	
	return 0;
}
