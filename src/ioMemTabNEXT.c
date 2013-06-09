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
#include "scc.h"
#include "mo.h"



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

/* Functions to be moved to other places */
void System_Timer_Read(void);
void FDD_Main_Status_Read(void);
void DSP_icr_Read(void);
void DSP_icr_Write(void);

Uint32 eventcounter; // debugging code
Uint32 lasteventc; // debugging code

void System_Timer_Read(void) { // tuned for power-on test
//    lasteventc = eventcounter; // debugging code
    if (ConfigureParams.System.nCpuLevel == 3) {
//        eventcounter = (nCyclesMainCounter/((128/ConfigureParams.System.nCpuFreq)*3))&0xFFFFF; // debugging code
        IoMem_WriteLong(IoAccessCurrentAddress&0x1FFFF, (nCyclesMainCounter/((128/ConfigureParams.System.nCpuFreq)*3))&0xFFFFF);
    } else { // System has 68040 CPU
//        eventcounter = (nCyclesMainCounter/((64/ConfigureParams.System.nCpuFreq)*9))&0xFFFFF; // debugging code
        IoMem_WriteLong(IoAccessCurrentAddress&0x1FFFF, (nCyclesMainCounter/((64/ConfigureParams.System.nCpuFreq)*9))&0xFFFFF);
    }
//    printf("DIFFERENCE = %i\n",eventcounter-lasteventc);
}

/* Floppy Disk Drive - Work on this later */
void FDD_Main_Status_Read (void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = 0x00;
}


static Uint32 DSP_icr=0;


/* DSP registers - Work on this later */
void DSP_icr_Read (void) {
    Log_Printf(LOG_WARN, "[DSP] read val %d PC=%x %s at %d",DSP_icr,m68k_getpc(),__FILE__,__LINE__);
    IoMem_WriteLong(IoAccessCurrentAddress & 0x1FFFF,0x7FFFFFFF);
}

void DSP_icr_Write (void) {
    DSP_icr=IoMem_ReadLong(IoAccessCurrentAddress & 0x1FFFF);
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
    /* These registers are read on turbo machines, they are somehow related to SCR1 */
    { 0x02000000, SIZE_WORD, TurboSCR1_Read0, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02000002, SIZE_WORD, TurboSCR1_Read2, IoMem_WriteWithoutInterceptionButTrace },

	{ 0x02008008, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008009, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200800a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200800b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	{ 0x02008018, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008019, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200801a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200801b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	{ 0x02008020, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008021, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008022, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008023, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	{ 0x02008028, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008029, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200802a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200802b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	{ 0x02008030, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008031, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008032, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008033, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	{ 0x02008038, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02008039, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200803a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200803b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	{ 0x02000088, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02000089, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200008a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200008b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	{ 0x0200008c, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200008d, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200008e, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200008f, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

    
    /* Brightness */
    { 0x02010000, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    
    // this is for video DMA channel
	{ 0x02004188, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02004189, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200418a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200418b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

    
    /* DSP (Motorola XSP56001) */
	{ 0x02008000, SIZE_LONG, DSP_icr_Read, DSP_icr_Write },

	/* Network Adapter (MB8795) */
	{ 0x02006000, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x02006001, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x02006002, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x02006003, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x02006004, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x02006005, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x02006006, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x02006008, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x02006009, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x0200600a, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x0200600b, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x0200600c, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x0200600d, SIZE_BYTE, Ethernet_Read, Ethernet_Write },
	{ 0x0200600e, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200600f, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    
    /* Memory Timing */
	{ 0x02006010, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02006011, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02006012, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02006013, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02006014, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    
    /* Interrupt Status and Mask Registers */
	{ 0x02007000, SIZE_LONG, IntRegStatRead, IntRegStatWrite },
	{ 0x02007800, SIZE_LONG, IntRegMaskRead, IntRegMaskWrite },

	/* System Control Register 1 */
	{ 0x0200c000, SIZE_BYTE, SCR1_Read0, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c001, SIZE_BYTE, SCR1_Read1, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c002, SIZE_BYTE, SCR1_Read2, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c003, SIZE_BYTE, SCR1_Read3, IoMem_WriteWithoutInterceptionButTrace },

    /* Slot ID (NeXT Cube) */
	{ 0x0200c800, SIZE_BYTE, SID_Read, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c801, SIZE_BYTE, SID_Read, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c802, SIZE_BYTE, SID_Read, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c803, SIZE_BYTE, SID_Read, IoMem_WriteWithoutInterceptionButTrace },

	/* System Control Register 2 */
	{ 0x0200d000, SIZE_BYTE, SCR2_Read0, SCR2_Write0 },
	{ 0x0200d001, SIZE_BYTE, SCR2_Read1, SCR2_Write1 },
	{ 0x0200d002, SIZE_BYTE, SCR2_Read2, SCR2_Write2 },
	{ 0x0200d003, SIZE_BYTE, SCR2_Read3, SCR2_Write3 },

 	/* Monitor Registers (Keyboard, Mouse, Sound) */
    { 0x0200e000, SIZE_BYTE, Monitor_CSR_Read, Monitor_CSR_Write },
	{ 0x0200e001, SIZE_BYTE, Monitor_CSR_Read, Monitor_CSR_Write },
	{ 0x0200e002, SIZE_BYTE, Monitor_CSR_Read, Monitor_CSR_Write },
    { 0x0200e003, SIZE_BYTE, Monitor_CSR_Read, Monitor_CSR_Write },
	{ 0x0200e004, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e005, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e006, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e007, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
    { 0x0200e008, SIZE_LONG, Keycode_Read, IoMem_WriteWithoutInterceptionButTrace },

    /* Printer Register */
    { 0x0200f000, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    
    /* Event counter */
    { 0x0201a000, SIZE_BYTE, System_Timer_Read, IoMem_WriteWithoutInterceptionButTrace },
    { 0x0201a001, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterceptionButTrace },
    { 0x0201a002, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterceptionButTrace },
    { 0x0201a003, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterceptionButTrace },
    	
  	/* Internal Hardclock */
    { 0x02016000, SIZE_BYTE, HardclockRead0, HardclockWrite0 },
    { 0x02016001, SIZE_BYTE, HardclockRead1, HardclockWrite1 },
    { 0x02016004, SIZE_BYTE, HardclockReadCSR, HardclockWriteCSR },

  
    /* Magneto-Optical Drive */
#if 0 /* temporary disabled */
    { 0x02012000, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012001, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012002, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012003, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012004, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012005, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012006, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012007, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012008, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
    { 0x02012009, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x0201200a, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x0201200b, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x0201200c, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x0201200d, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x0201200e, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x0201200f, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012010, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012011, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012012, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012013, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012014, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012015, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
	{ 0x02012016, SIZE_BYTE, MOdrive_Read, MOdrive_Write },
#else
    { 0x02012000, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012001, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012002, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012003, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012004, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012005, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012006, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012007, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012008, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
    { 0x02012009, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0201200a, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0201200b, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0201200c, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0201200d, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0201200e, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0201200f, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012010, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012011, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012012, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012013, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012014, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012015, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02012016, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
#endif

    
    /*------------------ DMA ------------------*/
    
    /* DMA Control/Status Registers (writes MUST be 32-bit) */
    { 0x02000010, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    { 0x02000040, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    { 0x02000050, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    { 0x02000080, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    { 0x02000090, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    { 0x020000c0, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    { 0x020000d0, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    { 0x02000110, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    { 0x02000150, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    
    { 0x02000180, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },    
    { 0x020001d0, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    { 0x020001c0, SIZE_LONG, DMA_CSR_Read, DMA_CSR_Write },
    
    /* Channel R2M */
    { 0x020041c0, SIZE_LONG, DMA_Saved_Next_Read, DMA_Saved_Next_Write },
    { 0x020041c4, SIZE_LONG, DMA_Saved_Limit_Read, DMA_Saved_Limit_Write },
    { 0x020041c8, SIZE_LONG, DMA_Saved_Start_Read, DMA_Saved_Start_Write },
    { 0x020041cc, SIZE_LONG, DMA_Saved_Stop_Read, DMA_Saved_Stop_Write },
    { 0x020043c0, SIZE_LONG, DMA_Init_Read, DMA_Init_Write }, // check this!
    { 0x020043c4, SIZE_LONG, DMA_Size_Read, DMA_Size_Write }, // check this!
    
    /* Channel M2R */
    { 0x020041d0, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
    { 0x020041d4, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
    { 0x020041d8, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
    { 0x020041dc, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
    { 0x020043d0, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
    { 0x020043d4, SIZE_LONG, DMA_Size_Read, DMA_Size_Write },
    
    /* Channel SCSI */
    { 0x02004000, SIZE_LONG, DMA_Saved_Next_Read, DMA_Saved_Next_Write },
    { 0x02004004, SIZE_LONG, DMA_Saved_Limit_Read, DMA_Saved_Limit_Write },
    { 0x02004008, SIZE_LONG, DMA_Saved_Start_Read, DMA_Saved_Start_Write },
    { 0x0200400c, SIZE_LONG, DMA_Saved_Stop_Read, DMA_Saved_Stop_Write },
    { 0x02004010, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
    { 0x02004014, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
    { 0x02004018, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
    { 0x0200401c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
    { 0x02004210, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
    { 0x02004214, SIZE_LONG, DMA_Size_Read, DMA_Size_Write },
    
    /* Channel Sound out */
    { 0x02004040, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
    { 0x02004044, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
    { 0x02004048, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
    { 0x0200404c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },

    /* Channel MO Drive */
    { 0x02004050, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
    { 0x02004054, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
    { 0x02004058, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
    { 0x0200405c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
    { 0x02004250, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
    { 0x02004254, SIZE_LONG, DMA_Size_Read, DMA_Size_Write },
    
    /* Channel SCC */
    { 0x020040c0, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
    { 0x020040c4, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
//    { 0x020040c8, SIZE_LONG, DMA_Saved_Start_Read, DMA_Saved_Start_Write },
//    { 0x020040cc, SIZE_LONG, DMA_Saved_Stop_Read, DMA_Saved_Stop_Write },
    
//    { 0x020040d0, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
//    { 0x020040d4, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
//    { 0x020040d8, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
//    { 0x020040dc, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
//    { 0x020042d0, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
//    { 0x020042d4, SIZE_LONG, DMA_Size_Read, DMA_Size_Write },
    
    /* Channel Ethernet Transmit */
    { 0x02004100, SIZE_LONG, DMA_Saved_Next_Read, DMA_Saved_Next_Write },
    { 0x02004104, SIZE_LONG, DMA_Saved_Limit_Read, DMA_Saved_Limit_Write },
    { 0x02004108, SIZE_LONG, DMA_Saved_Start_Read, DMA_Saved_Start_Write },
    { 0x0200410c, SIZE_LONG, DMA_Saved_Stop_Read, DMA_Saved_Stop_Write },
    { 0x02004110, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
    { 0x02004114, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
    { 0x02004118, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
    { 0x0200411c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
    { 0x02004310, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
    { 0x02004314, SIZE_LONG, DMA_Size_Read, DMA_Size_Write },
    
    /* Channel Ethernet Receive */
    { 0x02004140, SIZE_LONG, DMA_Saved_Next_Read, DMA_Saved_Next_Write },
    { 0x02004144, SIZE_LONG, DMA_Saved_Limit_Read, DMA_Saved_Limit_Write },
    { 0x02004148, SIZE_LONG, DMA_Saved_Start_Read, DMA_Saved_Start_Write },
    { 0x0200414c, SIZE_LONG, DMA_Saved_Stop_Read, DMA_Saved_Stop_Write },
    { 0x02004150, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
    { 0x02004154, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
    { 0x02004158, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
    { 0x0200415c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
    { 0x02004350, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
    { 0x02004354, SIZE_LONG, DMA_Size_Read, DMA_Size_Write },
    /*-------------------- End of DMA -------------------*/

        
    /* SCSI Command/Status Registers */
    { 0x02014020, SIZE_BYTE, SCSI_CSR0_Read, SCSI_CSR0_Write },
    { 0x02014021, SIZE_BYTE, SCSI_CSR1_Read, SCSI_CSR1_Write },

    /* SCSI Controller (NCR53C90) */
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
    /* Additional Register for NCR53C90A (68040) */
    { 0x0201400b, SIZE_BYTE, SCSI_Conf2_Read, IoMem_WriteWithoutInterceptionButTrace },
//  { 0x0201400c, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400d, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400e, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400f, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
    
    /* Floppy Controller (82077AA) */
    { 0x02014100, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014101, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014102, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014103, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014104, SIZE_BYTE, FDD_Main_Status_Read, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014105, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014106, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014107, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

    /* Floppy External Control */
    { 0x02014108, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

    /* Serial Communication Controller (Z8530) */
	{ 0x02018000, SIZE_BYTE, SCC_Read, SCC_Write },
	{ 0x02018001, SIZE_BYTE, SCC_Read, SCC_Write },
    { 0x02018002, SIZE_BYTE, SCC_Read, SCC_Write },
	{ 0x02018003, SIZE_BYTE, SCC_Read, SCC_Write },
	{ 0x02018004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0, 0, NULL, NULL }
};
