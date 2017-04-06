/*
  Hatari - ioMem.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This is where we intercept read/writes to/from the hardware. 
*/
const char IoMem_fileid[] = "Hatari ioMem.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "sysdeps.h"
#include "shortcut.h"

#define IO_SEG_MASK 0x0001FFFF
#define IO_MASK 0x0001FFFF
#define IO_SIZE 0x00020000

static void (*pInterceptReadTable[IO_SIZE])(void);     /* Table with read access handlers */
static void (*pInterceptWriteTable[IO_SIZE])(void);    /* Table with write access handlers */

int nIoMemAccessSize;                                 /* Set to 1, 2 or 4 according to byte, word or long word access */
Uint32 IoAccessBaseAddress;                           /* Stores the base address of the IO mem access */
Uint32 IoAccessCurrentAddress;                        /* Current byte address while handling WORD and LONG accesses */
static int nBusErrorAccesses;                         /* Needed to count bus error accesses */


/*-----------------------------------------------------------------------*/
/**
 * Fill a region with bus error handlers.
 */
static void IoMem_SetBusErrorRegion(Uint32 startaddr, Uint32 endaddr)
{
	Uint32 a;

	for (a = startaddr; a <= endaddr; a++)
	{
		if (a & 1)
		{
			pInterceptReadTable[a & 0x1FFFF] = IoMem_BusErrorOddReadAccess;     /* For 'read' */
			pInterceptWriteTable[a & 0x1FFFF] = IoMem_BusErrorOddWriteAccess;   /* and 'write' */
		}
		else
		{
			pInterceptReadTable[a & 0x1FFFF] = IoMem_BusErrorEvenReadAccess;    /* For 'read' */
			pInterceptWriteTable[a & 0x1FFFF] = IoMem_BusErrorEvenWriteAccess;  /* and 'write' */
		}
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Create 'intercept' tables for hardware address access. Each 'intercept
 */
void IoMem_Init(void)
{
	Uint32 addr;
	int i;
	const INTERCEPT_ACCESS_FUNC *pInterceptAccessFuncs = NULL;

	/* Set default IO access handler (-> bus error) */
	IoMem_SetBusErrorRegion(0x02000000, 0x0201FFFF);

	if (ConfigureParams.System.bTurbo) {
		pInterceptAccessFuncs = IoMemTable_Turbo;
	} else {
		pInterceptAccessFuncs = IoMemTable_NEXT;
	}

	/* Now set the correct handlers */
	for (addr=0x02000000; addr <= 0x0201FFFF; addr++)
	{
		/* Does this hardware location/span appear in our list of possible intercepted functions? */
		for (i=0; pInterceptAccessFuncs[i].Address != 0; i++)
		{
			if (addr >= pInterceptAccessFuncs[i].Address
			    && addr < pInterceptAccessFuncs[i].Address+pInterceptAccessFuncs[i].SpanInBytes)
			{
				/* Security checks... */
				if (pInterceptReadTable[addr & 0x1FFFF] != IoMem_BusErrorEvenReadAccess && pInterceptReadTable[addr&0x1FFFF] != IoMem_BusErrorOddReadAccess)
					fprintf(stderr, "IoMem_Init: Warning: $%x (R) already defined\n", addr);
				if (pInterceptWriteTable[addr& 0x1FFFF] != IoMem_BusErrorEvenWriteAccess && pInterceptWriteTable[addr&0x1FFFF] != IoMem_BusErrorOddWriteAccess)
					fprintf(stderr, "IoMem_Init: Warning: $%x (W) already defined\n", addr);

				/* This location needs to be intercepted, so add entry to list */
				pInterceptReadTable[addr& 0x1FFFF] = pInterceptAccessFuncs[i].ReadFunc;
				pInterceptWriteTable[addr& 0x1FFFF] = pInterceptAccessFuncs[i].WriteFunc;
			}
		}
	}


}


/*-----------------------------------------------------------------------*/
/**
 * Uninitialize the IoMem code (currently unused).
 */
void IoMem_UnInit(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Handle byte read access from IO memory.
 */
uae_u32 IoMem_bget(uaecptr addr)
{
	Uint8 val;

	if ((addr & IO_SEG_MASK) >= IO_SIZE)
	{
		/* invalid memory addressing --> bus error */
		M68000_BusError(addr, BUS_ERROR_READ);
		return -1;
	}

	IoAccessBaseAddress = addr;                   /* Store access location */
	nIoMemAccessSize = SIZE_BYTE;
	nBusErrorAccesses = 0;

	IoAccessCurrentAddress = addr;
	pInterceptReadTable[addr & IO_SEG_MASK]();         /* Call handler */

	/* Check if we read from a bus-error region */
	if (nBusErrorAccesses == 1)
	{
		M68000_BusError(addr, BUS_ERROR_READ);
		return -1;
	}

	val = IoMem[addr & IO_SEG_MASK];

	LOG_TRACE(TRACE_IOMEM_RD, "IO read.b $%06x = $%02x\n", addr, val);

	return val;
}


/*-----------------------------------------------------------------------*/
/**
 * Handle word read access from IO memory.
 */
uae_u32 IoMem_wget(uaecptr addr)
{
	Uint32 idx;
	Uint16 val;


	if ((addr & IO_SEG_MASK) >= IO_SIZE)
	{
		/* invalid memory addressing --> bus error */
		M68000_BusError(addr, BUS_ERROR_READ);
		return -1;
	}

	IoAccessBaseAddress = addr;                   /* Store for exception frame */
	nIoMemAccessSize = SIZE_WORD;
	nBusErrorAccesses = 0;
	idx = addr & IO_SEG_MASK;

	IoAccessCurrentAddress = addr;
	pInterceptReadTable[idx]();                   /* Call 1st handler */

	if (pInterceptReadTable[idx+1] != pInterceptReadTable[idx])
	{
		IoAccessCurrentAddress = addr + 1;
		pInterceptReadTable[idx+1]();             /* Call 2nd handler */
	}

	/* Check if we completely read from a bus-error region */
	if (nBusErrorAccesses == 2)
	{
		M68000_BusError(addr, BUS_ERROR_READ);
		return -1;
	}

	val = IoMem_ReadWord(addr);

	LOG_TRACE(TRACE_IOMEM_RD, "IO read.w $%06x = $%04x\n", addr, val);

	return val;
}


/*-----------------------------------------------------------------------*/
/**
 * Handle long-word read access from IO memory.
 */
uae_u32 IoMem_lget(uaecptr addr)
{
	Uint32 idx;
	Uint32 val;


	if ((addr & IO_SEG_MASK) >= IO_SIZE)
	{
		/* invalid memory addressing --> bus error */
		M68000_BusError(addr, BUS_ERROR_READ);
		return -1;
	}

	IoAccessBaseAddress = addr;                   /* Store for exception frame */
	nIoMemAccessSize = SIZE_LONG;
	nBusErrorAccesses = 0;
	idx = addr & IO_SEG_MASK;

	IoAccessCurrentAddress = addr;
	pInterceptReadTable[idx]();                   /* Call 1st handler */

	if (pInterceptReadTable[idx+1] != pInterceptReadTable[idx])
	{
		IoAccessCurrentAddress = addr + 1;
		pInterceptReadTable[idx+1]();             /* Call 2nd handler */
	}

	if (pInterceptReadTable[idx+2] != pInterceptReadTable[idx+1])
	{
		IoAccessCurrentAddress = addr + 2;
		pInterceptReadTable[idx+2]();             /* Call 3rd handler */
	}

	if (pInterceptReadTable[idx+3] != pInterceptReadTable[idx+2])
	{
		IoAccessCurrentAddress = addr + 3;
		pInterceptReadTable[idx+3]();             /* Call 4th handler */
	}

	/* Check if we completely read from a bus-error region */
	if (nBusErrorAccesses == 4)
	{
		M68000_BusError(addr, BUS_ERROR_READ);
		return -1;
	}

	val = IoMem_ReadLong(addr);

	LOG_TRACE(TRACE_IOMEM_RD, "IO read.l $%06x = $%08x\n", addr, val);

	return val;
}


/*-----------------------------------------------------------------------*/
/**
 * Handle byte write access to IO memory.
 */
void IoMem_bput(uaecptr addr, uae_u32 val)
{

	LOG_TRACE(TRACE_IOMEM_WR, "IO write.b $%06x = $%02x\n", addr, val&0x0ff);

	if ((addr & IO_SEG_MASK) >= IO_SIZE)
	{
		/* invalid memory addressing --> bus error */
		M68000_BusError(addr, BUS_ERROR_WRITE);
		return;
	}

	IoAccessBaseAddress = addr;                   /* Store for exception frame, just in case */
	nIoMemAccessSize = SIZE_BYTE;
	nBusErrorAccesses = 0;

	IoMem[addr & IO_SEG_MASK] = val;

	IoAccessCurrentAddress = addr;
	pInterceptWriteTable[addr & IO_SEG_MASK]();        /* Call handler */

	/* Check if we wrote to a bus-error region */
	if (nBusErrorAccesses == 1)
	{
		M68000_BusError(addr, BUS_ERROR_WRITE);
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Handle word write access to IO memory.
 */
void IoMem_wput(uaecptr addr, uae_u32 val)
{
	Uint32 idx;


	LOG_TRACE(TRACE_IOMEM_WR, "IO write.w $%06x = $%04x\n", addr, val&0xffff);

	if ((addr & IO_SEG_MASK) >= IO_SIZE)
	{
		/* invalid memory addressing --> bus error */
		M68000_BusError(addr, BUS_ERROR_WRITE);
		return;
	}

	IoAccessBaseAddress = addr;                   /* Store for exception frame, just in case */
	nIoMemAccessSize = SIZE_WORD;
	nBusErrorAccesses = 0;

	IoMem_WriteWord(addr, val);
	idx = addr & IO_SEG_MASK;

	IoAccessCurrentAddress = addr;
	pInterceptWriteTable[idx]();                  /* Call 1st handler */

	if (pInterceptWriteTable[idx+1] != pInterceptWriteTable[idx])
	{
		IoAccessCurrentAddress = addr + 1;
		pInterceptWriteTable[idx+1]();            /* Call 2nd handler */
	}

	/* Check if we wrote to a bus-error region */
	if (nBusErrorAccesses == 2)
	{
		M68000_BusError(addr, BUS_ERROR_WRITE);
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Handle long-word write access to IO memory.
 */
void IoMem_lput(uaecptr addr, uae_u32 val)
{
	Uint32 idx;

	LOG_TRACE(TRACE_IOMEM_WR, "IO write.l $%06x = $%08x\n", addr, val);

	if ((addr & IO_SEG_MASK) >= IO_SIZE)
	{
		/* invalid memory addressing --> bus error */
		M68000_BusError(addr, BUS_ERROR_WRITE);
		return;
	}

	IoAccessBaseAddress = addr;                   /* Store for exception frame, just in case */
	nIoMemAccessSize = SIZE_LONG;
	nBusErrorAccesses = 0;

	IoMem_WriteLong(addr, val);
	idx = addr & IO_SEG_MASK;

	IoAccessCurrentAddress = addr;
	pInterceptWriteTable[idx]();                  /* Call handler */

	if (pInterceptWriteTable[idx+1] != pInterceptWriteTable[idx])
	{
		IoAccessCurrentAddress = addr + 1;
		pInterceptWriteTable[idx+1]();            /* Call 2nd handler */
	}

	if (pInterceptWriteTable[idx+2] != pInterceptWriteTable[idx+1])
	{
		IoAccessCurrentAddress = addr + 2;
		pInterceptWriteTable[idx+2]();            /* Call 3rd handler */
	}

	if (pInterceptWriteTable[idx+3] != pInterceptWriteTable[idx+2])
	{
		IoAccessCurrentAddress = addr + 3;
		pInterceptWriteTable[idx+3]();            /* Call 4th handler */
	}

	/* Check if we wrote to a bus-error region */
	if (nBusErrorAccesses == 4)
	{
		M68000_BusError(addr, BUS_ERROR_WRITE);
	}
}


/*-------------------------------------------------------------------------*/
/**
 * This handler will be called if a program tries to read from an address
 * that causes a bus error on a real machine. However, we can't call M68000_BusError()
 * directly: For example, a "move.b $ff8204,d0" triggers a bus error on a real ST,
 * while a "move.w $ff8204,d0" works! So we have to count the accesses to bus error
 * addresses and we only trigger a bus error later if the count matches the complete
 * access size (e.g. nBusErrorAccesses==4 for a long word access).
 */
void IoMem_BusErrorEvenReadAccess(void)
{
	nBusErrorAccesses += 1;
	// IoMem[IoAccessCurrentAddress& IO_SEG_MASK] = 0xff;
	Log_Printf(LOG_WARN,"Bus error $%08x PC=$%08x %s at %d", IoAccessCurrentAddress,regs.pc,__FILE__,__LINE__);
}

/**
 * We need two handler so that the IoMem_*get functions can distinguish
 * consecutive addresses.
 */
void IoMem_BusErrorOddReadAccess(void)
{
	nBusErrorAccesses += 1;
	// IoMem[IoAccessCurrentAddress& IO_SEG_MASK] = 0xff;
	Log_Printf(LOG_WARN,"Bus error $%08x PC=$%08x %s at %d", IoAccessCurrentAddress,regs.pc,__FILE__,__LINE__);
}

/*-------------------------------------------------------------------------*/
/**
 * Same as IoMem_BusErrorReadAccess() but for write access this time.
 */
void IoMem_BusErrorEvenWriteAccess(void)
{
	nBusErrorAccesses += 1;
	Log_Printf(LOG_WARN,"Bus error $%08x PC=$%08x %s at %d", IoAccessCurrentAddress,regs.pc,__FILE__,__LINE__);
}

/**
 * We need two handler so that the IoMem_*put functions can distinguish
 * consecutive addresses.
 */
void IoMem_BusErrorOddWriteAccess(void)
{
	nBusErrorAccesses += 1;
	Log_Printf(LOG_WARN,"Bus error $%08x PC=$%08x %s at %d", IoAccessCurrentAddress,regs.pc,__FILE__,__LINE__);
}


/*-------------------------------------------------------------------------*/
/**
 * This is the read handler for the IO memory locations without an assigned
 * IO register and which also do not generate a bus error. Reading from such
 * a register will return the result 0xff.
 */
void IoMem_VoidRead(void)
{
	Uint32 a;

	/* handler is probably called only once, so we have to take care of the neighbour "void IO registers" */
	for (a = IoAccessBaseAddress; a < IoAccessBaseAddress + nIoMemAccessSize; a++)
	{
		if (pInterceptReadTable[a & IO_SEG_MASK] == IoMem_VoidRead)
		{
			IoMem[a & IO_SEG_MASK] = 0xff;
		}
	}
	Log_Printf(LOG_WARN,"IO read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
}

/*-------------------------------------------------------------------------*/
/**
 * This is the write handler for the IO memory locations without an assigned
 * IO register and which also do not generate a bus error. We simply ignore
 * a write access to these registers.
 */
void IoMem_VoidWrite(void)
{
	/* Nothing... */
	Log_Printf(LOG_WARN,"IO write at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
}


/*-------------------------------------------------------------------------*/
/**
 * A dummy function that does nothing at all - for memory regions that don't
 * need a special handler for read access.
 */
void IoMem_ReadWithoutInterception(void)
{
	/* Nothing... */
}

/*-------------------------------------------------------------------------*/
/**
 * A dummy function that does nothing at all - for memory regions that don't
 * need a special handler for write access.
 */
void IoMem_WriteWithoutInterception(void)
{
	/* Nothing... */
}

/*-------------------------------------------------------------------------*/
/**
 * A dummy function that does nothing at all - for memory regions that don't
 * need a special handler for read access.
 */
void IoMem_ReadWithoutInterceptionButTrace(void)
{
	Log_Printf(LOG_WARN,"IO read at $%08x PC=$%08x\n", IoAccessCurrentAddress,regs.pc);
}

/*-------------------------------------------------------------------------*/
/**
 * A dummy function that does nothing at all - for memory regions that don't
 * need a special handler for write access.
 */
void IoMem_WriteWithoutInterceptionButTrace(void)
{
	Log_Printf(LOG_WARN,"IO write at $%08x val=%02x PC=$%08x\n", IoAccessCurrentAddress,IoMem[IoAccessCurrentAddress & IO_SEG_MASK],regs.pc);
}

/*-------------------------------------------------------------------------*/
/* Jump into debugger upon access
 */
void IoMem_Debug(void) {
    ShortCut_Debug_M68K();
}
