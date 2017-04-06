/*  Previous - scc.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Serial Communication Controller (AMD AM8530H) Emulation.
 
 Incomplete. Hacked to pass power-on test --> see SCC_Reset()
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "scc.h"
#include "sysReg.h"
#include "dma.h"

#define IO_SEG_MASK	0x1FFFF

#define LOG_SCC_LEVEL		LOG_NONE
#define LOG_SCC_REG_LEVEL	LOG_NONE


/* Registers */

Uint8 scc_control_read(Uint8 channel);
void scc_control_write(Uint8 channel, Uint8 val);
Uint8 scc_data_read(Uint8 channel);
void scc_data_write(Uint8 channel, Uint8 val);


void SCC_ControlB_Read(void) { // 0x02018000
	IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = scc_control_read(1);
	Log_Printf(LOG_SCC_REG_LEVEL,"[SCC] Channel B control read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCC_ControlB_Write(void) {
	scc_control_write(1, IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
	Log_Printf(LOG_SCC_REG_LEVEL,"[SCC] Channel B control write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCC_ControlA_Read(void) { // 0x02018001
	IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = scc_control_read(0);
	Log_Printf(LOG_SCC_REG_LEVEL,"[SCC] Channel A control read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCC_ControlA_Write(void) {
	scc_control_write(0, IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
	Log_Printf(LOG_SCC_REG_LEVEL,"[SCC] Channel A control write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCC_DataB_Read(void) { // 0x02018002
	IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = scc_data_read(1);
	Log_Printf(LOG_SCC_REG_LEVEL,"[SCC] Channel B data read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCC_DataB_Write(void) {
	scc_data_write(1, IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
	Log_Printf(LOG_SCC_REG_LEVEL,"[SCC] Channel B data write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCC_DataA_Read(void) { // 0x02018003
	IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = scc_data_read(0);
	Log_Printf(LOG_SCC_REG_LEVEL,"[SCC] Channel A data read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCC_DataA_Write(void) {
	scc_data_write(0, IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
	Log_Printf(LOG_SCC_REG_LEVEL,"[SCC] Channel A data write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}


/* SCC Registers */

struct {
	Uint8 rreg[16];
	Uint8 wreg[16];
} scc[2];

Uint8 scc_register_pointer = 0;

/* Write function prototypes */
void scc_write_init_command(Uint8 ch, Uint8 val);
void scc_write_mode(Uint8 ch, Uint8 val);
void scc_write_master_intr_reset(Uint8 val);


Uint8 scc_control_read(Uint8 ch) {
	Uint8 val = 0;
	
	switch (scc_register_pointer) {
		case 0:
		case 1:
		case 3:
		case 10:
			val = scc[ch].rreg[scc_register_pointer];
			break;
			
		case 12:
		case 13:
		case 15:
			val = scc[ch].wreg[scc_register_pointer];
			break;
			
		case 2:
			val = scc[0].wreg[2]; // FIXME: different for channel B
			break;
			
		case 8:
			val = 0; // FIXME: Data Read
			break;
			
		default:
			Log_Printf(LOG_WARN, "[SCC] Invalid register (%i) read\n",scc_register_pointer);
			break;
	}

	Log_Printf(LOG_SCC_LEVEL,"[SCC] Channel %c: Register %i read %02X\n",
			   ch?'B':'A',scc_register_pointer,val);

	scc_register_pointer = 0;

	return val;
}

void scc_control_write(Uint8 ch, Uint8 val) {
	
	if (scc_register_pointer==0) {
		scc_register_pointer = val&7;
		if ((val&0x38)==8) {
			scc_register_pointer |= 8;
		}
		if (val&0xF0) {
			scc_write_init_command(ch, val);
		}
	} else {
		Log_Printf(LOG_SCC_LEVEL,"[SCC] Channel %c: Register %i write %02X\n",
				   ch?'B':'A',scc_register_pointer,val);
		
		switch (scc_register_pointer) {
			case 1:
				scc_write_mode(ch, val);
				break;
			case 9:
				scc_write_master_intr_reset(val);
				break;
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
				break;
			
			default:
				Log_Printf(LOG_WARN, "[SCC] Invalid register (%i) write\n",scc_register_pointer);
				break;
		}
		
		scc_register_pointer = 0;
	}
}

Uint8 scc_data_read(Uint8 ch) {
	Uint8 val = 0;
	
	val = scc_buf[0];
	
	Log_Printf(LOG_SCC_LEVEL,"[SCC] Channel %c: Data read %02X\n",
			   ch?'B':'A',val);

	return val;
}

void scc_data_write(Uint8 ch, Uint8 val) {
	
	scc_buf[0] = val;
	
	scc[ch].rreg[R_STATUS] = RR0_TXEMPTY|RR0_RXAVAIL;

	Log_Printf(LOG_SCC_LEVEL,"[SCC] Channel %c: Data write %02X\n",
			   ch?'B':'A',val);
}


/* Reset functions */
static void scc_channel_reset(Uint8 ch, bool hard) {
	
	Log_Printf(LOG_SCC_LEVEL, "[SCC] Reset channel %c\n", ch?'B':'A');
	
	scc[ch].wreg[0] = 0x00;
	scc[ch].wreg[1] &= ~0xDB;
	scc[ch].wreg[3] &= ~0x01;
	scc[ch].wreg[4] |= 0x04;
	scc[ch].wreg[5] &= ~0x9E;
	scc[0].wreg[9] &= ~0x20;
	scc[1].wreg[9] &= ~0x20;
	scc[ch].wreg[10] &= ~0x9F;
	scc[ch].wreg[14] = (scc[ch].wreg[14]&~0x3C)|0x20;
	scc[ch].wreg[15] = 0xF8;
 
	scc[ch].rreg[0] = (scc[ch].rreg[0]&~0xC7)|0x44;
	scc[ch].rreg[1] = 0x06;
	scc[ch].rreg[3] = 0x00;
	scc[ch].rreg[10] = 0x00;
 
	if (hard) {
		scc[0].wreg[9] = (scc[0].wreg[9]&~0xFC)|0xC0;
		scc[1].wreg[9] = (scc[1].wreg[9]&~0xFC)|0xC0;
		scc[ch].wreg[10] = 0x00;
		scc[ch].wreg[11] = 0x08;
		scc[ch].wreg[14] = (scc[ch].wreg[14]&~0x3F)|0x20;
	}
}

void SCC_Reset(Uint8 ch) {
	if(ch==0 || ch==1) { // channel reset
		scc_channel_reset(ch, false);
	} else { // hard reset
		scc_channel_reset(0, true);
		scc_channel_reset(1, true);
	}
}

/* Register write functions */

void scc_write_init_command(Uint8 ch, Uint8 val) {
	
}

void scc_write_mode(Uint8 ch, Uint8 val) {
	if ((val&0xC0)==0xC0) {
		dma_scc_read_memory();
		scc[ch].rreg[R_STATUS] |= RR0_RXAVAIL;
	}
}

void scc_write_master_intr_reset(Uint8 val) {
	switch ((val>>6)&3) {
		case 1:
			SCC_Reset(1);
			break;
		case 2:
			SCC_Reset(0);
			break;
		case 3:
			SCC_Reset(2);
			break;
			
		default:
			break;
	}
}
