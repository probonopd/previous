#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "dimension.h"
#include "sysdeps.h"
#include "nd_mem.h"
#include "nd_devs.h"
#include "nd_nbic.h"
#include "nd_sdl.h"

Uint8  ND_ram[64*1024*1024];
Uint8  ND_rom[128*1024];
Uint8  ND_vram[4*1024*1024];

/* NeXTdimension board and slot memory */
#define ND_BOARD_SIZE	0x10000000
#define ND_BOARD_MASK	0x0FFFFFFF
#define ND_BOARD_BITS   0xF0000000

#define ND_SLOT_SIZE	0x01000000
#define ND_SLOT_MASK	0x00FFFFFF
#define ND_SLOT_BITS    0x0F000000

#define ND_NBIC_SPACE   0xFFFFFFE8

/* NeXTdimension board memory access (i860) */

void   nd_board_rd8_be(Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    *((Uint8*)val) = nd_byteget(addr);
}

void   nd_board_rd16_be(Uint32 addr, Uint32* val) {
    addr |= ND_BOARD_BITS;
    *((Uint16*)val) = nd_wordget(addr);
}

void   nd_board_rd32_be(Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    val[0] = nd_longget(addr);
}

void   nd_board_rd64_be(Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    val[0] = nd_longget(addr+4);
    val[1] = nd_longget(addr+0);
}

void   nd_board_rd128_be(Uint32 addr, Uint32* val) {
    addr   |= ND_BOARD_BITS;
    val[0]  = nd_longget(addr+4);
    val[1]  = nd_longget(addr+0);
    val[2]  = nd_longget(addr+12);
    val[3]  = nd_longget(addr+8);
}

void   nd_board_wr8_be(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_byteput(addr, *((const Uint8*)val));
}

void   nd_board_wr16_be(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_wordput(addr, *((const Uint16*)val));
}

void   nd_board_wr32_be(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr, val[0]);
}

void   nd_board_wr64_be(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr+4, val[0]);
    nd_longput(addr+0, val[1]);
}

void   nd_board_wr128_be(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr+4,  val[0]);
    nd_longput(addr+0,  val[1]);
    nd_longput(addr+12, val[2]);
    nd_longput(addr+8,  val[3]);
}

void   nd_board_rd8_le(Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    *((Uint8*)val) = nd_byteget(addr^7);
}

void   nd_board_rd16_le(Uint32 addr, Uint32* val) {
    addr |= ND_BOARD_BITS;
    *((Uint16*)val) = nd_wordget(addr^6);
}

void   nd_board_rd32_le(Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    val[0] = nd_longget(addr^4);
}

void   nd_board_rd64_le(Uint32 addr, Uint32* val) {
    addr  |= ND_BOARD_BITS;
    val[0] = nd_longget(addr+0);
    val[1] = nd_longget(addr+4);
}

void   nd_board_rd128_le(Uint32 addr, Uint32* val) {
    addr   |= ND_BOARD_BITS;
    val[0]  = nd_longget(addr+0);
    val[1]  = nd_longget(addr+4);
    val[2]  = nd_longget(addr+8);
    val[3]  = nd_longget(addr+12);
}

void   nd_board_wr8_le(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_byteput(addr^7, *((const Uint8*)val));
}

void   nd_board_wr16_le(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_wordput(addr^6, *((const Uint16*)val));
}

void   nd_board_wr32_le(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr^4, val[0]);
}

void   nd_board_wr64_le(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr+0, val[0]);
    nd_longput(addr+4, val[1]);
}

void   nd_board_wr128_le(Uint32 addr, const Uint32* val) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr+0,  val[0]);
    nd_longput(addr+4,  val[1]);
    nd_longput(addr+8,  val[2]);
    nd_longput(addr+12, val[3]);
}

/* NeXTdimension board memory access (m68k) */

inline Uint32 nd_board_lget(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    Uint32 result = nd_longget(addr);
    // (SC) delay m68k read on csr0 while in ROM (CS8=1)to give ND some time to start up.
    if(addr == 0xFF800000 && (result & 0x02))
        M68000_AddCycles(ConfigureParams.System.nCpuFreq * 100);
    return result;
}

inline Uint16 nd_board_wget(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    return nd_wordget(addr);
}

inline Uint8 nd_board_bget(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    return nd_byteget(addr);
}

inline void nd_board_lput(Uint32 addr, Uint32 l) {
    addr |= ND_BOARD_BITS;
    nd_longput(addr, l);
}

inline void nd_board_wput(Uint32 addr, Uint16 w) {
    addr |= ND_BOARD_BITS;
    nd_wordput(addr, w);
}

inline void nd_board_bput(Uint32 addr, Uint8 b) {
    addr |= ND_BOARD_BITS;
    nd_byteput(addr, b);
}

inline Uint8 nd_board_cs8get(Uint32 addr) {
    addr |= ND_BOARD_BITS;
    return nd_cs8get(addr);
}

/* NeXTdimension slot memory access */
Uint32 nd_slot_lget(Uint32 addr) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        return nd_longget(addr);
    } else {
        return nd_nbic_lget(addr);
    }
}

Uint16 nd_slot_wget(Uint32 addr) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        return nd_wordget(addr);
    } else {
        return nd_nbic_wget(addr);
    }
}

Uint8 nd_slot_bget(Uint32 addr) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        return nd_byteget(addr);
    } else {
        return nd_nbic_bget(addr);
    }
}

void nd_slot_lput(Uint32 addr, Uint32 l) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        nd_longput(addr, l);
    } else {
        nd_nbic_lput(addr, l);
    }
}

void nd_slot_wput(Uint32 addr, Uint16 w) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        nd_wordput(addr, w);
    } else {
        nd_nbic_wput(addr, w);
    }
}

void nd_slot_bput(Uint32 addr, Uint8 b) {
    addr |= ND_SLOT_BITS;
    
    if (addr<ND_NBIC_SPACE) {
        nd_byteput(addr, b);
    } else {
        nd_nbic_bput(addr, b);
    }
}

/* Reset function */

void dimension_init(void) {
    nd_i860_uninit();
    nd_nbic_init();
    nd_devs_init();
    nd_memory_init();
    nd_i860_init();
    nd_sdl_init();
}

void dimension_uninit(void) {
	nd_i860_uninit();
    nd_sdl_uninit();
}
