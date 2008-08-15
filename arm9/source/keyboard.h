extern const unsigned char keyboard_Hit[12][32];
extern const unsigned char keyboard_Hit_Shift[12][32];

void setTile(uint16 *map, int x, int y, int pal);
void initKeyboard();
char processKeyboard(char* str, unsigned int max, unsigned int echo);
