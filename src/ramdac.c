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

#define LOG_RAMDAC_LEVEL    LOG_DEBUG


struct {
    Uint8 reg[4];
} ramdac;
