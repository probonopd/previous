/* SCSI Bus and Disk emulation */
#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "statusbar.h"
#include "scsi.h"
#include "file.h"

#define LOG_SCSI_LEVEL  LOG_DEBUG    /* Print debugging messages */


#define COMMAND_ReadInt16(a, i) (((unsigned) a[i] << 8) | a[i + 1])
#define COMMAND_ReadInt24(a, i) (((unsigned) a[i] << 16) | ((unsigned) a[i + 1] << 8) | a[i + 2])
#define COMMAND_ReadInt32(a, i) (((unsigned) a[i] << 24) | ((unsigned) a[i + 1] << 16) | ((unsigned) a[i + 2] << 8) | a[i + 3])


#define BLOCKSIZE 512

#define LUN_DISK 0 // for now only LUN 0 is valid for our phys drives

/* Status Codes */
#define STAT_GOOD           0x00
#define STAT_CHECK_COND     0x02
#define STAT_COND_MET       0x04
#define STAT_BUSY           0x08
#define STAT_INTERMEDIATE   0x10
#define STAT_INTER_COND_MET 0x14
#define STAT_RESERV_CONFL   0x18

/* Messages */
#define MSG_COMPLETE        0x00
#define MSG_SAVE_PTRS       0x02
#define MSG_RESTORE_PTRS    0x03
#define MSG_DISCONNECT      0x04
#define MSG_INITIATOR_ERR   0x05
#define MSG_ABORT           0x06
#define MSG_MSG_REJECT      0x07
#define MSG_NOP             0x08
#define MSG_PARITY_ERR      0x09
#define MSG_LINK_CMD_CMPLT  0x0A
#define MSG_LNKCMDCMPLTFLAG 0x0B
#define MSG_DEVICE_RESET    0x0C

#define MSG_IDENTIFY_MASK   0x80
#define MSG_ID_DISCONN      0x40
#define MSG_LUNMASK         0x07

/* Sense Keys */
#define SK_NOSENSE          0x00
#define SK_RECOVERED        0x01
#define SK_NOTREADY         0x02
#define SK_MEDIA            0x03
#define SK_HARDWARE         0x04
#define SK_ILLEGAL_REQ      0x05
#define SK_UNIT_ATN         0x06
#define SK_DATAPROTECT      0x07
#define SK_ABORTED_CMD      0x0B
#define SK_VOL_OVERFLOW     0x0D
#define SK_MISCOMPARE       0x0E

/* Additional Sense Codes */
#define SC_NO_ERROR         0x00    // 0
#define SC_NO_SECTOR        0x01    // 4
#define SC_WRITE_FAULT      0x03    // 5
#define SC_NOT_READY        0x04    // 2
#define SC_INVALID_CMD      0x20    // 5
#define SC_INVALID_LBA      0x21    // 5
#define SC_INVALID_CDB      0x24    // 5
#define SC_INVALID_LUN      0x25    // 5
#define SC_WRITE_PROTECT    0x27    // 7


/* SCSI Commands */

/* The following are multi-sector transfers with seek implied */
#define CMD_VERIFY_TRACK    0x05    /* Verify track */
#define CMD_FORMAT_TRACK    0x06    /* Format track */
#define CMD_READ_SECTOR     0x08    /* Read sector */
#define CMD_READ_SECTOR1    0x28    /* Read sector (class 1) */
#define CMD_WRITE_SECTOR    0x0A    /* Write sector */
#define CMD_WRITE_SECTOR1   0x2A    /* Write sector (class 1) */

/* Other codes */
#define CMD_TEST_UNIT_RDY   0x00    /* Test unit ready */
#define CMD_FORMAT_DRIVE    0x04    /* Format the whole drive */
#define CMD_SEEK            0x0B    /* Seek */
#define CMD_CORRECTION      0x0D    /* Correction */
#define CMD_INQUIRY         0x12    /* Inquiry */
#define CMD_MODESELECT      0x15    /* Mode select */
#define CMD_MODESENSE       0x1A    /* Mode sense */
#define CMD_REQ_SENSE       0x03    /* Request sense */
#define CMD_SHIP            0x1B    /* Ship drive */
#define CMD_READ_CAPACITY1  0x25    /* Read capacity (class 1) */

void SCSI_Emulate_Command(Uint8 *cdb);

void SCSI_Inquiry(Uint8 *cdb);
void SCSI_StartStop(Uint8 *cdb);
void SCSI_TestUnitReady(Uint8 *cdb);
void SCSI_ReadCapacity(Uint8 *cdb);
void SCSI_ReadSector(Uint8 *cdb);
void SCSI_WriteSector(Uint8 *cdb);
void SCSI_RequestSense(Uint8 *cdb);
void SCSI_ModeSense(Uint8 *cdb);
void SCSI_FormatDrive(Uint8 *cdb);


/* Helpers */
int SCSI_GetCommandLength(Uint8 opcode);
int SCSI_GetTransferLength(Uint8 opcode, Uint8 *cdb);
Uint64 SCSI_GetOffset(Uint8 opcode, Uint8 *cdb);
int SCSI_GetCount(Uint8 opcode, Uint8 *cdb);

void scsi_read_sector(void);
void scsi_write_sector(void);


/* SCSI disk */
struct {
    SCSI_DEVTYPE devtype;
    FILE* dsk;
    Uint64 size;
    bool readonly;
    Uint8 lun;
    Uint8 status;
    Uint8 message;
    
    struct {
        Uint8 key;
        Uint8 code;
        bool valid;
        Uint32 info;
    } sense;
    
    Uint32 lba;
    Uint32 blockcounter;
    Uint32 lastlba;
    
    Uint8** shadow;
} SCSIdisk[ESP_MAX_DEVS];


/* Mode Pages */
#define MODEPAGE_MAX_SIZE 24

typedef struct {
    Uint8 current[MODEPAGE_MAX_SIZE];
    Uint8 changeable[MODEPAGE_MAX_SIZE];
    Uint8 modepage[MODEPAGE_MAX_SIZE]; // default values
    Uint8 saved[MODEPAGE_MAX_SIZE];
    Uint8 pagesize;
} MODEPAGE;

MODEPAGE SCSI_GetModePage(Uint8 pagecode);


/* Initialize/Uninitialize SCSI disks */
void SCSI_Init(void) {
    Log_Printf(LOG_WARN, "Loading SCSI disks:\n");
    
    int i;
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        SCSIdisk[i].devtype = ConfigureParams.SCSI.target[i].nDeviceType;
        SCSI_Insert(i);
    }
}

void SCSI_Uninit(void) {
    int i;
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        if (SCSIdisk[i].dsk) {
            SCSI_Eject(i);
        }
    }
}

void SCSI_Reset(void) {
    SCSI_Uninit();
    SCSI_Init();
}

void SCSI_Eject(Uint8 i) {
    File_Close(SCSIdisk[i].dsk);
    SCSIdisk[i].dsk = NULL;
    SCSIdisk[i].size = 0;
    SCSIdisk[i].readonly = false;
    SCSIdisk[i].shadow = NULL;
}

void SCSI_EjectDisk(Uint8 i) {
    ConfigureParams.SCSI.target[i].bDiskInserted = false;
    ConfigureParams.SCSI.target[i].szImageName[0] = '\0';
    
    SCSI_Eject(i);
}

void SCSI_Insert(Uint8 i) {
    SCSIdisk[i].lun = SCSIdisk[i].status = SCSIdisk[i].message = 0;
    SCSIdisk[i].sense.code = SCSIdisk[i].sense.key = SCSIdisk[i].sense.info = 0;
    SCSIdisk[i].sense.valid = false;
    SCSIdisk[i].lba = SCSIdisk[i].lastlba = SCSIdisk[i].blockcounter = 0;
    
    SCSIdisk[i].shadow = NULL;
    
    Log_Printf(LOG_WARN, "SCSI Disk%i: %s\n",i,ConfigureParams.SCSI.target[i].szImageName);
    
    if (File_Exists(ConfigureParams.SCSI.target[i].szImageName) &&
        ConfigureParams.SCSI.target[i].bDiskInserted) {
        if (ConfigureParams.SCSI.target[i].bWriteProtected ||
            ConfigureParams.SCSI.target[i].nDeviceType==DEVTYPE_CD) {
            SCSIdisk[i].dsk = File_Open(ConfigureParams.SCSI.target[i].szImageName, "rb");
            if (SCSIdisk[i].dsk == NULL) {
                Log_Printf(LOG_WARN, "SCSI Disk%i: Cannot open image file %s\n",
                           i, ConfigureParams.SCSI.target[i].szImageName);
                SCSIdisk[i].size = 0;
                SCSIdisk[i].readonly = false;
                if (SCSIdisk[i].devtype == DEVTYPE_HARDDISK) {
                    SCSIdisk[i].devtype = DEVTYPE_NONE;
                }
            } else {
                SCSIdisk[i].size = File_Length(ConfigureParams.SCSI.target[i].szImageName);
                SCSIdisk[i].readonly = true;
            }
        } else {
            SCSIdisk[i].dsk = File_Open(ConfigureParams.SCSI.target[i].szImageName, "rb+");
            if (SCSIdisk[i].dsk == NULL) {
                SCSIdisk[i].dsk = File_Open(ConfigureParams.SCSI.target[i].szImageName, "rb");
                if (SCSIdisk[i].dsk == NULL) {
                    Log_Printf(LOG_WARN, "SCSI Disk%i: Cannot open image file %s\n",
                               i, ConfigureParams.SCSI.target[i].szImageName);
                    SCSIdisk[i].size = 0;
                    SCSIdisk[i].readonly = false;
                    if (SCSIdisk[i].devtype == DEVTYPE_HARDDISK) {
                        SCSIdisk[i].devtype = DEVTYPE_NONE;
                    }
                } else {
                    SCSIdisk[i].size = File_Length(ConfigureParams.SCSI.target[i].szImageName);
                    SCSIdisk[i].readonly = true;
                    Log_Printf(LOG_WARN, "SCSI Disk%i: Image file is not writable. Enabling write protection.\n", i);
                }
            } else {
                SCSIdisk[i].size = File_Length(ConfigureParams.SCSI.target[i].szImageName);
                SCSIdisk[i].readonly = false;
            }
        }
    } else {
        SCSIdisk[i].size = 0;
        SCSIdisk[i].dsk = NULL;
        SCSIdisk[i].readonly = false;
    }
}



/* INQUIRY response data */
#define DEVTYPE_DISK        0x00    /* read/write disks */
#define DEVTYPE_TAPE        0x01    /* tapes and other sequential devices */
#define DEVTYPE_PRINTER     0x02    /* printers */
#define DEVTYPE_PROCESSOR   0x03    /* cpus */
#define DEVTYPE_WORM        0x04    /* write-once optical disks */
#define DEVTYPE_READONLY    0x05    /* cd-roms */
#define DEVTYPE_NOTPRESENT  0x7f    /* logical unit not present */


static unsigned char inquiry_bytes[] =
{
    0x00,             /* 0: device type: see above */
    0x00,             /* 1: &0x7F - device type qualifier 0x00 unsupported, &0x80 - rmb: 0x00 = nonremovable, 0x80 = removable */
    0x01,             /* 2: ANSI SCSI standard (first release) compliant */
    0x02,             /* 3: Response format (format of following data): 0x01 SCSI-1 compliant */
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


Uint8 SCSIdisk_Send_Status(void) {
    SCSIbus.phase = PHASE_MI;
    return SCSIdisk[SCSIbus.target].status;
}

Uint8 SCSIdisk_Send_Message(void) {
    return SCSIdisk[SCSIbus.target].message;
}


bool SCSIdisk_Select(Uint8 target) {
    
    /* If there is no disk drive present, return timeout true */
    if (SCSIdisk[target].devtype==DEVTYPE_NONE) {
        Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Selection timeout, target = %i", target);
        SCSIbus.phase = PHASE_ST;
        return true;
    } else {
        SCSIbus.target = target;
        return false;
    }
}


void SCSIdisk_Receive_Command(Uint8 *cdb, Uint8 identify) {
    Uint8 lun = 0;
    
    /* Get logical unit number */
    if (identify&MSG_IDENTIFY_MASK) { /* if identify message is valid */
        lun = identify&MSG_LUNMASK; /* use lun from identify message */
    } else {
        lun = (cdb[1]&0xE0)>>5; /* use lun specified in CDB */
    }
    
    SCSIdisk[SCSIbus.target].lun = lun;
    
    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Opcode = $%02x, target = %i, lun = %i\n", cdb[0], SCSIbus.target,lun);
    
    SCSI_Emulate_Command(cdb);
}


void SCSI_Emulate_Command(Uint8 *cdb) {
    Uint8 opcode = cdb[0];
    Uint8 target = SCSIbus.target;
    
    /* First check for lun-independent commands */
    switch (opcode) {
        case CMD_INQUIRY:
            Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Inquiry\n");
            SCSI_Inquiry(cdb);
            break;
        case CMD_REQ_SENSE:
            Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Request sense\n");
            SCSI_RequestSense(cdb);
            break;
            /* Check if the specified lun is valid for our disk */
        default:
            if (SCSIdisk[target].lun!=LUN_DISK) {
                Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Invalid lun! Check condition.\n");
                SCSIbus.phase = PHASE_ST;
                SCSIdisk[target].status = STAT_CHECK_COND; /* status: check condition */
                SCSIdisk[target].message = MSG_COMPLETE; /* TODO: CHECK THIS! */
                SCSIdisk[target].sense.code = SC_INVALID_LUN;
                SCSIdisk[target].sense.valid = false;
                return;
            }
            
            /* Then check for lun-dependent commands */
            switch(opcode) {
                case CMD_TEST_UNIT_RDY:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Test unit ready\n");
                    SCSI_TestUnitReady(cdb);
                    break;
                case CMD_READ_CAPACITY1:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Read capacity\n");
                    SCSI_ReadCapacity(cdb);
                    break;
                case CMD_READ_SECTOR:
                case CMD_READ_SECTOR1:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Read sector\n");
                    SCSI_ReadSector(cdb);
                    break;
                case CMD_WRITE_SECTOR:
                case CMD_WRITE_SECTOR1:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Write sector\n");
                    SCSI_WriteSector(cdb);
                    break;
                case CMD_SEEK:
                    Log_Printf(LOG_WARN, "SCSI command: Seek\n");
                    abort();
                    break;
                case CMD_SHIP:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Ship\n");
                    SCSI_StartStop(cdb);
                    break;
                case CMD_MODESELECT:
                    Log_Printf(LOG_WARN, "SCSI command: Mode select\n");
                    abort();
                    break;
                case CMD_MODESENSE:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Mode sense\n");
                    SCSI_ModeSense(cdb);
                    break;
                case CMD_FORMAT_DRIVE:
                    Log_Printf(LOG_SCSI_LEVEL, "SCSI command: Format drive\n");
                    SCSI_FormatDrive(cdb);
                    break;
                    /* as of yet unsupported commands */
                case CMD_VERIFY_TRACK:
                case CMD_FORMAT_TRACK:
                case CMD_CORRECTION:
                default:
                    Log_Printf(LOG_WARN, "SCSI command: Unknown Command (%02X)\n",opcode);
                    SCSIdisk[target].status = STAT_CHECK_COND;
                    SCSIdisk[target].sense.code = SC_INVALID_CMD;
                    SCSIdisk[target].sense.valid = false;
                    SCSIbus.phase = PHASE_ST;
                    break;
            }
            break;
    }
    
    SCSIdisk[target].message = MSG_COMPLETE;
    
    /* Update the led each time a command is processed */
    Statusbar_BlinkLed(DEVICE_LED_SCSI);
}


/* Helpers */

int SCSI_GetCommandLength(Uint8 opcode) {
    Uint8 group_code = (opcode&0xE0)>>5;
    switch (group_code) {
        case 0: return 6;
        case 1: return 10;
        case 5: return 12;
        default:
            Log_Printf(LOG_WARN, "[SCSI] Unimplemented Group Code!");
            return 6;
    }
}

int SCSI_GetTransferLength(Uint8 opcode, Uint8 *cdb)
{
    return opcode < 0x20?
    // class 0
    cdb[4] :
    // class 1
    COMMAND_ReadInt16(cdb, 7);
}

Uint64 SCSI_GetOffset(Uint8 opcode, Uint8 *cdb)
{
    return opcode < 0x20?
    // class 0
    (COMMAND_ReadInt24(cdb, 1) & 0x1FFFFF) :
    // class 1
    COMMAND_ReadInt32(cdb, 2);
}

// get reserved count for SCSI reply
int SCSI_GetCount(Uint8 opcode, Uint8 *cdb)
{
    return opcode < 0x20?
    // class 0
    ((cdb[4]==0)?0x100:cdb[4]) :
    // class 1
    COMMAND_ReadInt16(cdb, 7);
}

static void SCSI_GuessGeometry(Uint32 size, Uint32 *cylinders, Uint32 *heads, Uint32 *sectors)
{
    Uint32 c,h,s;
    
    for (h=16; h>0; h--) {
        for (s=63; s>15; s--) {
            if ((size%(s*h))==0) {
                c=size/(s*h);
                *cylinders=c;
                *heads=h;
                *sectors=s;
                return;
            }
        }
    }
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk geometry: No valid geometry found! Using default.");
    
    h=16;
    s=63;
    c=size/(s*h);
    if ((size%(s*h))!=0) {
        c+=1;
    }
    *cylinders=c;
    *heads=h;
    *sectors=s;
}

#define SCSI_SEEK_TIME_HD       20000  /* 20 ms max seek time */
#define SCSI_SECTOR_TIME_HD     350    /* 1.4 MB/sec */
#define SCSI_SEEK_TIME_FD       200000 /* 200 ms max seek time */
#define SCSI_SECTOR_TIME_FD     5500   /* 90 kB/sec */
#define SCSI_SEEK_TIME_CD       500000 /* 500 ms max seek time */
#define SCSI_SECTOR_TIME_CD     3250   /* 150 kB/sec */

Sint64 SCSI_Seek_Time(void) {
    Uint8 target = SCSIbus.target;
    Sint64 seektime, seekoffset, disksize;
    
    if (scsi_buffer.disk) {
        switch (SCSIdisk[target].devtype) {
            case DEVTYPE_HARDDISK:
                seektime = SCSI_SEEK_TIME_HD;
                break;
            case DEVTYPE_CD:
                seektime = SCSI_SEEK_TIME_CD;
                break;
            case DEVTYPE_FLOPPY:
                seektime = SCSI_SEEK_TIME_FD;
                break;
            default:
                return 0;
        }
        if (SCSIdisk[target].lba < SCSIdisk[target].lastlba) {
            seekoffset = SCSIdisk[target].lastlba - SCSIdisk[target].lba;
        } else {
            seekoffset = SCSIdisk[target].lba - SCSIdisk[target].lastlba;
        }
        disksize = SCSIdisk[target].size/BLOCKSIZE;
        
        if (disksize <= 0) { /* make sure no zero divide occurs */
            return 0;
        }
        seektime *= seekoffset;
        seektime /= disksize;
        
        if (seektime > 500000) {
            seektime = 500000;
        }
        
        return seektime;
    } else {
        return 0;
    }
}

Sint64 SCSI_Sector_Time(void) {
    int target = SCSIbus.target;
    Sint64 sectors = SCSIdisk[target].blockcounter;
    
    if (sectors <= 0) {
        sectors = 1;
    }
    
    if (scsi_buffer.disk) {
        switch (SCSIdisk[target].devtype) {
            case DEVTYPE_HARDDISK:
                return sectors * SCSI_SECTOR_TIME_HD;
            case DEVTYPE_CD:
                return sectors * SCSI_SECTOR_TIME_CD;
            case DEVTYPE_FLOPPY:
                return sectors * SCSI_SECTOR_TIME_FD;
            default:
                return 1000;
        }
    } else {
        return 100;
    }
}

MODEPAGE SCSI_GetModePage(Uint8 pagecode) {
    Uint8 target = SCSIbus.target;
    
    MODEPAGE page;
    
    switch (pagecode) {
        case 0x00: // operating page
            page.pagesize = 4;
            page.modepage[0] = 0x00; // &0x80: page savable? (not supported!), &0x7F: page code = 0x00
            page.modepage[1] = 0x02; // page length = 2
            page.modepage[2] = 0x80; // &0x80: usage bit = 1, &0x10: disable unit attention = 0
            page.modepage[3] = 0x00; // &0x7F: device type qualifier = 0x00, see inquiry!
            break;
            
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
            
        case 0x03: // format device page
            page.pagesize = 0;
            Log_Printf(LOG_WARN, "[SCSI] Mode Sense: Page %02x not yet emulated!\n", pagecode);
            //abort();
            break;
            
        case 0x04: // rigid disc geometry page
        {
            Uint32 num_sectors = SCSIdisk[target].size/BLOCKSIZE;
            
            Uint32 cylinders, heads, sectors;
            
            SCSI_GuessGeometry(num_sectors, &cylinders, &heads, &sectors);
            
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk geometry: %i sectors, %i cylinders, %i heads\n", sectors, cylinders, heads);
            
            page.pagesize = 20;
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
        }
            break;
            
        case 0x02: // disconnect/reconnect page
        case 0x08: // caching page
        case 0x0C: // notch page
        case 0x0D: // power condition page
        case 0x38: // cache control page
        case 0x3C: // soft ID page (EEPROM)
            page.pagesize = 0;
            Log_Printf(LOG_WARN, "[SCSI] Mode Sense: Page %02x not yet emulated!\n", pagecode);
            //abort();
            break;
            
        default:
            page.pagesize = 0;
            Log_Printf(LOG_WARN, "[SCSI] Mode Sense: Invalid page code: %02x!\n", pagecode);
            break;
    }
    return page;
}



/* SCSI Commands */

void SCSI_TestUnitReady(Uint8 *cdb) {
    Uint8 target = SCSIbus.target;
    
    if (SCSIdisk[target].devtype!=DEVTYPE_NONE &&
        SCSIdisk[target].devtype!=DEVTYPE_HARDDISK &&
        SCSIdisk[target].dsk==NULL) { /* Empty drive */
        SCSIdisk[target].status = STAT_CHECK_COND;
        SCSIdisk[target].sense.code = SC_NOT_READY;
        SCSIbus.phase = PHASE_ST;
    } else {
        SCSIdisk[target].status = STAT_GOOD;
        SCSIdisk[target].sense.code = SC_NO_ERROR;
        SCSIbus.phase = PHASE_ST;
    }
}

void SCSI_ReadCapacity(Uint8 *cdb) {
    Uint8 target = SCSIbus.target;
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Read disk image: size = %llu byte\n", SCSIdisk[target].size);
    
    Uint32 sectors = (SCSIdisk[target].size / BLOCKSIZE) - 1; /* last LBA */
    
    static Uint8 scsi_disksize[8];
    
    scsi_disksize[0] = (sectors >> 24) & 0xFF;
    scsi_disksize[1] = (sectors >> 16) & 0xFF;
    scsi_disksize[2] = (sectors >> 8) & 0xFF;
    scsi_disksize[3] = sectors & 0xFF;
    scsi_disksize[4] = (BLOCKSIZE >> 24) & 0xFF;
    scsi_disksize[5] = (BLOCKSIZE >> 16) & 0xFF;
    scsi_disksize[6] = (BLOCKSIZE >> 8) & 0xFF;
    scsi_disksize[7] = BLOCKSIZE & 0xFF;
    
    memcpy(scsi_buffer.data, scsi_disksize, 8);
    scsi_buffer.limit=scsi_buffer.size=8;
    scsi_buffer.disk=false;
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIbus.phase = PHASE_DI;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;
}

void SCSI_WriteSector(Uint8 *cdb) {
    Uint8 target = SCSIbus.target;
    
    SCSIdisk[target].lastlba = SCSIdisk[target].lba;
    SCSIdisk[target].lba = SCSI_GetOffset(cdb[0], cdb);
    SCSIdisk[target].blockcounter = SCSI_GetCount(cdb[0], cdb);
    
    if (SCSIdisk[target].readonly) {
        Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Write sector: Disk is write protected! Check condition.");
        SCSIdisk[target].status = STAT_CHECK_COND;
        SCSIdisk[target].sense.code = SC_WRITE_PROTECT;
        SCSIdisk[target].sense.valid = false;
        SCSIbus.phase = PHASE_ST;
        return;
    }
    scsi_buffer.disk=true;
    scsi_buffer.size=0;
    scsi_buffer.limit=BLOCKSIZE;
    SCSIbus.phase = PHASE_DO;
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Write sector: %i block(s) at offset %i (blocksize: %i byte)",
               SCSIdisk[target].blockcounter, SCSIdisk[target].lba, BLOCKSIZE);
}

void scsi_write_sector(void) {
    Uint8 target = SCSIbus.target;
    int n=0;
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Writing block at offset %i (%i blocks remaining).",
               SCSIdisk[target].lba,SCSIdisk[target].blockcounter-1);
    
    /* seek to the position */
    if ((SCSIdisk[target].dsk==NULL) || (fseek(SCSIdisk[target].dsk, ((Uint64)SCSIdisk[target].lba)*BLOCKSIZE, SEEK_SET) != 0)) {
        n = 0;
    } else {
        if(ConfigureParams.SCSI.nWriteProtection != WRITEPROT_ON)
            n = fwrite(scsi_buffer.data, BLOCKSIZE, 1, SCSIdisk[target].dsk);
        else {
            n=1;
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] WARNING: File write disabled!");
            if(SCSIdisk[target].shadow) {
                if(!(SCSIdisk[target].shadow[SCSIdisk[target].lba]))
                    SCSIdisk[target].shadow[SCSIdisk[target].lba] = malloc(BLOCKSIZE);
                memcpy(SCSIdisk[target].shadow[SCSIdisk[target].lba], scsi_buffer.data, BLOCKSIZE);
            } else {
                Uint32 blocks = SCSIdisk[target].size / BLOCKSIZE;
                SCSIdisk[target].shadow = malloc(sizeof(Uint8*) * blocks);
                for(int i = blocks; --i >= 0;)
                    SCSIdisk[target].shadow[i] = NULL;
            }
        }
        scsi_buffer.limit=BLOCKSIZE;
        scsi_buffer.size=0;
    }
    
    if (n == 1) {
        SCSIdisk[target].status = STAT_GOOD;
        SCSIdisk[target].sense.code = SC_NO_ERROR;
        SCSIdisk[target].sense.valid = false;
        SCSIdisk[target].lba++;
        SCSIdisk[target].blockcounter--;
        if (SCSIdisk[target].blockcounter==0) {
            SCSIbus.phase = PHASE_ST;
        }
    } else {
        SCSIdisk[target].status = STAT_CHECK_COND;
        SCSIdisk[target].sense.code = SC_INVALID_LBA;
        SCSIdisk[target].sense.valid = true;
        SCSIdisk[target].sense.info = SCSIdisk[target].lba;
        SCSIbus.phase = PHASE_ST;
    }
}

void SCSIdisk_Receive_Data(Uint8 val) {
    /* Receive one byte. If the transfer is complete, set status phase
     * and write the buffer contents to the disk. */
    scsi_buffer.data[scsi_buffer.size]=val;
    scsi_buffer.size++;
    if (scsi_buffer.size==scsi_buffer.limit) {
        if (scsi_buffer.disk==true) {
            scsi_write_sector();  /* sets status phase if done or error */
        } else {
            SCSIbus.phase = PHASE_ST;
        }
    }
}


void SCSI_ReadSector(Uint8 *cdb) {
    Uint8 target = SCSIbus.target;
    
    SCSIdisk[target].lastlba = SCSIdisk[target].lba;
    SCSIdisk[target].lba = SCSI_GetOffset(cdb[0], cdb);
    SCSIdisk[target].blockcounter = SCSI_GetCount(cdb[0], cdb);
    scsi_buffer.disk=true;
    scsi_buffer.size=0;
    SCSIbus.phase = PHASE_DI;
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Read sector: %i block(s) at offset %i (blocksize: %i byte)",
               SCSIdisk[target].blockcounter, SCSIdisk[target].lba, BLOCKSIZE);
    scsi_read_sector();
}

void scsi_read_sector(void) {
    Uint8 target = SCSIbus.target;
    
    if (SCSIdisk[target].blockcounter==0) {
        SCSIbus.phase = PHASE_ST;
        return;
    }
    
    int n;
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Reading block at offset %i (%i blocks remaining).",
               SCSIdisk[target].lba,SCSIdisk[target].blockcounter-1);
    
    /* seek to the position */
    if ((SCSIdisk[target].dsk==NULL) || (fseek(SCSIdisk[target].dsk, ((Uint64)SCSIdisk[target].lba)*BLOCKSIZE, SEEK_SET) != 0)) {
        n = 0;
    } else {
        if(SCSIdisk[target].shadow && SCSIdisk[target].shadow[SCSIdisk[target].lba]) {
            memcpy(scsi_buffer.data, SCSIdisk[target].shadow[SCSIdisk[target].lba], BLOCKSIZE);
            n = 1;
        } else {
            n = fread(scsi_buffer.data, BLOCKSIZE, 1, SCSIdisk[target].dsk);
        }
        scsi_buffer.limit=scsi_buffer.size=BLOCKSIZE;
    }
    
    if (n == 1) {
        SCSIdisk[target].status = STAT_GOOD;
        SCSIdisk[target].sense.code = SC_NO_ERROR;
        SCSIdisk[target].sense.valid = false;
        SCSIdisk[target].lba++;
        SCSIdisk[target].blockcounter--;
    } else {
        SCSIdisk[target].status = STAT_CHECK_COND;
        SCSIdisk[target].sense.code = SC_INVALID_LBA;
        SCSIdisk[target].sense.valid = true;
        SCSIdisk[target].sense.info = SCSIdisk[target].lba;
        SCSIbus.phase = PHASE_ST;
    }
}

Uint8 SCSIdisk_Send_Data(void) {
    /* Send one byte. If the transfer is complete, set status phase */
    Uint8 val=scsi_buffer.data[scsi_buffer.limit-scsi_buffer.size];
    scsi_buffer.size--;
    if (scsi_buffer.size==0) {
        if (scsi_buffer.disk==true) {
            scsi_read_sector(); /* sets status phase if done or error */
        } else {
            SCSIbus.phase = PHASE_ST;
        }
    }
    return val;
}


void SCSI_Inquiry (Uint8 *cdb) {
    Uint8 target = SCSIbus.target;
    
    switch (SCSIdisk[target].devtype) {
        case DEVTYPE_HARDDISK:
            inquiry_bytes[0] = DEVTYPE_DISK;
            inquiry_bytes[1] &= ~0x80;
            inquiry_bytes[16] = 'H';
            inquiry_bytes[17] = 'D';
            inquiry_bytes[18] = 'D';
            inquiry_bytes[19] = ' ';
            inquiry_bytes[20] = ' ';
            inquiry_bytes[21] = ' ';
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk is HDD\n");
            break;
        case DEVTYPE_CD:
            inquiry_bytes[0] = DEVTYPE_READONLY;
            inquiry_bytes[1] |= 0x80;
            inquiry_bytes[16] = 'C';
            inquiry_bytes[17] = 'D';
            inquiry_bytes[18] = '-';
            inquiry_bytes[19] = 'R';
            inquiry_bytes[20] = 'O';
            inquiry_bytes[21] = 'M';
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk is CD-ROM\n");
            break;
        case DEVTYPE_FLOPPY:
            inquiry_bytes[0] = DEVTYPE_DISK;
            inquiry_bytes[1] |= 0x80;
            inquiry_bytes[16] = 'F';
            inquiry_bytes[17] = 'L';
            inquiry_bytes[18] = 'O';
            inquiry_bytes[19] = 'P';
            inquiry_bytes[20] = 'P';
            inquiry_bytes[21] = 'Y';
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Disk is Floppy\n");
            break;
            
        default:
            break;
    }
    
    if (SCSIdisk[target].lun!=LUN_DISK) {
        inquiry_bytes[0] = DEVTYPE_NOTPRESENT;
    }
    
    scsi_buffer.disk=false;
    scsi_buffer.limit = scsi_buffer.size = SCSI_GetTransferLength(cdb[0], cdb);
    if (scsi_buffer.limit > (int)sizeof(inquiry_bytes)) {
        scsi_buffer.limit = scsi_buffer.size = sizeof(inquiry_bytes);
    }
    memcpy(scsi_buffer.data, inquiry_bytes, scsi_buffer.limit);
    
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Inquiry data length: %d", scsi_buffer.limit);
    Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Inquiry Data: %c,%c,%c,%c,%c,%c,%c,%c\n",scsi_buffer.data[8],
               scsi_buffer.data[9],scsi_buffer.data[10],scsi_buffer.data[11],scsi_buffer.data[12],
               scsi_buffer.data[13],scsi_buffer.data[14],scsi_buffer.data[15]);
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIbus.phase = PHASE_DI;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;
}


void SCSI_StartStop(Uint8 *cdb) {
    Uint8 target = SCSIbus.target;
    
    switch (cdb[4]&0x03) {
        case 0:
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Stop disk %i", target);
            break;
        case 1:
            Log_Printf(LOG_SCSI_LEVEL, "[SCSI] Start disk %i", target);
            break;
        case 2:
            Log_Printf(LOG_WARN, "[SCSI] Eject disk %i", target);
            if (SCSIdisk[target].devtype!=DEVTYPE_HARDDISK) {
                SCSI_EjectDisk(target);
                Statusbar_AddMessage("Ejecting SCSI media.", 0);
            }
            break;
        default:
            Log_Printf(LOG_WARN, "[SCSI] Invalid start/stop");
            break;
    }
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIbus.phase = PHASE_ST;
}


void SCSI_RequestSense(Uint8 *cdb) {
    Uint8 target = SCSIbus.target;
    
    int nRetLen;
    Uint8 retbuf[22];
    
    nRetLen = SCSI_GetCount(cdb[0], cdb);
    
    if ((nRetLen<4 && nRetLen!=0) || nRetLen>22) {
        Log_Printf(LOG_WARN, "[SCSI] *** Strange REQUEST SENSE *** len=%d!",nRetLen);
    }
    
    /* Limit to sane length */
    if (nRetLen <= 0) {
        nRetLen = 4;
    } else if (nRetLen > 22) {
        nRetLen = 22;
    }
    
    Log_Printf(LOG_WARN, "[SCSI] REQ SENSE size = %d %s at %d", nRetLen,__FILE__,__LINE__);
    
    memset(retbuf, 0, nRetLen);
    
    retbuf[0] = 0x70;
    if (SCSIdisk[target].sense.valid) {
        retbuf[0] |= 0x80;
        retbuf[3] = SCSIdisk[target].sense.info >> 24;
        retbuf[4] = SCSIdisk[target].sense.info >> 16;
        retbuf[5] = SCSIdisk[target].sense.info >> 8;
        retbuf[6] = SCSIdisk[target].sense.info;
    }
    switch (SCSIdisk[target].sense.code) {
        case SC_NO_ERROR:
            SCSIdisk[target].sense.key = SK_NOSENSE;
            break;
        case SC_NOT_READY:
            SCSIdisk[target].sense.key = SK_NOTREADY;
            break;
        case SC_WRITE_FAULT:
        case SC_INVALID_CMD:
        case SC_INVALID_LBA:
        case SC_INVALID_CDB:
        case SC_INVALID_LUN:
            SCSIdisk[target].sense.key = SK_ILLEGAL_REQ;
            break;
        case SC_WRITE_PROTECT:
            SCSIdisk[target].sense.key = SK_DATAPROTECT;
            break;
        case SC_NO_SECTOR:
        default:
            SCSIdisk[target].sense.key = SK_HARDWARE;
            break;
    }
    retbuf[2] = SCSIdisk[target].sense.key;
    retbuf[7] = 14;
    retbuf[12] = SCSIdisk[target].sense.code;
    
    scsi_buffer.size=scsi_buffer.limit=nRetLen;
    memcpy(scsi_buffer.data, retbuf, scsi_buffer.limit);
    scsi_buffer.disk=false;
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIbus.phase = PHASE_DI;
}


void SCSI_ModeSense(Uint8 *cdb) {
    Uint8 target = SCSIbus.target;
    
    Uint8 retbuf[256];
    MODEPAGE page;
    
    Uint32 sectors = SCSIdisk[target].size / BLOCKSIZE;
    
    Uint8 pagecontrol = (cdb[2] & 0x0C) >> 6;
    Uint8 pagecode = cdb[2] & 0x3F;
    Uint8 dbd = cdb[1] & 0x08; // disable block descriptor
    
    Log_Printf(LOG_WARN, "[SCSI] Mode Sense: page = %02x, page_control = %i, %s\n", pagecode, pagecontrol, dbd == 0x08 ? "block descriptor disabled" : "block descriptor enabled");
    
    /* Header */
    retbuf[0] = 0x00; // length of following data
    retbuf[1] = 0x00; // medium type (always 0)
    retbuf[2] = SCSIdisk[target].readonly ? 0x80 : 0x00; // if media is read-only 0x80, else 0x00
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
        Log_Printf(LOG_WARN, "[SCSI] Mode Sense: Block descriptor data: %s, size = %i blocks, blocksize = %i byte\n",
                   SCSIdisk[target].readonly ? "disk is read-only" : "disk is read/write" , sectors, BLOCKSIZE);
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
    
    scsi_buffer.disk=false;
    scsi_buffer.limit = scsi_buffer.size = retbuf[0]+1;
    if (scsi_buffer.limit > SCSI_GetTransferLength(cdb[0], cdb)) {
        scsi_buffer.limit = scsi_buffer.size = SCSI_GetTransferLength(cdb[0], cdb);
    }
    memcpy(scsi_buffer.data, retbuf, scsi_buffer.limit);
    
    SCSIdisk[target].status = STAT_GOOD;
    SCSIbus.phase = PHASE_DI;
    SCSIdisk[target].sense.code = SC_NO_ERROR;
    SCSIdisk[target].sense.valid = false;
}


void SCSI_FormatDrive(Uint8 *cdb) {
    Uint8 format_data = cdb[1]&0x10;
    
    Log_Printf(LOG_WARN, "[SCSI] Format drive command with parameters %02X\n",cdb[1]&0x1F);
    
    if (format_data) {
        Log_Printf(LOG_WARN, "[SCSI] Format drive with format data unsupported!\n");
        abort();
    } else {
        SCSIdisk[SCSIbus.target].status = STAT_GOOD;
        SCSIbus.phase = PHASE_ST;
    }
}
