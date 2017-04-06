#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "dimension.h"
#include "sysdeps.h"
#include "nbic.h"

#define LOG_NEXTBUS_LEVEL   LOG_NONE

/* NeXTbus and NeXTbus Interface Chip emulation */

#define NUM_SLOTS	15


/* NBIC Registers */

struct {
	Uint32 control;
	Uint32 id;
	Uint8 intstatus;
	Uint8 intmask;
} nbic;


/* Control: 0x02020000 (rw), only non-Turbo */
#define NBIC_CTRL_IGNSID0	0x10000000 /* Ignore slot ID bit 0 */
#define NBIC_CTRL_STFWD		0x08000000 /* Store forward */
#define NBIC_CTRL_RMCOL		0x04000000 /* Read modify cycle collision */

/* ID: 0x02020004 (w), 0xF0FFFFFx (r), only non-Turbo */
#define NBIC_ID_VALID		0x80000000
#define NBIC_ID_M_MASK		0x7FFF0000 /* Manufacturer ID */
#define NBIC_ID_B_MASK		0x0000FFFF /* Board ID */

/* Interrupt: 0xF0FFFFE8 (r) */
#define NBIC_INT_STATUS		0x80

/* Interrupt mask: 0xF0FFFFEC (rw) */
#define NBIC_INT_MASK		0x80


/* Register access functions */
static Uint8 nbic_control_read0(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] Control (byte 0) read at %08X",addr);
	return (nbic.control>>24);
}
static Uint8 nbic_control_read1(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] Control (byte 1) read at %08X",addr);
	return (nbic.control>>16);
}
static Uint8 nbic_control_read2(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] Control (byte 2) read at %08X",addr);
	return (nbic.control>>8);
}
static Uint8 nbic_control_read3(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] Control (byte 3) read at %08X",addr);
	return nbic.control;
}

static void nbic_control_write0(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] Control (byte 0) write %02X at %08X",val,addr);
	nbic.control &= 0x00FFFFFF;
	nbic.control |= (val&0xFF)<<24;
}
static void nbic_control_write1(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] Control (byte 1) write %02X at %08X",val,addr);
	nbic.control &= 0xFF00FFFF;
	nbic.control |= (val&0xFF)<<16;
}
static void nbic_control_write2(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] Control (byte 2) write %02X at %08X",val,addr);
	nbic.control &= 0xFFFF00FF;
	nbic.control |= (val&0xFF)<<8;
}
static void nbic_control_write3(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] Control (byte 3) write %02X at %08X",val,addr);
	nbic.control &= 0xFFFFFF00;
	nbic.control |= val&0xFF;
}


static Uint8 nbic_id_read0(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] ID (byte 0) read at %08X",addr);
	return (nbic.id>>24);
}
static Uint8 nbic_id_read1(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] ID (byte 1) read at %08X",addr);
	return (nbic.id>>16);
}
static Uint8 nbic_id_read2(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] ID (byte 2) read at %08X",addr);
	return (nbic.id>>8);
}
static Uint8 nbic_id_read3(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] ID (byte 3) read at %08X",addr);
	return nbic.id;
}

static void nbic_id_write0(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] ID (byte 0) write %02X at %08X",val,addr);
	nbic.id &= 0x00FFFFFF;
	nbic.id |= (val&0xFF)<<24;
}
static void nbic_id_write1(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] ID (byte 1) write %02X at %08X",val,addr);
	nbic.id &= 0xFF00FFFF;
	nbic.id |= (val&0xFF)<<16;
}
static void nbic_id_write2(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] ID (byte 2) write %02X at %08X",val,addr);
	nbic.id &= 0xFFFF00FF;
	nbic.id |= (val&0xFF)<<8;
}
static void nbic_id_write3(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] ID (byte 3) write %02X at %08X",val,addr);
	nbic.id &= 0xFFFFFF00;
	nbic.id |= val&0xFF;
}

static Uint8 nbic_intstatus_read(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] Interrupt status read at %08X",addr);
	return nbic.intstatus;
}

static Uint8 nbic_intmask_read(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] Interrupt mask read at %08X",addr);
	return nbic.intmask;
}
static void nbic_intmask_write(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] Interrupt mask write %02X at %08X",val,addr);
	nbic.intmask = val;
}

static Uint8 nbic_zero_read(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] zero read at %08X",addr);
	return 0;
}
static void nbic_zero_write(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] zero write %02X at %08X",val,addr);
}

static Uint8 nbic_bus_error_read(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NBIC] bus error read at %08X",addr);
	M68000_BusError(addr, 1);
	return 0;
}
static void nbic_bus_error_write(Uint32 addr, Uint8 val) {
	Log_Printf(LOG_WARN, "[NBIC] bus error write at %08X",addr);
	M68000_BusError(addr, 0);
}


/* Direct register access */
static Uint8 (*nbic_read_reg[8])(Uint32) = {
	nbic_control_read0, nbic_control_read1, nbic_control_read2, nbic_control_read3,
	nbic_bus_error_read, nbic_bus_error_read, nbic_bus_error_read, nbic_bus_error_read
};

static void (*nbic_write_reg[8])(Uint32, Uint8) = {
	nbic_control_write0, nbic_control_write1, nbic_control_write2, nbic_control_write3,
	nbic_id_write0, nbic_id_write1, nbic_id_write2, nbic_id_write3
};


Uint32 nbic_reg_lget(Uint32 addr) {
	Uint32 val = 0;
	
	if (addr&3) {
		Log_Printf(LOG_WARN, "[NBIC] Unaligned access at %08X.",addr);
		abort();
	}
	
	if ((addr&0x0000FFFF)>7) {
		nbic_bus_error_read(addr);
	} else {
		val = nbic_read_reg[addr&7](addr)<<24;
		val |= nbic_read_reg[(addr&7)+1](addr+1)<<16;
		val |= nbic_read_reg[(addr&7)+2](addr+2)<<8;
		val |= nbic_read_reg[(addr&7)+3](addr+3);
	}
	
	return val;
}

Uint32 nbic_reg_wget(uaecptr addr) {
	Uint32 val = 0;
	
	if (addr&1) {
		Log_Printf(LOG_WARN, "[NBIC] Unaligned access at %08X.",addr);
		abort();
	}
	
	if ((addr&0x0000FFFF)>7) {
		nbic_bus_error_read(addr);
	} else {
		val = nbic_read_reg[addr&7](addr)<<8;
		val |= nbic_read_reg[(addr&7)+1](addr);
	}
	
	return val;
}

Uint32 nbic_reg_bget(uaecptr addr) {
	if ((addr&0x0000FFFF)>7) {
		return nbic_bus_error_read(addr);
	} else {
		return nbic_read_reg[addr&7](addr);
	}
}

void nbic_reg_lput(uaecptr addr, Uint32 l) {
	if (addr&3) {
		Log_Printf(LOG_WARN, "[NBIC] Unaligned access at %08X.",addr);
		abort();
	}
	
	if ((addr&0x0000FFFF)>7) {
		nbic_bus_error_write(addr,0);
	} else {
		nbic_write_reg[addr&7](addr,l>>24);
		nbic_write_reg[(addr&7)+1](addr,l>>16);
		nbic_write_reg[(addr&7)+2](addr,l>>8);
		nbic_write_reg[(addr&7)+3](addr,l);
	}
}

void nbic_reg_wput(uaecptr addr, Uint32 w) {
	if (addr&1) {
		Log_Printf(LOG_WARN, "[NBIC] Unaligned access at %08X.",addr);
		abort();
	}
	
	if ((addr&0x0000FFFF)>7) {
		nbic_bus_error_write(addr,0);
	} else {
		nbic_write_reg[addr&7](addr,w>>8);
		nbic_write_reg[(addr&7)+1](addr,w);
	}
}

void nbic_reg_bput(uaecptr addr, Uint32 b) {
	if ((addr&0x0000FFFF)>7) {
		nbic_bus_error_write(addr,0);
	} else {
		nbic_write_reg[addr&7](addr,b);
	}
}


/* NeXTbus CPU board slot space access */
static Uint8 (*nbic_read_cpu_slot[32])(Uint32) = {
	nbic_bus_error_read, nbic_bus_error_read, nbic_bus_error_read, nbic_bus_error_read,
	nbic_bus_error_read, nbic_bus_error_read, nbic_bus_error_read, nbic_bus_error_read,
	nbic_intstatus_read, nbic_zero_read, nbic_zero_read, nbic_zero_read,
	nbic_intmask_read, nbic_zero_read, nbic_zero_read, nbic_zero_read,

	nbic_id_read0, nbic_zero_read, nbic_zero_read, nbic_zero_read,
	nbic_id_read1, nbic_zero_read, nbic_zero_read, nbic_zero_read,
	nbic_id_read2, nbic_zero_read, nbic_zero_read, nbic_zero_read,
	nbic_id_read3, nbic_zero_read, nbic_zero_read, nbic_zero_read,
};

static void (*nbic_write_cpu_slot[32])(Uint32, Uint8) = {
	nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write,
	nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write,
	nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write,
	nbic_intmask_write, nbic_zero_write, nbic_zero_write, nbic_zero_write,
	
	nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write,
	nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write,
	nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write,
	nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write, nbic_bus_error_write
};

static Uint32 nb_cpu_slot_lget(Uint32 addr) {
	Uint32 val = 0;
	
	if (addr&3) {
		Log_Printf(LOG_WARN, "[NBIC] Unaligned access at %08X.",addr);
		abort();
	}
	
	if ((addr&0x00FFFFFF)<0x00FFFFE8) {
		nbic_bus_error_read(addr);
	} else {
		val = nbic_read_cpu_slot[addr&0x1F](addr)<<24;
		val |= nbic_read_cpu_slot[(addr&0x1F)+1](addr+1)<<16;
		val |= nbic_read_cpu_slot[(addr&0x1F)+2](addr+2)<<8;
		val |= nbic_read_cpu_slot[(addr&0x1F)+3](addr+3);
	}
	
	return val;
}

static Uint16 nb_cpu_slot_wget(Uint32 addr) {
	Uint32 val = 0;
	
	if (addr&1) {
		Log_Printf(LOG_WARN, "[NBIC] Unaligned access at %08X.",addr);
		abort();
	}
	
	if ((addr&0x00FFFFFF)<0x00FFFFE8) {
		nbic_bus_error_read(addr);
	} else {
		val = nbic_read_cpu_slot[addr&0x1F](addr)<<8;
		val |= nbic_read_cpu_slot[(addr&0x1F)+1](addr+1)<<16;
	}
	
	return val;
}

static Uint8 nb_cpu_slot_bget(Uint32 addr) {
	if ((addr&0x00FFFFFF)<0x00FFFFE8) {
		return nbic_bus_error_read(addr);
	} else {
		return nbic_read_cpu_slot[addr&0x1F](addr);
	}
}

static void nb_cpu_slot_lput(Uint32 addr, Uint32 l) {
	if (addr&3) {
		Log_Printf(LOG_WARN, "[NBIC] Unaligned access at %08X.",addr);
		abort();
	}
	
	if ((addr&0x00FFFFFF)<0x00FFFFE8) {
		nbic_bus_error_write(addr,0);
	} else {
		nbic_write_cpu_slot[addr&0x1F](addr,l>>24);
		nbic_write_cpu_slot[(addr&0x1F)+1](addr,l>>16);
		nbic_write_cpu_slot[(addr&0x1F)+2](addr,l>>8);
		nbic_write_cpu_slot[(addr&0x1F)+3](addr,l);
	}
}

static void nb_cpu_slot_wput(Uint32 addr, Uint16 w) {
	if (addr&1) {
		Log_Printf(LOG_WARN, "[NBIC] Unaligned access at %08X.",addr);
		abort();
	}
	
	if ((addr&0x00FFFFFF)<0x00FFFFE8) {
		nbic_bus_error_write(addr,0);
	} else {
		nbic_write_cpu_slot[addr&0x1F](addr,w>>8);
		nbic_write_cpu_slot[(addr&0x1F)+1](addr,w);
	}
}

static void nb_cpu_slot_bput(Uint32 addr, Uint8 b) {
	if ((addr&0x00FFFFFF)<0x00FFFFE8) {
		nbic_bus_error_write(addr,0);
	} else {
		nbic_write_cpu_slot[addr&0x1F](addr,b);
	}
}


/* NeXTbus timeout functions */
static Uint32 nb_timeout_lget(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NextBus] Bus error lget at %08X",addr);
	
	M68000_BusError(addr, 1);
	return 0;
}

static Uint16 nb_timeout_wget(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NextBus] Bus error wget at %08X",addr);
	
	M68000_BusError(addr, 1);
	return 0;
}

static Uint8 nb_timeout_bget(Uint32 addr) {
	Log_Printf(LOG_WARN, "[NextBus] Bus error bget at %08X",addr);
	
	M68000_BusError(addr, 1);
	return 0;
}

static void nb_timeout_lput(Uint32 addr, Uint32 l) {
	Log_Printf(LOG_WARN, "[NextBus] Bus error lput at %08X",addr);
	
	M68000_BusError(addr, 0);
}

static void nb_timeout_wput(Uint32 addr, Uint16 w) {
	Log_Printf(LOG_WARN, "[NextBus] Bus error lput at %08X",addr);
	
	M68000_BusError(addr, 0);
}

static void nb_timeout_bput(Uint32 addr, Uint8 b) {
	Log_Printf(LOG_WARN, "[NextBus] Bus error lput at %08X",addr);
	
	M68000_BusError(addr, 0);
}


/* Board access via NeXTbus */

typedef struct {
	Uint32 (*lget)(Uint32 addr);
	Uint16 (*wget)(Uint32 addr);
	Uint8 (*bget)(Uint32 addr);
	void (*lput)(Uint32 addr, Uint32 val);
	void (*wput)(Uint32 addr, Uint16 val);
	void (*bput)(Uint32 addr, Uint8 val);
} nextbus_access_funcs;

/* Board space access functions */
static nextbus_access_funcs nextbus_board[NUM_SLOTS] = {
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
};

/* Slot space access functions */
static nextbus_access_funcs nextbus_slot[16] = {
	{ nb_cpu_slot_lget, nb_cpu_slot_wget, nb_cpu_slot_bget, nb_cpu_slot_lput, nb_cpu_slot_wput, nb_cpu_slot_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput },
	{ nb_timeout_lget, nb_timeout_wget, nb_timeout_bget, nb_timeout_lput, nb_timeout_wput, nb_timeout_bput }
};


/* Slot memory */
Uint32 nextbus_slot_lget(Uint32 addr) {
	int slot;
	slot = (addr & 0x0F000000)>>24;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Slot %i: lget at %08X",slot,addr);
	
	return nextbus_slot[slot].lget(addr);
}

Uint32 nextbus_slot_wget(Uint32 addr) {
	int slot;
	slot = (addr & 0x0F000000)>>24;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Slot %i: wget at %08X",slot,addr);
	
	return nextbus_slot[slot].wget(addr);
}

Uint32 nextbus_slot_bget(Uint32 addr) {
	int slot;
	slot = (addr & 0x0F000000)>>24;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Slot %i: bget at %08X",slot,addr);
	
	return nextbus_slot[slot].bget(addr);
}

void nextbus_slot_lput(Uint32 addr, Uint32 val) {
	int slot;
	slot = (addr & 0x0F000000)>>24;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Slot %i: lput at %08X, val %08X",slot,addr,val);
	
	nextbus_slot[slot].lput(addr, val);
}

void nextbus_slot_wput(Uint32 addr, Uint32 val) {
	int slot;
	slot = (addr & 0x0F000000)>>24;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Slot %i: wput at %08X, val %04X",slot,addr,val);
	
	nextbus_slot[slot].wput(addr, val);
}

void nextbus_slot_bput(Uint32 addr, Uint32 val) {
	int slot;
	slot = (addr & 0x0F000000)>>24;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Slot %i: bput at %08X, val %02X",slot,addr,val);
	
	nextbus_slot[slot].bput(addr, val);
}

/* Board memory */
Uint32 nextbus_board_lget(Uint32 addr) {
	int slot;
	slot = (addr & 0xF0000000)>>28;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Board %i: lget at %08X",slot,addr);
	
	return nextbus_board[slot].lget(addr);
}

Uint32 nextbus_board_wget(Uint32 addr) {
	int slot;
	slot = (addr & 0xF0000000)>>28;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Board %i: wget at %08X",slot,addr);
	
	return nextbus_board[slot].wget(addr);
}

Uint32 nextbus_board_bget(Uint32 addr) {
	int slot;
	slot = (addr & 0xF0000000)>>28;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Board %i: bget at %08X",slot,addr);
	
	return nextbus_board[slot].bget(addr);
}

void nextbus_board_lput(Uint32 addr, Uint32 val) {
	int slot;
	slot = (addr & 0xF0000000)>>28;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Board %i: lput at %08X, val %08X",slot,addr,val);

	nextbus_board[slot].lput(addr, val);
}

void nextbus_board_wput(Uint32 addr, Uint32 val) {
	int slot;
	slot = (addr & 0xF0000000)>>28;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Board %i: wput at %08X, val %04X",slot,addr,val);
	
	nextbus_board[slot].wput(addr, val);
}

void nextbus_board_bput(Uint32 addr, Uint32 val) {
	int slot;
	slot = (addr & 0xF0000000)>>28;
	Log_Printf(LOG_NEXTBUS_LEVEL, "[NextBus] Board %i: bput at %08X, val %02X",slot,addr,val);
	
	nextbus_board[slot].bput(addr, val);
}

/* Init function for NextBus */
void nextbus_init(void) {
    if (ConfigureParams.Dimension.bEnabled && (ConfigureParams.System.nMachineType == NEXT_CUBE030 || ConfigureParams.System.nMachineType == NEXT_CUBE040)) {
        Log_Printf(LOG_WARN, "[NextBus/ND] board at slot %i",ND_SLOT);
        
        nextbus_board[ND_SLOT].lget = nd_board_lget;
        nextbus_board[ND_SLOT].wget = nd_board_wget;
        nextbus_board[ND_SLOT].bget = nd_board_bget;
        nextbus_board[ND_SLOT].lput = nd_board_lput;
        nextbus_board[ND_SLOT].wput = nd_board_wput;
        nextbus_board[ND_SLOT].bput = nd_board_bput;
        nextbus_slot[ND_SLOT].lget = nd_slot_lget;
        nextbus_slot[ND_SLOT].wget = nd_slot_wget;
        nextbus_slot[ND_SLOT].bget = nd_slot_bget;
        nextbus_slot[ND_SLOT].lput = nd_slot_lput;
        nextbus_slot[ND_SLOT].wput = nd_slot_wput;
        nextbus_slot[ND_SLOT].bput = nd_slot_bput;
        
        dimension_init();
	} else {
		nextbus_board[ND_SLOT].lget = nb_timeout_lget;
		nextbus_board[ND_SLOT].wget = nb_timeout_wget;
		nextbus_board[ND_SLOT].bget = nb_timeout_bget;
		nextbus_board[ND_SLOT].lput = nb_timeout_lput;
		nextbus_board[ND_SLOT].wput = nb_timeout_wput;
		nextbus_board[ND_SLOT].bput = nb_timeout_bput;
		nextbus_slot[ND_SLOT].lget = nb_timeout_lget;
		nextbus_slot[ND_SLOT].wget = nb_timeout_wget;
		nextbus_slot[ND_SLOT].bget = nb_timeout_bget;
		nextbus_slot[ND_SLOT].lput = nb_timeout_lput;
		nextbus_slot[ND_SLOT].wput = nb_timeout_wput;
		nextbus_slot[ND_SLOT].bput = nb_timeout_bput;
		
		dimension_uninit();
	}
}
