/*
  Hatari - stMemory.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_STMEMORY_H
#define HATARI_STMEMORY_H

#include "main.h"
#include "sysdeps.h"
#include "maccess.h"
#include "memory.h"

extern Uint32 NEXTRamEnd;

extern Uint8 NEXTRam[128*1024*1024];
extern Uint8 NEXTRom[0x20000];
extern Uint8 NEXTIo[0x20000];



/* Offset NEXT address to PC pointer: */
// # define NEXTRAM_ADDR(Var)  ((unsigned long)NEXTRam+((Uint32)(Var) & 0x03ffffff))

/**
 * Write 32-bit word into NeXT memory space.
 * NOTE - value will be convert to 68000 endian
 */
static inline void NEXTMemory_WriteLong(Uint32 Address, Uint32 Var)
{
	put_long(Address, Var);
}


/**
 * Write 16-bit word into NeXT memory space.
 * NOTE - value will be convert to 68000 endian.
 */
static inline void NEXTMemory_WriteWord(Uint32 Address, Uint16 Var)
{
	put_word(Address, Var);
}

/**
 * Write 8-bit byte into NeXT memory space.
 */
static inline void NEXTMemory_WriteByte(Uint32 Address, Uint8 Var)
{
	put_byte(Address, Var);
}


/**
 * Read 32-bit word from NeXT memory space.
 * NOTE - value will be converted to PC endian.
 */
static inline Uint32 NEXTMemory_ReadLong(Uint32 Address)
{
	return get_long(Address);
}


/**
 * Read 16-bit word from NeXT memory space.
 * NOTE - value will be converted to PC endian.
 */
static inline Uint16 NEXTMemory_ReadWord(Uint32 Address)
{
	return get_word(Address);
}


/**
 * Read 8-bit byte from NeXT memory space
 */
static inline Uint8 NEXTMemory_ReadByte(Uint32 Address)
{
	return get_byte(Address);
}

#endif
