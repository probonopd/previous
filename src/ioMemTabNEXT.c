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

#define IO_SEG_MASK	0x1FFFF
/*
 *
 */
static Uint8 scr2_0=0;
static Uint8 scr2_1=0;
static Uint8 scr2_2=0;
static Uint8 scr2_3=0;

static Uint32 intStat=0x04;
static Uint32 intMask=0x00000000;

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


#define SCR2_RTDATA		0x04
#define SCR2_RTCLK		0x02
#define SCR2_RTCE		0x01

#define SCR2_LED		0x01
#define SCR2_ROM		0x80


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

/*
*
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

void IntRegStatRead(void) {
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=intStat;
}

void IntRegStatWrite(void) {
	intStat=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}

void IntRegMaskRead(void) {
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=intMask;
}

void IntRegMaskWrite(void) {
	intMask=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}


/*-----------------------------------------------------------------------*/
/*
  List of functions to handle read/write hardware interceptions.
*/
const INTERCEPT_ACCESS_FUNC IoMemTable_NEXT[] =
{
	{ 0x02000150, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02004150, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02004154, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02004158, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200415c, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02004188, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02006000, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006001, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006002, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006003, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006004, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006005, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006006, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006008, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006009, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200600a, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200600b, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200600a, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200600b, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200600c, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200600d, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200600e, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x0200600f, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006010, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006011, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006012, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006013, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02006014, SIZE_BYTE, IoMem_ReadWithoutInterception, IoMem_WriteWithoutInterception },
	{ 0x02007000, SIZE_LONG, IntRegStatRead, IntRegStatWrite },
	{ 0x02007800, SIZE_BYTE, IntRegMaskRead, IntRegMaskWrite },
	{ 0x0200c000, SIZE_BYTE, SCR1_Read0, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c001, SIZE_BYTE, SCR1_Read1, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c002, SIZE_BYTE, SCR1_Read2, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c003, SIZE_BYTE, SCR1_Read3, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x0200c800, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
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
	{ 0x02012004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012005, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02012007, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02014003, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02018000, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02018001, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0x02018004, SIZE_BYTE, IoMem_ReadWithoutInterceptionButTrace, IoMem_WriteWithoutInterceptionButTrace },
	{ 0, 0, NULL, NULL }
};
