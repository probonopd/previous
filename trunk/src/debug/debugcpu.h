/*
  Hatari - debugcpu.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Public CPU debugging header.
*/

#ifndef HATARI_DEBUGCPU_H
#define HATARI_DEBUGCPU_H

void DebugCpu_Check(void);
void DebugCpu_SetDebugging(void);
int DebugCpu_DisAsm(int nArgc, char *psArgs[]);
int DebugCpu_MemDump(int nArgc, char *psArgs[]);
int DebugCpu_Register(int nArgc, char *psArgs[]);
int DebugCpu_GetRegisterAddress(const char *reg, Uint32 **addr);

Uint32 DBGMemory_ReadLong(Uint32 addr);
Uint16 DBGMemory_ReadWord(Uint32 addr);
Uint8  DBGMemory_ReadByte(Uint32 addr);

void DBGMemory_WriteLong(Uint32 addr,Uint32 val);
void DBGMemory_WriteWord(Uint32 addr,Uint16 val);
void DBGMemory_WriteByte(Uint32 addr,Uint8 val);

#endif /* HATARI_DEBUGCPU_H */
