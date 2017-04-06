#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "dimension.h"
#include "nd_mem.h"
#include "nd_devs.h"
#include "nd_rom.h"

#define write_log printf

/* --------- NEXTDIMENSION MEMORY ---------- */

/* NeXTdimension board memory */
#define ND_RAM_START	0xF8000000
#define ND_RAM_SIZE		0x04000000
#define ND_RAM_MASK		0x03FFFFFF

#define ND_VRAM_START	0xFE000000
#define ND_VRAM_SIZE	0x00400000
#define ND_VRAM_MASK	0x003FFFFF

#define ND_EEPROM_START	0xFFF00000
#define ND_EEPROM_SIZE	0x00020000
#define ND_EEPROM_MASK	0x0001FFFF

/* RAM banks */
#define ND_RAM_BANKSIZE 0x01000000
#define ND_RAM_BANKMASK 0x03000000
uae_u32 ND_RAM_bankmask0;
uae_u32 ND_RAM_bankmask1;
uae_u32 ND_RAM_bankmask2;
uae_u32 ND_RAM_bankmask3;

Uint8  ND_ram[64*1024*1024];
Uint8  ND_vram[4*1024*1024];
Uint8  ND_rom[128*1024];

Uint8 ND_dmem[512];

/* NeXTdimension dither memory */
#define ND_DMEM_START   0xFF000000
#define ND_DMEM_SIZE    0x00000200
#define ND_DMEM_MASK    0x000001FF

/* NeXTdimension board devices */
#define ND_IO_START		0xFF800000
#define ND_IO_SIZE		0x00004000
#define ND_IO_MASK		0x00003FFF

/* NeXTdimension board RAMDAC */
#define ND_RAMDAC_START	0xFF200000
#define ND_RAMDAC_SIZE  0x00001000
#define ND_RAMDAC_MASK  0x00000FFF

/* NeXTdimension board data path */
#define ND_DP_START	0xF0000000
#define ND_DP_SIZE  0x00001000
#define ND_DP_MASK  0x00000FFF

/* NeXTdimension unknown registers */
#define ND_UNKNWN_START 0xFF400000
#define ND_UNKNWN_SIZE  0x00000200
#define ND_UNKNWN_MASK  0x000001FF

/* Function to fix address for ROM access */
static uaecptr nd_rom_addr_fix(uaecptr addr)
{
#if ND_STEP
    addr >>= 2;
#else
    addr &= ~3;
    addr |= (addr>>17)&3;
#endif
    addr &= ND_EEPROM_MASK;
    return addr;
}

/* Memory banks */

nd_addrbank *nd_mem_banks[65536];

static void nd_map_banks (nd_addrbank *bank, int start, int size)
{
	int bnr;
	
	for (bnr = start; bnr < start + size; bnr++)
		nd_put_mem_bank (bnr << 16, bank);
	return;
}

/* NeXTdimension RAM */
static uae_u32 nd_ram_bank0_lget(uaecptr addr)
{
	addr &= ND_RAM_bankmask0;
	return do_get_mem_long(ND_ram + addr);
}

static uae_u32 nd_ram_bank0_wget(uaecptr addr)
{
	addr &= ND_RAM_bankmask0;
	return do_get_mem_word(ND_ram + addr);
}

static uae_u32 nd_ram_bank0_bget(uaecptr addr)
{
	addr &= ND_RAM_bankmask0;
	return ND_ram[addr];
}

static void nd_ram_bank0_lput(uaecptr addr, uae_u32 l)
{
	addr &= ND_RAM_bankmask0;
	do_put_mem_long(ND_ram + addr, l);
}

static void nd_ram_bank0_wput(uaecptr addr, uae_u32 w)
{
	addr &= ND_RAM_bankmask0;
	do_put_mem_word(ND_ram + addr, w);
}

static void nd_ram_bank0_bput(uaecptr addr, uae_u32 b)
{
	addr &= ND_RAM_bankmask0;
	ND_ram[addr] = b;
}

static uae_u32 nd_ram_bank1_lget(uaecptr addr)
{
    addr &= ND_RAM_bankmask1;
    return do_get_mem_long(ND_ram + addr);
}

static uae_u32 nd_ram_bank1_wget(uaecptr addr)
{
    addr &= ND_RAM_bankmask1;
    return do_get_mem_word(ND_ram + addr);
}

static uae_u32 nd_ram_bank1_bget(uaecptr addr)
{
    addr &= ND_RAM_bankmask1;
    return ND_ram[addr];
}

static void nd_ram_bank1_lput(uaecptr addr, uae_u32 l)
{
    addr &= ND_RAM_bankmask1;
    do_put_mem_long(ND_ram + addr, l);
}

static void nd_ram_bank1_wput(uaecptr addr, uae_u32 w)
{
    addr &= ND_RAM_bankmask1;
    do_put_mem_word(ND_ram + addr, w);
}

static void nd_ram_bank1_bput(uaecptr addr, uae_u32 b)
{
    addr &= ND_RAM_bankmask1;
    ND_ram[addr] = b;
}

static uae_u32 nd_ram_bank2_lget(uaecptr addr)
{
    addr &= ND_RAM_bankmask2;
    return do_get_mem_long(ND_ram + addr);
}

static uae_u32 nd_ram_bank2_wget(uaecptr addr)
{
    addr &= ND_RAM_bankmask2;
    return do_get_mem_word(ND_ram + addr);
}

static uae_u32 nd_ram_bank2_bget(uaecptr addr)
{
    addr &= ND_RAM_bankmask2;
    return ND_ram[addr];
}

static void nd_ram_bank2_lput(uaecptr addr, uae_u32 l)
{
    addr &= ND_RAM_bankmask2;
    do_put_mem_long(ND_ram + addr, l);
}

static void nd_ram_bank2_wput(uaecptr addr, uae_u32 w)
{
    addr &= ND_RAM_bankmask2;
    do_put_mem_word(ND_ram + addr, w);
}

static void nd_ram_bank2_bput(uaecptr addr, uae_u32 b)
{
    addr &= ND_RAM_bankmask2;
    ND_ram[addr] = b;
}

static uae_u32 nd_ram_bank3_lget(uaecptr addr)
{
    addr &= ND_RAM_bankmask3;
    return do_get_mem_long(ND_ram + addr);
}

static uae_u32 nd_ram_bank3_wget(uaecptr addr)
{
    addr &= ND_RAM_bankmask3;
    return do_get_mem_word(ND_ram + addr);
}

static uae_u32 nd_ram_bank3_bget(uaecptr addr)
{
    addr &= ND_RAM_bankmask3;
    return ND_ram[addr];
}

static void nd_ram_bank3_lput(uaecptr addr, uae_u32 l)
{
    addr &= ND_RAM_bankmask3;
    do_put_mem_long(ND_ram + addr, l);
}

static void nd_ram_bank3_wput(uaecptr addr, uae_u32 w)
{
    addr &= ND_RAM_bankmask3;
    do_put_mem_word(ND_ram + addr, w);
}

static void nd_ram_bank3_bput(uaecptr addr, uae_u32 b)
{
    addr &= ND_RAM_bankmask3;
    ND_ram[addr] = b;
}

static uae_u32 nd_ram_empty_lget(uaecptr addr)
{
    write_log("[ND] empty memory bank lget at %08X\n",addr);
    return 0;
}

static uae_u32 nd_ram_empty_wget(uaecptr addr)
{
    write_log("[ND] empty memory bank wget at %08X\n",addr);
    return 0;
}

static uae_u32 nd_ram_empty_bget(uaecptr addr)
{
    write_log("[ND] empty memory bank bget at %08X\n",addr);
    return 0;
}

static void nd_ram_empty_lput(uaecptr addr, uae_u32 l)
{
    write_log("[ND] empty memory bank lput at %08X\n",addr);
}

static void nd_ram_empty_wput(uaecptr addr, uae_u32 w)
{
    write_log("[ND] empty memory bank wput at %08X\n",addr);
}

static void nd_ram_empty_bput(uaecptr addr, uae_u32 b)
{
    write_log("[ND] empty memory bank bput at %08X\n",addr);
}

/* NeXTdimension VRAM */
static uae_u32 nd_vram_lget(uaecptr addr)
{
    addr &= ND_VRAM_MASK;
    return do_get_mem_long(ND_vram + addr);
}

static uae_u32 nd_vram_wget(uaecptr addr)
{
    addr &= ND_VRAM_MASK;
    return do_get_mem_word(ND_vram + addr);
}

static uae_u32 nd_vram_bget(uaecptr addr)
{
    addr &= ND_VRAM_MASK;
    return ND_vram[addr];
}

static void nd_vram_lput(uaecptr addr, uae_u32 l)
{
    addr &= ND_VRAM_MASK;
    do_put_mem_long(ND_vram + addr, l);
}

static void nd_vram_wput(uaecptr addr, uae_u32 w)
{
    addr &= ND_VRAM_MASK;
    do_put_mem_word(ND_vram + addr, w);
}

static void nd_vram_bput(uaecptr addr, uae_u32 b)
{
    addr &= ND_VRAM_MASK;
    ND_vram[addr] = b;
}

/* NeXTdimension ROM */
static uae_u32 nd_rom_lget(uaecptr addr)
{
    addr = nd_rom_addr_fix(addr);
    
	return (nd_rom_read(addr) << 24); /* byte lane at msb */
}

static uae_u32 nd_rom_wget(uaecptr addr)
{
    addr = nd_rom_addr_fix(addr);
    
    return (nd_rom_read(addr) << 8); /* byte lane at msb */
}

static uae_u32 nd_rom_bget(uaecptr addr)
{
    addr = nd_rom_addr_fix(addr);
    
    return nd_rom_read(addr);
}

static void nd_rom_lput(uaecptr addr, uae_u32 l)
{
    addr = nd_rom_addr_fix(addr);
    
    nd_rom_write(addr, l >> 24);
}

static void nd_rom_wput(uaecptr addr, uae_u32 w)
{
    addr = nd_rom_addr_fix(addr);
    
    nd_rom_write(addr, w >> 8);
}

static void nd_rom_bput(uaecptr addr, uae_u32 b)
{
    addr = nd_rom_addr_fix(addr);
    
    nd_rom_write(addr, b);
}

static uae_u32 nd_rom_cs8get(uaecptr addr)
{
    addr &= ND_EEPROM_MASK;
    return ND_rom[addr];
}

/* Illegal access functions */

static uae_u32 nd_illegal_lget(uaecptr addr)
{
    write_log("[ND] Illegal lget at %08X\n",addr);
    return 0;
}

static uae_u32 nd_illegal_wget(uaecptr addr)
{
    write_log("[ND] Illegal wget at %08X\n",addr);
    return 0;
}

static uae_u32 nd_illegal_bget(uaecptr addr)
{
    write_log("[ND] Illegal bget at %08X\n",addr);
    return 0;
}

static void nd_illegal_lput(uaecptr addr, uae_u32 l)
{
    write_log("[ND] Illegal lput at %08X\n",addr);
}

static void nd_illegal_wput(uaecptr addr, uae_u32 w)
{
    write_log("[ND] Illegal wput at %08X\n",addr);
}

static void nd_illegal_bput(uaecptr addr, uae_u32 b)
{
    write_log("[ND] Illegal bput at %08X\n",addr);
}

/* Unknown register access functions (memory controller step 1) */
#if ND_STEP
static uae_u32 nd_unknown_lget(uaecptr addr)
{
    write_log("[ND] Unknown lget at %08X\n",addr);
    return 0;
}

static uae_u32 nd_unknown_wget(uaecptr addr)
{
    write_log("[ND] Unknown wget at %08X\n",addr);
    return 0;
}

static uae_u32 nd_unknown_bget(uaecptr addr)
{
    write_log("[ND] Unknown bget at %08X\n",addr);
    return 0;
}

static void nd_unknown_lput(uaecptr addr, uae_u32 l)
{
    write_log("[ND] Unknown lput at %08X: %08X\n",addr,l);
}

static void nd_unknown_wput(uaecptr addr, uae_u32 w)
{
    write_log("[ND] Unknown wput at %08X: %04X\n",addr,w);
}

static void nd_unknown_bput(uaecptr addr, uae_u32 b)
{
    write_log("[ND] Unknown bput at %08X: %02X\n",addr,b);
}
#endif

/* NeXTdimension dither memory & datapath */

static uae_u32 nd_dmem_lget(uaecptr addr)
{
    addr &= ND_DP_MASK;
    return addr < ND_DMEM_SIZE ? do_get_mem_long(ND_dmem + addr) : nd_dp_lget(addr);
}

static uae_u32 nd_dmem_wget(uaecptr addr)
{
    addr &= ND_DP_MASK;
    return addr < ND_DMEM_SIZE ? do_get_mem_word(ND_dmem + addr) : nd_dp_lget(addr);
}

static uae_u32 nd_dmem_bget(uaecptr addr)
{
    addr &= ND_DP_MASK;
    return addr < ND_DMEM_SIZE ? do_get_mem_byte(ND_dmem + addr) : nd_dp_lget(addr);
}

static void nd_dmem_lput(uaecptr addr, uae_u32 l)
{
    addr &= ND_DP_MASK;
    if(addr < ND_DMEM_SIZE)
        do_put_mem_long(ND_dmem + addr, l);
    else
        nd_dp_lput(addr, l);
}

static void nd_dmem_wput(uaecptr addr, uae_u32 w)
{
    addr &= ND_DP_MASK;
    if(addr < ND_DMEM_SIZE)
        do_put_mem_word(ND_dmem + addr, w);
    else
        nd_dp_lput(addr, w);
}

static void nd_dmem_bput(uaecptr addr, uae_u32 b)
{
    addr &= ND_DP_MASK;
    if(addr < ND_DMEM_SIZE)
        do_put_mem_byte(ND_dmem + addr, b);
    else
        nd_dp_lput(addr, b);
}

static nd_addrbank nd_ram_bank0 =
{
	nd_ram_bank0_lget, nd_ram_bank0_wget, nd_ram_bank0_bget,
	nd_ram_bank0_lput, nd_ram_bank0_wput, nd_ram_bank0_bput,
	nd_ram_bank0_bget
};

static nd_addrbank nd_ram_bank1 =
{
    nd_ram_bank1_lget, nd_ram_bank1_wget, nd_ram_bank1_bget,
    nd_ram_bank1_lput, nd_ram_bank1_wput, nd_ram_bank1_bput,
    nd_ram_bank1_bget
};

static nd_addrbank nd_ram_bank2 =
{
    nd_ram_bank2_lget, nd_ram_bank2_wget, nd_ram_bank2_bget,
    nd_ram_bank2_lput, nd_ram_bank2_wput, nd_ram_bank2_bput,
    nd_ram_bank2_bget
};

static nd_addrbank nd_ram_bank3 =
{
    nd_ram_bank3_lget, nd_ram_bank3_wget, nd_ram_bank3_bget,
    nd_ram_bank3_lput, nd_ram_bank3_wput, nd_ram_bank3_bput,
    nd_ram_bank3_bget
};

static nd_addrbank nd_ram_empty =
{
    nd_ram_empty_lget, nd_ram_empty_wget, nd_ram_empty_bget,
    nd_ram_empty_lput, nd_ram_empty_wput, nd_ram_empty_bput,
    nd_ram_empty_bget
};

static nd_addrbank nd_vram_bank =
{
    nd_vram_lget, nd_vram_wget, nd_vram_bget,
    nd_vram_lput, nd_vram_wput, nd_vram_bput,
    nd_vram_bget
};

static nd_addrbank nd_rom_bank =
{
	nd_rom_lget, nd_rom_wget, nd_rom_bget,
    nd_rom_lput, nd_rom_wput, nd_rom_bput,
    nd_rom_cs8get

};

static nd_addrbank nd_dmem_bank =
{
    nd_dmem_lget, nd_dmem_wget, nd_dmem_bget,
    nd_dmem_lput, nd_dmem_wput, nd_dmem_bput,
    nd_dmem_bget
};

static nd_addrbank nd_io_bank =
{
	nd_io_lget, nd_io_wget, nd_io_bget,
	nd_io_lput, nd_io_wput, nd_io_bput,
	nd_io_bget
};

static nd_addrbank nd_ramdac_bank =
{
    nd_ramdac_lget, nd_ramdac_wget, nd_ramdac_bget,
    nd_ramdac_lput, nd_ramdac_wput, nd_ramdac_bput,
    nd_ramdac_bget
};

static nd_addrbank nd_illegal_bank =
{
    nd_illegal_lget, nd_illegal_wget, nd_illegal_bget,
    nd_illegal_lput, nd_illegal_wput, nd_illegal_bput,
    nd_illegal_bget
};

#if ND_STEP
static nd_addrbank nd_unknown_bank =
{
    nd_unknown_lget, nd_unknown_wget, nd_unknown_bget,
    nd_unknown_lput, nd_unknown_wput, nd_unknown_bput,
    nd_unknown_bget
};
#endif

static void nd_init_mem_banks (void)
{
    int i;
    for (i = 0; i < 65536; i++)
        nd_put_mem_bank (i<<16, &nd_illegal_bank);
}

void nd_memory_init(void) {
	
	write_log("[ND] Memory init: Memory size: %iMB\n",
              Configuration_CheckDimensionMemory(ConfigureParams.Dimension.nMemoryBankSize));

    /* Initialize banks with error memory */
    nd_init_mem_banks();
    
    /* Clear first 4k of memory for m68k ROM polling code */
    memset(ND_ram, 0, 4096 * sizeof(Uint8));
    
    /* Map main memory */
    if (ConfigureParams.Dimension.nMemoryBankSize[0]) {
        ND_RAM_bankmask0 = ND_RAM_BANKMASK|((ConfigureParams.Dimension.nMemoryBankSize[0]<<20)-1);
        nd_map_banks(&nd_ram_bank0, (ND_RAM_START+(0*ND_RAM_BANKSIZE))>>16, ND_RAM_BANKSIZE >> 16);
        write_log("[ND] Mapping main memory bank0 at $%08x: %iMB\n",
                  (ND_RAM_START+(0*ND_RAM_BANKSIZE)), ConfigureParams.Dimension.nMemoryBankSize[0]);
    } else {
        ND_RAM_bankmask0 = 0;
        nd_map_banks(&nd_ram_empty, (ND_RAM_START+(0*ND_RAM_BANKSIZE))>>16, ND_RAM_BANKSIZE >> 16);
        write_log("[ND] Mapping main memory bank0 at $%08x: empty\n", (ND_RAM_START+(0*ND_RAM_BANKSIZE)));
    }
    if (ConfigureParams.Dimension.nMemoryBankSize[1]) {
        ND_RAM_bankmask1 = ND_RAM_BANKMASK|((ConfigureParams.Dimension.nMemoryBankSize[1]<<20)-1);
        nd_map_banks(&nd_ram_bank1, (ND_RAM_START+(1*ND_RAM_BANKSIZE))>>16, ND_RAM_BANKSIZE >> 16);
        write_log("[ND] Mapping main memory bank1 at $%08x: %iMB\n",
                  (ND_RAM_START+(1*ND_RAM_BANKSIZE)), ConfigureParams.Dimension.nMemoryBankSize[1]);
    } else {
        ND_RAM_bankmask1 = 0;
        nd_map_banks(&nd_ram_empty, (ND_RAM_START+(1*ND_RAM_BANKSIZE))>>16, ND_RAM_BANKSIZE >> 16);
        write_log("[ND] Mapping main memory bank1 at $%08x: empty\n", (ND_RAM_START+(1*ND_RAM_BANKSIZE)));
    }
    if (ConfigureParams.Dimension.nMemoryBankSize[2]) {
        ND_RAM_bankmask2 = ND_RAM_BANKMASK|((ConfigureParams.Dimension.nMemoryBankSize[2]<<20)-1);
        nd_map_banks(&nd_ram_bank2, (ND_RAM_START+(2*ND_RAM_BANKSIZE))>>16, ND_RAM_BANKSIZE >> 16);
        write_log("[ND] Mapping main memory bank2 at $%08x: %iMB\n",
                  (ND_RAM_START+(2*ND_RAM_BANKSIZE)), ConfigureParams.Dimension.nMemoryBankSize[2]);
    } else {
        ND_RAM_bankmask2 = 0;
        nd_map_banks(&nd_ram_empty, (ND_RAM_START+(2*ND_RAM_BANKSIZE))>>16, ND_RAM_BANKSIZE >> 16);
        write_log("[ND] Mapping main memory bank2 at $%08x: empty\n", (ND_RAM_START+(2*ND_RAM_BANKSIZE)));
    }
    if (ConfigureParams.Dimension.nMemoryBankSize[3]) {
        ND_RAM_bankmask3 = ND_RAM_BANKMASK|((ConfigureParams.Dimension.nMemoryBankSize[3]<<20)-1);
        nd_map_banks(&nd_ram_bank3, (ND_RAM_START+(3*ND_RAM_BANKSIZE))>>16, ND_RAM_BANKSIZE >> 16);
        write_log("[ND] Mapping main memory bank3 at $%08x: %iMB\n",
                  (ND_RAM_START+(3*ND_RAM_BANKSIZE)), ConfigureParams.Dimension.nMemoryBankSize[3]);
    } else {
        ND_RAM_bankmask3 = 0;
        nd_map_banks(&nd_ram_empty, (ND_RAM_START+(3*ND_RAM_BANKSIZE))>>16, ND_RAM_BANKSIZE >> 16);
        write_log("[ND] Mapping main memory bank3 at $%08x: empty\n", (ND_RAM_START+(3*ND_RAM_BANKSIZE)));
    }
    
    write_log("[ND] Mapping video memory at $%08x: %iMB\n", ND_VRAM_START, ND_VRAM_SIZE/(1024*1024));
    nd_map_banks(&nd_vram_bank, ND_VRAM_START>>16, (4*ND_VRAM_SIZE)>>16);
    
	write_log("[ND] Mapping ROM at $%08x: %ikB\n", ND_EEPROM_START, ND_EEPROM_SIZE/1024);
	nd_map_banks(&nd_rom_bank, ND_EEPROM_START>>16, (ND_EEPROM_SIZE*8)>>16);
    nd_rom_load();
	
    write_log("[ND] Mapping dither memory and data path at $%08x: %ibyte\n", ND_DMEM_START, ND_DMEM_SIZE);
    nd_map_banks(&nd_dmem_bank, ND_DMEM_START>>16, 1);

	write_log("[ND] Mapping IO memory at $%08x\n", ND_IO_START);
	nd_map_banks(&nd_io_bank, ND_IO_START>>16, 1);
    
    write_log("[ND] Mapping RAMDAC registers at $%08x\n", ND_RAMDAC_START);
    nd_map_banks(&nd_ramdac_bank, ND_RAMDAC_START>>16, 1);
#if ND_STEP
    write_log("[ND] Mapping Unknown register at $%08x\n", ND_UNKNWN_START);
    nd_map_banks(&nd_unknown_bank, ND_UNKNWN_START>>16, 1);
#endif
}
