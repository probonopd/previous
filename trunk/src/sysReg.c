/* NeXT system registers emulation */

#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "sysReg.h"


#define IO_SEG_MASK	0x1FFFF


static Uint8 scr2_0=0;
static Uint8 scr2_1=0;
static Uint8 scr2_2=0;
static Uint8 scr2_3=0;

static Uint32 intStat=0x00000000;
static Uint32 intMask=0x00000000;


/* RTC NVRAM */

// file mon/nvram.h
// struct nvram_info {
// #define	NI_RESET	9
// 	u_int	ni_reset : 4,
// #define	SCC_ALT_CONS	0x08000000
// 		ni_alt_cons : 1,
// #define	ALLOW_EJECT	0x04000000
// 		ni_allow_eject : 1,
// 		ni_vol_r : 6,
// 		ni_brightness : 6,
// #define	HW_PWD	0x6
// 		ni_hw_pwd : 4,
// 		ni_vol_l : 6,
// 		ni_spkren : 1,
// 		ni_lowpass : 1,
// #define	BOOT_ANY	0x00000002
// 		ni_boot_any : 1,
// #define	ANY_CMD		0x00000001
// 		ni_any_cmd : 1;
// #define	NVRAM_HW_PASSWD	6
// 	u_char ni_ep[NVRAM_HW_PASSWD];
// #define	ni_enetaddr	ni_ep
// #define	ni_hw_passwd	ni_ep
// 	u_short ni_simm;		/* 4 SIMMs, 4 bits per SIMM */
// 	char ni_adobe[2];
// 	u_char ni_pot[3];
// 	u_char	ni_new_clock_chip : 1,
// 		ni_auto_poweron : 1,
// 		ni_use_console_slot : 1,	/* Console slot was set by user. */
// 		ni_console_slot : 2,		/* Preferred console dev slot>>1 */
// 		ni_use_parity_mem : 1,	/* Use parity RAM if available? */
// 		: 2;
// #define	NVRAM_BOOTCMD	12
// 	char ni_bootcmd[NVRAM_BOOTCMD];
// 	u_short ni_cksum;
// };

// #define	N_brightness	0
// #define	N_volume_l	1
// #define	N_volume_r	2

/* nominal values during self test */
// #define	BRIGHT_NOM	20
// #define	VOL_NOM		0

/* bits in ni_pot[0] */
// #define POT_ON			0x01
// #define EXTENDED_POT		0x02
// #define LOOP_POT		0x04
// #define	VERBOSE_POT		0x08
// #define	TEST_DRAM_POT		0x10
// #define	BOOT_POT		0x20
// #define	TEST_MONITOR_POT	0x40

// Uint8 rtc_ram[32];

Uint8 rtc_ram[32]={ // values from nextcomputers.org forums
    0x94,0x0f,0x40,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0xfb,0x6d,0x00,0x00,0x4b,0x00,
    0x41,0x00,0x20,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x84,0x7e
};

static char rtc_ram_info[1024];
char * get_rtc_ram_info(void) {
    char buf[256];
    int sum;
    int i;
    int ni_vol_l,ni_vol_r,ni_brightness;
    int ni_hw_pwd,ni_spkren,ni_lowpass;
    sprintf(buf,"Rtc info:\n");
    strcpy(rtc_ram_info,buf);
    
    // struct nvram_info {
    // #define	NI_RESET	9
    // 	u_int	ni_reset : 4,
    
    sprintf(buf,"RTC RESET:x%1X ",rtc_ram[0]>>4);
    strcat(rtc_ram_info,buf);	
    
    // #define	SCC_ALT_CONS	0x08000000
    // 		ni_alt_cons : 1,
    if (rtc_ram[0]&0x08) strcat(rtc_ram_info,"ALT_CONS ");
    // #define	ALLOW_EJECT	0x04000000
    // 		ni_allow_eject : 1,
    if (rtc_ram[0]&0x04) strcat(rtc_ram_info,"ALLOW_EJECT ");
    // 		ni_vol_r : 6,
    // 		ni_brightness : 6,
    // #define	HW_PWD	0x6
    // 		ni_hw_pwd : 4,
    // 		ni_vol_l : 6,
    // 		ni_spkren : 1,
    // 		ni_lowpass : 1,
    // #define	BOOT_ANY	0x00000002
    // 		ni_boot_any : 1,
    // #define	ANY_CMD		0x00000001	
    // 		ni_any_cmd : 1;
    
    ni_vol_r=(((rtc_ram[0]&0x3)<<4)|((rtc_ram[1]&0xF0)>>4));
    ni_brightness=(((rtc_ram[1]&0xF)<<2)|((rtc_ram[2]&0xC0)>>6));
    ni_vol_l=((rtc_ram[2]&0x3F)<<2);
    ni_hw_pwd=(rtc_ram[3]&0xF0)>>4;
    sprintf(buf,"VOL_R:x%1X BRIGHT:x%1X HWPWD:x%1X VOL_L:x%1X",ni_vol_r,ni_brightness,ni_vol_l,ni_hw_pwd);
    strcat(rtc_ram_info,buf);	
    
    if (rtc_ram[3]&0x08) strcat(rtc_ram_info,"SPK_ENABLE ");
    if (rtc_ram[3]&0x04) strcat(rtc_ram_info,"LOW_PASS ");
    if (rtc_ram[3]&0x02) strcat(rtc_ram_info,"BOOT_ANY ");
    if (rtc_ram[3]&0x01) strcat(rtc_ram_info,"ANY_CMD ");
    
    
    
    // #define	NVRAM_HW_PASSWD	6
    // 	u_char ni_ep[NVRAM_HW_PASSWD];
    
    sprintf(buf,"NVRAM_HW_PASSWD:%2X %2X %2X %2X %2X %2X ",rtc_ram[4],rtc_ram[5],rtc_ram[6],rtc_ram[7],rtc_ram[8],rtc_ram[9]);
    strcat(rtc_ram_info,buf);	
    // #define	ni_enetaddr	ni_ep
    // #define	ni_hw_passwd	ni_ep
    // 	u_short ni_simm;		/* 4 SIMMs, 4 bits per SIMM */
    sprintf(buf,"SIMM:%1X %1X %1X %1X ",rtc_ram[10]>>4,rtc_ram[10]&0x0F,rtc_ram[11]>>4,rtc_ram[11]&0x0F);
    strcat(rtc_ram_info,buf);
    
    
    // 	char ni_adobe[2];
    sprintf(buf,"ADOBE:%2X %2X ",rtc_ram[12],rtc_ram[13]);
    strcat(rtc_ram_info,buf);
    
    // 	u_char ni_pot[3];
    sprintf(buf,"POT:%2X %2X %2X ",rtc_ram[14],rtc_ram[15],rtc_ram[16]);
    strcat(rtc_ram_info,buf);
    
    // 	u_char	ni_new_clock_chip : 1,
    // 		ni_auto_poweron : 1,
    // 		ni_use_console_slot : 1,	/* Console slot was set by user. */
    // 		ni_console_slot : 2,		/* Preferred console dev slot>>1 */
    // 		ni_use_parity_mem : 1,	/* Use parity RAM if available? */
    // 		: 2;
    if (rtc_ram[17]&0x80) strcat(rtc_ram_info,"NEW_CLOCK_CHIP ");
    if (rtc_ram[17]&0x40) strcat(rtc_ram_info,"AUTO_POWERON ");
    if (rtc_ram[17]&0x20) strcat(rtc_ram_info,"CONSOLE_SLOT ");
    
    sprintf(buf,"console_slot:%X ",(rtc_ram[17]&0x18)>>3);
    strcat(rtc_ram_info,buf);
    
    if (rtc_ram[17]&0x04) strcat(rtc_ram_info,"USE_PARITY ");
    
    
    strcat(rtc_ram_info,"boot_command:");
    for (i=0;i<12;i++) {
        if ((rtc_ram[18+i]>=0x20) && (rtc_ram[18+i]<=0x7F)) {
            sprintf(buf,"%c",rtc_ram[18+i]);
            strcat(rtc_ram_info,buf);
        }
    }
    
    strcat(rtc_ram_info," ");
    sprintf(buf,"CKSUM:%2X %2X ",rtc_ram[30],rtc_ram[31]);
    strcat(rtc_ram_info,buf);
    
    
    sum=0;
    for (i=0;i<30;i+=2) {
        sum+=(rtc_ram[i]<<8)|(rtc_ram[i+1]);
        if (sum>=0x10000) { sum-=0x10000;
            sum+=1;
        }
    }
    
    sum=0xFFFF-sum;
    
    sprintf(buf,"CALC_CKSUM:%04X ",sum&0xFFFF);
    strcat(rtc_ram_info,buf);
    
    // #define	NVRAM_BOOTCMD	12
    // 	char ni_bootcmd[NVRAM_BOOTCMD];
    // 	u_short ni_cksum;
    // };
    
    return rtc_ram_info;
}



/* System Control Register 1

 bits 0:1 cpu speed; 0 = 40 MHz, 1 = 20 MHz, 2 = 25MHz, 3 = 33 MHz (meaningless on color Slab)
 bits 2:3 reserved: 0
 bits 4:5 mem speed; 0 = 120ns, 1 = 100ns, 2 = 80ns, 3 = 60ns (meaningless on color Slab)
 bits 6:7 video mem speed; 0 = 120ns, 1 = 100ns, 2 = 80ns, 3 = 60ns (meaningless on color Slab)
 bits 8:11 board revision; for 030 Cube: 0 = DCD input inverted, 1 = DCD polarity fixed, 2 = must disable DSP mem before reset; for all other systems: 0
 bits 12:15 cpu type; 0 = Cube 030, 1 = mono Slab, 2 = Cube 040, 3 = color Slab
 bits 16:23 dma rev: 1
 bits 24:27 reserved: 0
 bits 28:31 slot ID: 0
 
 
 for Cube 040:
 0000 0000 0000 0001 0011 0000 0101 0011
 00 01 20 52
 
 for Cube 030:
 0000 0000 0000 0001 0000 0001 0101 0011
 00 01 01 52
 */


void SCR1_Read0(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x00; // slot ID 0
}
void SCR1_Read1(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x01; // dma rev 1
}
void SCR1_Read2(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    if(ConfigureParams.System.nCpuLevel == 3)
        IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x01; // Cube 030, board rev 1
    else
        IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x20; // Cube 040, board rev 0
}
void SCR1_Read3(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x52; // vmem speed 100ns, mem speed 100ns, cpu speed 25 MHz
}



/* System Control Register 2 
 
 s_dsp_reset : 1,
 s_dsp_block_end : 1,
 s_dsp_unpacked : 1,
 s_dsp_mode_B : 1,
 s_dsp_mode_A : 1,
 s_remote_int : 1,
 s_local_int : 2,
 s_dram_256K : 4,
 s_dram_1M : 4,
 s_timer_on_ipl7 : 1,
 s_rom_wait_states : 3,
 s_rom_1M : 1,
 s_rtdata : 1,
 s_rtclk : 1,
 s_rtce : 1,
 s_rom_overlay : 1,
 s_dsp_int_en : 1,
 s_dsp_mem_en : 1,
 s_reserved : 4,
 s_led : 1;
 
 */

#define SCR2_RTDATA		0x04
#define SCR2_RTCLK		0x02
#define SCR2_RTCE		0x01

#define SCR2_LED		0x01
#define SCR2_ROM		0x80


void SCR2_Write0(void)
{	
    //	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
	scr2_0=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}

void SCR2_Read0(void)
{
    //	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=scr2_0;
}

void SCR2_Write1(void)
{	
    //	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
	scr2_1=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}

void SCR2_Read1(void)
{
    //	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=scr2_1;
}

void SCR2_Write2(void)
{	
	static int phase=0;
	static Uint8 rtc_command=0;
	static Uint8 rtc_value=0;
	static Uint8 rtc_return=0;
	static Uint8 rtc_status=0x90;	// FTU at startup 0x90 for MCS1850
    
	Uint8 old_scr2_2=scr2_2;
    //	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
	scr2_2=IoMem[IoAccessCurrentAddress & 0x1FFFF];
    
    /*
     if ((old_scr2_2&SCR2_RTCE)!=(scr2_2&SCR2_RTCE))
     Log_Printf(LOG_WARN,"SCR2 RTCE change at $%08x val=%x PC=$%08x\n",
     IoAccessCurrentAddress,scr2_2&SCR2_RTCE,m68k_getpc());
     
     if ((old_scr2_2&SCR2_RTCLK)!=(scr2_2&SCR2_RTCLK))
     Log_Printf(LOG_WARN,"SCR2 RTCLK change at $%08x val=%x PC=$%08x\n",
     IoAccessCurrentAddress,scr2_2&SCR2_RTCLK,m68k_getpc());
     
     if ((old_scr2_2&SCR2_RTDATA)!=(scr2_2&SCR2_RTDATA))
     Log_Printf(LOG_WARN,"SCR2 RTDATA change at $%08x val=%x PC=$%08x\n",
     IoAccessCurrentAddress,scr2_2&SCR2_RTDATA,m68k_getpc());
     */
    
    // and now some primitive handling
    // treat only if CE is set to 1
	if (scr2_2&SCR2_RTCE) {
        if (phase==-1) phase=0;
        // if we are in going down clock... do something
        if (((old_scr2_2&SCR2_RTCLK)!=(scr2_2&SCR2_RTCLK)) && ((scr2_2&SCR2_RTCLK)==0) ) {
            if (phase<8)
                rtc_command=(rtc_command<<1)|((scr2_2&SCR2_RTDATA)?1:0);
            if ((phase>=8) && (phase<16)) {
                rtc_value=(rtc_value<<1)|((scr2_2&SCR2_RTDATA)?1:0);
                
                // if we read RAM register, output RT_DATA bit
                if (rtc_command<=0x1F) {
                    scr2_2=scr2_2&(~SCR2_RTDATA);
                    if (rtc_ram[rtc_command]&(0x80>>(phase-8)))
                        scr2_2|=SCR2_RTDATA;
                    rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
                }
                // read the status 0x30
                if (rtc_command==0x30) {
                    scr2_2=scr2_2&(~SCR2_RTDATA);
                    // for now status = 0x98 (new rtc + FTU)
                    if (rtc_status&(0x80>>(phase-8)))
                        scr2_2|=SCR2_RTDATA;
                    rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
                }
                // read the status 0x31
                if (rtc_command==0x31) {
                    scr2_2=scr2_2&(~SCR2_RTDATA);
                    // for now 0x00
                    if (0x00&(0x80>>(phase-8)))
                        scr2_2|=SCR2_RTDATA;
                    rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
                }
                if ((rtc_command>=0x20) && (rtc_command<=0x2F)) {
                    scr2_2=scr2_2&(~SCR2_RTDATA);
                    // for now 0x00
                    if (0x00&(0x80>>(phase-8)))
                        scr2_2|=SCR2_RTDATA;
                    rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
                }
                
            }
            
            phase++;
            if (phase==16) {
                Log_Printf(LOG_WARN,"SCR2 RTC command complete %x %x %x at PC=$%08x\n",
                           rtc_command,rtc_value,rtc_return,m68k_getpc());
                if ((rtc_command>=0x80) && (rtc_command<=0x9F))
                    rtc_ram[rtc_command-0x80]=rtc_value;
                
                // write to x30 register
                if (rtc_command==0xB1) {
                    // clear FTU
                    if (rtc_value & 0x04) {
                        rtc_status=rtc_status&(~0x18);
                        intStat=intStat&(~0x04);
                    }
                }
            }
        }
	} else {
        // else end or abort
		phase=-1;
		rtc_command=0;
		rtc_value=0;
	}	
    
}

void SCR2_Read2(void)
{
    //	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    //	IoMem[IoAccessCurrentAddress & 0x1FFFF]=scr2_2 & (SCR2_RTDATA|SCR2_RTCLK|SCR2_RTCE)); // + data
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=scr2_2;
}

void SCR2_Write3(void)
{	
	Uint8 old_scr2_3=scr2_3;
    //	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
	scr2_3=IoMem[IoAccessCurrentAddress & 0x1FFFF];
	if ((old_scr2_3&SCR2_ROM)!=(scr2_3&SCR2_ROM))
		Log_Printf(LOG_WARN,"SCR2 ROM change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_3&SCR2_ROM,m68k_getpc());
	if ((old_scr2_3&SCR2_LED)!=(scr2_3&SCR2_LED))
		Log_Printf(LOG_WARN,"SCR2 LED change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_3&SCR2_LED,m68k_getpc());
}


void SCR2_Read3(void)
{
    //	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=scr2_3;
}



/* Interrupt Status Register */

void IntRegStatRead(void) {
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, intStat);
//	IoMem[0x02007000 & 0x1FFFF]=intStat >> 24;
//  IoMem[0x02007001 & 0x1FFFF]=intStat >> 16;
//  IoMem[0x02007002 & 0x1FFFF]=intStat >> 8;
//  IoMem[0x02007003 & 0x1FFFF]=intStat >> 0;
}

void IntRegStatWrite(void) {
    intStat = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
//	intStat=(IoMem[0x02007000 & 0x1FFFF] << 24) | (IoMem[0x02007001 & 0x1FFFF] << 16) | (IoMem[0x02007002 & 0x1FFFF] << 8) | IoMem[0x02007003 & 0x1FFFF];
}

void set_interrupt(Uint32 interrupt_val, Uint8 int_set_release) {
    
    Uint8 interrupt_level;
    
    if(int_set_release == SET_INT) {
        intStat = intStat | interrupt_val;
    } else {
        intStat = intStat & ~interrupt_val; // set bit to 0 - does this work ??
    }
    
    switch (interrupt_val) {
        case INT_SOFT1: interrupt_level = 1;
            break;
        case INT_SOFT2: interrupt_level = 2;
            break;
        case INT_POWER:
        case INT_KEYMOUSE:
        case INT_MONITOR:
        case INT_VIDEO:
        case INT_DSP_L3:
        case INT_PHONE:
        case INT_SOUND_OVRUN:
        case INT_ENETR:
        case INT_ENETX:
        case INT_PRINTER:
        case INT_SCSI:
        case INT_DISK: interrupt_level = 3;
            break;
        case INT_DSP_L4: interrupt_level = 4;
            break;
        case INT_BUS:
        case INT_REMOTE:
        case INT_SCC: interrupt_level = 5;
            break;
        case INT_R2M_DMA:
        case INT_M2R_DMA:
        case INT_DSP_DMA:
        case INT_SCC_DMA:
        case INT_SND_IN_DMA:
        case INT_SND_OUT_DMA:
        case INT_PRINTER_DMA:
        case INT_DISK_DMA:
        case INT_SCSI_DMA:
        case INT_ENETR_DMA:
        case INT_ENETX_DMA:
        case INT_TIMER: interrupt_level = 6;
            break;
        case INT_PFAIL:
        case INT_NMI: interrupt_level = 7;
            break;            
        default: interrupt_level = 0;
            break;
    }
    
    if(int_set_release == SET_INT) {
        Log_Printf(LOG_WARN,"Interrupt Level: %i", interrupt_level);
        M68000_Exception(((24+interrupt_level)*4), M68000_EXC_SRC_AUTOVEC);
    } else {
//        M68000_Exception(((24+0)*4), M68000_EXC_SRC_AUTOVEC); // release interrupt - does this work???
    }
}

/* Interrupt Mask Register */

void IntRegMaskRead(void) {
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=intMask;
}

void IntRegMaskWrite(void) {
	intMask=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}




