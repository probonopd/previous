/* NeXT system registers emulation */

#include <stdlib.h>
#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "sysReg.h"
#include "statusbar.h"


#define IO_SEG_MASK	0x1FFFF

static Uint32 scr1=0x00000000;
static Uint32 turboscr1=0x00000000;

static Uint8 scr2_0=0x00;
static Uint8 scr2_1=0x00;
static Uint8 scr2_2=0x80;
static Uint8 scr2_3=0x00;

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
#define POT_ON              0x01
#define EXTENDED_POT        0x02
#define LOOP_POT            0x04
#define VERBOSE_POT         0x08
#define TEST_DRAM_POT       0x10
#define BOOT_POT            0x20
#define TEST_MONITOR_POT    0x40

/* bits in byte 17 */
#define NEW_CLOCK_CHIP      0x80
#define AUTO_POWERON        0x40
#define USE_CONSOLE_SLOT    0x20
#define CONSOLE_SLOT        0x18
#define USE_PARITY_MEM      0x40

/* bits in ni_simm (rtc ram byte 10 and 11) *
 * -------- -----xxx bit 0 - 2: 1st simm: bit 1+2 define size, bit 3 defines page mode
 * -------- --xxx--- bit 3 - 5: 2nd simm: bit 1+2 define size, bit 3 defines page mode
 * -------x xx------ bit 6 - 8: 3rd simm: bit 1+2 define size, bit 3 defines page mode
 * ----xxx- -------- bit 9 -11: 4th simm: bit 1+2 define size, bit 3 defines page mode
 * xxxx---- -------- bit 12-15: defines parity, 1 bit for each simm (ignored on 68030)
 */
/* for 68030 and monochrome non-turbo systems */
#define SIMM_EMPTY          0x0
#define SIMM_16MB           0x1 /* Group of four 4 Mbyte SIMMs */
#define SIMM_4MB            0x2 /* Group of four 1 Mbyte SIMMs */
#define SIMM_1MB            0x3 /* Group of four 256 KByte SIMMs */
#define SIMM_PAGE_MODE      0x4 /* SIMM type is page mode, else nibble mode */
/* for all 68040 systems */
#define SIMM_PARITY         0x8 /* SIMMs support parity */
/* for non-turbo color systems */
#define SIMM_8MB_C          0x1 /* Pair of 4 Mbyte SIMMs */
#define SIMM_2MB_C          0x2 /* Pair of 1 Mbyte SIMMs */
#define SIMM_EMPTY2         0x3 /* reserved */
/* for turbo systems */
#define SIMM_32MB_T         0x1 /* Pair of 16 or 32 MByte SIMMs (front or back) */
#define SIMM_8MB_T          0x2 /* Pair of 4 or 8 MByte SIMMs (front or back) */
#define SIMM_2MB_T          0x3 /* Pair of 1 or 2 MByte SIMMs (front or back) */


/* RTC RAM */
Uint8 rtc_ram[32]={
    0x94,0x0f,0x40,0x00, // byte 0 - 3
    0x00,0x00,0x00,0x00,0x00,0x00, // byte 4 - 9: hardware password, ethernet address (?)
    0x00,0x00, // byte 10, 11: simm type and size (4 simms, 4 bits per simm), see bits in ni_simm above
    0x00,0x00, // byte 12, 13: adobe (?)
    0x4b,0x00,0x00, // byte 14: POT, byte 15: oldest ..., byte 16: most recent selftest error code
    0x00, // byte 17: bit7:clock chip; 6:auto poweron; 5:enable console slot; 3,4:console slot; 2:parity mem
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // byte 18 - 29: boot command
    0x0F,0x13 // byte 30, 31: checksum
};
Uint8 read_rtc_ram(Uint8 position) {
    return rtc_ram[position];
}

Uint8 rtc_ram_default[32]={
    0x94,0x0f,0x40,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x4b,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x13
};

/*
Uint8 rtc_ram[32]={ // values from nextcomputers.org forums
    0x94,0x0f,0x40,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0xfb,0x6d,0x00,0x00,0x4b,0x00,
    0x41,0x00,0x20,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x84,0x7e
};
*/

/*
old value (boot to network diagnostics)
Uint8 rtc_ram[32]={
0x94,0x0f,0x40,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xfb,0x6d,0x00,0x00,0x7B,0x00,
0x00,0x00,0x65,0x6e,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x50,0x13
};
*/

void nvram_init(void) {
    /* Reset RTC RAM to default values */
    memcpy(rtc_ram, rtc_ram_default, sizeof(rtc_ram_default));

    /* Build boot command */
    switch (ConfigureParams.Boot.nBootDevice) {
        case BOOT_ROM:
            rtc_ram[18] = 0x00;
            rtc_ram[19] = 0x00;
            break;
        case BOOT_SCSI:
            rtc_ram[18] = 's';
            rtc_ram[19] = 'd';
            break;
        case BOOT_ETHERNET:
            rtc_ram[18] = 'e';
            rtc_ram[19] = 'n';
            break;
        case BOOT_MO:
            rtc_ram[18] = 'o';
            rtc_ram[19] = 'd';
            break;
        case BOOT_FLOPPY:
            rtc_ram[18] = 'f';
            rtc_ram[19] = 'd';
            break;
            
        default: break;
    }
    
    /* Copy ethernet address from ROM to RTC RAM */
    int i;
    for (i = 0; i<6; i++) {
        rtc_ram[i+4]=NEXTRom[i+8];
    }
    
    /* Build SIMM bytes (for now only valid on monochrome systems) */
    Uint16 SIMMconfig = 0x0000;
    Uint8 simm[4];
    Uint8 parity = 0xF0;
    if (ConfigureParams.System.bTurbo) {
        for (i = 0; i<4; i++) {
            switch (MemBank_Size[i]>>20) {
                case 0: simm[i] = SIMM_EMPTY; parity &= ~(0x10<<i); break;
                case 2: simm[i] = SIMM_2MB_T; break;
                case 8: simm[i] = SIMM_8MB_T; break;
                case 32: simm[i] = SIMM_32MB_T; break;
                default: simm[i] = SIMM_EMPTY; break;
            }
        }

    } else if (ConfigureParams.System.bColor) {
        for (i = 0; i<4; i++) {
            switch (MemBank_Size[i]>>20) {
                case 0: simm[i] = SIMM_EMPTY; parity &= ~(0x10<<i); break;
                case 2: simm[i] = SIMM_2MB_C; break;
                case 8: simm[i] = SIMM_8MB_C; break;
                default: simm[i] = SIMM_EMPTY; break;
            }
        }

    } else {
        for (i = 0; i<4; i++) {
            switch (MemBank_Size[i]>>20) {
                case 0: simm[i] = SIMM_EMPTY; parity &= ~(0x10<<i); break;
                case 1: simm[i] = SIMM_1MB | SIMM_PAGE_MODE; break;
                case 4: simm[i] = SIMM_4MB | SIMM_PAGE_MODE; break;
                case 16: simm[i] = SIMM_16MB | SIMM_PAGE_MODE; break;
                default: simm[i] = SIMM_EMPTY | SIMM_PAGE_MODE; break;
            }
        }
    }
    
    SIMMconfig = ((parity&0xF0)<<8) | (simm[3]<<9) | (simm[2]<<6) | (simm[1]<<3) | simm[0];
    rtc_ram[10] = (SIMMconfig>>8)&0xFF;
    rtc_ram[11] = SIMMconfig&0xFF;
    
    /* Build POT byte[0] */
    rtc_ram[14] = 0x00;
    if (ConfigureParams.Boot.bEnableDRAMTest)
        rtc_ram[14] |= TEST_DRAM_POT;
    if (ConfigureParams.Boot.bEnablePot)
        rtc_ram[14] |= POT_ON;
    if (ConfigureParams.Boot.bEnableSoundTest)
        rtc_ram[14] |= TEST_MONITOR_POT;
    if (ConfigureParams.Boot.bEnableSCSITest)
        rtc_ram[14] |= EXTENDED_POT;
    if (ConfigureParams.Boot.bLoopPot)
        rtc_ram[14] |= LOOP_POT;
    if (ConfigureParams.Boot.bVerbose)
        rtc_ram[14] |= VERBOSE_POT;
    if (ConfigureParams.Boot.bExtendedPot)
        rtc_ram[14] |= BOOT_POT;
    
    /* Set clock chip bit */
    switch (ConfigureParams.System.nRTC) {
        case MCS1850: rtc_ram[17] |= NEW_CLOCK_CHIP; break;
        case MC68HC68T1: rtc_ram[17] &= ~NEW_CLOCK_CHIP; break;
        default: break;
    }
        
    /* Re-calculate checksum */
    rtc_checksum(1);
}

void rtc_checksum(int force) {
	int sum,i;
	sum=0;
	for (i=0;i<30;i+=2) {
		sum+=(rtc_ram[i]<<8)|(rtc_ram[i+1]);
		if (sum>=0x10000) { 
			sum-=0x10000;
			sum+=1;
		}
	}

	sum=0xFFFF-sum; 

	if (force) {
		rtc_ram[30]=(sum&0xFF00)>>8;
		rtc_ram[31]=(sum&0xFF);
		Log_Printf(LOG_WARN,"Forcing RTC checksum to %x %x",rtc_ram[30],rtc_ram[31]);
	} else {
		Log_Printf(LOG_WARN,"Check RTC checksum to %x %x %x %x",
		rtc_ram[30],(sum&0xFF00)>>8,
		rtc_ram[31],(sum&0xFF));
	}
}

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


void SID_Read(void) {
	Log_Printf(LOG_WARN,"SID read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x00; // slot ID 0
}

/* System Control Register 1
 *
 * These values are valid for all non-Turbo systems:
 * -------- -------- -------- ------xx  bits 0:1   --> cpu speed
 * -------- -------- -------- ----xx--  bits 2:3   --> reserved
 * -------- -------- -------- --xx----  bits 4:5   --> main memory speed
 * -------- -------- -------- xx------  bits 6:7   --> video memory speed
 * -------- -------- ----xxxx --------  bits 8:11  --> board revision
 * -------- -------- xxxx---- --------  bits 12:15 --> cpu type
 * -------- xxxxxxxx -------- --------  bits 16:23 --> dma revision
 * ----xxxx -------- -------- --------  bits 24:27 --> reserved
 * xxxx---- -------- -------- --------  bits 28:31 --> slot id
 *
 * cpu speed:       0 = 40MHz, 1 = 20MHz, 2 = 25MHz, 3 = 33MHz
 * main mem speed:  0 = 120ns, 1 = 100ns, 2 = 80ns, 3 = 60ns
 * video mem speed: 0 = 120ns, 1 = 100ns, 2 = 80ns, 3 = 60ns
 * board revision:  for 030 Cube:
 *                  0 = DCD input inverted
 *                  1 = DCD polarity fixed
 *                  2 = must disable DSP mem before reset
 * cpu type:        0 = NeXT Computer (68030)
 *                  1 = NeXTstation monochrome
 *                  2 = NeXTcube
 *                  3 = NeXTstation color
 *                  4 = all Turbo systems
 * dma revision:    1 on all systems ?
 * slot id:         f on Turbo systems (cube too?), 0 on other systems
 *
 *
 * These bits are always 0 on all Turbo systems:
 * ----xxxx xxxxxxxx ----xxxx xxxxxxxx
 */

/* for Slab 040:
 * 0000 0000 0000 0001 0001 0000 0101 0010
 * 00 01 10 52
 *
 * for Cube 030:
 * 0000 0000 0000 0001 0000 0001 0101 0010
 * 00 01 01 52
 */
#define SCR1_NEXT_COMPUTER  0x00010152
#define SCR1_SLAB_MONO      0x00011052
#define SCR1_SLAB_COLOR     0x00013052
#define SCR1_CUBE           0x00012052
#define SCR1_TURBO          0xF0004000

#define SCR1_CONST_MASK     0xFFFFFF00

void SCR_Reset(void) {
    scr2_0=0x00;
    scr2_1=0x00;
    scr2_2=0x80;
    scr2_3=0x00;
    
    intStat=0x00000000;
    intMask=0x00000000;

    if (ConfigureParams.System.bTurbo) {
        scr1 = SCR1_TURBO;
        TurboSCR1_Reset();
        return;
    } else {
        switch (ConfigureParams.System.nMachineType) {
            case NEXT_CUBE030:
                scr1 = SCR1_NEXT_COMPUTER & SCR1_CONST_MASK;
                break;
            case NEXT_CUBE040:
                scr1 = SCR1_CUBE & SCR1_CONST_MASK;
                break;
            case NEXT_STATION:
                if (ConfigureParams.System.bColor)
                    scr1 = SCR1_SLAB_COLOR & SCR1_CONST_MASK;
                else
                    scr1 = SCR1_SLAB_MONO & SCR1_CONST_MASK;
                break;
            default:
                break;
        }
    }
    
    Uint8 cpu_speed = ((ConfigureParams.System.nCpuFreq/8)-1)%4;
    Uint8 memory_speed;
    switch (ConfigureParams.Memory.nMemorySpeed) {
        case MEMORY_120NS: memory_speed = 0x00; break;
        case MEMORY_100NS: memory_speed = 0x50; break;
        case MEMORY_80NS: memory_speed = 0xA0; break;
        case MEMORY_60NS: memory_speed = 0xF0; break;
        default: Log_Printf(LOG_WARN, "SCR1 error: unknown memory speed\n"); break;
    }
    scr1 |= ((memory_speed&0xF0)|(cpu_speed&0x03));
}

void SCR1_Read0(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = (scr1&0xFF000000)>>24;
}
void SCR1_Read1(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = (scr1&0x00FF0000)>>16;
}
void SCR1_Read2(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = (scr1&0x0000FF00)>>8;
}
void SCR1_Read3(void)
{
	Log_Printf(LOG_WARN,"SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = scr1&0x000000FF;
}


/* Additional System Control Register for Turbo systems:
 * -------- -------- -------- -----?xx  bits 0:2   --> cpu speed
 * -------- -------- -------- --xx----  bits 4:5   --> main memory speed
 * -------- -------- -------- xx------  bits 6:7   --> video memory speed
 * -------- -------- xxxx---- --------  bits 8:11  --> cpu type
 * xxxxxxxx xxxxxxxx ----xxxx ----?---  all other bits: 1
 *
 * cpu speed:       7 = 33MHz?
 * main mem speed:  0 = 120ns?, 1 = 100ns?, 2 = 70ns, 3 = 60ns
 * video mem speed: 3 on all Turbo systems (60ns)?
 * cpu type:        4 = NeXTstation turbo monochrome
 *                  5 = NeXTstation turbo color
 */

#define TURBOSCR_FMASK   0xFFFF0F08

void TurboSCR1_Reset(void) {
    Uint8 memory_speed;
    Uint8 cpu_speed = 0x07; // 33 MHz
    switch (ConfigureParams.Memory.nMemorySpeed) {
        case MEMORY_120NS: memory_speed = 0xC0; break;
        case MEMORY_100NS: memory_speed = 0xD0; break;
        case MEMORY_80NS: memory_speed = 0xE0; break;
        case MEMORY_60NS: memory_speed = 0xF0; break;
        default: Log_Printf(LOG_WARN, "Turbo SCR1 error: unknown memory speed\n"); break;
    }
    turboscr1 = ((memory_speed&0xF0)|(cpu_speed&0x07));
    if (ConfigureParams.System.bColor) {
        turboscr1 |= 0x5000;
    } else {
        turboscr1 |= 0x4000;
    }
    turboscr1 |= TURBOSCR_FMASK;
}

void TurboSCR1_Read0(void) {
    Log_Printf(LOG_WARN,"Turbo SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem_WriteWord(IoAccessCurrentAddress&0x1FFFF, (turboscr1&0xFFFF0000)>>16);
}
void TurboSCR1_Read2(void) {
    Log_Printf(LOG_WARN,"Turbo SCR1 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    IoMem_WriteWord(IoAccessCurrentAddress&0x1FFFF, turboscr1&0x0000FFFF);
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

#define SCR2_SOFTINT2		0x02
#define SCR2_SOFTINT1		0x01

#define SCR2_TIMERIPL7		0x80
#define SCR2_RTDATA		0x04
#define SCR2_RTCLK		0x02
#define SCR2_RTCE		0x01

#define SCR2_LED		0x01
#define SCR2_ROM		0x80


void SCR2_Write0(void)
{	
	Uint8 old_scr2_0=scr2_0;
    //	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
	scr2_0=IoMem[IoAccessCurrentAddress & 0x1FFFF];

	if ((old_scr2_0&SCR2_SOFTINT1)!=(scr2_0&SCR2_SOFTINT1)) {
		Log_Printf(LOG_WARN,"SCR2 SCR2_SOFTINT1 change at $%08x val=%x PC=$%08x\n",
                           IoAccessCurrentAddress,scr2_0&SCR2_SOFTINT1,m68k_getpc());
		if (scr2_0&SCR2_SOFTINT1) 
			set_interrupt(INT_SOFT1,SET_INT);
		else
			set_interrupt(INT_SOFT1,RELEASE_INT);
	}

	if ((old_scr2_0&SCR2_SOFTINT2)!=(scr2_0&SCR2_SOFTINT2)) {
		Log_Printf(LOG_WARN,"SCR2 SCR2_SOFTINT2 change at $%08x val=%x PC=$%08x\n",
                           IoAccessCurrentAddress,scr2_0&SCR2_SOFTINT2,m68k_getpc());
		if (scr2_0&SCR2_SOFTINT2) 
			set_interrupt(INT_SOFT2,SET_INT);
		else
			set_interrupt(INT_SOFT2,RELEASE_INT);
	}
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
	static bool burst=false;
	static Uint8 rtc_command=0;
	static Uint8 rtc_value=0;
	static Uint8 rtc_return=0;
	static Uint8 rtc_status=0x00;
	static Uint8 rtc_ccr=0x00;	
	static Uint8 rtc_icr=0x00;	

    	static int day_in_week=0x02;
	static int day=0x31;
	static int month=0x12;
	static int year=0x98;
	static int century=0x19;

    
	Uint8 old_scr2_2=scr2_2;

	scr2_2=IoMem[IoAccessCurrentAddress & 0x1FFFF];

	if ((old_scr2_2&SCR2_TIMERIPL7)!=(scr2_2&SCR2_TIMERIPL7)) {
		Log_Printf(LOG_WARN,"SCR2 TIMER IPL7 change at $%08x val=%x PC=$%08x\n",
                           IoAccessCurrentAddress,scr2_2&SCR2_TIMERIPL7,m68k_getpc());
	}

    // treat only if CE is set to 1
	if (scr2_2&SCR2_RTCE) {
        if (phase==-1) {phase=0;}

        // if we are in going down clock... do something
        if (((old_scr2_2&SCR2_RTCLK)!=(scr2_2&SCR2_RTCLK)) && ((scr2_2&SCR2_RTCLK)==0) ) {
            if (phase<8)
                rtc_command=(rtc_command<<1)|((scr2_2&SCR2_RTDATA)?1:0);

	    // now reading or writing register (one bit at a time)
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
                    if (rtc_ccr&(0x80>>(phase-8)))
                        scr2_2|=SCR2_RTDATA;
                    rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
                }
                // read the status 0x32
                if (rtc_command==0x32) {
                    scr2_2=scr2_2&(~SCR2_RTDATA);
                    if (rtc_icr&(0x80>>(phase-8)))
                        scr2_2|=SCR2_RTDATA;
                    rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
                }
                if ((rtc_command>=0x20) && (rtc_command<=0x2F)) {
		    unsigned char v;
		    static time_t t;
                    static struct tm * ts;

                    scr2_2=scr2_2&(~SCR2_RTDATA);
                    // get time at once to avoid problems (in burst mode only).
	            if (!burst) {
			    t=time(NULL);
			    ts=localtime(&t);
			burst=true;
		    }
		    v=0;
		    switch (rtc_command) {
			case 0x20 : // seconds
				v=(((ts->tm_sec)/10)<<4)|(ts->tm_sec%10);
				break;
			case 0x21 : //min
				v=(((ts->tm_min)/10)<<4)|(ts->tm_min%10);
				break;
			case 0x22 : // hour
				v=(((ts->tm_hour)/10)<<4)|(ts->tm_hour%10);
				break;
			case 0x23 : 
				v=day_in_week;
				break;
			case 0x24 : 
				v=day;
				break;
			case 0x25 : 
				v=month;
				break;
			case 0x26 : 
				v=year;
				break;
			case 0x27 : 
				v=century;
				break;
		    }
                    if (v&(0x80>>(phase-8)))
                        scr2_2|=SCR2_RTDATA;
                    rtc_return=(rtc_return<<1)|((scr2_2&SCR2_RTDATA)?1:0);
                }
                
            } // of read/write phase
            
            phase++;
            if (phase==16) {
                Log_Printf(LOG_WARN,"SCR2 RTC command complete %x %x %x at PC=$%08x\n",
                           rtc_command,rtc_value,rtc_return,m68k_getpc());
                if ((rtc_command>=0x80) && (rtc_command<=0x9F))
                    rtc_ram[rtc_command-0x80]=rtc_value;
                
                // write to x31 register
                if (rtc_command==0xB1) {
		    rtc_ccr=rtc_value;


                    // clear FTU
                    if (rtc_value & 0x04) {
                        rtc_status=rtc_status&(~0x18);
                        intStat=intStat&(~0x04);
                    }


                }
                // write to x32 register
                if (rtc_command==0xB2) {
		    rtc_icr=rtc_value;
		}

                if (rtc_command==0xA3) {
		    day_in_week=rtc_value;
		}
                if (rtc_command==0xA4) {
		    day=rtc_value;
		}                
		if (rtc_command==0xA5) {
		    month=rtc_value;
		}                
		if (rtc_command==0xA6) {
		    year=rtc_value;
		}
		if (rtc_command==0xA7) {
		    century=rtc_value;
		}

		// burst mode
		phase=8;
		rtc_command++;
            } // of phase==16
        } // of going down clock
	} // of scr2_2&SCR2_RTCE
		else {
        // else end or abort
		phase=-1;
		rtc_command=0;
		rtc_value=0;
		burst=false;
	}	
    
}

void SCR2_Read2(void)
{
    //	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
    //	IoMem[IoAccessCurrentAddress & 0x1FFFF]=scr2_2 & (SCR2_RTDATA|SCR2_RTCLK|SCR2_RTCE)); // + data
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=scr2_2;
}

int SCR_ROM_overlay=0;

void SCR2_Write3(void)
{	
	Uint8 old_scr2_3=scr2_3;
    //	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
	scr2_3=IoMem[IoAccessCurrentAddress & 0x1FFFF];
	if ((old_scr2_3&SCR2_ROM)!=(scr2_3&SCR2_ROM)) {
		Log_Printf(LOG_WARN,"SCR2 ROM change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_3&SCR2_ROM,m68k_getpc());
		   SCR_ROM_overlay=scr2_3&SCR2_ROM;
		}
	if ((old_scr2_3&SCR2_LED)!=(scr2_3&SCR2_LED)) {
		Log_Printf(LOG_WARN,"SCR2 LED change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_3&SCR2_LED,m68k_getpc());
        Statusbar_SetSCR2Led(scr2_3&SCR2_LED);
    }
}


void SCR2_Read3(void)
{
    //	Log_Printf(LOG_WARN,"SCR2 read at $%08x PC=$%08x\n", IoAccessCurrentAddress,m68k_getpc());
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=scr2_3;
}



/* Interrupt Status Register */

void IntRegStatRead(void) {
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, intStat);
}

void IntRegStatWrite(void) {
    intStat = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
}

void set_interrupt(Uint32 interrupt_val, Uint8 int_set_release) {
    
    Uint8 interrupt_level;
    
    if(int_set_release == SET_INT) {
        intStat = intStat | interrupt_val;
    } else {
        intStat = intStat & ~interrupt_val; 
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
        case INT_EN_RX:
        case INT_EN_TX:
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
        case INT_EN_RX_DMA:
        case INT_EN_TX_DMA:
			interrupt_level = 6;
		break;
        case INT_TIMER:
		if (scr2_2&SCR2_TIMERIPL7)
		 interrupt_level = 7;
		else
		 interrupt_level = 6;
            break;
        case INT_PFAIL:
        case INT_NMI: interrupt_level = 7;
            break;            
        default: interrupt_level = 0;
            break;
    }
 
    if ((interrupt_val & intMask)==0) {
	Log_Printf(LOG_WARN,"[INT] interrupt is masked %04x mask %04x %s at %d",interrupt_val,intMask,__FILE__,__LINE__);
	return;
    }
   
    if(int_set_release == SET_INT) {
        Log_Printf(LOG_DEBUG,"Interrupt Level: %i", interrupt_level);
        M68000_Exception(((24+interrupt_level)*4), M68000_EXC_SRC_AUTOVEC);
    } else {
//        M68000_Exception(((24+0)*4), M68000_EXC_SRC_AUTOVEC); // release interrupt - does this work???
    }
}

/* Interrupt Mask Register */

void IntRegMaskRead(void) {
	IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK,intMask);
}

void IntRegMaskWrite(void) {
	intMask = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
        Log_Printf(LOG_WARN,"Interrupt mask: %08x", intMask);
}

/*
 * Hardclock internal interrupt
 *
 */

#define HARDCLOCK_ENABLE 0x80
#define HARDCLOCK_LATCH  0x40

Uint8 hardclock_csr=0;
Uint8 hardclock1=0;
Uint8 hardclock0=0;
int pseudo_counter=0;
int latch_hardclock=0;

void Hardclock_InterruptHandler ( void )
{
	CycInt_AcknowledgeInterrupt();
	if ((hardclock_csr&HARDCLOCK_ENABLE) && (latch_hardclock>0)) {
		// Log_Printf(LOG_WARN,"[INT] throwing hardclock");
		set_interrupt(INT_TIMER,SET_INT);
	        CycInt_AddRelativeInterrupt(latch_hardclock*33, INT_CPU_CYCLE, INTERRUPT_HARDCLOCK);
		pseudo_counter=latch_hardclock;
	}
}


void HardclockRead0(void){
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=(pseudo_counter>>8);
;
	if (pseudo_counter>0) pseudo_counter--;

	Log_Printf(LOG_WARN,"[hardclock] read at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
}
void HardclockRead1(void){
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=pseudo_counter&0xff;
	if (pseudo_counter>0) pseudo_counter--;
	Log_Printf(LOG_WARN,"[hardclock] read at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
}

void HardclockWrite0(void){
	Log_Printf(LOG_WARN,"[hardclock] write at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
	hardclock0=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}
void HardclockWrite1(void){
	Log_Printf(LOG_WARN,"[hardclock] write at $%08x val=%02x PC=$%08x",IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
	hardclock1=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}

void HardclockWriteCSR(void) {
	Log_Printf(LOG_WARN,"[hardclock] write at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
	hardclock_csr=IoMem[IoAccessCurrentAddress & 0x1FFFF];
	if ((hardclock_csr&HARDCLOCK_LATCH)) {
		latch_hardclock=(hardclock0<<8)|hardclock1;
		pseudo_counter=latch_hardclock;
	}
	if ((hardclock_csr&HARDCLOCK_ENABLE) && (latch_hardclock>0)) {
	        CycInt_AddRelativeInterrupt(latch_hardclock*33, INT_CPU_CYCLE, INTERRUPT_HARDCLOCK);
//        set_interrupt(INT_TIMER, SET_INT);
	}
}
void HardclockReadCSR(void) {
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=hardclock_csr;
	// Log_Printf(LOG_WARN,"[hardclock] read at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
	set_interrupt(INT_TIMER,RELEASE_INT);
}


