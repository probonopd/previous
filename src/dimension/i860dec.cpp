/***************************************************************************

    i860dec.cpp

    Execution engine for the Intel i860 emulator.

    Copyright (C) 1995-present Jason Eckhardt (jle@rice.edu)
    Released for general non-commercial use under the MAME license
    with the additional requirement that you are free to use and
    redistribute this code in modified or unmodified form, provided
    you list me in the credits.
    Visit http://mamedev.org for licensing and usage restrictions.

    Changes for previous/NeXTdimension by Simon Schubiger (SC)
 
***************************************************************************/

/*
 * References:
 *  `i860 Microprocessor Programmer's Reference Manual', Intel, 1990.
 *
 * This code was originally written by Jason Eckhardt as part of an
 * emulator for some i860-based Unix workstations (early 1990's) such
 * as the Stardent Vistra 800 series and the OkiStation/i860 7300 series.
 * The code you are reading now is the i860 CPU portion only, which has
 * been adapted to (and simplified for) MAME.
 * MAME-specific notes:
 * - i860XR emulation only (i860XP unnecessary for MAME).
 * - No emulation of data and instruction caches (unnecessary for MAME version).
 * - No emulation of DIM mode or CS8 mode (unnecessary for MAME version).
 * - No BL/IL/locked sequences (unnecessary for MAME).
 * NeXTdimension specfic notes:
 * - (SC) Added support for i860's MSB/LSB-first mode (BE = 1/0).
 * - (SC) We assume that the host CPU is little endian (for now, will be fixed)
 * - (SC) Instruction cache implemented (not present in MAME version)
 * - (SC) Added dual-instruction-mode support (removed in MAME version)
 * - (SC) Added rounding mode support and insn_fix
 * - (AG) Added machine independent floating point emulation library
 * Generic notes:
 * - There is some amount of code duplication (e.g., see the
 *   various insn_* routines for the branches and FP routines) that
 *   could be eliminated.
 * - The host's floating point types are used to emulate the i860's
 *   floating point.  Should probably be made machine independent by
 *   using an IEEE FP emulation library.  On the other hand, most machines
 *   today also use IEEE FP.
 *
 */

#define DELAY_SLOT_PC() ((m_dim == DIM_FULL) ? 12 : 8)
#define DELAY_SLOT() do{\
    m_pc += 4; \
    UINT32 insn = ifetch(orig_pc+4);\
    decode_exec(insn); \
    if((m_dim == DIM_FULL) || (m_flow & DIM_OP)) {\
        m_pc += 4; \
        decode_exec(ifetch(orig_pc+8)); \
    } \
    m_pc = orig_pc;}while(0)

int i860_cpu_device::delay_slots(UINT32 insn) {
	int opc = (insn >> 26) & 0x3f;
	if (opc == 0x10 || opc == 0x1a || opc == 0x1b || opc == 0x1d ||
		opc == 0x1f || opc == 0x2d || (opc == 0x13 && (insn & 3) == 2))
        return m_dim ? 2 : 1;
    return 0;
}

void i860_cpu_device::intr() {
    m_flow |= EXT_INTR;
}

/* This is the external interface for indicating an external interrupt
   to the i860.  */
void i860_cpu_device::gen_interrupt()
{
	/* If interrupts are enabled, then set PSR.IN and prepare for trap.
	   Otherwise, the external interrupt is ignored.  We also set
	   bit EPSR.INT (which tracks the INT pin).  */
	if (GET_PSR_IM ()) {
		SET_PSR_IN (1);
		m_flow |= TRAP_WAS_EXTERNAL;
	}
    SET_EPSR_INT (1);

#if TRACE_EXT_INT
    Log_Printf(LOG_WARN, "[i860] i860_gen_interrupt: External interrupt received %s", GET_PSR_IM() ? "[PSR.IN set, preparing to trap]" : "[ignored (interrupts disabled)]");
#endif
#if ENABLE_PERF_COUNTERS
    m_intrs++;
#endif
}


/* This is the external interface for indicating an external interrupt
 to the i860.  */
void i860_cpu_device::clr_interrupt() {
    SET_EPSR_INT (0);
}

void i860_cpu_device::invalidate_icache() {
    memset(m_icache_vaddr, 0xff, sizeof(UINT32) * (1<<I860_ICACHE_SZ));
#if ENABLE_PERF_COUNTERS
    m_icache_inval++;
#endif
}

void i860_cpu_device::invalidate_tlb() {
    memset(m_tlb_vaddr, 0xff, sizeof(UINT32) * (1<<I860_TLB_SZ));
#if ENABLE_PERF_COUNTERS
    m_tlb_inval++;
#endif
}

UINT32 i860_cpu_device::ifetch_notrap(const UINT32 pc) {
    UINT32 before     = m_flow;
    m_flow &= ~TRAP_MASK;
    UINT32 result  = ifetch(pc);
    m_flow = before;
    return result;
}

UINT32 i860_cpu_device::ifetch(const UINT32 pc) {
    return pc & 4 ? ifetch64(pc) >> 32 : ifetch64(pc);
}

UINT64 i860_cpu_device::ifetch64(const UINT32 pc, const UINT32 vaddr, int const cidx) {
#if ENABLE_PERF_COUNTERS
    m_icache_miss++;
#endif
    UINT32 paddr;
    
    if (GET_DIRBASE_ATE ()) {
        paddr = get_address_translation (pc, 0  /* is_dataref */, 0 /* is_write */) & ~7;
        m_flow &= ~EXITING_IFETCH;
        if (PENDING_TRAP() && (GET_PSR_DAT () || GET_PSR_IAT ())) {
            m_flow |= EXITING_IFETCH;
            return 0xffeeffeeffeeffeeLL;
        }
    } else
        paddr = vaddr;
    
    m_icache_vaddr[cidx] = vaddr;
    UINT64 insn64;
    if (GET_DIRBASE_CS8()) {
        insn64  = rdcs8(paddr+7); insn64 <<= 8;
        insn64 |= rdcs8(paddr+6); insn64 <<= 8;
        insn64 |= rdcs8(paddr+5); insn64 <<= 8;
        insn64 |= rdcs8(paddr+4); insn64 <<= 8;
        insn64 |= rdcs8(paddr+3); insn64 <<= 8;
        insn64 |= rdcs8(paddr+2); insn64 <<= 8;
        insn64 |= rdcs8(paddr+1); insn64 <<= 8;
        insn64 |= rdcs8(paddr+0);
    } else {
        nd_board_rd64_be(paddr, (UINT32*)&insn64);
    }
    m_icache[cidx] = insn64;
    
    return insn64;
}

inline UINT64 i860_cpu_device::ifetch64(const UINT32 pc) {
    const UINT32 vaddr = pc & ~7;
    const int    cidx = (vaddr>>3) & I860_ICACHE_MASK;
    if(m_icache_vaddr[cidx] != vaddr) {
        return ifetch64(pc, vaddr, cidx);
    } else {
#if ENABLE_PERF_COUNTERS
        m_icache_hit++;
#endif
        return m_icache[cidx];
    }
}

/* Given a virtual address, perform the i860 address translation and
   return the corresponding physical address.
     vaddr:      virtual address
     is_dataref: 1 = load/store, 0 = instruction fetch.
     is_write:   1 = writing to vaddr, 0 = reading from vaddr
   The last two arguments are only used to determine what types
   of traps should be taken.

   Page tables must always be in memory (not cached).  So the routine
   here only accesses memory.  
 
 (SC) added TLB support. Read access updates even entries, Write access updates odd entries.
 TLB lookup checks both entries. R/W separation is for DPS copy loops.
 */
inline UINT32 i860_cpu_device::get_address_translation (UINT32 vaddr, int is_dataref, int is_write)
{
    UINT32 voffset        = vaddr & I860_PAGE_OFF_MASK;
    UINT32 tlbidx         = ((vaddr << 1) | is_write) & I860_TLB_MASK;
    
    if(m_tlb_vaddr[tlbidx] == (vaddr & I860_PAGE_FRAME_MASK)) {
#if ENABLE_PERF_COUNTERS
        m_tlb_hit++;
#endif
        return (m_tlb_paddr[tlbidx] & I860_PAGE_FRAME_MASK) + voffset;
    }

    if(m_tlb_vaddr[tlbidx ^ 1] == (vaddr & I860_PAGE_FRAME_MASK)) {
#if ENABLE_PERF_COUNTERS
        m_tlb_hit++;
#endif
        return (m_tlb_paddr[tlbidx ^ 1] & I860_PAGE_FRAME_MASK) + voffset;
    }
    
    return get_address_translation(vaddr, voffset, tlbidx, is_dataref, is_write);
}

UINT32 i860_cpu_device::get_address_translation(UINT32 vaddr, UINT32 voffset, UINT32 tlbidx, int is_dataref, int is_write) {
#if ENABLE_PERF_COUNTERS
    m_tlb_miss++;
#endif

    UINT32 vpage          = (vaddr >> I860_PAGE_SZ) & 0x3ff;
    UINT32 vdir           = (vaddr >> 22) & 0x3ff;
	UINT32 dtb            = (m_cregs[CR_DIRBASE]) & I860_PAGE_FRAME_MASK;
	UINT32 pg_dir_entry_a = 0;
	UINT32 pg_dir_entry   = 0;
	UINT32 pg_tbl_entry_a = 0;
	UINT32 pg_tbl_entry   = 0;
	UINT32 pfa1           = 0;
	UINT32 pfa2           = 0;
	UINT32 ret            = 0;
	UINT32 ttpde          = 0;
	UINT32 ttpte          = 0;

	assert (GET_DIRBASE_ATE ());

	/* Get page directory entry at DTB:DIR:00.  */
	pg_dir_entry_a = dtb | (vdir << 2);
    nd_board_rd32_le(pg_dir_entry_a, &pg_dir_entry);

	/* Check for non-present PDE.  */
	if (!(pg_dir_entry & 1))
	{
		/* PDE is not present, generate DAT or IAT.  */
		if (is_dataref)
			SET_PSR_DAT (1);
		else
			SET_PSR_IAT (1);
		m_flow |= TRAP_NORMAL;

		/* Dummy return.  */
		return 0;
	}

	/* PDE Check for write protection violations.  */
	if (is_write && is_dataref
		&& !(pg_dir_entry & 2)                  /* W = 0.  */
		&& (GET_PSR_U () || GET_EPSR_WP ()))   /* PSR_U = 1 or EPSR_WP = 1.  */
	{
		SET_PSR_DAT (1);
        m_flow |= TRAP_NORMAL;
		/* Dummy return.  */
		return 0;
	}

	/* PDE Check for user-mode access to supervisor pages.  */
	if (GET_PSR_U ()
		&& !(pg_dir_entry & 4))                 /* U = 0.  */
	{
		if (is_dataref)
			SET_PSR_DAT (1);
		else
			SET_PSR_IAT (1);
		m_flow |= TRAP_NORMAL;
		/* Dummy return.  */
		return 0;
	}

	/* FIXME: How exactly to handle A check/update?.  */

	/* Get page table entry at PFA1:PAGE:00.  */
	pfa1 = pg_dir_entry & I860_PAGE_FRAME_MASK;
	pg_tbl_entry_a = pfa1 | (vpage << 2);
    nd_board_rd32_le(pg_tbl_entry_a, &pg_tbl_entry);

	/* Check for non-present PTE.  */
	if (!(pg_tbl_entry & 1))
	{
		/* PTE is not present, generate DAT or IAT.  */
		if (is_dataref)
			SET_PSR_DAT (1);
		else
			SET_PSR_IAT (1);
		m_flow |= TRAP_NORMAL;

		/* Dummy return.  */
		return 0;
	}

	/* PTE Check for write protection violations.  */
	if (is_write && is_dataref
		&& !(pg_tbl_entry & 2)                  /* W = 0.  */
		&& (GET_PSR_U () || GET_EPSR_WP ()))   /* PSR_U = 1 or EPSR_WP = 1.  */
	{
		SET_PSR_DAT (1);
		m_flow |= TRAP_NORMAL;
		/* Dummy return.  */
		return 0;
	}

	/* PTE Check for user-mode access to supervisor pages.  */
	if (GET_PSR_U ()
		&& !(pg_tbl_entry & 4))                 /* U = 0.  */
	{
		if (is_dataref)
			SET_PSR_DAT (1);
		else
			SET_PSR_IAT (1);
		m_flow |= TRAP_NORMAL;
		/* Dummy return.  */
		return 0;
	}

	/* Update A bit and check D bit.  */
	ttpde = pg_dir_entry | 0x20;
	ttpte = pg_tbl_entry | 0x20;
    nd_board_wr32_le(pg_dir_entry_a, &ttpde);
    nd_board_wr32_le(pg_tbl_entry_a, &ttpte);

	if (is_write && is_dataref && (pg_tbl_entry & 0x40) == 0)
	{
		/* Log_Printf(LOG_WARN, "[i860] DAT trap on write without dirty bit v%08X/p%08X\n",
		   vaddr, (pg_tbl_entry & ~0xfff)|voffset); */
		SET_PSR_DAT (1);
		m_flow |= TRAP_NORMAL;
		/* Dummy return.  */
		return 0;
	}

	pfa2 = (pg_tbl_entry & I860_PAGE_FRAME_MASK);
    
    m_tlb_vaddr[tlbidx] = vaddr & I860_PAGE_FRAME_MASK;
    m_tlb_paddr[tlbidx] = pfa2;
    
	ret = pfa2 | voffset;

#if TRACE_ADDR_TRANSLATION
	Log_Printf(LOG_WARN, "[i860] get_address_translation: virt(%08X) -> phys(%08X)\n", vaddr, ret);
#endif

	return ret;
}

/* Write memory emulation.
     addr = address to write.
     size = size of write in bytes.
     data = data to write.  */
inline void i860_cpu_device::writemem_emu (UINT32 addr, int size, UINT8 *data) {
#if TRACE_RDWR_MEM
	Log_Printf(LOG_WARN, "[i860] wrmem (ATE=%d) addr = %08X, size = %d, data = %08X\n", GET_DIRBASE_ATE (), addr, size, data); fflush(0);
#endif

#if ENABLE_DEBUGGER
    dbg_check_wr(addr, size, data);
#endif

	/* If virtual mode, do translation.  */
	if (GET_DIRBASE_ATE ())
	{
		UINT32 phys = get_address_translation (addr, 1 /* is_dataref */, 1 /* is_write */);
		if (PENDING_TRAP() && (GET_PSR_IAT () || GET_PSR_DAT ()))
		{
#if TRACE_PAGE_FAULT
            Log_Printf(LOG_WARN, "[i860] %08X: ## Page fault (writememi_emu) virt=%08X", m_pc, addr);
#endif
			SET_EXITING_MEMRW(EXITING_WRITEMEM);
			return;
		}
		addr = phys;
	}

#if ENABLE_I860_DB_BREAK
	/* First check for match to db register (before write).  */
	if (((addr & ~(size - 1)) == m_cregs[CR_DB]) && GET_PSR_BW ())
	{
		SET_PSR_DAT (1);
		m_flow |= TRAP_NORMAL;
		return;
	}
#endif
    
	/* Now do the actual write.  */
    wrmem[size](addr, (UINT32*)data);
}


/* Floating-point read mem routine.
     addr = address to read.
     size = size of read in bytes.
     dest = memory to put read data.  */
inline void i860_cpu_device::readmem_emu (UINT32 addr, int size, UINT8 *dest)
{
#if TRACE_RDWR_MEM
	Log_Printf(LOG_WARN, "[i860] fp_rdmem (ATE=%d) addr = %08X, size = %d\n", GET_DIRBASE_ATE (), addr, size); fflush(0);
#endif
    
	/* If virtual mode, do translation.  */
	if (GET_DIRBASE_ATE ())
	{
		UINT32 phys = get_address_translation (addr, 1 /* is_dataref */, 0 /* is_write */);
		if (PENDING_TRAP() && (GET_PSR_IAT () || GET_PSR_DAT ()))
		{
#if TRACE_PAGE_FAULT
			Log_Printf(LOG_WARN, "[i860] %08X: ## Page fault (fp_readmem_emu) virt=%08X",m_pc,addr);
//            debugger();
#endif
			SET_EXITING_MEMRW(EXITING_FPREADMEM);
			return;
		}
		addr = phys;
	}

#if ENABLE_I860_DB_BREAK
	/* First check for match to db register (before read).  */
	if (((addr & ~(size - 1)) == m_cregs[CR_DB]) && GET_PSR_BR ())
	{
		SET_PSR_DAT (1);
		m_flow |= TRAP_NORMAL;
		return;
	}
#endif
    rdmem[size](addr, (UINT32*)dest);
}


/* Floating-point write mem routine.
     addr = address to write.
     size = size of write in bytes.
     data = pointer to the data.
     wmask = bit mask of bytes to write (only for pst.d).  */
inline void i860_cpu_device::writemem_emu (UINT32 addr, int size, UINT8 *data, UINT32 wmask)
{
#if TRACE_RDWR_MEM
	Log_Printf(LOG_WARN, "[i860] fp_wrmem (ATE=%d) addr = %08X, size = %d", GET_DIRBASE_ATE (), addr, size); fflush(0);
#endif

	/* If virtual mode, do translation.  */
	if (GET_DIRBASE_ATE ())
	{
		UINT32 phys = get_address_translation (addr, 1 /* is_dataref */, 1 /* is_write */);
		if (PENDING_TRAP() && GET_PSR_DAT ())
		{
#if TRACE_PAGE_FAULT
			Log_Printf(LOG_WARN, "[i860] %08X: ## Page fault (fp_writememi_emu) virt=%08X", m_pc,addr);
//            debugger();
#endif
			SET_EXITING_MEMRW(EXITING_WRITEMEM);
			return;
		}
		addr = phys;
	}

#if ENABLE_I860_DB_BREAK
	/* First check for match to db register (before read).  */
	if (((addr & ~(size - 1)) == m_cregs[CR_DB]) && GET_PSR_BW ())
	{
		SET_PSR_DAT (1);
        m_flow |= TRAP_NORMAL;
		return;
	}
#endif
        
    if(size == 8 && wmask != 0xff) {
        if (wmask & 0x80) wrmem[1](addr+0, (UINT32*)&data[0]);
        if (wmask & 0x40) wrmem[1](addr+1, (UINT32*)&data[1]);
        if (wmask & 0x20) wrmem[1](addr+2, (UINT32*)&data[2]);
        if (wmask & 0x10) wrmem[1](addr+3, (UINT32*)&data[3]);
        if (wmask & 0x08) wrmem[1](addr+4, (UINT32*)&data[4]);
        if (wmask & 0x04) wrmem[1](addr+5, (UINT32*)&data[5]);
        if (wmask & 0x02) wrmem[1](addr+6, (UINT32*)&data[6]);
        if (wmask & 0x01) wrmem[1](addr+7, (UINT32*)&data[7]);
    } else {
        wrmem[size](addr, (UINT32*)data);
    }
}

/* Sign extend N-bit number.  */
inline INT32 sign_ext (UINT32 x, int n)
{
	INT32 t;
	t = x >> (n - 1);
	t = ((-t) << n) | x;
	return t;
}


void i860_cpu_device::unrecog_opcode (UINT32 pc, UINT32 insn) {
	debugger('d', "unrecognized opcode %08X pc=%08X", insn, pc);
    SET_PSR_IT (1);
    m_flow |= TRAP_NORMAL;
}


/* Execute "ld.c csrc2,idest" instruction.  */
void i860_cpu_device::insn_ld_ctrl (UINT32 insn)
{
	UINT32 csrc2 = get_creg (insn);
	UINT32 idest = get_idest (insn);

#if TRACE_UNDEFINED_I860
	if (csrc2 > 5)
	{
		/* Control register not between 0..5.  Undefined i860XR behavior.  */
		Log_Printf(LOG_WARN, "[i860:%08X] insn_ld_from_ctrl: bad creg in ld.c (ignored)", m_pc);
		return;
	}
#endif

	/* If this is a load of the fir, then there are two cases:
	   1. First load of fir after a trap = usual value.
	   2. Not first load of fir after a trap = address of the ld.c insn.  */
	if (csrc2 == CR_FIR)
	{
		if (m_flow & FIR_GETS_TRAP)
			set_iregval (idest, m_cregs[csrc2]);
		else
		{
			m_cregs[csrc2] = m_pc;
			set_iregval (idest, m_cregs[csrc2]);
		}
        m_flow &= ~FIR_GETS_TRAP;
	}
	else
		set_iregval (idest, m_cregs[csrc2]);
}


/* Execute "st.c isrc1,csrc2" instruction.  */
void i860_cpu_device::insn_st_ctrl (UINT32 insn)
{
	UINT32 csrc2 = get_creg (insn);
	UINT32 isrc1 = get_isrc1 (insn);

#if TRACE_UNDEFINED_I860
	if (csrc2 > 5)
	{
		/* Control register not between 0..5.  Undefined i860XR behavior.  */
		Log_Printf(LOG_WARN, "[i860:%08X] insn_st_to_ctrl: bad creg in st.c (ignored)", m_pc);
		return;
	}
#endif

    /* Look for CS8 bit turned off).  */
    if (csrc2 == CR_DIRBASE && (get_iregval (isrc1) & 0x80) == 0 && GET_DIRBASE_CS8()) {
        Log_Printf(LOG_WARN, "[i860:%08X] Leaving CS8 mode", m_pc);
		Statusbar_SetNdLed(2);
    }
    
	/* Look for ITI bit turned on (but it never actually is written --
	   it always appears to be 0).  */
	if (csrc2 == CR_DIRBASE && (get_iregval (isrc1) & 0x20))
	{
        invalidate_icache();
        invalidate_tlb();
        
		/* Make sure ITI isn't actually written.  */
		set_iregval (isrc1, (get_iregval (isrc1) & ~0x20));
	}

	if (csrc2 == CR_DIRBASE && (get_iregval (isrc1) & 1) && GET_DIRBASE_ATE () == 0){
		Log_Printf(LOG_WARN, "[i860:%08X]** Switching to virtual addressing (ATE=1)", m_pc);
	}

	/* Update the register -- unless it is fir which cannot be updated.  */
	if (csrc2 == CR_EPSR)
	{
		UINT32 enew = 0, tmp = 0;
		/* Make sure unchangeable EPSR bits stay unchanged (DCS, stepping,
		   and type).  Also, some bits are only writeable in supervisor
		   mode.  */
		if (GET_PSR_U ())
		{
			enew = get_iregval (isrc1) & ~(0x003e1fff | 0x00c06000);
			tmp = m_cregs[CR_EPSR] & (0x003e1fff | 0x00c06000);
		}
		else
		{
			enew = get_iregval (isrc1) & ~0x003e1fff;
			tmp = m_cregs[CR_EPSR] & 0x003e1fff;
		}
        if((enew ^ m_cregs[CR_EPSR]) & 0x00800000) { // BE/LE change
            set_mem_access((enew & 0x00800000) != 0);
        }
		m_cregs[CR_EPSR] = enew | tmp;
	}
	else if (csrc2 == CR_PSR)
	{
		/* Some PSR bits are only writeable in supervisor mode.  */
		if (GET_PSR_U ())
		{
			UINT32 enew = get_iregval (isrc1) & ~PSR_SUPERVISOR_ONLY_MASK;
			UINT32 tmp = m_cregs[CR_PSR] & PSR_SUPERVISOR_ONLY_MASK;
			m_cregs[CR_PSR] = enew | tmp;
		}
		else
			m_cregs[CR_PSR] = get_iregval (isrc1);
	}
	else if (csrc2 == CR_FSR)
	{
		/* I believe that only 21..17, 8..5, and 3..0 should be updated.  */
		UINT32 enew = get_iregval (isrc1) & 0x003e01ef;
		UINT32 tmp = m_cregs[CR_FSR] & ~0x003e01ef;
		m_cregs[CR_FSR] = enew | tmp;

		float_set_rounding_mode (GET_FSR_RM());
	}
	else if (csrc2 != CR_FIR)
		m_cregs[csrc2] = get_iregval (isrc1);
}


/* Execute "ld.{s,b,l} isrc1(isrc2),idest" or
   "ld.{s,b,l} #const(isrc2),idest".  */
void i860_cpu_device::insn_ldx (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	INT32 immsrc1 = sign_ext (get_imm16 (insn), 16);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 eff = 0;
	/* Operand size, in bytes.  */
	const int sizes[4] = { 1, 1, 2, 4};
	int size = 0;

	/* Bits 28 and 0 determine the operand size.  */
	size = sizes[((insn >> 27) & 2) | (insn & 1)];

    /* Bit 26 determines the addressing mode (reg+reg or disp+reg).  */
	/* Get effective address depending on disp+reg or reg+reg form.  */
	if (insn & 0x04000000)
	{
		/* Chop off lower bits of displacement.  */
		immsrc1 &= ~(size - 1);
		eff = (UINT32)(immsrc1 + (INT32)(get_iregval (isrc2)));
	}
	else
		eff = get_iregval (isrc1) + get_iregval (isrc2);

#if TRACE_UNALIGNED_MEM
	if (eff & (size - 1))
	{
		Log_Printf(LOG_WARN, "[i860:%08X] Unaligned access detected (%08X)", m_pc, eff);
		SET_PSR_DAT (1);
		m_flow |= TRAP_NORMAL;
		return;
	}
#endif

	/* The i860 sign-extends 8- or 16-bit integer loads.

	   Below, the readmemi_emu() needs to happen outside of the
	   set_iregval macro (otherwise the readmem won't occur if r0
	   is the target register).  */
	if (size < 4) {
        UINT32 readval = 0; readmem_emu(eff, size, (UINT8*)&readval);
        readval = sign_ext (readval, size * 8);
		/* Do not update register on page fault.  */
		if (GET_EXITING_MEMRW()) {
			return;
		}
		set_iregval (idest, readval);
	}
	else {
        UINT32 readval; readmem_emu(eff, size, (UINT8*)&readval);
		/* Do not update register on page fault.  */
		if (GET_EXITING_MEMRW()) {
			return;
		}
		set_iregval (idest, readval);
	}
}


/* Execute "st.x isrc1ni,#const(isrc2)" instruction (there is no
   (reg + reg form).  Store uses the split immediate, not the normal
   16-bit immediate as in ld.x.  */
void i860_cpu_device::insn_stx (UINT32 insn)
{
	INT32 immsrc = sign_ext ((((insn >> 5) & 0xf800) | (insn & 0x07ff)), 16);
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 eff = 0;
	/* Operand size, in bytes.  */
	const int sizes[4] = { 1, 1, 2, 4};
	int size = 0;

	/* Bits 28 and 0 determine the operand size.  */
	size = sizes[((insn >> 27) & 2) | (insn & 1)];

	/* FIXME: Do any necessary traps.  */

	/* Get effective address.  Chop off lower bits of displacement.  */
	immsrc &= ~(size - 1);
	eff = (UINT32)(immsrc + (INT32)get_iregval (isrc2));

	/* Write data (value of reg isrc1) to memory at eff.  */
    UINT32 tmp32 = get_iregval (isrc1);
	writemem_emu (eff, size, (UINT8*)&tmp32);
	if (GET_EXITING_MEMRW())
		return;
}


/* Execute "fst.y fdest,isrc1(isrc2)", "fst.y fdest,isrc1(isrc2)++",
           "fst.y fdest,#const(isrc2)" or "fst.y fdest,#const(isrc2)++"
   instruction.  */
void i860_cpu_device::insn_fsty (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	INT32 immsrc1 = sign_ext (get_imm16 (insn), 16);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	UINT32 eff = 0;
	/* Operand size, in bytes.  */
	const int sizes[4] = { 8, 4, 16, 4};
	int size = 0;
	int form_disp_reg = 0;
	int auto_inc = (insn & 1);

	/* Bits 2 and 1 determine the operand size.  */
	size = sizes[((insn >> 1) & 3)];

	/* Bit 26 determines the addressing mode (reg+reg or disp+reg).  */
	form_disp_reg = (insn & 0x04000000);

	/* FIXME: Check for undefined behavior, non-even or non-quad
	   register operands for fst.d and fst.q respectively.  */

	/* Get effective address depending on disp+reg or reg+reg form.  */
	if (form_disp_reg)
	{
		/* Chop off lower bits of displacement.  */
		immsrc1 &= ~(size - 1);
		eff = (UINT32)(immsrc1 + (INT32)(get_iregval (isrc2)));
	}
	else
		eff = get_iregval (isrc1) + get_iregval (isrc2);

#if TRACE_UNALIGNED_MEM
	if (eff & (size - 1))
	{
		Log_Printf(LOG_WARN, "[i860:%08X] Unaligned access detected (%08X)", m_pc, eff);
		SET_PSR_DAT (1);
		m_flow |= TRAP_NORMAL;
		return;
	}
#endif

	/* Do (post) auto-increment.  */
	if (auto_inc)
	{
		set_iregval (isrc2, eff);
#if TRACE_UNDEFINED_I860
		/* When auto-inc, isrc1 and isrc2 regs can't be the same.  */
		if (isrc1 == isrc2)
		{
			/* Undefined i860XR behavior.  */
			Log_Printf(LOG_WARN, "[i860:%08X] insn_fsty: isrc1 = isrc2 in fst with auto-inc (ignored)", m_pc);
			return;
		}
#endif
	}

	/* Write data (value of freg fdest) to memory at eff.  */
	writemem_emu (eff, size, (UINT8 *)(&m_fregs[4 * fdest]), 0xff);
}


/* Execute "fld.y isrc1(isrc2),fdest", "fld.y isrc1(isrc2)++,idest",
           "fld.y #const(isrc2),fdest" or "fld.y #const(isrc2)++,idest".
   Where y = {l,d,q}.  Note, there is no pfld.q, though.  */
void i860_cpu_device::insn_fldy (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	INT32 immsrc1 = sign_ext (get_imm16 (insn), 16);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	UINT32 eff = 0;
	/* Operand size, in bytes.  */
	const int sizes[4] = { 8, 4, 16, 4};
	int size = 0;
	int form_disp_reg = 0;
	int auto_inc = (insn & 1);
	int piped = (insn & 0x40000000);

	/* Bits 2 and 1 determine the operand size.  */
	size = sizes[((insn >> 1) & 3)];

	/* Bit 26 determines the addressing mode (reg+reg or disp+reg).  */
	form_disp_reg = (insn & 0x04000000);

#if TRACE_UNDEFINED_I860
	/* There is no pipelined load quad.  */
	if (piped && size == 16)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	/* FIXME: Check for undefined behavior, non-even or non-quad
	   register operands for fld.d and fld.q respectively.  */

	/* Get effective address depending on disp+reg or reg+reg form.  */
	if (form_disp_reg)
	{
		/* Chop off lower bits of displacement.  */
		immsrc1 &= ~(size - 1);
		eff = (UINT32)(immsrc1 + (INT32)(get_iregval (isrc2)));
	}
	else
		eff = get_iregval (isrc1) + get_iregval (isrc2);

	/* Do (post) auto-increment.  */
	if (auto_inc)
	{
		set_iregval (isrc2, eff);
#if TRACE_UNDEFINED_I860
		/* When auto-inc, isrc1 and isrc2 regs can't be the same.  */
		if (isrc1 == isrc2)
		{
			/* Undefined i860XR behavior.  */
			Log_Printf(LOG_WARN, "[i860:%08X] insn_fldy: isrc1 = isrc2 in fst with auto-inc (ignored)", m_pc);
			return;
		}
#endif
	}

#if TRACE_UNALIGNED_MEM
	if (eff & (size - 1))
	{
		Log_Printf(LOG_WARN, "[i860:%08X] Unaligned access detected (%08X)", m_pc, eff);
		SET_PSR_DAT (1);
        m_flow |= TRAP_NORMAL;
		return;
	}
#endif

	/* Update the load pipe if necessary.  */
	/* FIXME: Copy result-status bits to fsr from last stage.  */
	if (!piped)
	{
		/* Scalar version writes the current result to fdest.  */
		/* Read data at 'eff' into freg 'fdest' (reads to f0 or f1 are
		   thrown away).  */
        readmem_emu(eff, size, (UINT8 *)&(m_fregs[4 * fdest]));
		if (fdest < 2) {
            // (SC) special case with fdest=fr0/fr1. fr0 & fr1 are overwritten with values from mem
            // but always read as zero. Fix it.
            m_fregs[0] = 0; m_fregs[1] = 0; m_fregs[2] = 0; m_fregs[3] = 0;
            m_fregs[4] = 0; m_fregs[5] = 0; m_fregs[6] = 0; m_fregs[7] = 0;
        }
	}
	else
	{
		/* Read the data into a temp space first.  This way we can test
		   for any traps before updating the pipeline.  The pipeline must
		   stay unaffected after a trap so that the instruction can be
		   properly restarted.  */
		UINT8 bebuf[8];
		readmem_emu (eff, size, bebuf);
		if (PENDING_TRAP() && GET_EXITING_MEMRW())
			goto ab_op;

		/* Pipelined version writes fdest with the result from the last
		   stage of the pipeline, with precision specified by the LRP
		   bit of the stage's result-status bits.  */
#if 1 /* FIXME: WIP on FSR update.  This may not be correct.  */
		/* Copy 3rd stage LRP to FSR.  */
		if (m_L[1 /* 2 */].stat.lrp)
			m_cregs[CR_FSR] |= 0x04000000;
		else
			m_cregs[CR_FSR] &= ~0x04000000;
#endif
		if (m_L[2].stat.lrp)  /* 3rd (last) stage.  */
			set_fregval_d (fdest, m_L[2].val.d);
		else
			set_fregval_s (fdest, m_L[2].val.s);

		/* Now advance pipeline and write loaded data to first stage.  */
		m_L[2] = m_L[1];
		m_L[1] = m_L[0];
		if (size == 8) {
			m_L[0].val.d = *((FLOAT64*)bebuf);
			m_L[0].stat.lrp = 1;
		} else {
			m_L[0].val.s = *((FLOAT32*)bebuf);
			m_L[0].stat.lrp = 0;
		}
	}

	ab_op:;
}


/* Execute "pst.d fdest,#const(isrc2)" or "fst.d fdest,#const(isrc2)++"
   instruction.  */
void i860_cpu_device::insn_pstd (UINT32 insn)
{
	INT32 immsrc1 = sign_ext (get_imm16 (insn), 16);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	UINT32 eff = 0;
	int auto_inc = (insn & 1);
	int pm = GET_PSR_PM ();
	int i;
	UINT32 wmask;
	int orig_pm = pm;

	/* Get the pixel size, where:
	   PS: 0 = 8 bits, 1 = 16 bits, 2 = 32-bits.  */
	int ps = GET_PSR_PS ();

#if TRACE_UNDEFINED_I860
	if (!(ps == 0 || ps == 1 || ps == 2))
		Log_Printf(LOG_WARN, "[i860:%08X] insn_pstd: Undefined i860XR behavior, invalid value %d for pixel size", m_pc, ps);
#endif

#if TRACE_UNDEFINED_I860
	/* Bits 2 and 1 determine the operand size, which must always be
	   zero (indicating a 64-bit operand).  */
	if (insn & 0x6)
	{
		/* Undefined i860XR behavior.  */
		Log_Printf(LOG_WARN, "[i860:%08X] insn_pstd: bad operand size specifier", m_pc);
	}
#endif

	/* FIXME: Check for undefined behavior, non-even register operands.  */

	/* Get effective address.  Chop off lower bits of displacement.  */
	immsrc1 &= ~(8 - 1);
	eff = (UINT32)(immsrc1 + (INT32)(get_iregval (isrc2)));

#if TRACE_UNALIGNED_MEM
	if (eff & (8 - 1))
	{
		Log_Printf(LOG_WARN, "[i860:%08X] Unaligned access detected (%08X)", m_pc, eff);
		SET_PSR_DAT (1);
		m_flow |= TRAP_NORMAL;
		return;
	}
#endif

	/* Do (post) auto-increment.  */
	if (auto_inc)
		set_iregval (isrc2, eff);

	/* Update the pixel mask depending on the pixel size.  Shift PM
	   right by 8/2^ps bits.  */
	if (ps == 0)
		pm = (pm >> 8) & 0x00;
	else if (ps == 1)
		pm = (pm >> 4) & 0x0f;
	else if (ps == 2)
		pm = (pm >> 2) & 0x3f;
	SET_PSR_PM (pm);

	/* Write data (value of freg fdest) to memory at eff-- but only those
	   bytes that are enabled by the bits in PSR.PM.  Bit 0 of PM selects
	   the pixel at the lowest address.  */
	wmask = 0;
	for (i = 0; i < 8; )
	{
		if (ps == 0)
		{
			if (orig_pm & 0x80)
				wmask |= 1 << (7-i);
			i += 1;
		}
		else if (ps == 1)
		{
			if (orig_pm & 0x08)
				wmask |= 0x3 << (6-i);
			i += 2;
		}
		else if (ps == 2)
		{
			if (orig_pm & 0x02)
				wmask |= 0xf << (4-i);
			i += 4;
		}
		else
		{
			wmask = 0xff;
			break;
		}
		orig_pm <<= 1;
	}
	writemem_emu (eff, 8, (UINT8 *)(&m_fregs[4 * fdest]), wmask);
}


/* Execute "ixfr isrc1ni,fdest" instruction.  */
void i860_cpu_device::insn_ixfr (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 fdest = get_fdest (insn);
	UINT32 iv = 0;

	/* This is a bit-pattern transfer, not a conversion.  */
	iv = get_iregval (isrc1);
	set_fregval_s (fdest, *(FLOAT32 *)&iv);
}


/* Execute "addu isrc1,isrc2,idest".  */
void i860_cpu_device::insn_addu (UINT32 insn)
{
	UINT32 src1val;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 tmp_dest_val = 0;
	UINT64 tmp = 0;

	src1val = get_iregval (get_isrc1 (insn));

	/* We don't update the actual idest register now because below we
	   need to test the original src1 and src2 if either happens to
	   be the destination register.  */
	tmp_dest_val = src1val + get_iregval (isrc2);

	/* Set OF and CC flags.
	   For unsigned:
	     OF = bit 31 carry
	     CC = bit 31 carry.
	 */
	tmp = (UINT64)src1val + (UINT64)(get_iregval (isrc2));
	if ((tmp >> 32) & 1) {
		SET_PSR_CC (1);
		SET_EPSR_OF (1);
	} else {
		SET_PSR_CC (0);
		SET_EPSR_OF (0);
	}

	/* Now update the destination register.  */
	set_iregval (idest, tmp_dest_val);
}


/* Execute "addu #const,isrc2,idest".  */
void i860_cpu_device::insn_addu_imm (UINT32 insn)
{
	UINT32 src1val;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 tmp_dest_val = 0;
	UINT64 tmp = 0;

	src1val = sign_ext (get_imm16 (insn), 16);

	/* We don't update the actual idest register now because below we
	   need to test the original src1 and src2 if either happens to
	   be the destination register.  */
	tmp_dest_val = src1val + get_iregval (isrc2);

	/* Set OF and CC flags.
	   For unsigned:
	     OF = bit 31 carry
	     CC = bit 31 carry.
	 */
	tmp = (UINT64)src1val + (UINT64)(get_iregval (isrc2));
	if ((tmp >> 32) & 1)
	{
		SET_PSR_CC (1);
		SET_EPSR_OF (1);
	}
	else
	{
		SET_PSR_CC (0);
		SET_EPSR_OF (0);
	}

	/* Now update the destination register.  */
	set_iregval (idest, tmp_dest_val);
}


/* Execute "adds isrc1,isrc2,idest".  */
void i860_cpu_device::insn_adds (UINT32 insn)
{
	UINT32 src1val;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 tmp_dest_val = 0;
	int sa, sb, sres;

	src1val = get_iregval (get_isrc1 (insn));

	/* We don't update the actual idest register now because below we
	   need to test the original src1 and src2 if either happens to
	   be the destination register.  */
	tmp_dest_val = src1val + get_iregval (isrc2);

	/* Set OF and CC flags.
	   For signed:
	     OF = standard signed overflow.
	     CC set   if isrc2 < -isrc1
	     CC clear if isrc2 >= -isrc1
	 */
	sa = src1val & 0x80000000;
	sb = get_iregval (isrc2) & 0x80000000;
	sres = tmp_dest_val & 0x80000000;
	if (sa != sb && sa != sres)
		SET_EPSR_OF (1);
	else
		SET_EPSR_OF (0);

	if ((INT32)get_iregval (isrc2) < -(INT32)(src1val))
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	/* Now update the destination register.  */
	set_iregval (idest, tmp_dest_val);
}


/* Execute "adds #const,isrc2,idest".  */
void i860_cpu_device::insn_adds_imm (UINT32 insn)
{
	UINT32 src1val;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 tmp_dest_val = 0;
	int sa, sb, sres;

	src1val = sign_ext (get_imm16 (insn), 16);

	/* We don't update the actual idest register now because below we
	   need to test the original src1 and src2 if either happens to
	   be the destination register.  */
	tmp_dest_val = src1val + get_iregval (isrc2);

	/* Set OF and CC flags.
	   For signed:
	     OF = standard signed overflow.
	     CC set   if isrc2 < -isrc1
	     CC clear if isrc2 >= -isrc1
	 */
	sa = src1val & 0x80000000;
	sb = get_iregval (isrc2) & 0x80000000;
	sres = tmp_dest_val & 0x80000000;
	if (sa != sb && sa != sres)
		SET_EPSR_OF (1);
	else
		SET_EPSR_OF (0);

	if ((INT32)get_iregval (isrc2) < -(INT32)(src1val))
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	/* Now update the destination register.  */
	set_iregval (idest, tmp_dest_val);
}


/* Execute "subu isrc1,isrc2,idest".  */
void i860_cpu_device::insn_subu (UINT32 insn)
{
	UINT32 src1val;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 tmp_dest_val = 0;

	src1val = get_iregval (get_isrc1 (insn));

	/* We don't update the actual idest register now because below we
	   need to test the original src1 and src2 if either happens to
	   be the destination register.  */
	tmp_dest_val = src1val - get_iregval (isrc2);

	/* Set OF and CC flags.
	   For unsigned:
	     OF = NOT(bit 31 carry)
	     CC = bit 31 carry.
	     (i.e. CC set   if isrc2 <= isrc1
	           CC clear if isrc2 > isrc1
	 */
	if ((UINT32)get_iregval (isrc2) <= (UINT32)src1val)
	{
		SET_PSR_CC (1);
		SET_EPSR_OF (0);
	}
	else
	{
		SET_PSR_CC (0);
		SET_EPSR_OF (1);
	}

	/* Now update the destination register.  */
	set_iregval (idest, tmp_dest_val);
}


/* Execute "subu #const,isrc2,idest".  */
void i860_cpu_device::insn_subu_imm (UINT32 insn)
{
	UINT32 src1val;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 tmp_dest_val = 0;

	src1val = sign_ext (get_imm16 (insn), 16);

	/* We don't update the actual idest register now because below we
	   need to test the original src1 and src2 if either happens to
	   be the destination register.  */
	tmp_dest_val = src1val - get_iregval (isrc2);

	/* Set OF and CC flags.
	   For unsigned:
	     OF = NOT(bit 31 carry)
	     CC = bit 31 carry.
	     (i.e. CC set   if isrc2 <= isrc1
	           CC clear if isrc2 > isrc1
	 */
	if ((UINT32)get_iregval (isrc2) <= (UINT32)src1val)
	{
		SET_PSR_CC (1);
		SET_EPSR_OF (0);
	}
	else
	{
		SET_PSR_CC (0);
		SET_EPSR_OF (1);
	}

	/* Now update the destination register.  */
	set_iregval (idest, tmp_dest_val);
}


/* Execute "subs isrc1,isrc2,idest".  */
void i860_cpu_device::insn_subs (UINT32 insn)
{
	UINT32 src1val;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 tmp_dest_val = 0;
	int sa, sb, sres;

	src1val = get_iregval (get_isrc1 (insn));

	/* We don't update the actual idest register now because below we
	   need to test the original src1 and src2 if either happens to
	   be the destination register.  */
	tmp_dest_val = src1val - get_iregval (isrc2);

	/* Set OF and CC flags.
	   For signed:
	     OF = standard signed overflow.
	     CC set   if isrc2 > isrc1
	     CC clear if isrc2 <= isrc1
	 */
	sa = src1val & 0x80000000;
	sb = get_iregval (isrc2) & 0x80000000;
	sres = tmp_dest_val & 0x80000000;
	if (sa != sb && sa != sres)
		SET_EPSR_OF (1);
	else
		SET_EPSR_OF (0);

	if ((INT32)get_iregval (isrc2) > (INT32)(src1val))
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	/* Now update the destination register.  */
	set_iregval (idest, tmp_dest_val);
}


/* Execute "subs #const,isrc2,idest".  */
void i860_cpu_device::insn_subs_imm (UINT32 insn)
{
	UINT32 src1val;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 tmp_dest_val = 0;
	int sa, sb, sres;

	src1val = sign_ext (get_imm16 (insn), 16);

	/* We don't update the actual idest register now because below we
	   need to test the original src1 and src2 if either happens to
	   be the destination register.  */
	tmp_dest_val = src1val - get_iregval (isrc2);

	/* Set OF and CC flags.
	   For signed:
	     OF = standard signed overflow.
	     CC set   if isrc2 > isrc1
	     CC clear if isrc2 <= isrc1
	 */
	sa = src1val & 0x80000000;
	sb = get_iregval (isrc2) & 0x80000000;
	sres = tmp_dest_val & 0x80000000;
	if (sa != sb && sa != sres)
		SET_EPSR_OF (1);
	else
		SET_EPSR_OF (0);

	if ((INT32)get_iregval (isrc2) > (INT32)(src1val))
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	/* Now update the destination register.  */
	set_iregval (idest, tmp_dest_val);
}


/* Execute "shl isrc1,isrc2,idest".  */
void i860_cpu_device::insn_shl (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);

	src1val = get_iregval (get_isrc1 (insn));
	set_iregval (idest, get_iregval (isrc2) << src1val);
}


/* Execute "shl #const,isrc2,idest".  */
void i860_cpu_device::insn_shl_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);

	src1val = sign_ext (get_imm16 (insn), 16);
	set_iregval (idest, get_iregval (isrc2) << src1val);
}


/* Execute "shr isrc1,isrc2,idest".  */
void i860_cpu_device::insn_shr (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);

	src1val = get_iregval (get_isrc1 (insn));

	/* The iregs array is UINT32, so this is a logical shift.  */
	set_iregval (idest, get_iregval (isrc2) >> src1val);

	/* shr also sets the SC in psr (shift count).  */
	SET_PSR_SC (src1val);
}


/* Execute "shr #const,isrc2,idest".  */
void i860_cpu_device::insn_shr_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);

	src1val = sign_ext (get_imm16 (insn), 16);

	/* The iregs array is UINT32, so this is a logical shift.  */
	set_iregval (idest, get_iregval (isrc2) >> src1val);

	/* shr also sets the SC in psr (shift count).  */
	SET_PSR_SC (src1val);
}


/* Execute "shra isrc1,isrc2,idest".  */
void i860_cpu_device::insn_shra (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);

	src1val = get_iregval (get_isrc1 (insn));

	/* The iregs array is UINT32, so cast isrc2 to get arithmetic shift.  */
	set_iregval (idest, (INT32)get_iregval (isrc2) >> src1val);
}


/* Execute "shra #const,isrc2,idest".  */
void i860_cpu_device::insn_shra_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);

	src1val = sign_ext (get_imm16 (insn), 16);

	/* The iregs array is UINT32, so cast isrc2 to get arithmetic shift.  */
	set_iregval (idest, (INT32)get_iregval (isrc2) >> src1val);
}


/* Execute "shrd isrc1ni,isrc2,idest" instruction.  */
void i860_cpu_device::insn_shrd (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 sc = GET_PSR_SC ();
	UINT32 tmp;

	/* Do the operation:
	   idest = low_32(isrc1ni:isrc2 >> sc).  */
	if (sc == 0)
		tmp = get_iregval (isrc2);
	else
	{
		tmp = get_iregval (isrc1) << (32 - sc);
		tmp |= (get_iregval (isrc2) >> sc);
	}
	set_iregval (idest, tmp);
}


/* Execute "and isrc1,isrc2,idest".  */
void i860_cpu_device::insn_and (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	res = get_iregval (isrc1) & get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "and #const,isrc2,idest".  */
void i860_cpu_device::insn_and_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	src1val = get_imm16 (insn);
	res = src1val & get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "andh #const,isrc2,idest".  */
void i860_cpu_device::insn_andh_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	src1val = get_imm16 (insn);
	res = (src1val << 16) & get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "andnot isrc1,isrc2,idest".  */
void i860_cpu_device::insn_andnot (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	res = (~get_iregval (isrc1)) & get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "andnot #const,isrc2,idest".  */
void i860_cpu_device::insn_andnot_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	src1val = get_imm16 (insn);
	res = (~src1val) & get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "andnoth #const,isrc2,idest".  */
void i860_cpu_device::insn_andnoth_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	src1val = get_imm16 (insn);
	res = (~(src1val << 16)) & get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "or isrc1,isrc2,idest".  */
void i860_cpu_device::insn_or (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	res = get_iregval (isrc1) | get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "or #const,isrc2,idest".  */
void i860_cpu_device::insn_or_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	src1val = get_imm16 (insn);
	res = src1val | get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "orh #const,isrc2,idest".  */
void i860_cpu_device::insn_orh_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	src1val = get_imm16 (insn);
	res = (src1val << 16) | get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "xor isrc1,isrc2,idest".  */
void i860_cpu_device::insn_xor (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	res = get_iregval (isrc1) ^ get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "xor #const,isrc2,idest".  */
void i860_cpu_device::insn_xor_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	src1val = get_imm16 (insn);
	res = src1val ^ get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "xorh #const,isrc2,idest".  */
void i860_cpu_device::insn_xorh_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 idest = get_idest (insn);
	UINT32 res = 0;

	/* Do the operation.  */
	src1val = get_imm16 (insn);
	res = (src1val << 16) ^ get_iregval (isrc2);

	/* Set flags.  */
	if (res == 0)
		SET_PSR_CC (1);
	else
		SET_PSR_CC (0);

	set_iregval (idest, res);
}


/* Execute "trap isrc1ni,isrc2,idest" instruction.  */
void i860_cpu_device::insn_trap (UINT32 insn)
{
    debugger('d', "Software TRAP");
	SET_PSR_IT (1);
	m_flow |= TRAP_NORMAL;
}


/* Execute "intovr" instruction.  */
void i860_cpu_device::insn_intovr (UINT32 insn)
{
	if (GET_EPSR_OF ())
	{
		SET_PSR_IT (1);
		m_flow |= TRAP_NORMAL;
	}
}


/* Execute "bte isrc1,isrc2,sbroff".  */
void i860_cpu_device::insn_bte (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 target_addr = 0;
	INT32 sbroff = 0;
	int res = 0;

	src1val = get_iregval (get_isrc1 (insn));

	/* Compute the target address from the sbroff field.  */
	sbroff = sign_ext ((((insn >> 5) & 0xf800) | (insn & 0x07ff)), 16);
	target_addr = (INT32)m_pc + 4 + (sbroff << 2);

	/* Determine comparison result.  */
	res = (src1val == get_iregval (isrc2));

	/* Branch routines always update the PC.  */
	if (res)
		m_pc = target_addr;
	else
		m_pc += 4;

	SET_PC_UPDATED();
}


/* Execute "bte #const5,isrc2,sbroff".  */
void i860_cpu_device::insn_bte_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 target_addr = 0;
	INT32 sbroff = 0;
	int res = 0;

	src1val = (insn >> 11) & 0x1f;  /* 5-bit field, zero-extended.  */

	/* Compute the target address from the sbroff field.  */
	sbroff = sign_ext ((((insn >> 5) & 0xf800) | (insn & 0x07ff)), 16);
	target_addr = (INT32)m_pc + 4 + (sbroff << 2);

	/* Determine comparison result.  */
	res = (src1val == get_iregval (isrc2));

	/* Branch routines always update the PC.  */
	if (res)
		m_pc = target_addr;
	else
		m_pc += 4;

    SET_PC_UPDATED();
}


/* Execute "btne isrc1,isrc2,sbroff".  */
void i860_cpu_device::insn_btne (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 target_addr = 0;
	INT32 sbroff = 0;
	int res = 0;

	src1val = get_iregval (get_isrc1 (insn));

	/* Compute the target address from the sbroff field.  */
	sbroff = sign_ext ((((insn >> 5) & 0xf800) | (insn & 0x07ff)), 16);
	target_addr = (INT32)m_pc + 4 + (sbroff << 2);

	/* Determine comparison result.  */
	res = (src1val != get_iregval (isrc2));

	/* Branch routines always update the PC.  */
	if (res)
		m_pc = target_addr;
	else
		m_pc += 4;

    SET_PC_UPDATED();
}


/* Execute "btne #const5,isrc2,sbroff".  */
void i860_cpu_device::insn_btne_imm (UINT32 insn)
{
	UINT32 src1val = 0;
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 target_addr = 0;
	INT32 sbroff = 0;
	int res = 0;

	src1val = (insn >> 11) & 0x1f;  /* 5-bit field, zero-extended.  */

	/* Compute the target address from the sbroff field.  */
	sbroff = sign_ext ((((insn >> 5) & 0xf800) | (insn & 0x07ff)), 16);
	target_addr = (INT32)m_pc + 4 + (sbroff << 2);

	/* Determine comparison result.  */
	res = (src1val != get_iregval (isrc2));

	/* Branch routines always update the PC.  */
	if (res)
		m_pc = target_addr;
	else
		m_pc += 4;

    SET_PC_UPDATED();
}


/* Execute "bc lbroff" instruction.  */
void i860_cpu_device::insn_bc (UINT32 insn)
{
	UINT32 target_addr = 0;
	INT32 lbroff = 0;
	int res = 0;

	/* Compute the target address from the lbroff field.  */
	lbroff = sign_ext ((insn & 0x03ffffff), 26);
	target_addr = (INT32)m_pc + 4 + (lbroff << 2);

	/* Determine comparison result.  */
    res = m_dim_cc_valid ? m_dim_cc : (GET_PSR_CC () == 1);
    
	/* Branch routines always update the PC.  */
	if (res)
		m_pc = target_addr;
	else
		m_pc += 4;

    SET_PC_UPDATED();
}


/* Execute "bnc lbroff" instruction.  */
void i860_cpu_device::insn_bnc (UINT32 insn)
{
	UINT32 target_addr = 0;
	INT32 lbroff = 0;
	int res = 0;

	/* Compute the target address from the lbroff field.  */
	lbroff = sign_ext ((insn & 0x03ffffff), 26);
	target_addr = (INT32)m_pc + 4 + (lbroff << 2);

	/* Determine comparison result.  */
    res = m_dim_cc_valid ? !(m_dim_cc) : (GET_PSR_CC () == 0);

	/* Branch routines always update the PC, since pc_updated is set
	   in the decode routine.  */
	if (res)
		m_pc = target_addr;
	else
		m_pc += 4;

    SET_PC_UPDATED();
}


/* Execute "bc.t lbroff" instruction.  */
void i860_cpu_device::insn_bct (UINT32 insn)
{
	UINT32 target_addr = 0;
	INT32 lbroff = 0;
	int res = 0;
	UINT32 orig_pc = m_pc;

	/* Compute the target address from the lbroff field.  */
	lbroff = sign_ext ((insn & 0x03ffffff), 26);
	target_addr = (INT32)m_pc + 4 + (lbroff << 2);

	/* Determine comparison result.  */
	res = (GET_PSR_CC () == 1);

	/* Careful. Unlike bla, the delay slot instruction is only executed
	   if the branch is taken.  */
	if (res)
	{
		/* Execute delay slot instruction.  */
        DELAY_SLOT();
		if (PENDING_TRAP() )
		{
			m_flow |= TRAP_IN_DELAY_SLOT;
			goto ab_op;
		}
	}

	/* Since this branch is delayed, we must jump 2 or 3 instructions if
	   if isn't taken.  */
	if (res)
		m_pc = target_addr;
	else
        m_pc += DELAY_SLOT_PC();

    SET_PC_UPDATED();

	ab_op:
	;
}


/* Execute "bnc.t lbroff" instruction.  */
void i860_cpu_device::insn_bnct (UINT32 insn)
{
	UINT32 target_addr = 0;
	INT32 lbroff = 0;
	int res = 0;
	UINT32 orig_pc = m_pc;

	/* Compute the target address from the lbroff field.  */
	lbroff = sign_ext ((insn & 0x03ffffff), 26);
	target_addr = (INT32)m_pc + 4 + (lbroff << 2);

	/* Determine comparison result.  */
    res = (GET_PSR_CC () == 0);

	/* Careful. Unlike bla, the delay slot instruction is only executed
	   if the branch is taken.  */
	if (res)
	{
		/* Execute delay slot instruction.  */
        DELAY_SLOT();
		if (PENDING_TRAP() )
		{
			m_flow |= TRAP_IN_DELAY_SLOT;
			goto ab_op;
		}
	}

	/* Since this branch is delayed, we must jump 2 or 3 instructions if if isn't taken.  */
	if (res)
		m_pc = target_addr;
	else
        m_pc += DELAY_SLOT_PC();

    SET_PC_UPDATED();

	ab_op:
	;
}


/* Execute "call lbroff" instruction.  */
void i860_cpu_device::insn_call (UINT32 insn)
{
	UINT32 target_addr = 0;
	INT32 lbroff = 0;
	UINT32 orig_pc = m_pc;

	/* Compute the target address from the lbroff field.  */
	lbroff = sign_ext ((insn & 0x03ffffff), 26);
	target_addr = (INT32)m_pc + 4 + (lbroff << 2);

	/* Execute the delay slot instruction.  */
    DELAY_SLOT();
	if (PENDING_TRAP() )
	{
		m_flow |= TRAP_IN_DELAY_SLOT;
		goto ab_op;
	}

	/* Sets the return pointer (r1).  */
	set_iregval (1, orig_pc + DELAY_SLOT_PC());

	/* New target.  */
	m_pc = target_addr;
    SET_PC_UPDATED();

	ab_op:;
}


/* Execute "br lbroff".  */
void i860_cpu_device::insn_br (UINT32 insn)
{
	UINT32 target_addr = 0;
	INT32 lbroff = 0;
	UINT32 orig_pc = m_pc;

	/* Compute the target address from the lbroff field.  */
	lbroff = sign_ext ((insn & 0x03ffffff), 26);
	target_addr = (INT32)m_pc + 4 + (lbroff << 2);

	/* Execute the delay slot instruction.  */
    DELAY_SLOT();
	if (PENDING_TRAP() )
	{
		m_flow |= TRAP_IN_DELAY_SLOT;
		goto ab_op;
	}

	/* New target.  */
	m_pc = target_addr;
    SET_PC_UPDATED();

	ab_op:;
}


/* Execute "bri isrc1ni" instruction.
   Note: I didn't merge this code with calli because bri must do
   a lot of flag manipulation if any trap bits are set.  */
void i860_cpu_device::insn_bri (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 orig_pc = m_pc;
	UINT32 orig_psr = m_cregs[CR_PSR];
	UINT32 orig_src1_val = get_iregval (isrc1);

#if 1 /* TURBO.  */
	m_cregs[CR_PSR] &= ~PSR_ALL_TRAP_BITS_MASK;
#endif

    if(m_dim && PENDING_TRAP())
        goto ab_op;
    
	/* Execute the delay slot instruction.  */
    DELAY_SLOT();

	/* Delay slot insn caused a trap, abort operation.  */
	if (PENDING_TRAP() )
	{
		m_flow |= TRAP_IN_DELAY_SLOT;
		goto ab_op;
	}

	/* If any trap bits are set, we need to do the return from
	   trap work.  Note, we must use the PSR value that existed
	   before the delay slot instruction was executed since the
	   delay slot instruction might itself cause a trap bit to
	   be set.  */
	if (orig_psr & PSR_ALL_TRAP_BITS_MASK)
	{
		/* Restore U and IM from their previous copies.  */
		SET_PSR_U (GET_PSR_PU ());
		SET_PSR_IM (GET_PSR_PIM ());

        ret_from_trap();
	}

	/* Update PC.  */
	m_pc = orig_src1_val;

    SET_PC_UPDATED();
	ab_op:;
}

/* Execute "calli isrc1ni" instruction.  */
void i860_cpu_device::insn_calli (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 orig_pc = m_pc;
	UINT32 orig_src1_val = get_iregval (isrc1);

#if TRACE_UNDEFINED_I860
	/* Check for undefined behavior.  */
	if (isrc1 == 1)
	{
		/* Src1 must not be r1.  */
		Log_Printf(LOG_WARN, "[i860:%08X] insn_calli: isrc1 = r1 on a calli", m_pc);
	}
#endif

	/* Set return pointer before executing delay slot instruction.  */
	set_iregval (1, m_pc + DELAY_SLOT_PC());

	/* Execute the delay slot instruction.  */
    DELAY_SLOT();
	if (PENDING_TRAP() )
	{
		set_iregval (1, orig_src1_val);
		m_flow |= TRAP_IN_DELAY_SLOT;
		goto ab_op;
	}

	/* Set new PC.  */
	m_pc = orig_src1_val;
    SET_PC_UPDATED();

	ab_op:;
}


/* Execute "bla isrc1ni,isrc2,sbroff" instruction.  */
void i860_cpu_device::insn_bla (UINT32 insn)
{
	UINT32 isrc1 = get_isrc1 (insn);
	UINT32 isrc2 = get_isrc2 (insn);
	UINT32 target_addr = 0;
	INT32 sbroff = 0;
	int lcc_tmp = 0;
	UINT32 orig_pc = m_pc;
	UINT32 orig_isrc2val = get_iregval (isrc2);

#if TRACE_UNDEFINED_I860
	/* Check for undefined behavior.  */
	if (isrc1 == isrc2)
	{
		/* Src1 and src2 the same is undefined i860XR behavior.  */
		Log_Printf(LOG_WARN,  "[i860:%08X] insn_bla: isrc1 and isrc2 are the same (ignored)", m_pc);
		return;
	}
#endif

	/* Compute the target address from the sbroff field.  */
	sbroff = sign_ext ((((insn >> 5) & 0xf800) | (insn & 0x07ff)), 16);
	target_addr = (INT32)m_pc + 4 + (sbroff << 2);

	/* Determine comparison result based on opcode.  */
	lcc_tmp = ((INT32)get_iregval (isrc2) >= -(INT32)get_iregval (isrc1));

	set_iregval (isrc2, get_iregval (isrc1) + orig_isrc2val);

	/* Execute the delay slot instruction.  */
    DELAY_SLOT();
	if (PENDING_TRAP() )
	{
		m_flow |= TRAP_IN_DELAY_SLOT;
		goto ab_op;
	}

	if (GET_PSR_LCC ())
		m_pc = target_addr;
	else
	{
		/* Since this branch is delayed, we must jump 2 or 3 instructions if if isn't taken.  */
        m_pc += DELAY_SLOT_PC();
	}
	SET_PSR_LCC (lcc_tmp);

    SET_PC_UPDATED();
	ab_op:;
}


/* Execute "flush #const(isrc2)" or "flush #const(isrc2)++" instruction.  */
void i860_cpu_device::insn_flush (UINT32 insn)
{
	UINT32 src1val = sign_ext (get_imm16 (insn), 16);
	UINT32 isrc2 = get_isrc2 (insn);
	int auto_inc = (insn & 1);
	UINT32 eff = 0;

	/* Technically, idest should be encoded as r0 because idest
	   is undefined after the instruction.  We don't currently
	   check for this.

	   Flush D$ block at address #const+isrc2.  Block is undefined
	   after.  The effective address must be 16-byte aligned.

	   FIXME: Need to examine RB and RC and do this right.
	  */

	/* Chop off lower bits of displacement to 16-byte alignment.  */
	src1val &= ~(16-1);
	eff = src1val + get_iregval (isrc2);
	if (auto_inc)
		set_iregval (isrc2, eff);

	/* In user mode, the flush is ignored.  */
	if (GET_PSR_U () == 0)
	{
		/* If line is dirty, write it to memory and invalidate.
		   NOTE: The actual dirty write is unimplemented in the MAME version
		   as we don't emulate the dcache.  */
	}
}


/* Execute "[p]fmul.{ss,sd,dd} fsrc1,fsrc2,fdest" instruction or
   pfmul3.dd fsrc1,fsrc2,fdest.

   The pfmul3.dd differs from pfmul.dd in that it treats the pipeline
   as 3 stages, even though it is a double precision multiply.  */
void i860_cpu_device::insn_fmul (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
	int res_prec = insn & 0x080;     /* 1 = double, 0 = single.  */
	int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */
	FLOAT64 dbl_tmp_dest = FLOAT64_ZERO;
	FLOAT32 sgl_tmp_dest = FLOAT32_ZERO;
	FLOAT64 dbl_last_stage_contents = FLOAT64_ZERO;
	FLOAT32 sgl_last_stage_contents = FLOAT32_ZERO;
	int is_pfmul3 = insn & 0x4;
	int num_stages = (src_prec && !is_pfmul3) ? 2 : 3;

#if TRACE_UNDEFINED_I860
	/* Only .dd is valid for pfmul.  */
	if (is_pfmul3 && (insn & 0x180) != 0x180)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}

	/* Check for invalid .ds combination.  */
	if ((insn & 0x180) == 0x100)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	/* For pipelined version, retrieve the contents of the last stage
	   of the pipeline, whose precision is specified by the MRP bit
	   of the stage's result-status bits.  Note for pfmul, the number
	   of stages is determined by the source precision of the current
	   operation.  */
	if (piped)
	{
		if (m_M[num_stages - 1].stat.mrp)
			dbl_last_stage_contents = m_M[num_stages - 1].val.d;
		else
			sgl_last_stage_contents = m_M[num_stages - 1].val.s;
	}

	/* Do the operation, being careful about source and result
	   precision.  */
	if (src_prec)
	{
		FLOAT64 v1 = get_fregval_d (fsrc1);
		FLOAT64 v2 = get_fregval_d (fsrc2);

		/* For pipelined mul, if fsrc2 is the same as fdest, then the last
		   stage is bypassed to fsrc2 (rather than using the value in fsrc2).
		   This bypass is not available for fsrc1, and is undefined behavior.  */
		if (0 && piped && fdest != 0 && fsrc1 == fdest)
			v1 = dbl_last_stage_contents;
		if (piped && fdest != 0 && fsrc2 == fdest)
			v2 = dbl_last_stage_contents;

		if (res_prec)
			dbl_tmp_dest = float64_mul (v1, v2);
		else
			sgl_tmp_dest = float64_to_float32 (float64_mul (v1, v2));
	}
	else
	{
		FLOAT32 v1 = get_fregval_s (fsrc1);
		FLOAT32 v2 = get_fregval_s (fsrc2);

		/* For pipelined mul, if fsrc2 is the same as fdest, then the last
		   stage is bypassed to fsrc2 (rather than using the value in fsrc2).
		   This bypass is not available for fsrc1, and is undefined behavior.  */
		if (0 && piped && fdest != 0 && fsrc1 == fdest)
			v1 = sgl_last_stage_contents;
		if (piped && fdest != 0 && fsrc2 == fdest)
			v2 = sgl_last_stage_contents;

		if (res_prec)
			dbl_tmp_dest = float64_mul (float32_to_float64 (v1), float32_to_float64 (v2));
		else
			sgl_tmp_dest = float32_mul (v1, v2);
	}

	/* FIXME: Set result-status bits besides MRP. And copy to fsr from
	          last stage.  */
	/* FIXME: Scalar version flows through all stages.  */
	/* FIXME: Mixed precision (only weird for pfmul).  */
	if (!piped)
	{
		/* Scalar version writes the current calculation to the fdest
		   register, with precision specified by the R bit.  */
		if (res_prec)
			set_fregval_d (fdest, dbl_tmp_dest);
		else
			set_fregval_s (fdest, sgl_tmp_dest);
	}
	else
	{
		/* Pipelined version writes fdest with the result from the last
		   stage of the pipeline.  */
#if 1 /* FIXME: WIP on FSR update.  This may not be correct.  */
		/* Copy 3rd stage MRP to FSR.  */
		if (m_M[num_stages - 2  /* 1 */].stat.mrp)
			m_cregs[CR_FSR] |= 0x10000000;
		else
			m_cregs[CR_FSR] &= ~0x10000000;
#endif

		if (m_M[num_stages - 1].stat.mrp)
			set_fregval_d (fdest, dbl_last_stage_contents);
		else
			set_fregval_s (fdest, sgl_last_stage_contents);

		/* Now advance pipeline and write current calculation to
		   first stage.  */
		if (num_stages == 3)
		{
			m_M[2] = m_M[1];
			m_M[1] = m_M[0];
		}
		else
			m_M[1]  = m_M[0];

		if (res_prec)
		{
			m_M[0].val.d = dbl_tmp_dest;
			m_M[0].stat.mrp = 1;
		}
		else
		{
			m_M[0].val.s = sgl_tmp_dest;
			m_M[0].stat.mrp = 0;
		}
	}
}


/* Execute "fmlow.dd fsrc1,fsrc2,fdest" instruction.  */
void i860_cpu_device::insn_fmlow (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);

	FLOAT64 v1 = get_fregval_d (fsrc1);
	FLOAT64 v2 = get_fregval_d (fsrc2);
	INT64 i1 = *(UINT64 *)&v1;
	INT64 i2 = *(UINT64 *)&v2;
	INT64 tmp = 0;

#if TRACE_UNDEFINED_I860
	/* Only .dd is valid for fmlow.  */
	if ((insn & 0x180) != 0x180)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	/* The lower 32-bits are obvious.  What exactly goes in the upper
	   bits?
	   Technically, the upper-most 10 bits are undefined, but i'd like
	   to be undefined in the same way as the real i860 if possible.  */

	/* Keep lower 53 bits of multiply.  */
    tmp = i1 * i2;
	tmp &= 0x001fffffffffffffULL;
	tmp |= (i1 & 0x8000000000000000LL) ^ (i2 & 0x8000000000000000LL);
	set_fregval_d (fdest, *(FLOAT64 *)&tmp);
}


/* Execute [p]fadd.{ss,sd,dd} fsrc1,fsrc2,fdest (.ds disallowed above).  */
void i860_cpu_device::insn_fadd_sub (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
	int res_prec = insn & 0x080;     /* 1 = double, 0 = single.  */
	int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */
	int is_sub = insn & 1;           /* 1 = sub, 0 = add.  */
	FLOAT64 dbl_tmp_dest = FLOAT64_ZERO;
	FLOAT32 sgl_tmp_dest = FLOAT32_ZERO;
	FLOAT64 dbl_last_stage_contents = FLOAT64_ZERO;
	FLOAT32 sgl_last_stage_contents = FLOAT32_ZERO;
    
#if TRACE_UNDEFINED_I860
	/* Check for invalid .ds combination.  */
	if ((insn & 0x180) == 0x100)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	/* For pipelined version, retrieve the contents of the last stage
	   of the pipeline, whose precision is specified by the ARP bit
	   of the stage's result-status bits.  There are always three stages
	   for pfadd/pfsub.  */
	if (piped)
	{
		if (m_A[2].stat.arp)
			dbl_last_stage_contents = m_A[2].val.d;
		else
			sgl_last_stage_contents = m_A[2].val.s;
	}

	/* Do the operation, being careful about source and result
	   precision.  */
	if (src_prec)
	{
		FLOAT64 v1 = get_fregval_d (fsrc1);
		FLOAT64 v2 = get_fregval_d (fsrc2);

		/* For pipelined add/sub, if fsrc1 is the same as fdest, then the last
		   stage is bypassed to fsrc1 (rather than using the value in fsrc1).
		   Likewise for fsrc2.  */
		if (piped && fdest != 0 && fsrc1 == fdest)
			v1 = dbl_last_stage_contents;
		if (piped && fdest != 0 && fsrc2 == fdest)
			v2 = dbl_last_stage_contents;

		if (res_prec)
			dbl_tmp_dest = is_sub ? float64_sub (v1, v2) : float64_add (v1, v2);
		else
			sgl_tmp_dest = is_sub ? float64_to_float32 (float64_sub (v1, v2)) : float64_to_float32 (float64_add (v1, v2));
	}
	else
	{
		FLOAT32 v1 = get_fregval_s (fsrc1);
		FLOAT32 v2 = get_fregval_s (fsrc2);

		/* For pipelined add/sub, if fsrc1 is the same as fdest, then the last
		   stage is bypassed to fsrc1 (rather than using the value in fsrc1).
		   Likewise for fsrc2.  */
		if (piped && fdest != 0 && fsrc1 == fdest)
			v1 = sgl_last_stage_contents;
		if (piped && fdest != 0 && fsrc2 == fdest)
			v2 = sgl_last_stage_contents;

		if (res_prec)
			dbl_tmp_dest = is_sub ? float64_sub (float32_to_float64 (v1), float32_to_float64 (v2)) : float64_add (float32_to_float64 (v1), float32_to_float64 (v2));
		else
			sgl_tmp_dest = is_sub ? float32_sub (v1, v2) : float32_add (v1, v2);
	}

	/* FIXME: Set result-status bits besides ARP. And copy to fsr from
	          last stage.  */
	/* FIXME: Scalar version flows through all stages.  */
	if (!piped)
	{
		/* Scalar version writes the current calculation to the fdest
		   register, with precision specified by the R bit.  */
		if (res_prec)
			set_fregval_d (fdest, dbl_tmp_dest);
		else
			set_fregval_s (fdest, sgl_tmp_dest);
	}
	else
	{
		/* Pipelined version writes fdest with the result from the last
		   stage of the pipeline, with precision specified by the ARP
		   bit of the stage's result-status bits.  */
#if 1 /* FIXME: WIP on FSR update.  This may not be correct.  */
		/* Copy 3rd stage ARP to FSR.  */
		if (m_A[1 /* 2 */].stat.arp)
			m_cregs[CR_FSR] |= 0x20000000;
		else
			m_cregs[CR_FSR] &= ~0x20000000;
#endif
		if (m_A[2].stat.arp)  /* 3rd (last) stage.  */
			set_fregval_d (fdest, dbl_last_stage_contents);
		else
			set_fregval_s (fdest, sgl_last_stage_contents);

		/* Now advance pipeline and write current calculation to
		   first stage.  */
		m_A[2] = m_A[1];
		m_A[1] = m_A[0];
		if (res_prec)
		{
			m_A[0].val.d = dbl_tmp_dest;
			m_A[0].stat.arp = 1;
		}
		else
		{
			m_A[0].val.s = sgl_tmp_dest;
			m_A[0].stat.arp = 0;
		}
	}
}

/* Execute 0x32, [p]fix.{ss,sd,dd}  (SC) added and implemented this */
void i860_cpu_device::insn_fix(UINT32 insn) {
    UINT32 fsrc1 = get_fsrc1 (insn);
    UINT32 fdest = get_fdest (insn);
    int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
    int res_prec = insn & 0x080;     /* 1 = double, 0 = single.  */
    int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */
    
#if TRACE_UNDEFINED_I860
    /* Check for invalid .ds or .ss combinations.  */
    if ((insn & 0x080) == 0) {
        unrecog_opcode (m_pc, insn);
        return;
    }
#endif
    
    /* Do the operation, being careful about source and result
     precision.  Operation: fdest = integer part of fsrc1 in
     lower 32-bits.  */
    if (src_prec) {
        FLOAT64 v1 = get_fregval_d (fsrc1);
        INT32 iv = float64_to_int32 (v1);
        /* We always write a single, since the lower 32-bits of fdest
         get the result (and the even numbered reg is the lower).  */
        set_fregval_s (fdest, *(FLOAT32 *)&iv);
    }
    else
    {
        FLOAT32 v1 = get_fregval_s (fsrc1);
        INT32 iv = float32_to_int32 (v1);
        /* We always write a single, since the lower 32-bits of fdest
         get the result (and the even numbered reg is the lower).  */
        set_fregval_s (fdest, *(FLOAT32 *)&iv);
    }
    
    /* FIXME: Handle updating of pipestages for pfix.  */
    /* Includes looking at ARP (add result precision.) */
    if (piped)
    {
        Log_Printf(LOG_WARN, "[i860:%08X] insn_fix: FIXME: pipelined not functional yet", m_pc);
        if (res_prec)
            set_fregval_d (fdest, FLOAT64_ZERO);
        else
            set_fregval_s (fdest, FLOAT32_ZERO);
    }
}

/* Operand types for PFAM/PFMAM routine below.  */
enum {
	OP_SRC1     = 0,
	OP_SRC2     = 1,
	OP_KI       = 2,
	OP_KR       = 4,
	OP_T        = 8,
	OP_MPIPE    = 16,
	OP_APIPE    = 32,
	FLAGM       = 64   /* Indicates PFMAM uses M rather than A pipe result.  */
};

/* A table to map DPC value to source operands.

   The PFAM and PFMAM tables are nearly identical, and the only differences
   are that every time PFAM uses the A pipe, PFMAM uses the M pipe instead.
   So we only represent the PFAM table and use a special flag on any entry
   where the PFMAM table would use the M pipe rather than the A pipe.
   Also, entry 16 is not valid for PFMAM.  */
static const struct
{
	int M_unit_op1;
	int M_unit_op2;
	int A_unit_op1;
	int A_unit_op2;
	int T_loaded;
	int K_loaded;
} src_opers[] = {
	/* 0000 */ { OP_KR,   OP_SRC2,        OP_SRC1,        OP_MPIPE,       0, 0},
	/* 0001 */ { OP_KR,   OP_SRC2,        OP_T,           OP_MPIPE,       0, 1},
	/* 0010 */ { OP_KR,   OP_SRC2,        OP_SRC1,        OP_APIPE|FLAGM, 1, 0},
	/* 0011 */ { OP_KR,   OP_SRC2,        OP_T,           OP_APIPE|FLAGM, 1, 1},
	/* 0100 */ { OP_KI,   OP_SRC2,        OP_SRC1,        OP_MPIPE,       0, 0},
	/* 0101 */ { OP_KI,   OP_SRC2,        OP_T,           OP_MPIPE,       0, 1},
	/* 0110 */ { OP_KI,   OP_SRC2,        OP_SRC1,        OP_APIPE|FLAGM, 1, 0},
	/* 0111 */ { OP_KI,   OP_SRC2,        OP_T,           OP_APIPE|FLAGM, 1, 1},
	/* 1000 */ { OP_KR,   OP_APIPE|FLAGM, OP_SRC1,        OP_SRC2,        1, 0},
	/* 1001 */ { OP_SRC1, OP_SRC2,        OP_APIPE|FLAGM, OP_MPIPE,       0, 0},
	/* 1010 */ { OP_KR,   OP_APIPE|FLAGM, OP_SRC1,        OP_SRC2,        0, 0},
	/* 1011 */ { OP_SRC1, OP_SRC2,        OP_T,           OP_APIPE|FLAGM, 1, 0},
	/* 1100 */ { OP_KI,   OP_APIPE|FLAGM, OP_SRC1,        OP_SRC2,        1, 0},
	/* 1101 */ { OP_SRC1, OP_SRC2,        OP_T,           OP_MPIPE,       0, 0},
	/* 1110 */ { OP_KI,   OP_APIPE|FLAGM, OP_SRC1,        OP_SRC2,        0, 0},
	/* 1111 */ { OP_SRC1, OP_SRC2,        OP_T,           OP_APIPE|FLAGM, 0, 0}
};

FLOAT32 i860_cpu_device::get_fval_from_optype_s (UINT32 insn, int optype)
{
	FLOAT32 retval = FLOAT32_ZERO;
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);

	optype &= ~FLAGM;
	switch (optype)
	{
	case OP_SRC1:
		retval = get_fregval_s (fsrc1);
		break;
	case OP_SRC2:
		retval = get_fregval_s (fsrc2);
		break;
	case OP_KI:
		retval = m_KI.s;
		break;
	case OP_KR:
		retval = m_KR.s;
		break;
	case OP_T:
		retval = m_T.s;
		break;
	case OP_MPIPE:
		/* Last stage is 3rd stage for single precision input.  */
		retval = m_M[2].val.s;
		break;
	case OP_APIPE:
		retval = m_A[2].val.s;
		break;
	default:
		assert (0);
	}

	return retval;
}


FLOAT64 i860_cpu_device::get_fval_from_optype_d (UINT32 insn, int optype)
{
	FLOAT64 retval = FLOAT64_ZERO;
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);

	optype &= ~FLAGM;
	switch (optype)
	{
	case OP_SRC1:
		retval = get_fregval_d (fsrc1);
		break;
	case OP_SRC2:
		retval = get_fregval_d (fsrc2);
		break;
	case OP_KI:
		retval = m_KI.d;
		break;
	case OP_KR:
		retval = m_KR.d;
		break;
	case OP_T:
		retval = m_T.d;
		break;
	case OP_MPIPE:
		/* Last stage is 2nd stage for double precision input.  */
		retval = m_M[1].val.d;
		break;
	case OP_APIPE:
		retval = m_A[2].val.d;
		break;
	default:
		assert (0);
	}

	return retval;
}


/* Execute pf[m]{a,s}m.{ss,sd,dd} fsrc1,fsrc2,fdest (FP dual ops).

   Since these are always pipelined, the P bit is used to distinguish
   family pfam (P=1) from family pfmam (P=0), and the lower 4 bits
   of the extended opcode is the DPC.

   Note also that the S and R bits are slightly different than normal
   floating point operations.  The S bit denotes the precision of the
   multiplication source, while the R bit denotes the precision of
   the addition source as well as precision of all results.  */
void i860_cpu_device::insn_dualop (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
	int res_prec = insn & 0x080;     /* 1 = double, 0 = single.  */
	int is_pfam = insn & 0x400;      /* 1 = pfam, 0 = pfmam.  */
	int is_sub = insn & 0x10;        /* 1 = pf[m]sm, 0 = pf[m]am.  */
	FLOAT64 dbl_tmp_dest_mul = FLOAT64_ZERO;
	FLOAT32 sgl_tmp_dest_mul = FLOAT32_ZERO;
	FLOAT64 dbl_tmp_dest_add = FLOAT64_ZERO;
	FLOAT32 sgl_tmp_dest_add = FLOAT32_ZERO;
	FLOAT64 dbl_last_Mstage_contents = FLOAT64_ZERO;
	FLOAT32 sgl_last_Mstage_contents = FLOAT32_ZERO;
	FLOAT64 dbl_last_Astage_contents = FLOAT64_ZERO;
	FLOAT32 sgl_last_Astage_contents = FLOAT32_ZERO;
	int num_mul_stages = src_prec ? 2 : 3;

	int dpc = insn & 0xf;
	int M_unit_op1 = src_opers[dpc].M_unit_op1;
	int M_unit_op2 = src_opers[dpc].M_unit_op2;
	int A_unit_op1 = src_opers[dpc].A_unit_op1;
	int A_unit_op2 = src_opers[dpc].A_unit_op2;
	int T_loaded = src_opers[dpc].T_loaded;
	int K_loaded = src_opers[dpc].K_loaded;

#if TRACE_UNDEFINED_I860
	/* Check for invalid .ds combination.  */
	if ((insn & 0x180) == 0x100)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	if (is_pfam == 0)
	{
#if TRACE_UNDEFINED_I860
		/* Check for invalid DPC combination 16 for PFMAM.  */
		if (dpc == 16)
		{
			unrecog_opcode (m_pc, insn);
			return;
		}
#endif
        
		/* PFMAM table adjustments (M_unit_op1 is never a pipe stage,
		   so no adjustment made for it).   */
		M_unit_op2 = (M_unit_op2 & FLAGM) ? OP_MPIPE : M_unit_op2;
		A_unit_op1 = (A_unit_op1 & FLAGM) ? OP_MPIPE : A_unit_op1;
		A_unit_op2 = (A_unit_op2 & FLAGM) ? OP_MPIPE : A_unit_op2;
	}

	/* FIXME: Check for fsrc1/fdest overlap for some mul DPC combinations.  */

	/* Retrieve the contents of the last stage of the multiplier pipeline,
	   whose precision is specified by the MRP bit of the stage's result-
	   status bits.  Note for multiply, the number of stages is determined
	   by the source precision of the current operation.  */
	if (m_M[num_mul_stages - 1].stat.mrp)
		dbl_last_Mstage_contents = m_M[num_mul_stages - 1].val.d;
	else
		sgl_last_Mstage_contents = m_M[num_mul_stages - 1].val.s;

	/* Similarly, retrieve the last stage of the adder pipe.  */
	if (m_A[2].stat.arp)
		dbl_last_Astage_contents = m_A[2].val.d;
	else
		sgl_last_Astage_contents = m_A[2].val.s;

	/* Do the mul operation, being careful about source and result
	   precision.  */
	if (src_prec)
	{
		FLOAT64 v1 = get_fval_from_optype_d (insn, M_unit_op1);
		FLOAT64 v2 = get_fval_from_optype_d (insn, M_unit_op2);

		/* For mul, if fsrc2 is the same as fdest, then the last stage
		   is bypassed to fsrc2 (rather than using the value in fsrc2).
		   This bypass is not available for fsrc1, and is undefined behavior.  */
		if (0 && M_unit_op1 == OP_SRC1 && fdest != 0 && fsrc1 == fdest)
			v1 = is_pfam ? dbl_last_Astage_contents : dbl_last_Mstage_contents;
		if (M_unit_op2 == OP_SRC2 && fdest != 0 && fsrc2 == fdest)
			v2 = is_pfam ? dbl_last_Astage_contents : dbl_last_Mstage_contents;

		if (res_prec)
			dbl_tmp_dest_mul = float64_mul (v1, v2);
		else
			sgl_tmp_dest_mul = float64_to_float32 (float64_mul (v1, v2));
	}
	else
	{
		FLOAT32 v1 = get_fval_from_optype_s (insn, M_unit_op1);
		FLOAT32 v2 = get_fval_from_optype_s (insn, M_unit_op2);

		/* For mul, if fsrc2 is the same as fdest, then the last stage
		   is bypassed to fsrc2 (rather than using the value in fsrc2).
		   This bypass is not available for fsrc1, and is undefined behavior.  */
		if (0 && M_unit_op1 == OP_SRC1 && fdest != 0 && fsrc1 == fdest)
			v1 = is_pfam ? sgl_last_Astage_contents : sgl_last_Mstage_contents;
		if (M_unit_op2 == OP_SRC2 && fdest != 0 && fsrc2 == fdest)
			v2 = is_pfam ? sgl_last_Astage_contents : sgl_last_Mstage_contents;

		if (res_prec)
			dbl_tmp_dest_mul = float64_mul (float32_to_float64 (v1), float32_to_float64 (v2));
		else
			sgl_tmp_dest_mul = float32_mul (v1, v2);
	}

	/* Do the add operation, being careful about source and result
	   precision.  Remember, the R bit indicates source and result precision
	   here.  */
	if (res_prec)
	{
		FLOAT64 v1 = get_fval_from_optype_d (insn, A_unit_op1);
		FLOAT64 v2 = get_fval_from_optype_d (insn, A_unit_op2);

		/* For add/sub, if fsrc1 is the same as fdest, then the last stage
		   is bypassed to fsrc1 (rather than using the value in fsrc1).
		   Likewise for fsrc2.  */
		if (A_unit_op1 == OP_SRC1 && fdest != 0 && fsrc1 == fdest)
			v1 = is_pfam ? dbl_last_Astage_contents : dbl_last_Mstage_contents;
		if (A_unit_op2 == OP_SRC2 && fdest != 0 && fsrc2 == fdest)
			v2 = is_pfam ? dbl_last_Astage_contents : dbl_last_Mstage_contents;

		if (res_prec)
			dbl_tmp_dest_add = is_sub ? float64_sub (v1, v2) : float64_add (v1, v2);
		else
			sgl_tmp_dest_add = is_sub ? float64_to_float32 (float64_sub (v1, v2)) : float64_to_float32 (float64_add (v1, v2));
	}
	else
	{
		FLOAT32 v1 = get_fval_from_optype_s (insn, A_unit_op1);
		FLOAT32 v2 = get_fval_from_optype_s (insn, A_unit_op2);

		/* For add/sub, if fsrc1 is the same as fdest, then the last stage
		   is bypassed to fsrc1 (rather than using the value in fsrc1).
		   Likewise for fsrc2.  */
		if (A_unit_op1 == OP_SRC1 && fdest != 0 && fsrc1 == fdest)
			v1 = is_pfam ? sgl_last_Astage_contents : sgl_last_Mstage_contents;
		if (A_unit_op2 == OP_SRC2 && fdest != 0 && fsrc2 == fdest)
			v2 = is_pfam ? sgl_last_Astage_contents : sgl_last_Mstage_contents;

		if (res_prec)
			dbl_tmp_dest_add = is_sub ? float64_sub (float32_to_float64 (v1), float32_to_float64 (v2)) : float64_add (float32_to_float64 (v1), float32_to_float64 (v2));
		else
			sgl_tmp_dest_add = is_sub ? float32_sub (v1, v2) : float32_add (v1, v2);
	}

	/* If necessary, load T.  */
	if (T_loaded)
	{
		/* T is loaded from the result of the last stage of the multiplier.  */
		if (m_M[num_mul_stages - 1].stat.mrp)
			m_T.d = dbl_last_Mstage_contents;
		else
			m_T.s = sgl_last_Mstage_contents;
	}

	/* If necessary, load KR or KI.  */
	if (K_loaded)
	{
		/* KI or KR is loaded from the first register input.  */
		if (M_unit_op1 == OP_KI)
		{
			if (src_prec)
				m_KI.d = get_fregval_d (fsrc1);
			else
				m_KI.s  = get_fregval_s (fsrc1);
		}
		else if (M_unit_op1 == OP_KR)
		{
			if (src_prec)
				m_KR.d = get_fregval_d (fsrc1);
			else
				m_KR.s  = get_fregval_s (fsrc1);
		}
		else
			assert (0);
	}

	/* Now update fdest (either from adder pipe or multiplier pipe,
	   depending on whether the instruction is pfam or pfmam).  */
	if (is_pfam)
	{
		/* Update fdest with the result from the last stage of the
		   adder pipeline, with precision specified by the ARP
		   bit of the stage's result-status bits.  */
		if (m_A[2].stat.arp)
			set_fregval_d (fdest, dbl_last_Astage_contents);
		else
			set_fregval_s (fdest, sgl_last_Astage_contents);
	}
	else
	{
		/* Update fdest with the result from the last stage of the
		   multiplier pipeline, with precision specified by the MRP
		   bit of the stage's result-status bits.  */
		if (m_M[num_mul_stages - 1].stat.mrp)
			set_fregval_d (fdest, dbl_last_Mstage_contents);
		else
			set_fregval_s (fdest, sgl_last_Mstage_contents);
	}

	/* FIXME: Set result-status bits besides MRP. And copy to fsr from
	          last stage.  */
	/* FIXME: Mixed precision (only weird for pfmul).  */
#if 1 /* FIXME: WIP on FSR update.  This may not be correct.  */
	/* Copy 3rd stage MRP to FSR.  */
	if (m_M[num_mul_stages - 2  /* 1 */].stat.mrp)
		m_cregs[CR_FSR] |= 0x10000000;
	else
		m_cregs[CR_FSR] &= ~0x10000000;
#endif

	/* Now advance multiplier pipeline and write current calculation to
	   first stage.  */
	if (num_mul_stages == 3)
	{
		m_M[2] = m_M[1];
		m_M[1] = m_M[0];
	}
	else
		m_M[1]  = m_M[0];

	if (res_prec)
	{
		m_M[0].val.d = dbl_tmp_dest_mul;
		m_M[0].stat.mrp = 1;
	}
	else
	{
		m_M[0].val.s = sgl_tmp_dest_mul;
		m_M[0].stat.mrp = 0;
	}

	/* FIXME: Set result-status bits besides ARP. And copy to fsr from
	          last stage.  */
#if 1 /* FIXME: WIP on FSR update.  This may not be correct.  */
	/* Copy 3rd stage ARP to FSR.  */
	if (m_A[1 /* 2 */].stat.arp)
		m_cregs[CR_FSR] |= 0x20000000;
	else
		m_cregs[CR_FSR] &= ~0x20000000;
#endif

	/* Now advance adder pipeline and write current calculation to
	   first stage.  */
	m_A[2] = m_A[1];
	m_A[1] = m_A[0];
	if (res_prec)
	{
		m_A[0].val.d = dbl_tmp_dest_add;
		m_A[0].stat.arp = 1;
	}
	else
	{
		m_A[0].val.s = sgl_tmp_dest_add;
		m_A[0].stat.arp = 0;
	}
}


/* Execute frcp.{ss,sd,dd} fsrc2,fdest (.ds disallowed above).  */
void i860_cpu_device::insn_frcp (UINT32 insn)
{
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
	int res_prec = insn & 0x080;     /* 1 = double, 0 = single.  */

	/* Do the operation, being careful about source and result
	   precision.  */
	if (src_prec)
	{
		FLOAT64 v = get_fregval_d (fsrc2);
		FLOAT64 res;
        if (FLOAT64_IS_ZERO(v))
		{
			/* Generate source-exception trap if fsrc2 is 0.  */
			if (0 /* && GET_FSR_FTE () */)
			{
				SET_PSR_FT (1);
				SET_FSR_SE (1);
				m_flow |= GET_FSR_FTE ();
			}
			/* Set fdest to INF or some other exceptional value here?  */
		}
		else
		{
			/* Real i860 isn't a precise as a real divide, but this should
			   be okay.  */
			SET_FSR_SE (0);
			*((UINT64 *)&v) &= 0xfffff00000000000ULL;
			res = float64_div (FLOAT64_ONE, v);
			*((UINT64 *)&res) &= 0xfffff00000000000ULL;
			if (res_prec)
				set_fregval_d (fdest, res);
			else
				set_fregval_s (fdest, float64_to_float32 (res));
		}
	}
	else
	{
		FLOAT32 v = get_fregval_s (fsrc2);
		FLOAT32 res;
		if (FLOAT32_IS_ZERO(v))
		{
			/* Generate source-exception trap if fsrc2 is 0.  */
			if (0 /* GET_FSR_FTE () */)
			{
				SET_PSR_FT (1);
				SET_FSR_SE (1);
				m_flow |= GET_FSR_FTE ();
			}
			/* Set fdest to INF or some other exceptional value here?  */
		}
		else
		{
			/* Real i860 isn't a precise as a real divide, but this should
			   be okay.  */
			SET_FSR_SE (0);
			*((UINT32 *)&v) &= 0xffff8000;
			res = float32_div (FLOAT32_ONE, v);
			*((UINT32 *)&res) &= 0xffff8000;
			if (res_prec)
				set_fregval_d (fdest, float32_to_float64 (res));
			else
				set_fregval_s (fdest, res);
		}
	}
}


/* Execute frsqr.{ss,sd,dd} fsrc2,fdest (.ds disallowed above).  */
void i860_cpu_device::insn_frsqr (UINT32 insn)
{
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
	int res_prec = insn & 0x080;     /* 1 = double, 0 = single.  */

#if TRACE_UNDEFINED_I860
	/* Check for invalid .ds combination.  */
	if ((insn & 0x180) == 0x100)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}

	/* Check for invalid .ds combination.  */
	if ((insn & 0x180) == 0x100)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	/* Do the operation, being careful about source and result
	   precision.  */
	if (src_prec)
	{
		FLOAT64 v = get_fregval_d (fsrc2);
		FLOAT64 res;
		if (FLOAT64_IS_ZERO(v) || FLOAT64_IS_NEG(v))
		{
			/* Generate source-exception trap if fsrc2 is 0 or negative.  */
			if (0 /* GET_FSR_FTE () */)
			{
				SET_PSR_FT (1);
				SET_FSR_SE (1);
				m_flow |= GET_FSR_FTE ();
			}
			/* Set fdest to INF or some other exceptional value here?  */
		}
		else
		{
			SET_FSR_SE (0);
			*((UINT64 *)&v) &= 0xfffff00000000000ULL;
			res = float64_div (FLOAT64_ONE, float64_sqrt (v));
			*((UINT64 *)&res) &= 0xfffff00000000000ULL;
			if (res_prec)
				set_fregval_d (fdest, res);
			else
				set_fregval_s (fdest, float64_to_float32 (res));
		}
	}
	else
	{
		FLOAT32 v = get_fregval_s (fsrc2);
		FLOAT32 res;
		if (FLOAT32_IS_ZERO(v) || FLOAT32_IS_NEG(v))
		{
			/* Generate source-exception trap if fsrc2 is 0 or negative.  */
			if (0 /* GET_FSR_FTE () */)
			{
				SET_PSR_FT (1);
				SET_FSR_SE (1);
				m_flow |= GET_FSR_FTE ();
			}
			/* Set fdest to INF or some other exceptional value here?  */
		}
		else
		{
			SET_FSR_SE (0);
			*((UINT32 *)&v) &= 0xffff8000;
			res = float32_div (FLOAT32_ONE, float32_sqrt (v));
			*((UINT32 *)&res) &= 0xffff8000;
			if (res_prec)
				set_fregval_d (fdest, float32_to_float64 (res));
			else
				set_fregval_s (fdest, res);
		}
	}
}


/* Execute fxfr fsrc1,idest.  */
void i860_cpu_device::insn_fxfr (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 idest = get_idest (insn);
	FLOAT32 fv = FLOAT32_ZERO;

	/* This is a bit-pattern transfer, not a conversion.  */
	fv = get_fregval_s (fsrc1);
	set_iregval (idest, *(UINT32 *)&fv);
}


/* Execute [p]ftrunc.{ss,sd,dd} fsrc1,idest.  */
/* FIXME: Is .ss really a valid combination?  On the one hand,
   the programmer's reference (1990) lists ftrunc.p where .p
   is any of {ss,sd,dd}.  On the other hand, a paragraph on the
   same page states that [p]ftrunc must specify double-precision
   results.  Inconsistent.
   Update: The vendor SVR4 assembler does not accept .ss combination,
   so the latter sentence above appears to be the correct way.  */
void i860_cpu_device::insn_ftrunc (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fdest = get_fdest (insn);
	int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
	int res_prec = insn & 0x080;     /* 1 = double, 0 = single.  */
	int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */

#if TRACE_UNDEFINED_I860
	/* Check for invalid .ds or .ss combinations.  */
	if ((insn & 0x080) == 0)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	/* Do the operation, being careful about source and result
	   precision.  Operation: fdest = integer part of fsrc1 in
	   lower 32-bits.  */
	if (src_prec)
	{
		FLOAT64 v1 = get_fregval_d (fsrc1);
		INT32 iv = float64_to_int32_round_to_zero (v1);
		/* We always write a single, since the lower 32-bits of fdest
		   get the result (and the even numbered reg is the lower).  */
		set_fregval_s (fdest, *(FLOAT32 *)&iv);
	}
	else
	{
		FLOAT32 v1 = get_fregval_s (fsrc1);
		INT32 iv = float32_to_int32_round_to_zero (v1);
		/* We always write a single, since the lower 32-bits of fdest
		   get the result (and the even numbered reg is the lower).  */
		set_fregval_s (fdest, *(FLOAT32 *)&iv);
	}

	/* FIXME: Handle updating of pipestages for pftrunc.  */
	/* Includes looking at ARP (add result precision.) */
	if (piped)
	{
		Log_Printf(LOG_WARN, "[i860:%08X] insn_ftrunc: FIXME: pipelined not functional yet", m_pc);
		if (res_prec)
			set_fregval_d (fdest, FLOAT64_ZERO);
		else
			set_fregval_s (fdest, FLOAT32_ZERO);
	}
}


/* Execute [p]famov.{ss,sd,ds,dd} fsrc1,fdest.  */
void i860_cpu_device::insn_famov (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fdest = get_fdest (insn);
	int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
	int res_prec = insn & 0x080;     /* 1 = double, 0 = single.  */
	int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */
	FLOAT64 dbl_tmp_dest = FLOAT64_ZERO;
	FLOAT32 sgl_tmp_dest = FLOAT32_ZERO;

	/* Do the operation, being careful about source and result
	   precision.  */
	if (src_prec)
	{
		FLOAT64 v1 = get_fregval_d (fsrc1);
		if (res_prec)
			dbl_tmp_dest = v1;
		else
			sgl_tmp_dest = float64_to_float32 (v1);
	}
	else
	{
		FLOAT32 v1 = get_fregval_s (fsrc1);
		if (res_prec)
			dbl_tmp_dest = float32_to_float64 (v1);
		else
			sgl_tmp_dest = v1;
	}

	/* FIXME: Set result-status bits besides ARP. And copy to fsr from
	          last stage.  */
	/* FIXME: Scalar version flows through all stages.  */
	if (!piped)
	{
		/* Scalar version writes the current calculation to the fdest
		   register, with precision specified by the R bit.  */
		if (res_prec)
			set_fregval_d (fdest, dbl_tmp_dest);
		else
			set_fregval_s (fdest, sgl_tmp_dest);
	}
	else
	{
		/* Pipelined version writes fdest with the result from the last
		   stage of the pipeline, with precision specified by the ARP
		   bit of the stage's result-status bits.  */
#if 1 /* FIXME: WIP on FSR update.  This may not be correct.  */
		/* Copy 3rd stage ARP to FSR.  */
		if (m_A[1 /* 2 */].stat.arp)
			m_cregs[CR_FSR] |= 0x20000000;
		else
			m_cregs[CR_FSR] &= ~0x20000000;
#endif
		if (m_A[2].stat.arp)  /* 3rd (last) stage.  */
			set_fregval_d (fdest, m_A[2].val.d);
		else
			set_fregval_s (fdest, m_A[2].val.s);

		/* Now advance pipeline and write current calculation to
		   first stage.  */
		m_A[2] = m_A[1];
		m_A[1] = m_A[0];
		if (res_prec)
		{
			m_A[0].val.d = dbl_tmp_dest;
			m_A[0].stat.arp = 1;
		}
		else
		{
			m_A[0].val.s = sgl_tmp_dest;
			m_A[0].stat.arp = 0;
		}
	}
}


/* Execute [p]fiadd/sub.{ss,dd} fsrc1,fsrc2,fdest.  */
void i860_cpu_device::insn_fiadd_sub (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
	int res_prec = insn & 0x080;     /* 1 = double, 0 = single.  */
	int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */
	int is_sub = insn & 0x4;         /* 1 = sub, 0 = add.  */
	FLOAT64 dbl_tmp_dest = FLOAT64_ZERO;
	FLOAT32 sgl_tmp_dest = FLOAT32_ZERO;

#if TRACE_UNDEFINED_I860
	/* Check for invalid .ds and .sd combinations.  */
	if ((insn & 0x180) == 0x100 || (insn & 0x180) == 0x080)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	/* Do the operation, being careful about source and result
	   precision.  */
	if (src_prec)
	{
		FLOAT64 v1 = get_fregval_d (fsrc1);
		FLOAT64 v2 = get_fregval_d (fsrc2);
		UINT64 iv1 = *(UINT64 *)&v1;
		UINT64 iv2 = *(UINT64 *)&v2;
		UINT64 r;
		if (is_sub)
			r = iv1 - iv2;
		else
			r = iv1 + iv2;
		if (res_prec)
			dbl_tmp_dest = *(FLOAT64 *)&r;
		else
			assert (0);    /* .ds not allowed.  */
	}
	else
	{
		FLOAT32 v1 = get_fregval_s (fsrc1);
		FLOAT32 v2 = get_fregval_s (fsrc2);
		UINT64 iv1 = (UINT64)(*(UINT32 *)&v1);
		UINT64 iv2 = (UINT64)(*(UINT32 *)&v2);
		UINT32 r;
		if (is_sub)
			r = (UINT32)(iv1 - iv2);
		else
			r = (UINT32)(iv1 + iv2);
		if (res_prec)
			assert (0);    /* .sd not allowed.  */
		else
			sgl_tmp_dest = *(FLOAT32 *)&r;
	}

	/* FIXME: Copy result-status bit IRP to fsr from last stage.  */
	/* FIXME: Scalar version flows through all stages.  */
	if (!piped)
	{
		/* Scalar version writes the current calculation to the fdest
		   register, with precision specified by the R bit.  */
		if (res_prec)
			set_fregval_d (fdest, dbl_tmp_dest);
		else
			set_fregval_s (fdest, sgl_tmp_dest);
	}
	else
	{
		/* Pipelined version writes fdest with the result from the last
		   stage of the pipeline, with precision specified by the IRP
		   bit of the stage's result-status bits.  */
#if 1 /* FIXME: WIP on FSR update.  This may not be correct.  */
		/* Copy stage IRP to FSR.  */
		if (res_prec)
			m_cregs[CR_FSR] |= 0x08000000;
		else
			m_cregs[CR_FSR] &= ~0x08000000;
#endif
		if (m_G.stat.irp)   /* 1st (and last) stage.  */
			set_fregval_d (fdest, m_G.val.d);
		else
			set_fregval_s (fdest, m_G.val.s);

		/* Now write current calculation to first and only stage.  */
		if (res_prec)
		{
			m_G.val.d = dbl_tmp_dest;
			m_G.stat.irp = 1;
		}
		else
		{
			m_G.val.s = sgl_tmp_dest;
			m_G.stat.irp = 0;
		}
	}
}


/* Execute pf{gt,le,eq}.{ss,dd} fsrc1,fsrc2,fdest.
   Opcode pfgt has R bit cleared; pfle has R bit set.  */
void i860_cpu_device::insn_fcmp (UINT32 insn) {
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int src_prec = insn & 0x100;     /* 1 = double, 0 = single.  */
	FLOAT64 dbl_tmp_dest = FLOAT64_ZERO;
	FLOAT32 sgl_tmp_dest = FLOAT32_ZERO;
	/* int is_eq = insn & 1; */
	int is_gt = ((insn & 0x81) == 0x00);
	int is_le = ((insn & 0x81) == 0x80);

    /* Save the CC for DIM bc/bnc */
    m_dim_cc       = GET_PSR_CC();
    m_dim_cc_valid = m_dim != DIM_NONE;
    
	/* Do the operation.  Source and result precision must be the same.
	     pfgt: CC set     if fsrc1 > fsrc2, else cleared.
	     pfle: CC cleared if fsrc1 <= fsrc2, else set.
	     pfeq: CC set     if fsrc1 = fsrc2, else cleared.

	   Note that the compares write an undefined (but non-exceptional)
	   result into the first stage of the adder pipeline.  We'll model
	   this by just pushing in dbl_ or sgl_tmp_dest which equal 0.0.  */
	if (src_prec) {
		FLOAT64 v1 = get_fregval_d (fsrc1);
		FLOAT64 v2 = get_fregval_d (fsrc2);
		if (is_gt)                /* gt.  */
			SET_PSR_CC_F (float64_gt (v1, v2) ? 1 : 0); // v1 > v2
		else if (is_le)           /* le.  */
			SET_PSR_CC_F (float64_le (v1, v2) ? 0 : 1); // v1 <= v2
		else                      /* eq.  */
			SET_PSR_CC_F (float64_eq (v1, v2) ? 1 : 0); // v1 == v2
	} else {
		FLOAT32 v1 = get_fregval_s (fsrc1);
		FLOAT32 v2 = get_fregval_s (fsrc2);
		if (is_gt)                /* gt.  */
			SET_PSR_CC_F (float32_gt (v1, v2) ? 1 : 0); // v1 > v2
		else if (is_le)           /* le.  */
			SET_PSR_CC_F (float32_le (v1, v2) ? 0 : 1); // v1 <= v2
		else                      /* eq.  */
			SET_PSR_CC_F (float32_eq (v1, v2) ? 1 : 0); // v1 == v2
	}

	/* FIXME: Set result-status bits besides ARP. And copy to fsr from
	          last stage.  */
	/* These write fdest with the result from the last
	   stage of the pipeline, with precision specified by the ARP
	   bit of the stage's result-status bits.  */
#if 1 /* FIXME: WIP on FSR update.  This may not be correct.  */
	/* Copy 3rd stage ARP to FSR.  */
	if (m_A[1 /* 2 */].stat.arp)
		m_cregs[CR_FSR] |= 0x20000000;
	else
		m_cregs[CR_FSR] &= ~0x20000000;
#endif
	if (m_A[2].stat.arp)  /* 3rd (last) stage.  */
		set_fregval_d (fdest, m_A[2].val.d);
	else
		set_fregval_s (fdest, m_A[2].val.s);

	/* Now advance pipeline and write current calculation to
	   first stage.  */
	m_A[2] = m_A[1];
	m_A[1] = m_A[0];
	if (src_prec) {
		m_A[0].val.d = dbl_tmp_dest;
		m_A[0].stat.arp = 1;
	} else {
		m_A[0].val.s = sgl_tmp_dest;
		m_A[0].stat.arp = 0;
	}
}


/* Execute [p]fzchk{l,s} fsrc1,fsrc2,fdest.
   The fzchk instructions have S and R bits set.  */
void i860_cpu_device::insn_fzchk (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */
	int is_fzchks = insn & 8;        /* 1 = fzchks, 0 = fzchkl.  */
	FLOAT64 dbl_tmp_dest = FLOAT64_ZERO;
	int i;
	FLOAT64 v1 = get_fregval_d (fsrc1);
	FLOAT64 v2 = get_fregval_d (fsrc2);
	UINT64 iv1 = *(UINT64 *)&v1;
	UINT64 iv2 = *(UINT64 *)&v2;
	UINT64 r = 0;
	char pm = GET_PSR_PM ();

#if TRACE_UNDEFINED_I860
	/* Check for S and R bits set.  */
	if ((insn & 0x180) != 0x180)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	/* Do the operation.  The fzchks version operates in parallel on
	   four 16-bit pixels, while the fzchkl operates on two 32-bit
	   pixels (pixels are unsigned ordinals in this context).  */
	if (is_fzchks)
	{
		pm = (pm >> 4) & 0x0f;
		for (i = 3; i >= 0; i--)
		{
			UINT16 ps1 = (iv1 >> (i * 16)) & 0xffff;
			UINT16 ps2 = (iv2 >> (i * 16)) & 0xffff;
			if (ps2 <= ps1)
			{
				r |= ((UINT64)ps2 << (i * 16));
				pm |= (1 << (7 - (3 - i)));
			}
			else
			{
				r |= ((UINT64)ps1 << (i * 16));
				pm &= ~(1 << (7 - (3 - i)));
			}
		}
	}
	else
	{
		pm = (pm >> 2) & 0x3f;
		for (i = 1; i >= 0; i--)
		{
			UINT32 ps1 = (iv1 >> (i * 32)) & 0xffffffff;
			UINT32 ps2 = (iv2 >> (i * 32)) & 0xffffffff;
			if (ps2 <= ps1)
			{
				r |= ((UINT64)ps2 << (i * 32));
				pm |= (1 << (7 - (1 - i)));
			}
			else
			{
				r |= ((UINT64)ps1 << (i * 32));
				pm &= ~(1 << (7 - (1 - i)));
			}
		}
	}

	dbl_tmp_dest = *(FLOAT64 *)&r;
	SET_PSR_PM (pm);
    m_merge = 0;

	/* FIXME: Copy result-status bit IRP to fsr from last stage.  */
	/* FIXME: Scalar version flows through all stages.  */
	if (!piped)
	{
		/* Scalar version writes the current calculation to the fdest
		   register, always with double precision.  */
		set_fregval_d (fdest, dbl_tmp_dest);
	}
	else
	{
		/* Pipelined version writes fdest with the result from the last
		   stage of the pipeline, with precision specified by the IRP
		   bit of the stage's result-status bits.  */
		if (m_G.stat.irp)   /* 1st (and last) stage.  */
			set_fregval_d (fdest, m_G.val.d);
		else
			set_fregval_s (fdest, m_G.val.s);

		/* Now write current calculation to first and only stage.  */
		m_G.val.d = dbl_tmp_dest;
		m_G.stat.irp = 1;
	}
}


/* Execute [p]form.dd fsrc1,fdest.
   The form.dd instructions have S and R bits set.  */
void i860_cpu_device::insn_form (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fdest = get_fdest (insn);
	int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */
	FLOAT64 dbl_tmp_dest = FLOAT64_ZERO;
	FLOAT64 v1 = get_fregval_d (fsrc1);
	UINT64 iv1 = *(UINT64 *)&v1;

#if TRACE_UNDEFINED_I860
	/* Check for S and R bits set.  */
	if ((insn & 0x180) != 0x180)
	{
		unrecog_opcode (m_pc, insn);
		return;
	}
#endif
    
	iv1 |= m_merge;
	dbl_tmp_dest = *(FLOAT64 *)&iv1;
	m_merge = 0;

	/* FIXME: Copy result-status bit IRP to fsr from last stage.  */
	/* FIXME: Scalar version flows through all stages.  */
	if (!piped)
	{
		/* Scalar version writes the current calculation to the fdest
		   register, always with double precision.  */
		set_fregval_d (fdest, dbl_tmp_dest);
	}
	else
	{
		/* Pipelined version writes fdest with the result from the last
		   stage of the pipeline, with precision specified by the IRP
		   bit of the stage's result-status bits.  */
		if (m_G.stat.irp)   /* 1st (and last) stage.  */
			set_fregval_d (fdest, m_G.val.d);
		else
			set_fregval_s (fdest, m_G.val.s);

		/* Now write current calculation to first and only stage.  */
		m_G.val.d = dbl_tmp_dest;
		m_G.stat.irp = 1;
	}
}


/* Execute [p]faddp fsrc1,fsrc2,fdest.  */
void i860_cpu_device::insn_faddp (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */
	FLOAT64 dbl_tmp_dest = FLOAT64_ZERO;
	FLOAT64 v1 = get_fregval_d (fsrc1);
	FLOAT64 v2 = get_fregval_d (fsrc2);
	UINT64 iv1 = *(UINT64 *)&v1;
	UINT64 iv2 = *(UINT64 *)&v2;
	UINT64 r = 0;
	int ps = GET_PSR_PS ();

	r = iv1 + iv2;
	dbl_tmp_dest = *(FLOAT64 *)&r;
    
	/* Update the merge register depending on the pixel size.
	   PS: 0 = 8 bits, 1 = 16 bits, 2 = 32-bits.  */
	if (ps == 0)
	{
		m_merge = ((m_merge >> 8) & ~0xff00ff00ff00ff00ULL);
		m_merge |= (r & 0xff00ff00ff00ff00ULL);
	}
	else if (ps == 1)
	{
		m_merge = ((m_merge >> 6) & ~0xfc00fc00fc00fc00ULL);
		m_merge |= (r & 0xfc00fc00fc00fc00ULL);
	}
	else if (ps == 2)
	{
		m_merge = ((m_merge >> 8) & ~0xff000000ff000000ULL);
		m_merge |= (r & 0xff000000ff000000ULL);
	}
#if TRACE_UNDEFINED_I860
	else
		Log_Printf(LOG_WARN, "[i860:%08X] insn_faddp: Undefined i860XR behavior, invalid value %d for pixel size", m_pc, ps);
#endif

	/* FIXME: Copy result-status bit IRP to fsr from last stage.  */
	/* FIXME: Scalar version flows through all stages.  */
	if (!piped)
	{
		/* Scalar version writes the current calculation to the fdest
		   register, always with double precision.  */
		set_fregval_d (fdest, dbl_tmp_dest);
	}
	else
	{
		/* Pipelined version writes fdest with the result from the last
		   stage of the pipeline, with precision specified by the IRP
		   bit of the stage's result-status bits.  */
		if (m_G.stat.irp)   /* 1st (and last) stage.  */
			set_fregval_d (fdest, m_G.val.d);
		else
			set_fregval_s (fdest, m_G.val.s);

		/* Now write current calculation to first and only stage.  */
		m_G.val.d = dbl_tmp_dest;
		m_G.stat.irp = 1;
	}
}


/* Execute [p]faddz fsrc1,fsrc2,fdest.  */
void i860_cpu_device::insn_faddz (UINT32 insn)
{
	UINT32 fsrc1 = get_fsrc1 (insn);
	UINT32 fsrc2 = get_fsrc2 (insn);
	UINT32 fdest = get_fdest (insn);
	int piped = insn & 0x400;        /* 1 = pipelined, 0 = scalar.  */
	FLOAT64 dbl_tmp_dest = FLOAT64_ZERO;
	FLOAT64 v1 = get_fregval_d (fsrc1);
	FLOAT64 v2 = get_fregval_d (fsrc2);
	UINT64 iv1 = *(UINT64 *)&v1;
	UINT64 iv2 = *(UINT64 *)&v2;
	UINT64 r = 0;

	r = iv1 + iv2;
	dbl_tmp_dest = *(FLOAT64 *)&r;

	/* Update the merge register.  */
	m_merge = ((m_merge >> 16) & ~0xffff0000ffff0000ULL);
	m_merge |= (r & 0xffff0000ffff0000ULL);

	/* FIXME: Copy result-status bit IRP to fsr from last stage.  */
	/* FIXME: Scalar version flows through all stages.  */
	if (!piped)
	{
		/* Scalar version writes the current calculation to the fdest
		   register, always with double precision.  */
		set_fregval_d (fdest, dbl_tmp_dest);
	}
	else
	{
		/* Pipelined version writes fdest with the result from the last
		   stage of the pipeline, with precision specified by the IRP
		   bit of the stage's result-status bits.  */
		if (m_G.stat.irp)   /* 1st (and last) stage.  */
			set_fregval_d (fdest, m_G.val.d);
		else
			set_fregval_s (fdest, m_G.val.s);

		/* Now write current calculation to first and only stage.  */
		m_G.val.d = dbl_tmp_dest;
		m_G.stat.irp = 1;
	}
}

/* First-level decode table (i.e., for the 6 primary opcode bits).  */
const i860_cpu_device::insn_func i860_cpu_device::decode_tbl[64] = {
	/* A slight bit of decoding for loads and stores is done in the
	   execution routines (operand size and addressing mode), which
	   is why their respective entries are identical.  */
	&i860_cpu_device::insn_ldx,          /* ld.b isrc1(isrc2),idest.  */
	&i860_cpu_device::insn_ldx,          /* ld.b #const(isrc2),idest.  */
	&i860_cpu_device::insn_ixfr,         /* ixfr isrc1ni,fdest.  */
	&i860_cpu_device::insn_stx,          /* st.b isrc1ni,#const(isrc2).  */
	&i860_cpu_device::insn_ldx,          /* ld.{s,l} isrc1(isrc2),idest.  */
	&i860_cpu_device::insn_ldx,          /* ld.{s,l} #const(isrc2),idest.  */
	&i860_cpu_device::dec_unrecog,
	&i860_cpu_device::insn_stx,          /* st.{s,l} isrc1ni,#const(isrc2),idest.*/
	&i860_cpu_device::insn_fldy,         /* fld.{l,d,q} isrc1(isrc2)[++],fdest. */
	&i860_cpu_device::insn_fldy,         /* fld.{l,d,q} #const(isrc2)[++],fdest. */
	&i860_cpu_device::insn_fsty,         /* fst.{l,d,q} fdest,isrc1(isrc2)[++] */
	&i860_cpu_device::insn_fsty,         /* fst.{l,d,q} fdest,#const(isrc2)[++] */
	&i860_cpu_device::insn_ld_ctrl,      /* ld.c csrc2,idest.  */
	&i860_cpu_device::insn_flush,        /* flush #const(isrc2) (or autoinc).  */
	&i860_cpu_device::insn_st_ctrl,      /* st.c isrc1,csrc2.  */
	&i860_cpu_device::insn_pstd,         /* pst.d fdest,#const(isrc2)[++].  */
	&i860_cpu_device::insn_bri,          /* bri isrc1ni.  */
	&i860_cpu_device::insn_trap,         /* trap isrc1ni,isrc2,idest.   */
	&i860_cpu_device::dec_unrecog,       /* FP ESCAPE FORMAT, more decode.  */
	&i860_cpu_device::dec_unrecog,       /* CORE ESCAPE FORMAT, more decode.  */
	&i860_cpu_device::insn_btne,         /* btne isrc1,isrc2,sbroff.  */
	&i860_cpu_device::insn_btne_imm,     /* btne #const,isrc2,sbroff.  */
	&i860_cpu_device::insn_bte,          /* bte isrc1,isrc2,sbroff.  */
	&i860_cpu_device::insn_bte_imm,      /* bte #const5,isrc2,idest.  */
	&i860_cpu_device::insn_fldy,         /* pfld.{l,d,q} isrc1(isrc2)[++],fdest.*/
	&i860_cpu_device::insn_fldy,         /* pfld.{l,d,q} #const(isrc2)[++],fdest.*/
	&i860_cpu_device::insn_br,           /* br lbroff.  */
	&i860_cpu_device::insn_call,         /* call lbroff .  */
	&i860_cpu_device::insn_bc,           /* bc lbroff.  */
	&i860_cpu_device::insn_bct,          /* bc.t lbroff.  */
	&i860_cpu_device::insn_bnc,          /* bnc lbroff.  */
	&i860_cpu_device::insn_bnct,         /* bnc.t lbroff.  */
	&i860_cpu_device::insn_addu,         /* addu isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_addu_imm,     /* addu #const,isrc2,idest.  */
	&i860_cpu_device::insn_subu,         /* subu isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_subu_imm,     /* subu #const,isrc2,idest.  */
	&i860_cpu_device::insn_adds,         /* adds isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_adds_imm,     /* adds #const,isrc2,idest.  */
	&i860_cpu_device::insn_subs,         /* subs isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_subs_imm,     /* subs #const,isrc2,idest.  */
	&i860_cpu_device::insn_shl,          /* shl isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_shl_imm,      /* shl #const,isrc2,idest.  */
	&i860_cpu_device::insn_shr,          /* shr isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_shr_imm,      /* shr #const,isrc2,idest.  */
	&i860_cpu_device::insn_shrd,         /* shrd isrc1ni,isrc2,idest.  */
	&i860_cpu_device::insn_bla,          /* bla isrc1ni,isrc2,sbroff.  */
	&i860_cpu_device::insn_shra,         /* shra isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_shra_imm,     /* shra #const,isrc2,idest.  */
	&i860_cpu_device::insn_and,          /* and isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_and_imm,      /* and #const,isrc2,idest.  */
	&i860_cpu_device::dec_unrecog,
	&i860_cpu_device::insn_andh_imm,     /* andh #const,isrc2,idest.  */
	&i860_cpu_device::insn_andnot,       /* andnot isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_andnot_imm,   /* andnot #const,isrc2,idest.  */
	&i860_cpu_device::dec_unrecog,
	&i860_cpu_device::insn_andnoth_imm,  /* andnoth #const,isrc2,idest.  */
	&i860_cpu_device::insn_or,           /* or isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_or_imm,       /* or #const,isrc2,idest.  */
	&i860_cpu_device::dec_unrecog,
	&i860_cpu_device::insn_orh_imm,      /* orh #const,isrc2,idest.  */
	&i860_cpu_device::insn_xor,          /* xor isrc1,isrc2,idest.  */
	&i860_cpu_device::insn_xor_imm,      /* xor #const,isrc2,idest.  */
	&i860_cpu_device::dec_unrecog,
	&i860_cpu_device::insn_xorh_imm,     /* xorh #const,isrc2,idest.  */
};


/* Second-level decode table (i.e., for the 3 core escape opcode bits).  */
const i860_cpu_device::insn_func i860_cpu_device::core_esc_decode_tbl[8] = {
	&i860_cpu_device::dec_unrecog,
	&i860_cpu_device::dec_unrecog, /* lock  (FIXME: unimplemented).  */
	&i860_cpu_device::insn_calli,        /* calli isrc1ni.                 */
	&i860_cpu_device::dec_unrecog,
	&i860_cpu_device::insn_intovr,       /* intovr.                        */
	&i860_cpu_device::dec_unrecog,
	&i860_cpu_device::dec_unrecog,
	&i860_cpu_device::dec_unrecog, /* unlock (FIXME: unimplemented). */
};


/* Second-level decode table (i.e., for the 7 FP extended opcode bits).  */
const i860_cpu_device::insn_func i860_cpu_device::fp_decode_tbl[128] = {
	/* Floating point instructions.  The least significant 7 bits are
	   the (extended) opcode and bits 10:7 are P,D,S,R respectively
	   ([p]ipelined, [d]ual, [s]ource prec., [r]esult prec.).
	   For some operations, I defer decoding the P,S,R bits to the
	   emulation routine for them.  */
	&i860_cpu_device::insn_dualop,       /* 0x00 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x01 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x02 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x03 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x04 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x05 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x06 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x07 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x08 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x09 pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x0A pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x0B pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x0C pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x0D pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x0E pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x0F pf[m]am */
	&i860_cpu_device::insn_dualop,       /* 0x10 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x11 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x12 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x13 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x14 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x15 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x16 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x17 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x18 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x19 pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x1A pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x1B pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x1C pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x1D pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x1E pf[m]sm */
	&i860_cpu_device::insn_dualop,       /* 0x1F pf[m]sm */
	&i860_cpu_device::insn_fmul,         /* 0x20 [p]fmul */
	&i860_cpu_device::insn_fmlow,        /* 0x21 fmlow.dd */
	&i860_cpu_device::insn_frcp,         /* 0x22 frcp.{ss,sd,dd} */
	&i860_cpu_device::insn_frsqr,        /* 0x23 frsqr.{ss,sd,dd} */
	&i860_cpu_device::insn_fmul,         /* 0x24 pfmul3.dd */
	&i860_cpu_device::dec_unrecog, /* 0x25 */
	&i860_cpu_device::dec_unrecog, /* 0x26 */
	&i860_cpu_device::dec_unrecog, /* 0x27 */
	&i860_cpu_device::dec_unrecog, /* 0x28 */
	&i860_cpu_device::dec_unrecog, /* 0x29 */
	&i860_cpu_device::dec_unrecog, /* 0x2A */
	&i860_cpu_device::dec_unrecog, /* 0x2B */
	&i860_cpu_device::dec_unrecog, /* 0x2C */
	&i860_cpu_device::dec_unrecog, /* 0x2D */
	&i860_cpu_device::dec_unrecog, /* 0x2E */
	&i860_cpu_device::dec_unrecog, /* 0x2F */
	&i860_cpu_device::insn_fadd_sub,     /* 0x30, [p]fadd.{ss,sd,dd} */
	&i860_cpu_device::insn_fadd_sub,     /* 0x31, [p]fsub.{ss,sd,dd} */
	&i860_cpu_device::insn_fix,          /* 0x32, [p]fix.{ss,sd,dd} */
	&i860_cpu_device::insn_famov,        /* 0x33, [p]famov.{ss,sd,ds,dd} */
	&i860_cpu_device::insn_fcmp,         /* 0x34, pf{gt,le}.{ss,dd} */
	&i860_cpu_device::insn_fcmp,         /* 0x35, pfeq.{ss,dd} */
	&i860_cpu_device::dec_unrecog, /* 0x36 */
	&i860_cpu_device::dec_unrecog, /* 0x37 */
	&i860_cpu_device::dec_unrecog, /* 0x38 */
	&i860_cpu_device::dec_unrecog, /* 0x39 */
	&i860_cpu_device::insn_ftrunc,       /* 0x3A, [p]ftrunc.{ss,sd,dd} */
	&i860_cpu_device::dec_unrecog, /* 0x3B */
	&i860_cpu_device::dec_unrecog, /* 0x3C */
	&i860_cpu_device::dec_unrecog, /* 0x3D */
	&i860_cpu_device::dec_unrecog, /* 0x3E */
	&i860_cpu_device::dec_unrecog, /* 0x3F */
	&i860_cpu_device::insn_fxfr,         /* 0x40, fxfr */
	&i860_cpu_device::dec_unrecog, /* 0x41 */
	&i860_cpu_device::dec_unrecog, /* 0x42 */
	&i860_cpu_device::dec_unrecog, /* 0x43 */
	&i860_cpu_device::dec_unrecog, /* 0x44 */
	&i860_cpu_device::dec_unrecog, /* 0x45 */
	&i860_cpu_device::dec_unrecog, /* 0x46 */
	&i860_cpu_device::dec_unrecog, /* 0x47 */
	&i860_cpu_device::dec_unrecog, /* 0x48 */
	&i860_cpu_device::insn_fiadd_sub,    /* 0x49, [p]fiadd.{ss,dd} */
	&i860_cpu_device::dec_unrecog, /* 0x4A */
	&i860_cpu_device::dec_unrecog, /* 0x4B */
	&i860_cpu_device::dec_unrecog, /* 0x4C */
	&i860_cpu_device::insn_fiadd_sub,    /* 0x4D, [p]fisub.{ss,dd} */
	&i860_cpu_device::dec_unrecog, /* 0x4E */
	&i860_cpu_device::dec_unrecog, /* 0x4F */
	&i860_cpu_device::insn_faddp,        /* 0x50, [p]faddp */
	&i860_cpu_device::insn_faddz,        /* 0x51, [p]faddz */
	&i860_cpu_device::dec_unrecog, /* 0x52 */
	&i860_cpu_device::dec_unrecog, /* 0x53 */
	&i860_cpu_device::dec_unrecog, /* 0x54 */
	&i860_cpu_device::dec_unrecog, /* 0x55 */
	&i860_cpu_device::dec_unrecog, /* 0x56 */
	&i860_cpu_device::insn_fzchk,        /* 0x57, [p]fzchkl */
	&i860_cpu_device::dec_unrecog, /* 0x58 */
	&i860_cpu_device::dec_unrecog, /* 0x59 */
	&i860_cpu_device::insn_form,         /* 0x5A, [p]form.dd */
	&i860_cpu_device::dec_unrecog, /* 0x5B */
	&i860_cpu_device::dec_unrecog, /* 0x5C */
	&i860_cpu_device::dec_unrecog, /* 0x5D */
	&i860_cpu_device::dec_unrecog, /* 0x5E */
	&i860_cpu_device::insn_fzchk,        /* 0x5F, [p]fzchks */
	&i860_cpu_device::dec_unrecog, /* 0x60 */
	&i860_cpu_device::dec_unrecog, /* 0x61 */
	&i860_cpu_device::dec_unrecog, /* 0x62 */
	&i860_cpu_device::dec_unrecog, /* 0x63 */
	&i860_cpu_device::dec_unrecog, /* 0x64 */
	&i860_cpu_device::dec_unrecog, /* 0x65 */
	&i860_cpu_device::dec_unrecog, /* 0x66 */
	&i860_cpu_device::dec_unrecog, /* 0x67 */
	&i860_cpu_device::dec_unrecog, /* 0x68 */
	&i860_cpu_device::dec_unrecog, /* 0x69 */
	&i860_cpu_device::dec_unrecog, /* 0x6A */
	&i860_cpu_device::dec_unrecog, /* 0x6B */
	&i860_cpu_device::dec_unrecog, /* 0x6C */
	&i860_cpu_device::dec_unrecog, /* 0x6D */
	&i860_cpu_device::dec_unrecog, /* 0x6E */
	&i860_cpu_device::dec_unrecog, /* 0x6F */
	&i860_cpu_device::dec_unrecog, /* 0x70 */
	&i860_cpu_device::dec_unrecog, /* 0x71 */
	&i860_cpu_device::dec_unrecog, /* 0x72 */
	&i860_cpu_device::dec_unrecog, /* 0x73 */
	&i860_cpu_device::dec_unrecog, /* 0x74 */
	&i860_cpu_device::dec_unrecog, /* 0x75 */
	&i860_cpu_device::dec_unrecog, /* 0x76 */
	&i860_cpu_device::dec_unrecog, /* 0x77 */
	&i860_cpu_device::dec_unrecog, /* 0x78 */
	&i860_cpu_device::dec_unrecog, /* 0x79 */
	&i860_cpu_device::dec_unrecog, /* 0x7A */
	&i860_cpu_device::dec_unrecog, /* 0x7B */
	&i860_cpu_device::dec_unrecog, /* 0x7C */
	&i860_cpu_device::dec_unrecog, /* 0x7D */
	&i860_cpu_device::dec_unrecog, /* 0x7E */
	&i860_cpu_device::dec_unrecog, /* 0x7F */
};

i860_cpu_device::insn_func i860_cpu_device::decoder_tbl[8192];

/*
 * Main decoder driver.
 *  insn = instruction at the current PC to execute.
 *  non_shadow = This insn is not in the shadow of a delayed branch - (SC) unused, removed).
 */
void i860_cpu_device::decode_exec (UINT32 insn) {
    if(m_flow & EXITING_IFETCH) return;
    
#if ENABLE_PERF_COUNTERS
    m_insn_decoded++;
#endif
    
#if ENABLE_DEBUGGER
    m_traceback[m_traceback_idx++] = m_pc;
    if(m_traceback_idx >= (sizeof(m_traceback) / sizeof(m_traceback[0])))
        m_traceback_idx = 0;
#endif    
//    (this->*decode_tbl[(insn >> 26) & 0x3f])(insn);
    (this->*decoder_tbl[((insn >> 19) & 0x1F80) | (insn & 0x7F)])(insn);
}

void i860_cpu_device::dec_unrecog(UINT32 insn) {
    unrecog_opcode(m_pc, insn);
}

/* Set-up all the default power-on/reset values.  */
void i860_cpu_device::reset() {
    UINT32 UNDEF_VAL = 0x55aa5500;
    
	int i;
	/* On power-up/reset, i860 has values:
	     PC = 0xffffff00.
	     Integer registers: r0 = 0, others = undefined.
	     FP registers:      f0:f1 = 0, others undefined.
	     psr: U = IM = BR = BW = 0; others = undefined.
	     epsr: IL = WP = PBM = BE = 0; processor type, stepping, and
	           DCS are proper and read-only; others = undefined.
	     db: undefined.
	     dirbase: DPS, BL, ATE = 0
	     fir, fsr, KR, KI, MERGE: undefined. (what about T?)

	     I$: flushed.
	     D$: undefined (all modified bits = 0).
	     TLB: flushed.

	   Note that any undefined values are set to UNDEF_VAL patterns to
	   try to detect defective i860 software.  */

	/* PC is at trap address after reset.  */
	m_pc = 0xffffff00;

	/* Set grs and frs to undefined/nonsense values, except r0.  */
	for (i = 0; i < 32; i++){
        set_iregval (i, UNDEF_VAL | i);
		set_fregval_s (i, FLOAT32_ZERO);
	}
	set_iregval (0, 0);
	set_fregval_s (0, FLOAT32_ZERO);
	set_fregval_s (1, FLOAT32_ZERO);

	/* Set whole psr to 0.  This sets the proper bits to 0 as specified
	   above, and zeroes the undefined bits.  */
	m_cregs[CR_PSR] = 0;

	/* Set most of the epsr bits to 0 (as specified above), leaving
	   undefined as zero as well.  Then properly set processor type,
	   step, and DCS. Type = EPSR[7..0], step = EPSR[12..8],
	   DCS = EPSR[21..18] (2^[12+dcs] = cache size).
	   We'll pretend to be stepping D0, since it has the fewest bugs
	   (and I don't want to emulate the many defects in the earlier
	   steppings).
	   Proc type: 1 = XR, 2 = XP   (XR has 8KB data cache -> DCS = 1).
	   Steppings (XR): 3,4,5,6,7 = (B2, C0, B3, C1, D0 respectively).
	   Steppings (XP): 0, 2, 3, 4 = (A0, B0, B1, B2) (any others?).  */
	m_cregs[CR_EPSR] = 0x00040601;

	/* Set DPS, BL, ATE = 0 and the undefined parts also to 0. But CS8 mode to 1 */
	m_cregs[CR_DIRBASE] = 0x00000080;

	/* Set fir, fsr, KR, KI, MERGE, T to undefined.  */
	m_cregs[CR_FIR] = UNDEF_VAL;
	m_cregs[CR_FSR] = UNDEF_VAL;
	m_KR.d          = FLOAT64_ZERO;
	m_KI.d          = FLOAT64_ZERO;
	m_T.d           = FLOAT64_ZERO;
	m_merge         = 0;
	m_flow          = 0;
    
    /* dual instruction mode is off after reset */
    m_dim           = DIM_NONE;
    m_dim_cc_valid  = false;
    
    /* invalidate caches */
    invalidate_icache();
    invalidate_tlb();
    
    /* memory access is little endian */
    set_mem_access(false);
    
    halt(false);
}
