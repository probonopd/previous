/*  Previous - iethernet.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

   Network adapter for the NEXT. 

   constants and struct netBSD mb8795reg.h file

*/

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "esp.h"
#include "sysReg.h"
#include "dma.h"
#include "scsi.h"

#define LOG_EN_LEVEL LOG_DEBUG

void Ethernet_read(void) {
	int addr=IoAccessCurrentAddress & 0x1FFFF;
	int reg=addr%16;
	int val=0;
	switch (addr) {
		case 9 :
			val=0x80;
			break;

	}
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=val;
    	Log_Printf(LOG_EN_LEVEL, "[Ethernet] read reg %d val %d PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
}

void Ethernet_write(void) {
	int val=IoMem[IoAccessCurrentAddress & 0x1FFFF];
	int reg=IoAccessCurrentAddress%16;
    	Log_Printf(LOG_EN_LEVEL, "[Ethernet] write reg %d val %d PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);	
}

