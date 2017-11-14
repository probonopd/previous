/*  Previous - mo.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Canon Magneto-Optical Disk Drive and NeXT Optical Storage Processor emulation.
  
 NeXT Optical Storage Processor uses Reed-Solomon algorithm for error correction.
 It has two 1296 (128?) byte internal buffers and uses double-buffering to perform
 error correction.
  
 */

/* TODO:
 * - Fix soft timeouts when polling empty second drive
 * - Add realistic seek timings
 * - Check drive error handling (attn conditions)
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "mo.h"
#include "sysReg.h"
#include "dma.h"
#include "floppy.h"
#include "file.h"
#include "rs.h"
#include "statusbar.h"


#define LOG_MO_REG_LEVEL    LOG_DEBUG
#define LOG_MO_CMD_LEVEL    LOG_DEBUG
#define LOG_MO_ECC_LEVEL    LOG_DEBUG
#define LOG_MO_IO_LEVEL     LOG_DEBUG

#define IO_SEG_MASK	0x1FFFF


/* Registers */

struct {
    Uint8 tracknuml;
    Uint8 tracknumh;
    Uint8 sector_num;
    Uint8 sector_count;
    Uint8 intstatus;
    Uint8 intmask;
    Uint8 ctrlr_csr2;
    Uint8 ctrlr_csr1;
    Uint8 csrl;
    Uint8 csrh;
    Uint8 err_stat;
    Uint8 ecc_cnt;
    Uint8 init;
    Uint8 format;
    Uint8 mark;
    Uint8 flag[7];
} mo;
int sector_counter;

struct {
    Uint16 status;
    Uint16 dstat;
    Uint16 estat;
    Uint16 hstat;
    
    Uint8 head;
    
    Uint32 head_pos;
    Uint32 ho_head_pos;
    Uint32 sec_offset;
    
    FILE* dsk;
    
    bool spinning;
    bool spiraling;
    bool seeking;
    
    bool attn;
    bool complete;
    
    bool protected;
    bool inserted;
    bool connected;
} modrv[MO_MAX_DRIVES];

int dnum;


#define NO_HEAD     0
#define READ_HEAD   1
#define WRITE_HEAD  2
#define ERASE_HEAD  3
#define VERIFY_HEAD 4
#define RF_HEAD     5


/* Sector increment and number */
#define MOSEC_NUM_MASK      0x0F /* rw */
#define MOSEC_INCR_MASK     0xF0 /* wo */

/* Interrupt status */
#define MOINT_CMD_COMPL     0x01 /* ro */
#define MOINT_ATTN          0x02 /* ro */
#define MOINT_OPER_COMPL    0x04 /* rw */
#define MOINT_ECC_DONE      0x08 /* rw */
#define MOINT_TIMEOUT       0x10 /* rw */
#define MOINT_READ_FAULT    0x20 /* rw */
#define MOINT_PARITY_ERR    0x40 /* rw */
#define MOINT_DATA_ERR      0x80 /* rw */
#define MOINT_RESET         0x01 /* wo */
#define MOINT_GPO           0x02 /* wo */

#define MOINT_OSP_MASK      0xFC
#define MOINT_MO_MASK       0x03

/* Controller CSR 2 */
#define MOCSR2_DRIVE_SEL    0x01
#define MOCSR2_ECC_CMP      0x02
#define MOCSR2_BUF_TOGGLE   0x04
#define MOCSR2_CLR_BUFP     0x08
#define MOCSR2_ECC_BLOCKS   0x10
#define MOCSR2_ECC_MODE     0x20
#define MOCSR2_ECC_DIS      0x40
#define MOCSR2_SECT_TIMER   0x80

/* Controller CSR 1 */
/* see below (formatter commands) */

/* Drive CSR (lo and hi) */
/* see below (drive commands) */

/* Data error status */
#define ERRSTAT_ECC         0x01
#define ERRSTAT_CMP         0x02
#define ERRSTAT_TIMING      0x04
#define ERRSTAT_STARVE      0x08

/* Init */
#define MOINIT_ID_MASK      0x03
#define MOINIT_EVEN_PAR     0x04
#define MOINIT_DMA_STV_ENA  0x08
#define MOINIT_25_MHZ       0x10
#define MOINIT_ID_CMP_TRK   0x20
#define MOINIT_ECC_STV_DIS  0x40
#define MOINIT_SEC_GREATER  0x80

#define MOINIT_ID_34    0
#define MOINIT_ID_234   1
#define MOINIT_ID_1234  3
#define MOINIT_ID_0     2

/* Format */
#define MOFORM_RD_GATE_NOM  0x06
#define MOFORM_WR_GATE_NOM  0x30

#define MOFORM_RD_GATE_MIN  0x00
#define MOFORM_RD_GATE_MAX  0x0F
#define MOFORM_RD_GATE_MASK 0x0F


/* Disk layout */
#define MO_SEC_PER_TRACK    16
#define MO_TRACK_OFFSET     4096 /* offset to first logical sector of kernel driver is 4149 */
#define MO_TRACK_LIMIT      (19819-(MO_TRACK_OFFSET)) /* no more tracks beyond this offset */

#define MO_SECTORSIZE_DISK  1296 /* size of encoded sector, like stored on disk */
#define MO_SECTORSIZE_DATA  1024 /* size of decoded sector, like handled by software */


static Uint32 get_logical_sector(Uint32 sector_id) {
    Sint32 tracknum = (sector_id&0xFFFF00)>>8;
    Uint8 sectornum = sector_id&0x0F;

    tracknum-=MO_TRACK_OFFSET;
    if (tracknum<0 || tracknum>=MO_TRACK_LIMIT) {
        Log_Printf(LOG_WARN, "MO disk %i: Error! Bad sector (%i)! Disk limit exceeded.", dnum,
                   (tracknum*MO_SEC_PER_TRACK)+mo.sector_num);
        abort();
    }

    return (tracknum*MO_SEC_PER_TRACK)+sectornum;
}



/* Functions */
void mo_formatter_cmd(void);
void mo_formatter_cmd2(void);
void mo_drive_cmd(void);

void mo_reset(void);
void osp_select(int drive);

void MO_Init(void);
void MO_Uninit(void);

/* Experimental */
#define SECTOR_IO_DELAY 1250
#define CMD_DELAY       40

#define SEEK_TIMING 1

static void mo_set_signals(bool complete, bool attn, int delay);
static void mo_push_signals(bool complete, bool attn, int drive);
static void osp_poll_mo_signals(void);

static void ecc_read(void);
static void ecc_write(void);
static void ecc_verify(void);

static void mo_read_sector(Uint32 sector_id);
static void mo_write_sector(Uint32 sector_id);
static void mo_erase_sector(Uint32 sector_id);
static void mo_verify_sector(Uint32 sector_id);

static void mo_seek(Uint16 command);
static void mo_high_order_seek(Uint16 command);
static void mo_jump_head(Uint16 command);
static void mo_recalibrate(void);
static void mo_return_drive_status(void);
static void mo_return_track_addr(void);
static void mo_return_extended_status(void);
static void mo_return_hardware_status(void);
static void mo_return_version(void);
static void mo_select_head(int head);
static void mo_reset_attn_status(void);
static void mo_stop_spinning(void);
static void mo_start_spinning(void);
static void mo_eject_disk(int drv);
static void mo_start_spiraling(void);
static void mo_stop_spiraling(void);
static void mo_self_diagnostic(void);

static Uint32 get_logical_sector(Uint32 sector_id);
static void fmt_sector_done(void);
static bool fmt_match_id(Uint32 sector_id);
static void fmt_io(Uint32 sector_id);
static void ecc_toggle_buffer(void);
static void ecc_clear_buffer(void);
static void ecc_decode(void);
static void ecc_encode(void);
static void ecc_sequence_done(void);
static bool mo_drive_empty(void);
static bool mo_protected(void);
static void mo_unimplemented_cmd(void);
static void mo_spiraling_operation(void);
static Uint32 get_logical_sector(Uint32 sector_id);
static void mo_insert_disk(int drv);

static int sector_increment = 0;

static void osp_interrupt(Uint8 interrupt);


/* ------------------------ OPTICAL STORAGE PROCESSOR ------------------------ */

/* OSP registers */

void MO_TrackNumH_Read(void) { // 0x02012000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.tracknumh;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Track number hi read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_TrackNumH_Write(void) {
    mo.tracknumh=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Track number hi write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_TrackNumL_Read(void) { // 0x02012001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.tracknuml;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Track number lo read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_TrackNumL_Write(void) {
    mo.tracknuml=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Track number lo write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_SectorIncr_Read(void) { // 0x02012002
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.sector_num&MOSEC_NUM_MASK;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector increment and number read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_SectorIncr_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    mo.sector_num = val&MOSEC_NUM_MASK;
    sector_increment = (val&MOSEC_INCR_MASK)>>4;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector increment and number write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_SectorCnt_Read(void) { // 0x02012003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = sector_counter&0xFF;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector count read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_SectorCnt_Write(void) {
    sector_counter=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    if (sector_counter==0) {
        sector_counter=0x100;
    }
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Sector count write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_IntStatus_Read(void) { // 0x02012004
    osp_poll_mo_signals();
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.intstatus;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_IntStatus_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    osp_poll_mo_signals();
    mo.intstatus &= ~(val&MOINT_OSP_MASK);
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());

    if ((mo.intstatus&mo.intmask)==0) {
        set_interrupt(INT_DISK, RELEASE_INT);
    }
    if (ConfigureParams.System.nMachineType==NEXT_CUBE030) {
        set_floppy_select(val&MOINT_GPO, true);
    }
    if (val&MOINT_RESET) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Hard reset\n");
        mo_reset();
    }
}

void MO_IntMask_Read(void) { // 0x02012005
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.intmask;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt mask read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_IntMask_Write(void) {
    mo.intmask=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Interrupt mask write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());

    osp_poll_mo_signals();
    if ((mo.intstatus&mo.intmask)==0) {
        set_interrupt(INT_DISK, RELEASE_INT);
    } else {
        set_interrupt(INT_DISK, SET_INT);
    }
}

void MOctrl_CSR2_Read(void) { // 0x02012006
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.ctrlr_csr2;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO Controller] CSR2 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MOctrl_CSR2_Write(void) {
    mo.ctrlr_csr2=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO Controller] CSR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    mo_formatter_cmd2();
}

void MOctrl_CSR1_Read(void) { // 0x02012007
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.ctrlr_csr1;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO Controller] CSR1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MOctrl_CSR1_Write(void) {
    mo.ctrlr_csr1=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO Controller] CSR1 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    mo_formatter_cmd();
}

void MO_CSR_H_Read(void) { // 0x02012009
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.csrh;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] CSR hi read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_CSR_H_Write(void) {
    mo.csrh=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] CSR hi write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_CSR_L_Read(void) { // 0x02012008
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.csrl;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] CSR lo read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_CSR_L_Write(void) {
    mo.csrl=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] CSR lo write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    mo_drive_cmd();
}

void MO_ErrStat_Read(void) { // 0x0201200a
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.err_stat;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Error status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_EccCnt_Read(void) { // 0x0201200b
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.ecc_cnt;
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] ECC count read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Init_Write(void) { // 0x0201200c
    mo.init=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Init write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Format_Write(void) { // 0x0201200d
    mo.format=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Format write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Mark_Write(void) { // 0x0201200e
    mo.mark=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Mark write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag0_Read(void) { // 0x02012010
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[0];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 0 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag0_Write(void) {
    mo.flag[0]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 0 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag1_Read(void) { // 0x02012011
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[1];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag1_Write(void) {
    mo.flag[1]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 1 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag2_Read(void) { // 0x02012012
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[2];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 2 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag2_Write(void) {
    mo.flag[2]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag3_Read(void) { // 0x02012013
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[3];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 3 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag3_Write(void) {
    mo.flag[3]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 3 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag4_Read(void) { // 0x02012014
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[4];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 4 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag4_Write(void) {
    mo.flag[4]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 4 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag5_Read(void) { // 0x02012015
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[5];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 5 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag5_Write(void) {
    mo.flag[5]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 5 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag6_Read(void) { // 0x02012016
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = mo.flag[6];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 6 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void MO_Flag6_Write(void) {
    mo.flag[6]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_MO_REG_LEVEL,"[MO] Flag 6 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

#if 0
/* Register debugging */
static void print_regs(void) {
    int i;
    Log_Printf(LOG_WARN,"sector ID:  %02X%02X%02X",mo.tracknumh,mo.tracknuml,mo.sector_num);
    Log_Printf(LOG_WARN,"head pos:   %04X",modrv[dnum].head_pos);
    Log_Printf(LOG_WARN,"sector cnt: %02X",sector_counter&0xFF);
    Log_Printf(LOG_WARN,"intstatus:  %02X",mo.intstatus);
    Log_Printf(LOG_WARN,"intmask:    %02X",mo.intmask);
    Log_Printf(LOG_WARN,"ctrlr csr2: %02X",mo.ctrlr_csr2);
    Log_Printf(LOG_WARN,"ctrlr csr1: %02X",mo.ctrlr_csr1);
    Log_Printf(LOG_WARN,"drive csrl: %02X",mo.csrl);
    Log_Printf(LOG_WARN,"drive csrh: %02X",mo.csrh);
    Log_Printf(LOG_WARN,"errstat:    %02X",mo.err_stat);
    Log_Printf(LOG_WARN,"ecc count:  %02X",mo.ecc_cnt);
    Log_Printf(LOG_WARN,"init:       %02X",mo.init);
    Log_Printf(LOG_WARN,"format:     %02X",mo.format);
    Log_Printf(LOG_WARN,"mark:       %02X",mo.mark);
    for (i=0; i<7; i++) {
        Log_Printf(LOG_WARN,"flag %i:     %02X",i+1,mo.flag[i]);
    }
}
#endif

void osp_interrupt(Uint8 interrupt) {
    mo.intstatus|=interrupt;
    if (interrupt&mo.intmask) {
        set_interrupt(INT_DISK, SET_INT);
    }
}


enum {
    ECC_MODE_READ,
    ECC_MODE_WRITE,
    ECC_MODE_VERIFY
} ecc_mode;

enum {
    ECC_STATE_FILLING,
    ECC_STATE_DRAINING,
    ECC_STATE_ECCING,
    ECC_STATE_WAITING,
    ECC_STATE_DONE
} ecc_state;

/* Formatter commands */

#define FMT_RESET       0x00
#define FMT_ECC_READ    0x80
#define FMT_ECC_WRITE   0x40
#define FMT_RD_STAT     0x20
#define FMT_ID_READ     0x10
#define FMT_VERIFY      0x08
#define FMT_ERASE       0x04
#define FMT_READ        0x02
#define FMT_WRITE       0x01

enum {
    FMT_MODE_READ,
    FMT_MODE_WRITE,
    FMT_MODE_ERASE,
    FMT_MODE_VERIFY,
    FMT_MODE_READ_ID,
    FMT_MODE_IDLE
} fmt_mode;

bool write_timing;

void mo_formatter_cmd(void) {
    
    if (mo.ctrlr_csr1==FMT_RESET) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[OSP] Formatter command: Reset (%02X)\n", mo.ctrlr_csr1);
        if (fmt_mode!=FMT_MODE_IDLE) {
            Log_Printf(LOG_WARN,"[OSP] Warning: Formatter reset while busy!\n");
        }
        fmt_mode = FMT_MODE_IDLE;
        ecc_state = ECC_STATE_DONE;
        mo.ecc_cnt=0;
        mo.err_stat=0;
        return;
    }
    if (mo.ctrlr_csr1&FMT_ECC_READ) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[OSP] Formatter command: ECC Read (%02X)\n", mo.ctrlr_csr1);
        ecc_read();
    }
    if (mo.ctrlr_csr1&FMT_ECC_WRITE) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[OSP] Formatter command: ECC Write (%02X)\n", mo.ctrlr_csr1);
        ecc_write();
    }
    if (mo.ctrlr_csr1&FMT_RD_STAT) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[OSP] Formatter command: Read Status (%02X)\n", mo.ctrlr_csr1);
        mo.csrh = (modrv[dnum].status>>8)&0xFF;
        mo.csrl = modrv[dnum].status&0xFF;
    }
    if (mo.ctrlr_csr1&FMT_ID_READ) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[OSP] Formatter command: ID Read (%02X)\n", mo.ctrlr_csr1);
        fmt_mode = FMT_MODE_READ_ID;
    }
    if (mo.ctrlr_csr1&FMT_VERIFY) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[OSP] Formatter command: Verify (%02X)\n", mo.ctrlr_csr1);
        fmt_mode = FMT_MODE_VERIFY;
    }
    if (mo.ctrlr_csr1&FMT_ERASE) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[OSP] Formatter command: Erase (%02X)\n", mo.ctrlr_csr1);
        fmt_mode = FMT_MODE_ERASE;
    }
    if (mo.ctrlr_csr1&FMT_READ) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[OSP] Formatter command: Read (%02X)\n", mo.ctrlr_csr1);
        fmt_mode = FMT_MODE_READ;
    }
    if (mo.ctrlr_csr1&FMT_WRITE) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[OSP] Formatter command: Write (%02X)\n", mo.ctrlr_csr1);
        write_timing = false;
        fmt_mode = FMT_MODE_WRITE;
    }
}

void fmt_sector_done(void) {
    Uint16 track = (mo.tracknumh<<8)|mo.tracknuml;
    mo.sector_num+=sector_increment;
    track+=mo.sector_num/MO_SEC_PER_TRACK;
    mo.sector_num%=MO_SEC_PER_TRACK;
    mo.tracknumh = (track>>8)&0xFF;
    mo.tracknuml = track&0xFF;
    /* CHECK: decrement with sector_increment value? */
    sector_counter--;
    /* Check if the operation is complete */
    if (sector_counter==0) {
        fmt_mode = FMT_MODE_IDLE;
        osp_interrupt(MOINT_OPER_COMPL);
    }
}

int sector_timer=0;
#define SECTOR_TIMEOUT_COUNT    100 /* FIXME: what is the correct value? */
bool fmt_match_id(Uint32 sector_id) {
    if ((mo.init&MOINIT_ID_MASK)==MOINIT_ID_0) {
        Log_Printf(LOG_MO_CMD_LEVEL, "[OSP] Sector ID matching disabled!");
        abort(); /* CHECK: this routine is critical to disk image corruption, check if it gives correct results */
        return true;
    }
    
    Uint32 fmt_id = (mo.tracknumh<<16)|(mo.tracknuml<<8)|mo.sector_num;
    
    if (mo.init&MOINIT_ID_CMP_TRK) {
        Log_Printf(LOG_MO_CMD_LEVEL, "[OSP] Compare only track ID.");
        fmt_id=(fmt_id>>8)&0xFFFF;
        sector_id=(sector_id>>8)&0xFFFF;
    }
    
    if (sector_id==fmt_id) {
        sector_timer=0;
        return true;
    } else {
        Log_Printf(LOG_MO_CMD_LEVEL, "[OSP] Sector ID mismatch (Sector ID=%06X, Looking for %06X)",
                   sector_id,fmt_id);
        if (mo.ctrlr_csr2&MOCSR2_SECT_TIMER) {
            sector_timer++;
            if (sector_timer>SECTOR_TIMEOUT_COUNT) {
                Log_Printf(LOG_WARN, "[OSP] Sector timeout!");
                sector_timer=0;
                fmt_mode=FMT_MODE_IDLE;
                osp_interrupt(MOINT_TIMEOUT);
            }
        }
        return false;
    }
}

void fmt_io(Uint32 sector_id) {

    switch (fmt_mode) {
        case FMT_MODE_IDLE:
            return;
        case FMT_MODE_READ_ID:
            mo.tracknumh = (sector_id>>16)&0xFF;
            mo.tracknuml = (sector_id>>8)&0xFF;
            mo.sector_num = sector_id&0x0F;
            osp_interrupt(MOINT_OPER_COMPL);
            return;
        case FMT_MODE_READ:
            if (modrv[dnum].head!=READ_HEAD) {
                abort();
            }
            if (fmt_match_id(sector_id)) {
                /* First read sector from disk to ECC buffer */
                mo_read_sector(sector_id);
                /* Then decode data and write to memory using DMA */
                ecc_read();
                fmt_sector_done();
            }
            break;
        case FMT_MODE_WRITE:
            if (modrv[dnum].head!=WRITE_HEAD) {
                abort();
            }
            /* WARNING: first sector must be mismatch to pre-fill the ECC buffer for writing */
            if (fmt_match_id(sector_id) && write_timing) {
                /* Write sector from ECC buffer to disk */
                mo_write_sector(sector_id);
                fmt_sector_done();
            } else {
                write_timing = true;
            }
            /* (Re)fill ECC buffer from memory using DMA and encode data */
            ecc_write();
            break;
        case FMT_MODE_ERASE:
            if (modrv[dnum].head!=ERASE_HEAD) {
                abort();
            }
            if (fmt_match_id(sector_id)) {
                mo_erase_sector(sector_id);
                fmt_sector_done();
            }
            break;
        case FMT_MODE_VERIFY:
            if (modrv[dnum].head!=VERIFY_HEAD) {
                abort();
            }
            if (fmt_match_id(sector_id)) {
                /* First read sector from disk to ECC buffer */
                mo_verify_sector(sector_id);
                /* Then verify data */
                ecc_verify();
                fmt_sector_done();
            }
            break;
            
        default:
            abort();
            break;
    }
}


/* Drive selection (formatter command 2) */
void osp_select(int drive) {
    Log_Printf(LOG_MO_CMD_LEVEL, "[OSP] Selecting drive %i",drive);
    dnum=drive;
    if (!modrv[dnum].connected) {
        Log_Printf(LOG_MO_CMD_LEVEL, "[OSP] Selection failed! Drive %i not connected.",drive);
    }
}

/* Check for MO drive signals (used for interrupt status register) */
void osp_poll_mo_signals(void) {
    if (modrv[dnum].complete) {
        mo.intstatus |= MOINT_CMD_COMPL;
    } else {
        mo.intstatus &= ~MOINT_CMD_COMPL;
    }
    if (modrv[dnum].attn) {
        mo.intstatus |= MOINT_ATTN;
    } else {
        mo.intstatus &= ~MOINT_ATTN;
    }
}

void ecc_toggle_buffer(void) {
    if (eccin==0) {
        eccout=0;
        eccin=1;
    } else {
        eccout=1;
        eccin=0;
    }
    Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC switching buffer (in: %i, out: %i)",eccin,eccout);
}

void ecc_clear_buffer(void) {
    ecc_buffer[eccin].size=ecc_buffer[eccout].size=0;
    ecc_buffer[eccin].limit=ecc_buffer[eccout].limit=MO_SECTORSIZE_DATA;
}

void mo_formatter_cmd2(void) {
    if (mo.ctrlr_csr2&MOCSR2_BUF_TOGGLE) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] Toggle ECC buffer.");
        ecc_toggle_buffer();
    }
    if (mo.ctrlr_csr2&MOCSR2_ECC_CMP) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC compare.");
    }
    if (mo.ctrlr_csr2&MOCSR2_CLR_BUFP) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] Clear ECC buffer.");
        ecc_clear_buffer();
    }
    if (mo.ctrlr_csr2&MOCSR2_ECC_BLOCKS) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC blocks.");
    }
    if (mo.ctrlr_csr2&MOCSR2_ECC_MODE) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC decoding mode.");
    } else {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC encoding mode.");
    }
    if (mo.ctrlr_csr2&MOCSR2_SECT_TIMER) {
        Log_Printf(LOG_MO_CMD_LEVEL, "[OSP] Sector timer enabled.");
    } else {
        Log_Printf(LOG_MO_CMD_LEVEL, "[OSP] Sector timer disabled.");
    }
    if (mo.ctrlr_csr2&MOCSR2_ECC_DIS) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] Disable ECC passthrough.");
    }
    
    osp_select(mo.ctrlr_csr2&MOCSR2_DRIVE_SEL);
}


/* ECC emulation:
 *
 * read     mem <----- buf <-de-- disk
 * write    mem -----> buf --en-> disk
 * verify              buf <-de-- disk
 *
 * ecc_dis
 * read     mem <-en-- buf
 * write    mem -----> buf
 *
 * ecc_dis|ecc_mode
 * read     mem <----- buf
 * write    mem --de-> buf
 *
 */

#define ECC_DELAY SECTOR_IO_DELAY/5 /* must be a fraction of sector delay */

bool ecc_repeat=false; /* This is for ECC blocks */

int eccin=0;
int eccout=1;

void ecc_decode(void) {
    if (mo.ctrlr_csr2&MOCSR2_ECC_DIS && !(mo.ctrlr_csr2&MOCSR2_ECC_MODE)) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC decoding disabled.");
        if (mo.ctrlr_csr2&MOCSR2_ECC_BLOCKS && ecc_repeat==true) {
            ecc_toggle_buffer();
        }
    } else if (ecc_buffer[eccin].size==MO_SECTORSIZE_DISK) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC decoding buffer.");
        ecc_buffer[eccin].limit=ecc_buffer[eccin].size=MO_SECTORSIZE_DATA;
        
        int num_errors = rs_decode(ecc_buffer[eccin].data);
        
        if (num_errors<0) {
            Log_Printf(LOG_WARN, "[OSP] ECC: Sector has uncorrectable errors!");
            mo.err_stat = ERRSTAT_ECC;
            osp_interrupt(MOINT_DATA_ERR);
            /* CHECK: Stop ECC and formatter? */
        } else {
            Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC: Number of corrected errors: %i\n",num_errors);
            
            if (mo.ecc_cnt==0) {
                mo.ecc_cnt=num_errors;
            }
        }
        
        ecc_toggle_buffer();
    } else {
        Log_Printf(LOG_WARN, "[OSP] ECC buffer is not ready (%i bytes)!",ecc_buffer[eccin].size);
        abort();
    }
}
void ecc_encode(void) {
    if (mo.ctrlr_csr2&MOCSR2_ECC_DIS && mo.ctrlr_csr2&MOCSR2_ECC_MODE) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC encoding disabled.");
        if (mo.ctrlr_csr2&MOCSR2_ECC_BLOCKS && ecc_repeat==false) {
            ecc_toggle_buffer();
        }
    } else if (ecc_buffer[eccin].limit==MO_SECTORSIZE_DATA) {
        Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC encoding buffer.");
        ecc_buffer[eccin].limit=ecc_buffer[eccin].size=MO_SECTORSIZE_DISK;

        rs_encode(ecc_buffer[eccin].data);

        ecc_toggle_buffer();
    } else {
        Log_Printf(LOG_WARN, "[OSP] ECC buffer is not ready (%i bytes)!",ecc_buffer[eccin].size);
        abort();
    }
}

void ecc_write(void) {
    if (ecc_state!=ECC_STATE_DONE) {
        Log_Printf(LOG_MO_ECC_LEVEL,"[OSP] Warning: ECC not accepting command (busy %i)", ecc_state);
        return;
    }
    ecc_mode=ECC_MODE_WRITE;
    ecc_state=ECC_STATE_FILLING;
    if (mo.ctrlr_csr2&MOCSR2_ECC_BLOCKS) {
        ecc_repeat=true;
    }
    ecc_buffer[eccin].size=0; /* FIXME: find a better place for this */
    ecc_buffer[eccin].limit=MO_SECTORSIZE_DATA; /* and this */
    CycInt_AddRelativeInterruptUsCycles(ECC_DELAY, 80, INTERRUPT_ECC_IO);
}
void ecc_read(void) {
    if (ecc_state!=ECC_STATE_DONE) {
        Log_Printf(LOG_WARN,"[OSP] Warning: ECC not accepting command (busy %i)", ecc_state);
        return;
    }
    ecc_mode=ECC_MODE_READ;
    ecc_state=ECC_STATE_ECCING;
    if (mo.ctrlr_csr2&MOCSR2_ECC_BLOCKS) {
        ecc_repeat=true;
    }
    CycInt_AddRelativeInterruptUsCycles(ECC_DELAY, 80, INTERRUPT_ECC_IO);
}
void ecc_verify(void) {
    if (ecc_state!=ECC_STATE_DONE) {
        Log_Printf(LOG_WARN,"[OSP] Warning: ECC not accepting command (busy %i)", ecc_state);
        return;
    }
    ecc_mode=ECC_MODE_VERIFY;
    ecc_state=ECC_STATE_ECCING;
    CycInt_AddRelativeInterruptUsCycles(ECC_DELAY, 80, INTERRUPT_ECC_IO);
}
void ecc_sequence_done(void) {
    if (ecc_repeat==true) {
        ecc_repeat=false;
        if (ecc_mode==ECC_MODE_WRITE) {
            ecc_buffer[eccin].size=0; /* FIXME: find a better place for this */
            ecc_state=ECC_STATE_FILLING;
        } else {
            ecc_state=ECC_STATE_ECCING;
        }
        CycInt_AddRelativeInterruptUsCycles(ECC_DELAY, 80, INTERRUPT_ECC_IO);
        return;
    }

    ecc_state=ECC_STATE_DONE;
    if (mo.ctrlr_csr2&MOCSR2_ECC_DIS) {
        osp_interrupt(MOINT_ECC_DONE);
    }
}
Uint32 old_size;
void ECC_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    
    switch (ecc_state) {
        case ECC_STATE_FILLING:
            if (ecc_buffer[eccin].size<ecc_buffer[eccin].limit) {
                if (mo.ctrlr_csr2&MOCSR2_ECC_MODE) {
                    ecc_buffer[eccin].limit=MO_SECTORSIZE_DISK;
                } else {
                    ecc_buffer[eccin].limit=MO_SECTORSIZE_DATA;
                }
                old_size=ecc_buffer[eccin].size;
                dma_mo_read_memory();
                
                if (ecc_buffer[eccin].size==old_size) {
                    Log_Printf(LOG_WARN,"[OSP] No more data! ECC starve! (%i byte)", old_size);
                    mo.err_stat = ERRSTAT_STARVE;
                    osp_interrupt(MOINT_DATA_ERR);
                }
            }
            if (ecc_buffer[eccin].size==ecc_buffer[eccin].limit) {
                ecc_state=ECC_STATE_ECCING;
            }
            break;
        case ECC_STATE_ECCING:
            if (ecc_mode==ECC_MODE_WRITE) {
                if (mo.ctrlr_csr2&MOCSR2_ECC_DIS) {
                    ecc_decode();
                    ecc_sequence_done();
                    return;
                } else { /* Go to disk write */
                    ecc_encode();
                    ecc_state=ECC_STATE_WAITING;
                    if (sector_counter==1) {
                        osp_interrupt(MOINT_ECC_DONE);
                    }
                    break;
                }
            } else { /* mode is read or verify */
                if (mo.ctrlr_csr2&MOCSR2_ECC_DIS) {
                    if (mo.ctrlr_csr2&MOCSR2_ECC_BLOCKS) {
                        ecc_buffer[eccin].limit=ecc_buffer[eccin].size=MO_SECTORSIZE_DATA;
                    }
                    ecc_encode();
                } else { /* From disk read */
                    if (ecc_buffer[eccin].size!=MO_SECTORSIZE_DISK) {
                        Log_Printf(LOG_WARN, "[OSP] ECC waiting for disk read!");
                        break; /* Loop and wait for disk read */
                    }
                    ecc_decode();
                    if (sector_counter==0) {
                        osp_interrupt(MOINT_ECC_DONE);
                    }
                    if (ecc_mode==ECC_MODE_VERIFY) {
                        ecc_clear_buffer();
                        ecc_sequence_done();
                        return;
                    }
                }
                ecc_state=ECC_STATE_DRAINING;
                break;
            }
        case ECC_STATE_DRAINING:
            old_size=ecc_buffer[eccout].size;
            dma_mo_write_memory();
            if (ecc_buffer[eccout].size==old_size) {
                Log_Printf(LOG_WARN,"[OSP] DMA not ready! Stopping.");
                ecc_sequence_done();
                return;
            }
            if (ecc_buffer[eccout].size==0) {
                dma_mo_write_memory(); /* Flush buffer */
                ecc_sequence_done();
                return;
            }
            break;
        case ECC_STATE_WAITING:
            if (ecc_buffer[eccout].size==0) {
                ecc_sequence_done();
                if (sector_counter>0) {
                    ecc_write();
                }
                return;
            }
            Log_Printf(LOG_MO_ECC_LEVEL, "[OSP] ECC waiting for disk write!");
            break;

        default:
            Log_Printf(LOG_WARN, "[OSP] Warning: ECC was reset while busy!");
            return;
    }
    
    CycInt_AddRelativeInterruptUsCycles(ECC_DELAY, 80, INTERRUPT_ECC_IO);
}


/* ------------------------ MAGNETO-OPTICAL DISK DRIVE ------------------------ */

/* I/O functions */

void mo_read_sector(Uint32 sector_id) {
    Uint32 sector_num = get_logical_sector(sector_id);
    
    Log_Printf(LOG_MO_IO_LEVEL, "MO disk %i: Read sector at offset %i (%i sectors remaining)",
               dnum, sector_num, sector_counter-1);
    
    File_Read(ecc_buffer[eccin].data, MO_SECTORSIZE_DISK, sector_num*MO_SECTORSIZE_DISK, modrv[dnum].dsk);
    
    ecc_buffer[eccin].limit = ecc_buffer[eccin].size = MO_SECTORSIZE_DISK;
}

void mo_write_sector(Uint32 sector_id) {
    Uint32 sector_num = get_logical_sector(sector_id);
    
    Log_Printf(LOG_MO_IO_LEVEL, "MO disk %i: Write sector at offset %i (%i sectors remaining)",
               dnum, sector_num, sector_counter-1);
    
    if (ecc_buffer[eccout].limit==MO_SECTORSIZE_DISK) {
        File_Write(ecc_buffer[eccout].data, MO_SECTORSIZE_DISK, sector_num*MO_SECTORSIZE_DISK, modrv[dnum].dsk);

        ecc_buffer[eccout].size = 0;
        ecc_buffer[eccout].limit = MO_SECTORSIZE_DATA;
    } else {
        Log_Printf(LOG_WARN, "MO disk %i: Incomplete write (in: size=%i limit=%i, out: size=%i limit=%i)!", dnum,
                   ecc_buffer[eccin].size, ecc_buffer[eccin].limit, ecc_buffer[eccout].size, ecc_buffer[eccout].limit);
        abort();
    }
}

void mo_erase_sector(Uint32 sector_id) {
    Uint32 sector_num = get_logical_sector(sector_id);
    
    Log_Printf(LOG_MO_IO_LEVEL, "MO disk %i: Erase sector at offset %i (%i sectors remaining)",
               dnum, sector_num, sector_counter-1);
    
    Uint8 erase_buf[MO_SECTORSIZE_DISK];
    memset(erase_buf, 0xFF, MO_SECTORSIZE_DISK);
    
    File_Write(erase_buf, MO_SECTORSIZE_DISK, sector_num*MO_SECTORSIZE_DISK, modrv[dnum].dsk);
}

void mo_verify_sector(Uint32 sector_id) {
    Uint32 sector_num = get_logical_sector(sector_id);
    
    Log_Printf(LOG_MO_IO_LEVEL, "MO disk %i: Verify sector at offset %i (%i sectors remaining)",
               dnum, sector_num, sector_counter-1);
    
    File_Read(ecc_buffer[eccin].data, MO_SECTORSIZE_DISK, sector_num*MO_SECTORSIZE_DISK, modrv[dnum].dsk);
    
    ecc_buffer[eccin].limit = ecc_buffer[eccin].size = MO_SECTORSIZE_DISK;
}


/* Drive commands */

#define DRV_SEK     0x0000 /* seek (last 12 bits are track position) */
#define DRV_HOS     0xA000 /* high order seek (last 4 bits are high order (<<12) track position) */
#define DRV_REC     0x1000 /* recalibrate */
#define DRV_RDS     0x2000 /* return drive status */
#define DRV_RCA     0x2200 /* return current track address */
#define DRV_RES     0x2800 /* return extended status */
#define DRV_RHS     0x2A00 /* return hardware status */
#define DRV_RGC     0x3000 /* return general config */
#define DRV_RVI     0x3F00 /* return drive version information */
#define DRV_SRH     0x4100 /* select read head */
#define DRV_SVH     0x4200 /* select verify head */
#define DRV_SWH     0x4300 /* select write head */
#define DRV_SEH     0x4400 /* select erase head */
#define DRV_SFH     0x4500 /* select RF head */
#define DRV_RID     0x5000 /* reset attn and status */
#define DRV_RJ      0x5100 /* relative jump (see below) */
#define DRV_SPM     0x5200 /* stop motor */
#define DRV_STM     0x5300 /* start motor */
#define DRV_LC      0x5400 /* lock cartridge */
#define DRV_ULC     0x5500 /* unlock cartridge */
#define DRV_EC      0x5600 /* eject */
#define DRV_SOO     0x5900 /* spiral operation on */
#define DRV_SOF     0x5A00 /* spiral operation off */
#define DRV_RSD     0x8000 /* request self-diagnostic */
#define DRV_SD      0xB000 /* send data (last 12 bits used) */

/* Relative jump:
 * bits 0 to 3: offset (signed -8 (0x8) to +7 (0x7)
 * bits 4 to 6: head select
 */

/* Head select for relative jump */
#define RJ_READ     0x10
#define RJ_VERIFY   0x20
#define RJ_WRITE    0x30
#define RJ_ERASE    0x40

/* Drive status information */

/* Disk status (returned for DRV_RDS) */
#define DS_INSERT   0x0004 /* load completed */
#define DS_RESET    0x0008 /* power on reset */
#define DS_SEEK     0x0010 /* address fault */
#define DS_CMD      0x0020 /* invalid or unimplemented command */
#define DS_INTFC    0x0040 /* interface fault */
#define DS_I_PARITY 0x0080 /* interface parity error */
#define DS_STOPPED  0x0200 /* not spinning */
#define DS_SIDE     0x0400 /* media upside down */
#define DS_SERVO    0x0800 /* servo not ready */
#define DS_POWER    0x1000 /* laser power alarm */
#define DS_WP       0x2000 /* disk write protected */
#define DS_EMPTY    0x4000 /* no disk inserted */
#define DS_BUSY     0x8000 /* execute busy */

/* Extended status (returned for DRV_RES) */
#define ES_RF       0x0002 /* RF detected */
#define ES_WR_INH   0x0008 /* write inhibit (high temperature) */
#define ES_WRITE    0x0010 /* write mode failed */
#define ES_COARSE   0x0020 /* coarse seek failed */
#define ES_TEST     0x0040 /* test write failed */
#define ES_SLEEP    0x0080 /* sleep/wakeup failed */
#define ES_LENS     0x0100 /* lens out of range */
#define ES_TRACKING 0x0200 /* tracking servo failed */
#define ES_PLL      0x0400 /* PLL failed */
#define ES_FOCUS    0x0800 /* focus failed */
#define ES_SPEED    0x1000 /* not at speed */
#define ES_STUCK    0x2000 /* disk cartridge stuck */
#define ES_ENCODER  0x4000 /* linear encoder failed */
#define ES_LOST     0x8000 /* tracing failure */

/* Hardware status (returned for DRV_RHS) */
#define HS_LASER    0x0040 /* laser power failed */
#define HS_INIT     0x0080 /* drive init failed */
#define HS_TEMP     0x0100 /* high drive temperature */
#define HS_CLAMP    0x0200 /* spindle clamp misaligned */
#define HS_STOP     0x0400 /* spindle stop timeout */
#define HS_TEMPSENS 0x0800 /* temperature sensor failed */
#define HS_LENSPOS  0x1000 /* lens position failure */
#define HS_SERVOCMD 0x2000 /* servo command failure */
#define HS_SERVOTO  0x4000 /* servo timeout failure */
#define HS_HEAD     0x8000 /* head select failure */

/* Version information (returned for DRV_RVI) */
#define VI_VERSION  0x0880

void mo_drive_cmd(void) {

    if (!modrv[dnum].connected) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Drive %i not connected.\n", dnum);
        return;
    }

    Uint16 command = (mo.csrh<<8) | mo.csrl;
    
    /* Command in progress */
    modrv[dnum].complete=false;
    
    if ((command&0xF000)==DRV_SEK) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Seek (%04X)\n", command);
        mo_seek(command);
    } else if ((command&0xF000)==DRV_SD) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Send Data (%04X)\n", command);
        abort();
    } else if ((command&0xFF00)==DRV_RJ) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Relative Jump (%04X)\n", command);
        mo_jump_head(command);
    } else if ((command&0xFFF0)==DRV_HOS) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: High Order Seek (%04X)\n", command);
        mo_high_order_seek(command);
    } else {
    
        switch (command&0xFFFF) {
            case DRV_REC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Recalibrate (%04X)\n", command);
                mo_recalibrate();
                break;
            case DRV_RDS:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Drive Status (%04X)\n", command);
                mo_return_drive_status();
                break;
            case DRV_RCA:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Current Track Address (%04X)\n", command);
                mo_return_track_addr();
                break;
            case DRV_RES:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Extended Status (%04X)\n", command);
                mo_return_extended_status();
                break;
            case DRV_RHS:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Hardware Status (%04X)\n", command);
                mo_return_hardware_status();
                break;
            case DRV_RGC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return General Config (%04X)\n", command);
                abort();
                break;
            case DRV_RVI:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Return Version Information (%04X)\n", command);
                mo_return_version();
                break;
            case DRV_SRH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Read Head (%04X)\n", command);
                mo_select_head(READ_HEAD);
                break;
            case DRV_SVH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Verify Head (%04X)\n", command);
                mo_select_head(VERIFY_HEAD);
                break;
            case DRV_SWH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Write Head (%04X)\n", command);
                mo_select_head(WRITE_HEAD);
                break;
            case DRV_SEH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select Erase Head (%04X)\n", command);
                mo_select_head(ERASE_HEAD);
                break;
            case DRV_SFH:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Select RF Head (%04X)\n", command);
                mo_select_head(RF_HEAD);
                break;
            case DRV_RID:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Reset Attn and Status (%04X)\n", command);
                mo_reset_attn_status();
                break;
            case DRV_SPM:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Stop Spindle Motor (%04X)\n", command);
                mo_stop_spinning();
                break;
            case DRV_STM:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Start Spindle Motor (%04X)\n", command);
                mo_start_spinning();
                break;
            case DRV_LC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Lock Cartridge (%04X)\n", command);
                abort();
                break;
            case DRV_ULC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Unlock Cartridge (%04X)\n", command);
                abort();
                break;
            case DRV_EC:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Eject (%04X)\n", command);
                mo_eject_disk(-1);
                break;
            case DRV_SOO:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Start Spiraling (%04X)\n", command);
                mo_start_spiraling();
                break;
            case DRV_SOF:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Stop Spiraling (%04X)\n", command);
                mo_stop_spiraling();
                break;
            case DRV_RSD:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Request Self-Diagnostic (%04X)\n", command);
                mo_self_diagnostic();
                break;
                
            default:
                Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Unimplemented command! (%04X)\n", command);
                mo_unimplemented_cmd();
                break;
        }
    }
    Statusbar_BlinkLed(DEVICE_LED_OD);
}


bool mo_drive_empty(void) {
    if (!modrv[dnum].inserted) {
        Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Drive %i: No disk inserted.\n", dnum);
        modrv[dnum].dstat |= DS_EMPTY;
        mo_set_signals(true, true, CMD_DELAY);
        return true;
    } else {
        return false;
    }
}

bool mo_protected(void) {
    if (modrv[dnum].protected) {
        if (modrv[dnum].head==ERASE_HEAD || modrv[dnum].head==WRITE_HEAD) {
            Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Drive command: Drive %i: Disk is write protected!\n", dnum);
            modrv[dnum].dstat|=DS_WP;
            modrv[dnum].head=NO_HEAD;
            mo_set_signals(true, true, CMD_DELAY);
            return true;
        }
    }
    return false;
}

void mo_seek(Uint16 command) {
#if SEEK_TIMING
    int seek_time=modrv[dnum].head_pos;
#endif
    if (mo_drive_empty()) {
        return;
    }
    modrv[dnum].seeking = true;
    modrv[dnum].head_pos = (modrv[dnum].ho_head_pos&0xF000) | (command&0x0FFF);
#if SEEK_TIMING
    if (seek_time>modrv[dnum].head_pos) {
        seek_time=seek_time-modrv[dnum].head_pos;
    } else {
        seek_time=modrv[dnum].head_pos-seek_time;
    }
    if (seek_time>95000) {
        seek_time=95000;
    }
    seek_time+=5000;

    mo_set_signals(true, false, seek_time);
#else
    mo_set_signals(true, false, CMD_DELAY);
#endif
}

void mo_high_order_seek(Uint16 command) {
    if (mo_drive_empty()) {
        return;
    }
    if ((command&0xF)>4) {
        modrv[dnum].dstat|=DS_SEEK;
        mo_set_signals(true, true, CMD_DELAY);
    } else {
        modrv[dnum].ho_head_pos = (command&0xF)<<12;
        mo_set_signals(true, false, CMD_DELAY);
    }
}

void mo_jump_head(Uint16 command) {
    if (mo_drive_empty()) {
        return;
    }
    modrv[dnum].seeking = true;

    int offset = command&0x7;
    if (command&0x8) {
        offset = 8 - offset;
        modrv[dnum].head_pos-=offset;
    } else {
        modrv[dnum].head_pos+=offset;
    }
    modrv[dnum].sec_offset=0;
    
    switch (command&0xF0) {
        case RJ_READ:
            modrv[dnum].head=READ_HEAD;
            break;
        case RJ_VERIFY:
            modrv[dnum].head=VERIFY_HEAD;
            break;
        case RJ_WRITE:
            modrv[dnum].head=WRITE_HEAD;
            break;
        case RJ_ERASE:
            modrv[dnum].head=ERASE_HEAD;
            break;
            
        default:
            modrv[dnum].head=NO_HEAD;
            break;
    }
    Log_Printf(LOG_MO_CMD_LEVEL,"[MO] Relative Jump: %i sectors %s (%s head)\n", offset*16,
               (command&0x8)?"back":"forward",
               (command&0xF0)==RJ_READ?"read":
               (command&0xF0)==RJ_VERIFY?"verify":
               (command&0xF0)==RJ_WRITE?"write":
               (command&0xF0)==RJ_ERASE?"erase":"unknown");
    
    if (mo_protected()) {
        return;
    }
#if SEEK_TIMING
    mo_set_signals(true, false, 1600);
#else
    mo_set_signals(true, false, CMD_DELAY);
#endif
}

void mo_recalibrate(void) {
    if (mo_drive_empty()) {
        return;
    }
    modrv[dnum].head_pos = 0;
    modrv[dnum].sec_offset = 0;
    modrv[dnum].spiraling = false;
    
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_return_drive_status(void) {
    if (!modrv[dnum].spinning) {
        modrv[dnum].dstat|=DS_STOPPED;
    }
    if (!modrv[dnum].inserted) {
        modrv[dnum].dstat|=DS_EMPTY;
    }
    modrv[dnum].status = modrv[dnum].dstat;
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_return_track_addr(void) {
    modrv[dnum].status = modrv[dnum].head_pos;
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_return_extended_status(void) {
    modrv[dnum].status = modrv[dnum].estat;
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_return_hardware_status(void) {
    modrv[dnum].status = modrv[dnum].hstat;
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_return_version(void) {
    modrv[dnum].status = VI_VERSION;
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_select_head(int head) {
    if (mo_drive_empty()) {
        return;
    }
    modrv[dnum].head = head;
    if (mo_protected()) {
        return;
    }
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_reset_attn_status(void) {
    modrv[dnum].dstat=modrv[dnum].estat=modrv[dnum].hstat=0;
    modrv[dnum].attn=false;
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_stop_spinning(void) {
    if (mo_drive_empty()) {
        return;
    }
    Statusbar_AddMessage("Stop magneto-optical disk spin.", 0);
    modrv[dnum].spinning=false;
    modrv[dnum].spiraling=false;
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_start_spinning(void) {
    if (mo_drive_empty()) {
        return;
    }
    Statusbar_AddMessage("Spin-up magneto-optical disk.", 0);
    modrv[dnum].dstat &= ~DS_STOPPED;
    modrv[dnum].spinning=true;
    mo_set_signals(true, false, 1600000);
}

void mo_eject_disk(int drv) {
    if (drv<0) { /* Called from emulator, else called from GUI */
        drv=dnum;
        if (mo_drive_empty())
            return;
        
        Statusbar_AddMessage("Ejecting magneto-optical disk.", 0);
        mo_set_signals(true, false, CMD_DELAY);
    }

    Log_Printf(LOG_WARN, "MO disk %i: Eject",drv);
    
    File_Close(modrv[drv].dsk);
    modrv[drv].dsk=NULL;
    modrv[drv].inserted=false;
    modrv[drv].spinning=false;
    modrv[drv].spiraling=false;
    
    ConfigureParams.MO.drive[drv].bDiskInserted=false;
    ConfigureParams.MO.drive[drv].szImageName[0]='\0';
}

void mo_insert_disk(int drv) {
    Log_Printf(LOG_WARN, "MO disk %i: Insert",drv);
    
    if (ConfigureParams.MO.drive[drv].bWriteProtected) {
        modrv[drv].dsk = File_Open(ConfigureParams.MO.drive[drv].szImageName, "rb");
        if (modrv[drv].dsk == NULL) {
            Log_Printf(LOG_WARN, "MO Disk%i: Cannot open image file %s\n",
                       drv, ConfigureParams.MO.drive[drv].szImageName);
            modrv[drv].inserted=false;
            modrv[drv].protected=false;
            Statusbar_AddMessage("Cannot insert magneto-optical disk.", 0);
            return;
        } else {
            modrv[drv].inserted=true;
            modrv[drv].protected=true;
        }
    } else {
        modrv[drv].dsk = File_Open(ConfigureParams.MO.drive[drv].szImageName, "rb+");
        if (modrv[drv].dsk == NULL) {
            modrv[drv].dsk = File_Open(ConfigureParams.MO.drive[drv].szImageName, "rb");
            if (modrv[drv].dsk == NULL) {
                Log_Printf(LOG_WARN, "MO Disk%i: Cannot open image file %s\n",
                           drv, ConfigureParams.MO.drive[drv].szImageName);
                modrv[drv].inserted=false;
                modrv[drv].protected=false;
                Statusbar_AddMessage("Cannot insert magneto-optical disk.", 0);
                return;
            } else {
                modrv[drv].inserted=true;
                modrv[drv].protected=true;
            }
        } else {
            modrv[drv].inserted=true;
            modrv[drv].protected=false;
        }
    }

    Statusbar_AddMessage("Inserting magneto-optical disk.", 0);
    modrv[drv].dstat&=~DS_EMPTY;
    modrv[drv].dstat|=DS_INSERT;
    modrv[drv].spinning=false;
    modrv[drv].spiraling=false;
    mo_push_signals(true, false, drv);
}

void mo_start_spiraling(void) {
    if (mo_drive_empty()) {
        return;
    }
    if (!modrv[dnum].spinning) {
        modrv[dnum].dstat|=DS_STOPPED;
        mo_set_signals(true, true, CMD_DELAY);
        return;
    }

    if (!modrv[0].spiraling && !modrv[1].spiraling) { /* periodic disk operation already active? */
        CycInt_AddRelativeInterruptUsCycles(SECTOR_IO_DELAY, 400, INTERRUPT_MO_IO);
    }
    modrv[dnum].spiraling=true;

    mo_set_signals(true, false, CMD_DELAY);
}

void mo_stop_spiraling(void) {
    if (mo_drive_empty()) {
        return;
    }
    modrv[dnum].spiraling=false;
    mo_set_signals(true, false, CMD_DELAY);
}

void mo_spiraling_operation(void) {
    if (!modrv[0].spiraling && !modrv[1].spiraling) { /* this stops periodic disk operation */
        return; /* nothing to do */
    }
    
    int i;
    for (i=0; i<MO_MAX_DRIVES; i++) {
        if (modrv[i].spiraling && !modrv[i].seeking) {
            
            /* If the drive is selected, connect to formatter */
            if (i==dnum) {
                fmt_io((modrv[i].head_pos<<8)|modrv[i].sec_offset);
            }
            
            /* Continue spiraling */
            modrv[i].sec_offset++;
            modrv[i].head_pos+=modrv[i].sec_offset/MO_SEC_PER_TRACK;
            modrv[i].sec_offset%=MO_SEC_PER_TRACK;
        }
    }
    CycInt_AddRelativeInterruptUsCycles(SECTOR_IO_DELAY, 400, INTERRUPT_MO_IO);
}

void mo_self_diagnostic(void) {
    mo_set_signals(true, false, CMD_DELAY);
}

void MO_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();

    mo_spiraling_operation();
}

void mo_unimplemented_cmd(void) {
    modrv[dnum].dstat|=DS_CMD;
    mo_set_signals(true, true, CMD_DELAY);
}

void mo_reset(void) {
    int i;
    for (i=0; i<MO_MAX_DRIVES; i++) {
        if (modrv[i].connected) {
            modrv[i].head=NO_HEAD;
            modrv[i].head_pos=modrv[i].ho_head_pos=0;
            modrv[i].sec_offset=0;
            
            modrv[i].dstat=DS_RESET;
            modrv[i].estat=modrv[i].hstat=0;
            if (modrv[i].inserted) { /* CHECK: really spin up on reset? */
                modrv[i].spinning=true;
            } else {
                modrv[i].spinning=false;
            }
            modrv[i].spiraling=false;
            
            if (!modrv[i].inserted) {
                modrv[i].dstat|=DS_EMPTY;
            } else if (!modrv[i].spinning) {
                modrv[i].dstat|=DS_STOPPED;
            }
            modrv[i].attn=false;
            modrv[i].complete=true;
        }
    }
}


/* MO drive signals */

void mo_push_signals(bool complete, bool attn, int drive) {
    if (drive<0) {
        Log_Printf(LOG_WARN, "[MO] Error: No drive specified for delayed interrupt.");
        abort();
    }
    bool interrupt = false;
    
    if (!modrv[drive].complete) {
        modrv[drive].complete=complete;
        if (modrv[drive].complete) {
            if (drive==dnum && mo.intmask&MOINT_CMD_COMPL) {
                interrupt=true;
            }
        }
    }
    if (!modrv[drive].attn) {
        modrv[drive].attn=attn;
        if (modrv[drive].attn) {
            if (drive==dnum && mo.intmask&MOINT_ATTN) {
                interrupt=true;
            }
        }
    }
    
    modrv[drive].seeking=false;

    if (interrupt) {
        set_interrupt(INT_DISK, SET_INT);
    }
}

bool delayed_compl;
bool delayed_attn;
int delayed_drive=-1;

void mo_set_signals(bool complete, bool attn, int delay) {
    if (delay>0) {
        if (delayed_drive>=0) {
            if (delayed_drive!=dnum) {
                Log_Printf(LOG_WARN, "[MO] Warning: Delayed interrupt from other drive (%i) in progress!",delayed_drive);
                mo_push_signals(delayed_compl, delayed_attn, delayed_drive);
                CycInt_RemovePendingInterrupt(INTERRUPT_MO);
            } else {
                Log_Printf(LOG_WARN, "[MO] Warning: Delayed interrupt already in progress!");
            }
        }
        delayed_drive=dnum;
        delayed_compl=complete;
        delayed_attn=attn;
        CycInt_AddRelativeInterruptUsCycles(delay, CMD_DELAY, INTERRUPT_MO);
    } else {
        mo_push_signals(complete, attn, dnum);
    }
}

void MO_InterruptHandler(void) {
    CycInt_AcknowledgeInterrupt();
    
    mo_push_signals(delayed_compl,delayed_attn,delayed_drive);
    
    delayed_compl=false;
    delayed_attn=false;
    delayed_drive=-1;
}


/* Initialize/Uninitialize MO disks */
void MO_Init(void) {
    Log_Printf(LOG_WARN, "Loading magneto-optical disks:");
    int i;
    
    for (i=0; i<MO_MAX_DRIVES; i++) {
        modrv[i].spinning=false;
        modrv[i].spiraling=false;
        /* Check if files exist. */
        if (ConfigureParams.MO.drive[i].bDriveConnected) {
            modrv[i].connected=true;
            modrv[i].complete=true;
            modrv[i].attn=false;
            modrv[i].dstat=modrv[i].estat=modrv[i].hstat=0;
            if (ConfigureParams.MO.drive[i].bDiskInserted &&
                File_Exists(ConfigureParams.MO.drive[i].szImageName)) {
                if (ConfigureParams.MO.drive[i].bWriteProtected) {
                    modrv[i].dsk = File_Open(ConfigureParams.MO.drive[i].szImageName, "rb");
                    if (modrv[i].dsk == NULL) {
                        Log_Printf(LOG_WARN, "MO Disk%i: Cannot open image file %s\n",
                                   i, ConfigureParams.MO.drive[i].szImageName);
                        modrv[i].inserted=false;
                        modrv[i].protected=false;
                    } else {
                        modrv[i].inserted=true;
                        modrv[i].protected=true;
                    }
                } else {
                    modrv[i].dsk = File_Open(ConfigureParams.MO.drive[i].szImageName, "rb+");
                    if (modrv[i].dsk == NULL) {
                        modrv[i].dsk = File_Open(ConfigureParams.MO.drive[i].szImageName, "rb");
                        if (modrv[i].dsk == NULL) {
                            Log_Printf(LOG_WARN, "MO Disk%i: Cannot open image file %s\n",
                                       i, ConfigureParams.MO.drive[i].szImageName);
                            modrv[i].inserted=false;
                            modrv[i].protected=false;
                        } else {
                            modrv[i].inserted=true;
                            modrv[i].protected=true;
                        }
                    } else {
                        modrv[i].inserted=true;
                        modrv[i].protected=false;
                    }
                }
            } else {
                modrv[i].dsk = NULL;
                modrv[i].inserted=false;
            }
        } else {
            modrv[i].connected=false;
            modrv[i].complete=false;
            modrv[i].attn=false;
        }

        Log_Printf(LOG_WARN, "MO Disk%i: %s\n",i,ConfigureParams.MO.drive[i].szImageName);
    }
    
    /* Initialize formatter variables */
    ecc_state=ECC_STATE_DONE;
}

void MO_Uninit(void) {
    if (modrv[0].dsk)
        File_Close(modrv[0].dsk);
    if (modrv[1].dsk) {
        File_Close(modrv[1].dsk);
    }
    modrv[0].dsk = modrv[1].dsk = NULL;
    modrv[0].inserted = modrv[1].inserted = false;
}

void MO_Insert(int drive) {
    Log_Printf(LOG_WARN, "Loading magneto-optical disk:");
    Log_Printf(LOG_WARN, "MO Disk%i: %s\n",drive,ConfigureParams.MO.drive[drive].szImageName);

    mo_insert_disk(drive);
}

void MO_Eject(int drive) {
    Log_Printf(LOG_WARN, "Unloading magneto-optical disk %i",drive);

    mo_eject_disk(drive);
}

void MO_Reset(void) {
    MO_Uninit();
    MO_Init();
}
