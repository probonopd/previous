/* NeXT system registers emulation */

#include <stdlib.h>
#include "main.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "video.h"
#include "configuration.h"
#include "sysdeps.h"
#include "m68000.h"
#include "dsp.h"
#include "sysReg.h"
#include "rtcnvram.h"
#include "statusbar.h"


#define LOG_HARDCLOCK_LEVEL LOG_DEBUG
#define LOG_SOFTINT_LEVEL   LOG_DEBUG

#define IO_SEG_MASK	0x1FFFF

int SCR_ROM_overlay=0;

static Uint32 scr1=0x00000000;

static Uint8 scr2_0=0x00;
static Uint8 scr2_1=0x00;
static Uint8 scr2_2=0x00;
static Uint8 scr2_3=0x00;

static Uint32 intStat=0x00000000;
static Uint32 intMask=0x00000000;



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
#define SCR1_TURBO          0x00004000

#define SCR1_CONST_MASK     0xFFFFFF00

void SCR_Reset(void) {
    SCR_ROM_overlay = 0;
    
    scr2_0=0x00;
    scr2_1=0x00;
	if (ConfigureParams.System.bTurbo) {
		scr2_2=0x10;
		scr2_3=0x80;
	} else {
		scr2_2=0x00;
		scr2_3=0x00;
	}
	
    intStat=0x00000000;
    intMask=0x00000000;

    if (ConfigureParams.System.bTurbo) {
        scr1 = SCR1_TURBO;
        scr1 |= (ConfigureParams.System.nMachineType==NEXT_CUBE040)?0:0xF0000000;
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
    
    Uint8 cpu_speed;
    Uint8 memory_speed;
    
    if (ConfigureParams.System.nCpuFreq<20) {
        cpu_speed = 0;
    } else if (ConfigureParams.System.nCpuFreq<25) {
        cpu_speed = 1;
    } else if (ConfigureParams.System.nCpuFreq<33) {
        cpu_speed = 2;
    } else {
        cpu_speed = 3;
    }
    
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

/* byte 0 */
#define SCR2_DSP_RESET      0x80
#define SCR2_DSP_BLK_END    0x40
#define SCR2_DSP_UNPKD      0x20
#define SCR2_DSP_MODE_B     0x10
#define SCR2_DSP_MODE_A     0x08
#define SCR2_SOFTINT2		0x02
#define SCR2_SOFTINT1		0x01

/* byte 2 */
#define SCR2_TIMERIPL7		0x80
#define SCR2_RTDATA		0x04
#define SCR2_RTCLK		0x02
#define SCR2_RTCE		0x01

/* byte 3 */
#define SCR2_ROM		0x80
#define SCR2_DSP_INT_EN 0x40
#define SCR2_DSP_MEM_EN 0x20
#define SCR2_LED		0x01


void SCR2_Write0(void)
{	
	Uint8 old_scr2_0=scr2_0;
    //	Log_Printf(LOG_WARN,"SCR2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],m68k_getpc());
	scr2_0=IoMem[IoAccessCurrentAddress & 0x1FFFF];

	if ((old_scr2_0&SCR2_SOFTINT1)!=(scr2_0&SCR2_SOFTINT1)) {
		Log_Printf(LOG_SOFTINT_LEVEL,"SCR2 SCR2_SOFTINT1 change at $%08x val=%x PC=$%08x\n",
                           IoAccessCurrentAddress,scr2_0&SCR2_SOFTINT1,m68k_getpc());
		if (scr2_0&SCR2_SOFTINT1) 
			set_interrupt(INT_SOFT1,SET_INT);
		else
			set_interrupt(INT_SOFT1,RELEASE_INT);
	}

	if ((old_scr2_0&SCR2_SOFTINT2)!=(scr2_0&SCR2_SOFTINT2)) {
		Log_Printf(LOG_SOFTINT_LEVEL,"SCR2 SCR2_SOFTINT2 change at $%08x val=%x PC=$%08x\n",
                           IoAccessCurrentAddress,scr2_0&SCR2_SOFTINT2,m68k_getpc());
		if (scr2_0&SCR2_SOFTINT2) 
			set_interrupt(INT_SOFT2,SET_INT);
		else
			set_interrupt(INT_SOFT2,RELEASE_INT);
	}
    
    /* DSP bits */
    if (scr2_0&SCR2_DSP_MODE_A) {
        Log_Printf(LOG_WARN,"[SCR2] DSP Mode A");
    }
    if (scr2_0&SCR2_DSP_MODE_B) {
        Log_Printf(LOG_WARN,"[SCR2] DSP Mode B");
    }
    if (!(scr2_0&SCR2_DSP_RESET) && (old_scr2_0&SCR2_DSP_RESET)) {
        Log_Printf(LOG_WARN,"[SCR2] DSP Reset");
        DSP_Reset();
    } else if ((scr2_0&SCR2_DSP_RESET) && !(old_scr2_0&SCR2_DSP_RESET)) {
        Log_Printf(LOG_WARN,"[SCR2] DSP Start (mode %i)",(~(scr2_0>>3))&3);
        DSP_Start((~(scr2_0>>3))&3);
    }
	dsp_intr_at_block_end = scr2_0&SCR2_DSP_BLK_END;
    if ((old_scr2_0&SCR2_DSP_BLK_END) != (scr2_0&SCR2_DSP_BLK_END)) {
        Log_Printf(LOG_WARN,"[SCR2] %s DSP interrupt from DMA at block end",dsp_intr_at_block_end?"Enable":"Disable");
    }
	dsp_dma_unpacked = scr2_0&SCR2_DSP_UNPKD;
    if ((old_scr2_0&SCR2_DSP_UNPKD) != (scr2_0&SCR2_DSP_UNPKD)) {
        Log_Printf(LOG_WARN,"[SCR2] %s DSP DMA unpacked mode",dsp_dma_unpacked?"Enable":"Disable");
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
	Uint8 old_scr2_2=scr2_2;

	scr2_2=IoMem[IoAccessCurrentAddress & 0x1FFFF];

	if ((old_scr2_2&SCR2_TIMERIPL7)!=(scr2_2&SCR2_TIMERIPL7)) {
		Log_Printf(LOG_WARN,"SCR2 TIMER IPL7 change at $%08x val=%x PC=$%08x\n",
                           IoAccessCurrentAddress,scr2_2&SCR2_TIMERIPL7,m68k_getpc());
	}

    /* RTC enabled */
	if (scr2_2&SCR2_RTCE) {
        if (((old_scr2_2&SCR2_RTCLK)!=(scr2_2&SCR2_RTCLK)) && ((scr2_2&SCR2_RTCLK)==0) ) {
            Uint8 rtdata = scr2_2&SCR2_RTDATA;
            scr2_2 &= ~SCR2_RTDATA;
            scr2_2 |= rtc_interface_io(rtdata)?SCR2_RTDATA:0;
        }
	} else {
        rtc_interface_reset();
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
	if ((old_scr2_3&SCR2_ROM)!=(scr2_3&SCR2_ROM)) {
		Log_Printf(LOG_WARN,"SCR2 ROM change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_3&SCR2_ROM,m68k_getpc());
		   SCR_ROM_overlay=scr2_3&SCR2_ROM;
		}
	if ((old_scr2_3&SCR2_LED)!=(scr2_3&SCR2_LED)) {
		Log_Printf(LOG_DEBUG,"SCR2 LED change at $%08x val=%x PC=$%08x\n",
                   IoAccessCurrentAddress,scr2_3&SCR2_LED,m68k_getpc());
        Statusbar_SetSystemLed(scr2_3&SCR2_LED);
    }
    
    if ((old_scr2_3&SCR2_DSP_INT_EN) != (scr2_3&SCR2_DSP_INT_EN)) {
        Log_Printf(LOG_WARN,"[SCR2] DSP interrupt at level %i",(scr2_3&SCR2_DSP_INT_EN)?4:3);
		if (intStat&(INT_DSP_L3|INT_DSP_L4)) {
			Log_Printf(LOG_WARN,"[SCR2] Switching DSP interrupt to level %i",(scr2_3&SCR2_DSP_INT_EN)?4:3);
			set_interrupt(INT_DSP_L3|INT_DSP_L4, RELEASE_INT);
			set_dsp_interrupt(SET_INT);
		}
    }
    if ((old_scr2_3&SCR2_DSP_MEM_EN) != (scr2_3&SCR2_DSP_MEM_EN)) {
        Log_Printf(LOG_WARN,"[SCR2] %s DSP memory",(scr2_3&SCR2_DSP_MEM_EN)?"Enable":"Disable");
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

void set_dsp_interrupt(Uint8 state) {
    if (scr2_3&SCR2_DSP_INT_EN || ConfigureParams.System.bTurbo) {
		set_interrupt(INT_DSP_L4, state);
    } else {
		set_interrupt(INT_DSP_L3, state);
    }
}

void set_interrupt(Uint32 intr, Uint8 state) {
    /* The interrupt gets polled by the cpu via intlev()
     * --> see previous-glue.c
     */
    if (state==SET_INT) {
        intStat |= intr;
    } else {
        intStat &= ~intr;
    }
}

int get_interrupt_level(void) {
    Uint32 interrupt = intStat&intMask;
    
    if (!interrupt) {
        return 0;
    } else if (interrupt&INT_L7_MASK) {
        return 7;
    } else if ((interrupt&INT_TIMER) && (scr2_2&SCR2_TIMERIPL7)) {
        return 7;
    } else if (interrupt&INT_L6_MASK) {
        return 6;
    } else if (interrupt&INT_L5_MASK) {
        return 5;
    } else if (interrupt&INT_L4_MASK) {
        return 4;
    } else if (interrupt&INT_L3_MASK) {
        return 3;
    } else if (interrupt&INT_L2_MASK) {
        return 2;
    } else if (interrupt&INT_L1_MASK) {
        return 1;
    } else {
        abort();
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


/* Hardclock internal interrupt */

#define HARDCLOCK_ENABLE 0x80
#define HARDCLOCK_LATCH  0x40
#define HARDCLOCK_ZERO   0x3F

Uint8 hardclock_csr=0;
Uint8 hardclock1=0;
Uint8 hardclock0=0;
int hardclock_delay=0;
int latch_hardclock=0;

void Hardclock_InterruptHandler ( void )
{
	CycInt_AcknowledgeInterrupt();
	if ((hardclock_csr&HARDCLOCK_ENABLE) && (latch_hardclock>0)) {
		// Log_Printf(LOG_WARN,"[INT] throwing hardclock");
        set_interrupt(INT_TIMER,SET_INT);
        CycInt_AddRelativeInterrupt(hardclock_delay, INT_CPU_CYCLE, INTERRUPT_HARDCLOCK);
	}
}


void HardclockRead0(void){
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=(latch_hardclock>>8);
	Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] read at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
}
void HardclockRead1(void){
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=latch_hardclock&0xff;
	Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] read at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
}

void HardclockWrite0(void){
	Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] write at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
	hardclock0=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}
void HardclockWrite1(void){
	Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] write at $%08x val=%02x PC=$%08x",IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
	hardclock1=IoMem[IoAccessCurrentAddress & 0x1FFFF];
}

void HardclockWriteCSR(void) {
	Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] write at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
	hardclock_csr=IoMem[IoAccessCurrentAddress & 0x1FFFF];
	if (hardclock_csr&HARDCLOCK_LATCH) {
        hardclock_csr&= ~HARDCLOCK_LATCH;
		latch_hardclock=(hardclock0<<8)|hardclock1;
		hardclock_delay=latch_hardclock*11;
	}
	if ((hardclock_csr&HARDCLOCK_ENABLE) && (latch_hardclock>0)) {
        Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] enable periodic interrupt (%i microseconds).", latch_hardclock);
        CycInt_AddRelativeInterrupt(hardclock_delay, INT_CPU_CYCLE, INTERRUPT_HARDCLOCK);
	} else {
        Log_Printf(LOG_HARDCLOCK_LEVEL,"[hardclock] disable periodic interrupt.");
    }
    set_interrupt(INT_TIMER,RELEASE_INT);
}
void HardclockReadCSR(void) {
	IoMem[IoAccessCurrentAddress & 0x1FFFF]=hardclock_csr;
	// Log_Printf(LOG_WARN,"[hardclock] read at $%08x val=%02x PC=$%08x", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & 0x1FFFF],m68k_getpc());
	set_interrupt(INT_TIMER,RELEASE_INT);
}


/* Event counter register */
#define EVENTC_DEBUG 0
Uint32 lasteventc; /* debugging code */

Uint32 eventcounter;

#if USE_FREQ_DIVIDER
void System_Timer_Read(void) { /* tuned for power-on test */
#if EVENTC_DEBUG
    lasteventc = eventcounter; /* for debugging */
#endif
    if (ConfigureParams.System.nCpuLevel == 3) {
        if (NEXTRom[0xFFAB]==0x04) { // HACK for ROM version 0.8.31 power-on test, WARNING: this causes slowdown of emulation
            eventcounter = (nCyclesMainCounter/(240/nCpuFreqDivider))&0xFFFFF;
        } else {
            eventcounter = (nCyclesMainCounter/(48/nCpuFreqDivider))&0xFFFFF;
        }
    } else { // System has 68040 CPU
        eventcounter = (nCyclesMainCounter/(72/nCpuFreqDivider))&0xFFFFF;
    }
    IoMem_WriteLong(IoAccessCurrentAddress&IO_SEG_MASK, eventcounter);
    
#if EVENTC_DEBUG
    Log_Printf(LOG_WARN, "[Eventcounter] Difference = %i (Frequency = %i, Divider = %i)",
               eventcounter-lasteventc,ConfigureParams.System.nCpuFreq,nCpuFreqDivider);
#endif
}
#else
void System_Timer_Read(void) { // tuned for power-on test
//  lasteventc = eventcounter; // debugging code
    if (ConfigureParams.System.nCpuLevel == 3) {
        if (NEXTRom[0xFFAB]==0x04) { // HACK for ROM version 0.8.31 power-on test, WARNING: this causes slowdown of emulation
//          eventcounter = (nCyclesMainCounter/((1280/ConfigureParams.System.nCpuFreq)*3))&0xFFFFF; // debugging code
            IoMem_WriteLong(IoAccessCurrentAddress&0x1FFFF, (nCyclesMainCounter/((1280/ConfigureParams.System.nCpuFreq)*3))&0xFFFFF);
        } else {
//          eventcounter = (nCyclesMainCounter/((128/ConfigureParams.System.nCpuFreq)*3))&0xFFFFF; // debugging code
            IoMem_WriteLong(IoAccessCurrentAddress&0x1FFFF, (nCyclesMainCounter/((128/ConfigureParams.System.nCpuFreq)*3))&0xFFFFF);
        }
    } else { // System has 68040 CPU
//      eventcounter = (nCyclesMainCounter/((64/ConfigureParams.System.nCpuFreq)*9))&0xFFFFF; // debugging code
        IoMem_WriteLong(IoAccessCurrentAddress&0x1FFFF, (nCyclesMainCounter/((64/ConfigureParams.System.nCpuFreq)*9))&0xFFFFF);
    }
//  printf("DIFFERENCE = %i PC = %08X\n",eventcounter-lasteventc,m68k_getpc());
}
#endif


/* Color Video Interrupt Register */

#define VID_CMD_CLEAR_INT    0x01
#define VID_CMD_ENABLE_INT   0x02
#define VID_CMD_UNBLANK      0x04

Uint8 col_vid_intr = 0;

void ColorVideo_CMD_Write(void) {
	col_vid_intr=IoMem[IoAccessCurrentAddress & 0x1FFFF];
	Log_Printf(LOG_DEBUG,"[Color Video] Command write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
	
	if (col_vid_intr&VID_CMD_CLEAR_INT) {
		set_interrupt(INT_DISK, RELEASE_INT);
	}
}

void color_video_interrupt(void) {
	if (col_vid_intr&VID_CMD_ENABLE_INT) {
		set_interrupt(INT_DISK, SET_INT);
		col_vid_intr &= ~VID_CMD_ENABLE_INT;
	}
}
