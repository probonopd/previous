/* Floppy drive emulation */


#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "sysReg.h"
#include "dma.h"
#include "floppy.h"
#include "cycInt.h"
#include "file.h"
#include "statusbar.h"


#define LOG_FLP_REG_LEVEL   LOG_DEBUG
#define LOG_FLP_CMD_LEVEL   LOG_DEBUG

#define IO_SEG_MASK	0x1FFFF


/* Controller */
struct {
    Uint8 sra;      /* Status Register A (ro) */
    Uint8 srb;      /* Status Register B (ro) */
    Uint8 dor;      /* Digital Output Register (rw) */
    Uint8 msr;      /* Main Status Register (ro) */
    Uint8 dsr;      /* Data Rate Select Register (wo) */
    Uint8 fifo[16]; /* Data FIFO (rw) */
    Uint8 din;      /* Digital Input Register (wo) */
    Uint8 ccr;      /* Configuration Control Register (ro) */
    Uint8 ctrl;     /* External Control (rw) */
    
    Uint8 st[4];    /* Internal Status Registers */
    Uint8 pcn;      /* Cylinder Number */
    
    Uint8 sel;
    Uint8 eis;
} flp;

Uint8 floppy_select = 0;

/* Drives */
struct {
    Uint8 cyl;
    Uint8 head;
    Uint8 sector;
    Uint8 blocksize;
    
    FILE* dsk;
    Uint32 floppysize;
    
    Uint32 seekoffset;
    
    bool spinning;
    
    bool protected;
    bool inserted;
    bool connected;
} flpdrv[FLP_MAX_DRIVES];


/* Register bits */

#define SRA_INT         0x80
#define SRA_DRV1_N      0x40
#define SRA_STEP        0x20
#define SRA_TRK0_N      0x10
#define SRA_HDSEL       0x08
#define SRA_INDEX_N     0x04
#define SRA_WP_N        0x02
#define SRA_DIR         0x01

#define SRB_ALL1        0xC0
#define SRB_DRVSEL0_N   0x20
#define SRB_W_TOGGLE    0x10
#define SRB_R_TOGGLE    0x08
#define SRB_W_ENABLE    0x04
#define SRB_MOTEN_MASK  0x03

#define DOR_MOT3EN      0x80
#define DOR_MOT2EN      0x40
#define DOR_MOT1EN      0x20
#define DOR_MOT0EN      0x10
#define DOR_MOTEN_MASK  0xF0
#define DOR_DMA_N       0x08
#define DOR_RESET_N     0x04
#define DOR_SEL_MSK     0x03

#define STAT_RQM        0x80
#define STAT_DIO        0x40    /* read = 1 */
#define STAT_NONDMA     0x20
#define STAT_CMDBSY     0x10
#define STAT_DRV3BSY    0x08
#define STAT_DRV2BSY    0x04
#define STAT_DRV1BSY    0x02
#define STAT_DRV0BSY    0x01
#define STAT_BSY_MASK   0x0F

#define DSR_RESET       0x80
#define DSR_PDOWN       0x40
#define DSR_0           0x20
#define DSR_PRE_MASK    0x1C
#define DSR_RATE_MASK   0x03

#define DIR_DSKCHG      0x80
#define DIR_HIDENS_N    0x01

#define CCR_RATE_MASK   0x03

#define CTRL_EJECT      0x80
#define CTRL_82077      0x40
#define CTRL_DRV_ID     0x04
#define CTRL_MEDIA_ID1  0x02
#define CTRL_MEDIA_ID0  0x01


/* Functions */
void floppy_reset(bool hard);
Uint8 floppy_fifo_read(void);
void floppy_fifo_write(Uint8 val);
Uint8 floppy_dor_read(void);
void floppy_dor_write(Uint8 val);
Uint8 floppy_sra_read(void);


void FLP_StatA_Read(void) { // 0x02014100
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = floppy_sra_read();
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Status A read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void FLP_StatB_Read(void) { // 0x02014101
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = flp.srb|SRB_ALL1;
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Status B read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void FLP_DataOut_Read(void) { // 0x02014102
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = floppy_dor_read();
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Data out read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void FLP_DataOut_Write(void) {
    floppy_dor_write(IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Data out write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void FLP_Status_Read(void) { // 0x02014104
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = flp.msr;
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Main status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void FLP_DataRate_Write(void) {
    flp.dsr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Data rate write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (flp.dsr&DSR_RESET) {
        floppy_reset(false);
        flp.dsr&=~DSR_RESET;
    }
}

void FLP_FIFO_Read(void) { // 0x02014105
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = floppy_fifo_read();
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] FIFO read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void FLP_FIFO_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] FIFO write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    floppy_fifo_write(val);
}

void FLP_DataIn_Read(void) { // 0x02014107
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = flp.din;
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Data in read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void FLP_Configuration_Write(void) {
    flp.ccr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Configuration write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void FLP_Control_Read(void) { // 0x02014108
    Uint8 val;
    if (ConfigureParams.System.nMachineType==NEXT_CUBE030)
        val=flp.ctrl&~CTRL_DRV_ID;
    else
        val=flp.ctrl;
    
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = val;
    Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Control read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void FLP_Select_Write(void) {
    Uint8 val;
    val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DEBUG,"[Floppy] Select write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());

    if (ConfigureParams.System.nMachineType!=NEXT_CUBE030) {
        set_floppy_select(val&CTRL_82077,false);
    }
    
    if (val&CTRL_EJECT) {
        Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Ejecting floppy disk");
        Floppy_Eject(-1);
    }
}

void set_floppy_select(Uint8 sel, bool osp) {
    if (sel) {
        Log_Printf(LOG_DEBUG,"[%s] Selecting floppy controller",osp?"OSP":"Floppy");
        floppy_select = 1;
    } else {
        Log_Printf(LOG_FLP_REG_LEVEL,"[%s] Selecting SCSI controller",osp?"OSP":"Floppy");
        floppy_select = 0;
    }
}

enum {
    FLP_STATE_WRITE,
    FLP_STATE_READ,
    FLP_STATE_FORMAT,
    FLP_STATE_INTERRUPT,
    FLP_STATE_DONE
} flp_io_state;

int flp_sector_counter = 0;
int flp_io_drv = 0;


/* Internal registers */

#define ST0_IC      0xC0    /* Interrupt Code (0 = normal termination) */
#define ST0_SE      0x20    /* Seek End */
#define ST0_EC      0x10    /* Equipment Check */
#define ST0_H       0x04    /* Head Address */
#define ST0_DS      0x03    /* Drive Select */

#define ST1_EN      0x80    /* End of Cylinder */
#define ST1_DE      0x20    /* Data Error */
#define ST1_OR      0x10    /* Overrun/Underrun */
#define ST1_ND      0x04    /* No Data */
#define ST1_NW      0x02    /* Not writable */
#define ST1_MA      0x01    /* Missing Address Mark */

#define ST2_CM      0x40    /* Control Mark */
#define ST2_DD      0x20    /* Data Error in Data Field */
#define ST2_WC      0x10    /* Wrong Cylinder */
#define ST2_BC      0x02    /* Bad Cylinder */
#define ST2_MD      0x01    /* Missing Data Address Mark */

#define ST3_WP      0x40    /* Write Protected */
#define ST3_T0      0x10    /* Track 0 */
#define ST3_HD      0x04    /* Head Address */
#define ST3_DS      0x03    /* Drive Select */


#define IC_NORMAL   0x00
#define IC_ABNORMAL 0x40
#define IC_INV_CMD  0x80



void floppy_reset(bool hard) {
    Log_Printf(LOG_WARN,"[Floppy] Reset.");
    
    flp.sra &= ~(SRA_INT|SRA_STEP|SRA_HDSEL|SRA_DIR);
    flp.srb &= ~(SRB_R_TOGGLE|SRB_W_TOGGLE);
    flp.msr |= STAT_RQM;
    flp.msr &= ~STAT_DIO;
	
    if (hard) {
        flp_io_state = FLP_STATE_DONE;
        flp.dor &= ~DOR_RESET_N;
        flp.srb &= ~(SRB_MOTEN_MASK|SRB_DRVSEL0_N);
        flp.din &= ~DIR_HIDENS_N;
        flp.st[0]=flp.st[1]=flp.st[2]=flp.st[3]=0;
        flp.pcn=0;
    } else {
        /* Single poll interrupt after reset (delay = 250 ms) */
        flp_io_state = FLP_STATE_INTERRUPT;
        CycInt_AddRelativeInterruptUs(250*1000, 0, INTERRUPT_FLP_IO);
    }
}

/* Floppy commands */

#define CMD_MT_MSK      0x80
#define CMD_MFM_MSK     0x40
#define CMD_SK_MSK      0x20
#define CMD_OPCODE_MSK  0x1F

#define CMD_READ        0x06
#define CMD_READ_DEL    0x0C
#define CMD_WRITE       0x05
#define CMD_WRITE_DEL   0x09
#define CMD_READ_TRK    0x02
#define CMD_VERIFY      0x16
#define CMD_VERSION     0x10
#define CMD_FORMAT      0x0D
#define CMD_SCAN_E      0x11
#define CMD_SCAN_LE     0x19
#define CMD_SCAN_HE     0x1D
#define CMD_RECAL       0x07
#define CMD_INTSTAT     0x08
#define CMD_SPECIFY     0x03
#define CMD_DRV_STATUS  0x04
#define CMD_SEEK        0x0F
#define CMD_CONFIGURE   0x13
#define CMD_DUMPREG     0x0E
#define CMD_READ_ID     0x0A
#define CMD_PERPEND     0x12
#define CMD_LOCK        0x14

Uint8 cmd_data_size[] = {
    0,0,8,2,1,8,8,1,
    0,8,1,0,8,5,0,2,
    0,8,1,3,0,0,8,0,
    0,8,0,0,0,8,0,0
};

bool cmd_phase = false;
int cmd_size = 0;
int cmd_limit = 0;
Uint8 command;
Uint8 cmd_data[8];

int result_size = 0;

static void floppy_interrupt(void) {
    Log_Printf(LOG_FLP_CMD_LEVEL,"[Floppy] Interrupt.");
    
    if (result_size>0) {
        /* Go to result phase */
        flp.msr |= (STAT_RQM|STAT_DIO);
    } else {
        flp.msr |= STAT_RQM;
    }
    
    flp.msr &= ~(STAT_BSY_MASK);
    flp.sra |= SRA_INT;
    set_interrupt(INT_PHONE, SET_INT);
}

/* -- Helpers -- */

/* Geometry */
#define SIZE_720K    737280
#define SIZE_1440K  1474560
#define SIZE_2880K  2949120

#define NUM_CYLINDERS   80
#define TRACKS_PER_CYL  2

static Uint32 physical_to_logical_sector(Uint8 c, Uint8 h, Uint8 s, int drive) {
    Uint32 disksize = flpdrv[drive].floppysize;
    Uint32 blocksize = 0x80<<flpdrv[drive].blocksize;
    Uint32 spt = disksize/blocksize/TRACKS_PER_CYL/NUM_CYLINDERS;
    
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Geometry: Cylinders: %i, Tracks per cylinder: %i, Sectors per track: %i, Blocksize: %i",
               NUM_CYLINDERS,TRACKS_PER_CYL,spt,blocksize);
    
    if (s>spt) {
        Log_Printf(LOG_WARN, "[Floppy] Geometry error: sector (%i) beyond limit (%i)!",s,spt);
        flp.st[0] |= IC_ABNORMAL;
        flp.st[1] |= ST1_EN;
    }
    
    return (((c*TRACKS_PER_CYL)+h)*spt)+s-1;
}

static void check_blocksize(int drive, Uint8 blocksize) {
    if (blocksize!=flpdrv[drive].blocksize) {
        Log_Printf(LOG_WARN, "[Floppy] Geometry error: Blocksize not supported (%i)!",blocksize);
        flp.st[0] |= IC_ABNORMAL;
        flp.st[1] |= ST1_ND;
    }
}

static void check_protection(int drive) {
    if (flpdrv[drive].protected) {
        Log_Printf(LOG_WARN, "[Floppy] Protection error: Disk is read-only!");
        flp.st[0] |= IC_ABNORMAL;
        flp.st[1] |= ST1_NW;
    }
}

static void floppy_seek_track(Uint8 c, Uint8 h, int drive) {
    if (c>(NUM_CYLINDERS-1)) /* CHECK: does this cause an error? */
        c=(NUM_CYLINDERS-1);
    
    flp.st[0] |= ST0_SE;
    
    flpdrv[drive].seekoffset = (flpdrv[drive].cyl < c) ? (c - flpdrv[drive].cyl) : (flpdrv[drive].cyl - c);
    flpdrv[drive].cyl = c;
    flpdrv[drive].head = h;
}

/* Timings */
#define FLP_SEEK_TIME 200000 /* 200 ms */

static int get_sector_time(int drive) {
    switch (flpdrv[drive].floppysize) {
        case SIZE_720K: return 22000;
        case SIZE_1440K: return 11000;
        case SIZE_2880K: return 5500;
        default: return 1000;
    }
}

static int get_seek_time(int drive) {
    if (flpdrv[drive].seekoffset > NUM_CYLINDERS) {
        return FLP_SEEK_TIME;
    }
    
    return (flpdrv[drive].seekoffset * FLP_SEEK_TIME / NUM_CYLINDERS);
}

/* Media IDs for control register */
#define MEDIA_ID_NONE   0
#define MEDIA_ID_720K   3
#define MEDIA_ID_1440K  2
#define MEDIA_ID_2880K  1
#define MEDIA_ID_MSK    3

static Uint8 get_media_id(int drive) {
    if (!flpdrv[drive].inserted) {
        return MEDIA_ID_NONE;
    } else {
        switch (flpdrv[drive].floppysize) {
            case SIZE_720K: return MEDIA_ID_720K;
            case SIZE_1440K: return MEDIA_ID_1440K;
            case SIZE_2880K: return MEDIA_ID_2880K;
            default: return MEDIA_ID_NONE;
        }
    }
}

/* Validate data rate */
#define CCR_RATE250     0x02
#define CCR_RATE500     0x00
#define CCR_RATE1000    0x03

static void check_data_rate(int drive) {
    switch (flpdrv[drive].floppysize) {
        case SIZE_720K:
            if ((flp.ccr&CCR_RATE_MASK)==CCR_RATE250) {
                return;
            }
            break;
        case SIZE_1440K:
            if ((flp.ccr&CCR_RATE_MASK)==CCR_RATE500) {
                return;
            }
            break;
        case SIZE_2880K:
            if ((flp.ccr&CCR_RATE_MASK)==CCR_RATE1000) {
                return;
            }
            break;

        default:
            break;
    }
    Log_Printf(LOG_WARN, "[Floppy] Invalid data rate %02X",flp.ccr&CCR_RATE_MASK);
    flp.st[0] |= IC_ABNORMAL;
    flp.st[1] |= ST1_ND;
}

static void send_rw_status(int drive) {
    flp.st[0] |= drive|(flpdrv[drive].head<<2);
    flp.fifo[0] = flp.st[0];
    flp.fifo[1] = flp.st[1];
    flp.fifo[2] = flp.st[2];
    flp.fifo[3] = flpdrv[drive].cyl;
    flp.fifo[4] = flpdrv[drive].head;
    flp.fifo[5] = flpdrv[drive].sector;
    flp.fifo[6] = flpdrv[drive].blocksize;
    result_size = 7;
}

/* -- Floppy commands -- */

static void floppy_read(void) {
    int drive = cmd_data[0]&0x03;
    int head = (cmd_data[0]&0x04)>>2;
    Uint8 c = cmd_data[1];
    Uint8 h = cmd_data[2];
    Uint8 s = cmd_data[3];
    Uint8 bs = cmd_data[4];
    
    Uint32 sector_size, num_sectors, logical_sec;
    
    flp.st[0] = flp.st[1] = flp.st[2] = 0;
    
    /* If implied seek is enabled, seek track */
    if (flp.eis) {
        floppy_seek_track(c, h, drive);
    }
    /* Set start sector */
    flpdrv[drive].sector = s;
    
    /* Match actual track with specified track */
    if (flpdrv[drive].cyl!=c || flpdrv[drive].head!=h || flpdrv[drive].head!=head) {
        Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Read: track mismatch!");
        flp.st[0]|=IC_ABNORMAL;
        flp.st[2]|=ST2_WC;
    }
    /* Validate blocksize */
    check_blocksize(drive,bs);
    sector_size = 0x80<<bs;
    
    /* Validate data rate */
    check_data_rate(drive);

    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Read: Cylinder=%i, Head=%i, Sector=%i, Blocksize=%i",
               flpdrv[drive].cyl,flpdrv[drive].head,flpdrv[drive].sector,sector_size);
    
    /* Get sector transfer count and logical sector offset */
    num_sectors = cmd_data[5]-cmd_data[3]+1;
    logical_sec = physical_to_logical_sector(flpdrv[drive].cyl,flpdrv[drive].head,flpdrv[drive].sector,drive);
    
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Read %i sectors at offset %i",num_sectors,logical_sec);
    
    if (flp.st[0]&IC_ABNORMAL) {
        send_rw_status(drive);
        flp_io_state = FLP_STATE_INTERRUPT;
    } else {
        flp_buffer.size = 0;
        flp_buffer.limit = sector_size;
        flp_sector_counter = num_sectors;
        flp_io_drv = drive;
        flp_io_state = FLP_STATE_READ;
    }
    CycInt_AddRelativeInterruptUs(get_seek_time(drive) + get_sector_time(drive), 100, INTERRUPT_FLP_IO);
}

static void floppy_write(void) {
    int drive = cmd_data[0]&0x03;
    int head = (cmd_data[0]&0x04)>>2;
    Uint8 c = cmd_data[1];
    Uint8 h = cmd_data[2];
    Uint8 s = cmd_data[3];
    Uint8 bs = cmd_data[4];
    
    Uint32 sector_size, num_sectors, logical_sec;
    
    flp.st[0] = flp.st[1] = flp.st[2] = 0;
    
    /* If implied seek is enabled, seek track */
    if (flp.eis) {
        floppy_seek_track(c, h, drive);
    }
    /* Set start sector */
    flpdrv[drive].sector = s;
    
    /* Match actual track with specified track */
    if (flpdrv[drive].cyl!=c || flpdrv[drive].head!=h || flpdrv[drive].head!=head) {
        Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Write: track mismatch!");
        flp.st[0]|=IC_ABNORMAL;
        flp.st[2]|=ST2_WC;
    }
    /* Validate blocksize */
    check_blocksize(drive,bs);
    sector_size = 0x80<<bs;
    
    /* Validate data rate */
    check_data_rate(drive);
    
    /* Check protection */
    check_protection(drive);
    
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Write: Cylinder=%i, Head=%i, Sector=%i, Blocksize=%i",
               flpdrv[drive].cyl,flpdrv[drive].head,flpdrv[drive].sector,sector_size);
    
    /* Get sector transfer count and logical sector offset */
    num_sectors = cmd_data[5]-cmd_data[3]+1;
    logical_sec = physical_to_logical_sector(flpdrv[drive].cyl,flpdrv[drive].head,flpdrv[drive].sector,drive);
    
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Write %i sectors at offset %i",num_sectors,logical_sec);
    
    if (flp.st[0]&IC_ABNORMAL) {
        send_rw_status(drive);
        flp_io_state = FLP_STATE_INTERRUPT;
    } else {
        flp_buffer.size = 0;
        flp_buffer.limit = sector_size;
        flp_sector_counter = num_sectors;
        flp_io_drv = drive;
        flp_io_state = FLP_STATE_WRITE;
    }
    CycInt_AddRelativeInterruptUs(get_seek_time(drive) + get_sector_time(drive), 100, INTERRUPT_FLP_IO);
}

static void floppy_format(void) {
    int drive = cmd_data[0]&0x03;
    int head = (cmd_data[0]&0x04)>>2;
    
    flp.st[0] = flp.st[1] = flp.st[2] = 0;
    
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Format: Cylinder=%i, Head=%i",flpdrv[drive].cyl,head);
    
    /* Validate data rate */
    check_data_rate(drive);
    
    /* Check protection */
    check_protection(drive);
    
    if (flp.st[0]&IC_ABNORMAL) {
        send_rw_status(drive);
        flp_io_state = FLP_STATE_INTERRUPT;
    } else {
        flp_buffer.size = 0;
        flp_buffer.limit = 4;
        flp_io_drv = drive;
        flp_io_state = FLP_STATE_FORMAT;
        CycInt_AddRelativeInterruptUs(get_seek_time(drive) + get_sector_time(drive), 100, INTERRUPT_FLP_IO);
    }
}

static void floppy_read_id(void) {
    int drive = cmd_data[0]&0x03;
    int head = (cmd_data[0]&0x04)>>2;
    
    Uint32 sec_size = 0x80<<flpdrv[drive].blocksize;
    
    if (flpdrv[drive].head!=head)
        abort();
    
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Read ID: Cylinder=%i, Head=%i, Sector=%i, Blocksize=%i",
               flpdrv[drive].cyl,flpdrv[drive].head,flpdrv[drive].sector,sec_size);
    
    flp.st[0] = flp.st[1] = flp.st[2] = 0;
    
    check_data_rate(drive); /* check data rate */
    send_rw_status(drive);
    
    flp_io_state = FLP_STATE_INTERRUPT;
    CycInt_AddRelativeInterruptUs(get_seek_time(drive), 100, INTERRUPT_FLP_IO);
}

static void floppy_recalibrate(void) {
    int drive = cmd_data[0]&0x03;
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Drive = %i",drive);

    if (flpdrv[drive].connected) {
        flp.msr |= (1<<drive); /* drive busy */
        flp.dor &= 0xFC; /* CHECK: really needed? */
        flp.dor |= drive;
        flpdrv[drive].cyl = flp.pcn = 0;

        flp.st[0] = flp.st[1] = flp.st[2] = 0;
        floppy_seek_track(0, 0, drive);

        /* Done */
        flp.st[0] = IC_NORMAL|ST0_SE;
        flp.sra &= ~SRA_TRK0_N;
        
        flp_io_state = FLP_STATE_INTERRUPT;
        CycInt_AddRelativeInterruptUs(get_seek_time(drive), 100, INTERRUPT_FLP_IO);
    }
}

static void floppy_seek(Uint8 relative) {
    int drive = cmd_data[0]&0x03;
    int head = (cmd_data[0]&0x04)>>2;
    flp.st[0] = flp.st[1] = flp.st[2] = 0;
    
    if (relative) {
        Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Relative seek: Head %i: %i cylinders",head,0);
        abort();
    } else {
        Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Seek: Head %i to cylinder %i",head,flpdrv[drive].cyl);
        flp.st[0] = IC_NORMAL;

        floppy_seek_track(cmd_data[1], head, drive);
        if (!(flp.st[0]&IC_ABNORMAL)) {
            flp.pcn = flpdrv[drive].cyl;
        }
    }
        
    flp_io_state = FLP_STATE_INTERRUPT;
    CycInt_AddRelativeInterruptUs(get_seek_time(drive), 100, INTERRUPT_FLP_IO);
}

static void floppy_interrupt_status(void) {
    /* Release interrupt */
    set_interrupt(INT_PHONE, RELEASE_INT);
    flp.sra &= ~SRA_INT;
    /* Go to result phase */
    flp.msr |= (STAT_RQM|STAT_DIO);
    /* Return data */
    flp.fifo[0] = flp.st[0];
    flp.fifo[1] = flp.pcn;
    result_size = 2;
}

static void floppy_specify(void) {
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Specify: %02X %02X",cmd_data[0],cmd_data[1]);

    if (cmd_data[1]&0x01) {
        Log_Printf(LOG_WARN, "[Floppy] Specify: Non-DMA mode");
    } else {
        Log_Printf(LOG_WARN, "[Floppy] Specify: DMA mode");
    }
    flp.msr |= STAT_RQM;
}

static void floppy_configure(void) {
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Configure: %02X %02X %02X",cmd_data[0],cmd_data[1],cmd_data[2]);
    
    flp.eis = cmd_data[1]&0x40; /* Enable or disable implied seek */

    if (cmd_data[1]&0x10) {
        Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Configure: disable polling");
        if (CycInt_InterruptActive(INTERRUPT_FLP_IO)) {
            Log_Printf(LOG_WARN, "[Floppy] Disable pending reset poll interrupt");
            CycInt_RemovePendingInterrupt(INTERRUPT_FLP_IO);
        }
    }
    if (cmd_data[2]) {
        abort();
    }
    flp.msr |= STAT_RQM;
}

static void floppy_perpendicular(void) {
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Perpendicular: %02X",cmd_data[0]);
    flp.msr |= STAT_RQM;
}

static void floppy_unimplemented(void) {
    flp.st[0] = IC_INV_CMD;
    
    flp.fifo[0] = flp.st[0];
    result_size = 1;
    
    flp_io_state = FLP_STATE_INTERRUPT;
    CycInt_AddRelativeInterruptUs(1000, 100, INTERRUPT_FLP_IO);
}

static void floppy_execute_cmd(void) {
    Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Executing %02X",command);
    
    switch (command&CMD_OPCODE_MSK) {
        case CMD_READ:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Read");
            floppy_read();
            break;
        case CMD_READ_DEL:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Read and delete");
            abort();
            break;
        case CMD_WRITE:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Write");
            floppy_write();
            break;
        case CMD_WRITE_DEL:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Write and delete");
            abort();
            break;
        case CMD_READ_TRK:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Read track");
            abort();
            break;
        case CMD_VERIFY:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Verify");
            abort();
            break;
        case CMD_VERSION:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Version");
            abort();
            break;
        case CMD_FORMAT:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Format");
            floppy_format();
            break;
        case CMD_SCAN_E:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Scan equal");
            abort();
            break;
        case CMD_SCAN_LE:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Scan lower or equal");
            abort();
            break;
        case CMD_SCAN_HE:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Scan higher or equal");
            abort();
            break;
        case CMD_RECAL:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Recalibrate");
            floppy_recalibrate();
            break;
        case CMD_INTSTAT:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Interrupt status");
            floppy_interrupt_status();
            break;
        case CMD_SPECIFY:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Specify");
            floppy_specify();
            break;
        case CMD_DRV_STATUS:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Drive status");
            abort();
            break;
        case CMD_SEEK:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Seek");
            floppy_seek(command&0x80);
            break;
        case CMD_CONFIGURE:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Configure");
            floppy_configure();
            break;
        case CMD_DUMPREG:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Dump register");
            abort();
            break;
        case CMD_READ_ID:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Read ID");
            floppy_read_id();
            break;
        case CMD_PERPEND:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Perpendicular");
            floppy_perpendicular();
            break;
        case CMD_LOCK:
            Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Command: Lock");
            abort();
            break;

        default:
            Log_Printf(LOG_WARN, "[Floppy] Command: Unknown");
            floppy_unimplemented();
            break;
    }
    Statusbar_BlinkLed(DEVICE_LED_FD);
}

void floppy_fifo_write(Uint8 val) {
    
    flp.msr &= ~STAT_RQM;
    
    if (!cmd_phase) {
        cmd_phase = true;
        command = val;
        cmd_limit = cmd_size = cmd_data_size[val&CMD_OPCODE_MSK];
        flp.msr |= STAT_RQM;
    } else if (cmd_size>0) {
        cmd_data[cmd_limit-cmd_size]=val;
        cmd_size--;
        flp.msr |= STAT_RQM;
    }
    
    if (cmd_size==0) {
        cmd_phase = false;
        floppy_execute_cmd();
    }
}

Uint8 floppy_fifo_read(void) {
    int i;
    Uint8 val;
    if (result_size>0) {
        val = flp.fifo[0];
        for (i=0; i<(16-1); i++)
            flp.fifo[i]=flp.fifo[i+1];
        flp.fifo[16-1] = 0x00;
        result_size--;
        Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] FIFO reading byte, val=%02x", val);
    } else {
        Log_Printf(LOG_WARN,"[Floppy] FIFO is emtpy!");
        val = 0;
    }
    
    if (result_size==0) {
        flp.msr |= STAT_RQM;
        flp.msr &= ~STAT_DIO;
        /* FIXME: does this really belong here? */
        set_interrupt(INT_PHONE, RELEASE_INT);
        flp.sra &= ~SRA_INT;
    }
    
    return val;
}

void floppy_dor_write(Uint8 val) {
    if ((val&DOR_SEL_MSK)!=flp.sel) {
        Log_Printf(LOG_FLP_CMD_LEVEL,"[Floppy] Selecting drive %i.",val&DOR_SEL_MSK);
    }
    flp.sel = val&DOR_SEL_MSK;
    
    flp.dor &= ~DOR_SEL_MSK;
    flp.dor |= flp.sel;
    
    
    if (!(val&DOR_RESET_N)) {
        flp.dor &= ~DOR_RESET_N;
        floppy_reset(false);
    } else {
        flp.dor |= DOR_RESET_N;
        Log_Printf(LOG_FLP_REG_LEVEL,"[Floppy] Clear reset state.");
    }
    if (flpdrv[flp.sel].connected) {
        flp.ctrl &= ~CTRL_DRV_ID;

        if (val&(0x10<<flp.sel)) { /* motor enable */
            Log_Printf(LOG_FLP_CMD_LEVEL,"[Floppy] Starting motor of drive %i.",flp.sel);
            flp.ctrl &= ~MEDIA_ID_MSK;
            flp.ctrl |= get_media_id(flp.sel);
            flpdrv[flp.sel].spinning = true;
        } else {
            Log_Printf(LOG_FLP_CMD_LEVEL,"[Floppy] Stopping motor of drive %i.",flp.sel);
            flp.ctrl &= ~MEDIA_ID_MSK;          /* CHECK: really delete ID? */
            flpdrv[flp.sel].spinning = false;
        }
    } else {
        flp.ctrl |= CTRL_DRV_ID;
    }
}

Uint8 floppy_dor_read(void) {
    int i;
    Uint8 val = flp.dor&~DOR_MOTEN_MASK;  /* clear motor bits */
    for (i=0; i<FLP_MAX_DRIVES; i++) {
        if (flpdrv[i].spinning) {           /* set motor bit if motor is on */
            val |= 0x10<<i;
        }
    }
    return val;
}

Uint8 floppy_sra_read(void) {
    Uint8 val = flp.sra;
    if (!flpdrv[flp.sel].protected) {
        val|=SRA_WP_N;
    }
    return val;
}


/* -- Floppy I/O functions -- */

static void floppy_rw_nodata(void) {
    Log_Printf(LOG_WARN, "[Floppy] Write: No more data from DMA. Stopping.");
    /* Stop transfer */
    flp_sector_counter=0;
    flp.st[0] |= IC_ABNORMAL;
    flp.st[1] |= ST1_OR;
    send_rw_status(flp_io_drv);
}

static void floppy_format_done(void) {
    int drive = flp_io_drv;
    
    Log_Printf(LOG_WARN, "[Floppy] Format: No more data from DMA. Done.");
    
    flp.st[0] = IC_NORMAL;
    send_rw_status(drive);
}

static void floppy_read_sector(void) {
    int drive = flp_io_drv;
    
    /* Read from image */
    Uint32 sec_size = 0x80<<flpdrv[drive].blocksize;
    Uint32 logical_sec = physical_to_logical_sector(flpdrv[drive].cyl,flpdrv[drive].head,flpdrv[drive].sector,drive);
    
    if (flp.st[0]&IC_ABNORMAL) {
        Log_Printf(LOG_WARN, "[Floppy] Read error. Bad sector offset (%i).",logical_sec);
        flp_sector_counter=0; /* stop the transfer */
        flp_io_state = FLP_STATE_INTERRUPT;
        send_rw_status(drive);
        return;
    } else {
        Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Read sector at offset %i",logical_sec);

        flp_buffer.size = flp_buffer.limit = sec_size;
        File_Read(flp_buffer.data, flp_buffer.size, logical_sec*sec_size, flpdrv[drive].dsk);
        flpdrv[drive].sector++;
        flp_sector_counter--;
    }
    
    if (flp_sector_counter==0) {
        flp.st[0] = IC_ABNORMAL; /* Strange behavior of NeXT hardware */
        flp.st[1] |= ST1_EN;
        send_rw_status(drive);
    }
}

static void floppy_write_sector(void) {
    int drive = flp_io_drv;
    
    /* Write to image */
    Uint32 sec_size = 0x80<<flpdrv[drive].blocksize;
    Uint32 logical_sec = physical_to_logical_sector(flpdrv[drive].cyl,flpdrv[drive].head,flpdrv[drive].sector,drive);
    
    if (flp.st[0]&IC_ABNORMAL) {
        Log_Printf(LOG_WARN, "[Floppy] Write error. Bad sector offset (%i).",logical_sec);
        flp_sector_counter=0; /* stop the transfer */
        flp_io_state = FLP_STATE_INTERRUPT;
        send_rw_status(drive);
        return;
    } else {
        Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Write sector at offset %i",logical_sec);
        
        File_Write(flp_buffer.data, flp_buffer.size, logical_sec*sec_size, flpdrv[drive].dsk);
        flp_buffer.size = 0;
        flp_buffer.limit = sec_size;
        flpdrv[drive].sector++;
        flp_sector_counter--;
    }
    
    if (flp_sector_counter==0) {
        flp.st[0] = IC_ABNORMAL; /* Strange behavior of NeXT hardware */
        flp.st[1] |= ST1_EN;
        send_rw_status(drive);
    }
}

static void floppy_format_sector(void) {
    int drive = flp_io_drv;
    Uint8 c = flp_buffer.data[0];
    Uint8 h = flp_buffer.data[1];
    Uint8 s = flp_buffer.data[2];
    Uint8 bs = flp_buffer.data[3];
    
    if (c!=flpdrv[drive].cyl || h!=flpdrv[drive].head || bs!=flpdrv[drive].blocksize) {
        Log_Printf(LOG_WARN, "[Floppy] Format error. Bad sector data. Stopping.");
        flp_io_state = FLP_STATE_INTERRUPT; /* This is a hack */
        floppy_format_done();
        flp_buffer.size = 0;
        return;
    }
    flpdrv[drive].sector = s;
    
    /* Erase data */
    Uint32 sec_size = 0x80<<bs;
    Uint32 logical_sec = physical_to_logical_sector(flpdrv[drive].cyl,flpdrv[drive].head,flpdrv[drive].sector,drive);
    flp_buffer.size = sec_size;
    memset(flp_buffer.data, 0, flp_buffer.size);

    if (flp.st[0]&IC_ABNORMAL) {
        Log_Printf(LOG_WARN, "[Floppy] Format error. Bad sector offset (%i).",logical_sec);
        flp_buffer.size = flp_buffer.limit = 0;
        send_rw_status(drive);
    } else {
        Log_Printf(LOG_FLP_CMD_LEVEL, "[Floppy] Format sector at offset %i (%i/%i/%i), blocksize: %i",
                   logical_sec,c,h,s,sec_size);
        File_Write(flp_buffer.data, flp_buffer.size, logical_sec*sec_size, flpdrv[drive].dsk);
        flp_buffer.size = 0;
        flp_buffer.limit = 4;
    }
}


static Uint32 old_size;

void FLP_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    
    switch (flp_io_state) {
        case FLP_STATE_WRITE:
            if (flp_buffer.size==flp_buffer.limit) {
                floppy_write_sector();
                if (flp_sector_counter==0) { /* done */
                    floppy_interrupt();
                    flp_io_state = FLP_STATE_DONE;
                    return;
                }
            } else if (flp_buffer.size<flp_buffer.limit) { /* loop in filling mode */
                old_size = flp_buffer.size;
                dma_esp_read_memory();
                if (flp_buffer.size==old_size) {
                    floppy_rw_nodata();
                    floppy_interrupt();
                    flp_io_state = FLP_STATE_DONE;
                    return;
                }
            }
            break;
            
        case FLP_STATE_READ:
            if (flp_buffer.size==0 && flp_sector_counter>0) {
                floppy_read_sector();
            }
            if (flp_buffer.size>0) { /* loop in draining mode */
                old_size = flp_buffer.size;
                dma_esp_write_memory();
                if (flp_buffer.size==old_size) {
                    floppy_rw_nodata();
                    floppy_interrupt();
                    flp_io_state = FLP_STATE_DONE;
                    return;
                }
            }
            if (flp_buffer.size==0 && flp_sector_counter==0) { /* done */
                floppy_interrupt();
                flp_io_state = FLP_STATE_DONE;
                return;
            }
            break;
            
        case FLP_STATE_FORMAT:
            if (flp_buffer.size<flp_buffer.limit) {
                old_size = flp_buffer.size;
                dma_esp_read_memory();
                if (flp_buffer.size==old_size) {
                    floppy_format_done();
                    floppy_interrupt();
                    flp_io_state = FLP_STATE_DONE;
                    return;
                }
            } else if (flp_buffer.size==flp_buffer.limit) {
                floppy_format_sector();
            }
            break;
            
        case FLP_STATE_INTERRUPT:
            floppy_interrupt();
            flp_io_state = FLP_STATE_DONE;
            /* fall through */
            
        case FLP_STATE_DONE:
            return;
            
        default:
            return;
    }
    
    CycInt_AddRelativeInterruptUs(get_sector_time(flp_io_drv), 250, INTERRUPT_FLP_IO);
}


/* Initialize/Uninitialize floppy disks */

static void Floppy_Init(void) {
    Log_Printf(LOG_WARN, "Loading floppy disks:");
    int i;
    
    for (i=0; i<FLP_MAX_DRIVES; i++) {
        flpdrv[i].spinning=false;
        /* Check if files exist. */
        if (ConfigureParams.Floppy.drive[i].bDriveConnected) {
            flpdrv[i].connected=true;
            if (ConfigureParams.Floppy.drive[i].bDiskInserted &&
                File_Exists(ConfigureParams.Floppy.drive[i].szImageName)) {
                Floppy_Insert(i);
            } else {
                flpdrv[i].dsk = NULL;
                flpdrv[i].inserted=false;
            }
        } else {
            flpdrv[i].connected=false;
        }
    }
    
    floppy_reset(true);
}

static void Floppy_Uninit(void) {
    if (flpdrv[0].dsk)
        File_Close(flpdrv[0].dsk);
    if (flpdrv[1].dsk) {
        File_Close(flpdrv[1].dsk);
    }
    flpdrv[0].dsk = flpdrv[1].dsk = NULL;
    flpdrv[0].inserted = flpdrv[1].inserted = false;
}

static Uint32 Floppy_CheckSize(int drive) {
    Uint32 size = File_Length(ConfigureParams.Floppy.drive[drive].szImageName);
    
    switch (size) {
        case SIZE_720K:
        case SIZE_1440K:
        case SIZE_2880K:
            return size;
            
        default:
            Log_Printf(LOG_WARN, "Floppy Disk%i: Invalid size (%i byte)\n",drive,size);
            return 0;
    }
}

int Floppy_Insert(int drive) {
    Uint32 size;
    
    /* Check floppy size. */
    size = Floppy_CheckSize(drive);
    if (size) {
        flpdrv[drive].floppysize = size;
        flpdrv[drive].blocksize = 2; /* 512 byte */
        flpdrv[drive].cyl = flpdrv[drive].head = flpdrv[drive].sector = 0;
        flpdrv[drive].seekoffset = 0;
    } else {
        flpdrv[drive].dsk = NULL;
        flpdrv[drive].inserted=false;
        flpdrv[drive].floppysize = 0;
        return 1; /* bad size, do not insert */
    }
    
    if (ConfigureParams.Floppy.drive[drive].bWriteProtected) {
        flpdrv[drive].dsk = File_Open(ConfigureParams.Floppy.drive[drive].szImageName, "rb");
        if (flpdrv[drive].dsk == NULL) {
            Log_Printf(LOG_WARN, "Floppy Disk%i: Cannot open image file %s\n",
                       drive, ConfigureParams.Floppy.drive[drive].szImageName);
            flpdrv[drive].inserted=false;
            flpdrv[drive].spinning=false;
            Statusbar_AddMessage("Cannot insert floppy disk", 0);
            return 1;
        }
        flpdrv[drive].protected=true;
    } else {
        flpdrv[drive].dsk = File_Open(ConfigureParams.Floppy.drive[drive].szImageName, "rb+");
        flpdrv[drive].protected=false;
        if (flpdrv[drive].dsk == NULL) {
            flpdrv[drive].dsk = File_Open(ConfigureParams.Floppy.drive[drive].szImageName, "rb");
            if (flpdrv[drive].dsk == NULL) {
                Log_Printf(LOG_WARN, "Floppy Disk%i: Cannot open image file %s\n",
                           drive, ConfigureParams.Floppy.drive[drive].szImageName);
                flpdrv[drive].inserted=false;
                flpdrv[drive].spinning=false;
                Statusbar_AddMessage("Cannot insert floppy disk", 0);
                return 1;
            }
            flpdrv[drive].protected=true;
            Log_Printf(LOG_WARN, "Floppy Disk%i: Image file is not writable. Enabling write protection.\n",
                       drive);
        }
    }
    
    flpdrv[drive].inserted=true;
    flpdrv[drive].spinning=false;

    Log_Printf(LOG_WARN, "Floppy Disk%i: %s, %iK\n",drive,
               ConfigureParams.Floppy.drive[drive].szImageName,flpdrv[drive].floppysize/1024);
    
    Statusbar_AddMessage("Inserting floppy disk.", 0);
    
    return 0;
}

void Floppy_Eject(int drive) {
    if (drive<0) { /* Called from emulator, else called from GUI */
        drive=flp.sel;
        
        Statusbar_AddMessage("Ejecting floppy disk.", 0);
    }
    
    Log_Printf(LOG_WARN, "Unloading floppy disk %i",drive);
    Log_Printf(LOG_WARN, "Floppy disk %i: Eject",drive);
    
    File_Close(flpdrv[drive].dsk);
    flpdrv[drive].floppysize = 0;
    flpdrv[drive].blocksize = 0;
    flpdrv[drive].dsk=NULL;
    flpdrv[drive].inserted=false;
    flpdrv[drive].spinning=false;
    
    ConfigureParams.Floppy.drive[drive].bDiskInserted=false;
    ConfigureParams.Floppy.drive[drive].szImageName[0]='\0';
}

void Floppy_Reset(void) {
    Floppy_Uninit();
    Floppy_Init();
}
