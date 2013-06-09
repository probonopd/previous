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
#include "dialog.h"
#include "file.h"


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
Uint32 nDiskSize;


static FILE *scsidisk = NULL;

FILE* scsiimage[ESP_MAX_DEVS];
Uint32 nFileSize[ESP_MAX_DEVS];


/* Initialize/Uninitialize SCSI disks */
void SCSI_Init(void) {
    Log_Printf(LOG_WARN, "CALL SCSI INIT\n");
    
    /* Check if files exist. Present dialog to re-select missing files. */        
    int target;
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        if (File_Exists(ConfigureParams.SCSI.target[target].szImageName) && ConfigureParams.SCSI.target[target].bAttached) {
            nFileSize[target] = File_Length(ConfigureParams.SCSI.target[target].szImageName);
            scsiimage[target] = ConfigureParams.SCSI.target[target].bCDROM == true ? File_Open(ConfigureParams.SCSI.target[target].szImageName, "r") : File_Open(ConfigureParams.SCSI.target[target].szImageName, "r+");
        } else {
            nFileSize[target] = 0;
            scsiimage[target]=NULL;
        }
    }
    
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        Log_Printf(LOG_WARN, "Disk0: %s\n", ConfigureParams.SCSI.target[target].szImageName);
    }
}

void SCSI_Uninit(void) {
    int target;
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        if (scsiimage[target])
    		File_Close(scsiimage[target]);
    }
    
    scsidisk = NULL;
}

void SCSI_Reset(void) {
    SCSI_Uninit();
    SCSI_Init();
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
    if (target >= ESP_MAX_DEVS) {
        Log_Printf(LOG_WARN, "Invalid target: %i!\n", target);
        abort();
    }
    SCSIcommand.target = target;
    SCSIcommand.lun = lun;
    Log_Printf(LOG_WARN, "SCSI command: Length = %i, Opcode = $%02x, target = %i, lun=%i\n", size, SCSIcommand.opcode, SCSIcommand.target,SCSIcommand.lun);
    
    scsidisk = scsiimage[target];
    nDiskSize = nFileSize[target];
    bCDROM = ConfigureParams.SCSI.target[target].bCDROM;
    bTargetDevice = ConfigureParams.SCSI.target[target].bAttached;

    //bTargetDevice |= bCDROM; // handle empty cd-rom drive - does not work yet!
    if(scsidisk) { // experimental!
        SCSIcommand.nodevice = false;
        SCSIcommand.timeout = false;
        if ((SCSIcommand.lun!=LUN_DISC) && (SCSIcommand.opcode!=HD_REQ_SENSE) && (SCSIcommand.opcode!=HD_INQUIRY))
	{
        	Log_Printf(LOG_WARN, "SCSI command: No device at target %i\n", SCSIcommand.target);
        SCSIcommand.nodevice = true;
        SCSIcommand.timeout = false;
        	SCSIcommand.transferdirection_todevice = 0;
		SCSIcommand.transfer_data_len=0;
		SCSIcommand.returnCode = HD_STATUS_ERROR;
		nLastError= HD_REQSENS_NODRIVE;
		return;
    	}
        SCSI_Emulate_Command();
    } else {	
	// hacks for NeXT (to be tested on real life...)
	// question is : what an SCSI controler should answer for missing drives (and if SCSI controler is aware of SCSI opcodes)
    //	if (SCSIcommand.opcode==HD_TEST_UNIT_RDY) {SCSI_TestMissingUnitReady();SCSIcommand.nodevice = false;return;}
//        SCSIcommand.nodevice = false;
//        SCSIcommand.timeout = false;
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
            SCSI_WriteSector();
//            abort();
            break;
            
        case HD_INQUIRY:
            Log_Printf(LOG_WARN, "SCSI command: Inquiry\n");
            SCSI_Inquiry();
            break;
            
        case HD_SEEK:
            Log_Printf(LOG_WARN, "SCSI command: Seek\n");
//            HDC_Cmd_Seek();
            abort();
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
            abort();
            break;
            
        case HD_MODESENSE:
            Log_Printf(LOG_WARN, "SCSI command: Mode sense\n");
            SCSI_ModeSense();
            break;
            
        case HD_FORMAT_DRIVE:
            Log_Printf(LOG_WARN, "SCSI command: Format drive\n");
//            HDC_Cmd_FormatDrive();
            abort();
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

// get reserved count for SCSI reply
int SCSI_GetCount(void)
{
	return SCSIcommand.opcode < 0x20?
    // class 0
    SCSIcommand.command[4] :
    // class 1
    COMMAND_ReadInt16(SCSIcommand.command, 7);
}

MODEPAGE SCSI_GetModePage(Uint8 pagecode) {
    MODEPAGE page;
    Uint32 sectors;
    Uint32 cylinders;
    Uint8 heads;
    
    switch (pagecode) {
        case 0x01: // error recovery page
            page.pagesize = 8;
            page.modepage[0] = 0x01; // &0x80: page savable? (not supported!), &0x7F: page code = 0x01
            page.modepage[1] = 0x06; // page length = 6
            page.modepage[2] = 0x00; // AWRE, ARRE, TB, RC, EER, PER, DTE, DCR
            page.modepage[3] = 0x1B; // retry count
            page.modepage[4] = 0x0B; // correction span in bits
            page.modepage[5] = 0x00; // head offset count
            page.modepage[6] = 0x00; // data strobe offset count
            page.modepage[7] = 0xFF; // recovery time limit
            break;
            
        case 0x02: // disconnect/reconnect page
        case 0x03: // format device page
            page.pagesize = 0;
            Log_Printf(LOG_WARN, "Mode Sense: Page %02x not yet emulated!\n", pagecode);
            break;

        case 0x04: // rigid disc geometry page
            sectors = nDiskSize / BLOCKSIZE;
            heads = 16; // max heads per cylinder: 16
            cylinders = sectors / (63 * heads); // max sectors per track: 63
            if ((sectors % (63 * heads)) != 0) {
                cylinders += 1;
            }
            Log_Printf(LOG_WARN, "Disk geometry: %i sectors, %i cylinders, %i heads\n", sectors, cylinders, heads);
            
            page.pagesize = 0; //20;
            Log_Printf(LOG_WARN, "Disk geometry page disabled!\n"); abort();
            page.modepage[0] = 0x04; // &0x80: page savable? (not supported!), &0x7F: page code = 0x04
            page.modepage[1] = 0x12;
            page.modepage[2] = (cylinders >> 16) & 0xFF;
            page.modepage[3] = (cylinders >> 8) & 0xFF;
            page.modepage[4] = cylinders & 0xFF;
            page.modepage[5] = heads;
            page.modepage[6] = 0x00; // 6,7,8: starting cylinder - write precomp (not supported)
            page.modepage[7] = 0x00;
            page.modepage[8] = 0x00;
            page.modepage[9] = 0x00; // 9,10,11: starting cylinder - reduced write current (not supported)
            page.modepage[10] = 0x00;
            page.modepage[11] = 0x00;
            page.modepage[12] = 0x00; // 12,13: drive step rate (not supported)
            page.modepage[13] = 0x00;
            page.modepage[14] = 0x00; // 14,15,16: loading zone cylinder (not supported)
            page.modepage[15] = 0x00;
            page.modepage[16] = 0x00;
            page.modepage[17] = 0x00; // &0x03: rotational position locking
            page.modepage[18] = 0x00; // rotational position lock offset
            page.modepage[19] = 0x00; // reserved
            break;
            
        case 0x08: // caching page
        case 0x0C: // notch page
        case 0x0D: // power condition page
        case 0x38: // cache control page
        case 0x3C: // soft ID page (EEPROM)
            page.pagesize = 0;
            Log_Printf(LOG_WARN, "Mode Sense: Page %02x not yet emulated!\n", pagecode);
            break;
            
        case 0x00: // operating page
            page.pagesize = 4;
            page.modepage[0] = 0x00; // &0x80: page savable? (not supported!), &0x7F: page code = 0x00
            page.modepage[1] = 0x02; // page length = 2
            page.modepage[2] = 0x80; // &0x80: usage bit = 1, &0x10: disable unit attention = 0
            page.modepage[3] = 0x00; // &0x7F: device type qualifier = 0x00, see inquiry!
            break;
            
        default:
            page.pagesize = 0;
            Log_Printf(LOG_WARN, "Mode Sense: Invalid page code: %02x!\n", pagecode);
            break;
    }
    return page;
}



/* SCSI Commands */

void SCSI_TestUnitReady(void)
{
    SCSIcommand.transfer_data_len = 0;
	SCSIcommand.returnCode = HD_STATUS_OK;
}


void SCSI_ReadCapacity(void)
{        
    Log_Printf(LOG_WARN, "Read disk image: size = %i\n", nDiskSize);

    SCSIcommand.transfer_data_len = 8;
  
    Uint32 sectors = nDiskSize / BLOCKSIZE;
    
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

void SCSI_WriteSector(void) {
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
    if (1 == 1) {
// fixme : for now writes are error
			SCSIcommand.returnCode = HD_STATUS_OK;
			nLastError = HD_REQSENS_OK;
		}
	}
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
    
    if (SCSIcommand.lun!=LUN_DISC) {        inquiry_bytes[0] = 0x1F;}
    
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
		Log_Printf(LOG_WARN, "SCSI: *** Strange REQUEST SENSE *** len=%d!",nRetLen);
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


void SCSI_ModeSense(void) {
    Uint8 retbuf[256];
    MODEPAGE page;
    
    Uint32 sectors = nDiskSize / BLOCKSIZE;

    Uint8 pagecontrol = (SCSIcommand.command[2] & 0x0C) >> 6;
    Uint8 pagecode = SCSIcommand.command[2] & 0x3F;
    Uint8 dbd = SCSIcommand.command[1] & 0x08; // disable block descriptor
        
    Log_Printf(LOG_WARN, "Mode Sense: page = %02x, page_control = %i, %s\n", pagecode, pagecontrol, dbd == 0x08 ? "block descriptor disabled" : "block descriptor enabled");
    
    /* Header */
    retbuf[0] = 0x00; // length of following data
    retbuf[1] = 0x00; // medium type (always 0)
    retbuf[2] = bCDROM == true ? 0x80 : 0x00; // if media is read-only 0x80, else 0x00
    retbuf[3] = 0x08; // block descriptor length
    
    /* Block descriptor data */
    Uint8 header_size = 4;
    if (!dbd) {
        retbuf[4] = 0x00; // media density code
        retbuf[5] = sectors >> 16;  // Number of blocks, high (?)
        retbuf[6] = sectors >> 8;   // Number of blocks, med (?)
        retbuf[7] = sectors;        // Number of blocks, low (?)
        retbuf[8] = 0x00; // reserved
        retbuf[9] = (BLOCKSIZE >> 16) & 0xFF;      // Block size in bytes, high
        retbuf[10] = (BLOCKSIZE >> 8) & 0xFF;     // Block size in bytes, med
        retbuf[11] = BLOCKSIZE & 0xFF;     // Block size in bytes, low
        header_size = 12;
        Log_Printf(LOG_WARN, "Mode Sense: Block descriptor data: %s, size = %i blocks, blocksize = %i byte\n", bCDROM == true ? "disk is read-only" : "disk is read/write" , sectors, BLOCKSIZE);
    }
    retbuf[0] = header_size - 1;
    
    /* Mode Pages */
    if (pagecode == 0x3F) { // return all pages!
        Uint8 offset = header_size;
        Uint8 counter;
        for (pagecode = 0; pagecode < 0x3F; pagecode++) {
            page = SCSI_GetModePage(pagecode);
            switch (pagecontrol) {
                case 0: // current values (not supported, using default values)
                    memcpy(page.current, page.modepage, page.pagesize);
                    for (counter = 0; counter < page.pagesize; counter++) {
                        retbuf[counter+offset] = page.current[counter];
                    }
                    break;
                case 1: // changeable values (not supported, all 0)
                    memset(page.changeable, 0x00, page.pagesize);
                    for (counter = 0; counter < page.pagesize; counter++) {
                        retbuf[counter+offset] = page.changeable[counter];
                    }
                    break;
                case 2: // default values
                    for (counter = 0; counter < page.pagesize; counter++) {
                        retbuf[counter+offset] = page.modepage[counter];
                    }
                    break;
                case 3: // saved values (not supported, using default values)
                    memcpy(page.saved, page.modepage, page.pagesize);
                    for (counter = 0; counter < page.pagesize; counter++) {
                        retbuf[counter+offset] = page.saved[counter];
                    }
                    break;
                    
                default:
                    break;
            }
            offset += page.pagesize;
            retbuf[0] += page.pagesize;
        }
    } else { // return only single requested page
        page = SCSI_GetModePage(pagecode);
        
        Uint8 counter;
        switch (pagecontrol) {
            case 0: // current values (not supported, using default values)
                memcpy(page.current, page.modepage, page.pagesize);
                for (counter = 0; counter < page.pagesize; counter++) {
                    retbuf[counter+header_size] = page.current[counter];
                }
                break;
            case 1: // changeable values (not supported, all 0)
                memset(page.changeable, 0x00, page.pagesize);
                for (counter = 0; counter < page.pagesize; counter++) {
                    retbuf[counter+header_size] = page.changeable[counter];
                }
                break;
            case 2: // default values
                for (counter = 0; counter < page.pagesize; counter++) {
                    retbuf[counter+header_size] = page.modepage[counter];
                }
                break;
            case 3: // saved values (not supported, using default values)
                memcpy(page.saved, page.modepage, page.pagesize);
                for (counter = 0; counter < page.pagesize; counter++) {
                    retbuf[counter+header_size] = page.saved[counter];
                }
                break;
                
            default:
                break;
        }
        
        retbuf[0] += page.pagesize;
    }
    
    
    SCSIcommand.transfer_data_len = retbuf[0] + 1;
    if (SCSIcommand.transfer_data_len > SCSI_GetTransferLength())
        SCSIcommand.transfer_data_len = SCSI_GetTransferLength();
    
    memcpy(dma_write_buffer, retbuf, SCSIcommand.transfer_data_len);
    
    SCSIcommand.returnCode = HD_STATUS_OK;
    nLastError = HD_REQSENS_OK;
    
	bSetLastBlockAddr = false;
}
