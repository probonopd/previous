#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "dimension.h"
#include "sysdeps.h"
#include "sysReg.h"
#include "nd_nbic.h"
#include "i860cfg.h"

/* NeXTdimention NBIC */
#define ND_NBIC_ID		0xC0000001
#define ND_NBIC_INTR    0x80

static volatile struct {
    Uint32 control;
    Uint32 id;
    Uint8  intstatus;
    Uint8  intmask;
} nd_nbic;

#if 0 // We keep this code around in case registers turn out to be accessible. For now, just for reference.
static Uint8 nd_nbic_control_read0(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC Control (byte 0) read at %08X",addr);
    return (nd_nbic.control>>24);
}
static Uint8 nd_nbic_control_read1(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC Control (byte 1) read at %08X",addr);
    return (nd_nbic.control>>16);
}
static Uint8 nd_nbic_control_read2(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC Control (byte 2) read at %08X",addr);
    return (nd_nbic.control>>8);
}
static Uint8 nd_nbic_control_read3(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC Control (byte 3) read at %08X",addr);
    return nd_nbic.control;
}

static void nd_nbic_control_write0(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC Control (byte 0) write %02X at %08X",val,addr);
    nd_nbic.control &= 0x00FFFFFF;
    nd_nbic.control |= (val&0xFF)<<24;
}
static void nd_nbic_control_write1(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC Control (byte 1) write %02X at %08X",val,addr);
    nd_nbic.control &= 0xFF00FFFF;
    nd_nbic.control |= (val&0xFF)<<16;
}
static void nd_nbic_control_write2(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC Control (byte 2) write %02X at %08X",val,addr);
    nd_nbic.control &= 0xFFFF00FF;
    nd_nbic.control |= (val&0xFF)<<8;
}
static void nd_nbic_control_write3(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC Control (byte 3) write %02X at %08X",val,addr);
    nd_nbic.control &= 0xFFFFFF00;
    nd_nbic.control |= val&0xFF;
}

static void nd_nbic_id_write0(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC ID (byte 0) write %02X at %08X",val,addr);
    nd_nbic.id &= 0x00FFFFFF;
    nd_nbic.id |= (val&0xFF)<<24;
}
static void nd_nbic_id_write1(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC ID (byte 1) write %02X at %08X",val,addr);
    nd_nbic.id &= 0xFF00FFFF;
    nd_nbic.id |= (val&0xFF)<<16;
}
static void nd_nbic_id_write2(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC ID (byte 2) write %02X at %08X",val,addr);
    nd_nbic.id &= 0xFFFF00FF;
    nd_nbic.id |= (val&0xFF)<<8;
}
static void nd_nbic_id_write3(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC ID (byte 3) write %02X at %08X",val,addr);
    nd_nbic.id &= 0xFFFFFF00;
    nd_nbic.id |= val&0xFF;
}
#endif

static Uint8 nd_nbic_id_read0(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC ID (byte 0) read at %08X",addr);
    return (nd_nbic.id>>24);
}
static Uint8 nd_nbic_id_read1(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC ID (byte 1) read at %08X",addr);
    return (nd_nbic.id>>16);
}
static Uint8 nd_nbic_id_read2(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC ID (byte 2) read at %08X",addr);
    return (nd_nbic.id>>8);
}
static Uint8 nd_nbic_id_read3(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC ID (byte 3) read at %08X",addr);
    return nd_nbic.id;
}

static Uint8 nd_nbic_intstatus_read(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC Interrupt status read at %08X",addr);
    return nd_nbic.intstatus;
}

static Uint8 nd_nbic_intmask_read(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC Interrupt mask read at %08X",addr);
    return nd_nbic.intmask;
}
static void nd_nbic_intmask_write(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC Interrupt mask write %02X at %08X",val,addr);
    nd_nbic.intmask = val;
}

static Uint8 nd_nbic_zero_read(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC zero read at %08X",addr);
    return 0;
}
static void nd_nbic_zero_write(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC zero write %02X at %08X",val,addr);
}

static Uint8 nd_nbic_bus_error_read(Uint32 addr) {
    Log_Printf(ND_LOG_IO_RD, "[ND] NBIC bus error read at %08X",addr);
    M68000_BusError(addr, 1);
    return 0;
}
static void nd_nbic_bus_error_write(Uint32 addr, Uint8 val) {
    Log_Printf(ND_LOG_IO_WR, "[ND] NBIC bus error write at %08X",addr);
    M68000_BusError(addr, 0);
}

static Uint8 (*nd_nbic_read[32])(Uint32) = {
    nd_nbic_bus_error_read, nd_nbic_bus_error_read, nd_nbic_bus_error_read, nd_nbic_bus_error_read,
    nd_nbic_bus_error_read, nd_nbic_bus_error_read, nd_nbic_bus_error_read, nd_nbic_bus_error_read,
    nd_nbic_intstatus_read, nd_nbic_zero_read, nd_nbic_zero_read, nd_nbic_zero_read,
    nd_nbic_intmask_read, nd_nbic_zero_read, nd_nbic_zero_read, nd_nbic_zero_read,
    
    nd_nbic_id_read0, nd_nbic_zero_read, nd_nbic_zero_read, nd_nbic_zero_read,
    nd_nbic_id_read1, nd_nbic_zero_read, nd_nbic_zero_read, nd_nbic_zero_read,
    nd_nbic_id_read2, nd_nbic_zero_read, nd_nbic_zero_read, nd_nbic_zero_read,
    nd_nbic_id_read3, nd_nbic_zero_read, nd_nbic_zero_read, nd_nbic_zero_read,
};

static void (*nd_nbic_write[32])(Uint32, Uint8) = {
    nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write,
    nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write,
    nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write,
    nd_nbic_intmask_write, nd_nbic_zero_write, nd_nbic_zero_write, nd_nbic_zero_write,
    
    nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write,
    nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write,
    nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write,
    nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write, nd_nbic_bus_error_write
};


/* NeXTdimension NBIC access */
Uint32 nd_nbic_lget(Uint32 addr) {
    Uint32 val = 0;
    
    if (addr&3) {
        Log_Printf(LOG_WARN, "[ND] NBIC Unaligned access at %08X.",addr);
        abort();
    }
    val = nd_nbic_read[addr&0x1F](addr)<<24;
    val |= nd_nbic_read[(addr&0x1F)+1](addr+1)<<16;
    val |= nd_nbic_read[(addr&0x1F)+2](addr+2)<<8;
    val |= nd_nbic_read[(addr&0x1F)+3](addr+3);
    
    return val;
}

Uint16 nd_nbic_wget(Uint32 addr) {
    Uint32 val = 0;
    
    if (addr&1) {
        Log_Printf(LOG_WARN, "[ND] NBIC Unaligned access at %08X.",addr);
        abort();
    }
    val = nd_nbic_read[addr&0x1F](addr)<<8;
    val |= nd_nbic_read[(addr&0x1F)+1](addr+1)<<16;
    
    return val;
}

Uint8 nd_nbic_bget(Uint32 addr) {
    return nd_nbic_read[addr&0x1F](addr);
}

void nd_nbic_lput(Uint32 addr, Uint32 l) {
    if (addr&3) {
        Log_Printf(LOG_WARN, "[ND] NBIC Unaligned access at %08X.",addr);
        abort();
    }
    nd_nbic_write[addr&0x1F](addr,l>>24);
    nd_nbic_write[(addr&0x1F)+1](addr,l>>16);
    nd_nbic_write[(addr&0x1F)+2](addr,l>>8);
    nd_nbic_write[(addr&0x1F)+3](addr,l);
}

void nd_nbic_wput(Uint32 addr, Uint16 w) {
    if (addr&1) {
        Log_Printf(LOG_WARN, "[ND] NBIC Unaligned access at %08X.",addr);
        abort();
    }
    nd_nbic_write[addr&0x1F](addr,w>>8);
    nd_nbic_write[(addr&0x1F)+1](addr,w);
}

void nd_nbic_bput(Uint32 addr, Uint8 b) {
    nd_nbic_write[addr&0x1F](addr,b);
}

void nd_nbic_set_intstatus(bool set) {
	if (set) {
        nd_nbic.intstatus |= ND_NBIC_INTR;
	} else {
        nd_nbic.intstatus &= ~ND_NBIC_INTR;
	}
}


/* Reset function */

void nd_nbic_init(void) {
    nd_nbic.id = ND_NBIC_ID;
    /* Release any interrupt that may be pending */
    nd_nbic.intmask   = 0;
    nd_nbic.intstatus = 0;
    set_interrupt(INT_REMOTE, RELEASE_INT);
}

/* Interrupt functions */
void nd_nbic_interrupt(void) {
    if (nd_nbic.intmask&nd_nbic.intstatus&ND_NBIC_INTR) {
        set_interrupt(INT_REMOTE, SET_INT);
    } else {
        set_interrupt(INT_REMOTE, RELEASE_INT);
    }
}
