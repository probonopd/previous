/* SCSI Bus and Disk emulation */

#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "sysReg.h"
#include "statusbar.h"
#include "scsi.h"
#include "dma.h"
#include "esp.h"


#define COMMAND_ReadInt16(a, i) (((unsigned) a[i] << 8) | a[i + 1])
#define COMMAND_ReadInt24(a, i) (((unsigned) a[i] << 16) | ((unsigned) a[i + 1] << 8) | a[i + 2])
#define COMMAND_ReadInt32(a, i) (((unsigned) a[i] << 24) | ((unsigned) a[i + 1] << 16) | ((unsigned) a[i + 2] << 8) | a[i + 3])


#define BLOCKSIZE 512 // always correct?
int nPartitions = 0;
unsigned long hdSize = 0;
short int HDCSectorCount;
bool bAcsiEmuOn = false;

static FILE *hd_image_file = NULL;
static Uint32 nLastBlockAddr;
static bool bSetLastBlockAddr;
static Uint8 nLastError;


/* Image input experiment ----------------------------------------*/

typedef struct {
    Uint32 filesize;
} SCSIHDINFO;

SCSIHDINFO scsihd0;
FILE* scsi0;

void read_image(void);
void read_image(void) {
//    scsi0 = fopen("./hdd-001.dd","r");
    scsi0 = fopen("./2.2_Boot_Disk.dd","r");
//    scsi0 = fopen("./3.3_Moto_Boot_Disk.floppyimage","r");
//    scsi0 = fopen("./hdd-027.dd","r");

    fseek(scsi0, 0L, SEEK_END);
    scsihd0.filesize = ftell(scsi0);
    Log_Printf(LOG_WARN, "Read disk image: size = %i\n", scsihd0.filesize);
}

/* ----------------------------------------------------------------*/




/* INQUIRY response data */
static unsigned char inquiry_bytes[] =
{
	0x00,             /* 0: device type: 0x00 = direct access device, 0x05 = cd-rom, 0x07 = mo-disk */
	0x00,             /* 1: &0x7F - device type qulifier 0x00 unsupported, &0x80 - rmb: 0x00 = nonremovable, 0x80 = removable */
	0x01,             /* 2: ANSI SCSI standard (first release) compliant */
    0x01,             /* 3: Restponse format (format of following data): 0x01 SCSI-1 compliant */
	0x31,             /* 4: additional length of the following data */
    0x00, 0x00,       /* 5,6: reserved */
    0x1C,             /* 7: RelAdr=0, Wbus32=0, Wbus16=0, Sync=1, Linked=1, RSVD=1, CmdQue=0, SftRe=0 */
	'P','r','e','v','i','o','u','s',        /*  8-15: Vendor ASCII */
	'H','D','D',' ',' ',' ',' ',' ',        /* 16-23: Model ASCII */
    ' ',' ',' ',' ',' ',' ',' ',' ',        /* 24-31: Blank space ASCII */
    '0','0','0','0','0','0','0','1',        /* 32-39: Revision ASCII */
	'0','0','0','0','0','0','0','0',        /* 40-47: Serial Number ASCII */
    ' ',' ',' ',' ',' ',' '                 /* 48-53: Blank space ASCII */
};




void scsi_command_analyzer(Uint8 commandbuf[], int size, int target) {
    int i;
    SCSICommandBlock.source_busid = commandbuf[0];
    for (i = 1; i < size; i++) {
        SCSICommandBlock.command[i-1] = commandbuf[i];
    }
    //memcpy(SCSICommandBlock.command, commandbuf, size);
    SCSICommandBlock.opcode = SCSICommandBlock.command[0];
    SCSICommandBlock.target = target;
    Log_Printf(LOG_WARN, "SCSI command: Length = %i, Opcode = $%02x, target = %i\n", size, SCSICommandBlock.opcode, SCSICommandBlock.target);
    SCSI_Emulate_Command();
}

void SCSI_Emulate_Command(void)
{
    
	switch(SCSICommandBlock.opcode)
	{
            
        case HD_TEST_UNIT_RDY:
            Log_Printf(LOG_WARN, "SCSI command: Test unit ready\n");
            SCSI_TestUnitReady();
//            HDC_Cmd_TestUnitReady();
            break;
            
        case HD_READ_CAPACITY1:
            Log_Printf(LOG_WARN, "SCSI command: Read capacity\n");
            SCSI_ReadCapacity();
//            HDC_Cmd_ReadCapacity();
            break;
            
        case HD_READ_SECTOR:
        case HD_READ_SECTOR1:
            Log_Printf(LOG_WARN, "SCSI command: Read sector\n");
            SCSI_ReadSector();
//            HDC_Cmd_ReadSector();
            break;
            
        case HD_WRITE_SECTOR:
        case HD_WRITE_SECTOR1:
            Log_Printf(LOG_WARN, "SCSI command: Write sector\n");
//            HDC_Cmd_WriteSector();
            break;
            
        case HD_INQUIRY:
            Log_Printf(LOG_WARN, "SCSI command: Inquiry\n");
            SCSI_Inquiry();
//            HDC_Cmd_Inquiry();
            break;
            
        case HD_SEEK:
            Log_Printf(LOG_WARN, "SCSI command: Seek\n");
//            HDC_Cmd_Seek();
            break;
            
        case HD_SHIP:
            Log_Printf(LOG_WARN, "SCSI command: Ship\n");
            SCSICommandBlock.transfer_data_len = 0;
            SCSICommandBlock.transferdirection_todevice = 0;
            SCSICommandBlock.returnCode = HD_STATUS_OK;
//            FDC_AcknowledgeInterrupt();
            break;
            
        case HD_REQ_SENSE:
            Log_Printf(LOG_WARN, "SCSI command: Request sense\n");
            SCSI_RequestSense();
//            HDC_Cmd_RequestSense();
            break;
            
        case HD_MODESELECT:
            Log_Printf(LOG_WARN, "MODE SELECT call not implemented yet.\n");
            SCSICommandBlock.returnCode = HD_STATUS_OK;
            nLastError = HD_REQSENS_OK;
            bSetLastBlockAddr = false;
//            FDC_SetDMAStatus(false);
//            FDC_AcknowledgeInterrupt();
            break;
            
        case HD_MODESENSE:
            Log_Printf(LOG_WARN, "SCSI command: Mode sense\n");
//            HDC_Cmd_ModeSense();
            break;
            
        case HD_FORMAT_DRIVE:
            Log_Printf(LOG_WARN, "SCSI command: Format drive\n");
//            HDC_Cmd_FormatDrive();
            break;
            
            /* as of yet unsupported commands */
        case HD_VERIFY_TRACK:
        case HD_FORMAT_TRACK:
        case HD_CORRECTION:
            
        default:
            Log_Printf(LOG_WARN, "Unknown Command\n");
            SCSICommandBlock.returnCode = HD_STATUS_ERROR;
            nLastError = HD_REQSENS_OPCODE;
            bSetLastBlockAddr = false;
//            FDC_AcknowledgeInterrupt();
            break;
	}
    esp_command_complete();
    
    /* Update the led each time a command is processed */
	Statusbar_EnableHDLed();
}



int SCSI_GetTransferLength(void)
{
	return SCSICommandBlock.opcode < 0x20?
    // class 0
    SCSICommandBlock.command[4] :
    // class1
    COMMAND_ReadInt16(SCSICommandBlock.command, 7);
}

unsigned long SCSI_GetOffset(void)
{
	/* offset = logical block address * 512 */
	return SCSICommandBlock.opcode < 0x20?
    // class 0
    (COMMAND_ReadInt24(SCSICommandBlock.command, 1) & 0x1FFFFF) :
    // class 1
    COMMAND_ReadInt32(SCSICommandBlock.command, 2);
}

int SCSI_GetCount(void)
{
	return SCSICommandBlock.opcode < 0x20?
    // class 0
    SCSICommandBlock.command[4] :
    // class1
    COMMAND_ReadInt16(SCSICommandBlock.command, 7);
}



/* SCSI Commands */


void SCSI_TestUnitReady(void)
{
//	FDC_SetDMAStatus(false);            /* no DMA error */
//	FDC_AcknowledgeInterrupt();
    SCSICommandBlock.transfer_data_len = 0;
	SCSICommandBlock.returnCode = HD_STATUS_OK;
//    esp_command_complete();
}


void SCSI_ReadCapacity(void)
{
//	Uint32 nDmaAddr = FDC_GetDMAAddress();
    
    SCSICommandBlock.transfer_data_len = 8;
  
    read_image(); // experimental, work on this later!
    Uint32 sectors = scsihd0.filesize / BLOCKSIZE;
    
    static Uint8 scsi_disksize[8];

    scsi_disksize[0] = (sectors >> 24) & 0xFF;
    scsi_disksize[1] = (sectors >> 16) & 0xFF;
    scsi_disksize[2] = (sectors >> 8) & 0xFF;
    scsi_disksize[3] = sectors & 0xFF;
    scsi_disksize[4] = (BLOCKSIZE >> 24) & 0xFF;
    scsi_disksize[5] = (BLOCKSIZE >> 16) & 0xFF;
    scsi_disksize[6] = (BLOCKSIZE >> 8) & 0xFF;
    scsi_disksize[7] = BLOCKSIZE & 0xFF;
    
    memcpy(dma_write_buffer, scsi_disksize, SCSICommandBlock.transfer_data_len);
		SCSICommandBlock.returnCode = HD_STATUS_OK;
		nLastError = HD_REQSENS_OK;
/*	}
	else
	{
		Log_Printf(LOG_WARN, "HDC capacity read uses invalid RAM range 0x%x+%i\n", nDmaAddr, 8);
		HDCCommand.returnCode = HD_STATUS_ERROR;
		nLastError = HD_REQSENS_NOSECTOR;
	}*/
    
//	FDC_SetDMAStatus(false);              /* no DMA error */
//	FDC_AcknowledgeInterrupt();
	bSetLastBlockAddr = false;
	//FDCSectorCountRegister = 0;
//    esp_command_complete();
}


void SCSI_ReadSector(void)
{
	int n;
    
	nLastBlockAddr = SCSI_GetOffset() * BLOCKSIZE;
    SCSICommandBlock.transfer_data_len = SCSI_GetCount() * BLOCKSIZE;
    
    
	/* seek to the position */
	if (fseek(scsi0, nLastBlockAddr, SEEK_SET) != 0)
	{
        SCSICommandBlock.returnCode = HD_STATUS_ERROR;
        nLastError = HD_REQSENS_INVADDR;
	}
	else
	{
//		Uint32 nDmaAddr = FDC_GetDMAAddress();
//		if (STMemory_ValidArea(nDmaAddr, 512*HDC_GetCount()))
//		{
			n = fread(dma_write_buffer, SCSICommandBlock.transfer_data_len, 1, scsi0);
        
        /* Test to check if we read correct data */
//        Log_Printf(LOG_WARN, "Disk Read Test: $%02x,$%02x,$%02x,$%02x,$%02x,$%02x,$%02x,$%02x\n", dma_write_buffer[0],dma_write_buffer[1],dma_write_buffer[2],dma_write_buffer[3],dma_write_buffer[4],dma_write_buffer[5],dma_write_buffer[6],dma_write_buffer[07]);
//		}
//		else
//		{
//			Log_Printf(LOG_WARN, "HDC sector read uses invalid RAM range 0x%x+%i\n",
//                       nDmaAddr, 512*HDC_GetCount());
//			n = 0;
		}

    if (n == 1) {
			SCSICommandBlock.returnCode = HD_STATUS_OK;
			nLastError = HD_REQSENS_OK;
		}
		else
		{
			SCSICommandBlock.returnCode = HD_STATUS_ERROR;
			nLastError = HD_REQSENS_NOSECTOR;
		}
        
		/* Update DMA counter */
//		FDC_WriteDMAAddress(nDmaAddr + 512*n);
//  }
    
//	FDC_SetDMAStatus(false);              /* no DMA error */
//	FDC_AcknowledgeInterrupt();
	bSetLastBlockAddr = true;
	//FDCSectorCountRegister = 0;
//    esp_command_complete();
}



void SCSI_Inquiry (void) {  //conversion in progress
//    Uint32 nDmaAddr;
    SCSICommandBlock.transfer_data_len = SCSI_GetTransferLength();
    Log_Printf(LOG_WARN, "return length: %d", SCSICommandBlock.transfer_data_len);
    SCSICommandBlock.transferdirection_todevice = 0;
    memcpy(dma_write_buffer, inquiry_bytes, SCSICommandBlock.transfer_data_len);
    
    Log_Printf(LOG_WARN, "Inquiry Data: %c,%c,%c,%c,%c,%c,%c,%c\n",dma_write_buffer[8],dma_write_buffer[9],dma_write_buffer[10],dma_write_buffer[11],dma_write_buffer[12],dma_write_buffer[13],dma_write_buffer[14],dma_write_buffer[15]);
        
//	nDmaAddr = FDC_GetDMAAddress();
        
	if (SCSICommandBlock.transfer_data_len > (int)sizeof(inquiry_bytes))
		SCSICommandBlock.transfer_data_len = sizeof(inquiry_bytes);
    
//	inquiry_bytes[4] = return_length - 8;
    
//	if (NEXTMemory_SafeCopy(nDmaAddr, inquiry_bytes, count, "HDC DMA inquiry"))
		SCSICommandBlock.returnCode = HD_STATUS_OK;
//	else
//		SCSICommandBlock.returnCode = HD_STATUS_ERROR;
    
//	FDC_WriteDMAAddress(nDmaAddr + count);
    
//	FDC_SetDMAStatus(false);              /* no DMA error */
//	FDC_AcknowledgeInterrupt();
	nLastError = HD_REQSENS_OK;
	bSetLastBlockAddr = false;
//    esp_command_complete();
}


void SCSI_RequestSense(void)
{
//	Uint32 nDmaAddr;
	int nRetLen;
	Uint8 retbuf[22];
    
#ifdef HDC_VERBOSE
	fprintf(stderr,"HDC: Request Sense.\n");
#endif
    
	nRetLen = SCSI_GetCount();
    
	if ((nRetLen < 4 && nRetLen != 0) || nRetLen > 22)
	{
		Log_Printf(LOG_WARN, "SCSI: *** Strange REQUEST SENSE ***!\n");
	}
    
	/* Limit to sane length */
	if (nRetLen <= 0)
	{
		nRetLen = 4;
	}
	else if (nRetLen > 22)
	{
		nRetLen = 22;
	}
    
//	nDmaAddr = FDC_GetDMAAddress();
    
	memset(retbuf, 0, nRetLen);
    
	if (nRetLen <= 4)
	{
		retbuf[0] = nLastError;
		if (bSetLastBlockAddr)
		{
			retbuf[0] |= 0x80;
			retbuf[1] = nLastBlockAddr >> 16;
			retbuf[2] = nLastBlockAddr >> 8;
			retbuf[3] = nLastBlockAddr;
		}
	}
	else
	{
		retbuf[0] = 0x70;
		if (bSetLastBlockAddr)
		{
			retbuf[0] |= 0x80;
			retbuf[4] = nLastBlockAddr >> 16;
			retbuf[5] = nLastBlockAddr >> 8;
			retbuf[6] = nLastBlockAddr;
		}
		switch (nLastError)
		{
            case HD_REQSENS_OK:  retbuf[2] = 0; break;
            case HD_REQSENS_OPCODE:  retbuf[2] = 5; break;
            case HD_REQSENS_INVADDR:  retbuf[2] = 5; break;
            case HD_REQSENS_INVARG:  retbuf[2] = 5; break;
            case HD_REQSENS_NODRIVE:  retbuf[2] = 2; break;
            default: retbuf[2] = 0; break;
//            default: retbuf[2] = 4; break;
		}
		retbuf[7] = 14;
		retbuf[12] = nLastError;
		retbuf[19] = nLastBlockAddr >> 16;
		retbuf[20] = nLastBlockAddr >> 8;
		retbuf[21] = nLastBlockAddr;
	}
    
    SCSICommandBlock.transfer_data_len = nRetLen;
    memcpy(dma_write_buffer, retbuf, SCSICommandBlock.transfer_data_len);
    SCSICommandBlock.returnCode = HD_STATUS_OK;
    
	/*
     fprintf(stderr,"*** Requested sense packet:\n");
     int i;
     for (i = 0; i<nRetLen; i++) fprintf(stderr,"%2x ",retbuf[i]);
     fprintf(stderr,"\n");
     */
    
//	if (STMemory_SafeCopy(nDmaAddr, retbuf, nRetLen, "HDC request sense"))
//		HDCCommand.returnCode = HD_STATUS_OK;
//	else
//		HDCCommand.returnCode = HD_STATUS_ERROR;
    
//	FDC_WriteDMAAddress(nDmaAddr + nRetLen);
    
//	FDC_SetDMAStatus(false);            /* no DMA error */
//	FDC_AcknowledgeInterrupt();
	//FDCSectorCountRegister = 0;
}
