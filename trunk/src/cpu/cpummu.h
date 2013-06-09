/*
 * cpummu.h - MMU emulation
 *
 * Copyright (c) 2001-2004 Milan Jurik of ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by UAE MMU patch
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * ARAnyM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ARAnyM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ARAnyM; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CPUMMU_H
#define CPUMMU_H

#include "compat.h"
#include "mmu_common.h"

#ifndef FULLMMU
#define FULLMMU
#endif

#define DUNUSED(x)
#define D
#if DEBUG
#define bug write_log
#else
#define bug
#endif

static __inline void flush_internals (void) { }

//typedef uae_u8 flagtype;

//static m68k_exception except;

struct xttrx {
    uae_u32 log_addr_base : 8;
    uae_u32 log_addr_mask : 8;
    uae_u32 enable : 1;
    uae_u32 s_field : 2;
    uae_u32 : 3;
    uae_u32 usr1 : 1;
    uae_u32 usr0 : 1;
    uae_u32 : 1;
    uae_u32 cmode : 2;
    uae_u32 : 2;
    uae_u32 write : 1;
    uae_u32 : 2;
};

struct mmusr_t {
   uae_u32 phys_addr : 20;
   uae_u32 bus_err : 1;
   uae_u32 global : 1;
   uae_u32 usr1 : 1;
   uae_u32 usr0 : 1;
   uae_u32 super : 1;
   uae_u32 cmode : 2;
   uae_u32 modif : 1;
   uae_u32 : 1;
   uae_u32 write : 1;
   uae_u32 ttrhit : 1;
   uae_u32 resident : 1;
};

struct log_addr4 {
   uae_u32 rif : 7;
   uae_u32 pif : 7;
   uae_u32 paif : 6;
   uae_u32 poff : 12;
};

struct log_addr8 {
  uae_u32 rif : 7;
  uae_u32 pif : 7;
  uae_u32 paif : 5;
  uae_u32 poff : 13;
};

#define MMU_TEST_PTEST					1
#define MMU_TEST_VERBOSE				2
#define MMU_TEST_FORCE_TABLE_SEARCH		4
#define MMU_TEST_NO_BUSERR				8

extern void mmu_dump_tables(void);

#define MMU_TTR_LOGICAL_BASE				0xff000000
#define MMU_TTR_LOGICAL_MASK				0x00ff0000
#define MMU_TTR_BIT_ENABLED					(1 << 15)
#define MMU_TTR_BIT_SFIELD_ENABLED			(1 << 14)
#define MMU_TTR_BIT_SFIELD_SUPER			(1 << 13)
#define MMU_TTR_SFIELD_SHIFT				13
#define MMU_TTR_UX_MASK						((1 << 9) | (1 << 8))
#define MMU_TTR_UX_SHIFT					8
#define MMU_TTR_CACHE_MASK					((1 << 6) | (1 << 5))
#define MMU_TTR_CACHE_SHIFT					5
#define MMU_TTR_BIT_WRITE_PROTECT			(1 << 2)

#define MMU_UDT_MASK	3
#define MMU_PDT_MASK	3

#define MMU_DES_WP			4
#define MMU_DES_USED		8

/* page descriptors only */
#define MMU_DES_MODIFIED	16
#define MMU_DES_SUPER		(1 << 7)
#define MMU_DES_GLOBAL		(1 << 10)

#define MMU_ROOT_PTR_ADDR_MASK			0xfffffe00
#define MMU_PTR_PAGE_ADDR_MASK_8		0xffffff80
#define MMU_PTR_PAGE_ADDR_MASK_4		0xffffff00

#define MMU_PAGE_INDIRECT_MASK			0xfffffffc
#define MMU_PAGE_ADDR_MASK_8			0xffffe000
#define MMU_PAGE_ADDR_MASK_4			0xfffff000
#define MMU_PAGE_UR_MASK_8				((1 << 12) | (1 << 11))
#define MMU_PAGE_UR_MASK_4				(1 << 11)
#define MMU_PAGE_UR_SHIFT				11

#define MMU_MMUSR_ADDR_MASK				0xfffff000
#define MMU_MMUSR_B						(1 << 11)
#define MMU_MMUSR_G						(1 << 10)
#define MMU_MMUSR_U1					(1 << 9)
#define MMU_MMUSR_U0					(1 << 8)
#define MMU_MMUSR_Ux					(MMU_MMUSR_U1 | MMU_MMUSR_U0)
#define MMU_MMUSR_S						(1 << 7)
#define MMU_MMUSR_CM					((1 << 6) | ( 1 << 5))
#define MMU_MMUSR_M						(1 << 4)
#define MMU_MMUSR_W						(1 << 2)
#define MMU_MMUSR_T						(1 << 1)
#define MMU_MMUSR_R						(1 << 0)

#define TTR_I0	4
#define TTR_I1	5
#define TTR_D0	6
#define TTR_D1	7

#define TTR_NO_MATCH	0
#define TTR_NO_WRITE	1
#define TTR_OK_MATCH	2

struct mmu_atc_line {
	uaecptr tag; // tag is 16 or 17 bits S+logical
	unsigned valid : 1;
	unsigned global : 1;
	unsigned modified : 1;
	unsigned write_protect : 1;
	uaecptr phys; // phys base address
};

/*
 * 68040 ATC is a 4 way 16 slot associative address translation cache
 * the 68040 has a DATA and an INSTRUCTION ATC.
 * an ATC lookup may result in : a hit, a miss and a modified state.
 * the 68060 can disable ATC allocation
 * we must take care of 8k and 4k page size, index position is relative to page size
 */

#define ATC_WAYS 4
#define ATC_SLOTS 16
#define ATC_TYPE 2

extern bool mmu_is_super;
extern struct mmu_atc_line mmu_atc_array[ATC_TYPE][ATC_WAYS][ATC_SLOTS];

/*
 * mmu access is a 4 step process:
 * if mmu is not enabled just read physical
 * check transparent region, if transparent, read physical
 * check ATC (address translation cache), read immediatly if HIT
 * read from mmu with the long path (and allocate ATC entry if needed)
 */
static ALWAYS_INLINE bool mmu_lookup(uaecptr addr, bool data, bool write,
									  struct mmu_atc_line **cl)
{
	int way,index;
	static int way_miss=0;

	addr = ((mmu_is_super?0x80000000:0x00000000)|(addr >> 1)) & (regs.mmu_pagesize_8k?0xFFFE0000:0xFFFF0000);
	if (regs.mmu_pagesize_8k)
		index=(addr & 0x0001E000)>>13;
	else
		index=(addr & 0x0000F000)>>12;
	for (way=0;way<ATC_WAYS;way++) {
		// if we have this 
		if ((addr == mmu_atc_array[data][way][index].tag) && (mmu_atc_array[data][way][index].valid)) {
			*cl=&mmu_atc_array[data][way][index];
			// if first write to this take slow path (but modify this slot)
			if ((!mmu_atc_array[data][way][index].modified & write) || (mmu_atc_array[data][way][index].write_protect & write))
				return false; 
			return true;
		}
	}
	// we select a random way to void
	*cl=&mmu_atc_array[data][way_miss%ATC_WAYS][index];
	way_miss++;
	return false;
}

/*
 */
static ALWAYS_INLINE bool mmu_user_lookup(uaecptr addr, bool super, bool data,
										   bool write, struct mmu_atc_line **cl)
{
	int way,index;
	static int way_miss=0;

	addr = ((super?0x80000000:0x00000000)|(addr >> 1)) & (regs.mmu_pagesize_8k?0xFFFE0000:0xFFFF0000);
	if (regs.mmu_pagesize_8k)
		index=(addr & 0x0001E000)>>13;
	else
		index=(addr & 0x0000F000)>>12;
	for (way=0;way<ATC_WAYS;way++) {
		// if we have this 
		if ((addr == mmu_atc_array[data][way][index].tag) && (mmu_atc_array[data][way][index].valid)) {
			*cl=&mmu_atc_array[data][way][index];
			// if first write to this take slow path (but modify this slot)
			if ((!mmu_atc_array[data][way][index].modified & write) || (mmu_atc_array[data][way][index].write_protect & write))
				return false; 
			return true;
		}
	}
	// we select a random way to void
	*cl=&mmu_atc_array[data][way_miss%ATC_WAYS][index];
	way_miss++;
	return false;
}

/* check if an address matches a ttr */
static inline int mmu_do_match_ttr(uae_u32 ttr, uaecptr addr, bool super)
{
	if (ttr & MMU_TTR_BIT_ENABLED)	{	/* TTR enabled */
		uae_u8 msb, mask;

		msb = ((addr ^ ttr) & MMU_TTR_LOGICAL_BASE) >> 24;
		mask = (ttr & MMU_TTR_LOGICAL_MASK) >> 16;

		if (!(msb & ~mask)) {

			if ((ttr & MMU_TTR_BIT_SFIELD_ENABLED) == 0) {
				if (((ttr & MMU_TTR_BIT_SFIELD_SUPER) == 0) != (super == 0)) {
					return TTR_NO_MATCH;
				}
			}

			return (ttr & MMU_TTR_BIT_WRITE_PROTECT) ? TTR_NO_WRITE : TTR_OK_MATCH;
		}
	}
	return TTR_NO_MATCH;
}

static inline int mmu_match_ttr(uaecptr addr, bool super, bool data)
{
	int res;

	if (data) {
		res = mmu_do_match_ttr(regs.dtt0, addr, super);
		if (res == TTR_NO_MATCH)
			res = mmu_do_match_ttr(regs.dtt1, addr, super);
	} else {
		res = mmu_do_match_ttr(regs.itt0, addr, super);
		if (res == TTR_NO_MATCH)
			res = mmu_do_match_ttr(regs.itt1, addr, super);
	}
	return res;
}

extern uae_u16 REGPARAM3 mmu_get_word_unaligned(uaecptr addr, bool data) REGPARAM;
extern uae_u32 REGPARAM3 mmu_get_long_unaligned(uaecptr addr, bool data) REGPARAM;

extern uae_u8 REGPARAM3 mmu_get_byte_slow(uaecptr addr, bool super, bool data,
										  int size, struct mmu_atc_line *cl) REGPARAM;
extern uae_u16 REGPARAM3 mmu_get_word_slow(uaecptr addr, bool super, bool data,
										   int size, struct mmu_atc_line *cl) REGPARAM;
extern uae_u32 REGPARAM3 mmu_get_long_slow(uaecptr addr, bool super, bool data,
										   int size, struct mmu_atc_line *cl) REGPARAM;

extern void REGPARAM3 mmu_put_word_unaligned(uaecptr addr, uae_u16 val, bool data) REGPARAM;
extern void REGPARAM3 mmu_put_long_unaligned(uaecptr addr, uae_u32 val, bool data) REGPARAM;

extern void REGPARAM3 mmu_put_byte_slow(uaecptr addr, uae_u8 val, bool super, bool data,
										int size, struct mmu_atc_line *cl) REGPARAM;
extern void REGPARAM3 mmu_put_word_slow(uaecptr addr, uae_u16 val, bool super, bool data,
										int size, struct mmu_atc_line *cl) REGPARAM;
extern void REGPARAM3 mmu_put_long_slow(uaecptr addr, uae_u32 val, bool super, bool data,
										int size, struct mmu_atc_line *cl) REGPARAM;

extern void mmu_make_transparent_region(uaecptr baseaddr, uae_u32 size, int datamode);

#define FC_DATA		(regs.s ? 5 : 1)
#define FC_INST		(regs.s ? 6 : 2)

extern uaecptr REGPARAM3 mmu_translate(uaecptr addr, bool super, bool data, bool write) REGPARAM;

extern uae_u32 REGPARAM3 sfc_get_long(uaecptr addr) REGPARAM;
extern uae_u16 REGPARAM3 sfc_get_word(uaecptr addr) REGPARAM;
extern uae_u8 REGPARAM3 sfc_get_byte(uaecptr addr) REGPARAM;
extern void REGPARAM3 dfc_put_long(uaecptr addr, uae_u32 val) REGPARAM;
extern void REGPARAM3 dfc_put_word(uaecptr addr, uae_u16 val) REGPARAM;
extern void REGPARAM3 dfc_put_byte(uaecptr addr, uae_u8 val) REGPARAM;


extern void REGPARAM3 mmu_flush_atc(uaecptr addr, bool super, bool global) REGPARAM;
extern void REGPARAM3 mmu_flush_atc_all(bool global) REGPARAM;
extern void REGPARAM3 mmu_op_real(uae_u32 opcode, uae_u16 extra) REGPARAM;

extern void REGPARAM3 mmu_reset(void) REGPARAM;
extern void REGPARAM3 mmu_set_tc(uae_u16 tc) REGPARAM;
extern void REGPARAM3 mmu_set_super(bool super) REGPARAM;

static ALWAYS_INLINE uaecptr mmu_get_real_address(uaecptr addr, struct mmu_atc_line *cl)
{
    return cl->phys | (addr & (regs.mmu_pagesize_8k?0x00001fff:0x00000fff));
}

static ALWAYS_INLINE uae_u32 mmu_get_long(uaecptr addr, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                       addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,regs.s != 0,data)!=TTR_NO_MATCH))
		return phys_get_long(addr);
	if (likely(mmu_lookup(addr, data, false, &cl)))
		return phys_get_long(mmu_get_real_address(addr, cl));
	return mmu_get_long_slow(addr, regs.s != 0, data, size, cl);
}

static ALWAYS_INLINE uae_u16 mmu_get_word(uaecptr addr, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                       addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,regs.s != 0,data)!=TTR_NO_MATCH))
		return phys_get_word(addr);
	if (likely(mmu_lookup(addr, data, false, &cl)))
		return phys_get_word(mmu_get_real_address(addr, cl));
	return mmu_get_word_slow(addr, regs.s != 0, data, size, cl);
}

static ALWAYS_INLINE uae_u8 mmu_get_byte(uaecptr addr, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                       addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,regs.s != 0,data)!=TTR_NO_MATCH))
		return phys_get_byte(addr);
	if (likely(mmu_lookup(addr, data, false, &cl)))
		return phys_get_byte(mmu_get_real_address(addr, cl));
	return mmu_get_byte_slow(addr, regs.s != 0, data, size, cl);
}


static ALWAYS_INLINE void mmu_put_long(uaecptr addr, uae_u32 val, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                        addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,regs.s != 0,data)==TTR_OK_MATCH)) {
		phys_put_long(addr,val);
		return;
	}
	if (likely(mmu_lookup(addr, data, true, &cl)))
		phys_put_long(mmu_get_real_address(addr, cl), val);
	else
		mmu_put_long_slow(addr, val, regs.s != 0, data, size, cl);
}

static ALWAYS_INLINE void mmu_put_word(uaecptr addr, uae_u16 val, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                        addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,regs.s != 0,data)==TTR_OK_MATCH)) {
		phys_put_word(addr,val);
		return;
	}
	if (likely(mmu_lookup(addr, data, true, &cl)))
		phys_put_word(mmu_get_real_address(addr, cl), val);
	else
		mmu_put_word_slow(addr, val, regs.s != 0, data, size, cl);
}

static ALWAYS_INLINE void mmu_put_byte(uaecptr addr, uae_u8 val, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                        addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,regs.s != 0,data)==TTR_OK_MATCH)) {
		phys_put_byte(addr,val);
		return;
	}
	if (likely(mmu_lookup(addr, data, true, &cl)))
		phys_put_byte(mmu_get_real_address(addr, cl), val);
	else
		mmu_put_byte_slow(addr, val, regs.s != 0, data, size, cl);
}

static ALWAYS_INLINE uae_u32 mmu_get_user_long(uaecptr addr, bool super, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                       addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,super,data)!=TTR_NO_MATCH))
		return phys_get_long(addr);
	if (likely(mmu_user_lookup(addr, super, data, false, &cl)))
		return phys_get_long(mmu_get_real_address(addr, cl));
	return mmu_get_long_slow(addr, super, data, size, cl);
}

static ALWAYS_INLINE uae_u16 mmu_get_user_word(uaecptr addr, bool super, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                       addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,super,data)!=TTR_NO_MATCH))
		return phys_get_word(addr);
	if (likely(mmu_user_lookup(addr, super, data, false, &cl)))
		return phys_get_word(mmu_get_real_address(addr, cl));
	return mmu_get_word_slow(addr, super, data, size, cl);
}

static ALWAYS_INLINE uae_u8 mmu_get_user_byte(uaecptr addr, bool super, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                       addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,super,data)!=TTR_NO_MATCH))
		return phys_get_byte(addr);
	if (likely(mmu_user_lookup(addr, super, data, false, &cl)))
		return phys_get_byte(mmu_get_real_address(addr, cl));
	return mmu_get_byte_slow(addr, super, data, size, cl);
}

static ALWAYS_INLINE void mmu_put_user_long(uaecptr addr, uae_u32 val, bool super, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                        addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,super,data)==TTR_OK_MATCH)) {
		phys_put_long(addr,val);
		return;
	}
	if (likely(mmu_user_lookup(addr, super, data, true, &cl)))
		phys_put_long(mmu_get_real_address(addr, cl), val);
	else
		mmu_put_long_slow(addr, val, super, data, size, cl);
}

static ALWAYS_INLINE void mmu_put_user_word(uaecptr addr, uae_u16 val, bool super, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                        addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,super,data)==TTR_OK_MATCH)) {
		phys_put_word(addr,val);
		return;
	}
	if (likely(mmu_user_lookup(addr, super, data, true, &cl)))
		phys_put_word(mmu_get_real_address(addr, cl), val);
	else
		mmu_put_word_slow(addr, val, super, data, size, cl);
}

static ALWAYS_INLINE void mmu_put_user_byte(uaecptr addr, uae_u8 val, bool super, bool data, int size)
{
	struct mmu_atc_line *cl;

	//                                        addr,super,data
	if ((!regs.mmu_enabled) || (mmu_match_ttr(addr,super,data)==TTR_OK_MATCH)) {
		phys_put_byte(addr,val);
		return;
	}
	if (likely(mmu_user_lookup(addr, super, data, true, &cl)))
		phys_put_byte(mmu_get_real_address(addr, cl), val);
	else
		mmu_put_byte_slow(addr, val, super, data, size, cl);
}


static ALWAYS_INLINE void HWput_l(uaecptr addr, uae_u32 l)
{
    put_long (addr, l);
}
static ALWAYS_INLINE void HWput_w(uaecptr addr, uae_u32 w)
{
    put_word (addr, w);
}
static ALWAYS_INLINE void HWput_b(uaecptr addr, uae_u32 b)
{
    put_byte (addr, b);
}
static ALWAYS_INLINE uae_u32 HWget_l(uaecptr addr)
{
    return get_long (addr);
}
static ALWAYS_INLINE uae_u32 HWget_w(uaecptr addr)
{
    return get_word (addr);
}
static ALWAYS_INLINE uae_u32 HWget_b(uaecptr addr)
{
    return get_byte (addr);
}

static ALWAYS_INLINE uae_u32 uae_mmu_get_ilong(uaecptr addr)
{
	if (unlikely(is_unaligned(addr, 4)))
		return mmu_get_long_unaligned(addr, false);
	return mmu_get_long(addr, false, sz_long);
}
static ALWAYS_INLINE uae_u16 uae_mmu_get_iword(uaecptr addr)
{
	if (unlikely(is_unaligned(addr, 2)))
		return mmu_get_word_unaligned(addr, false);
	return mmu_get_word(addr, false, sz_word);
}
static ALWAYS_INLINE uae_u16 uae_mmu_get_ibyte(uaecptr addr)
{
	return mmu_get_byte(addr, false, sz_byte);
}
static ALWAYS_INLINE uae_u32 uae_mmu_get_long(uaecptr addr)
{
	if (unlikely(is_unaligned(addr, 4)))
		return mmu_get_long_unaligned(addr, true);
	return mmu_get_long(addr, true, sz_long);
}
static ALWAYS_INLINE uae_u16 uae_mmu_get_word(uaecptr addr)
{
	if (unlikely(is_unaligned(addr, 2)))
		return mmu_get_word_unaligned(addr, true);
	return mmu_get_word(addr, true, sz_word);
}
static ALWAYS_INLINE uae_u8 uae_mmu_get_byte(uaecptr addr)
{
	return mmu_get_byte(addr, true, sz_byte);
}
static ALWAYS_INLINE void uae_mmu_put_long(uaecptr addr, uae_u32 val)
{
	if (unlikely(is_unaligned(addr, 4)))
		mmu_put_long_unaligned(addr, val, true);
	else
		mmu_put_long(addr, val, true, sz_long);
}
static ALWAYS_INLINE void uae_mmu_put_word(uaecptr addr, uae_u16 val)
{
	if (unlikely(is_unaligned(addr, 2)))
		mmu_put_word_unaligned(addr, val, true);
	else
		mmu_put_word(addr, val, true, sz_word);
}
static ALWAYS_INLINE void uae_mmu_put_byte(uaecptr addr, uae_u8 val)
{
	mmu_put_byte(addr, val, true, sz_byte);
}

STATIC_INLINE void put_byte_mmu (uaecptr addr, uae_u32 v)
{
    uae_mmu_put_byte (addr, v);
}
STATIC_INLINE void put_word_mmu (uaecptr addr, uae_u32 v)
{
    uae_mmu_put_word (addr, v);
}
STATIC_INLINE void put_long_mmu (uaecptr addr, uae_u32 v)
{
    uae_mmu_put_long (addr, v);
}
STATIC_INLINE uae_u32 get_byte_mmu (uaecptr addr)
{
    return uae_mmu_get_byte (addr);
}
STATIC_INLINE uae_u32 get_word_mmu (uaecptr addr)
{
    return uae_mmu_get_word (addr);
}
STATIC_INLINE uae_u32 get_long_mmu (uaecptr addr)
{
    return uae_mmu_get_long (addr);
}
STATIC_INLINE uae_u32 get_ibyte_mmu (int o)
{
    uae_u32 pc = m68k_getpc () + o;
    return uae_mmu_get_iword (pc);
}
STATIC_INLINE uae_u32 get_iword_mmu (int o)
{
    uae_u32 pc = m68k_getpc () + o;
    return uae_mmu_get_iword (pc);
}
STATIC_INLINE uae_u32 get_ilong_mmu (int o)
{
    uae_u32 pc = m68k_getpc () + o;
    return uae_mmu_get_ilong (pc);
}
STATIC_INLINE uae_u32 next_iword_mmu (void)
{
    uae_u32 pc = m68k_getpc ();
    m68k_incpci (2);
    return uae_mmu_get_iword (pc);
}
STATIC_INLINE uae_u32 next_ilong_mmu (void)
{
    uae_u32 pc = m68k_getpc ();
    m68k_incpci (4);
    return uae_mmu_get_ilong (pc);
}

extern void m68k_do_rts_mmu (void);
extern void m68k_do_rte_mmu (uaecptr a7);
extern void m68k_do_bsr_mmu (uaecptr oldpc, uae_s32 offset);

#endif /* CPUMMU_H */
