/*
  Hatari - cart.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Cartridge program

  To load programs into memory, through TOS, we need to intercept GEMDOS so we
  can relocate/execute programs via GEMDOS call $4B (Pexec).
  We have some 68000 assembler, located at 0xFA0000 (cartridge memory), which is
  used as our new GEMDOS handler. This checks if we need to intercept the call.

  The assembler routine can be found in 'cart_asm.s', and has been converted to
  a byte array and stored in 'Cart_data[]' (see cartData.c).
*/
const char Cart_fileid[] = "Hatari cart.c : " __DATE__ " " __TIME__;

/* 2007/12/09	[NP]	Change the function associated to opcodes $8, $a and $c only if hard drive	*/
/*			emulation is ON. Else, these opcodes should give illegal instructions (also	*/
/*			see uae-cpu/newcpu.c).								*/


#include "main.h"
#include "cart.h"
#include "configuration.h"
#include "file.h"
#include "log.h"
#include "m68000.h"
#include "nextMemory.h"
#include "hatari-glue.h"
#include "newcpu.h"

#include "cartData.c"


/* Possible cartridge file extensions to scan for */
static const char * const psCartNameExts[] =
{
	".img",
	".rom",
	".stc",
	NULL
};


/*-----------------------------------------------------------------------*/
/**
 * Load an external cartridge image file.
 */
static void Cart_LoadImage(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Copy ST GEMDOS intercept program image into cartridge memory space
 * or load an external cartridge file.
 * The intercept program is part of Hatari and used as an interface to the host
 * file system through GemDOS. It is also needed for Line-A-Init when using
 * extended VDI resolutions.
 */
void Cart_ResetImage(void)
{
}
