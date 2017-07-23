/***************************************************************************

    i860.h

    Interface file for the Intel i860 emulator.

    Copyright (C) 1995-present Jason Eckhardt (jle@rice.edu)
    Released for general non-commercial use under the MAME license
    with the additional requirement that you are free to use and
    redistribute this code in modified or unmodified form, provided
    you list me in the credits.
    Visit http://mamedev.org for licensing and usage restrictions.

    Changes for previous/NeXTdimension by Simon Schubiger (SC)

***************************************************************************/

#pragma once

#ifndef __I860_H__
#define __I860_H__

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

#include "i860cfg.h"
#include "host.h"
#include "nd_sdl.h"

const int LOG_WARN = 3;
const int ND_SLOT  = 2; // HACK: one day we should put the whole ND in a C++ class or make an array of NeXTbus slots

extern "C" void Log_Printf(int nType, const char *psFormat, ...);

typedef uint64_t UINT64;
typedef int64_t INT64;

typedef uint32_t UINT32;
typedef int32_t INT32;

typedef uint16_t UINT16;
typedef int16_t INT16;

typedef uint8_t  UINT8;
typedef int8_t  INT8;

typedef int64_t offs_t;

extern "C" {
#include "dimension.h"

    void   nd_nbic_interrupt(void);
    bool   nd_dbg_cmd(const char* cmd);
    void   Statusbar_SetNdLed(int state);
    
    void   nd_set_blank_state(int src, bool state);

    typedef void (*mem_rd_func)(UINT32, UINT32*);
    typedef void (*mem_wr_func)(UINT32, const UINT32*);
}

#if WITH_SOFTFLOAT_I860
extern "C" {
#include <softfloat.h>
}
typedef float32 FLOAT32;
typedef float64 FLOAT64;

#define FLOAT32_ZERO            0x00000000
#define FLOAT32_ONE             0x3F800000
#define FLOAT32_IS_NEG(x)       ((x) & 0x80000000)
#define FLOAT32_IS_ZERO(x)      (((x) & 0x7FFFFFFF) == 0x00000000)
#define FLOAT64_ZERO            LIT64(0x0000000000000000)
#define FLOAT64_ONE             LIT64(0x3FF0000000000000)
#define FLOAT64_IS_NEG(x)       ((x) & LIT64(0x8000000000000000))
#define FLOAT64_IS_ZERO(x)      (((x) & LIT64(0x7FFFFFFFFFFFFFFF)) == LIT64(0x0000000000000000))

static inline void float_set_rounding_mode (int mode) {
    switch (mode) {
        case 0: float_rounding_mode2 = float_round_nearest_even; break;
        case 1: float_rounding_mode2 = float_round_down;         break;
        case 2: float_rounding_mode2 = float_round_up;           break;
        case 3: float_rounding_mode2 = float_round_to_zero;      break;
    }
}

#else // NATIVE FLOAT

#include <math.h>
#ifdef __MINGW32__
#define _GLIBCXX_HAVE_FENV_H 1
#endif
#include <fenv.h>
#if __APPLE__
#else
#pragma STDC FENV_ACCESS ON
#endif

typedef float FLOAT32;
typedef double FLOAT64;

#define FLOAT32_ZERO            0.0
#define FLOAT32_ONE             1.0
#define FLOAT32_IS_NEG(x)       ((x) < 0.0)
#define FLOAT32_IS_ZERO(x)      ((x) == 0.0)
#define FLOAT64_ZERO            0.0
#define FLOAT64_ONE             1.0
#define FLOAT64_IS_NEG(x)       ((x) < 0.0)
#define FLOAT64_IS_ZERO(x)      ((x) == 0.0)

#define float32_add(x,y)        ((x)+(y))
#define float32_sub(x,y)        ((x)-(y))
#define float32_mul(x,y)        ((x)*(y))
#define float32_div(x,y)        ((x)/(y))
#define float32_sqrt(x)         (sqrt(x))
#define float32_to_int32(x)     (rint(x))
#define float32_to_int32_round_to_zero(x)     ((UINT32)(x))
#define float32_to_float64(x)   ((double)(x))
#define float32_gt(x,y)         ((x)>(y))
#define float32_le(x,y)         ((x)<=(y))
#define float32_eq(x,y)         ((x)==(y))
#define float64_add(x,y)        ((x)+(y))
#define float64_sub(x,y)        ((x)-(y))
#define float64_mul(x,y)        ((x)*(y))
#define float64_div(x,y)        ((x)/(y))
#define float64_sqrt(x)         (sqrt(x))
#define float64_to_int32(x)     (rint(x))
#define float64_to_int32_round_to_zero(x)     ((UINT32)(x))
#define float64_to_float32(x)   ((float)(x))
#define float64_gt(x,y)         ((x)>(y))
#define float64_le(x,y)         ((x)<=(y))
#define float64_eq(x,y)         ((x)==(y))

static inline void float_set_rounding_mode (int mode) {
    switch (mode) {
        case 0: fesetround(FE_TONEAREST);  break;
        case 1: fesetround(FE_DOWNWARD);   break;
        case 2: fesetround(FE_UPWARD);     break;
        case 3: fesetround(FE_TOWARDZERO); break;
    }
}
#endif // NATIVE FLOAT


/***************************************************************************
    REGISTER ENUMERATION
***************************************************************************/


/* Various m_flow control flags (pending traps, pc update) */
enum {
    FLOW_CLEAR_MASK    = 0xF0000000,
    /* Indicate an instruction just generated a trap, so we know the PC
     needs to go to the trap address.  */
    TRAP_NORMAL        = 0x00000001,
    TRAP_IN_DELAY_SLOT = 0x00000002,
    TRAP_WAS_EXTERNAL  = 0x00000004,
    TRAP_MASK          = 0x00000007,
    /* Indicate a control-flow instruction, so we know the PC is updated.  */
    PC_UPDATED         = 0x00000100,
    /* Various memory access faults */
    EXITING_IFETCH     = 0x00001000,
    EXITING_READMEM    = 0x00010000,
    EXITING_WRITEMEM   = 0x00020000,
    EXITING_FPREADMEM  = 0x00030000,
    EXITING_FPWRITEMEM = 0x00040000,
    EXITING_MEMRW      = 0x00070000,
    /* This is 1 if the next fir load gets the trap address, otherwise
     it is 0 to get the ld.c address.  This is set to 1 only when a
     non-reset trap occurs.  */
    FIR_GETS_TRAP      = 0x10000000,
    /* An external interrupt occured. */
    EXT_INTR           = 0x20000000,
    /* A f-op with DIM bit set encountered. */
    DIM_OP             = 0x40000000,
};

enum {
    MSG_NONE           = 0x00,
    MSG_I860_RESET     = 0x01,
    MSG_I860_KILL      = 0x02,
    MSG_DBG_BREAK      = 0x04,
    MSG_INTR           = 0x08,
    MSG_DISPLAY_BLANK  = 0x10,
    MSG_VIDEO_BLANK    = 0x20,
};

/* dual mode instruction state */
enum {
    DIM_NONE,
    DIM_TEMP,
    DIM_FULL,
};

/* Macros for accessing register fields in instruction word.  */
#define get_isrc1(bits) (((bits) >> 11) & 0x1f)
#define get_isrc2(bits) (((bits) >> 21) & 0x1f)
#define get_idest(bits) (((bits) >> 16) & 0x1f)
#define get_fsrc1(bits) (((bits) >> 11) & 0x1f)
#define get_fsrc2(bits) (((bits) >> 21) & 0x1f)
#define get_fdest(bits) (((bits) >> 16) & 0x1f)
#define get_creg(bits) (((bits) >> 21) & 0x7)

/* Macros for accessing immediate fields.  */
/* 16-bit immediate.  */
#define get_imm16(insn) ((insn) & 0xffff)

/* A mask for all the trap bits of the PSR (FT, DAT, IAT, IN, IT, or
 bits [12..8]).  */
#define PSR_ALL_TRAP_BITS_MASK 0x00001f00

/* A mask for PSR bits which can only be changed from supervisor level.  */
#define PSR_SUPERVISOR_ONLY_MASK 0x0000fff3


/* PSR: BR flag (PSR[0]):  set/get.  */
#define GET_PSR_BR()  ((m_cregs[CR_PSR] >> 0) & 1)
#define SET_PSR_BR(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 0)) | (((val) & 1) << 0))

/* PSR: BW flag (PSR[1]):  set/get.  */
#define GET_PSR_BW()  ((m_cregs[CR_PSR] >> 1) & 1)
#define SET_PSR_BW(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 1)) | (((val) & 1) << 1))

/* PSR: Shift count (PSR[21..17]):  set/get.  */
#define GET_PSR_SC()  ((m_cregs[CR_PSR] >> 17) & 0x1f)
#define SET_PSR_SC(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~0x003e0000) | (((val) & 0x1f) << 17))

/* PSR: CC flag (PSR[2]):  set/get.  */
#define GET_PSR_CC()      ((m_cregs[CR_PSR] >> 2) & 1)
#define SET_PSR_CC_F(val) (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 2)) | (((val) & 1) << 2))

/* PSR: IT flag (PSR[8]):  set/get.  */
#define GET_PSR_IT()  ((m_cregs[CR_PSR] >> 8) & 1)
#define SET_PSR_IT(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 8)) | (((val) & 1) << 8))

/* PSR: IN flag (PSR[9]):  set/get.  */
#define GET_PSR_IN()  ((m_cregs[CR_PSR] >> 9) & 1)
#define SET_PSR_IN(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 9)) | (((val) & 1) << 9))

/* PSR: IAT flag (PSR[10]):  set/get.  */
#define GET_PSR_IAT()  ((m_cregs[CR_PSR] >> 10) & 1)
#define SET_PSR_IAT(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 10)) | (((val) & 1) << 10))

/* PSR: DAT flag (PSR[11]):  set/get.  */
#define GET_PSR_DAT()  ((m_cregs[CR_PSR] >> 11) & 1)
#define SET_PSR_DAT(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 11)) | (((val) & 1) << 11))

/* PSR: FT flag (PSR[12]):  set/get.  */
#define GET_PSR_FT()  ((m_cregs[CR_PSR] >> 12) & 1)
#define SET_PSR_FT(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 12)) | (((val) & 1) << 12))

/* PSR: DS flag (PSR[13]):  set/get.  */
#define GET_PSR_DS()  ((m_cregs[CR_PSR] >> 13) & 1)
#define SET_PSR_DS(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 13)) | (((val) & 1) << 13))

/* PSR: DIM flag (PSR[14]):  set/get.  */
#define GET_PSR_DIM()  ((m_cregs[CR_PSR] >> 14) & 1)
#define SET_PSR_DIM(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 14)) | (((val) & 1) << 14))

/* PSR: LCC (PSR[3]):  set/get.  */
#define GET_PSR_LCC()  ((m_cregs[CR_PSR] >> 3) & 1)
#define SET_PSR_LCC(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 3)) | (((val) & 1) << 3))

/* PSR: IM (PSR[4]):  set/get.  */
#define GET_PSR_IM()  ((m_cregs[CR_PSR] >> 4) & 1)
#define SET_PSR_IM(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 4)) | (((val) & 1) << 4))

/* PSR: PIM (PSR[5]):  set/get.  */
#define GET_PSR_PIM()  ((m_cregs[CR_PSR] >> 5) & 1)
#define SET_PSR_PIM(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 5)) | (((val) & 1) << 5))

/* PSR: U (PSR[6]):  set/get.  */
#define GET_PSR_U()  ((m_cregs[CR_PSR] >> 6) & 1)
#define SET_PSR_U(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 6)) | (((val) & 1) << 6))

/* PSR: PU (PSR[7]):  set/get.  */
#define GET_PSR_PU()  ((m_cregs[CR_PSR] >> 7) & 1)
#define SET_PSR_PU(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 7)) | (((val) & 1) << 7))

/* PSR: Pixel size (PSR[23..22]):  set/get.  */
#define GET_PSR_PS()  ((m_cregs[CR_PSR] >> 22) & 0x3)
#define SET_PSR_PS(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~0x00c00000) | (((val) & 0x3) << 22))

/* PSR: Pixel mask (PSR[31..24]):  set/get.  */
#define GET_PSR_PM()  ((m_cregs[CR_PSR] >> 24) & 0xff)
#define SET_PSR_PM(val)  (m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~0xff000000) | (((val) & 0xff) << 24))

/* EPSR: WP bit (EPSR[14]):  set/get.  */
#define GET_EPSR_WP()  ((m_cregs[CR_EPSR] >> 14) & 1)
#define SET_EPSR_WP(val)  (m_cregs[CR_EPSR] = (m_cregs[CR_EPSR] & ~(1 << 14)) | (((val) & 1) << 14))

/* EPSR: INT bit (EPSR[17]):  set/get.  */
#define GET_EPSR_INT()  ((m_cregs[CR_EPSR] >> 17) & 1)
#define SET_EPSR_INT(val)  (m_cregs[CR_EPSR] = (m_cregs[CR_EPSR] & ~(1 << 17)) | (((val) & 1) << 17))

/* EPSR: OF flag (EPSR[24]):  set/get.  */
#define GET_EPSR_OF()  ((m_cregs[CR_EPSR] >> 24) & 1)
#define SET_EPSR_OF(val)  (m_cregs[CR_EPSR] = (m_cregs[CR_EPSR] & ~(1 << 24)) | (((val) & 1) << 24))

/* EPSR: BE flag (EPSR[23]):  set/get.  */
#define GET_EPSR_BE()  ((m_cregs[CR_EPSR] >> 23) & 1)
#define SET_EPSR_BE(val)  (m_cregs[CR_EPSR] = (m_cregs[CR_EPSR] & ~(1 << 23)) | (((val) & 1) << 23))

/* DIRBASE: ATE bit (DIRBASE[0]):  get.  */
#define GET_DIRBASE_ATE()  (m_cregs[CR_DIRBASE] & 1)

/* DIRBASE: CS8 bit (DIRBASE[7]):  get.  */
#define GET_DIRBASE_CS8()  ((m_cregs[CR_DIRBASE] >> 7) & 1)

/* DIRBASE: CS8 bit (DIRBASE[7]):  get.  */
#define GET_DIRBASE_ITI()  ((m_cregs[CR_DIRBASE] >> 5) & 1)

/* FSR: FTE bit (FSR[5]):  set/get.  */
#define GET_FSR_FTE()  ((m_cregs[CR_FSR] >> 5) & 1)
#define SET_FSR_FTE(val)  (m_cregs[CR_FSR] = (m_cregs[CR_FSR] & ~(1 << 5)) | (((val) & 1) << 5))

/* FSR: SE bit (FSR[8]):  set/get.  */
#define GET_FSR_SE()  ((m_cregs[CR_FSR] >> 8) & 1)
#define SET_FSR_SE(val)  (m_cregs[CR_FSR] = (m_cregs[CR_FSR] & ~(1 << 8)) | (((val) & 1) << 8))

/* FSR: SE bit (RM[3..2]):  set/get.  */
#define GET_FSR_RM()    ((m_cregs[CR_FSR] >> 2) & 3)
#define SET_FSR_RM(val) (m_cregs[CR_FSR] = (m_cregs[CR_FSR] & ~0xC) | (((val) & 3) << 2))

#define CLEAR_FLOW() (m_flow &= FLOW_CLEAR_MASK)

/* check for pending trap */
#define PENDING_TRAP() (m_flow & TRAP_MASK)

/* check for updated PC */
#define GET_PC_UPDATED() (m_flow & PC_UPDATED)
#define SET_PC_UPDATED() m_flow |= PC_UPDATED

/* access fault traps */
#define GET_EXITING_MEMRW()    (m_flow & EXITING_MEMRW)
#define SET_EXITING_MEMRW(val) (m_flow = (val) | (m_flow & ~EXITING_MEMRW))

const UINT32 INSN_NOP      = 0xA0000000;
const UINT32 INSN_DIM      = 0x00000200;
const UINT32 INSN_FNOP     = 0xB0000000;
const UINT32 INSN_FNOP_DIM = INSN_FNOP | INSN_DIM;
const UINT32 INSN_FP       = 0x48000000;
const UINT32 INSN_FP_DIM   = INSN_FP   | INSN_DIM;
const UINT32 INSN_MASK     = 0xFC000000;
const UINT32 INSN_MASK_DIM = INSN_MASK | INSN_DIM;

const size_t I860_ICACHE_SZ       = 9; // in powers of two lines (2^9 = 512; 512 x 2 words = 4 kbytes)
const size_t I860_ICACHE_MASK     = (1<<I860_ICACHE_SZ)-1;
const size_t I860_TLB_SZ          = 11; // in powers of two
const size_t I860_TLB_MASK        = (1<<I860_TLB_SZ)-1;
const size_t I860_PAGE_SZ         = 12; // in powers of two
const size_t I860_PAGE_OFF_MASK   = (1<<I860_PAGE_SZ)-1;
const size_t I860_PAGE_FRAME_MASK = ~I860_PAGE_OFF_MASK;
const size_t I860_TLB_FLAGS       = I860_PAGE_OFF_MASK;

/* Control register numbers.  */
enum {
    CR_FIR     = 0,
    CR_PSR     = 1,
    CR_DIRBASE = 2,
    CR_DB      = 3,
    CR_FSR     = 4,
    CR_EPSR    = 5
};

class i860_reg {
    UINT32        id;
    const char*   name;
    const char*   format;
    const UINT32* reg;
public:
    i860_reg() : id(0), name(0), format(0), reg(&id) {}
    
    bool valid() {
        return name;
    }
    
    void formatstr(const char* format) {
        this->format = format;
    }
    
    void set(int regId, const char* name, const UINT32 * reg) {
        this->id   = regId;
        this->name = name;
        this->reg  = reg;
    }
    
    UINT32 get() const {
        return *reg;
    }
    
    const char* get_name() {
        return name;
    }
};

class i860_cpu_device {
public:
	// construction/destruction
    i860_cpu_device();
    
    /* External interface */
    void send_msg(int msg);
    void init();
    void uninit();
    void halt(bool state);
    void pause(bool state);
    inline bool is_halted() {return m_halt;};

    /* Run one i860 cycle */
    void    run_cycle();
    /* Run the i860 thread */
    void run();
    /* i860 thread message handler */
    bool   handle_msgs();
    /* External interrupt for i860 emulator */
    void   interrupt();
    
    const char* reports(double realTime, double hostTIme);
private:
    // debugger
    void debugger(char cmd, const char* format, ...);
    void debugger();
    
    /* Message port for host->i860 communication */
    volatile int m_port;
    lock_t       m_port_lock;
    thread_t*    m_thread;

    UINT64 m_insn_decoded;
    UINT64 m_icache_hit;
    UINT64 m_icache_miss;
    UINT64 m_icache_inval;
    UINT64 m_tlb_hit;
    UINT64 m_tlb_miss;
    UINT64 m_tlb_inval;
    UINT64 m_intrs;
    UINT32 m_last_rt;
    UINT32 m_last_vt;
    char   m_report[1024];

    /* Debugger stuff */
    char   m_lastcmd;
    char   m_console[32*1024];
    int    m_console_idx;
    bool   m_break_on_next_msg;
    UINT32 m_traceback[256];
    int    m_traceback_idx;
    
    /* Program counter (1 x 32-bits).  Reset starts at pc=0xffffff00.  */
    UINT32 m_pc;

	/* Integer registers (32 x 32-bits).  */
	UINT32  m_iregs[32];
    
	/* Floating point registers (32 x 32-bits, 16 x 64 bits, or 8 x 128 bits).
	   When referenced as pairs or quads, the higher numbered registers
	   are the upper bits. E.g., double precision f0 is f1:f0.  */
	UINT8   m_fregs[32 * 4];

	/* Control registers (6 x 32-bits).  */
	UINT32 m_cregs[6];

    /* Dual instruction mode flags */
    int  m_dim;
    bool m_dim_cc;
    bool m_dim_cc_valid;
    int  m_save_dim;
    int  m_save_flow;
    bool m_save_cc;
    bool m_save_cc_valid;
    
	/* Special registers (4 x 64-bits).  */
	union
	{
		FLOAT32 s;
		FLOAT64 d;
	} m_KR, m_KI, m_T;
    
	UINT64 m_merge;

	/* The adder pipeline, always 3 stages.  */
	struct
	{
		/* The stage contents.  */
		union {
			FLOAT32 s;
			FLOAT64 d;
		} val;

		/* The stage status bits.  */
		struct {
			/* Adder result precision (1 = dbl, 0 = sgl).  */
			char arp;
		} stat;
	} m_A[3];

	/* The multiplier pipeline. 3 stages for single precision, 2 stages
	   for double precision, and confusing for mixed precision.  */
	struct {
		/* The stage contents.  */
		union {
			FLOAT32 s;
			FLOAT64 d;
		} val;

		/* The stage status bits.  */
		struct {
			/* Multiplier result precision (1 = dbl, 0 = sgl).  */
			char mrp;
		} stat;
	} m_M[3];

	/* The load pipeline, always 3 stages.  */
	struct {
		/* The stage contents.  */
		union {
			FLOAT32 s;
			FLOAT64 d;
		} val;

		/* The stage status bits.  */
		struct {
			/* Load result precision (1 = dbl, 0 = sgl).  */
			char lrp;
		} stat;
	} m_L[3];

	/* The graphics/integer pipeline, always 1 stage.  */
	struct {
		/* The stage contents.  */
		union {
			FLOAT32 s;
			FLOAT64 d;
		} val;

		/* The stage status bits.  */
		struct {
			/* Integer/graphics result precision (1 = dbl, 0 = sgl).  */
			char irp;
		} stat;
	} m_G;

    /* Instruction cache */
    UINT64 m_icache[1<<I860_ICACHE_SZ];
    UINT32 m_icache_vaddr[1<<I860_ICACHE_SZ];
    
    /* Translation look-aside buffer */
    UINT32 m_tlb_vaddr[1<<I860_TLB_SZ];
    UINT32 m_tlb_paddr[1<<I860_TLB_SZ];
    
	/*
	 * Halt state. Can be set externally
	 */
    volatile bool m_halt;
    
	/* Indicate an instruction just generated a trap,
     needs to go to the trap address or a control-flow 
     instruction, so we know the PC is updated.  */
	UINT32 m_flow;
    
    /* Single stepping state - for internal use.  */
    UINT32 m_single_stepping;

    /* memory access */
    mem_rd_func rdmem[17];
    mem_wr_func wrmem[17];
    
    void   set_mem_access(bool be);
    UINT8  rdcs8(UINT32 addr);
	inline void   writemem_emu(UINT32 addr, int size, UINT8 *data);
	inline void   writemem_emu(UINT32 addr, int size, UINT8 *data, UINT32 wmask);
    inline void   readmem_emu (UINT32 addr, int size, UINT8 *data);

    /* instructions */
	void insn_ld_ctrl (UINT32 insn);
	void insn_st_ctrl (UINT32 insn);
	void insn_ldx (UINT32 insn);
	void insn_stx (UINT32 insn);
	void insn_fsty (UINT32 insn);
	void insn_fldy (UINT32 insn);
	void insn_pstd (UINT32 insn);
	void insn_ixfr (UINT32 insn);
	void insn_addu (UINT32 insn);
	void insn_addu_imm (UINT32 insn);
	void insn_adds (UINT32 insn);
	void insn_adds_imm (UINT32 insn);
	void insn_subu (UINT32 insn);
	void insn_subu_imm (UINT32 insn);
	void insn_subs (UINT32 insn);
	void insn_subs_imm (UINT32 insn);
	void insn_shl (UINT32 insn);
	void insn_shl_imm (UINT32 insn);
	void insn_shr (UINT32 insn);
	void insn_shr_imm (UINT32 insn);
	void insn_shra (UINT32 insn);
	void insn_shra_imm (UINT32 insn);
	void insn_shrd (UINT32 insn);
	void insn_and (UINT32 insn);
	void insn_and_imm (UINT32 insn);
	void insn_andh_imm (UINT32 insn);
	void insn_andnot (UINT32 insn);
	void insn_andnot_imm (UINT32 insn);
	void insn_andnoth_imm (UINT32 insn);
	void insn_or (UINT32 insn);
	void insn_or_imm (UINT32 insn);
	void insn_orh_imm (UINT32 insn);
	void insn_xor (UINT32 insn);
	void insn_xor_imm (UINT32 insn);
	void insn_xorh_imm (UINT32 insn);
	void insn_trap (UINT32 insn);
	void insn_intovr (UINT32 insn);
	void insn_bte (UINT32 insn);
	void insn_bte_imm (UINT32 insn);
	void insn_btne (UINT32 insn);
	void insn_btne_imm (UINT32 insn);
	void insn_bc (UINT32 insn);
	void insn_bnc (UINT32 insn);
	void insn_bct (UINT32 insn);
	void insn_bnct (UINT32 insn);
	void insn_call (UINT32 insn);
	void insn_br (UINT32 insn);
	void insn_bri (UINT32 insn);
	void insn_calli (UINT32 insn);
	void insn_bla (UINT32 insn);
	void insn_flush (UINT32 insn);
	void insn_fmul (UINT32 insn);
	void insn_fmlow (UINT32 insn);
	void insn_fadd_sub (UINT32 insn);
	void insn_dualop (UINT32 insn);
	void insn_frcp (UINT32 insn);
	void insn_frsqr (UINT32 insn);
	void insn_fxfr (UINT32 insn);
	void insn_ftrunc (UINT32 insn);
    void insn_fix (UINT32 insn);
	void insn_famov (UINT32 insn);
	void insn_fiadd_sub (UINT32 insn);
	void insn_fcmp (UINT32 insn);
	void insn_fzchk (UINT32 insn);
	void insn_form (UINT32 insn);
	void insn_faddp (UINT32 insn);
	void insn_faddz (UINT32 insn);

    void dec_unrecog (UINT32 insn);

    /* register access */
    UINT32 get_iregval(int gr);
    void   set_iregval(int gr, UINT32 val);
    FLOAT32  get_fregval_s (int fr);
    void   set_fregval_s (int fr, FLOAT32 s);
    FLOAT64 get_fregval_d (int fr);
    void   set_fregval_d (int fr, FLOAT64 d);
    void   SET_PSR_CC(int val);
    
    void   invalidate_icache();
    void   invalidate_tlb();
    inline UINT64 ifetch64(const UINT32 pc);
    UINT64 ifetch64(const UINT32 pc, const UINT32 vaddr, int const cidx);
    UINT32 ifetch(const UINT32 pc);
    UINT32 ifetch_notrap(const UINT32 pc);
    void   handle_trap(UINT32 savepc);
    void   ret_from_trap();
    void   unrecog_opcode (UINT32 pc, UINT32 insn);
    
    void   decode_exec (UINT32 insn);
    void   dump_pipe (int type);
    void   dump_state ();
	UINT32 disasm (UINT32 addr, int len);
    offs_t disasm(char* buffer, offs_t pc);
	void   dbg_memdump (UINT32 addr, int len);
	int    delay_slots(UINT32 insn);
	UINT32 get_address_translation(UINT32 vaddr, int is_dataref, int is_write);
    inline UINT32 get_address_translation(UINT32 vaddr, UINT32 voffset, UINT32 tlbidx, int is_dataref, int is_write);
	FLOAT32  get_fval_from_optype_s (UINT32 insn, int optype);
	FLOAT64 get_fval_from_optype_d (UINT32 insn, int optype);
    int    memtest(bool be);
    void   dbg_check_wr(UINT32 addr, int size, UINT8* data);
    
    /* This is theinterface for asserting an external interrupt to the i860.  */
    void gen_interrupt();
    /* This is the interface for clearing an external interrupt of the i860.  */
    void clr_interrupt();
    /* This is the interface for reseting the i860.  */
    void reset();
    void intr();

	typedef void (i860_cpu_device::*insn_func)(UINT32);
	static const insn_func decode_tbl[64];
	static const insn_func core_esc_decode_tbl[8];
	static const insn_func fp_decode_tbl[128];
    static       insn_func decoder_tbl[8192];
};

/* disassembler */
int i860_disassembler(UINT32 pc, UINT32 insn, char* buffer);

#endif /* __I860_H__ */
