/*  Previous - ramdac.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Brooktree Bt463 RAMDAC emulation.
 
 This chip was used in color NeXTstations.
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "ramdac.h"
#include "sysReg.h"

#define IO_SEG_MASK 0x1FFFF

#define LOG_RAMDAC_LEVEL    LOG_WARN


struct {
    Uint8 reg[4];
    Uint8 cmd;
} ramdac;


#define RAMDAC_CMD_CLEAR_INT    0x01
#define RAMDAC_CMD_ENABLE_INT   0x02
#define RAMDAC_CMD_UNBLANK      0x04

void RAMDAC_CMD_Write(void) {
    ramdac.cmd=IoMem[IoAccessCurrentAddress & 0x1FFFF];
    Log_Printf(LOG_RAMDAC_LEVEL,"[RAMDAC] Command write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (ramdac.cmd&RAMDAC_CMD_CLEAR_INT) {
        set_interrupt(INT_DISK, RELEASE_INT);
    }
}


void ramdac_video_interrupt(void) {
    if (ramdac.cmd&RAMDAC_CMD_ENABLE_INT) {
        set_interrupt(INT_DISK, SET_INT);
        ramdac.cmd &= ~RAMDAC_CMD_ENABLE_INT;
    }
}