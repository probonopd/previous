/*
  Hatari - m68000.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

*/


const char M68000_fileid[] = "Hatari m68000.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "hatari-glue.h"
#include "cycInt.h"
#include "m68000.h"
#include "options.h"
#include "nextMemory.h"

#include "mmu_common.h"

Uint32 BusErrorAddress;         /* Stores the offending address for bus-/address errors */
Uint32 BusErrorPC;              /* Value of the PC when bus error occurs */
bool bBusErrorReadWrite;        /* 0 for write error, 1 for read error */
int BusMode = BUS_MODE_CPU;	/* Used to tell which part is owning the bus (cpu, blitter, ...) */

int LastOpcodeFamily = i_NOP;	/* see the enum in readcpu.h i_XXX */
int LastInstrCycles = 0;	/* number of cycles for previous instr. (not rounded to 4) */
int Pairing = 0;		/* set to 1 if the latest 2 intr paired */
char PairingArray[ MAX_OPCODE_FAMILY ][ MAX_OPCODE_FAMILY ];


/* to convert the enum from OpcodeFamily to a readable value for pairing's debug */
const char *OpcodeName[] = { "ILLG",
	"OR","AND","EOR","ORSR","ANDSR","EORSR",
	"SUB","SUBA","SUBX","SBCD",
	"ADD","ADDA","ADDX","ABCD",
	"NEG","NEGX","NBCD","CLR","NOT","TST",
	"BTST","BCHG","BCLR","BSET",
	"CMP","CMPM","CMPA",
	"MVPRM","MVPMR","MOVE","MOVEA","MVSR2","MV2SR",
	"SWAP","EXG","EXT","MVMEL","MVMLE",
	"TRAP","MVR2USP","MVUSP2R","RESET","NOP","STOP","RTE","RTD",
	"LINK","UNLK",
	"RTS","TRAPV","RTR",
	"JSR","JMP","BSR","Bcc",
	"LEA","PEA","DBcc","Scc",
	"DIVU","DIVS","MULU","MULS",
	"ASR","ASL","LSR","LSL","ROL","ROR","ROXL","ROXR",
	"ASRW","ASLW","LSRW","LSLW","ROLW","RORW","ROXLW","ROXRW",
	"CHK","CHK2",
	"MOVEC2","MOVE2C","CAS","CAS2","DIVL","MULL",
	"BFTST","BFEXTU","BFCHG","BFEXTS","BFCLR","BFFFO","BFSET","BFINS",
	"PACK","UNPK","TAS","BKPT","CALLM","RTM","TRAPcc","MOVES",
	"FPP","FDBcc","FScc","FTRAPcc","FBcc","FSAVE","FRESTORE",
	"CINVL","CINVP","CINVA","CPUSHL","CPUSHP","CPUSHA","MOVE16",
	"MMUOP"
};


/*-----------------------------------------------------------------------*/
/**
 * Add pairing between all the bit shifting instructions and a given Opcode
 */

static void M68000_InitPairing_BitShift ( int OpCode )
{
	PairingArray[  i_ASR ][ OpCode ] = 1; 
	PairingArray[  i_ASL ][ OpCode ] = 1; 
	PairingArray[  i_LSR ][ OpCode ] = 1; 
	PairingArray[  i_LSL ][ OpCode ] = 1; 
	PairingArray[  i_ROL ][ OpCode ] = 1; 
	PairingArray[  i_ROR ][ OpCode ] = 1; 
	PairingArray[ i_ROXR ][ OpCode ] = 1; 
	PairingArray[ i_ROXL ][ OpCode ] = 1; 
}


/**
 * Init the pairing matrix
 * Two instructions can pair if PairingArray[ LastOpcodeFamily ][ OpcodeFamily ] == 1
 */
static void M68000_InitPairing(void)
{
	/* First, clear the matrix (pairing is false) */
	memset(PairingArray , 0 , MAX_OPCODE_FAMILY * MAX_OPCODE_FAMILY);

	/* Set all valid pairing combinations to 1 */
	PairingArray[  i_EXG ][ i_DBcc ] = 1;
	PairingArray[  i_EXG ][ i_MOVE ] = 1;
	PairingArray[  i_EXG ][ i_MOVEA] = 1;

	PairingArray[ i_CMPA ][  i_Bcc ] = 1;
	PairingArray[  i_CMP ][  i_Bcc ] = 1;

	M68000_InitPairing_BitShift ( i_DBcc );
	M68000_InitPairing_BitShift ( i_MOVE );
	M68000_InitPairing_BitShift ( i_MOVEA );
	M68000_InitPairing_BitShift ( i_LEA );
	M68000_InitPairing_BitShift ( i_JMP );

	PairingArray[ i_MULU ][ i_MOVEA] = 1; 
	PairingArray[ i_MULS ][ i_MOVEA] = 1; 
	PairingArray[ i_MULU ][ i_MOVE ] = 1; 
	PairingArray[ i_MULS ][ i_MOVE ] = 1; 

	PairingArray[ i_MULU ][ i_DIVU ] = 1;
	PairingArray[ i_MULU ][ i_DIVS ] = 1;
	PairingArray[ i_MULS ][ i_DIVU ] = 1;
	PairingArray[ i_MULS ][ i_DIVS ] = 1;

	PairingArray[ i_BTST ][  i_Bcc ] = 1;

	M68000_InitPairing_BitShift ( i_ADD );
	M68000_InitPairing_BitShift ( i_SUB );
	M68000_InitPairing_BitShift ( i_OR );
	M68000_InitPairing_BitShift ( i_AND );
	M68000_InitPairing_BitShift ( i_EOR );
	M68000_InitPairing_BitShift ( i_NOT );
	M68000_InitPairing_BitShift ( i_CLR );
	M68000_InitPairing_BitShift ( i_NEG );
	M68000_InitPairing_BitShift ( i_ADDX );
	M68000_InitPairing_BitShift ( i_SUBX );
	M68000_InitPairing_BitShift ( i_ABCD );
	M68000_InitPairing_BitShift ( i_SBCD );

	PairingArray[ i_ADD ][ i_MOVE ] = 1;		/* when using xx(an,dn) addr mode */
	PairingArray[ i_SUB ][ i_MOVE ] = 1;
}


/**
 * One-time CPU initialization.
 */
void M68000_Init(void)
{
	/* Init UAE CPU core */
	Init680x0();

	/* Init the pairing matrix */
	M68000_InitPairing();
}

static int pendingInterrupts = 0;

/*-----------------------------------------------------------------------*/
/**
 * Reset CPU 68000 variables
 */
void M68000_Reset(bool bCold) {
    pendingInterrupts = 0;
    if (bCold) {
        /* Clear registers, but we need to keep SPCFLAG_MODE_CHANGE and SPCFLAG_BRK unchanged */
        int spcFlags = regs.spcflags & (SPCFLAG_MODE_CHANGE | SPCFLAG_BRK);
        memset(&regs, 0, sizeof(regs));
        regs.spcflags = spcFlags;
    }
    /* Now reset the WINUAE CPU core */
    m68k_reset(bCold);
    BusMode = BUS_MODE_CPU;
}


/*-----------------------------------------------------------------------*/
/**
 * Stop 680x0 emulation
 */
void M68000_Stop(void)
{
    M68000_SetSpecial(SPCFLAG_BRK);
}


/*-----------------------------------------------------------------------*/
/**
 * Start 680x0 emulation
 */
void M68000_Start(void)
{
	m68k_go(true);
}


/*-----------------------------------------------------------------------*/
/**
 * Check whether CPU settings have been changed.
 */
void M68000_CheckCpuSettings(void)
{
    if (ConfigureParams.System.nCpuFreq < 20)
    {
        ConfigureParams.System.nCpuFreq = 16;
    }
    else if (ConfigureParams.System.nCpuFreq < 24)
    {
        ConfigureParams.System.nCpuFreq = 20;
    }
    else if (ConfigureParams.System.nCpuFreq < 32)
    {
        ConfigureParams.System.nCpuFreq = 25;
    }
    else if (ConfigureParams.System.nCpuFreq < 40)
    {
        ConfigureParams.System.nCpuFreq = 33;
    } else {
        if (ConfigureParams.System.bTurbo) {
            ConfigureParams.System.nCpuFreq = 40;
        } else {
            ConfigureParams.System.nCpuFreq = 33;
        }
    }
	changed_prefs.cpu_level = ConfigureParams.System.nCpuLevel;
	changed_prefs.cpu_compatible = ConfigureParams.System.bCompatibleCpu;

	switch (changed_prefs.cpu_level) {
		case 0 : changed_prefs.cpu_model = 68000; break;
		case 1 : changed_prefs.cpu_model = 68010; break;
		case 2 : changed_prefs.cpu_model = 68020; break;
		case 3 : changed_prefs.cpu_model = 68030; break;
		case 4 : changed_prefs.cpu_model = 68040; break;
		case 5 : changed_prefs.cpu_model = 68060; break;
		default: fprintf (stderr, "Init680x0() : Error, cpu_level unknown\n");
	}

    changed_prefs.fpu_model = ConfigureParams.System.n_FPUType;
    switch (changed_prefs.fpu_model) {
        case 68881: changed_prefs.fpu_revision = 0x1f; break;
        case 68882: changed_prefs.fpu_revision = 0x20; break;
        case 68040:
            if (ConfigureParams.System.bTurbo)
                changed_prefs.fpu_revision = 0x41;
            else
                changed_prefs.fpu_revision = 0x40;
            break;
		default: fprintf (stderr, "Init680x0() : Error, fpu_model unknown\n");
    }

	changed_prefs.fpu_strict = ConfigureParams.System.bCompatibleFPU;
	changed_prefs.mmu_model = ConfigureParams.System.bMMU?changed_prefs.cpu_model:0;

	if (table68k)
		check_prefs_changed_cpu();
}

/*-----------------------------------------------------------------------*/
/**
 * BUSERROR - Access outside valid memory range.
 * Use bRead = 0 for write errors and bRead = 1 for read errors!
 */
void M68000_BusError(Uint32 addr, bool bRead)
{
	exception2 (addr, bRead, 0, regs.s ? 5 : 1); /* assumes data access,
                                                  size not set */
}
#if 0
void M68000_BusError(Uint32 addr, bool bRead)
{
	/* FIXME: In prefetch mode, m68k_getpc() seems already to point to the next instruction */
	// BusErrorPC = M68000_GetPC();		/* [NP] We set BusErrorPC in m68k_run_1 */

	
	if ((regs.spcflags & SPCFLAG_BUSERROR) == 0)	/* [NP] Check that the opcode has not already generated a read bus error */
	{
        regs.mmu_fault_addr = addr;
		BusErrorAddress = addr;				/* Store for exception frame */
		bBusErrorReadWrite = bRead;

        if (currprefs.mmu_model) {
            /* This is a hack for the special status word, this needs to be corrected later */
            if (ConfigureParams.System.nCpuLevel==3) { /* CPU 68030 */
                int fc = 5; /* hack */
                regs.mmu_ssw = (fc&1) ? MMU030_SSW_DF : (MMU030_SSW_FB|MMU030_SSW_RB);
                regs.mmu_ssw |= bRead ? MMU030_SSW_RW : 0;
                regs.mmu_ssw |= fc&MMU030_SSW_FC_MASK;
                /*switch (size) {
                 case 4: regs.mmu_ssw |= MMU030_SSW_SIZE_L; break;
                 case 2: regs.mmu_ssw |= MMU030_SSW_SIZE_W; break;
                 case 1: regs.mmu_ssw |= MMU030_SSW_SIZE_B; break;
                 default: break;
                 }*/
                printf("Bus Error: Warning! Using hacked SSW (%04X)!\n", regs.mmu_ssw);
            }
            THROW(2);
            return;
        }

		M68000_SetSpecial(SPCFLAG_BUSERROR);		/* The exception will be done in newcpu.c */
	}
}
#endif

/*-----------------------------------------------------------------------*/
/**
 * Exception handler
 */
void M68000_Exception(Uint32 ExceptionVector , int ExceptionSource)
{
	int exceptionNr = ExceptionVector/4;

	if ((ExceptionSource == M68000_EXC_SRC_AUTOVEC)
		&& (exceptionNr>24 && exceptionNr<32))	/* 68k autovector interrupt? */
	{
		/* Handle autovector interrupts the UAE's way
		 * (see intlev() and do_specialties() in UAE CPU core) */
		int intnr = exceptionNr - 24;
		pendingInterrupts |= (1 << intnr);
		M68000_SetSpecial(SPCFLAG_INT);
	}

	else							/* direct CPU exceptions */
	{
		Uint16 SR;

		/* Was the CPU stopped, i.e. by a STOP instruction? */
		if (regs.spcflags & SPCFLAG_STOP)
		{
			regs.stopped = 0;
			M68000_UnsetSpecial(SPCFLAG_STOP);    /* All is go,go,go! */
		}

		/* 68k exceptions are handled by Exception() of the UAE CPU core */
		Exception(exceptionNr/*, m68k_getpc(), ExceptionSource*/);

		SR = M68000_GetSR();

		/* Set Status Register so interrupt can ONLY be stopped by another interrupt
		 * of higher priority! */
		
		SR = (SR&SR_CLEAR_IPL)|0x0600;     /* DSP, level 6 */
        
        M68000_SetSR(SR);
	}
}