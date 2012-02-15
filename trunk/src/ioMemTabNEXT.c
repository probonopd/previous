/*
  Previous - ioMemTabNEXT.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Table with hardware IO handlers for the NEXT.
*/


const char IoMemTabST_fileid[] = "Previous ioMemTabST.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "keymap.h"
#include "esp.h"
#include "ethernet.h"
#include "sysReg.h"
#include "dma.h"



/* Hack from QEMU-NeXT, Correct this later with the data below */

/* system timer */
struct timer_reg {
	unsigned char	t_counter_latch[2];	/* counted up at 1 MHz */
	unsigned char	: 8;
	unsigned char	: 8;
	unsigned char	t_enable : 1,		/* counter enable */
    t_update : 1,		/* copy latch to counter */
    : 6;
};

// 
void System_Timer0_Read(void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = 0;
//    Log_Printf(LOG_WARN, "[CLK] read val0 %d PC=%x %s at %d",0,m68k_getpc(),__FILE__,__LINE__);
}

void System_Timer1_Read(void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = ((nCyclesMainCounter/33)& 0xF0000) >> 16;
//    Log_Printf(LOG_WARN, "[CLK] read val1 %d PC=%x %s at %d",IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc(),__FILE__,__LINE__);
}

void System_Timer2_Read(void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = ((nCyclesMainCounter/33) & 0xFF00) >> 8;
//    Log_Printf(LOG_WARN, "[CLK] read val2 %d PC=%x %s at %d",IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc(),__FILE__,__LINE__);
}

void System_Timer3_Read(void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = ((nCyclesMainCounter/33)& 0xFF);
//    Log_Printf(LOG_WARN, "[CLK] read val3 %d PC=%x %s at %d",IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc(),__FILE__,__LINE__);
}


/* Floppy Disk Drive - Work on this later */
void FDD_Main_Status_Read (void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = 0x00;
}


static Uint8 DSP_icr=0;


/* DSP registers - Work on this later */
void DSP_icr_Read (void) {
    Log_Printf(LOG_WARN, "[DSP] read val %d PC=%x %s at %d",DSP_icr,m68k_getpc(),__FILE__,__LINE__);
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = 0;
}

void DSP_icr_Write (void) {
    DSP_icr=IoMem[IoAccessCurrentAddress & 0x1FFFF];
    Log_Printf(LOG_WARN, "[DSP] write val %d PC=%x %s at %d",DSP_icr,m68k_getpc(),__FILE__,__LINE__);
}


#define	P_VIDEO_CSR	(SLOT_ID+0x02000180)
#define	P_M2R_CSR	(SLOT_ID+0x020001d0)
#define	P_R2M_CSR	(SLOT_ID+0x020001c0)

/* DMA scratch pad (writes MUST be 32-bit) */
#define	P_VIDEO_SPAD	(SLOT_ID+0x02004180)
#define	P_EVENT_SPAD	(SLOT_ID+0x0200418c)
#define	P_M2M_SPAD	(SLOT_ID+0x020041e0)

/*-----------------------------------------------------------------------*/
/*
  List of functions to handle read/write hardware interceptions.
*/
const INTERCEPT_ACCESS_FUNC IoMemTable_NEXT[] =
{
/* DMA control/status (writes MUST be 32-bit) */
    	{ 0x02000010, SIZE_LONG, DMA_SCSI_CSR_Read, DMA_SCSI_CSR_Write },

    	{ 0x02000180, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    	{ 0x020001d0, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    	{ 0x020001c0, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },


	// blocking device?
	{ 0x02004350, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	// DSP 
	{ 0x02008000, SIZE_BYTE, DSP_icr_Read, DSP_icr_Write },


	{ 0x02000110, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02000111, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02000112, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02000113, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	{ 0x02000150, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02000151, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02000152, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02000153, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },


#define SERIAL_IO_WRITE IoMem_WriteWithoutInterception
#define SERIAL_IO_READ IoMem_ReadWithoutInterception
	{ 0x02004150, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE },
	{ 0x02004151, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE },
	{ 0x02004152, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE },
	{ 0x02004153, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE },
	{ 0x02004154, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE },
	{ 0x02004155, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE },
	{ 0x02004156, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE },
	{ 0x02004157, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE },
	{ 0x02004158, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE},
	{ 0x0200415c, SIZE_BYTE, SERIAL_IO_READ, SERIAL_IO_WRITE },


	{ 0x02004188, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02004189, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200418a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200418b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	// network adapter
	{ 0x02006000, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x02006001, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x02006002, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x02006003, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x02006004, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x02006005, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x02006006, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x02006008, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x02006009, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x0200600a, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x0200600b, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x0200600c, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x0200600d, SIZE_BYTE, Ethernet_read, Ethernet_write },
	{ 0x0200600e, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },

	{ 0x0200600f, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006010, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006011, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006012, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006013, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006014, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },


	{ 0x02007000, SIZE_LONG, IntRegStatRead, IntRegStatWrite },
	{ 0x02007800, SIZE_LONG, IntRegMaskRead, IntRegMaskWrite },


	// system control register 1
	{ 0x0200c000, SIZE_BYTE, SCR1_Read0, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c001, SIZE_BYTE, SCR1_Read1, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c002, SIZE_BYTE, SCR1_Read2, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c003, SIZE_BYTE, SCR1_Read3, IoMem_WriteWithoutInterceptionButTrace },


	{ 0x0200c800, SIZE_BYTE, SID_Read, IoMem_WriteWithoutInterceptionButTrace }, // Next cube slot Id
	{ 0x0200c801, SIZE_BYTE, SID_Read, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c802, SIZE_BYTE, SID_Read, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c803, SIZE_BYTE, SID_Read, IoMem_WriteWithoutInterceptionButTrace },


	// system control register 2
	{ 0x0200d000, SIZE_BYTE, SCR2_Read0, SCR2_Write0 },
	{ 0x0200d001, SIZE_BYTE, SCR2_Read1, SCR2_Write1 },
	{ 0x0200d002, SIZE_BYTE, SCR2_Read2, SCR2_Write2 },
	{ 0x0200d003, SIZE_BYTE, SCR2_Read3, SCR2_Write3 },

 	// monitor register (kbd + mouse + sound)
    	{ 0x0200e000, SIZE_BYTE, Keyboard_Read0, IoMem_WriteWithoutInterception },
	{ 0x0200e001, SIZE_BYTE, Keyboard_Read1, IoMem_WriteWithoutInterception },
	{ 0x0200e002, SIZE_BYTE, Keyboard_Read2, IoMem_WriteWithoutInterception },
    	{ 0x0200e003, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },

	{ 0x0200e004, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e005, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e006, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e007, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },

    	{ 0x0200e008, SIZE_LONG, Keycode_Read, IoMem_WriteWithoutInterceptionButTrace },

    
    	/* Event counter */
    	{ 0x0201a000, SIZE_BYTE, System_Timer0_Read, IoMem_WriteWithoutInterception },
    	{ 0x0201a001, SIZE_BYTE, System_Timer1_Read, IoMem_WriteWithoutInterception },
    	{ 0x0201a002, SIZE_BYTE, System_Timer2_Read, IoMem_WriteWithoutInterception },
    	{ 0x0201a003, SIZE_BYTE, System_Timer3_Read, IoMem_WriteWithoutInterception },


  	// internal hardclock

    	{ 0x02016000, SIZE_BYTE, HardclockRead0, HardclockWrite0 },
    	{ 0x02016001, SIZE_BYTE, HardclockRead1, HardclockWrite1 },
    	{ 0x02016004, SIZE_BYTE, HardclockReadCSR, HardclockWriteCSR },



    	{ 0x02010000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
  
    	/* MO-Drive Registers */
    	{ 0x02012000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012001, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012002, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012003, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012005, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012006, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012007, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012008, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012009, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201200a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201200b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201200c, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201200d, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201200e, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201200f, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

    /* Device Command/Status Registers */
    { 0x02014020, SIZE_BYTE, SCSI_CSR0_Read, SCSI_CSR0_Write },
    { 0x02014021, SIZE_BYTE, SCSI_CSR1_Read, SCSI_CSR1_Write },
    
    /* DMA SCSI */
    { 0x02004000, SIZE_LONG, DMA_SCSI_Saved_Next_Read, DMA_SCSI_Saved_Next_Write },
    { 0x02004004, SIZE_LONG, DMA_SCSI_Saved_Limit_Read, DMA_SCSI_Saved_Limit_Write },
    { 0x02004008, SIZE_LONG, DMA_SCSI_Saved_Start_Read, DMA_SCSI_Saved_Start_Write },
    { 0x0200400c, SIZE_LONG, DMA_SCSI_Saved_Stop_Read, DMA_SCSI_Saved_Stop_Write },
    { 0x02004010, SIZE_LONG, DMA_SCSI_Next_Read, DMA_SCSI_Next_Write },
    { 0x02004014, SIZE_LONG, DMA_SCSI_Limit_Read, DMA_SCSI_Limit_Write },
    { 0x02004018, SIZE_LONG, DMA_SCSI_Start_Read, DMA_SCSI_Start_Write },
    { 0x0200401c, SIZE_LONG, DMA_SCSI_Stop_Read, DMA_SCSI_Stop_Write },
    { 0x02004210, SIZE_LONG, DMA_SCSI_Init_Read, DMA_SCSI_Init_Write },
    { 0x02004214, SIZE_LONG, DMA_SCSI_Size_Read, DMA_SCSI_Size_Write },
        
    /* SCSI Registers for NCR53C90 (68030) */
    { 0x02014000, SIZE_BYTE, SCSI_TransCountL_Read, SCSI_TransCountL_Write },
    { 0x02014001, SIZE_BYTE, SCSI_TransCountH_Read, SCSI_TransCountH_Write },
    { 0x02014002, SIZE_BYTE, SCSI_FIFO_Read, SCSI_FIFO_Write },
    { 0x02014003, SIZE_BYTE, SCSI_Command_Read, SCSI_Command_Write },
    { 0x02014004, SIZE_BYTE, SCSI_Status_Read, SCSI_SelectBusID_Write },
    { 0x02014005, SIZE_BYTE, SCSI_IntStatus_Read, SCSI_SelectTimeout_Write },
    { 0x02014006, SIZE_BYTE, SCSI_SeqStep_Read, SCSI_SyncPeriod_Write },
    { 0x02014007, SIZE_BYTE, SCSI_FIFOflags_Read, SCSI_SyncOffset_Write },
    { 0x02014008, SIZE_BYTE, SCSI_Configuration_Read, SCSI_Configuration_Write },
    { 0x02014009, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, SCSI_ClockConv_Write },
    { 0x0201400a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, SCSI_Test_Write },
    /* additional Registers for NCR53C90A (68040) */
    { 0x0201400b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace},
//  { 0x0201400c, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400d, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400e, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400f, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
    
    /* Floppy 82077 */
    { 0x02014100, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014101, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014102, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014103, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014104, SIZE_BYTE, FDD_Main_Status_Read, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014105, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014106, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014107, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

    /* floppy external control */
    { 0x02014108, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

    
	{ 0x02018000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02018001, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02018004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0, 0, NULL, NULL }
};
