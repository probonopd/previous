#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "sysReg.h"
#include "adb.h"
#include "tmc.h"

#define LOG_TMC_LEVEL LOG_DEBUG

/* NeXT TMC emulation */

#define TMC_ADB_ADDR_MASK	0x02208000

#define TMC_REGS_MASK		0x0000FFFF

/* TMC registers */

struct {
	Uint32 scr1;
	Uint32 control;
	Uint32 horizontal;
	Uint32 vertical;
	Uint8 video_intr;
	Uint32 nitro;
} tmc;

/* Additional System Control Register for Turbo systems:
 * -------- -------- -------- -----xxx  bits 0:2   --> cpu speed
 * -------- -------- -------- --xx----  bits 4:5   --> main memory speed
 * -------- -------- -------- xx------  bits 6:7   --> video memory speed
 * -------- -------- ----xxxx --------  bits 8:11  --> cpu revision
 * -------- -------- xxxx---- --------  bits 12:15 --> cpu type
 * xxxx---- -------- -------- --------  bits 28:31 --> slot id
 * ----xxxx xxxxxxxx -------- ----x---  all other bits: 1
 *
 * cpu speed:       7 = 33MHz?
 * main mem speed:  0 = 60ns, 1 = 70ns, 2 = 80ns, 3 = 100ns
 * video mem speed: 3 on all Turbo systems (100ns)?
 * cpu revision:    0xF = rev 0
 *                  0xE = rev 1
 *                  0xD = rev 2
 *                  0xC - 0x0: rev 3 - 15
 * cpu type:        4 = NeXTstation turbo monochrome
 *                  5 = NeXTstation turbo color
 *                  8 = NeXTcube turbo
 */

#define TURBOSCR_FMASK   0x0FFF0F08

static void TurboSCR1_Reset(void) {
	Uint8 memory_speed = 0;
	Uint8 cpu_speed = 0x07; // 33 MHz
	
	if (ConfigureParams.System.nCpuFreq<20) {
		cpu_speed = 4;
	} else if (ConfigureParams.System.nCpuFreq<25) {
		cpu_speed = 5;
	} else if (ConfigureParams.System.nCpuFreq<33) {
		cpu_speed = 6;
	} else {
		cpu_speed = 7;
	}
	
	switch (ConfigureParams.Memory.nMemorySpeed) {
		case MEMORY_120NS: memory_speed = 0x00; break;
		case MEMORY_100NS: memory_speed = 0x50; break;
		case MEMORY_80NS: memory_speed = 0xA0; break;
		case MEMORY_60NS: memory_speed = 0xF0; break;
		default: Log_Printf(LOG_WARN, "Turbo SCR1 error: unknown memory speed\n"); break;
	}
	tmc.scr1 = ((memory_speed&0xF0)|(cpu_speed&0x07));
	if (ConfigureParams.System.nMachineType == NEXT_CUBE040)
		tmc.scr1 |= 0x8000;
	else if (ConfigureParams.System.bColor) {
		tmc.scr1 |= 0x5000;
	} else {
		tmc.scr1 |= 0x4000;
	}
	tmc.scr1 |= TURBOSCR_FMASK;
}



/* Register read/write functions */

static void tmc_ill_write(Uint8 val) {
	Log_Printf(LOG_WARN, "[TMC] Illegal write!\n");
}

static Uint8 tmc_unimpl_read(void) {
	Log_Printf(LOG_WARN, "[TMC] Unimplemented read!\n");
	return 0;
}

static void tmc_unimpl_write(Uint8 val) {
	Log_Printf(LOG_WARN, "[TMC] Unimplemented write!\n");
}

static Uint8 tmc_void_read(void) {
	return 0;
}

static void tmc_void_write(Uint8 val) {
}

/* SCR1 */
static Uint8 tmc_scr1_read0(void) {
	Log_Printf(LOG_WARN,"[TMC] SCR1 read at $0x2200000 PC=$%08x\n",m68k_getpc());
	return (tmc.scr1>>24);
}
static Uint8 tmc_scr1_read1(void) {
	Log_Printf(LOG_WARN,"[TMC] SCR1 read at $0x2200001 PC=$%08x\n",m68k_getpc());
	return (tmc.scr1>>16);
}
static Uint8 tmc_scr1_read2(void) {
	Log_Printf(LOG_WARN,"[TMC] SCR1 read at $0x2200002 PC=$%08x\n",m68k_getpc());
	return (tmc.scr1>>8);
}
static Uint8 tmc_scr1_read3(void) {
	Log_Printf(LOG_WARN,"[TMC] SCR1 read at $0x2200003 PC=$%08x\n",m68k_getpc());
	return tmc.scr1;
}

/* TMC Control Register */

static Uint8 tmc_ctrl_read0(void) {
	return (tmc.control>>24);
}
static Uint8 tmc_ctrl_read1(void) {
	return (tmc.control>>16);
}
static Uint8 tmc_ctrl_read2(void) {
	return (tmc.control>>8);
}
static Uint8 tmc_ctrl_read3(void) {
	return tmc.control;
}

static void tmc_ctrl_write0(Uint8 val) {
	tmc.control &= 0x00FFFFFF;
	tmc.control |= (val&0xFF)<<24;
}
static void tmc_ctrl_write1(Uint8 val) {
	tmc.control &= 0xFF00FFFF;
	tmc.control |= (val&0xFF)<<16;
}
static void tmc_ctrl_write2(Uint8 val) {
	val &= ~0x04; /* no parity memory */
	
	tmc.control &= 0xFFFF00FF;
	tmc.control |= (val&0xFF)<<8;
}
static void tmc_ctrl_write3(Uint8 val) {
	tmc.control &= 0xFFFFFF00;
	tmc.control |= val&0xFF;
}


/* Video Interrupt Register */
#define TMC_VI_INTERRUPT	0x01
#define TMC_VI_INT_MASK		0x02

/* Horizontal and Vertical Configruation Registers */
#define HFPORCH  0x18
#define HSYNC	 0x20
#define HBPORCH  0x48
#define HDISCNT	 0x118

#define VFPORCH  0x08
#define VSYNC	 0x08
#define VBPORCH	 0x30
#define VDISCNT	 0x340

static void tmc_video_reg_reset(void) {
	tmc.video_intr = 0x00;
	tmc.horizontal = (HFPORCH<<25)|(HSYNC<<19)|(HBPORCH<<12)|HDISCNT;
	tmc.vertical = (VFPORCH<<25)|(VSYNC<<19)|(VBPORCH<<12)|VDISCNT;
}

void tmc_video_interrupt(void) {
	if (tmc.video_intr&TMC_VI_INT_MASK) {
		set_interrupt(INT_DISK, SET_INT);
		tmc.video_intr |= TMC_VI_INTERRUPT;
	}
}

static Uint8 tmc_vir_read0(void) {
	return tmc.video_intr;
}

static void tmc_vir_write0(Uint8 val) {
	tmc.video_intr = val;
	if (tmc.video_intr&TMC_VI_INTERRUPT) {
		tmc.video_intr &= ~TMC_VI_INTERRUPT;
		set_interrupt(INT_DISK, RELEASE_INT);
	}
}

static Uint8 tmc_hcr_read0(void) {
	return (tmc.horizontal>>24);
}
static Uint8 tmc_hcr_read1(void) {
	return (tmc.horizontal>>16);
}
static Uint8 tmc_hcr_read2(void) {
	return (tmc.horizontal>>8);
}
static Uint8 tmc_hcr_read3(void) {
	return tmc.horizontal;
}

static void tmc_hcr_write0(Uint8 val) {
	tmc.horizontal &= 0x00FFFFFF;
	tmc.horizontal |= (val&0xFF)<<24;
}
static void tmc_hcr_write1(Uint8 val) {
	tmc.horizontal &= 0xFF00FFFF;
	tmc.horizontal |= (val&0xFF)<<16;
}
static void tmc_hcr_write2(Uint8 val) {
	tmc.horizontal &= 0xFFFF00FF;
	tmc.horizontal |= (val&0xFF)<<8;
}
static void tmc_hcr_write3(Uint8 val) {
	tmc.horizontal &= 0xFFFFFF00;
	tmc.horizontal |= val&0xFF;
}

static Uint8 tmc_vcr_read0(void) {
	return (tmc.vertical>>24);
}
static Uint8 tmc_vcr_read1(void) {
	return (tmc.vertical>>16);
}
static Uint8 tmc_vcr_read2(void) {
	return (tmc.vertical>>8);
}
static Uint8 tmc_vcr_read3(void) {
	return tmc.vertical;
}

static void tmc_vcr_write0(Uint8 val) {
	tmc.vertical &= 0x00FFFFFF;
	tmc.vertical |= (val&0xFF)<<24;
}
static void tmc_vcr_write1(Uint8 val) {
	tmc.vertical &= 0xFF00FFFF;
	tmc.vertical |= (val&0xFF)<<16;
}
static void tmc_vcr_write2(Uint8 val) {
	tmc.vertical &= 0xFFFF00FF;
	tmc.vertical |= (val&0xFF)<<8;
}
static void tmc_vcr_write3(Uint8 val) {
	tmc.vertical &= 0xFFFFFF00;
	tmc.vertical |= val&0xFF;
}


/* Read register functions */
static Uint8 (*tmc_read_reg[36])(void) = {
	tmc_scr1_read0, tmc_scr1_read1, tmc_scr1_read2, tmc_scr1_read3,
	tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read,
	tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read,
	tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read,
	tmc_ctrl_read0, tmc_ctrl_read1, tmc_ctrl_read2, tmc_ctrl_read3,
	tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read,
	tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read,
	tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read,
	tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read
};

static Uint8 (*tmc_read_vid_reg[16])(void) = {
	tmc_vir_read0, tmc_void_read, tmc_void_read, tmc_void_read,
	tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read, tmc_unimpl_read,
	tmc_hcr_read0, tmc_hcr_read1, tmc_hcr_read2, tmc_hcr_read3,
	tmc_vcr_read0, tmc_vcr_read1, tmc_vcr_read2, tmc_vcr_read3
};

/* Write register functions */
static void (*tmc_write_reg[36])(Uint8) = {
	tmc_ill_write, tmc_ill_write, tmc_ill_write, tmc_ill_write,
	tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write,
	tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write,
	tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write,
	tmc_ctrl_write0, tmc_ctrl_write1, tmc_ctrl_write2, tmc_ctrl_write3,
	tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write,
	tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write,
	tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write,
	tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write
};

static void (*tmc_write_vid_reg[16])(Uint8) = {
	tmc_vir_write0, tmc_void_write, tmc_void_write, tmc_void_write,
	tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write, tmc_unimpl_write,
	tmc_hcr_write0, tmc_hcr_write1, tmc_hcr_write2, tmc_hcr_write3,
	tmc_vcr_write0, tmc_vcr_write1, tmc_vcr_write2, tmc_vcr_write3
};


Uint32 tmc_lget(uaecptr addr) {
	Uint32 val = 0;
	
    if (addr%4) {
        Log_Printf(LOG_WARN, "[TMC] Unaligned access.");
        abort();
    }
	
	if (addr==0x02210000) {
		Log_Printf(LOG_WARN, "[TMC] Nitro register lget from $%08X",addr);
		if (ConfigureParams.System.nCpuFreq==40) {
			val = tmc.nitro;
		} else {
			Log_Printf(LOG_WARN, "[TMC] No nitro --> bus error!");
			M68000_BusError(addr, 1);
		}
		return val;
	}

	if ((addr&0xFFFFF00)==TMC_ADB_ADDR_MASK) {
		return adb_lget(addr);
	}
	
	Log_Printf(LOG_TMC_LEVEL, "[TMC] lget from %08X",addr);
	
	addr &= TMC_REGS_MASK;

	if (addr<36) {
		val = tmc_read_reg[addr]()<<24;
		val |= tmc_read_reg[addr+1]()<<16;
		val |= tmc_read_reg[addr+2]()<<8;
		val |= tmc_read_reg[addr+3]();
	} else if (addr>=128 && addr<144) {
		val = tmc_read_vid_reg[addr&0xF]()<<24;
		val |= tmc_read_vid_reg[(addr+1)&0xF]()<<16;
		val |= tmc_read_vid_reg[(addr+2)&0xF]()<<8;
		val |= tmc_read_vid_reg[(addr+3)&0xF]();
	}

	return val;
}

Uint32 tmc_wget(uaecptr addr) {
    Uint32 val = 0;
    
	if (addr%2) {
		Log_Printf(LOG_WARN, "[TMC] Unaligned access.");
		abort();
	}
	
	if ((addr&0xFFFFF00)==TMC_ADB_ADDR_MASK) {
		return adb_wget(addr);
	}
	
	Log_Printf(LOG_TMC_LEVEL, "[TMC] wget from %08X",addr);

	addr &= TMC_REGS_MASK;
	
	if (addr<36) {
		val = tmc_read_reg[addr]()<<8;
		val |= tmc_read_reg[addr+1]();
	} else if (addr>=128 && addr<144) {
		val = tmc_read_vid_reg[addr&0xF]()<<8;
		val |= tmc_read_vid_reg[(addr+1)&0xF]();
	}
	
	return val;
}

Uint32 tmc_bget(uaecptr addr) {
	if ((addr&0xFFFFF00)==TMC_ADB_ADDR_MASK) {
		return adb_bget(addr);
	}
	
	Log_Printf(LOG_TMC_LEVEL, "[TMC] bget from %08X",addr);

	addr &= TMC_REGS_MASK;

	if (addr<36) {
		return tmc_read_reg[addr]();
	} else if (addr>=128 && addr<144) {
		return tmc_read_vid_reg[addr&0xF]();
	}

    return 0;
}

void tmc_lput(uaecptr addr, Uint32 l) {
	if (addr%4) {
		Log_Printf(LOG_WARN, "[TMC] Unaligned access.");
		abort();
	}
	
	if (addr==0x02210000) {
		Log_Printf(LOG_WARN, "[TMC] Nitro register lput %08X at $%08X",l,addr);
		if (ConfigureParams.System.nCpuFreq==40) {
			tmc.nitro = l&0x0000011F;
		} else {
			Log_Printf(LOG_WARN, "[TMC] No nitro --> bus error!");
			M68000_BusError(addr, 0);
		}
		return;
	}
	
	if ((addr&0xFFFFF00)==TMC_ADB_ADDR_MASK) {
		adb_lput(addr, l);
		return;
	}
	
	Log_Printf(LOG_TMC_LEVEL, "[TMC] lput %08X to %08X",l,addr);

	addr &= TMC_REGS_MASK;
	
	if (addr<36) {
		tmc_write_reg[addr](l>>24);
		tmc_write_reg[addr+1](l>>16);
		tmc_write_reg[addr+2](l>>8);
		tmc_write_reg[addr+3](l);
	} else if (addr>=128 && addr<144) {
		tmc_write_vid_reg[addr&0xF](l>>24);
		tmc_write_vid_reg[(addr+1)&0xF](l>>16);
		tmc_write_vid_reg[(addr+2)&0xF](l>>8);
		tmc_write_vid_reg[(addr+3)&0xF](l);
	}
}

void tmc_wput(uaecptr addr, Uint32 w) {
	if (addr%2) {
		Log_Printf(LOG_WARN, "[TMC] Unaligned access.");
		abort();
	}
	
	if ((addr&0xFFFFF00)==TMC_ADB_ADDR_MASK) {
		adb_wput(addr, w);
		return;
	}
	
	Log_Printf(LOG_TMC_LEVEL, "[TMC] wput %04X to %08X",w,addr);

	addr &= TMC_REGS_MASK;
	
	if (addr<36) {
		tmc_write_reg[addr](w>>8);
		tmc_write_reg[addr+1](w);
	} else if (addr>=128 && addr<144) {
		tmc_write_vid_reg[addr&0xF](w>>8);
		tmc_write_vid_reg[(addr+1)&0xF](w);
	}
}

void tmc_bput(uaecptr addr, Uint32 b) {
	if ((addr&0xFFFFF00)==TMC_ADB_ADDR_MASK) {
		adb_bput(addr, b);
		return;
	}
	
	Log_Printf(LOG_TMC_LEVEL, "[TMC] bput %02X to %08X",b,addr);

	addr &= TMC_REGS_MASK;
	
	if (addr<36) {
		tmc_write_reg[addr](b);
	} else if (addr>=128 && addr<144) {
		tmc_write_vid_reg[addr&0xF](b);
	}
}


/* TMC Reset */

void TMC_Reset(void) {
	TurboSCR1_Reset();
	
	tmc_video_reg_reset();
	tmc.control = 0x0D17038F;
	tmc.nitro = 0x00000000;
	ADB_Reset();
}
