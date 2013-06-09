/*  Previous - mo.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Canon Magneto-optical Disk Drive Emulation.
 
 Based on MESS source code.
 
 NeXT Optical Storage Processor uses Reed-Solomon algorithm for error correction.
 It has 2 128 byte internal buffers and uses double-buffering to perform error correction.
 
 Dummy to pass POT.
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "mo.h"
#include "sysReg.h"
#include "dma.h"

#define LOG_MO_LEVEL LOG_WARN
#define IO_SEG_MASK	0x1FFFF


struct {
    Uint16 track_num;
    Uint8 sector_incrnum;
    Uint8 sector_count;
    Uint8 intstatus;
    Uint8 intmask;
    Uint8 ctrlr_csr2;
    Uint8 ctrlr_csr1;
    Uint16 command;
} mo_drive;

Uint8 ECC_buffer[1600];
Uint32 length;
Uint8 sector_position;

/* MO Drive Registers */
#define MO_INTSTATUS    4
#define MO_INTMASK      5
#define MO_CTRLR_CSR2   6
#define MO_CTRLR_CSR1   7
#define MO_COMMAND_HI   8
#define MO_COMMAND_LO   9
#define MO_INIT         12
#define MO_FORMAT       13
#define MO_MARK         14

/* MO Drive Register Constants */
#define INTSTAT_CLR     0xFC
#define INTSTAT_RESET   0x01
// controller csr 2
#define ECC_MODE        0x20
// controller csr 1
#define ECC_READ        0x80
#define ECC_WRITE       0x40

// drive commands
#define OD_SEEK         0x0000
#define OD_HOS          0xA000
#define OD_RECALIB      0x1000
#define OD_RDS          0x2000
#define OD_RCA          0x2200

#define OD_RID          0x5000

void check_ecc(void);
void compute_ecc(void);


void MOdrive_Read(void) {
    Uint8 val;
	Uint8 reg = IoAccessCurrentAddress&0x1F;
    
    switch (reg) {
        case MO_INTSTATUS:
            val = mo_drive.intstatus;
            mo_drive.intstatus |= 0x01;
            break;
            
        case MO_INTMASK:
            val = mo_drive.intmask;
            break;
            
        case MO_CTRLR_CSR2:
            val = mo_drive.ctrlr_csr2;
            break;

        case MO_CTRLR_CSR1:
            val = mo_drive.ctrlr_csr1;
            break;

        case MO_COMMAND_HI:
            val = 0x00;
            break;

        case MO_COMMAND_LO:
            val = 0x00;
            break;
            
        case 10:
            val = 0x00;
            break;

        case 11:
            val = 0x24;
            break;

        case 16:
            val = 0x00;
            break;

        default:
            break;
    }
    
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = val;
    Log_Printf(LOG_MO_LEVEL, "[MO Drive] read reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
}


void MOdrive_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
	Uint8 reg = IoAccessCurrentAddress&0x1F;
    
    Log_Printf(LOG_MO_LEVEL, "[MO Drive] write reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
    
    switch (reg) {
        case MO_INTSTATUS: // reg 4
            switch (val) {
                case INTSTAT_CLR:
                    mo_drive.intstatus &= 0x02;
                    mo_drive.intstatus |= 0x01;
                    break;
                    
                case INTSTAT_RESET:
                    mo_drive.intstatus |= 0x01;
                    //MOdrive_Reset();

                default:
                    break;
                    
                //mo_drive.intstatus = (mo_drive.intstatus & (~val & 0xfc)) | (val & 3);
            }
            break;
            
        case MO_INTMASK: // reg 5
            mo_drive.intmask = val;
            break;
            
        case MO_CTRLR_CSR2: // reg 6
            mo_drive.ctrlr_csr2 = val;
            break;
            
        case MO_CTRLR_CSR1: // reg 7
            mo_drive.ctrlr_csr1 = val;
            switch (mo_drive.ctrlr_csr1) {
                case ECC_WRITE:
                    dma_memory_read(ECC_buffer, &length, CHANNEL_DISK);
                    mo_drive.intstatus |= 0xFF;
                    if (mo_drive.ctrlr_csr2&ECC_MODE)
                        check_ecc();
                    else
                        compute_ecc();
                    break;
                    
                case ECC_READ:
                    dma_memory_write(ECC_buffer, length, CHANNEL_DISK);
                    mo_drive.intstatus |= 0xFF;
                    break;
                    
                case 0x20: // RD_STAT
                    mo_drive.intstatus |= 0x01; // set cmd complete
                    break;
                    
                default:
                    break;
            }
            break;
        case MO_COMMAND_HI:
            mo_drive.command = (val << 8)&0xFF00;
            break;
        case MO_COMMAND_LO:
            mo_drive.command |= val&0xFF;
            MOdrive_Execute_Command(mo_drive.command);
//            mo_drive.intstatus &= ~0x01; // release cmd complete
//            set_interrupt(INT_DISK, SET_INT);
            break;
            
        case MO_INIT: // reg 12
            if (val&0x80) { // sector > enable
                printf("MO Init: sector > enable\n");
            }
            if (val&0x40) { // ECC starve disable
                printf("MO Init: ECC starve disable\n");
            }
            if (val&0x20) { // ID cmp on track, not sector
                printf("MO Init: ID cmp on track not sector\n");
            }
            if (val&0x10) { // 25 MHz ECC clk for 3600 RPM
                printf("MO Init: 25 MHz ECC clk for 3600 RPM\n");
            }
            if (val&0x08) { // DMA starve enable
                printf("MO Init: DMA starve enable\n");
            }
            if (val&0x04) { // diag: generate bad parity
                printf("MO Init: diag: generate bad parity\n");
            }
            if (val&0x03) {
                printf("MO Init: %i IDs must match\n", val&0x03);
            }
            break;
            
        case MO_FORMAT: // reg 13
            break;
            
        case MO_MARK: // reg 14
            break;
            
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
            break; // flag strategy
            
        default:
            break;
    }
}


void MOdrive_Execute_Command(Uint16 command) {
    mo_drive.intstatus &= ~0x01; // release cmd complete
    switch (command) {
        case OD_SEEK:
//            set_interrupt(INT_DISK, SET_INT);
            break;
        case OD_RDS:
            break;
            
        case OD_RID:
            break;
            
        default:
            break;
    }
}


void check_ecc(void) {
    int i;
	for(i=0; i<0x400; i++)
		ECC_buffer[i] = i;
}

void compute_ecc(void) {
	memset(ECC_buffer+0x400, 0, 0x110);
}