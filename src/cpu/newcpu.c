/*
* UAE - The Un*x Amiga Emulator
*
* MC68000 emulation
*
* (c) 1995 Bernd Schmidt
*/

#define MMUOP_DEBUG 2
#define DEBUG_CD32CDTVIO 0

#include "main.h"
#include "m68000.h"
#include "compat.h"
#include "sysconfig.h"
#include "sysdeps.h"
#include "hatari-glue.h"
#include "options_cpu.h"
#include "maccess.h"
#include "memory.h"
#include "newcpu.h"
#include "cpummu.h"
#include "cpummu030.h"
#ifdef WITH_SOFTFLOAT
#include "fpp-softfloat.h"
#else
#include "md-fpp.h"
#endif
#include "main.h"
#include "dsp.h"
#include "dimension.h"
#include "reset.h"
#include "cycInt.h"
#include "dialog.h"
#include "screen.h"
#include "video.h"
#include "options.h"
#include "log.h"
#include "debugui.h"
#include "debugcpu.h"


/* Opcode of faulting instruction */
static uae_u16 last_op_for_exception_3;
/* PC at fault time */
static uaecptr last_addr_for_exception_3;
/* Address that generated the exception */
static uaecptr last_fault_for_exception_3;
/* read (0) or write (1) access */
static int last_writeaccess_for_exception_3;
/* instruction (1) or data (0) access */
static int last_instructionaccess_for_exception_3;
int mmu_enabled, mmu_triggered;
int cpu_cycles;

const int areg_byteinc[] = { 1, 1, 1, 1, 1, 1, 1, 2 };
const int imm8_table[] = { 8, 1, 2, 3, 4, 5, 6, 7 };

int movem_index1[256];
int movem_index2[256];
int movem_next[256];

cpuop_func *cpufunctbl[65536];

int OpcodeFamily;
struct mmufixup mmufixup[2];

uae_u32 fpp_get_fpsr (void);

static struct cache030 icaches030[CACHELINES030];
static struct cache030 dcaches030[CACHELINES030];

void m68k_disasm_2 (TCHAR *buf, int bufsize, uaecptr addr, uaecptr *nextpc, int cnt, uae_u32 *seaddr, uae_u32 *deaddr, int safemode);

uae_u32 (*x_prefetch)(int);
uae_u32 (*x_next_iword)(void);
uae_u32 (*x_next_ilong)(void);
uae_u32 (*x_get_ilong)(int);
uae_u32 (*x_get_iword)(int);
uae_u32 (*x_get_ibyte)(int);
uae_u32 (*x_get_long)(uaecptr);
uae_u32 (*x_get_word)(uaecptr);
uae_u32 (*x_get_byte)(uaecptr);
void (*x_put_long)(uaecptr,uae_u32);
void (*x_put_word)(uaecptr,uae_u32);
void (*x_put_byte)(uaecptr,uae_u32);

uae_u32 (*x_cp_next_iword)(void);
uae_u32 (*x_cp_next_ilong)(void);
uae_u32 (*x_cp_get_long)(uaecptr);
uae_u32 (*x_cp_get_word)(uaecptr);
uae_u32 (*x_cp_get_byte)(uaecptr);
void (*x_cp_put_long)(uaecptr,uae_u32);
void (*x_cp_put_word)(uaecptr,uae_u32);
void (*x_cp_put_byte)(uaecptr,uae_u32);
uae_u32 (REGPARAM3 *x_cp_get_disp_ea_020)(uae_u32 base, int idx) REGPARAM;

// shared memory access functions
static void set_x_funcs (void)
{
    if (currprefs.cpu_model == 68040) {
        x_prefetch   = get_iword_mmu040;
        x_get_ilong  = get_ilong_mmu040;
        x_get_iword  = get_iword_mmu040;
        x_get_ibyte  = get_ibyte_mmu040;
        x_next_iword = next_iword_mmu040;
        x_next_ilong = next_ilong_mmu040;
        x_put_long   = put_long_mmu040;
        x_put_word   = put_word_mmu040;
        x_put_byte   = put_byte_mmu040;
        x_get_long   = get_long_mmu040;
        x_get_word   = get_word_mmu040;
        x_get_byte   = get_byte_mmu040;
    } else {
        x_prefetch   = get_iword_mmu030;
        x_get_ilong  = get_ilong_mmu030;
        x_get_iword  = get_iword_mmu030;
        x_get_ibyte  = get_ibyte_mmu030;
        x_next_iword = next_iword_mmu030;
        x_next_ilong = next_ilong_mmu030;
        x_put_long   = put_long_mmu030;
        x_put_word   = put_word_mmu030;
        x_put_byte   = put_byte_mmu030;
        x_get_long   = get_long_mmu030;
        x_get_word   = get_word_mmu030;
        x_get_byte   = get_byte_mmu030;
    }
    
    if (currprefs.mmu_model == 68030) {
        x_cp_put_long = put_long_mmu030_state;
        x_cp_put_word = put_word_mmu030_state;
        x_cp_put_byte = put_byte_mmu030_state;
        x_cp_get_long = get_long_mmu030_state;
        x_cp_get_word = get_word_mmu030_state;
        x_cp_get_byte = get_byte_mmu030_state;
        x_cp_next_iword = next_iword_mmu030_state;
        x_cp_next_ilong = next_ilong_mmu030_state;
        x_cp_get_disp_ea_020 = get_disp_ea_020_mmu030;
    } else {
        x_cp_put_long = x_put_long;
        x_cp_put_word = x_put_word;
        x_cp_put_byte = x_put_byte;
        x_cp_get_long = x_get_long;
        x_cp_get_word = x_get_word;
        x_cp_get_byte = x_get_byte;
        x_cp_next_iword = x_next_iword;
        x_cp_next_ilong = x_next_ilong;
        x_cp_get_disp_ea_020 = x_get_disp_ea_020;
    }
}

void flush_cpu_caches_040(uae_u16 opcode)
{
    int cache = (opcode >> 6) & 3;
    if (!(cache & 2))
        return;
    set_cpu_caches(true);
}

void set_cpu_caches (bool flush)
{
	int i;

	if (currprefs.cpu_model == 68030) {
		if (regs.cacr & 0x08) { // clear instr cache
			for (i = 0; i < CACHELINES030; i++) {
				icaches030[i].valid[0] = 0;
				icaches030[i].valid[1] = 0;
				icaches030[i].valid[2] = 0;
				icaches030[i].valid[3] = 0;
			}
            regs.cacr &= ~0x08;
		}
		if (regs.cacr & 0x04) { // clear entry in instr cache
			icaches030[(regs.caar >> 4) & (CACHELINES030 - 1)].valid[(regs.caar >> 2) & 3] = 0;
			regs.cacr &= ~0x04;
		}
		if (regs.cacr & 0x800) { // clear data cache
			for (i = 0; i < CACHELINES030; i++) {
				dcaches030[i].valid[0] = 0;
				dcaches030[i].valid[1] = 0;
				dcaches030[i].valid[2] = 0;
				dcaches030[i].valid[3] = 0;
			}
			regs.cacr &= ~0x800;
		}
		if (regs.cacr & 0x400) { // clear entry in data cache
			dcaches030[(regs.caar >> 4) & (CACHELINES030 - 1)].valid[(regs.caar >> 2) & 3] = 0;
			regs.cacr &= ~0x400;
		}
	} else if (currprefs.cpu_model == 68040) {
        if(ConfigureParams.System.bRealtime) {
#if 0 /* FIXME */
            if(regs.cacr & 0x8000) {
                flush_icache(0, -1);
                x_prefetch   = getc_iword_mmu040;
                x_get_ilong  = getc_ilong_mmu040;
                x_get_iword  = getc_iword_mmu040;
                x_get_ibyte  = getc_ibyte_mmu040;
                x_next_iword = nextc_iword_mmu040;
                x_next_ilong = nextc_ilong_mmu040;
            } else
#endif
            {
                x_prefetch   = get_iword_mmu040;
                x_get_ilong  = get_ilong_mmu040;
                x_get_iword  = get_iword_mmu040;
                x_get_ibyte  = get_ibyte_mmu040;
                x_next_iword = next_iword_mmu040;
                x_next_ilong = next_ilong_mmu040;
            }
        }
	}
}

static uae_u32 REGPARAM2 op_illg_1 (uae_u32 opcode)
{
    op_illg (opcode);
    return 4;
}
static uae_u32 REGPARAM2 op_unimpl_1 (uae_u32 opcode)
{
    if ((opcode & 0xf000) == 0xf000 || currprefs.cpu_model < 68060)
        op_illg (opcode);
    else
        op_unimpl (opcode);
    return 4;
}

void build_cpufunctbl (void)
{
	int i, opcnt;
	unsigned long opcode;
	const struct cputbl *tbl = 0;
	int lvl;

	switch (currprefs.cpu_model)
	{
	case 68040:
		lvl = 4;
		tbl = op_smalltbl_31_ff;
		break;
	case 68030:
		lvl = 3;
        tbl = op_smalltbl_32_ff;
		break;
	default:
		changed_prefs.cpu_model = currprefs.cpu_model = 68030;
        lvl = 3;
        tbl = op_smalltbl_32_ff;
	}

	if (tbl == 0) {
		write_log ("no CPU emulation cores available CPU=%d!", currprefs.cpu_model);
        DebugUI();
	}

	for (opcode = 0; opcode < 65536; opcode++)
		cpufunctbl[opcode] = op_illg_1;
	for (i = 0; tbl[i].handler != NULL; i++) {
		opcode = tbl[i].opcode;
		cpufunctbl[opcode] = tbl[i].handler;
	}

	opcnt = 0;
	for (opcode = 0; opcode < 65536; opcode++) {
		cpuop_func *f;

		if (table68k[opcode].mnemo == i_ILLG)
			continue;
		if (table68k[opcode].clev > lvl) {
			continue;
		}

		if (table68k[opcode].handler != -1) {
			int idx = table68k[opcode].handler;
			f = cpufunctbl[idx];
			if (f == op_illg_1)
                DebugUI();
			cpufunctbl[opcode] = f;
			opcnt++;
		}
	}
	write_log ("Building CPU, %d opcodes (%d %d)\n",
		opcnt, lvl, currprefs.cpu_compatible ? 1 : 0);
	write_log ("CPU=%d, MMU=%d, FPU=%d ($%02x), JIT%s=%d, realtime=%d\n",
               currprefs.cpu_model, currprefs.mmu_model,
               currprefs.fpu_model, currprefs.fpu_revision,
		currprefs.cachesize ? (currprefs.compfpu ? "=CPU/FPU" : "=CPU") : "",
		currprefs.cachesize, ConfigureParams.System.bRealtime);
	set_cpu_caches (0);
	if (currprefs.mmu_model) {
        if (currprefs.cpu_model >= 68040) {
            mmu_reset ();
            mmu_set_tc (regs.tcr);
            mmu_set_super (regs.s != 0);
        } else {
            mmu030_reset(1);
        }
    }
}

static void prefs_changed_cpu (void) {
	currprefs.cpu_model = changed_prefs.cpu_model;
	currprefs.fpu_model = changed_prefs.fpu_model;
    currprefs.fpu_revision = changed_prefs.fpu_revision;
	currprefs.mmu_model = changed_prefs.mmu_model;
	currprefs.cpu_compatible = changed_prefs.cpu_compatible;
}

void check_prefs_changed_cpu (void)
{
	bool changed = 0;

    if (changed
		|| currprefs.cpu_model != changed_prefs.cpu_model
		|| currprefs.fpu_model != changed_prefs.fpu_model
        || currprefs.fpu_revision != changed_prefs.fpu_revision
		|| currprefs.mmu_model != changed_prefs.mmu_model
		|| currprefs.cpu_compatible != changed_prefs.cpu_compatible) {

			prefs_changed_cpu ();
			build_cpufunctbl ();
			changed = 1;
	}

	if (currprefs.cpu_idle != changed_prefs.cpu_idle) {
		currprefs.cpu_idle = changed_prefs.cpu_idle;
	}
	if (changed)
		set_special (SPCFLAG_MODE_CHANGE);

}

void init_m68k (void)
{
	int i;

	prefs_changed_cpu ();
    
	for (i = 0 ; i < 256 ; i++) {
		int j;
		for (j = 0 ; j < 8 ; j++) {
			if (i & (1 << j)) break;
		}
		movem_index1[i] = j;
		movem_index2[i] = 7-j;
		movem_next[i] = i & (~(1 << j));
	}

	write_log ("Building CPU table for configuration: %d", currprefs.cpu_model);
	regs.address_space_mask = 0xffffffff;

    if (currprefs.fpu_model > 0)
		write_log ("/%d", currprefs.fpu_model);
    if (currprefs.cpu_compatible)
		write_log (" prefetch");
	write_log ("\n");

	read_table68k ();
	do_merges ();

	write_log ("%d CPU functions\n", nr_cpuop_funcs);

	build_cpufunctbl ();
	set_x_funcs ();
}

struct regstruct regs;
struct flag_struct regflags;

#define get_word_debug(o) DBGMemory_ReadWord (o)
#define get_iword_debug(o) DBGMemory_ReadWord (o)
#define get_ilong_debug(o) DBGMemory_ReadLong (o)

static uaecptr ShowEA (void *f, uaecptr pc, uae_u16 opcode, int reg, amodes mode, wordsizes size, TCHAR *buf, uae_u32 *eaddr, int safemode)
{
    uae_u16 dp;
    uae_s8 disp8;
    uae_s16 disp16;
    int r;
    uae_u32 dispreg;
    uaecptr addr = pc;
    uae_s32 offset = 0;
    TCHAR buffer[80];
    
    switch (mode){
        case Dreg:
            _stprintf (buffer, _T("D%d"), reg);
            break;
        case Areg:
            _stprintf (buffer, _T("A%d"), reg);
            break;
        case Aind:
            _stprintf (buffer, _T("(A%d)"), reg);
            addr = regs.regs[reg + 8];
            break;
        case Aipi:
            _stprintf (buffer, _T("(A%d)+"), reg);
            addr = regs.regs[reg + 8];
            break;
        case Apdi:
            _stprintf (buffer, _T("-(A%d)"), reg);
            addr = regs.regs[reg + 8];
            break;
        case Ad16:
        {
            TCHAR offtxt[80];
            disp16 = get_iword_debug (pc); pc += 2;
            if (disp16 < 0)
                _stprintf (offtxt, _T("-$%04x"), -disp16);
            else
                _stprintf (offtxt, _T("$%04x"), disp16);
            addr = m68k_areg (regs, reg) + disp16;
            _stprintf (buffer, _T("(A%d, %s) == $%08x"), reg, offtxt, addr);
        }
            break;
        case Ad8r:
            dp = get_iword_debug (pc); pc += 2;
            disp8 = dp & 0xFF;
            r = (dp & 0x7000) >> 12;
            dispreg = dp & 0x8000 ? m68k_areg (regs, r) : m68k_dreg (regs, r);
            if (!(dp & 0x800)) dispreg = (uae_s32)(uae_s16)(dispreg);
            dispreg <<= (dp >> 9) & 3;
            
            if (dp & 0x100) {
                uae_s32 outer = 0, disp = 0;
                uae_s32 base = m68k_areg (regs, reg);
                TCHAR name[10];
                _stprintf (name, _T("A%d, "), reg);
                if (dp & 0x80) { base = 0; name[0] = 0; }
                if (dp & 0x40) dispreg = 0;
                if ((dp & 0x30) == 0x20) { disp = (uae_s32)(uae_s16)get_iword_debug (pc); pc += 2; }
                if ((dp & 0x30) == 0x30) { disp = get_ilong_debug (pc); pc += 4; }
                base += disp;
                
                if ((dp & 0x3) == 0x2) { outer = (uae_s32)(uae_s16)get_iword_debug (pc); pc += 2; }
                if ((dp & 0x3) == 0x3) { outer = get_ilong_debug (pc); pc += 4; }
                
                if (!(dp & 4)) base += dispreg;
                if ((dp & 3) && !safemode) base = get_ilong_debug (base);
                if (dp & 4) base += dispreg;
                
                addr = base + outer;
                _stprintf (buffer, _T("(%s%c%d.%c*%d+%d)+%d == $%08x"), name,
                           dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W',
                           1 << ((dp >> 9) & 3),
                           disp, outer, addr);
            } else {
                addr = m68k_areg (regs, reg) + (uae_s32)((uae_s8)disp8) + dispreg;
                _stprintf (buffer, _T("(A%d, %c%d.%c*%d, $%02x) == $%08x"), reg,
                           dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W',
                           1 << ((dp >> 9) & 3), disp8, addr);
            }
            break;
        case PC16:
            disp16 = get_iword_debug (pc); pc += 2;
            addr += (uae_s16)disp16;
            _stprintf (buffer, _T("(PC,$%04x) == $%08x"), disp16 & 0xffff, addr);
            break;
        case PC8r:
            dp = get_iword_debug (pc); pc += 2;
            disp8 = dp & 0xFF;
            r = (dp & 0x7000) >> 12;
            dispreg = dp & 0x8000 ? m68k_areg (regs, r) : m68k_dreg (regs, r);
            if (!(dp & 0x800)) dispreg = (uae_s32)(uae_s16)(dispreg);
            dispreg <<= (dp >> 9) & 3;
            
            if (dp & 0x100) {
                uae_s32 outer = 0, disp = 0;
                uae_s32 base = addr;
                TCHAR name[10];
                _stprintf (name, _T("PC, "));
                if (dp & 0x80) { base = 0; name[0] = 0; }
                if (dp & 0x40) dispreg = 0;
                if ((dp & 0x30) == 0x20) { disp = (uae_s32)(uae_s16)get_iword_debug (pc); pc += 2; }
                if ((dp & 0x30) == 0x30) { disp = get_ilong_debug (pc); pc += 4; }
                base += disp;
                
                if ((dp & 0x3) == 0x2) { outer = (uae_s32)(uae_s16)get_iword_debug (pc); pc += 2; }
                if ((dp & 0x3) == 0x3) { outer = get_ilong_debug (pc); pc += 4; }
                
                if (!(dp & 4)) base += dispreg;
                if ((dp & 3) && !safemode) base = get_ilong_debug (base);
                if (dp & 4) base += dispreg;
                
                addr = base + outer;
                _stprintf (buffer, _T("(%s%c%d.%c*%d+%d)+%d == $%08x"), name,
                           dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W',
                           1 << ((dp >> 9) & 3),
                           disp, outer, addr);
            } else {
                addr += (uae_s32)((uae_s8)disp8) + dispreg;
                _stprintf (buffer, _T("(PC, %c%d.%c*%d, $%02x) == $%08x"), dp & 0x8000 ? 'A' : 'D',
                           (int)r, dp & 0x800 ? 'L' : 'W',  1 << ((dp >> 9) & 3),
                           disp8, addr);
            }
            break;
        case absw:
            addr = (uae_s32)(uae_s16)get_iword_debug (pc);
            _stprintf (buffer, _T("$%08x"), addr);
            pc += 2;
            break;
        case absl:
            addr = get_ilong_debug (pc);
            _stprintf (buffer, _T("$%08x"), addr);
            pc += 4;
            break;
        case imm:
            switch (size){
                case sz_byte:
                    _stprintf (buffer, _T("#$%02x"), (get_iword_debug (pc) & 0xff));
                    pc += 2;
                    break;
                case sz_word:
                    _stprintf (buffer, _T("#$%04x"), (get_iword_debug (pc) & 0xffff));
                    pc += 2;
                    break;
                case sz_long:
                    _stprintf(buffer, _T("#$%08x"), (get_ilong_debug(pc)));
                    pc += 4;
                    break;
                case sz_single:
                {
                    fptype fp;
                    to_single(&fp, get_ilong_debug(pc));
                    _stprintf(buffer, _T("#%s"), fp_print(&fp));
                    pc += 4;
                }
                    break;
                case sz_double:
                {
                    fptype fp;
                    to_double(&fp, get_ilong_debug(pc), get_ilong_debug(pc + 4));
                    _stprintf(buffer, _T("#%s"), fp_print(&fp));
                    pc += 8;
                }
                    break;
                case sz_extended:
                {
                    fptype fp;
                    to_exten(&fp, get_ilong_debug(pc), get_ilong_debug(pc + 4), get_ilong_debug(pc + 8));
                    _stprintf(buffer, _T("#%s"), fp_print(&fp));
                    pc += 12;
                    break;
                }
                case sz_packed:
                    _stprintf(buffer, _T("#$%08x%08x%08x"), get_ilong_debug(pc), get_ilong_debug(pc + 4), get_ilong_debug(pc + 8));
                    pc += 12;
                    break;
                default:
                    break;
            }
            break;
        case imm0:
            offset = (uae_s32)(uae_s8)get_iword_debug (pc);
            _stprintf (buffer, _T("#$%02x"), (uae_u32)(offset & 0xff));
            addr = pc + 2 + offset;
            pc += 2;
            break;
        case imm1:
            offset = (uae_s32)(uae_s16)get_iword_debug (pc);
            buffer[0] = 0;
            _stprintf (buffer, _T("#$%04x"), (uae_u32)(offset & 0xffff));
            addr = pc + offset;
            pc += 2;
            break;
        case imm2:
            offset = (uae_s32)get_ilong_debug (pc);
            _stprintf (buffer, _T("#$%08x"), (uae_u32)offset);
            addr = pc + offset;
            pc += 4;
            break;
        case immi:
            offset = (uae_s32)(uae_s8)(reg & 0xff);
            _stprintf (buffer, _T("#$%08x"), (uae_u32)offset);
            addr = pc + offset;
            break;
        default:
            break;
    }
    if (buf == 0)
        f_out (f, _T("%s"), buffer);
    else
        _tcscat (buf, buffer);
    if (eaddr)
        *eaddr = addr;
    return pc;
}

void REGPARAM2 MakeSR (void)
{
	regs.sr = ((regs.t1 << 15) | (regs.t0 << 14)
		| (regs.s << 13) | (regs.m << 12) | (regs.intmask << 8)
		| (GET_XFLG () << 4) | (GET_NFLG () << 3)
		| (GET_ZFLG () << 2) | (GET_VFLG () << 1)
		|  GET_CFLG ());
}

void REGPARAM2 MakeFromSR (void)
{
	int oldm = regs.m;
	int olds = regs.s;

	SET_XFLG ((regs.sr >> 4) & 1);
	SET_NFLG ((regs.sr >> 3) & 1);
	SET_ZFLG ((regs.sr >> 2) & 1);
	SET_VFLG ((regs.sr >> 1) & 1);
	SET_CFLG (regs.sr & 1);
	if (regs.t1 == ((regs.sr >> 15) & 1) &&
		regs.t0 == ((regs.sr >> 14) & 1) &&
		regs.s  == ((regs.sr >> 13) & 1) &&
		regs.m  == ((regs.sr >> 12) & 1) &&
		regs.intmask == ((regs.sr >> 8) & 7))
		return;
	regs.t1 = (regs.sr >> 15) & 1;
	regs.t0 = (regs.sr >> 14) & 1;
	regs.s  = (regs.sr >> 13) & 1;
	regs.m  = (regs.sr >> 12) & 1;
	regs.intmask = (regs.sr >> 8) & 7;
	if (currprefs.cpu_model >= 68020) {
		if (olds != regs.s) {
			if (olds) {
				if (oldm)
					regs.msp = m68k_areg (regs, 7);
				else
					regs.isp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.usp;
			} else {
				regs.usp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.m ? regs.msp : regs.isp;
			}
		} else if (olds && oldm != regs.m) {
			if (oldm) {
				regs.msp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.isp;
			} else {
				regs.isp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.msp;
			}
		}
	} else {
		regs.t0 = regs.m = 0;
		if (olds != regs.s) {
			if (olds) {
				regs.isp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.usp;
			} else {
				regs.usp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.isp;
			}
		}
	}
	if (currprefs.mmu_model)
		mmu_set_super (regs.s != 0);

	doint ();
	if (regs.t1 || regs.t0)
		set_special (SPCFLAG_TRACE);
	else
		/* Keep SPCFLAG_DOTRACE, we still want a trace exception for
		SR-modifying instructions (including STOP).  */
		unset_special (SPCFLAG_TRACE);
}

static void exception_check_trace (int nr)
{
    unset_special (SPCFLAG_TRACE | SPCFLAG_DOTRACE);
    if (regs.t1 && !regs.t0) {
        /* trace stays pending if exception is div by zero, chk,
         * trapv or trap #x
         */
        if (nr == 5 || nr == 6 || nr == 7 || (nr >= 32 && nr <= 47))
            set_special (SPCFLAG_DOTRACE);
    }
    regs.t1 = regs.t0 = 0;
}

static void exception_debug (int nr)
{
#ifdef DEBUGGER
	if (!exception_debugging)
		return;
	printf ("Exception %d, PC=%08X\n", nr, M68K_GETPC);
    DebugUI();
#endif
}

void cpu_halt (int e)
{
	write_log("HALT!");
    DebugUI();
}

static void Exception_build_stack_frame (uae_u32 oldpc, uae_u32 currpc, uae_u32 ssw, int nr, int format)
{
    int i;
   
#if 0
    if (nr < 24 || nr > 31) { // do not print debugging for interrupts
        write_log(_T("Building exception stack frame (format %X)\n"), format);
    }
#endif

    switch (format) {
        case 0x0: // four word stack frame
        case 0x1: // throwaway four word stack frame
            break;
        case 0x2: // six word stack frame
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), oldpc);
            break;
        case 0x3: // floating point post-instruction stack frame (68040)
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.fp_ea);
            break;
        case 0x7: // access error stack frame (68040)

			for (i = 3; i >= 0; i--) {
				// WB1D/PD0,PD1,PD2,PD3
                m68k_areg (regs, 7) -= 4;
                x_put_long (m68k_areg (regs, 7), mmu040_move16[i]);
			}

            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), 0); // WB1A
			m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), 0); // WB2D
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.wb2_address); // WB2A
			m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.wb3_data); // WB3D
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr); // WB3A

			m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr); // FA
            
			m68k_areg (regs, 7) -= 2;
            x_put_word (m68k_areg (regs, 7), 0);
            m68k_areg (regs, 7) -= 2;
            x_put_word (m68k_areg (regs, 7), regs.wb2_status);
            regs.wb2_status = 0;
            m68k_areg (regs, 7) -= 2;
            x_put_word (m68k_areg (regs, 7), regs.wb3_status);
            regs.wb3_status = 0;

			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), ssw);
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.mmu_effective_addr);
            break;
        case 0x9: // coprocessor mid-instruction stack frame (68020, 68030)
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.fp_ea);
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.fp_opword);
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), oldpc);
            break;
        case 0x8: // bus and address error stack frame (68010)
            write_log(_T("Exception stack frame format %X not implemented\n"), format);
            return;
        case 0x4: // floating point unimplemented stack frame (68LC040, 68EC040)
				// or 68060 bus access fault stack frame
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), ssw);
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), oldpc);
			break;
		case 0xB: // long bus cycle fault stack frame (68020, 68030)
			// We always use B frame because it is easier to emulate,
			// our PC always points at start of instruction but A frame assumes
			// it is + 2 and handling this properly is not easy.
			// Store state information to internal register space
			for (i = 0; i < mmu030_idx + 1; i++) {
				m68k_areg (regs, 7) -= 4;
				x_put_long (m68k_areg (regs, 7), mmu030_ad[i].val);
			}
			while (i < 9) {
                uae_u32 v = 0;
				m68k_areg (regs, 7) -= 4;
                // mmu030_idx is always small enough if instruction is FMOVEM.
                if (mmu030_state[1] & MMU030_STATEFLAG1_FMOVEM) {
                    if (i == 7)
                        v = mmu030_fmovem_store[0];
                    else if (i == 8)
                        v = mmu030_fmovem_store[1];
                }
                x_put_long (m68k_areg (regs, 7), v);
                i++;
			}
			 // version & internal information (We store index here)
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), mmu030_idx);
			// 3* internal registers
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), mmu030_state[2]);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), mmu030_state[1]);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), mmu030_state[0]);
			// data input buffer = fault address
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr);
			// 2xinternal
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);
			// stage b address
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), mm030_stageb_address);
			// 2xinternal
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), mmu030_disp_store[1]);
		/* fall through */
		case 0xA: // short bus cycle fault stack frame (68020, 68030)
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), mmu030_disp_store[0]);
			m68k_areg (regs, 7) -= 4;
			 // Data output buffer = value that was going to be written
			x_put_long (m68k_areg (regs, 7), (mmu030_state[1] & MMU030_STATEFLAG1_MOVEM1) ? mmu030_data_buffer : mmu030_ad[mmu030_idx].val);
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), mmu030_opcode);  // Internal register (opcode storage)
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr); // data cycle fault address
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);  // Instr. pipe stage B
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);  // Instr. pipe stage C
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), ssw);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);  // Internal register
			break;
		default:
            write_log(_T("Unknown exception stack frame format: %X\n"), format);
            return;
    }
    m68k_areg (regs, 7) -= 2;
    x_put_word (m68k_areg (regs, 7), (format << 12) | (nr * 4));
    m68k_areg (regs, 7) -= 4;
    x_put_long (m68k_areg (regs, 7), currpc);
    m68k_areg (regs, 7) -= 2;
    x_put_word (m68k_areg (regs, 7), regs.sr);
}

// 68030 MMU
static void Exception_mmu030 (int nr, uaecptr oldpc)
{
    uae_u32 currpc = m68k_getpc (), newpc;
    
    exception_debug (nr);
    MakeSR ();
    
    if (!regs.s) {
        regs.usp = m68k_areg (regs, 7);
        m68k_areg(regs, 7) = regs.m ? regs.msp : regs.isp;
        regs.s = 1;
        mmu_set_super (1);
    }
 
#if 0
    if (nr < 24 || nr > 31) { // do not print debugging for interrupts
        write_log (_T("Exception_mmu030: Exception %i: %08x %08x %08x\n"),
                   nr, currpc, oldpc, regs.mmu_fault_addr);
    }
#endif

#if 0
	write_log (_T("Exception %d -> %08x\n", nr, newpc));
#endif


    newpc = x_get_long (regs.vbr + 4 * nr);

	if (regs.m && nr >= 24 && nr < 32) { /* M + Interrupt */
        Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x0);
        MakeSR(); /* this sets supervisor bit in status reg */
        regs.m = 0; /* clear the M bit (but frame 0x1 uses sr with M bit set) */
        regs.msp = m68k_areg (regs, 7);
        m68k_areg (regs, 7) = regs.isp;
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr, 0x1);
    } else if (nr ==5 || nr == 6 || nr == 7 || nr == 9 || nr == 56) {
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr, 0x2);
    } else if (nr == 2) {
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr,  0xB);
    } else if (nr == 3) {
		regs.mmu_fault_addr = last_fault_for_exception_3;
		mmu030_state[0] = mmu030_state[1] = 0;
		mmu030_data_buffer = 0;
        Exception_build_stack_frame (last_fault_for_exception_3, currpc, MMU030_SSW_RW | MMU030_SSW_SIZE_W | (regs.s ? 6 : 2), nr,  0xA);
    } else if (nr >= 48 && nr < 55) {
        if (regs.fpu_exp_pre) {
            Exception_build_stack_frame(oldpc, regs.instruction_pc, 0, nr, 0x0);
        } else { /* mid-instruction */
            Exception_build_stack_frame(regs.fpiar, currpc, 0, nr, 0x9);
        }
    } else {
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr, 0x0);
    }
    
	if (newpc & 1) {
		if (nr == 2 || nr == 3)
			cpu_halt (2);
		else
			exception3_read(regs.ir, newpc);
		return;
	}
	m68k_setpci (newpc);
	exception_check_trace (nr);
}


// 68040 MMU
static void Exception_mmu (int nr, uaecptr oldpc)
{
	uae_u32 currpc = m68k_getpc (), newpc;

	exception_debug (nr);
	MakeSR ();

	if (!regs.s) {
		regs.usp = m68k_areg (regs, 7);
        if (currprefs.cpu_model >= 68020 && currprefs.cpu_model < 68060) {
			m68k_areg (regs, 7) = regs.m ? regs.msp : regs.isp;
		} else {
			m68k_areg (regs, 7) = regs.isp;
		}
		regs.s = 1;
		mmu_set_super (1);
	}
    
	newpc = x_get_long (regs.vbr + 4 * nr);
#if 0
	write_log (_T("Exception %d: %08x -> %08x\n"), nr, currpc, newpc);
#endif

	if (nr == 2) { // bus error
        //write_log (_T("Exception_mmu %08x %08x %08x\n"), currpc, oldpc, regs.mmu_fault_addr);
        if (currprefs.mmu_model == 68040)
			Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x7);
		else
			Exception_build_stack_frame(regs.mmu_fault_addr, currpc, regs.mmu_fslw, nr, 0x4);
	} else if (nr == 3) { // address error
        Exception_build_stack_frame(last_fault_for_exception_3, currpc, 0, nr, 0x2);
		write_log (_T("Exception %d (%x) at %x!\n"), nr, last_fault_for_exception_3, currpc);
	} else if (nr == 5 || nr == 6 || nr == 7 || nr == 9) {
        Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x2);
	} else if (regs.m && nr >= 24 && nr < 32) { /* M + Interrupt */
        Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x0);
        MakeSR(); /* this sets supervisor bit in status reg */
        regs.m = 0; /* clear the M bit (but frame 0x1 uses sr with M bit set) */
        regs.msp = m68k_areg (regs, 7);
        m68k_areg (regs, 7) = regs.isp;
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr, 0x1);
	} else if (nr == 60 || nr == 61) {
        Exception_build_stack_frame(oldpc, regs.instruction_pc, regs.mmu_ssw, nr, 0x0);
    } else if (nr >= 48 && nr <= 55) {
        if (regs.fpu_exp_pre) {
            if (currprefs.cpu_model == 68060 && nr == 55 && regs.fp_unimp_pend == 2) { // packed decimal real
                Exception_build_stack_frame(regs.fp_ea, regs.instruction_pc, 0, nr, 0x2);
            } else {
                Exception_build_stack_frame(oldpc, regs.instruction_pc, 0, nr, 0x0);
            }
        } else { /* post-instruction */
            if (currprefs.cpu_model == 68060 && nr == 55 && regs.fp_unimp_pend == 2) { // packed decimal real
                Exception_build_stack_frame(regs.fp_ea, currpc, 0, nr, 0x2);
            } else {
                Exception_build_stack_frame(oldpc, currpc, 0, nr, 0x3);
            }
        }
    } else if (nr == 11 && regs.fp_unimp_ins) {
        regs.fp_unimp_ins = false;
        if ((currprefs.cpu_model == 68060 && (currprefs.fpu_model == 0 || (regs.pcr & 2))) ||
            (currprefs.cpu_model == 68040 && currprefs.fpu_model == 0)) {
            Exception_build_stack_frame(regs.fp_ea, currpc, regs.instruction_pc, nr, 0x4);
        } else {
            Exception_build_stack_frame(regs.fp_ea, currpc, regs.mmu_ssw, nr, 0x2);
        }
	} else {
        Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x0);
	}
    
	if (newpc & 1) {
		if (nr == 2 || nr == 3)
			cpu_halt (2);
		else
			exception3_read (regs.ir, newpc);
		return;
	}
	m68k_setpci (newpc);
	exception_check_trace (nr);
}


/* Handle exceptions. */
static void ExceptionX (int nr, uaecptr address)
{
    if (currprefs.cpu_model == 68030)
        Exception_mmu030 (nr, m68k_getpc ());
    else
        Exception_mmu (nr, m68k_getpc ());
}

void REGPARAM2 Exception (int nr)
{
    ExceptionX (nr, -1);
}
void REGPARAM2 ExceptionL (int nr, uaecptr address)
{
    ExceptionX (nr, address);
}

static void do_interrupt (int nr, int Pending)
{
	regs.stopped = 0;
	unset_special (SPCFLAG_STOP);
	assert (nr < 8 && nr >= 0);

	/* On Hatari, only video ints are using SPCFLAG_INT (see m68000.c) */
	Exception (nr + 24);

	regs.intmask = nr;
	doint ();

	set_special (SPCFLAG_INT);
}


void m68k_reset (int hardreset)
{
    regs.spcflags &= (SPCFLAG_MODE_CHANGE | SPCFLAG_BRK);
	regs.ipl = regs.ipl_pin = 0;
	regs.s = 1;
	regs.m = 0;
	regs.stopped = 0;
	regs.t1 = 0;
	regs.t0 = 0;
	SET_ZFLG (0);
	SET_XFLG (0);
	SET_CFLG (0);
	SET_VFLG (0);
	SET_NFLG (0);
	regs.intmask = 7;
	regs.vbr = regs.sfc = regs.dfc = 0;
	regs.irc = 0xffff;
	
	m68k_areg (regs, 7) = get_long (0);
	m68k_setpc (get_long (4));

	fpu_reset ();

	regs.caar = regs.cacr = 0;
	regs.itt0 = regs.itt1 = regs.dtt0 = regs.dtt1 = 0;
	regs.tcr = regs.mmusr = regs.urp = regs.srp = regs.buscr = 0;

	mmufixup[0].reg = -1;
	mmufixup[1].reg = -1;
	if (currprefs.mmu_model) {
        if (currprefs.cpu_model >= 68040) {
            mmu_reset ();
            mmu_set_tc (regs.tcr);
            mmu_set_super (regs.s != 0);
        }
        else {
            mmu030_reset(hardreset);
        }
    }

	regs.pcr = 0;
}

void REGPARAM2 op_unimpl (uae_u16 opcode)
{
    static int warned;
    if (warned < 20) {
        write_log (_T("68060 unimplemented opcode %04X, PC=%08x\n"), opcode, regs.instruction_pc);
        warned++;
    }
    ExceptionL (61, regs.instruction_pc);
}

uae_u32 REGPARAM2 op_illg (uae_u32 opcode)
{
	uaecptr pc = m68k_getpc ();
	static int warned;

	if (warned < 20) {
		write_log ("Illegal instruction: %04x at %08X\n", opcode, pc);
		warned++;
	}

	if ((opcode & 0xF000)==0xA000)
		Exception (10);
	else 
	if ((opcode & 0xF000)==0xF000)
		Exception (11);
	else
		Exception (4);
	return 4;
}

#ifdef CPUEMU_0

// 68030 (68851) MMU instructions only
bool mmu_op30 (uaecptr pc, uae_u32 opcode, uae_u16 extra, uaecptr extraa)
{
    if (extra & 0x8000) {
        return mmu_op30_ptest (pc, opcode, extra, extraa);
    } else if ((extra&0xE000)==0x2000 && (extra & 0x1C00)) {
        return mmu_op30_pflush (pc, opcode, extra, extraa);
    } else if ((extra&0xE000)==0x2000 && !(extra & 0x1C00)) {
        return mmu_op30_pload (pc, opcode, extra, extraa);
    } else {
        return mmu_op30_pmove (pc, opcode, extra, extraa);
    }
    return false;
}

void mmu_op (uae_u32 opcode, uae_u32 extra)
{
	if (currprefs.cpu_model) {
		mmu_op_real (opcode, extra);
		return;
	}
#if MMUOP_DEBUG > 1
	write_log ("mmu_op %04X PC=%08X\n", opcode, m68k_getpc ());
#endif
	if ((opcode & 0xFE0) == 0x0500) {
		/* PFLUSH */
		regs.mmusr = 0;
#if MMUOP_DEBUG > 0
		write_log ("PFLUSH\n");
#endif
		return;
	} else if ((opcode & 0x0FD8) == 0x548) {
		if (currprefs.cpu_model < 68060) { /* PTEST not in 68060 */
			/* PTEST */
#if MMUOP_DEBUG > 0
			write_log ("PTEST\n");
#endif
			return;
		}
	}
#if MMUOP_DEBUG > 0
	write_log ("Unknown MMU OP %04X\n", opcode);
#endif
	m68k_setpc (m68k_getpc () - 2);
	op_illg (opcode);
}

#endif

static uaecptr last_trace_ad = 0;

static void do_trace (void)
{
	if (regs.t0 && currprefs.cpu_model >= 68020) {
		uae_u16 opcode;
		/* should also include TRAP, CHK, SR modification FPcc */
		/* probably never used so why bother */
		/* We can afford this to be inefficient... */
		m68k_setpc (m68k_getpc ());
		opcode = x_get_word (regs.pc);
		if (opcode == 0x4e73 			/* RTE */
			|| opcode == 0x4e74 		/* RTD */
			|| opcode == 0x4e75 		/* RTS */
			|| opcode == 0x4e77 		/* RTR */
			|| opcode == 0x4e76 		/* TRAPV */
			|| (opcode & 0xffc0) == 0x4e80 	/* JSR */
			|| (opcode & 0xffc0) == 0x4ec0 	/* JMP */
			|| (opcode & 0xff00) == 0x6100	/* BSR */
			|| ((opcode & 0xf000) == 0x6000	/* Bcc */
			&& cctrue ((opcode >> 8) & 0xf))
			|| ((opcode & 0xf0f0) == 0x5050	/* DBcc */
			&& !cctrue ((opcode >> 8) & 0xf)
			&& (uae_s16)m68k_dreg (regs, opcode & 7) != 0))
		{
			last_trace_ad = m68k_getpc ();
			unset_special (SPCFLAG_TRACE);
			set_special (SPCFLAG_DOTRACE);
		}
	} else if (regs.t1) {
		last_trace_ad = m68k_getpc ();
		unset_special (SPCFLAG_TRACE);
		set_special (SPCFLAG_DOTRACE);
	}
}

void doint (void)
{
	if (currprefs.cpu_compatible)
		set_special (SPCFLAG_INT);
	else
		set_special (SPCFLAG_DOINT);
}

#define IDLETIME (currprefs.cpu_idle * sleep_resolution / 700)

/*
 * Handle special flags
 */

static int do_specialties (int cycles)
{
	if (regs.spcflags & SPCFLAG_DOTRACE)
		Exception (9);

    /* Handle the STOP instruction */
    if ( regs.spcflags & SPCFLAG_STOP ) {
        while (regs.spcflags & SPCFLAG_STOP) {

            /* Take care of quit event if needed */
            if (regs.spcflags & SPCFLAG_BRK)
                return 1;
        
            M68000_AddCycles(cpu_cycles);

            /* It is possible one or more ints happen at the same time */
            /* We must process them during the same cpu cycle until the special INT flag is set */
            while (PendingInterrupt.time <=0 && PendingInterrupt.pFunction) {
                /* 1st, we call the interrupt handler */
                CALL_VAR(PendingInterrupt.pFunction);
                
                if ((regs.spcflags & (SPCFLAG_BRK | SPCFLAG_MODE_CHANGE))) {
                    unset_special (SPCFLAG_BRK | SPCFLAG_MODE_CHANGE);
                    // SPCFLAG_BRK breaks STOP condition, need to prefetch
                    m68k_resumestopped ();
                    return 1;
                }

                if (currprefs.cpu_idle && ((regs.spcflags & SPCFLAG_STOP)) == SPCFLAG_STOP) {
                    /* sleep 1ms if STOP-instruction is executed */
                    if (1) {
                        static int sleepcnt, lvpos;
                        if (vpos != lvpos) {
                            sleepcnt--;
                            lvpos = vpos;
                            if (sleepcnt < 0) {
                                    /*sleepcnt = IDLETIME / 2; */  /* Laurent : badly removed for now */
                                host_sleep_ms(1);
                            }
                        }
                    }
                }
            }
        }
	}

	if (regs.spcflags & SPCFLAG_TRACE)
		do_trace ();

	if (regs.spcflags & SPCFLAG_DOINT) {
		unset_special (SPCFLAG_DOINT);
		set_special (SPCFLAG_INT);
	}

    if (regs.spcflags & SPCFLAG_DEBUGGER)
		DebugCpu_Check();

	if ((regs.spcflags & (SPCFLAG_BRK | SPCFLAG_MODE_CHANGE))) {
		unset_special(SPCFLAG_MODE_CHANGE);
		return 1;
	}
    
	return 0;
}

#ifndef CPUEMU_0

#else

static int lastRegsS = 0;

// Previous MMU 68030
static void m68k_run_mmu030 (void)
{
	uae_u16 opcode;
	uaecptr pc;
	struct flag_struct f;
	f.cznv = 0;
	f.x    = 0;
    int intr             = 0;
    int lastintr         = 0;
	mmu030_opcode_stageb = -1;
retry:
	TRY (prb) {
		for (;;) {
			int cnt;
insretry:
			pc = regs.instruction_pc = m68k_getpc ();
			f.cznv = regflags.cznv;
			f.x    = regflags.x;
            
			mmu030_state[0] = mmu030_state[1] = mmu030_state[2] = 0;
			mmu030_opcode = -1;
			if (mmu030_opcode_stageb < 0) {
				opcode = get_iword_mmu030 (0);
			} else {
				opcode = mmu030_opcode_stageb;
				mmu030_opcode_stageb = -1;
			}

			mmu030_opcode = opcode;
			mmu030_ad[0].done = false;

            Uint64 beforeCycles = nCyclesMainCounter;
			cnt = 50;
			for (;;) {
				opcode = mmu030_opcode;
				mmu030_idx = 0;
				mmu030_retry = false;
				cpu_cycles = (*cpufunctbl[opcode])(opcode);
				cnt--; // so that we don't get in infinite loop if things go horribly wrong
				if (!mmu030_retry)
					break;
				if (cnt < 0) {
					cpu_halt (9);
					break;
				}
				if (mmu030_retry && mmu030_opcode == -1)
					goto insretry; // urgh
			}

			mmu030_opcode = -1;
            
            M68000_AddCycles(cpu_cycles);
            cpu_cycles = nCyclesMainCounter - beforeCycles;
            
			DSP_Run(cpu_cycles);
            i860_Run(cpu_cycles);

			/* We can have several interrupts at the same time before the next CPU instruction */
			/* We must check for pending interrupt and call do_specialties_interrupt() only */
			/* if the cpu is not in the STOP state. Else, the int could be acknowledged now */
			/* and prevent exiting the STOP state when calling do_specialties() after. */
			/* For performance, we first test PendingInterruptCount, then regs.spcflags */
			while ( ( PendingInterrupt.time <= 0 ) && ( PendingInterrupt.pFunction ) && ( ( regs.spcflags & SPCFLAG_STOP ) == 0 ) ) {
				CALL_VAR(PendingInterrupt.pFunction);		/* call the interrupt handler */
			}

            /* Previous: for now we poll the interrupt pins with every instruction.
             * TODO: only do this when an actual interrupt is active to not
             * unneccessarily slow down emulation.
             */
            intr = intlev ();
            if (intr>regs.intmask || (intr==7 && intr>lastintr))
                do_interrupt (intr, false);
            lastintr = intr;
            
            if(lastRegsS != regs.s) {
                host_realtime(!(regs.s));
                lastRegsS = regs.s;
            }

            if (regs.spcflags & ~SPCFLAG_INT) {
				if (do_specialties (cpu_cycles))
					return;
			}
		}
	} CATCH (prb) {

		regflags.cznv = f.cznv;
		regflags.x    = f.x;

		m68k_setpci (regs.instruction_pc);

		if (mmufixup[0].reg >= 0) {
			m68k_areg (regs, mmufixup[0].reg) = mmufixup[0].value;
			mmufixup[0].reg = -1;
		}
		if (mmufixup[1].reg >= 0) {
			m68k_areg (regs, mmufixup[1].reg) = mmufixup[1].value;
			mmufixup[1].reg = -1;
		}

		TRY (prb2) {
			Exception (prb);
		} CATCH (prb2) {
            write_log("[FATAL] double fault\n");
            m68k_reset (1); // auto-reset CPU
			//cpu_halt (1);
			return;
		} ENDTRY
	} ENDTRY
    goto retry;
}

/* Aranym MMU 68040  */
static void m68k_run_mmu040 (void)
{
	uae_u16 opcode;
	struct flag_struct f;
	f.cznv = 0;
	f.x    = 0;
	uaecptr pc;
    int intr = 0;
    int lastintr = 0;
	
	for (;;) {
	TRY (prb) {
		for (;;) {
			f.cznv = regflags.cznv;
			f.x = regflags.x;
			mmu_restart = true;
			pc = regs.instruction_pc = m68k_getpc ();
        
            Uint64 beforeCycles = nCyclesMainCounter;
			mmu_opcode = -1;
			mmu_opcode = opcode = x_prefetch (0);
			cpu_cycles = (*cpufunctbl[opcode])(opcode);
            M68000_AddCycles(cpu_cycles);
            
            cpu_cycles = nCyclesMainCounter - beforeCycles;

			DSP_Run(cpu_cycles);
            i860_Run(cpu_cycles);

			/* We can have several interrupts at the same time before the next CPU instruction */
			/* We must check for pending interrupt and call do_specialties_interrupt() only */
			/* if the cpu is not in the STOP state. Else, the int could be acknowledged now */
			/* and prevent exiting the STOP state when calling do_specialties() after. */
			/* For performance, we first test PendingInterruptCount, then regs.spcflags */
			while ( ( PendingInterrupt.time <= 0 ) && ( PendingInterrupt.pFunction ) && ( ( regs.spcflags & SPCFLAG_STOP ) == 0 ) ) {
				CALL_VAR(PendingInterrupt.pFunction);		/* call the interrupt handler */
			}

            /* Previous: for now we poll the interrupt pins with every instruction.
             * TODO: only do this when an actual interrupt is active to not
             * unneccessarily slow down emulation.
             */
            intr = intlev ();
            if (intr>regs.intmask || (intr==7 && intr>lastintr))
                do_interrupt (intr, false);
            lastintr = intr;
            
            if(lastRegsS != regs.s) {
                host_realtime(!(regs.s));
                lastRegsS = regs.s;
            }
            
			if (regs.spcflags & ~SPCFLAG_INT) {
				if (do_specialties (cpu_cycles))
					return;
			}
		} // end of for(;;)
	} CATCH (prb) {

		if (mmu_restart) {
			/* restore state if instruction restart */
			regflags.cznv = f.cznv;
			regflags.x = f.x;
			m68k_setpci (regs.instruction_pc);
		}

		if (mmufixup[0].reg >= 0) {
			m68k_areg (regs, mmufixup[0].reg) = mmufixup[0].value;
			mmufixup[0].reg = -1;
		}

		TRY (prb2) {
			Exception (prb);
		} CATCH (prb2) {
            write_log("[FATAL] double fault\n");
            m68k_reset (1); // auto-reset CPU
            //cpu_halt (1);
            return;
		} ENDTRY

	} ENDTRY
	}
}

#endif /* CPUEMU_0 */

int in_m68k_go = 0;

#if 0
static void exception2_handle (uaecptr addr, uaecptr fault)
{
	last_addr_for_exception_3 = addr;
	last_fault_for_exception_3 = fault;
	last_writeaccess_for_exception_3 = 0;
	last_instructionaccess_for_exception_3 = 0;
	Exception (2);
}
#endif

void m68k_go (int may_quit)
{
	int hardboot = 1;

	if (in_m68k_go || !may_quit) {
		write_log ("Bug! m68k_go is not reentrant.\n");
        DebugUI();
	}
    
	in_m68k_go++;
	for (;;) {
		void (*run_func)(void);
		
		if (regs.spcflags & SPCFLAG_BRK) {
			unset_special(SPCFLAG_BRK);
				break;
		}

			quit_program = 0;
			hardboot = 0;

#ifdef DEBUGGER
		if (debugging)
			debug ();
#endif
        
	set_x_funcs ();
        run_func=currprefs.cpu_model == 68040 ? m68k_run_mmu040 : m68k_run_mmu030;
		run_func ();
	}
	in_m68k_go--;
}


#if 1
TCHAR* buf_out (TCHAR *buffer, int *bufsize, const TCHAR *format, ...)
{
    va_list parms;
    
    if (buffer == NULL)
    {
        return NULL;
    }
    
    va_start (parms, format);
    vsnprintf (buffer, (*bufsize) - 1, format, parms);
    va_end (parms);
    *bufsize -= _tcslen (buffer);
    
    return buffer + _tcslen (buffer);
}
#endif

static const TCHAR *ccnames[] =
{
    _T("T "),_T("F "),_T("HI"),_T("LS"),_T("CC"),_T("CS"),_T("NE"),_T("EQ"),
    _T("VC"),_T("VS"),_T("PL"),_T("MI"),_T("GE"),_T("LT"),_T("GT"),_T("LE")
};
static const TCHAR *fpccnames[] =
{
    _T("F"),
    _T("EQ"),
    _T("OGT"),
    _T("OGE"),
    _T("OLT"),
    _T("OLE"),
    _T("OGL"),
    _T("OR"),
    _T("UN"),
    _T("UEQ"),
    _T("UGT"),
    _T("UGE"),
    _T("ULT"),
    _T("ULE"),
    _T("NE"),
    _T("T"),
    _T("SF"),
    _T("SEQ"),
    _T("GT"),
    _T("GE"),
    _T("LT"),
    _T("LE"),
    _T("GL"),
    _T("GLE"),
    _T("NGLE"),
    _T("NGL"),
    _T("NLE"),
    _T("NLT"),
    _T("NGE"),
    _T("NGT"),
    _T("SNE"),
    _T("ST")
};
static const TCHAR *fpuopcodes[] =
{
    _T("FMOVE"),
    _T("FINT"),
    _T("FSINH"),
    _T("FINTRZ"),
    _T("FSQRT"),
    NULL,
    _T("FLOGNP1"),
    NULL,
    _T("FETOXM1"),
    _T("FTANH"),
    _T("FATAN"),
    NULL,
    _T("FASIN"),
    _T("FATANH"),
    _T("FSIN"),
    _T("FTAN"),
    _T("FETOX"),	// 0x10
    _T("FTWOTOX"),
    _T("FTENTOX"),
    NULL,
    _T("FLOGN"),
    _T("FLOG10"),
    _T("FLOG2"),
    NULL,
    _T("FABS"),
    _T("FCOSH"),
    _T("FNEG"),
    NULL,
    _T("FACOS"),
    _T("FCOS"),
    _T("FGETEXP"),
    _T("FGETMAN"),
    _T("FDIV"),		// 0x20
    _T("FMOD"),
    _T("FADD"),
    _T("FMUL"),
    _T("FSGLDIV"),
    _T("FREM"),
    _T("FSCALE"),
    _T("FSGLMUL"),
    _T("FSUB"),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    _T("FSINCOS"),	// 0x30
    _T("FSINCOS"),
    _T("FSINCOS"),
    _T("FSINCOS"),
    _T("FSINCOS"),
    _T("FSINCOS"),
    _T("FSINCOS"),
    _T("FSINCOS"),
    _T("FCMP"),
    NULL,
    _T("FTST"),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static const TCHAR *movemregs[] =
{
    _T("D0"),
    _T("D1"),
    _T("D2"),
    _T("D3"),
    _T("D4"),
    _T("D5"),
    _T("D6"),
    _T("D7"),
    _T("A0"),
    _T("A1"),
    _T("A2"),
    _T("A3"),
    _T("A4"),
    _T("A5"),
    _T("A6"),
    _T("A7"),
    _T("FP0"),
    _T("FP1"),
    _T("FP2"),
    _T("FP3"),
    _T("FP4"),
    _T("FP5"),
    _T("FP6"),
    _T("FP7"),
    _T("FPIAR"),
    _T("FPSR"),
    _T("FPCR")
};

static void addmovemreg (TCHAR *out, int *prevreg, int *lastreg, int *first, int reg, int fpmode)
{
    TCHAR *p = out + _tcslen (out);
    if (*prevreg < 0) {
        *prevreg = reg;
        *lastreg = reg;
        return;
    }
    if (reg < 0 || fpmode == 2 || (*prevreg) + 1 != reg || (reg & 8) != ((*prevreg & 8))) {
        _stprintf (p, _T("%s%s"), (*first) ? _T("") : _T("/"), movemregs[*lastreg]);
        p = p + _tcslen (p);
        if (*lastreg != *prevreg) {
            if ((*lastreg) + 2 == reg) {
                _stprintf(p, _T("/%s"), movemregs[*prevreg]);
            } else if ((*lastreg) != (*prevreg)) {
                _stprintf(p, _T("-%s"), movemregs[*prevreg]);
            }
        }
        *lastreg = reg;
        *first = 0;
    }
    *prevreg = reg;
}

static void movemout (TCHAR *out, uae_u16 mask, int mode, int fpmode)
{
    unsigned int dmask, amask;
    int prevreg = -1, lastreg = -1, first = 1;
    int i;
    
    if (mode == Apdi && !fpmode) {
        uae_u8 dmask2;
        uae_u8 amask2;
        
        amask2 = mask & 0xff;
        dmask2 = (mask >> 8) & 0xff;
        dmask = 0;
        amask = 0;
        for (i = 0; i < 8; i++) {
            if (dmask2 & (1 << i))
                dmask |= 1 << (7 - i);
            if (amask2 & (1 << i))
                amask |= 1 << (7 - i);
        }
    } else {
        dmask = mask & 0xff;
        amask = (mask >> 8) & 0xff;
        if (fpmode == 1 && mode != Apdi) {
            uae_u8 dmask2 = dmask;
            dmask = 0;
            for (i = 0; i < 8; i++) {
                if (dmask2 & (1 << i))
                    dmask |= 1 << (7 - i);
            }
        }
    }
    if (fpmode) {
        while (dmask) { addmovemreg(out, &prevreg, &lastreg, &first, movem_index1[dmask] + (fpmode == 2 ? 24 : 16), fpmode); dmask = movem_next[dmask]; }
    } else {
        while (dmask) { addmovemreg (out, &prevreg, &lastreg, &first, movem_index1[dmask], fpmode); dmask = movem_next[dmask]; }
        while (amask) { addmovemreg (out, &prevreg, &lastreg, &first, movem_index1[amask] + 8, fpmode); amask = movem_next[amask]; }
    }
    addmovemreg(out, &prevreg, &lastreg, &first, -1, fpmode);
}

static const TCHAR *fpsizes[] = {
    _T("L"),
    _T("S"),
    _T("X"),
    _T("P"),
    _T("W"),
    _T("D"),
    _T("B"),
    _T("P")
};
static const int fpsizeconv[] = {
    sz_long,
    sz_single,
    sz_extended,
    sz_packed,
    sz_word,
    sz_double,
    sz_byte,
    sz_packed
};

static void disasm_size (TCHAR *instrname, struct instr *dp)
{
    if (dp->unsized) {
        _tcscat(instrname, _T(" "));
        return;
    }
    switch (dp->size)
    {
        case sz_byte:
            _tcscat (instrname, _T(".B "));
            break;
        case sz_word:
            _tcscat (instrname, _T(".W "));
            break;
        case sz_long:
            _tcscat (instrname, _T(".L "));
            break;
        default:
            _tcscat (instrname, _T(" "));
            break;
    }
}

void m68k_disasm_2 (TCHAR *buf, int bufsize, uaecptr pc, uaecptr *nextpc, int cnt, uae_u32 *seaddr, uae_u32 *deaddr, int safemode)
{
    uae_u32 seaddr2;
    uae_u32 deaddr2;
    
    if (buf)
        memset (buf, 0, bufsize * sizeof (TCHAR));
    if (!table68k)
        return;
    while (cnt-- > 0) {
        TCHAR instrname[100], *ccpt;
        int i;
        uae_u32 opcode;
        uae_u16 extra;
        struct mnemolookup *lookup;
        struct instr *dp;
        uaecptr oldpc;
        uaecptr m68kpc_illg = 0;
        bool illegal = false;
        
        seaddr2 = deaddr2 = 0;
        oldpc = pc;
        opcode = get_word_debug (pc);
        extra = get_word_debug (pc + 2);
        if (cpufunctbl[opcode] == op_illg_1 || cpufunctbl[opcode] == op_unimpl_1) {
            m68kpc_illg = pc + 2;
            illegal = true;
        }
        
        dp = table68k + opcode;
        if (dp->mnemo == i_ILLG) {
            illegal = false;
            opcode = 0x4AFC;
            dp = table68k + opcode;
        }
        for (lookup = lookuptab;lookup->mnemo != dp->mnemo; lookup++)
            ;
        
        buf = buf_out (buf, &bufsize, _T("%08X "), pc);
        
        pc += 2;
        
        if (lookup->friendlyname)
            _tcscpy (instrname, lookup->friendlyname);
        else
            _tcscpy (instrname, lookup->name);
        ccpt = _tcsstr (instrname, _T("cc"));
        if (ccpt != 0) {
            if ((opcode & 0xf000) == 0xf000)
                _tcscpy (ccpt, fpccnames[extra & 0x1f]);
            else
                _tcsncpy (ccpt, ccnames[dp->cc], 2);
        }
        disasm_size (instrname, dp);
        
        if (lookup->mnemo == i_MOVEC2 || lookup->mnemo == i_MOVE2C) {
            uae_u16 imm = extra;
            uae_u16 creg = imm & 0x0fff;
            uae_u16 r = imm >> 12;
            TCHAR regs[16];
            const TCHAR *cname = _T("?");
            int i;
            for (i = 0; m2cregs[i].regname; i++) {
                if (m2cregs[i].regno == creg)
                    break;
            }
            _stprintf (regs, _T("%c%d"), r >= 8 ? 'A' : 'D', r >= 8 ? r - 8 : r);
            if (m2cregs[i].regname)
                cname = m2cregs[i].regname;
            if (lookup->mnemo == i_MOVE2C) {
                _tcscat (instrname, regs);
                _tcscat (instrname, _T(","));
                _tcscat (instrname, cname);
            } else {
                _tcscat (instrname, cname);
                _tcscat (instrname, _T(","));
                _tcscat (instrname, regs);
            }
            pc += 2;
        } else if (lookup->mnemo == i_MVMEL) {
            uae_u16 mask = extra;
            pc += 2;
            pc = ShowEA (0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, deaddr, safemode);
            _tcscat (instrname, _T(","));
            movemout (instrname, mask, dp->dmode, 0);
        } else if (lookup->mnemo == i_MVMLE) {
            uae_u16 mask = extra;
            pc += 2;
            movemout(instrname, mask, dp->dmode, 0);
            _tcscat(instrname, _T(","));
            pc = ShowEA(0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, deaddr, safemode);
        } else if (lookup->mnemo == i_DIVL || lookup->mnemo == i_MULL) {
            TCHAR *p;
            pc = ShowEA(0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, &seaddr2, safemode);
            extra = get_word_debug(pc);
            pc += 2;
            p = instrname + _tcslen(instrname);
            if (extra & 0x0400)
                _stprintf(p, _T(",D%d:D%d"), extra & 7, (extra >> 12) & 7);
            else
                _stprintf(p, _T(",D%d"), (extra >> 12) & 7);
        } else if (lookup->mnemo == i_MOVES) {
            TCHAR *p;
            pc += 2;
            if (!(extra & 0x1000)) {
                pc = ShowEA(0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, &seaddr2, safemode);
                p = instrname + _tcslen(instrname);
                _stprintf(p, _T(",%c%d"), (extra & 0x8000) ? 'A' : 'D', (extra >> 12) & 7);
            } else {
                p = instrname + _tcslen(instrname);
                _stprintf(p, _T("%c%d,"), (extra & 0x8000) ? 'A' : 'D', (extra >> 12) & 7);
                pc = ShowEA(0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, &seaddr2, safemode);
            }
        } else if (lookup->mnemo == i_BFEXTS || lookup->mnemo == i_BFEXTU ||
                   lookup->mnemo == i_BFCHG || lookup->mnemo == i_BFCLR ||
                   lookup->mnemo == i_BFFFO || lookup->mnemo == i_BFINS ||
                   lookup->mnemo == i_BFSET || lookup->mnemo == i_BFTST) {
            TCHAR *p;
            int reg = -1;
            
            pc += 2;
            p = instrname + _tcslen(instrname);
            if (lookup->mnemo == i_BFEXTS || lookup->mnemo == i_BFEXTU || lookup->mnemo == i_BFFFO || lookup->mnemo == i_BFINS)
                reg = (extra >> 12) & 7;
            if (lookup->mnemo == i_BFINS)
                _stprintf(p, _T("D%d,"), reg);
            pc = ShowEA(0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, &seaddr2, safemode);
            _tcscat(instrname, _T(" {"));
            p = instrname + _tcslen(instrname);
            if (extra & 0x0800)
                _stprintf(p, _T("D%d"), (extra >> 6) & 7);
            else
                _stprintf(p, _T("%d"), (extra >> 6) & 31);
            _tcscat(instrname, _T(":"));
            p = instrname + _tcslen(instrname);
            if (extra & 0x0020)
                _stprintf(p, _T("D%d"), extra & 7);
            else
                _stprintf(p, _T("%d"), extra  & 31);
            _tcscat(instrname, _T("}"));
            p = instrname + _tcslen(instrname);
            if (lookup->mnemo == i_BFFFO || lookup->mnemo == i_BFEXTS || lookup->mnemo == i_BFEXTU)
                _stprintf(p, _T(",D%d"), reg);
        } else if (lookup->mnemo == i_CPUSHA || lookup->mnemo == i_CPUSHL || lookup->mnemo == i_CPUSHP ||
                   lookup->mnemo == i_CINVA || lookup->mnemo == i_CINVL || lookup->mnemo == i_CINVP) {
            if ((opcode & 0xc0) == 0xc0)
                _tcscat(instrname, _T("BC"));
            else if (opcode & 0x80)
                _tcscat(instrname, _T("IC"));
            else if (opcode & 0x40)
                _tcscat(instrname, _T("DC"));
            else
                _tcscat(instrname, _T("?"));
            if (lookup->mnemo == i_CPUSHL || lookup->mnemo == i_CPUSHP || lookup->mnemo == i_CINVL || lookup->mnemo == i_CINVP) {
                TCHAR *p = instrname + _tcslen(instrname);
                _stprintf(p, _T(",(A%d)"), opcode & 7);
            }
        } else if (lookup->mnemo == i_FPP) {
            TCHAR *p;
            int ins = extra & 0x3f;
            int size = (extra >> 10) & 7;
            
            pc += 2;
            if ((extra & 0xfc00) == 0x5c00) { // FMOVECR (=i_FPP with source specifier = 7)
                fptype fp;
                if (fpu_get_constant(&fp, extra))
                    _stprintf(instrname, _T("FMOVECR.X #%s,FP%d"), fp_print(&fp), (extra >> 7) & 7);
                else
                    _stprintf(instrname, _T("FMOVECR.X #?,FP%d"), (extra >> 7) & 7);
            } else if ((extra & 0x8000) == 0x8000) { // FMOVEM
                int dr = (extra >> 13) & 1;
                int mode;
                int dreg = (extra >> 4) & 7;
                int regmask, fpmode;
                
                if (extra & 0x4000) {
                    mode = (extra >> 11) & 3;
                    regmask = extra & 0xff;  // FMOVEM FPx
                    fpmode = 1;
                    _tcscpy(instrname, _T("FMOVEM.X "));
                } else {
                    mode = 0;
                    regmask = (extra >> 10) & 7;  // FMOVEM control
                    fpmode = 2;
                    _tcscpy(instrname, _T("FMOVEM.L "));
                    if (regmask == 1 || regmask == 2 || regmask == 4)
                        _tcscpy(instrname, _T("FMOVE.L "));
                }
                p = instrname + _tcslen(instrname);
                if (dr) {
                    if (mode & 1)
                        _stprintf(instrname, _T("D%d"), dreg);
                    else
                        movemout(instrname, regmask, dp->dmode, fpmode);
                    _tcscat(instrname, _T(","));
                    pc = ShowEA(0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, deaddr, safemode);
                } else {
                    pc = ShowEA(0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, deaddr, safemode);
                    _tcscat(instrname, _T(","));
                    p = instrname + _tcslen(instrname);
                    if (mode & 1)
                        _stprintf(p, _T("D%d"), dreg);
                    else
                        movemout(p, regmask, dp->dmode, fpmode);
                }
            } else {
                if (fpuopcodes[ins])
                    _tcscpy(instrname, fpuopcodes[ins]);
                else
                    _tcscpy(instrname, _T("F?"));
                
                if ((extra & 0xe000) == 0x6000) { // FMOVE to memory
                    int kfactor = extra & 0x7f;
                    _tcscpy(instrname, _T("FMOVE."));
                    _tcscat(instrname, fpsizes[size]);
                    _tcscat(instrname, _T(" "));
                    p = instrname + _tcslen(instrname);
                    _stprintf(p, _T("FP%d,"), (extra >> 7) & 7);
                    pc = ShowEA(0, pc, opcode, dp->dreg, dp->dmode, fpsizeconv[size], instrname, &deaddr2, safemode);
                    p = instrname + _tcslen(instrname);
                    if (size == 7) {
                        _stprintf(p, _T(" {D%d}"), (kfactor >> 4));
                    } else if (kfactor) {
                        if (kfactor & 0x40)
                            kfactor |= ~0x3f;
                        _stprintf(p, _T(" {%d}"), kfactor);
                    }
                } else {
                    if (extra & 0x4000) { // source is EA
                        _tcscat(instrname, _T("."));
                        _tcscat(instrname, fpsizes[size]);
                        _tcscat(instrname, _T(" "));
                        pc = ShowEA(0, pc, opcode, dp->dreg, dp->dmode, fpsizeconv[size], instrname, &seaddr2, safemode);
                    } else { // source is FPx
                        p = instrname + _tcslen(instrname);
                        _stprintf(p, _T(".X FP%d"), (extra >> 10) & 7);
                    }
                    p = instrname + _tcslen(instrname);
                    if ((extra & 0x4000) || (((extra >> 7) & 7) != ((extra >> 10) & 7)))
                        _stprintf(p, _T(",FP%d"), (extra >> 7) & 7);
                    if (ins >= 0x30 && ins < 0x38) { // FSINCOS
                        p = instrname + _tcslen(instrname);
                        _stprintf(p, _T(",FP%d"), extra & 7);
                    }
                }
            }
        } else if ((opcode & 0xf000) == 0xa000) {
            _tcscpy(instrname, _T("A-LINE"));
        } else {
            if (dp->suse) {
                pc = ShowEA (0, pc, opcode, dp->sreg, dp->smode, dp->size, instrname, &seaddr2, safemode);
            }
            if (dp->suse && dp->duse)
                _tcscat (instrname, _T(","));
            if (dp->duse) {
                pc = ShowEA (0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, &deaddr2, safemode);
            }
        }
        
        for (i = 0; i < (int)(pc - oldpc) / 2 && i < 5; i++) {
            buf = buf_out (buf, &bufsize, _T("%04x "), get_word_debug (oldpc + i * 2));
        }
        while (i++ < 5)
            buf = buf_out (buf, &bufsize, _T("     "));
        
        if (illegal)
            buf = buf_out (buf, &bufsize, _T("[ "));
        buf = buf_out (buf, &bufsize, instrname);
        if (illegal)
            buf = buf_out (buf, &bufsize, _T(" ]"));
        
        if (ccpt != 0) {
            uaecptr addr2 = deaddr2 ? deaddr2 : seaddr2;
            if (deaddr)
                *deaddr = pc;
            if ((opcode & 0xf000) == 0xf000) {
                if (fpp_cond(dp->cc)) {
                    buf = buf_out(buf, &bufsize, _T(" == $%08x (T)"), addr2);
                } else {
                    buf = buf_out(buf, &bufsize, _T(" == $%08x (F)"), addr2);
                }
            } else {
                if (cctrue (dp->cc)) {
                    buf = buf_out (buf, &bufsize, _T(" == $%08x (T)"), addr2);
                } else {
                    buf = buf_out (buf, &bufsize, _T(" == $%08x (F)"), addr2);
                }
            }
        } else if ((opcode & 0xff00) == 0x6100) { /* BSR */
            if (deaddr)
                *deaddr = pc;
            buf = buf_out (buf, &bufsize, _T(" == $%08x"), seaddr2);
        }
        buf = buf_out (buf, &bufsize, _T("\n"));
        
        if (illegal)
            pc =  m68kpc_illg;
    }
    if (nextpc)
        *nextpc = pc;
    if (seaddr)
        *seaddr = seaddr2;
    if (deaddr)
        *deaddr = deaddr2;
}

void m68k_disasm_ea (uaecptr addr, uaecptr *nextpc, int cnt, uae_u32 *seaddr, uae_u32 *deaddr)
{
    TCHAR *buf;
    
    buf = xmalloc (TCHAR, (MAX_LINEWIDTH + 1) * cnt);
    if (!buf)
        return;
    m68k_disasm_2 (buf, (MAX_LINEWIDTH + 1) * cnt, addr, nextpc, cnt, seaddr, deaddr, 1);
    xfree (buf);
}
void m68k_disasm (uaecptr addr, uaecptr *nextpc, int cnt)
{
    TCHAR *buf;
    
    buf = xmalloc (TCHAR, (MAX_LINEWIDTH + 1) * cnt);
    if (!buf)
        return;
    m68k_disasm_2 (buf, (MAX_LINEWIDTH + 1) * cnt, addr, nextpc, cnt, NULL, NULL, 0);
    printf (_T("%s"), buf);
    xfree (buf);
}

/*************************************************************
Disasm the m68kcode at the given address into instrname
and instrcode
*************************************************************/
void sm68k_disasm (TCHAR *instrname, TCHAR *instrcode, uaecptr addr, uaecptr *nextpc)
{
    TCHAR *ccpt;
    uae_u32 opcode;
    struct mnemolookup *lookup;
    struct instr *dp;
    uaecptr pc, oldpc;
    
    pc = oldpc = addr;
    opcode = get_word_debug (pc);
    if (cpufunctbl[opcode] == op_illg_1) {
        opcode = 0x4AFC;
    }
    dp = table68k + opcode;
    for (lookup = lookuptab;lookup->mnemo != dp->mnemo; lookup++);
    
    pc += 2;
    
    _tcscpy (instrname, lookup->name);
    ccpt = _tcsstr (instrname, _T("cc"));
    if (ccpt != 0) {
        _tcsncpy (ccpt, ccnames[dp->cc], 2);
    }
    switch (dp->size){
        case sz_byte: _tcscat (instrname, _T(".B ")); break;
        case sz_word: _tcscat (instrname, _T(".W ")); break;
        case sz_long: _tcscat (instrname, _T(".L ")); break;
        default: _tcscat (instrname, _T("   ")); break;
    }
    
    if (dp->suse) {
        pc = ShowEA (0, pc, opcode, dp->sreg, dp->smode, dp->size, instrname, NULL, 0);
    }
    if (dp->suse && dp->duse)
        _tcscat (instrname, _T(","));
    if (dp->duse) {
        pc = ShowEA (0, pc, opcode, dp->dreg, dp->dmode, dp->size, instrname, NULL, 0);
    }
    if (instrcode)
    {
        int i;
        for (i = 0; i < (int)(pc - oldpc) / 2; i++)
        {
            _stprintf (instrcode, _T("%04x "), get_iword_debug (oldpc + i * 2));
            instrcode += _tcslen (instrcode);
        }
    }
    if (nextpc)
        *nextpc = pc;
}

struct cpum2c m2cregs[] = {
	{ 0, "SFC" },
	{ 1, "DFC" },
	{ 2, "CACR" },
	{ 3, "TC" },
	{ 4, "ITT0" },
	{ 5, "ITT1" },
	{ 6, "DTT0" },
	{ 7, "DTT1" },
	{ 8, "BUSC" },
	{ 0x800, "USP" },
	{ 0x801, "VBR" },
	{ 0x802, "CAAR" },
	{ 0x803, "MSP" },
	{ 0x804, "ISP" },
	{ 0x805, "MMUS" },
	{ 0x806, "URP" },
	{ 0x807, "SRP" },
	{ 0x808, "PCR" },
	{ -1, NULL }
};

void m68k_dumpstate_2 (uaecptr pc, uaecptr *nextpc)
{
    int i, j;
    
    for (i = 0; i < 8; i++){
        printf (_T("  D%d %08X "), i, m68k_dreg (regs, i));
        if ((i & 3) == 3) printf (_T("\n"));
    }
    for (i = 0; i < 8; i++){
        printf (_T("  A%d %08X "), i, m68k_areg (regs, i));
        if ((i & 3) == 3) printf (_T("\n"));
    }
    if (regs.s == 0)
        regs.usp = m68k_areg (regs, 7);
    if (regs.s && regs.m)
        regs.msp = m68k_areg (regs, 7);
    if (regs.s && regs.m == 0)
        regs.isp = m68k_areg (regs, 7);
    j = 2;
    printf (_T("USP  %08X ISP  %08X "), regs.usp, regs.isp);
    for (i = 0; m2cregs[i].regno>= 0; i++) {
        if (!movec_illg (m2cregs[i].regno)) {
            if (!_tcscmp (m2cregs[i].regname, _T("USP")) || !_tcscmp (m2cregs[i].regname, _T("ISP")))
                continue;
            if (j > 0 && (j % 4) == 0)
                printf (_T("\n"));
            printf (_T("%-4s %08X "), m2cregs[i].regname, val_move2c (m2cregs[i].regno));
            j++;
        }
    }
    if (j > 0)
        printf (_T("\n"));
    printf (_T("T=%d%d S=%d M=%d X=%d N=%d Z=%d V=%d C=%d IMASK=%d STP=%d\n"),
                   regs.t1, regs.t0, regs.s, regs.m,
                   GET_XFLG (), GET_NFLG (), GET_ZFLG (),
                   GET_VFLG (), GET_CFLG (),
                   regs.intmask, regs.stopped);
#ifdef FPUEMU
    if (currprefs.fpu_model) {
        uae_u32 fpsr;
        for (i = 0; i < 8; i++){
            printf (_T("FP%d: %s "), i, fp_print(&regs.fp[i]));
            if ((i & 3) == 3)
                printf (_T("\n"));
        }
        fpsr = fpp_get_fpsr ();
        printf (_T("FPSR: %08X FPCR: %04x FPIAR: %08x N=%d Z=%d I=%d NAN=%d\n"),
                       fpsr, regs.fpcr, regs.fpiar,
                       (fpsr & 0x8000000) != 0,
                       (fpsr & 0x4000000) != 0,
                       (fpsr & 0x2000000) != 0,
                       (fpsr & 0x1000000) != 0);
    }
#endif
    if (currprefs.mmu_model == 68030) {
        printf (_T("SRP: %llX CRP: %llX\n"), srp_030, crp_030);
        printf (_T("TT0: %08X TT1: %08X TC: %08X\n"), tt0_030, tt1_030, tc_030);
    }
    if (currprefs.cpu_compatible && currprefs.cpu_model == 68000) {
        struct instr *dp;
        struct mnemolookup *lookup1, *lookup2;
        dp = table68k + regs.irc;
        for (lookup1 = lookuptab; lookup1->mnemo != dp->mnemo; lookup1++);
        dp = table68k + regs.ir;
        for (lookup2 = lookuptab; lookup2->mnemo != dp->mnemo; lookup2++);
        printf (_T("Prefetch %04x (%s) %04x (%s) Chip latch %08X\n"), regs.irc, lookup1->name, regs.ir, lookup2->name, regs.chipset_latch_rw);
    }
    
    if (pc != 0xffffffff) {
        m68k_disasm (pc, nextpc, 1);
        if (nextpc)
            printf (_T("Next PC: %08x\n"), *nextpc);
    }
}
void m68k_dumpstate (uaecptr *nextpc)
{
    m68k_dumpstate_2 (m68k_getpc (), nextpc);
}

static void exception3f (uae_u32 opcode, uaecptr addr, bool writeaccess, bool instructionaccess, bool notinstruction, uaecptr pc, bool plus2)
{
    if (currprefs.cpu_model >= 68040)
        addr &= ~1;
    if (currprefs.cpu_model >= 68020) {
        if (pc == 0xffffffff)
            last_addr_for_exception_3 = regs.instruction_pc;
        else
            last_addr_for_exception_3 = pc;
    } else if (pc == 0xffffffff) {
        last_addr_for_exception_3 = m68k_getpc ();
        if (plus2)
            last_addr_for_exception_3 += 2;
    } else {
        last_addr_for_exception_3 = pc;
    }
    last_fault_for_exception_3 = addr;
    last_op_for_exception_3 = opcode;
    last_writeaccess_for_exception_3 = writeaccess;
    last_instructionaccess_for_exception_3 = instructionaccess;
    Exception (3);
}

void exception3_read(uae_u32 opcode, uaecptr addr)
{
    exception3f (opcode, addr, false, 0, false, 0xffffffff, false);
}
void exception3i (uae_u32 opcode, uaecptr addr)
{
    exception3f (opcode, addr, 0, 1, false, 0xffffffff, true);
}
void exception3b (uae_u32 opcode, uaecptr addr, bool w, bool i, uaecptr pc)
{
    exception3f (opcode, addr, w, i, false, pc, true);
}

void exception2 (uaecptr addr, bool read, int size, uae_u32 fc)
{
    if (currprefs.mmu_model == 68030) {
        uae_u32 flags = size == 1 ? MMU030_SSW_SIZE_B : (size == 2 ? MMU030_SSW_SIZE_W : MMU030_SSW_SIZE_L);
        mmu030_page_fault (addr, read, flags, fc);
    } else {
        mmu_bus_error (addr, 0, fc, read == false, size, true);
    }
}

void cpureset (void) {
	uaecptr pc;
	uaecptr ksboot = 0xf80002 - 2; /* -2 = RESET hasn't increased PC yet */
	uae_u16 ins;

	if (currprefs.cpu_compatible) {
		return;
	}
    
	pc = m68k_getpc ();
	/* panic, RAM is going to disappear under PC */
	ins = get_word (pc + 2);
	if ((ins & ~7) == 0x4ed0) {
		int reg = ins & 7;
		uae_u32 addr = m68k_areg (regs, reg);
		write_log ("reset/jmp (ax) combination emulated -> %x\n", addr);
		if (addr < 0x80000)
			addr += 0xf80000;
		m68k_setpc (addr - 2);
		return;
	}
	write_log ("M68K RESET PC=%x, rebooting..\n", pc);
	m68k_setpc (ksboot);
}


void m68k_setstopped (void)
{
	regs.stopped = 1;
	/* A traced STOP instruction drops through immediately without
	actually stopping.  */
	if ((regs.spcflags & SPCFLAG_DOTRACE) == 0)
		set_special (SPCFLAG_STOP);
	else
		m68k_resumestopped ();
}

void m68k_resumestopped (void)
{
	if (!regs.stopped)
		return;
	regs.stopped = 0;
	unset_special (SPCFLAG_STOP);
}
