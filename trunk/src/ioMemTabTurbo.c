/*
 Previous - ioMemTabTurbo.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Table with hardware IO handlers for the NEXT.
 */


const char IoMemTabTurbo_fileid[] = "Previous ioMemTabTurbo.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "esp.h"
#include "ethernet.h"
#include "sysReg.h"
#include "dma.h"
#include "scc.h"
#include "mo.h"
#include "kms.h"
#include "printer.h"
#include "ramdac.h"
#include "floppy.h"
#include "dsp.h"



/*-----------------------------------------------------------------------*/
/*
 List of functions to handle read/write hardware interceptions.
 */
const INTERCEPT_ACCESS_FUNC IoMemTable_Turbo[] =
{
	/* DMA Controller (Motorola) (writes MUST be 32-bit) */
	{ 0x02000010, SIZE_LONG, TDMA_CSR_Read, TDMA_CSR_Write },
	{ 0x02000040, SIZE_LONG, TDMA_CSR_Read, TDMA_CSR_Write },
	{ 0x02000080, SIZE_LONG, TDMA_CSR_Read, TDMA_CSR_Write },
	{ 0x02000090, SIZE_LONG, TDMA_CSR_Read, TDMA_CSR_Write },
	{ 0x020000d0, SIZE_LONG, TDMA_CSR_Read, TDMA_CSR_Write },
	{ 0x02000110, SIZE_LONG, TDMA_CSR_Read, TDMA_CSR_Write },
	{ 0x02000150, SIZE_LONG, TDMA_CSR_Read, TDMA_CSR_Write },
	
	/* Channel SCSI */
	{ 0x02004010, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
	{ 0x02004014, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
	{ 0x02004018, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
	{ 0x0200401c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
	
	/* Channel Sound out */
	{ 0x02004040, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
	{ 0x02004044, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
	{ 0x02004048, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
	{ 0x0200404c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
	
	/* Ethernet Saved Next */
	{ 0x02004050, SIZE_LONG, TDMA_Saved_Next_Read, IoMem_WriteWithoutInterceptionButTrace },

	/* Channel Sound in */
	{ 0x02004080, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
	{ 0x02004084, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
	{ 0x02004088, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
	{ 0x0200408c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
	
	/* Channel Printer */
	{ 0x02004090, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
	{ 0x02004094, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
	{ 0x02004098, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
	{ 0x0200409c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
	
	/* Channel DSP */
	{ 0x020040d0, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
	{ 0x020040d4, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
	{ 0x020040d8, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
	{ 0x020040dc, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
	
	/* Channel Ethernet Transmit */
	{ 0x02004110, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
	{ 0x02004114, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
	{ 0x02004118, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
	{ 0x0200411c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
	
	/* Channel Ethernet Receive */
	{ 0x02004150, SIZE_LONG, DMA_Next_Read, DMA_Next_Write },
	{ 0x02004154, SIZE_LONG, DMA_Limit_Read, DMA_Limit_Write },
	{ 0x02004158, SIZE_LONG, DMA_Start_Read, DMA_Start_Write },
	{ 0x0200415c, SIZE_LONG, DMA_Stop_Read, DMA_Stop_Write },
	
	/* DMA Init */
	{ 0x02004210, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
	{ 0x02004240, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
	{ 0x02004280, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
	{ 0x02004290, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
	{ 0x020042d0, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
	{ 0x02004310, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
	{ 0x02004350, SIZE_LONG, DMA_Init_Read, DMA_Init_Write },
	
	/* Network Adapter (AT&T 7213) */
	{ 0x02006000, SIZE_BYTE, EN_TX_Status_Read, EN_TX_Status_Write },
	{ 0x02006001, SIZE_BYTE, EN_TX_Mask_Read, EN_TX_Mask_Write },
	{ 0x02006002, SIZE_BYTE, EN_RX_Status_Read, EN_RX_Status_Write },
	{ 0x02006003, SIZE_BYTE, EN_RX_Mask_Read, EN_RX_Mask_Write },
	{ 0x02006004, SIZE_BYTE, EN_TX_Mode_Read, EN_TX_Mode_Write },
	{ 0x02006005, SIZE_BYTE, EN_RX_Mode_Read, EN_RX_Mode_Write },
	{ 0x02006006, SIZE_BYTE, EN_Control_Read, EN_Control_Write },
	{ 0x02006007, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02006008, SIZE_BYTE, EN_NodeID0_Read, EN_NodeID0_Write },
	{ 0x02006009, SIZE_BYTE, EN_NodeID1_Read, EN_NodeID1_Write },
	{ 0x0200600a, SIZE_BYTE, EN_NodeID2_Read, EN_NodeID2_Write },
	{ 0x0200600b, SIZE_BYTE, EN_NodeID3_Read, EN_NodeID3_Write },
	{ 0x0200600c, SIZE_BYTE, EN_NodeID4_Read, EN_NodeID4_Write },
	{ 0x0200600d, SIZE_BYTE, EN_NodeID5_Read, EN_NodeID5_Write },
	{ 0x0200600e, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200600f, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	
	/* Interrupt Status and Mask Registers */
	{ 0x02007000, SIZE_LONG, IntRegStatRead, IntRegStatWrite },
	{ 0x02007800, SIZE_LONG, IntRegMaskRead, IntRegMaskWrite },
	
	/* DSP (Motorola XSP56001) */
	{ 0x02008000, SIZE_BYTE, DSP_ICR_Read, DSP_ICR_Write },
	{ 0x02008001, SIZE_BYTE, DSP_CVR_Read, DSP_CVR_Write },
	{ 0x02008002, SIZE_BYTE, DSP_ISR_Read, DSP_ISR_Write },
	{ 0x02008003, SIZE_BYTE, DSP_IVR_Read, DSP_IVR_Write },
	{ 0x02008004, SIZE_BYTE, DSP_Data0_Read, DSP_Data0_Write },
	{ 0x02008005, SIZE_BYTE, DSP_Data1_Read, DSP_Data1_Write },
	{ 0x02008006, SIZE_BYTE, DSP_Data2_Read, DSP_Data2_Write },
	{ 0x02008007, SIZE_BYTE, DSP_Data3_Read, DSP_Data3_Write },
	
	/* System Control Register 1 */
	{ 0x0200c000, SIZE_BYTE, SCR1_Read0, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c001, SIZE_BYTE, SCR1_Read1, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c002, SIZE_BYTE, SCR1_Read2, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c003, SIZE_BYTE, SCR1_Read3, IoMem_WriteWithoutInterceptionButTrace },
	
	/* System Control Register 2 */
	{ 0x0200d000, SIZE_BYTE, SCR2_Read0, SCR2_Write0 },
	{ 0x0200d001, SIZE_BYTE, SCR2_Read1, SCR2_Write1 },
	{ 0x0200d002, SIZE_BYTE, SCR2_Read2, SCR2_Write2 },
	{ 0x0200d003, SIZE_BYTE, SCR2_Read3, SCR2_Write3 },
	
	/* Monitor/Soundbox (Keyboard, Mouse, Sound) */
	{ 0x0200e000, SIZE_BYTE, KMS_Stat_Snd_Read, KMS_Ctrl_Snd_Write },
	{ 0x0200e001, SIZE_BYTE, KMS_Stat_KM_Read, KMS_Ctrl_KM_Write },
	{ 0x0200e002, SIZE_BYTE, KMS_Stat_TX_Read, KMS_Ctrl_TX_Write },
	{ 0x0200e003, SIZE_BYTE, KMS_Stat_Cmd_Read, KMS_Ctrl_Cmd_Write },
	{ 0x0200e004, SIZE_LONG, KMS_Data_Read, KMS_Data_Write },
	{ 0x0200e008, SIZE_LONG, KMS_KM_Data_Read, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200e00c, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	
	/* Printer */
    { 0x0200f000, SIZE_BYTE, LP_CSR0_Read, LP_CSR0_Write },
    { 0x0200f001, SIZE_BYTE, LP_CSR1_Read, LP_CSR1_Write },
    { 0x0200f002, SIZE_BYTE, LP_CSR2_Read, LP_CSR2_Write },
    { 0x0200f003, SIZE_BYTE, LP_CSR3_Read, LP_CSR3_Write },
    { 0x0200f004, SIZE_LONG, LP_Data_Read, LP_Data_Write },
	
	/* Brightness */
	{ 0x02010000, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	/* GPIO Register */
	{ 0x02012000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012001, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012002, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012003, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },

	/* SCSI Controller (NCR53C90A) */
	{ 0x02014000, SIZE_BYTE, ESP_TransCountL_Read, ESP_TransCountL_Write },
	{ 0x02014001, SIZE_BYTE, ESP_TransCountH_Read, ESP_TransCountH_Write },
	{ 0x02014002, SIZE_BYTE, ESP_FIFO_Read, ESP_FIFO_Write },
	{ 0x02014003, SIZE_BYTE, ESP_Command_Read, ESP_Command_Write },
	{ 0x02014004, SIZE_BYTE, ESP_Status_Read, ESP_SelectBusID_Write },
	{ 0x02014005, SIZE_BYTE, ESP_IntStatus_Read, ESP_SelectTimeout_Write },
	{ 0x02014006, SIZE_BYTE, ESP_SeqStep_Read, ESP_SyncPeriod_Write },
	{ 0x02014007, SIZE_BYTE, ESP_FIFOflags_Read, ESP_SyncOffset_Write },
	{ 0x02014008, SIZE_BYTE, ESP_Configuration_Read, ESP_Configuration_Write },
	{ 0x02014009, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, ESP_ClockConv_Write },
	{ 0x0201400a, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, ESP_Test_Write },
	{ 0x0201400b, SIZE_BYTE, ESP_Conf2_Read, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201400c, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201400d, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201400e, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201400f, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	
	/* SCSI DMA Control/Status Registers */
	{ 0x02014020, SIZE_BYTE, ESP_DMA_CTRL_Read, ESP_DMA_CTRL_Write },
	{ 0x02014021, SIZE_BYTE, ESP_DMA_FIFO_STAT_Read, ESP_DMA_FIFO_STAT_Write },
	
	/* Event Counter */
	{ 0x0201a000, SIZE_BYTE, System_Timer_Read, System_Timer_Write },
	{ 0x0201a001, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201a002, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201a003, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterceptionButTrace },
	
	/* Floppy Controller (Intel 82077AA) */
	{ 0x02014100, SIZE_BYTE, FLP_StatA_Read, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02014101, SIZE_BYTE, FLP_StatB_Read, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02014102, SIZE_BYTE, FLP_DataOut_Read, FLP_DataOut_Write },
	{ 0x02014103, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02014104, SIZE_BYTE, FLP_Status_Read, FLP_DataRate_Write },
	{ 0x02014105, SIZE_BYTE, FLP_FIFO_Read, FLP_FIFO_Write },
	{ 0x02014106, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02014107, SIZE_BYTE, FLP_DataIn_Read, FLP_Configuration_Write },
	
	/* Floppy External Control */
	{ 0x02014108, SIZE_BYTE, FLP_Control_Read, FLP_Select_Write },
	
	/* Internal Hardclock */
	{ 0x02016000, SIZE_BYTE, HardclockRead0, HardclockWrite0 },
	{ 0x02016001, SIZE_BYTE, HardclockRead1, HardclockWrite1 },
	{ 0x02016004, SIZE_BYTE, HardclockReadCSR, HardclockWriteCSR },
	
	/* Serial Communication Controller (AMD Z8530H) */
	{ 0x02018000, SIZE_BYTE, SCC_ControlB_Read, SCC_ControlB_Write },
	{ 0x02018001, SIZE_BYTE, SCC_ControlA_Read, SCC_ControlA_Write },
	{ 0x02018002, SIZE_BYTE, SCC_DataB_Read, SCC_DataB_Write },
	{ 0x02018003, SIZE_BYTE, SCC_DataA_Read, SCC_DataA_Write },
	/* Serial Interface Clock */
	{ 0x02018004, SIZE_LONG, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	
	/* RAMDAC (Brooktree Bt463) */
	{ 0x0201c000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201c001, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201c002, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0201c003, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	
	{ 0, 0, NULL, NULL }
};
