/*
  Hatari - stMemory.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  ST Memory access functions.
*/
const char STMemory_fileid[] = "Previous nextMemory.c : " __DATE__ " " __TIME__;

#include "nextMemory.h"
#include "configuration.h"
#include "ioMem.h"
#include "log.h"
#include "memory.h"

/*
 * Main RAM buffer (128 MB for turbo systems)
 */
Uint8 NEXTRam[128*1024*1024];

Uint32 NEXTRamEnd;


Uint8 NEXTRom[0x20000];

Uint8 NEXTIo[0x20000];
