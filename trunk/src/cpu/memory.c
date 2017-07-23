/*
 * UAE - The Un*x Amiga Emulator - CPU core
 *
 * Memory management
 *
 * (c) 1995 Bernd Schmidt
 *
 * Adaptation to Hatari by Thomas Huth
 * Adaptation to Previous by Andreas Grabher
 *
 * This file is distributed under the GNU Public License, version 2 or at
 * your option any later version. Read the file gpl.txt for details.
 */
const char Memory_fileid[] = "Previous memory.c : " __DATE__ " " __TIME__;

#include "config.h"
#include "sysdeps.h"
#include "hatari-glue.h"
#include "maccess.h"
#include "memory.h"

#include "main.h"
#include "ioMem.h"
#include "bmap.h"
#include "tmc.h"
#include "nbic.h"
#include "reset.h"
#include "nextMemory.h"
#include "m68000.h"
#include "configuration.h"

#include "newcpu.h"


/* Set illegal_mem to 1 for debug output: */
#define illegal_mem 1

#define illegal_trace(s) {static int count=0; if (count++<50) { s; }}

/*
 * NeXT memory map (example for 68030 NeXT Computer)
 *
 * Local bus:
 * 0x00000000 - 0x0001FFFF: ROM
 * 0x01000000 - 0x0101FFFF: ROM mirror
 *
 * 0x02000000 - 0x020FFFFF: Device space
 *
 * 0x04000000 - 0x04FFFFFF: RAM bank 0
 * 0x05000000 - 0x05FFFFFF: RAM bank 1
 * 0x06000000 - 0x06FFFFFF: RAM bank 2
 * 0x07000000 - 0x07FFFFFF: RAM bank 3
 *
 * 0x0B000000 - 0x0B03FFFF: VRAM
 *
 * 0x0C000000 - 0x0C03FFFF: VRAM mirror for MWF0
 * 0x0D000000 - 0x0D03FFFF: VRAM mirror for MWF1
 * 0x0E000000 - 0x0E03FFFF: VRAM mirror for MWF2
 * 0x0F000000 - 0x0F03FFFF: VRAM mirror for MWF3
 *
 * 0x10000000 - 0x13FFFFFF: RAM mirror for MWF0
 * 0x14000000 - 0x17FFFFFF: RAM mirror for MWF1
 * 0x18000000 - 0x1BFFFFFF: RAM mirror for MWF2
 * 0x1C000000 - 0x1FFFFFFF: RAM mirror for MWF3
 *
 * NeXTbus: (Note: Boards can be configured to occupy 2 slots)
 * 0x00000000 - 0x1FFFFFFF: NeXTbus board space for slot 0
 * 0x20000000 - 0x3FFFFFFF: NeXTbus board space for slot 2
 * 0x40000000 - 0x5FFFFFFF: NeXTbus board space for slot 4
 * 0x60000000 - 0x7FFFFFFF: NeXTbus board space for slot 6
 *
 * 0xF0000000 - 0xF0FFFFFF: NeXTbus slot space for slot 0
 * 0xF2000000 - 0xF2FFFFFF: NeXTbus slot space for slot 2
 * 0xF4000000 - 0xF4FFFFFF: NeXTbus slot space for slot 4
 * 0xF6000000 - 0xF6FFFFFF: NeXTbus slot space for slot 6
 */


/* ROM */
#define NEXT_EPROM_START		0x00000000
#define NEXT_EPROM_DIAG_START	0x01000000
#define NEXT_EPROM_BMAP_START	0x01000000
#define NEXT_EPROM_SIZE			0x00020000
#define NEXT_EPROM_MASK			0x0001FFFF

/* Main memory */
#define NEXT_RAM_START			0x04000000

#define N_BANKS 4
#define NEXT_RAM_BANK_MAX		0x01000000
#define NEXT_RAM_BANK_MAX_C		0x00800000
#define NEXT_RAM_BANK_MAX_T		0x02000000

#define NEXT_RAM_BANK_SEL		0x03000000
#define NEXT_RAM_BANK_SEL_C		0x01800000
#define NEXT_RAM_BANK_SEL_T		0x06000000

uae_u32 NEXT_ram_bank_size;
uae_u32 NEXT_ram_bank_mask;
uae_u32 NEXT_ram_bank0_mask;
uae_u32 NEXT_ram_bank1_mask;
uae_u32 NEXT_ram_bank2_mask;
uae_u32 NEXT_ram_bank3_mask;

/* Main memory with memory write functions */
#define NEXT_RAM_MWF0_START		0x10000000
#define NEXT_RAM_MWF1_START		0x14000000
#define NEXT_RAM_MWF2_START		0x18000000
#define NEXT_RAM_MWF3_START		0x1C000000

/* VRAM monochrome */
#define NEXT_VRAM_START			0x0B000000
#define NEXT_VRAM_SIZE			0x00040000
#define NEXT_VRAM_MASK			0x0003FFFF

/* VRAM monochrome with memory write functions */
#define NEXT_VRAM_MWF0_START	0x0C000000
#define NEXT_VRAM_MWF1_START	0x0D000000
#define NEXT_VRAM_MWF2_START	0x0E000000
#define NEXT_VRAM_MWF3_START	0x0F000000

/* VRAM color */
#define NEXT_VRAM_COLOR_START	0x2C000000
#define NEXT_VRAM_COLOR_SIZE	0x00200000
#define NEXT_VRAM_COLOR_MASK	0x001FFFFF

/* VRAM turbo monochrome and color */
#define NEXT_VRAM_TURBO_START	0x0C000000

/* IO memory */
#define NEXT_IO_START			0x02000000
#define NEXT_IO_BMAP_START		0x02100000
#define NEXT_IO_TMC_START		0x02200000
#define NEXT_IO_SIZE			0x00020000
#define NEXT_IO_MASK			0x0001FFFF

#define NEXT_BMAP_START			0x020C0000
#define NEXT_BMAP_SIZE			0x00000040
#define NEXT_BMAP_MASK			0x0000003F

#define NEXT_BMAP_MAP_SIZE		0x00010000

#define NEXT_NBIC_START			0x02020000
#define NEXT_NBIC_SIZE			0x00000008
#define NEXT_NBIC_MASK			0x00000007

#define NEXT_NBIC_MAP_SIZE		0x00010000

/* Cache memory for nitro systems */
#define NEXT_CACHE_TAG_START	0x03E00000
#define NEXT_CACHE_TAG_SIZE		0x00100000
#define NEXT_CACHE_TAG_MASK		0x000FFFFF

#define NEXT_CACHE_START		0x03F00000
#define NEXT_CACHE_SIZE			0x00100000
#define NEXT_CACHE_MASK			0x000FFFFF

/* NeXTbus slot memory space */
#define NEXTBUS_SLOT_START(x)	(0xF0000000|((x)<<24))
#define NEXTBUS_SLOT_SIZE		0x01000000
#define NEXTBUS_SLOT_MASK		0x00FFFFFF

/* NeXTbus board memory space */
#define NEXTBUS_BOARD_START(x)	((x)<<28)
#define NEXTBUS_BOARD_SIZE		0x10000000
#define NEXTBUS_BOARD_MASK		0x0FFFFFFF


uae_u8 NEXTVideo[256*1024];

uae_u8 NEXTColorVideo[2*1024*1024];


#ifdef SAVE_MEMORY_BANKS
addrbank *mem_banks[65536];
#else
addrbank mem_banks[65536];
#endif

#ifdef NO_INLINE_MEMORY_ACCESS
__inline__ uae_u32 longget (uaecptr addr)
{
	return call_mem_get_func (get_mem_bank (addr).lget, addr);
}
__inline__ uae_u32 wordget (uaecptr addr)
{
	return call_mem_get_func (get_mem_bank (addr).wget, addr);
}
__inline__ uae_u32 byteget (uaecptr addr)
{
	return call_mem_get_func (get_mem_bank (addr).bget, addr);
}
__inline__ void longput (uaecptr addr, uae_u32 l)
{
	call_mem_put_func (get_mem_bank (addr).lput, addr, l);
}
__inline__ void wordput (uaecptr addr, uae_u32 w)
{
	call_mem_put_func (get_mem_bank (addr).wput, addr, w);
}
__inline__ void byteput (uaecptr addr, uae_u32 b)
{
	call_mem_put_func (get_mem_bank (addr).bput, addr, b);
}
#endif


/* Some prototypes: */
void SDL_Quit(void);

uae_u8 ce_cachable[65536];


/* **** A dummy bank that only contains zeros **** */

static uae_u32 dummy_lget(uaecptr addr)
{
	illegal_trace(write_log ("Illegal lget at %08lx PC=%08x\n", (long)addr,m68k_getpc()));
	return 0;
}

static uae_u32 dummy_wget(uaecptr addr)
{
	illegal_trace(write_log ("Illegal wget at %08lx PC=%08x\n", (long)addr,m68k_getpc()));
	return 0;
}

static uae_u32 dummy_bget(uaecptr addr)
{
	illegal_trace(write_log ("Illegal bget at %08lx PC=%08x\n", (long)addr,m68k_getpc()));
	return 0;
}

static void dummy_lput(uaecptr addr, uae_u32 l)
{
	illegal_trace(write_log ("Illegal lput at %08lx PC=%08x\n", (long)addr,m68k_getpc()));
}

static void dummy_wput(uaecptr addr, uae_u32 w)
{
	illegal_trace(write_log ("Illegal wput at %08lx PC=%08x\n", (long)addr,m68k_getpc()));
}

static void dummy_bput(uaecptr addr, uae_u32 b)
{
	illegal_trace(write_log ("Illegal bput at %08lx PC=%08x\n", (long)addr,m68k_getpc()));
}


/* **** This memory bank only generates bus errors **** */

static uae_u32 BusErrMem_lget(uaecptr addr)
{
	if (illegal_mem)
		write_log ("Bus error lget at %08lx\n", (long)addr);
	
	M68000_BusError(addr, 1);
	return 0;
}

static uae_u32 BusErrMem_wget(uaecptr addr)
{
	if (illegal_mem)
		write_log ("Bus error wget at %08lx\n", (long)addr);
	
	M68000_BusError(addr, 1);
	return 0;
}

static uae_u32 BusErrMem_bget(uaecptr addr)
{
	if (illegal_mem)
		write_log ("Bus error bget at %08lx\n", (long)addr);
	
	M68000_BusError(addr, 1);
	return 0;
}

static void BusErrMem_lput(uaecptr addr, uae_u32 l)
{
	if (illegal_mem)
		write_log ("Bus error lput at %08lx\n", (long)addr);
	
	M68000_BusError(addr, 0);
}

static void BusErrMem_wput(uaecptr addr, uae_u32 w)
{
	if (illegal_mem)
		write_log ("Bus error wput at %08lx\n", (long)addr);
	
	M68000_BusError(addr, 0);
}

static void BusErrMem_bput(uaecptr addr, uae_u32 b)
{
	if (illegal_mem)
		write_log ("Bus error bput at %08lx\n", (long)addr);
	
	M68000_BusError(addr, 0);
}


/* **** ROM memory **** */

extern int SCR_ROM_overlay;

uae_u8 *ROMmemory;

static uae_u32 mem_rom_lget(uaecptr addr)
{
	//  if ((addr<0x2000) && (SCR_ROM_overlay)) {do_get_mem_long(NEXTRam + 0x03FFE000 +addr);}
	addr &= NEXT_EPROM_MASK;
	return do_get_mem_long(ROMmemory + addr);
}

static uae_u32 mem_rom_wget(uaecptr addr)
{
	//  if ((addr<0x2000) && (SCR_ROM_overlay)) {do_get_mem_word(NEXTRam + 0x03FFE000 +addr);}
	addr &= NEXT_EPROM_MASK;
	return do_get_mem_word(ROMmemory + addr);
	
}

static uae_u32 mem_rom_bget(uaecptr addr)
{
	//  if ((addr<0x2000) && (SCR_ROM_overlay)) {return NEXTRam[0x03FFE000 +addr];}
	addr &= NEXT_EPROM_MASK;
	return ROMmemory[addr];
}

static void mem_rom_lput(uaecptr addr, uae_u32 b)
{
	illegal_trace(write_log ("Illegal ROMmem lput at %08lx\n", (long)addr));
	M68000_BusError(addr, 0);
}

static void mem_rom_wput(uaecptr addr, uae_u32 b)
{
	illegal_trace(write_log ("Illegal ROMmem wput at %08lx\n", (long)addr));
	M68000_BusError(addr, 0);
}

static void mem_rom_bput(uaecptr addr, uae_u32 b)
{
	illegal_trace(write_log ("Illegal ROMmem bput at %08lx\n", (long)addr));
	M68000_BusError(addr, 0);
}


/* **** NEXT RAM memory **** */

static uae_u32 mem_ram_bank0_lget(uaecptr addr)
{
	addr &= NEXT_ram_bank0_mask;
	return do_get_mem_long(NEXTRam + addr);
}

static uae_u32 mem_ram_bank0_wget(uaecptr addr)
{
	addr &= NEXT_ram_bank0_mask;
	return do_get_mem_word(NEXTRam + addr);
}

static uae_u32 mem_ram_bank0_bget(uaecptr addr)
{
	addr &= NEXT_ram_bank0_mask;
	return NEXTRam[addr];
}

static void mem_ram_bank0_lput(uaecptr addr, uae_u32 l)
{
	addr &= NEXT_ram_bank0_mask;
	do_put_mem_long(NEXTRam + addr, l);
}

static void mem_ram_bank0_wput(uaecptr addr, uae_u32 w)
{
	addr &= NEXT_ram_bank0_mask;
	do_put_mem_word(NEXTRam + addr, w);
}

static void mem_ram_bank0_bput(uaecptr addr, uae_u32 b)
{
	addr &= NEXT_ram_bank0_mask;
	NEXTRam[addr] = b;
}


static uae_u32 mem_ram_bank1_lget(uaecptr addr)
{
	addr &= NEXT_ram_bank1_mask;
	return do_get_mem_long(NEXTRam + addr);
}

static uae_u32 mem_ram_bank1_wget(uaecptr addr)
{
	addr &= NEXT_ram_bank1_mask;
	return do_get_mem_word(NEXTRam + addr);
}

static uae_u32 mem_ram_bank1_bget(uaecptr addr)
{
	addr &= NEXT_ram_bank1_mask;
	return NEXTRam[addr];
}

static void mem_ram_bank1_lput(uaecptr addr, uae_u32 l)
{
	addr &= NEXT_ram_bank1_mask;
	do_put_mem_long(NEXTRam + addr, l);
}

static void mem_ram_bank1_wput(uaecptr addr, uae_u32 w)
{
	addr &= NEXT_ram_bank1_mask;
	do_put_mem_word(NEXTRam + addr, w);
}

static void mem_ram_bank1_bput(uaecptr addr, uae_u32 b)
{
	addr &= NEXT_ram_bank1_mask;
	NEXTRam[addr] = b;
}


static uae_u32 mem_ram_bank2_lget(uaecptr addr)
{
	addr &= NEXT_ram_bank2_mask;
	return do_get_mem_long(NEXTRam + addr);
}

static uae_u32 mem_ram_bank2_wget(uaecptr addr)
{
	addr &= NEXT_ram_bank2_mask;
	return do_get_mem_word(NEXTRam + addr);
}

static uae_u32 mem_ram_bank2_bget(uaecptr addr)
{
	addr &= NEXT_ram_bank2_mask;
	return NEXTRam[addr];
}

static void mem_ram_bank2_lput(uaecptr addr, uae_u32 l)
{
	addr &= NEXT_ram_bank2_mask;
	do_put_mem_long(NEXTRam + addr, l);
}

static void mem_ram_bank2_wput(uaecptr addr, uae_u32 w)
{
	addr &= NEXT_ram_bank2_mask;
	do_put_mem_word(NEXTRam + addr, w);
}

static void mem_ram_bank2_bput(uaecptr addr, uae_u32 b)
{
	addr &= NEXT_ram_bank2_mask;
	NEXTRam[addr] = b;
}


static uae_u32 mem_ram_bank3_lget(uaecptr addr)
{
	addr &= NEXT_ram_bank3_mask;
	return do_get_mem_long(NEXTRam + addr);
}

static uae_u32 mem_ram_bank3_wget(uaecptr addr)
{
	addr &= NEXT_ram_bank3_mask;
	return do_get_mem_word(NEXTRam + addr);
}

static uae_u32 mem_ram_bank3_bget(uaecptr addr)
{
	addr &= NEXT_ram_bank3_mask;
	return NEXTRam[addr];
}

static void mem_ram_bank3_lput(uaecptr addr, uae_u32 l)
{
	addr &= NEXT_ram_bank3_mask;
	do_put_mem_long(NEXTRam + addr, l);
}

static void mem_ram_bank3_wput(uaecptr addr, uae_u32 w)
{
	addr &= NEXT_ram_bank3_mask;
	do_put_mem_word(NEXTRam + addr, w);
}

static void mem_ram_bank3_bput(uaecptr addr, uae_u32 b)
{
	addr &= NEXT_ram_bank3_mask;
	NEXTRam[addr] = b;
}

/* **** NEXT RAM empty areas **** */

static uae_u32 mem_ram_empty_lget(uaecptr addr)
{
	if (illegal_mem)
		write_log ("Empty mem area lget at %08lx\n", (long)addr);
	
	return addr;
}

static uae_u32 mem_ram_empty_wget(uaecptr addr)
{
	if (illegal_mem)
		write_log ("Empty mem area wget at %08lx\n", (long)addr);
	
	return addr;
}

static uae_u32 mem_ram_empty_bget(uaecptr addr)
{
	if (illegal_mem)
		write_log ("Empty mem area bget at %08lx\n", (long)addr);
	
	return addr;
}

static void mem_ram_empty_lput(uaecptr addr, uae_u32 l)
{
	if (illegal_mem)
		write_log ("Empty mem area lput at %08lx\n", (long)addr);
}

static void mem_ram_empty_wput(uaecptr addr, uae_u32 w)
{
	if (illegal_mem)
		write_log ("Empty mem area wput at %08lx\n", (long)addr);
}

static void mem_ram_empty_bput(uaecptr addr, uae_u32 b)
{
	if (illegal_mem)
		write_log ("Empty mem area bput at %08lx\n", (long)addr);
}


/* **** NEXT VRAM memory for monochrome systems **** */

static uae_u32 mem_video_lget(uaecptr addr)
{
	addr &= NEXT_VRAM_MASK;
	return do_get_mem_long(NEXTVideo + addr);
}

static uae_u32 mem_video_wget(uaecptr addr)
{
	addr &= NEXT_VRAM_MASK;
	return do_get_mem_word(NEXTVideo + addr);
}

static uae_u32 mem_video_bget(uaecptr addr)
{
	addr &= NEXT_VRAM_MASK;
	return NEXTVideo[addr];
}

static void mem_video_lput(uaecptr addr, uae_u32 l)
{
	addr &= NEXT_VRAM_MASK;
	do_put_mem_long(NEXTVideo + addr, l);
}

static void mem_video_wput(uaecptr addr, uae_u32 w)
{
	addr &= NEXT_VRAM_MASK;
	do_put_mem_word(NEXTVideo + addr, w);
}

static void mem_video_bput(uaecptr addr, uae_u32 b)
{
	addr &= NEXT_VRAM_MASK;
	NEXTVideo[addr] = b;
}


/* **** NEXT memory banks with write functions **** */

static uae_u8 mwf0[4][4] = { /* AB */
	{ 0, 0, 0, 0 },
	{ 0, 0, 1, 1 },
	{ 0, 1, 1, 2 },
	{ 0, 1, 2, 3 }
};

static uae_u8 mwf1[4][4] = { /* ceil(A+B) */
	{ 0, 1, 2, 3 },
	{ 1, 2, 3, 3 },
	{ 2, 3, 3, 3 },
	{ 3, 3, 3, 3 }
};

static uae_u8 mwf2[4][4] = { /* (1-A)B */
	{ 0, 0, 0, 0 },
	{ 1, 1, 0, 0 },
	{ 2, 1, 1, 0 },
	{ 3, 2, 1, 0 }
};

static uae_u8 mwf3[4][4] = { /* A+B-AB */
	{ 0, 1, 2, 3 },
	{ 1, 2, 2, 3 },
	{ 2, 2, 3, 3 },
	{ 3, 3, 3, 3 }
};

static uae_u32 memory_write_func(uae_u32 old, uae_u32 new, int function, int size)
{
	int a,b,i;
	uae_u32 v=0;
#if 0
	write_log("[MWF] Function%i: size=%i, old=%08X, new=%08X\n",function,size,old,new);
#endif
	
	switch (function) {
		case 0:
			for (i=0; i<(size*4); i++) {
				a=old>>(i*2)&3;
				b=new>>(i*2)&3;
				v|=mwf0[a][b]<<(i*2);
			}
			return v;
		case 1:
			for (i=0; i<(size*4); i++) {
				a=old>>(i*2)&3;
				b=new>>(i*2)&3;
				v|=mwf1[a][b]<<(i*2);
			}
			return v;
		case 2:
			for (i=0; i<(size*4); i++) {
				a=old>>(i*2)&3;
				b=new>>(i*2)&3;
				v|=mwf2[a][b]<<(i*2);
			}
			return v;
		case 3:
			for (i=0; i<(size*4); i++) {
				a=old>>(i*2)&3;
				b=new>>(i*2)&3;
				v|=mwf3[a][b]<<(i*2);
			}
			return v;
			
		default:
			write_log("Unknown memory write function!\n");
			abort();
	}
}

static uae_u32 mem_ram_mwf_lget(uaecptr addr)
{
	int function = (addr>>26)&0x3;
	addr = NEXT_RAM_START|(addr&0x03FFFFFF);
	
	return function==0?0xFFFFFFFF:0;
}

static uae_u32 mem_ram_mwf_wget(uaecptr addr)
{
	int function = (addr>>26)&0x3;
	addr = NEXT_RAM_START|(addr&0x03FFFFFF);
	
	return function==0?0xFFFF:0;
}

static uae_u32 mem_ram_mwf_bget(uaecptr addr)
{
	int function = (addr>>26)&0x3;
	addr = NEXT_RAM_START|(addr&0x03FFFFFF);
	
	return function==0?0xFF:0;
}

static void mem_ram_mwf_lput(uaecptr addr, uae_u32 l)
{
	int function = (addr>>26)&0x3;
	addr = NEXT_RAM_START|(addr&0x03FFFFFF);
	
	uae_u32 old = longget(addr);
	uae_u32 val = memory_write_func(old, l, function, 4);
	
	longput(addr, val);
}

static void mem_ram_mwf_wput(uaecptr addr, uae_u32 w)
{
	int function = (addr>>26)&0x3;
	addr = NEXT_RAM_START|(addr&0x03FFFFFF);
	
	uae_u32 old = wordget(addr);
	uae_u32 val = memory_write_func(old, w, function, 2);
	
	wordput(addr, val);
}

static void mem_ram_mwf_bput(uaecptr addr, uae_u32 b)
{
	int function = (addr>>26)&0x3;
	addr = NEXT_RAM_START|(addr&0x03FFFFFF);
	
	uae_u32 old = byteget(addr);
	uae_u32 val = memory_write_func(old, b, function, 1);
	
	byteput(addr, val);
}


static uae_u32 mem_video_mwf_lget(uaecptr addr)
{
	int function = (addr>>24)&0x3;
	addr = NEXT_VRAM_START|(addr&NEXT_VRAM_MASK);
	
	return function==0?0xFFFFFFFF:0;
}

static uae_u32 mem_video_mwf_wget(uaecptr addr)
{
	int function = (addr>>24)&0x3;
	addr = NEXT_VRAM_START|(addr&NEXT_VRAM_MASK);
	
	return function==0?0xFFFF:0;
}

static uae_u32 mem_video_mwf_bget(uaecptr addr)
{
	int function = (addr>>24)&0x3;
	addr = NEXT_VRAM_START|(addr&NEXT_VRAM_MASK);
	
	return function==0?0xFF:0;
}

static void mem_video_mwf_lput(uaecptr addr, uae_u32 l)
{
	int function = (addr>>24)&0x3;
	addr = NEXT_VRAM_START|(addr&NEXT_VRAM_MASK);
	
	uae_u32 old = longget(addr);
	uae_u32 val = memory_write_func(old, l, function, 4);
	
	longput(addr, val);
}

static void mem_video_mwf_wput(uaecptr addr, uae_u32 w)
{
	int function = (addr>>24)&0x3;
	addr = NEXT_VRAM_START|(addr&NEXT_VRAM_MASK);
	
	uae_u32 old = wordget(addr);
	uae_u32 val = memory_write_func(old, w, function, 2);
	
	wordput(addr, val);
}

static void mem_video_mwf_bput(uaecptr addr, uae_u32 b)
{
	int function = (addr>>24)&0x3;
	addr = NEXT_VRAM_START|(addr&NEXT_VRAM_MASK);
	
	uae_u32 old = byteget(addr);
	uae_u32 val = memory_write_func(old, b, function, 1);
	
	byteput(addr, val);
}


/* **** NEXT VRAM memory for color systems **** */

static uae_u32 mem_color_video_lget(uaecptr addr)
{
	addr &= NEXT_VRAM_COLOR_MASK;
	return do_get_mem_long(NEXTColorVideo + addr);
}

static uae_u32 mem_color_video_wget(uaecptr addr)
{
	addr &= NEXT_VRAM_COLOR_MASK;
	return do_get_mem_word(NEXTColorVideo + addr);
}

static uae_u32 mem_color_video_bget(uaecptr addr)
{
	addr &= NEXT_VRAM_COLOR_MASK;
	return NEXTColorVideo[addr];
}

static void mem_color_video_lput(uaecptr addr, uae_u32 l)
{
	addr &= NEXT_VRAM_COLOR_MASK;
	do_put_mem_long(NEXTColorVideo + addr, l);
}

static void mem_color_video_wput(uaecptr addr, uae_u32 w)
{
	addr &= NEXT_VRAM_COLOR_MASK;
	do_put_mem_word(NEXTColorVideo + addr, w);
}

static void mem_color_video_bput(uaecptr addr, uae_u32 b)
{
	addr &= NEXT_VRAM_COLOR_MASK;
	NEXTColorVideo[addr] = b;
}


/* Hardware IO memory */
/* --> see ioMem.c */
uae_u8 *IOmemory;


/* **** NEXT BMAP memory **** */

static uae_u32 mem_bmap_lget(uaecptr addr)
{
	if ((addr&NEXT_BMAP_MASK)>NEXT_BMAP_SIZE) {
		write_log ("bmap bus error at %08lx PC=%08x\n", (long)addr,m68k_getpc());
		M68000_BusError(addr, 0);
		return 0;
	}
	//write_log ("bmap lget at %08lx PC=%08x\n", (long)addr,m68k_getpc());
	addr &= NEXT_BMAP_MASK;
	return bmap_lget(addr);
}

static uae_u32 mem_bmap_wget(uaecptr addr)
{
	if ((addr&NEXT_BMAP_MASK)>NEXT_BMAP_SIZE) {
		write_log ("bmap bus error at %08lx PC=%08x\n", (long)addr,m68k_getpc());
		M68000_BusError(addr, 0);
		return 0;
	}
	//write_log ("bmap wget at %08lx PC=%08x\n", (long)addr,m68k_getpc());
	addr &= NEXT_BMAP_MASK;
	return bmap_wget(addr);
}

static uae_u32 mem_bmap_bget(uaecptr addr)
{
	if ((addr&NEXT_BMAP_MASK)>NEXT_BMAP_SIZE) {
		write_log ("bmap bus error at %08lx PC=%08x\n", (long)addr,m68k_getpc());
		M68000_BusError(addr, 0);
		return 0;
	}
	//write_log ("bmap bget at %08lx PC=%08x\n", (long)addr,m68k_getpc());
	addr &= NEXT_BMAP_MASK;
	return bmap_bget(addr);
}

static void mem_bmap_lput(uaecptr addr, uae_u32 l)
{
	if ((addr&NEXT_BMAP_MASK)>NEXT_BMAP_SIZE) {
		write_log ("bmap bus error at %08lx PC=%08x\n", (long)addr,m68k_getpc());
		M68000_BusError(addr, 0);
	}
	//write_log ("bmap lput at %08lx val=%x PC=%08x\n", (long)addr,l,m68k_getpc());
	addr &= NEXT_BMAP_MASK;
	bmap_lput(addr, l);
}

static void mem_bmap_wput(uaecptr addr, uae_u32 w)
{
	if ((addr&NEXT_BMAP_MASK)>NEXT_BMAP_SIZE) {
		write_log ("bmap bus error at %08lx PC=%08x\n", (long)addr,m68k_getpc());
		M68000_BusError(addr, 0);
	}
	//write_log ("bmap wput at %08lx val=%x PC=%08x\n", (long)addr,w,m68k_getpc());
	addr &= NEXT_BMAP_MASK;
	bmap_wput(addr, w);
}

static void mem_bmap_bput(uaecptr addr, uae_u32 b)
{
	if ((addr&NEXT_BMAP_MASK)>NEXT_BMAP_SIZE) {
		write_log ("bmap bus error at %08lx PC=%08x\n", (long)addr,m68k_getpc());
		M68000_BusError(addr, 0);
	}
	//write_log ("bmap bput at %08lx val=%x PC=%08x\n", (long)addr,b,m68k_getpc());
	addr &= NEXT_BMAP_MASK;
	bmap_bput(addr, b);
}



/* **** Address banks **** */

static addrbank dummy_bank =
{
	dummy_lget, dummy_wget, dummy_bget,
	dummy_lput, dummy_wput, dummy_bput,
	dummy_lget, dummy_wget, ABFLAG_NONE
};

static addrbank BusErrMem_bank =
{
	BusErrMem_lget, BusErrMem_wget, BusErrMem_bget,
	BusErrMem_lput, BusErrMem_wput, BusErrMem_bput,
	BusErrMem_lget, BusErrMem_wget, ABFLAG_NONE
};

static addrbank ROM_bank =
{
	mem_rom_lget, mem_rom_wget, mem_rom_bget,
	mem_rom_lput, mem_rom_wput, mem_rom_bput,
	mem_rom_lget, mem_rom_wget, ABFLAG_ROM
};

static addrbank RAM_bank0 =
{
	mem_ram_bank0_lget, mem_ram_bank0_wget, mem_ram_bank0_bget,
	mem_ram_bank0_lput, mem_ram_bank0_wput, mem_ram_bank0_bput,
	mem_ram_bank0_lget, mem_ram_bank0_wget, ABFLAG_RAM
	
};

static addrbank RAM_bank1 =
{
	mem_ram_bank1_lget, mem_ram_bank1_wget, mem_ram_bank1_bget,
	mem_ram_bank1_lput, mem_ram_bank1_wput, mem_ram_bank1_bput,
	mem_ram_bank1_lget, mem_ram_bank1_wget, ABFLAG_RAM
	
};

static addrbank RAM_bank2 =
{
	mem_ram_bank2_lget, mem_ram_bank2_wget, mem_ram_bank2_bget,
	mem_ram_bank2_lput, mem_ram_bank2_wput, mem_ram_bank2_bput,
	mem_ram_bank2_lget, mem_ram_bank2_wget, ABFLAG_RAM
	
};

static addrbank RAM_bank3 =
{
	mem_ram_bank3_lget, mem_ram_bank3_wget, mem_ram_bank3_bget,
	mem_ram_bank3_lput, mem_ram_bank3_wput, mem_ram_bank3_bput,
	mem_ram_bank3_lget, mem_ram_bank3_wget, ABFLAG_RAM
	
};

static addrbank RAM_empty_bank =
{
	mem_ram_empty_lget, mem_ram_empty_wget, mem_ram_empty_bget,
	mem_ram_empty_lput, mem_ram_empty_wput, mem_ram_empty_bput,
	mem_ram_empty_lget, mem_ram_empty_wget, ABFLAG_RAM
};

static addrbank RAM_mwf_bank =
{
	mem_ram_mwf_lget, mem_ram_mwf_wget, mem_ram_mwf_bget,
	mem_ram_mwf_lput, mem_ram_mwf_wput, mem_ram_mwf_bput,
	mem_ram_mwf_lget, mem_ram_mwf_wget, ABFLAG_RAM
};

static addrbank VRAM_bank =
{
	mem_video_lget, mem_video_wget, mem_video_bget,
	mem_video_lput, mem_video_wput, mem_video_bput,
	mem_video_lget, mem_video_wget, ABFLAG_RAM
};

static addrbank VRAM_mwf_bank =
{
	mem_video_mwf_lget, mem_video_mwf_wget, mem_video_mwf_bget,
	mem_video_mwf_lput, mem_video_mwf_wput, mem_video_mwf_bput,
	mem_video_mwf_lget, mem_video_mwf_wget, ABFLAG_RAM
};

static addrbank VRAM_color_bank =
{
	mem_color_video_lget, mem_color_video_wget, mem_color_video_bget,
	mem_color_video_lput, mem_color_video_wput, mem_color_video_bput,
	mem_color_video_lget, mem_color_video_wget, ABFLAG_RAM
};

static addrbank IO_bank =
{
	IoMem_lget, IoMem_wget, IoMem_bget,
	IoMem_lput, IoMem_wput, IoMem_bput,
	IoMem_lget, IoMem_wget, ABFLAG_IO
};

static addrbank BMAP_bank =
{
	mem_bmap_lget, mem_bmap_wget, mem_bmap_bget,
	mem_bmap_lput, mem_bmap_wput, mem_bmap_bput,
	mem_bmap_lget, mem_bmap_wget, ABFLAG_IO
};

static addrbank TMC_bank =
{
	tmc_lget, tmc_wget, tmc_bget,
	tmc_lput, tmc_wput, tmc_bput,
	tmc_lget, tmc_wget, ABFLAG_IO
};

static addrbank NBIC_bank =
{
	nbic_reg_lget, nbic_reg_wget, nbic_reg_bget,
	nbic_reg_lput, nbic_reg_wput, nbic_reg_bput,
	nbic_reg_lget, nbic_reg_wget, ABFLAG_IO
};

static addrbank NEXTBUS_slot_bank =
{
	nextbus_slot_lget, nextbus_slot_wget, nextbus_slot_bget,
	nextbus_slot_lput, nextbus_slot_wput, nextbus_slot_bput,
	nextbus_slot_lget, nextbus_slot_wget, ABFLAG_IO
};

static addrbank NEXTBUS_board_bank =
{
	nextbus_board_lget, nextbus_board_wget, nextbus_board_bget,
	nextbus_board_lput, nextbus_board_wput, nextbus_board_bput,
	nextbus_board_lget, nextbus_board_wget, ABFLAG_IO
};



static void init_mem_banks (void)
{
	int i;
	for (i = 0; i < 65536; i++)
		put_mem_bank (i<<16, &BusErrMem_bank);
}


/*
 * Initialize the memory banks
 */
const char* memory_init(int *nNewNEXTMemSize)
{
	int i;
	uae_u32 bankstart[4];
	
	/* Set machine dependent variables */
	if (ConfigureParams.System.bTurbo) {
		NEXT_ram_bank_size = NEXT_RAM_BANK_MAX_T;
		NEXT_ram_bank_mask = NEXT_RAM_BANK_SEL_T;
	} else if (ConfigureParams.System.bColor) {
		NEXT_ram_bank_size = NEXT_RAM_BANK_MAX_C;
		NEXT_ram_bank_mask = NEXT_RAM_BANK_SEL_C;
	} else {
		NEXT_ram_bank_size = NEXT_RAM_BANK_MAX;
		NEXT_ram_bank_mask = NEXT_RAM_BANK_SEL;
	}
	
	write_log("Memory init: Memory size: %iMB\n", Configuration_CheckMemory(nNewNEXTMemSize));
	
	/* Convert values from MB to byte */
	for (i=0; i<N_BANKS; i++) {
		bankstart[i] = NEXT_RAM_START + (NEXT_ram_bank_size * i);
	}
	
	/* Fill every 65536 bank with dummy */
	init_mem_banks();
	
	/* Map ROM */
	map_banks(&ROM_bank, NEXT_EPROM_START >> 16, NEXT_EPROM_SIZE>>16);
	write_log("Mapping ROM at $%08x: %ikB\n", NEXT_EPROM_START, NEXT_EPROM_SIZE/1024);
	if (ConfigureParams.System.nMachineType != NEXT_CUBE030) {
		map_banks(&ROM_bank, NEXT_EPROM_BMAP_START >> 16, NEXT_EPROM_SIZE>>16);
		write_log("Mapping ROM trough BMAP at $%08x: %ikB\n", NEXT_EPROM_BMAP_START, NEXT_EPROM_SIZE/1024);
	}
	
	/* Map main memory */
	if (nNewNEXTMemSize[0]) {
		NEXT_ram_bank0_mask = NEXT_ram_bank_mask|((nNewNEXTMemSize[0]<<20)-1);
		map_banks(&RAM_bank0, bankstart[0]>>16, NEXT_ram_bank_size >> 16);
		write_log("Mapping main memory bank0 at $%08x: %iMB\n", bankstart[0], nNewNEXTMemSize[0]);
	} else {
		NEXT_ram_bank0_mask = 0;
		map_banks(&RAM_empty_bank, bankstart[0]>>16, NEXT_ram_bank_size >> 16);
		write_log("Mapping main memory bank0 at $%08x: empty\n", bankstart[0]);
	}
	
	if (nNewNEXTMemSize[1]) {
		NEXT_ram_bank1_mask = NEXT_ram_bank_mask|((nNewNEXTMemSize[1]<<20)-1);
		map_banks(&RAM_bank1, bankstart[1]>>16, NEXT_ram_bank_size >> 16);
		write_log("Mapping main memory bank1 at $%08x: %iMB\n", bankstart[1], nNewNEXTMemSize[1]);
	} else {
		NEXT_ram_bank1_mask = 0;
		map_banks(&RAM_empty_bank, bankstart[1]>>16, NEXT_ram_bank_size >> 16);
		write_log("Mapping main memory bank1 at $%08x: empty\n", bankstart[1]);
	}
	
	if (nNewNEXTMemSize[2]) {
		NEXT_ram_bank2_mask = NEXT_ram_bank_mask|((nNewNEXTMemSize[2]<<20)-1);
		map_banks(&RAM_bank2, bankstart[2]>>16, NEXT_ram_bank_size >> 16);
		write_log("Mapping main memory bank2 at $%08x: %iMB\n", bankstart[2], nNewNEXTMemSize[2]);
	} else {
		NEXT_ram_bank2_mask = 0;
		map_banks(&RAM_empty_bank, bankstart[2]>>16, NEXT_ram_bank_size >> 16);
		write_log("Mapping main memory bank2 at $%08x: empty\n", bankstart[2]);
	}
	
	if (nNewNEXTMemSize[3]) {
		NEXT_ram_bank3_mask = NEXT_ram_bank_mask|((nNewNEXTMemSize[3]<<20)-1);
		map_banks(&RAM_bank3, bankstart[3]>>16, NEXT_ram_bank_size >> 16);
		write_log("Mapping main memory bank3 at $%08x: %iMB\n", bankstart[3], nNewNEXTMemSize[3]);
	} else {
		NEXT_ram_bank3_mask = 0;
		map_banks(&RAM_empty_bank, bankstart[3]>>16, NEXT_ram_bank_size >> 16);
		write_log("Mapping main memory bank3 at $%08x: empty\n", bankstart[3]);
	}
	
	/* Map mirrors of main memory for memory write functions */
	if (!ConfigureParams.System.bColor && !ConfigureParams.System.bTurbo) {
		map_banks(&RAM_mwf_bank, NEXT_RAM_MWF0_START>>16, (NEXT_RAM_BANK_MAX*N_BANKS) >> 16);
		map_banks(&RAM_mwf_bank, NEXT_RAM_MWF1_START>>16, (NEXT_RAM_BANK_MAX*N_BANKS) >> 16);
		map_banks(&RAM_mwf_bank, NEXT_RAM_MWF2_START>>16, (NEXT_RAM_BANK_MAX*N_BANKS) >> 16);
		map_banks(&RAM_mwf_bank, NEXT_RAM_MWF3_START>>16, (NEXT_RAM_BANK_MAX*N_BANKS) >> 16);
		write_log("Mapping mirrors of main memory for memory write functions:\n");
		for (i = 0; i<4; i++)
			write_log("Function%i at $%08X\n",i,0x10000000+0x04000000*i);
	}
	
	/* Map video memory */
	if (ConfigureParams.System.bTurbo && ConfigureParams.System.bColor) {
		map_banks(&VRAM_color_bank, NEXT_VRAM_TURBO_START>>16, NEXT_VRAM_COLOR_SIZE >> 16);
		write_log("Mapping video memory at $%08x: %ikB\n", NEXT_VRAM_TURBO_START, NEXT_VRAM_COLOR_SIZE/1024);
	} else if (ConfigureParams.System.bTurbo) {
		map_banks(&VRAM_bank, NEXT_VRAM_TURBO_START>>16, NEXT_VRAM_SIZE >> 16);
		write_log("Mapping video memory at $%08x: %ikB\n", NEXT_VRAM_TURBO_START, NEXT_VRAM_SIZE/1024);
	} else if (ConfigureParams.System.bColor) {
		map_banks(&VRAM_color_bank, NEXT_VRAM_COLOR_START>>16, NEXT_VRAM_COLOR_SIZE >> 16);
		write_log("Mapping video memory at $%08x: %ikB\n", NEXT_VRAM_COLOR_START, NEXT_VRAM_COLOR_SIZE/1024);
	} else {
		map_banks(&VRAM_bank, NEXT_VRAM_START>>16, NEXT_VRAM_SIZE >> 16);
		write_log("Mapping video memory at $%08x: %ikB\n", NEXT_VRAM_START, NEXT_VRAM_SIZE/1024);
		
		map_banks(&VRAM_mwf_bank, NEXT_VRAM_MWF0_START>>16, NEXT_VRAM_SIZE >> 16);
		map_banks(&VRAM_mwf_bank, NEXT_VRAM_MWF1_START>>16, NEXT_VRAM_SIZE >> 16);
		map_banks(&VRAM_mwf_bank, NEXT_VRAM_MWF2_START>>16, NEXT_VRAM_SIZE >> 16);
		map_banks(&VRAM_mwf_bank, NEXT_VRAM_MWF3_START>>16, NEXT_VRAM_SIZE >> 16);
		write_log("Mapping mirrors of video memory for memory write functions:\n");
		for (i = 0; i<4; i++)
			write_log("Function%i at $%08X\n",i,0x0C000000+0x01000000*i);
	}
	
	/* Map device space */
	map_banks(&IO_bank, NEXT_IO_START >> 16, NEXT_IO_SIZE>>16);
	write_log("Mapping device space at $%08X\n", NEXT_IO_START);
	
	if (ConfigureParams.System.nMachineType != NEXT_CUBE030) {
		map_banks(&IO_bank, NEXT_IO_BMAP_START >> 16, NEXT_IO_SIZE>>16);
		if (!ConfigureParams.System.bTurbo) {
			map_banks(&BMAP_bank, NEXT_BMAP_START >> 16, NEXT_BMAP_MAP_SIZE>>16);
			map_banks(&BMAP_bank, (0x80000000|NEXT_BMAP_START) >> 16, NEXT_BMAP_MAP_SIZE>>16);
			write_log("Mapping BMAP device space at $%08X\n", NEXT_IO_BMAP_START);
		} else {
			write_log("Mapping device space at $%08X\n", NEXT_IO_BMAP_START);
		}
	}
	bmap_init();
	
	if (ConfigureParams.System.bTurbo) {
		map_banks(&TMC_bank, NEXT_IO_TMC_START >> 16, NEXT_IO_SIZE>>16);
		write_log("Mapping TMC device space at $%08X\n", NEXT_IO_TMC_START);

		if (ConfigureParams.System.nCpuFreq==40) {
			map_banks(&dummy_bank, NEXT_CACHE_START>>16, NEXT_CACHE_SIZE>>16);
			write_log("Mapping cache memory at $%08x: %ikB\n", NEXT_CACHE_START, NEXT_CACHE_SIZE/1024);
			map_banks(&dummy_bank, NEXT_CACHE_TAG_START>>16, NEXT_CACHE_TAG_SIZE>>16);
			write_log("Mapping cache tag memory at $%08x: %ikB\n", NEXT_CACHE_TAG_START, NEXT_CACHE_TAG_SIZE/1024);
		}
	}

    /* Map NBIC and board spaces via NextBus */
	if (ConfigureParams.System.nMachineType!=NEXT_STATION && ConfigureParams.System.bNBIC) {
		if (!ConfigureParams.System.bTurbo) {
			map_banks(&NBIC_bank, NEXT_NBIC_START>>16, NEXT_NBIC_MAP_SIZE>>16);
			write_log("Mapping NextBus interface chip at $%08x\n", NEXT_NBIC_START);
		}
		for (i = 2; i < 8; i++) {
			if (i==8 && ConfigureParams.System.nMachineType!=NEXT_CUBE030 && !ConfigureParams.System.bTurbo) {
				/* FIXME: conflict with BMAP. Implement: only SCR2 ROM_OVERLAY enables NBIC */
				continue;
			}
			map_banks(&NEXTBUS_board_bank, NEXTBUS_BOARD_START(i)>>16, NEXTBUS_BOARD_SIZE>>16);
			write_log("Mapping NextBus board memory for slot %i at $%08x\n", i, NEXTBUS_BOARD_START(i));
		}
		for (i = 0; i < 16; i++) {
			map_banks(&NEXTBUS_slot_bank, NEXTBUS_SLOT_START(i)>>16, NEXTBUS_SLOT_SIZE>>16);
		}
		write_log("Mapping NextBus slot memory at $%08x\n", NEXTBUS_SLOT_START(i));
	}
    nextbus_init();
    
	ROMmemory=NEXTRom;
	IOmemory=NEXTIo;
	
	{
		FILE* fin;
		int ret;
		/* Loading ROM depending on emulated system */
		if(ConfigureParams.System.nMachineType == NEXT_CUBE030)
			fin=fopen(ConfigureParams.Rom.szRom030FileName,"rb");
		else if(ConfigureParams.System.bTurbo == true)
			fin=fopen(ConfigureParams.Rom.szRomTurboFileName,"rb");
		else
			fin=fopen(ConfigureParams.Rom.szRom040FileName, "rb");
		
		if (fin==NULL) {
			return "Cannot open ROM file";
		}
		
		ret=fread(ROMmemory,1,0x20000,fin);
		
		write_log("Read ROM %d\n",ret);
		fclose(fin);
	}
	
	{
		int i;
		for (i=0;i<sizeof(NEXTVideo);i++) NEXTVideo[i]=0;
		for (i=0;i<sizeof(NEXTRam);i++) NEXTRam[i]=0;
		for (i=0;i<sizeof(NEXTIo);i++) NEXTIo[i]=0;
	}
	
	IoMem_Init();
	
	return NULL;
}


/*
 * Uninitialize the memory banks.
 */
void memory_uninit (void)
{
}


void map_banks (addrbank *bank, int start, int size)
{
	int bnr;
	
	for (bnr = start; bnr < start + size; bnr++)
		put_mem_bank (bnr << 16, bank);
	return;
}

void memory_hardreset (void)
{
}
