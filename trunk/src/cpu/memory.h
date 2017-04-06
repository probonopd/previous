 /*
  * UAE - The Un*x Amiga Emulator
  *
  * memory management
  *
  * Copyright 1995 Bernd Schmidt
  *
  * Adaptation to Hatari by Thomas Huth
  *
  * This file is distributed under the GNU Public License, version 2 or at
  * your option any later version. Read the file gpl.txt for details.
  */

#ifndef UAE_MEMORY_H
#define UAE_MEMORY_H

#include "maccess.h"

#ifdef JIT
extern int special_mem;
#define S_READ 1
#define S_WRITE 2

uae_u8 *cache_alloc (int);
void cache_free (uae_u8*);
#endif

#define call_mem_get_func(func, addr) ((*func)(addr))
#define call_mem_put_func(func, addr, v) ((*func)(addr, v))

extern uae_u8 NEXTVideo[256*1024];

extern uae_u8 NEXTColorVideo[2*1024*1024];


/* Enabling this adds one additional native memory reference per 68k memory
 * access, but saves one shift (on the x86). Enabling this is probably
 * better for the cache. My favourite benchmark (PP2) doesn't show a
 * difference, so I leave this enabled. */
#if 1 || defined SAVE_MEMORY
#define SAVE_MEMORY_BANKS
#endif

void memory_hardreset (void);

typedef uae_u32 (*mem_get_func)(uaecptr) REGPARAM;
typedef void (*mem_put_func)(uaecptr, uae_u32) REGPARAM;
typedef uae_u8 *(*xlate_func)(uaecptr) REGPARAM;
typedef int (*check_func)(uaecptr, uae_u32) REGPARAM;

extern char *address_space, *good_address_map;

uae_u32 wait_cpu_cycle_read (uaecptr addr, int mode);
void wait_cpu_cycle_write (uaecptr addr, int mode, uae_u32 v);
uae_u32 wait_cpu_cycle_read_ce020 (uaecptr addr, int mode);
void wait_cpu_cycle_write_ce020 (uaecptr addr, int mode, uae_u32 v);

enum { ABFLAG_UNK = 0, ABFLAG_RAM = 1, ABFLAG_ROM = 2, ABFLAG_ROMIN = 4, ABFLAG_IO = 8, ABFLAG_NONE = 16, ABFLAG_SAFE = 32 };
typedef struct {
	/* These ones should be self-explanatory... */
	mem_get_func lget, wget, bget;
	mem_put_func lput, wput, bput;
	mem_get_func lgeti, wgeti;
	int flags;
} addrbank;

extern uae_u8 ce_cachable[65536];

#define bankindex(addr) (((uaecptr)(addr)) >> 16)

#ifdef SAVE_MEMORY_BANKS
extern addrbank *mem_banks[65536];
#define get_mem_bank(addr) (*mem_banks[bankindex(addr)])
#define put_mem_bank(addr, b) (mem_banks[bankindex(addr)] = (b))
#else
extern addrbank mem_banks[65536];
#define get_mem_bank(addr) (mem_banks[bankindex(addr)])
#define put_mem_bank(addr, b) (mem_banks[bankindex(addr)] = *(b))
#endif

const char* memory_init(int *membanks);
void memory_uninit (void);
void map_banks(addrbank *bank, int first, int count);

#ifndef NO_INLINE_MEMORY_ACCESS

#define longget(addr) (call_mem_get_func(get_mem_bank(addr).lget, addr))
#define wordget(addr) (call_mem_get_func(get_mem_bank(addr).wget, addr))
#define byteget(addr) (call_mem_get_func(get_mem_bank(addr).bget, addr))
#define longput(addr,l) (call_mem_put_func(get_mem_bank(addr).lput, addr, l))
#define wordput(addr,w) (call_mem_put_func(get_mem_bank(addr).wput, addr, w))
#define byteput(addr,b) (call_mem_put_func(get_mem_bank(addr).bput, addr, b))

#else

uae_u32 alongget(uaecptr addr);
uae_u32 awordget(uaecptr addr);
uae_u32 longget(uaecptr addr);
uae_u32 wordget(uaecptr addr);
uae_u32 byteget(uaecptr addr);
void longput(uaecptr addr, uae_u32 l);
void wordput(uaecptr addr, uae_u32 w);
void byteput(uaecptr addr, uae_u32 b);

#endif

#define longget(addr) (call_mem_get_func(get_mem_bank(addr).lget, addr))
#define wordget(addr) (call_mem_get_func(get_mem_bank(addr).wget, addr))
#define byteget(addr) (call_mem_get_func(get_mem_bank(addr).bget, addr))
#define longgeti(addr) (call_mem_get_func(get_mem_bank(addr).lgeti, addr))
#define wordgeti(addr) (call_mem_get_func(get_mem_bank(addr).wgeti, addr))
#define longput(addr,l) (call_mem_put_func(get_mem_bank(addr).lput, addr, l))
#define wordput(addr,w) (call_mem_put_func(get_mem_bank(addr).wput, addr, w))
#define byteput(addr,b) (call_mem_put_func(get_mem_bank(addr).bput, addr, b))

static inline uae_u32 get_long(uaecptr addr)
{
    return longget(addr);
}

static inline uae_u32 get_word(uaecptr addr)
{
    return wordget(addr);
}

static inline uae_u32 get_byte(uaecptr addr)
{
    return byteget(addr);
}

static inline void put_long(uaecptr addr, uae_u32 l)
{
    longput(addr, l);
}

static inline void put_word(uaecptr addr, uae_u32 w)
{
    wordput(addr, w);
}

static inline void put_byte(uaecptr addr, uae_u32 b)
{
    byteput(addr, b);
}

static inline uae_u32 get_longi(uaecptr addr)
{
	return longgeti (addr);
}

static inline uae_u32 get_wordi(uaecptr addr)
{
	return wordgeti (addr);
}

#endif /* UAE_MEMORY_H */
