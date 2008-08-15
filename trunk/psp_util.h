#ifndef __PSP_UTIL_H__
#define __PSP_UTIL_H__

/* graphic defines */
struct Vertex {
	unsigned int color;
	float x,y,z;
};

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define PIXEL_SIZE (4)
#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE)
#define ZBUF_SIZE (BUF_WIDTH SCR_HEIGHT * 2)

/* create and delete exit callbacks */
int SetupCallbacks(void);
void DisableCallbacks(void);

/**
 * Get user input using the PSP on screen keyboard
 */
char *getKeyboard(int max, int column, char *pretext, char *current);


int ShowMessageDialog(const char *message, int enableYesno);

/**
 * Seting up INet
 */
void setupINet();
int netDialog();
int connectToAp(int config);
void INetTerm(void);

/**
 * Setting up Adhoc
 */
void setupAdhoc(char *adhoc_name);
void adhocTerm(void);

/**
 * Use the gamesharing dialog
 */
int gsDialog();

#endif
