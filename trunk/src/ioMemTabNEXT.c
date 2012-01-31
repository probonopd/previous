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


void System_Timer0_Read(void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = clock() >> 24;
}

void System_Timer1_Read(void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = clock() >> 16;
}

void System_Timer2_Read(void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = clock() >> 8;
}

void System_Timer3_Read(void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = clock();
}


/* Floppy Disk Drive - Work on this later */
void FDD_Main_Status_Read (void) {
    IoMem[IoAccessCurrentAddress & 0x1FFFF] = 0x80;
}


/*-----------------------------------------------------------------------*/
/*
  List of functions to handle read/write hardware interceptions.
*/
const INTERCEPT_ACCESS_FUNC IoMemTable_NEXT[] =
{
	{ 0x02000150, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02004150, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02004154, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02004158, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200415c, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02004188, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02006000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006001, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006002, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006003, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006005, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006006, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006008, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006009, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x0200600a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x0200600b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x0200600a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x0200600b, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x0200600c, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x0200600d, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x0200600e, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x0200600f, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006010, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006011, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006012, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006013, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02006014, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_ReadWithoutInterceptionButTrace },
	{ 0x02007000, SIZE_LONG, IntRegStatRead, IntRegStatWrite },
	{ 0x02007800, SIZE_BYTE, IntRegMaskRead, IntRegMaskWrite },
	{ 0x0200c000, SIZE_BYTE, SCR1_Read0, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c001, SIZE_BYTE, SCR1_Read1, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c002, SIZE_BYTE, SCR1_Read2, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c003, SIZE_BYTE, SCR1_Read3, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c800, SIZE_BYTE, SID_Read, IoMem_WriteWithoutInterceptionButTrace }, // Next cube slot Id
	{ 0x0200d000, SIZE_BYTE, SCR2_Read0, SCR2_Write0 },
	{ 0x0200d001, SIZE_BYTE, SCR2_Read1, SCR2_Write1 },
	{ 0x0200d002, SIZE_BYTE, SCR2_Read2, SCR2_Write2 },
	{ 0x0200d003, SIZE_BYTE, SCR2_Read3, SCR2_Write3 },
    { 0x0200e000, SIZE_BYTE, Keyboard_Read0, IoMem_WriteWithoutInterception },
	{ 0x0200e001, SIZE_BYTE, Keyboard_Read1, IoMem_WriteWithoutInterception },
	{ 0x0200e002, SIZE_BYTE, Keyboard_Read2, IoMem_WriteWithoutInterception },
    { 0x0200e003, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e004, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e005, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200e006, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
    { 0x0200e008, SIZE_LONG, Keycode_Read, IoMem_WriteWithoutInterception },
	{ 0x02010000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    
    /* System Timer */
    { 0x0201a000, SIZE_BYTE, System_Timer0_Read, IoMem_WriteWithoutInterceptionButTrace },
    { 0x0201a001, SIZE_BYTE, System_Timer1_Read, IoMem_WriteWithoutInterceptionButTrace },
    { 0x0201a002, SIZE_BYTE, System_Timer2_Read, IoMem_WriteWithoutInterceptionButTrace },
    { 0x0201a003, SIZE_BYTE, System_Timer3_Read, IoMem_WriteWithoutInterceptionButTrace },
  
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
    { 0x02000010, SIZE_LONG, DMA_SCSI_CSR_Read, DMA_SCSI_CSR_Write },
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
//  { 0x0201400b, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400c, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400d, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400e, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
//  { 0x0201400f, SIZE_BYTE, SCSI_CMD_Read, SCSI_CMD_Write },
    
    /* Floppy */
    { 0x02014100, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014101, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014102, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014103, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014104, SIZE_BYTE, FDD_Main_Status_Read, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014105, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014106, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014107, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
    { 0x02014108, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

    
	{ 0x02018000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02018001, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02018004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0, 0, NULL, NULL }
};
