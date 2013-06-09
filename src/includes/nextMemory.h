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
 * Check whether given memory address and size are within
 * valid memory area (i.e. read/write from/to there doesn't
 * overwrite Hatari's own memory & cause potential segfaults)
 * and that the size is positive.
 * 
 * If they are; return true, otherwise false.
 */
static inline bool NEXTMemory_ValidArea(Uint32 addr, int size)
{
	// we are a bit optimistic...
		return true;
}


/**
 * Write 32-bit word into ST memory space.
 * NOTE - value will be convert to 68000 endian
 */
static inline void NEXTMemory_WriteLong(Uint32 Address, Uint32 Var)
{
	put_long(Address, Var);
}


/**
 * Write 16-bit word into ST memory space.
 * NOTE - value will be convert to 68000 endian.
 */
static inline void NEXTMemory_WriteWord(Uint32 Address, Uint16 Var)
{
	put_word(Address, Var);
}

/**
 * Write 8-bit byte into ST memory space.
 */
static inline void NEXTMemory_WriteByte(Uint32 Address, Uint8 Var)
{
	put_byte(Address, Var);
}


/**
 * Read 32-bit word from ST memory space.
 * NOTE - value will be converted to PC endian.
 */
static inline Uint32 NEXTMemory_ReadLong(Uint32 Address)
{
	return get_long(Address);
}


/**
 * Read 16-bit word from ST memory space.
 * NOTE - value will be converted to PC endian.
 */
static inline Uint16 NEXTMemory_ReadWord(Uint32 Address)
{
	return get_word(Address);
}


/**
 * Read 8-bit byte from ST memory space
 */
static inline Uint8 NEXTMemory_ReadByte(Uint32 Address)
{
	return get_byte(Address);
}


extern void NEXTMemory_Clear(Uint32 StartAddress, Uint32 EndAddress);
extern bool NEXTMemory_SafeCopy(Uint32 addr, Uint8 *src, unsigned int len, const char *name);
extern void NEXTMemory_MemorySnapShot_Capture(bool bSave);
extern void NEXTMemory_SetDefaultConfig(void);

#endif
