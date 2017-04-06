#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "file.h"
#include "dimension.h"
#include "nd_mem.h"
#include "nd_rom.h"

/* --------- NEXTDIMENSION EEPROM --------- *
 *                                          *
 *  Intel 28F010 (1024k CMOS Flash Memory)  *
 *                                          */

/* NeXTdimension board memory */
#define ROM_CMD_READ    0x00
#define ROM_CMD_READ_ID 0x90
#define ROM_CMD_ERASE   0x20
#define ROM_CMD_WRITE   0x40
#define ROM_CMD_VERIFY  0x80
#define ROM_CMD_RESET   0xFF

#define ROM_ID_MFG      0x89
#define ROM_ID_DEV      0xB4


Uint8 nd_rom_command = 0x00;
Uint32 nd_rom_last_addr = 0x00;

Uint8 nd_rom_read(Uint32 addr) {
    switch (nd_rom_command) {
        case ROM_CMD_READ:
            return ND_rom[addr];
        case ROM_CMD_READ_ID:
            switch (addr) {
                case 0: return ROM_ID_MFG;
                case 1: return ROM_ID_DEV;
                default: return ROM_ID_DEV;
            }
        case (ROM_CMD_ERASE|ROM_CMD_VERIFY):
            nd_rom_command = ROM_CMD_READ;
            return ND_rom[addr];
        case (ROM_CMD_WRITE|ROM_CMD_VERIFY):
            nd_rom_command = ROM_CMD_READ;
            return ND_rom[nd_rom_last_addr];
        case ROM_CMD_RESET:
        default:
            return 0;
    }
}

void nd_rom_write(Uint32 addr, Uint8 val) {
    int i;
    
    switch (nd_rom_command) {
        case ROM_CMD_WRITE:
            Log_Printf(LOG_WARN, "[ND] ROM: Writing ROM (addr=%04X, val=%02X)",addr,val);
            ND_rom[addr] = val;
            nd_rom_last_addr = addr;
            nd_rom_command = ROM_CMD_READ;
            break;
        case ROM_CMD_ERASE:
            if (val==ROM_CMD_ERASE) {
                Log_Printf(LOG_WARN, "[ND] ROM: Erasing ROM");
                for (i = 0; i < (128*1024); i++)
                    ND_rom[i] = 0xFF;
                break;
            } /* else fall through */
        default:
            Log_Printf(LOG_WARN, "[ND] ROM: Command %02X",val);
            nd_rom_command = val;
            break;
    }
}


/* Load NeXTdimension ROM from file */
void nd_rom_load(void) {
    FILE* romfile;
    
    nd_rom_command = ROM_CMD_READ;
    nd_rom_last_addr = 0;
    
    if (!File_Exists(ConfigureParams.Dimension.szRomFileName)) {
        Log_Printf(LOG_WARN, "[ND] Error: ROM file does not exist or is not readable");
        return;
    }
    
    romfile = File_Open(ConfigureParams.Dimension.szRomFileName, "rb");
    
    if (romfile==NULL) {
        Log_Printf(LOG_WARN, "[ND] Error: Cannot open ROM file");
        return;
    }
    
    fread(ND_rom,1, 128 * 1024 ,romfile);
    
    Log_Printf(LOG_WARN, "[ND] Read ROM from %s",ConfigureParams.Dimension.szRomFileName);
    
    File_Close(romfile);
}