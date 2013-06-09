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
 * Main RAM buffer (128 MB for turbo systems)
 */
Uint8 NEXTRam[128*1024*1024];

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

/** --useful for next?
 * Copy given memory area safely to Atari RAM.
 * If the memory area isn't fully within RAM, only the valid parts are written.
 * Useful for all kinds of IO operations.
 * 
 * addr - destination Atari RAM address
 * src - source Hatari memory address
 * len - number of bytes to copy
 * name - name / description if this memory copy for error messages
 * 
 * Return true if whole copy was safe / valid.
 */

bool NEXTMemory_SafeCopy(Uint32 addr, Uint8 *src, unsigned int len, const char *name)
{
    Uint32 end;

    if (NEXTMemory_ValidArea(addr, len))
    {
        memcpy(&NEXTRam[addr], src, len);
        return true;
    }
    Log_Printf(LOG_WARN, "Invalid '%s' RAM range 0x%x+%i!\n", name, addr, len);

    for (end = addr + len; addr < end; addr++, src++)
    {
        if (NEXTMemory_ValidArea(addr, 1))
            NEXTRam[addr] = *src;
    }
    return false;
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
