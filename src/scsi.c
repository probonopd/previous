/* SCSI Bus and Disk emulation */
#include <sys/stat.h>

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


#define BLOCKSIZE 512

#define LUN_DISC 0 // for now only LUN 0 is valid for our phys drives

static Uint32 nLastBlockAddr;
static bool bSetLastBlockAddr;
static Uint8 nLastError;

/* Disk Infos */

bool bCDROM;
bool bTargetDevice;


/*typedef struct {
    Uint32 filesize;
    bool cdrom;
} SCSIHDINFO;

SCSIHDINFO scsiimage;

SCSIHDINFO scsiimage0;
SCSIHDINFO scsiimage1;
SCSIHDINFO scsiimage2;
SCSIHDINFO scsiimage3;
SCSIHDINFO scsiimage4;
SCSIHDINFO scsiimage5;
SCSIHDINFO scsiimage6;*/

static FILE *scsidisk = NULL;

FILE* scsidisk0;
FILE* scsidisk1;
FILE* scsidisk2;
FILE* scsidisk3;
FILE* scsidisk4;
FILE* scsidisk5;
FILE* scsidisk6;


/* Initialize/Uninitialize SCSI disks */
void SCSI_Init(void) {
    struct stat st;
    Log_Printf(LOG_WARN, "CALL SCSI INIT\n");
    char *filename0 = ConfigureParams.HardDisk.szSCSIDiskImage0;
    char *filename1 = ConfigureParams.HardDisk.szSCSIDiskImage1;
    char *filename2 = ConfigureParams.HardDisk.szSCSIDiskImage2;
    char *filename3 = ConfigureParams.HardDisk.szSCSIDiskImage3;
    char *filename4 = ConfigureParams.HardDisk.szSCSIDiskImage4;
    char *filename5 = ConfigureParams.HardDisk.szSCSIDiskImage5;
    char *filename6 = ConfigureParams.HardDisk.szSCSIDiskImage6;

    stat(filename0,&st);
    if (S_ISREG(st.st_mode))
	scsidisk0 = ConfigureParams.HardDisk.bCDROM0 == true ? fopen(filename0, "r") : fopen(filename0, "r+");
    else
	scsidisk0=NULL;

    stat(filename1,&st);
    if (S_ISREG(st.st_mode))

	scsidisk1 = ConfigureParams.HardDisk.bCDROM1 == true ? fopen(filename1, "r") : fopen(filename1, "r+");
    else
	scsidisk1=NULL;

    stat(filename2,&st);
    if (S_ISREG(st.st_mode))

	scsidisk2 = ConfigureParams.HardDisk.bCDROM2 == true ? fopen(filename2, "r") : fopen(filename2, "r+");
    else
	scsidisk2=NULL;

    stat(filename3,&st);
    if (S_ISREG(st.st_mode))
	scsidisk3 = ConfigureParams.HardDisk.bCDROM3 == true ? fopen(filename3, "r") : fopen(filename3, "r+");
    else
	scsidisk3=NULL;

    stat(filename4,&st);
    if (S_ISREG(st.st_mode))
	scsidisk4 = ConfigureParams.HardDisk.bCDROM4 == true ? fopen(filename4, "r") : fopen(filename4, "r+");
    else
	scsidisk4=NULL;

    stat(filename5,&st);
    if (S_ISREG(st.st_mode))
	scsidisk5 = ConfigureParams.HardDisk.bCDROM5 == true ? fopen(filename5, "r") : fopen(filename5, "r+");
    else
	scsidisk5=NULL;

    stat(filename6,&st);
    if (S_ISREG(st.st_mode))
	scsidisk6 = ConfigureParams.HardDisk.bCDROM6 == true ? fopen(filename6, "r") : fopen(filename6, "r+");
    else
	scsidisk6=NULL;
    
//  TODO: Better get disksize here or in SCSI_ReadCapacity?
    
    Log_Printf(LOG_WARN, "Disk0: %s\n", filename0);
    Log_Printf(LOG_WARN, "Disk1: %s\n", filename1);
    Log_Printf(LOG_WARN, "Disk2: %s\n", filename2);
    Log_Printf(LOG_WARN, "Disk3: %s\n", filename3);
    Log_Printf(LOG_WARN, "Disk4: %s\n", filename4);
    Log_Printf(LOG_WARN, "Disk5: %s\n", filename5);
    Log_Printf(LOG_WARN, "Disk6: %s\n", filename6);
}

void SCSI_Uninit(void) {
	if (scsidisk0)
    		fclose(scsidisk0);
	if (scsidisk1)
    		fclose(scsidisk1);
	if (scsidisk2)
    		fclose(scsidisk2);
	if (scsidisk3)
    		fclose(scsidisk3);
	if (scsidisk4)
    		fclose(scsidisk4);
	if (scsidisk5)
    		fclose(scsidisk5);
	if (scsidisk6)
    		fclose(scsidisk6);
    
    scsidisk = NULL;
}



/* INQUIRY response data */
static unsigned char inquiry_bytes[] =
{
	0x00,             /* 0: device type: 0x00 = direct access device, 0x05 = cd-rom, 0x07 = mo-disk */
	0x00,             /* 1: &0x7F - device type qulifier 0x00 unsupported, &0x80 - rmb: 0x00 = nonremovable, 0x80 = removable */
	0x01,             /* 2: ANSI SCSI standard (first release) compliant */
    0x02,             /* 3: Restponse format (format of following data): 0x01 SCSI-1 compliant */
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



void scsi_command_analyzer(Uint8 commandbuf[], int size, int target, int lun) {
    int i;
    SCSIcommand.source_busid = commandbuf[0];
    for (i = 1; i < size; i++) {
        SCSIcommand.command[i-1] = commandbuf[i];
    }

    SCSIcommand.opcode = SCSIcommand.command[0];
    SCSIcommand.target = target;
    SCSIcommand.lun = lun;
    Log_Printf(LOG_WARN, "SCSI command: Length = %i, Opcode = $%02x, target = %i, lun=%i\n", size, SCSIcommand.opcode, SCSIcommand.target,SCSIcommand.lun);
   if (SCSIcommand.lun!=LUN_DISC)
	{
        Log_Printf(LOG_WARN, "SCSI command: No device at target %i\n", SCSIcommand.target);
        SCSIcommand.nodevice = true;
        SCSIcommand.timeout = false;
        SCSIcommand.transferdirection_todevice = 0;
	return;
    }
    switch (SCSIcommand.target) {
        case 0:
            scsidisk = scsidisk0;
            bCDROM = ConfigureParams.HardDisk.bCDROM0;
            bTargetDevice = ConfigureParams.HardDisk.bSCSIImageAttached0;
            break;
        case 1:
            scsidisk = scsidisk1;
            bCDROM = ConfigureParams.HardDisk.bCDROM1;
            bTargetDevice = ConfigureParams.HardDisk.bSCSIImageAttached1;
            break;
        case 2:
            scsidisk = scsidisk2;
            bCDROM = ConfigureParams.HardDisk.bCDROM2;
            bTargetDevice = ConfigureParams.HardDisk.bSCSIImageAttached2;
            break;
        case 3:
            scsidisk = scsidisk3;
            bCDROM = ConfigureParams.HardDisk.bCDROM3;
            bTargetDevice = ConfigureParams.HardDisk.bSCSIImageAttached3;
            break;
        case 4:
            scsidisk = scsidisk4;
            bCDROM = ConfigureParams.HardDisk.bCDROM4;
            bTargetDevice = ConfigureParams.HardDisk.bSCSIImageAttached4;
            break;
        case 5:
            scsidisk = scsidisk5;
            bCDROM = ConfigureParams.HardDisk.bCDROM5;
            bTargetDevice = ConfigureParams.HardDisk.bSCSIImageAttached5;
            break;
        case 6:
            scsidisk = scsidisk6;
            bCDROM = ConfigureParams.HardDisk.bCDROM6;
            bTargetDevice = ConfigureParams.HardDisk.bSCSIImageAttached6;
            break;

        default:
            Log_Printf(LOG_WARN, "SCSI command: Invalid target: %i\n", SCSIcommand.target);
            break;
    }
    //bTargetDevice |= bCDROM; // handle empty cd-rom drive - does not work yet!
    if(scsidisk) { // experimental!
        SCSIcommand.nodevice = false;
        SCSIcommand.timeout = false;
        SCSI_Emulate_Command();
    } else {	
	// hacks for NeXT (to be tested on real life...)
	// question is : what an SCSI controler should answer for missing drives (and if SCSI controler is aware of SCSI opcodes)
//	if (SCSIcommand.opcode==HD_TEST_UNIT_RDY) {SCSI_TestMissingUnitReady();SCSIcommand.nodevice = false;return;}
//	if (SCSIcommand.opcode==HD_REQ_SENSE) {SCSI_Emulate_Command();return;}
        Log_Printf(LOG_WARN, "SCSI command: No target %i %s at %d", SCSIcommand.target,__FILE__,__LINE__);
        SCSIcommand.nodevice = false;
        SCSIcommand.timeout = true;
        SCSIcommand.transferdirection_todevice = 0;
    }
}

void SCSI_Emulate_Command(void)
{
    
	switch(SCSIcommand.opcode)
	{
            
        case HD_TEST_UNIT_RDY:
            Log_Printf(LOG_WARN, "SCSI command: Test unit ready\n");
            SCSI_TestUnitReady();
            break;
            
        case HD_READ_CAPACITY1:
            Log_Printf(LOG_WARN, "SCSI command: Read capacity\n");
            SCSI_ReadCapacity();
            break;
            
        case HD_READ_SECTOR:
        case HD_READ_SECTOR1:
            Log_Printf(LOG_WARN, "SCSI command: Read sector\n");
            SCSI_ReadSector();
            break;
            
        case HD_WRITE_SECTOR:
        case HD_WRITE_SECTOR1:
            Log_Printf(LOG_WARN, "SCSI command: Write sector\n");
//            HDC_Cmd_WriteSector();
            break;
            
        case HD_INQUIRY:
            Log_Printf(LOG_WARN, "SCSI command: Inquiry\n");
            SCSI_Inquiry();
            break;
            
        case HD_SEEK:
            Log_Printf(LOG_WARN, "SCSI command: Seek\n");
//            HDC_Cmd_Seek();
            break;
            
        case HD_SHIP:
            Log_Printf(LOG_WARN, "SCSI command: Ship\n");
            SCSI_StartStop();
            break;
            
        case HD_REQ_SENSE:
            Log_Printf(LOG_WARN, "SCSI command: Request sense\n");
            SCSI_RequestSense();
            break;
            
        case HD_MODESELECT:
            Log_Printf(LOG_WARN, "MODE SELECT call not implemented yet.\n");
            SCSIcommand.returnCode = HD_STATUS_OK;
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
            SCSIcommand.returnCode = HD_STATUS_ERROR;
            nLastError = HD_REQSENS_OPCODE;
            bSetLastBlockAddr = false;
//            FDC_AcknowledgeInterrupt();
            break;
	}
    esp_command_complete();
    
    /* Update the led each time a command is processed */
	Statusbar_EnableHDLed();
}


/* Helpers */

int SCSI_GetTransferLength(void)
{
	return SCSIcommand.opcode < 0x20?
    // class 0
    SCSIcommand.command[4] :
    // class 1
    COMMAND_ReadInt16(SCSIcommand.command, 7);
}

unsigned long SCSI_GetOffset(void)
{
	return SCSIcommand.opcode < 0x20?
    // class 0
    (COMMAND_ReadInt24(SCSIcommand.command, 1) & 0x1FFFFF) :
    // class 1
    COMMAND_ReadInt32(SCSIcommand.command, 2);
}

int SCSI_GetCount(void)
{
	return SCSIcommand.opcode < 0x20?
    // class 0
    SCSIcommand.command[4] :
    // class 1
    COMMAND_ReadInt16(SCSIcommand.command, 7);
}



/* SCSI Commands */

void SCSI_TestUnitReady(void)
{
    SCSIcommand.transfer_data_len = 0;
	SCSIcommand.returnCode = HD_STATUS_OK;
}


void SCSI_ReadCapacity(void)
{
    Uint32 filesize;
        
    fseek(scsidisk, 0L, SEEK_END);
    filesize = ftell(scsidisk);
    
    Log_Printf(LOG_WARN, "Read disk image: size = %i\n", filesize);

    SCSIcommand.transfer_data_len = 8;
  
    Uint32 sectors = filesize / BLOCKSIZE;
    
    static Uint8 scsi_disksize[8];

    scsi_disksize[0] = (sectors >> 24) & 0xFF;
    scsi_disksize[1] = (sectors >> 16) & 0xFF;
    scsi_disksize[2] = (sectors >> 8) & 0xFF;
    scsi_disksize[3] = sectors & 0xFF;
    scsi_disksize[4] = (BLOCKSIZE >> 24) & 0xFF;
    scsi_disksize[5] = (BLOCKSIZE >> 16) & 0xFF;
    scsi_disksize[6] = (BLOCKSIZE >> 8) & 0xFF;
    scsi_disksize[7] = BLOCKSIZE & 0xFF;
    
    memcpy(dma_write_buffer, scsi_disksize, SCSIcommand.transfer_data_len);
		SCSIcommand.returnCode = HD_STATUS_OK;
		nLastError = HD_REQSENS_OK;
    
	bSetLastBlockAddr = false;
}


void SCSI_ReadSector(void)
{
	int n=0;
    
	nLastBlockAddr = SCSI_GetOffset() * BLOCKSIZE;
    SCSIcommand.transfer_data_len = SCSI_GetCount() * BLOCKSIZE;
    
    
	/* seek to the position */
	if ((scsidisk==NULL) || (fseek(scsidisk, nLastBlockAddr, SEEK_SET) != 0))
	{
        SCSIcommand.returnCode = HD_STATUS_ERROR;
        nLastError = HD_REQSENS_INVADDR;
	}
	else
	{
        n = fread(dma_write_buffer, SCSIcommand.transfer_data_len, 1, scsidisk);
        
        /* Test to check if we read correct data */
        //        Log_Printf(LOG_WARN, "Disk Read Test: $%02x,$%02x,$%02x,$%02x,$%02x,$%02x,$%02x,$%02x\n", dma_write_buffer[0],dma_write_buffer[1],dma_write_buffer[2],dma_write_buffer[3],dma_write_buffer[4],dma_write_buffer[5],dma_write_buffer[6],dma_write_buffer[07]);
    }

    if (n == 1) {
			SCSIcommand.returnCode = HD_STATUS_OK;
			nLastError = HD_REQSENS_OK;
		}
		else
		{
			SCSIcommand.returnCode = HD_STATUS_ERROR;
			nLastError = HD_REQSENS_NOSECTOR;
		}
        
		/* Update DMA counter */
//		FDC_WriteDMAAddress(nDmaAddr + 512*n);
//  }
    
//	FDC_SetDMAStatus(false);              /* no DMA error */
//	FDC_AcknowledgeInterrupt();
	bSetLastBlockAddr = true;
	//FDCSectorCountRegister = 0;
}



void SCSI_Inquiry (void) {
    
    if (bCDROM) {
        inquiry_bytes[0] = 0x05;
        inquiry_bytes[1] |= 0x80;
        inquiry_bytes[16] = 'C';
        inquiry_bytes[18] = '-';
        inquiry_bytes[19] = 'R';
        inquiry_bytes[20] = 'O';
        inquiry_bytes[21] = 'M';
        Log_Printf(LOG_WARN, "Disk is CD-ROM\n");
    } else {
        inquiry_bytes[0] = 0x00;
        inquiry_bytes[1] &= ~0x80;
        inquiry_bytes[16] = 'H';
        inquiry_bytes[18] = 'D';
        inquiry_bytes[19] = ' ';
        inquiry_bytes[20] = ' ';
        inquiry_bytes[21] = ' ';
        Log_Printf(LOG_WARN, "Disk is HDD\n");
    }
    
    
    SCSIcommand.transfer_data_len = SCSI_GetTransferLength();
    Log_Printf(LOG_WARN, "return length: %d", SCSIcommand.transfer_data_len);
    SCSIcommand.transferdirection_todevice = 0;
    memcpy(dma_write_buffer, inquiry_bytes, SCSIcommand.transfer_data_len);
    
    Log_Printf(LOG_WARN, "Inquiry Data: %c,%c,%c,%c,%c,%c,%c,%c\n",dma_write_buffer[8],dma_write_buffer[9],dma_write_buffer[10],dma_write_buffer[11],dma_write_buffer[12],dma_write_buffer[13],dma_write_buffer[14],dma_write_buffer[15]);
                
	if (SCSIcommand.transfer_data_len > (int)sizeof(inquiry_bytes))
		SCSIcommand.transfer_data_len = sizeof(inquiry_bytes);
    
    SCSIcommand.returnCode = HD_STATUS_OK;
	nLastError = HD_REQSENS_OK;
	bSetLastBlockAddr = false;
}


void SCSI_StartStop(void) {
    SCSIcommand.transfer_data_len = 0;
    SCSIcommand.transferdirection_todevice = 0;
    SCSIcommand.returnCode = HD_STATUS_OK;
}


void SCSI_RequestSense(void) {
	int nRetLen;
	Uint8 retbuf[22];

        SCSIcommand.returnCode = HD_STATUS_OK;
        
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

        Log_Printf(LOG_WARN, "[SCSI] REQ SENSE size = %d %s at %d", nRetLen,__FILE__,__LINE__);
        
	if (SCSIcommand.lun!=LUN_DISC) {
	        Log_Printf(LOG_WARN, "[SCSI] REQ SENSE missing lun %d %s at %d", SCSIcommand.lun,__FILE__,__LINE__);
		SCSIcommand.returnCode=HD_REQSENS_NODRIVE;
	}

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
            default: retbuf[2] = 4; break;
		}
		retbuf[7] = 14;
		retbuf[12] = nLastError;
		retbuf[19] = nLastBlockAddr >> 16;
		retbuf[20] = nLastBlockAddr >> 8;
		retbuf[21] = nLastBlockAddr;
	}
    
    SCSIcommand.transfer_data_len = nRetLen;
    memcpy(dma_write_buffer, retbuf, SCSIcommand.transfer_data_len);
    SCSIcommand.returnCode = HD_STATUS_OK;
}
