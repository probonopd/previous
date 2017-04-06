#pragma once

#ifndef __DIMENSION_H__
#define __DIMENSION_H__

#define ND_SLOT 2

/* NeXTdimension memory controller revision (0 and 1 allowed) */
#define ND_STEP 1

Uint32 nd_slot_lget(Uint32 addr);
Uint16 nd_slot_wget(Uint32 addr);
Uint8  nd_slot_bget(Uint32 addr);
void   nd_slot_lput(Uint32 addr, Uint32 l);
void   nd_slot_wput(Uint32 addr, Uint16 w);
void   nd_slot_bput(Uint32 addr, Uint8 b);

Uint32 nd_board_lget(Uint32 addr);
Uint16 nd_board_wget(Uint32 addr);
Uint8  nd_board_bget(Uint32 addr);
void   nd_board_lput(Uint32 addr, Uint32 l);
void   nd_board_wput(Uint32 addr, Uint16 w);
void   nd_board_bput(Uint32 addr, Uint8 b);
Uint8  nd_board_cs8get(Uint32 addr);
    
void   nd_board_rd8_le  (Uint32 addr, Uint32* val);
void   nd_board_rd16_le (Uint32 addr, Uint32* val);
void   nd_board_rd32_le (Uint32 addr, Uint32* val);
void   nd_board_rd64_le (Uint32 addr, Uint32* val);
void   nd_board_rd128_le(Uint32 addr, Uint32* val);

void   nd_board_rd8_be  (Uint32 addr, Uint32* val);
void   nd_board_rd16_be (Uint32 addr, Uint32* val);
void   nd_board_rd32_be (Uint32 addr, Uint32* val);
void   nd_board_rd64_be (Uint32 addr, Uint32* val);
void   nd_board_rd128_be(Uint32 addr, Uint32* val);

void   nd_board_wr8_le  (Uint32 addr, const Uint32* val);
void   nd_board_wr16_le (Uint32 addr, const Uint32* val);
void   nd_board_wr32_le (Uint32 addr, const Uint32* val);
void   nd_board_wr64_le (Uint32 addr, const Uint32* val);
void   nd_board_wr128_le(Uint32 addr, const Uint32* val);
    
void   nd_board_wr8_be  (Uint32 addr, const Uint32* val);
void   nd_board_wr16_be (Uint32 addr, const Uint32* val);
void   nd_board_wr32_be (Uint32 addr, const Uint32* val);
void   nd_board_wr64_be (Uint32 addr, const Uint32* val);
void   nd_board_wr128_be(Uint32 addr, const Uint32* val);

extern Uint8  ND_ram[64*1024*1024];
extern Uint8  ND_rom[128*1024];
extern Uint8  ND_vram[4*1024*1024];

typedef void (*i860_run_func)(int);
extern i860_run_func i860_Run;

void dimension_init(void);
void dimension_uninit(void);
void nd_i860_init(void);
void nd_i860_uninit(void);
void i860_reset(void);
void i860_interrupt(void);
void nd_start_debugger(void);
const char* nd_reports(double realTime, double hostTime);

#define ND_LOG_IO_RD LOG_NONE
#define ND_LOG_IO_WR LOG_NONE

#endif /* __DIMENSION_H__ */
