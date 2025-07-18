#include "ibm.h"

/* Definitions of global variables previously in ibm.h */

uint8_t *ram;
uint32_t rammask;
int readlookup[256], readlookupp[256];
uintptr_t *readlookup2;
int readlnext;
int writelookup[256], writelookupp[256];
uintptr_t *writelookup2;
int writelnext;
PIT pit, pit2;
dma_t dma[8];
PPI ppi;
PIC pic, pic2;
char discfns[2][256];
int driveempty[2];
int GAMEBLASTER, GUS, SSI2001, voodoo_enabled;
PcemHDC hdc[7];
int keybsenddelay;

int hasfpu;
int romset;
int gfxcard;
int cpuspeed;
int readflash;
int ppispeakon;
int gated, speakval, speakon;
