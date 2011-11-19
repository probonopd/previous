/*
  Hatari - stMemory.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  ST Memory access functions.
*/
const char STMemory_fileid[] = "Hatari stMemory.c : " __DATE__ " " __TIME__;

#include "nextMemory.h"
#include "configuration.h"
#include "ioMem.h"
#include "log.h"
#include "memory.h"
#include "memorySnapShot.h"

/*
 * main RAM buffer (8mb for stock Next)
 */
Uint8 NEXTRam[64*1024*1024];

Uint32 NEXTRamEnd;            /* End of ST Ram, above this address is no-mans-land and ROM/IO memory */


Uint8 NEXTRom[0x20000];

Uint8 NEXTIo[0x20000];

/**
 * Clear section of NEXT's memory space.
 */
void NEXTMemory_Clear(Uint32 StartAddress, Uint32 EndAddress)
{
	memset(&NEXTRam[StartAddress], 0, EndAddress-StartAddress);
}


/**
 * TODO 
 */
void NEXTMemory_MemorySnapShot_Capture(bool bSave)
{
	MemorySnapShot_Store(&NEXTRamEnd, sizeof(NEXTRamEnd));

	/* Only save/restore area of memory machine is set to, eg 1Mb */
	MemorySnapShot_Store(NEXTRam, NEXTRamEnd);
}


/**
 * TODO
 */
void NEXTMemory_SetDefaultConfig(void)
{
}
