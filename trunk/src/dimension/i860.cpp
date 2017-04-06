/***************************************************************************

    i860.c

    Interface file for the Intel i860 emulator.

    Copyright (C) 1995-present Jason Eckhardt (jle@rice.edu)
    Released for general non-commercial use under the MAME license
    with the additional requirement that you are free to use and
    redistribute this code in modified or unmodified form, provided
    you list me in the credits.
    Visit http://mamedev.org for licensing and usage restrictions.

    Changes for previous/NeXTdimension by Simon Schubiger (SC)

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "host.h"
#include "i860.hpp"

static i860_cpu_device nd_i860;

extern "C" {
#include "configuration.h"
    
    static void i860_run_nop(int nHostCycles) {}
    
    i860_run_func i860_Run = i860_run_nop;

    static void i860_run_thread(int nHostCycles) {
        nd_nbic_interrupt();
    }

    static void i860_run_no_thread(int nHostCycles) {
        nd_i860.handle_msgs();
        
        if(nd_i860.is_halted()) return;
        
        nHostCycles *= 33; // i860 @ 33MHz
        nHostCycles /= ConfigureParams.System.nCpuFreq;
        while (nHostCycles > 0) {
            nd_i860.run_cycle();
            nHostCycles -= 2;
        }
        
        nd_nbic_interrupt();
    }
    
    void nd_i860_init() {
        i860_Run = ConfigureParams.Dimension.bI860Thread ? i860_run_thread : i860_run_no_thread;
        nd_i860.init();
    }
	
	void nd_i860_uninit() {
        nd_i860.uninit();
	}
	    
    void nd_start_debugger(void) {
        nd_i860.send_msg(MSG_DBG_BREAK);
    }
    
    int i860_thread(void* data) {
        SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
        ((i860_cpu_device*)data)->run();
        return 0;
    }
    
    void i860_reset() {
        nd_i860.send_msg(MSG_I860_RESET);
    }

    void nd_display_blank() {
        nd_i860.send_msg(MSG_DISPLAY_BLANK);
    }

    void nd_video_blank() {
        nd_i860.send_msg(MSG_VIDEO_BLANK);
    }

    void i860_interrupt() {
        nd_i860.interrupt();
    }
    
    const char* nd_reports(double realTime, double hostTime) {
        return nd_i860.reports(realTime, hostTime);
    }
}

i860_cpu_device::i860_cpu_device() {
    m_thread = NULL;
    m_halt   = true;
    
    for(int i = 0; i < 8192; i++) {
        int upper6 = i >> 7;
        switch (upper6) {
            case 0x12:
                decoder_tbl[i] = fp_decode_tbl[i & 0x7f];
                break;
            case 0x13:
                decoder_tbl[i] = core_esc_decode_tbl[i&3];
                break;
            default:
                decoder_tbl[i] = decode_tbl[upper6];
        }
    }
}

void i860_cpu_device::set_mem_access(bool be) {
    if(be) {
        rdmem[1]  = nd_board_rd8_be;
        rdmem[2]  = nd_board_rd16_be;
        rdmem[4]  = nd_board_rd32_be;
        rdmem[8]  = nd_board_rd64_be;
        rdmem[16] = nd_board_rd128_be;
        
        wrmem[1]  = nd_board_wr8_be;
        wrmem[2]  = nd_board_wr16_be;
        wrmem[4]  = nd_board_wr32_be;
        wrmem[8]  = nd_board_wr64_be;
        wrmem[16] = nd_board_wr128_be;
    } else {
        rdmem[1]  = nd_board_rd8_le;
        rdmem[2]  = nd_board_rd16_le;
        rdmem[4]  = nd_board_rd32_le;
        rdmem[8]  = nd_board_rd64_le;
        rdmem[16] = nd_board_rd128_le;
        
        wrmem[1]  = nd_board_wr8_le;
        wrmem[2]  = nd_board_wr16_le;
        wrmem[4]  = nd_board_wr32_le;
        wrmem[8]  = nd_board_wr64_le;
        wrmem[16] = nd_board_wr128_le;
    }
}

inline UINT8 i860_cpu_device::rdcs8(UINT32 addr) {
    return nd_board_cs8get(addr);
}

inline UINT32 i860_cpu_device::get_iregval(int gr) {
    return m_iregs[gr];
}

inline void i860_cpu_device::set_iregval(int gr, UINT32 val) {
    m_iregs[gr] = val;
    m_iregs[0]  = 0; // make sure r0 is always 0
}

inline float i860_cpu_device::get_fregval_s (int fr) {
    return *(float*)(&m_fregs[fr * 4]);
}

inline void i860_cpu_device::set_fregval_s (int fr, float s) {
    if(fr > 1)
        *(float*)(&m_fregs[fr * 4]) = s;
}

inline double i860_cpu_device::get_fregval_d (int fr) {
    return *(double*)(&m_fregs[fr * 4]);
}

inline void i860_cpu_device::set_fregval_d (int fr, double d) {
    if(fr > 1)
        *(double*)(&m_fregs[fr * 4]) = d;
}

inline void i860_cpu_device::SET_PSR_CC(int val) {
    if(!(m_dim_cc_valid))
        m_cregs[CR_PSR] = (m_cregs[CR_PSR] & ~(1 << 2)) | ((val & 1) << 2);
}

void i860_cpu_device::send_msg(int msg) {
    host_lock(&m_port_lock);
    m_port |= msg;
    host_unlock(&m_port_lock);
}

void i860_cpu_device::handle_trap(UINT32 savepc) {
    static char buffer[256];
    buffer[0] = 0;
    strcat(buffer, "TRAP");
    if(m_flow & TRAP_NORMAL)        strcat(buffer, " [Normal]");
    if(m_flow & TRAP_IN_DELAY_SLOT) strcat(buffer, " [Delay Slot]");
    if(m_flow & TRAP_WAS_EXTERNAL)  strcat(buffer, " [External]");
    if(!(GET_PSR_IT() || GET_PSR_FT() || GET_PSR_IAT() || GET_PSR_DAT() || GET_PSR_IN()))
        strcat(buffer, " >Reset<");
    else {
        if(GET_PSR_IT())  strcat(buffer, " >Instruction Fault<");
        if(GET_PSR_FT())  strcat(buffer, " >Floating Point Fault<");
        if(GET_PSR_IAT()) strcat(buffer, " >Instruction Access Fault<");
        if(GET_PSR_DAT()) strcat(buffer, " >Data Access Fault<");
        if(GET_PSR_IN())  strcat(buffer, " >Interrupt<");
    }
    
    if(!(m_single_stepping) && !((GET_PSR_IAT() || GET_PSR_DAT() || GET_PSR_IN())))
        debugger('d', buffer);
    
    if(m_dim)
        Log_Printf(LOG_WARN, "[i860] Trap while DIM %s pc=%08X m_flow=%08X", buffer, savepc, m_flow);

    /* If we need to trap, change PC to trap address.
     Also set supervisor mode, copy U and IM to their
     previous versions, clear IM.  */
    if(m_flow & TRAP_WAS_EXTERNAL) {
        if (GET_PC_UPDATED()) {
            m_cregs[CR_FIR] = m_pc;
        } else {
            m_cregs[CR_FIR] = savepc + 4;
        }
    }
    else if (m_flow & TRAP_IN_DELAY_SLOT) {
        m_cregs[CR_FIR] = savepc + 4;
    }
    else
        m_cregs[CR_FIR] = savepc;
    
    m_flow |= FIR_GETS_TRAP;
    SET_PSR_PU (GET_PSR_U ());
    SET_PSR_PIM (GET_PSR_IM ());
    SET_PSR_U (0);
    SET_PSR_IM (0);
    SET_PSR_DIM (0);
    SET_PSR_DS (0);
    
    m_save_flow     = m_flow & DIM_OP;
    m_save_dim      = m_dim;
    m_save_cc       = m_dim_cc;
    m_save_cc_valid = m_dim_cc_valid;
    
    m_dim           = DIM_NONE;
    m_dim_cc        = false;
    m_dim_cc_valid  = false;
    
    m_pc = 0xffffff00;
}

void i860_cpu_device::ret_from_trap() {
    m_flow          |= m_save_flow & ~DIM_OP;
    m_dim            = m_save_dim;
    m_flow          &= ~FIR_GETS_TRAP;
    m_dim_cc         = m_save_cc;
    m_dim_cc_valid   = m_save_cc_valid;
}

void i860_cpu_device::run_cycle() {
    CLEAR_FLOW();
    m_dim_cc_valid = false;
    m_flow        &= ~DIM_OP;
    UINT64 insn64  = ifetch64(m_pc);
    
    if(!(m_pc & 4)) {
        UINT32 savepc  = m_pc;
        
#if ENABLE_DEBUGGER
        if(m_single_stepping) debugger(0,0);
#endif
        
        UINT32 insnLow = insn64;
        if(insnLow == INSN_FNOP_DIM) {
            if(m_dim) m_flow |=  DIM_OP;
            else      m_flow &= ~DIM_OP;
        } else if((insnLow & INSN_MASK_DIM) == INSN_FP_DIM)
            m_flow |= DIM_OP;
        
        decode_exec(insnLow);
        
        if (PENDING_TRAP()) {
            handle_trap(savepc);
            goto done;
        } else if(GET_PC_UPDATED()) {
            goto done;
        } else {
            // If the PC wasn't updated by a control flow instruction, just bump to next sequential instruction.
            m_pc   += 4;
            CLEAR_FLOW();
        }
    }
    
    if(m_pc & 4) {
        UINT32 savepc  = m_pc;
        
#if ENABLE_DEBUGGER
        if(m_single_stepping && !(m_dim)) debugger(0,0);
#endif

        UINT32 insnHigh= insn64 >> 32;
        decode_exec(insnHigh);
        
        // only check for external interrupts
        // - on high-word (speedup)
        // - not DIM (safety :-)
        // - when no other traps are pending
        if(!(m_dim) && !(PENDING_TRAP())) {
            if(m_flow & EXT_INTR) {
                m_flow &= ~EXT_INTR;
                gen_interrupt();
            } else
                clr_interrupt();
        }
        
        if (PENDING_TRAP()) {
            handle_trap(savepc);
        } else if (GET_PC_UPDATED()) {
            goto done;
        } else {
            // If the PC wasn't updated by a control flow instruction, just bump to next sequential instruction.
            m_pc += 4;
        }
    }
done:
    switch (m_dim) {
        case DIM_NONE:
            if(m_flow & DIM_OP)
                m_dim = DIM_TEMP;
            break;
        case DIM_TEMP:
            m_dim = m_flow & DIM_OP ? DIM_FULL : DIM_NONE;
            break;
        case DIM_FULL:
            if(!(m_flow & DIM_OP))
                m_dim = DIM_TEMP;
            break;
    }
}

int i860_cpu_device::memtest(bool be) {
    const UINT32 P_TEST_ADDR = 0x28000000; // assume ND in slot 2
    
    m_cregs[CR_DIRBASE] = 0; // turn VM off

    const UINT8  uint8  = 0x01;
    const UINT16 uint16 = 0x0123;
    const UINT32 uint32 = 0x01234567;
    const UINT64 uint64 = 0x0123456789ABCDEFLL;
    
    UINT8  tmp8;
    UINT16 tmp16;
    UINT32 tmp32;
    
    int err = be ? 20000 : 30000;
    
    // intel manual example
    SET_EPSR_BE(0);
    set_mem_access(false);
    
    tmp8 = 'A'; wrmem[1](P_TEST_ADDR+0, (UINT32*)&tmp8);
    tmp8 = 'B'; wrmem[1](P_TEST_ADDR+1, (UINT32*)&tmp8);
    tmp8 = 'C'; wrmem[1](P_TEST_ADDR+2, (UINT32*)&tmp8);
    tmp8 = 'D'; wrmem[1](P_TEST_ADDR+3, (UINT32*)&tmp8);
    tmp8 = 'E'; wrmem[1](P_TEST_ADDR+4, (UINT32*)&tmp8);
    tmp8 = 'F'; wrmem[1](P_TEST_ADDR+5, (UINT32*)&tmp8);
    tmp8 = 'G'; wrmem[1](P_TEST_ADDR+6, (UINT32*)&tmp8);
    tmp8 = 'H'; wrmem[1](P_TEST_ADDR+7, (UINT32*)&tmp8);
    
    rdmem[1](P_TEST_ADDR+0, (UINT32*)&tmp8); if(tmp8 != 'A') return err + 100;
    rdmem[1](P_TEST_ADDR+1, (UINT32*)&tmp8); if(tmp8 != 'B') return err + 101;
    rdmem[1](P_TEST_ADDR+2, (UINT32*)&tmp8); if(tmp8 != 'C') return err + 102;
    rdmem[1](P_TEST_ADDR+3, (UINT32*)&tmp8); if(tmp8 != 'D') return err + 103;
    rdmem[1](P_TEST_ADDR+4, (UINT32*)&tmp8); if(tmp8 != 'E') return err + 104;
    rdmem[1](P_TEST_ADDR+5, (UINT32*)&tmp8); if(tmp8 != 'F') return err + 105;
    rdmem[1](P_TEST_ADDR+6, (UINT32*)&tmp8); if(tmp8 != 'G') return err + 106;
    rdmem[1](P_TEST_ADDR+7, (UINT32*)&tmp8); if(tmp8 != 'H') return err + 107;
    
    rdmem[2](P_TEST_ADDR+0, (UINT32*)&tmp16); if(tmp16 != (('B'<<8)|('A'))) return err + 110;
    rdmem[2](P_TEST_ADDR+2, (UINT32*)&tmp16); if(tmp16 != (('D'<<8)|('C'))) return err + 111;
    rdmem[2](P_TEST_ADDR+4, (UINT32*)&tmp16); if(tmp16 != (('F'<<8)|('E'))) return err + 112;
    rdmem[2](P_TEST_ADDR+6, (UINT32*)&tmp16); if(tmp16 != (('H'<<8)|('G'))) return err + 113;

    rdmem[4](P_TEST_ADDR+0, &tmp32); if(tmp32 != (('D'<<24)|('C'<<16)|('B'<<8)|('A'))) return err + 120;
    rdmem[4](P_TEST_ADDR+4, &tmp32); if(tmp32 != (('H'<<24)|('G'<<16)|('F'<<8)|('E'))) return err + 121;

    SET_EPSR_BE(1);
    set_mem_access(true);

    rdmem[1](P_TEST_ADDR+0, (UINT32*)&tmp8); if(tmp8 != 'H') return err + 200;
    rdmem[1](P_TEST_ADDR+1, (UINT32*)&tmp8); if(tmp8 != 'G') return err + 201;
    rdmem[1](P_TEST_ADDR+2, (UINT32*)&tmp8); if(tmp8 != 'F') return err + 202;
    rdmem[1](P_TEST_ADDR+3, (UINT32*)&tmp8); if(tmp8 != 'E') return err + 203;
    rdmem[1](P_TEST_ADDR+4, (UINT32*)&tmp8); if(tmp8  != 'D') return err + 204;
    rdmem[1](P_TEST_ADDR+5, (UINT32*)&tmp8); if(tmp8  != 'C') return err + 205;
    rdmem[1](P_TEST_ADDR+6, (UINT32*)&tmp8); if(tmp8  != 'B') return err + 206;
    rdmem[1](P_TEST_ADDR+7, (UINT32*)&tmp8); if(tmp8  != 'A') return err + 207;
    
    rdmem[2](P_TEST_ADDR+0, (UINT32*)&tmp16); if(tmp16 != (('H'<<8)|('G'))) return err + 210;
    rdmem[2](P_TEST_ADDR+2, (UINT32*)&tmp16); if(tmp16 != (('F'<<8)|('E'))) return err + 211;
    rdmem[2](P_TEST_ADDR+4, (UINT32*)&tmp16); if(tmp16 != (('D'<<8)|('C'))) return err + 212;
    rdmem[2](P_TEST_ADDR+6, (UINT32*)&tmp16); if(tmp16 != (('B'<<8)|('A'))) return err + 213;
    
    rdmem[4](P_TEST_ADDR+0, &tmp32); if(tmp32 != (('H'<<24)|('G'<<16)|('F'<<8)|('E'))) return err + 220;
    rdmem[4](P_TEST_ADDR+4, &tmp32); if(tmp32 != (('D'<<24)|('C'<<16)|('B'<<8)|('A'))) return err + 221;
    
    // some register and mem r/w tests
    
    SET_EPSR_BE(be);
    set_mem_access(be);

    wrmem[1](P_TEST_ADDR, (UINT32*)&uint8);
    rdmem[1](P_TEST_ADDR, (UINT32*)&tmp8);
    if(tmp8 != 0x01) return err;
    
    wrmem[2](P_TEST_ADDR, (UINT32*)&uint16);
    rdmem[2](P_TEST_ADDR, (UINT32*)&tmp16);
    if(tmp16 != 0x0123) return err+1;
    
    wrmem[4](P_TEST_ADDR, &uint32);
    rdmem[4](P_TEST_ADDR, &tmp32); if(tmp32 != 0x01234567) return err+2;
    
    readmem_emu(P_TEST_ADDR, 4, (UINT8*)&uint32);
    if(uint32 != 0x01234567) return err+3;
    
    writemem_emu(P_TEST_ADDR, 4, (UINT8*)&uint32, 0xff);
    rdmem[4](P_TEST_ADDR+0, &tmp32); if(tmp32 != 0x01234567) return err+4;
    
    UINT8* uint8p = (UINT8*)&uint64;
    set_fregval_d(2, *((double*)uint8p));
    writemem_emu(P_TEST_ADDR, 8, &m_fregs[8], 0xff);
    readmem_emu (P_TEST_ADDR, 8, &m_fregs[8]);
    *((double*)&uint64) = get_fregval_d(2);
    if(uint64 != 0x0123456789ABCDEFLL) return err+5;

    UINT32 lo;
    UINT32 hi;

    rdmem[4](P_TEST_ADDR+0, &lo);
    rdmem[4](P_TEST_ADDR+4, &hi);
    
    if(lo != 0x01234567) return err+6;
    if(hi != 0x89ABCDEF) return err+7;
    
    return 0;
}

void i860_cpu_device::init() {
    /* Configurations - keep in sync with i860cfg.h */
    static const char* CFGS[8];
    for(int i = 0; i < 8; i++) CFGS[i] = "Unknown emulator configuration";
    CFGS[CONF_I860_SPEED]     = CONF_STR(CONF_I860_SPEED);
    CFGS[CONF_I860_DEV]       = CONF_STR(CONF_I860_DEV);
    CFGS[CONF_I860_NO_THREAD] = CONF_STR(CONF_I860_NO_THREAD);
    Log_Printf(LOG_WARN, "[i860] Emulator configured for %s, %d logical cores detected, %s",
               CFGS[CONF_I860], host_num_cpus(),
               ConfigureParams.Dimension.bI860Thread ? "using seperate thread for i860" : "i860 running on m68k thread. WARNING: expect slow emulation");
    
    m_single_stepping   = 0;
    m_lastcmd           = 0;
    m_console_idx       = 0;
    m_break_on_next_msg = false;
    m_dim               = DIM_NONE;
    m_traceback_idx     = 0;
    
    set_mem_access(false);

    // some sanity checks for endianess
    int    err    = 0;
    {
        UINT32 uint32 = 0x01234567;
        UINT8* uint8p = (UINT8*)&uint32;
        if(uint8p[3] != 0x01) {err = 1; goto error;}
        if(uint8p[2] != 0x23) {err = 2; goto error;}
        if(uint8p[1] != 0x45) {err = 3; goto error;}
        if(uint8p[0] != 0x67) {err = 4; goto error;}
        
        for(int i = 0; i < 32; i++) {
            uint8p[3] = i;
            set_fregval_s(i, *((float*)uint8p));
        }
        if(get_fregval_s(0) != 0)   {err = 198; goto error;}
        if(get_fregval_s(1) != 0)   {err = 199; goto error;}
        for(int i = 2; i < 32; i++) {
            uint8p[3] = i;
            if(get_fregval_s(i) != *((float*)uint8p))
                {err = 100+i; goto error;}
        }
        for(int i = 2; i < 32; i++) {
            if(m_fregs[i*4+3] != i)    {err = 200+i; goto error;}
            if(m_fregs[i*4+2] != 0x23) {err = 200+i; goto error;}
            if(m_fregs[i*4+1] != 0x45) {err = 200+i; goto error;}
            if(m_fregs[i*4+0] != 0x67) {err = 200+i; goto error;}
        }
    }
    
    {
        UINT64 uint64 = 0x0123456789ABCDEFLL;
        UINT8* uint8p = (UINT8*)&uint64;
        if(uint8p[7] != 0x01) {err = 10001; goto error;}
        if(uint8p[6] != 0x23) {err = 10002; goto error;}
        if(uint8p[5] != 0x45) {err = 10003; goto error;}
        if(uint8p[4] != 0x67) {err = 10004; goto error;}
        if(uint8p[3] != 0x89) {err = 10005; goto error;}
        if(uint8p[2] != 0xAB) {err = 10006; goto error;}
        if(uint8p[1] != 0xCD) {err = 10007; goto error;}
        if(uint8p[0] != 0xEF) {err = 10008; goto error;}
        
        for(int i = 0; i < 16; i++) {
            uint8p[7] = i;
            set_fregval_d(i*2, *((double*)uint8p));
        }
        if(get_fregval_d(0) != 0)
            {err = 10199; goto error;}
        for(int i = 1; i < 16; i++) {
            uint8p[7] = i;
            if(get_fregval_d(i*2) != *((double*)uint8p))
                {err = 10100+i; goto error;}
        }
        for(int i = 2; i < 32; i += 2) {
            float hi = get_fregval_s(i+1);
            float lo = get_fregval_s(i+0);
            if((*(UINT32*)&hi) != (0x00234567 | (i<<23))) {err = 10100+i; goto error;}
            if((*(UINT32*)&lo) !=  0x89ABCDEF)            {err = 10100+i; goto error;}
        }
        for(int i = 1; i < 16; i++) {
            if(m_fregs[i*8+7] != i)    {err = 10200+i; goto error;}
            if(m_fregs[i*8+6] != 0x23) {err = 10200+i; goto error;}
            if(m_fregs[i*8+5] != 0x45) {err = 10200+i; goto error;}
            if(m_fregs[i*8+4] != 0x67) {err = 10200+i; goto error;}
            if(m_fregs[i*8+3] != 0x89) {err = 10200+i; goto error;}
            if(m_fregs[i*8+2] != 0xAB) {err = 10200+i; goto error;}
            if(m_fregs[i*8+1] != 0xCD) {err = 10200+i; goto error;}
            if(m_fregs[i*8+0] != 0xEF) {err = 10200+i; goto error;}
        }
    }
    
    err = memtest(true); if(err) goto error;
    err = memtest(false); if(err) goto error;
    
error:
    if(err) {
        fprintf(stderr, "NeXTdimension i860 emulator requires a little-endian host. This system seems to be big endian. Error %d. Exiting.\n", err);
        fflush(stderr);
        exit(err);
    }

    send_msg(MSG_I860_RESET);
    if(ConfigureParams.Dimension.bI860Thread)
        m_thread = host_thread_create(i860_thread, this);
}

void i860_cpu_device::uninit() {
    if(is_halted()) return;
    
	halt(true);

    if(m_thread) {
        send_msg(MSG_I860_KILL);
        host_thread_wait(m_thread);
        m_thread = NULL;
    }
    send_msg(MSG_NONE);
}

/* Message disaptcher - executed on i860 thread, safe to call i860 methods */
bool i860_cpu_device::handle_msgs() {
    host_lock(&m_port_lock);
    int msg = m_port;
    m_port = 0;
    host_unlock(&m_port_lock);
    
    if(msg & MSG_I860_KILL)
        return false;
    
    if(msg & MSG_I860_RESET)
        reset();
    else if(msg & MSG_INTR)
        intr();
    if(msg & MSG_DISPLAY_BLANK)
        nd_set_blank_state(ND_DISPLAY, host_blank_state(ND_SLOT, ND_DISPLAY));
    if(msg & MSG_VIDEO_BLANK)
        nd_set_blank_state(ND_VIDEO, host_blank_state(ND_SLOT, ND_VIDEO));
    if(msg & MSG_DBG_BREAK)
        debugger('d', "BREAK at pc=%08X", m_pc);
    return true;
}

void i860_cpu_device::run() {
    while(handle_msgs()) {
        
        /* Sleep a bit if halted */
        if(is_halted()) {
            host_sleep_ms(100);
            continue;
        }
        
        /* Run some i860 cycles before re-checking messages*/
        for(int i = 16; --i >= 0;)
            run_cycle();
    }
}

void i860_cpu_device::interrupt() {
    send_msg(MSG_INTR);
}

const char* i860_cpu_device::reports(double realTime, double hostTime) {
    double dVT = hostTime - m_last_vt;
    
    if(is_halted()) {
        m_report[0] = 0;
    } else {
        if(dVT == 0) dVT = 0.0001;
        sprintf(m_report, "i860:{MIPS=%.1f icache_hit=%lld%% tlb_hit=%lld%% icach_inval/s=%.0f tlb_inval/s=%.0f intr/s=%0.f}",
                               (m_insn_decoded / (dVT*1000*1000)),
                               m_icache_hit+m_icache_miss == 0 ? 0 : (100 * m_icache_hit) / (m_icache_hit+m_icache_miss) ,
                               m_tlb_hit+m_tlb_miss       == 0 ? 0 : (100 * m_tlb_hit)    / (m_tlb_hit+m_tlb_miss),
                               (m_icache_inval)/dVT,
                               (m_tlb_inval)/dVT,
                               (m_intrs)/dVT
                               );
        
        m_insn_decoded  = 0;
        m_icache_hit    = 0;
        m_icache_miss   = 0;
        m_icache_inval  = 0;
        m_tlb_hit       = 0;
        m_tlb_miss      = 0;
        m_tlb_inval     = 0;
        m_intrs         = 0;

        m_last_rt = realTime;
        m_last_vt = hostTime;
    }
    
    return m_report;
}

offs_t i860_cpu_device::disasm(char* buffer, offs_t pc) {
    return pc + i860_disassembler(pc, ifetch_notrap(pc), buffer);
}

/**************************************************************************
 * The actual decode and execute code.
 **************************************************************************/
#include "i860dec.cpp"

/**************************************************************************
 * The debugger code.
 **************************************************************************/
#include "i860dbg.cpp"