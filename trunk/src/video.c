/*
  video.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

*/
const char Video_fileid[] = "Hatari video.c : " __DATE__ " " __TIME__;

#include <SDL_endian.h>

#include "main.h"
#include "configuration.h"
#include "cycles.h"
#include "cycInt.h"
#include "ioMem.h"
#include "m68000.h"
#include "memorySnapShot.h"
#include "screen.h"
#include "screenSnapShot.h"
#include "shortcut.h"
#include "nextMemory.h"
#include "video.h"
#include "avi_record.h"


/*--------------------------------------------------------------*/
/* Local functions prototypes                                   */
/*--------------------------------------------------------------*/

static void	Video_SetSystemTimings ( void );

static Uint32	Video_CalculateAddress ( void );
static int	Video_GetMMUStartCycle ( int DisplayStartCycle );
static void	Video_WriteToShifter ( Uint8 Res );
static void 	Video_Sync_SetDefaultStartEnd ( Uint8 Freq , int HblCounterVideo , int LineCycles );

static int	Video_HBL_GetPos ( void );
static void	Video_EndHBL ( void );
static void	Video_StartHBL ( void );

static void	Video_StoreFirstLinePalette(void);
static void	Video_StoreResolution(int y);
static void	Video_CopyScreenLineMono(void);
static void	Video_CopyScreenLineColor(void);
static void	Video_CopyVDIScreen(void);
static void	Video_SetHBLPaletteMaskPointers(void);

static void	Video_UpdateTTPalette(int bpp);
static void	Video_DrawScreen(void);

static void	Video_ResetShifterTimings(void);
static void	Video_InitShifterLines(void);
static void	Video_ClearOnVBL(void);

static void	Video_AddInterrupt ( int Pos , interrupt_id Handler );
static void	Video_AddInterruptHBL ( int Pos );

static void	Video_ColorReg_WriteWord(Uint32 addr);

int nScanlinesPerFrame = 400;                   /* Number of scan lines per frame */
int nCyclesPerLine = 512;

/*-----------------------------------------------------------------------*/
/**
 * Save/Restore snapshot of local variables('MemorySnapShot_Store' handles type)
 */
void Video_MemorySnapShot_Capture(bool bSave)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Reset video chip
 */
void Video_Reset(void)
{
	Video_StartInterrupts(0);
}


/*-----------------------------------------------------------------------*/
/**
 * Reset the GLUE chip responsible for generating the H/V sync signals.
 * When the 68000 RESET instruction is called, frequency and resolution
 * should be reset to 0.
 */
void Video_Reset_Glue(void)
{
}


/*-----------------------------------------------------------------------*/
/*
 * Set specific video timings, depending on the system being emulated.
 */
static void	Video_SetSystemTimings(void)
{
	Video_StartInterrupts(0);
}


/*-----------------------------------------------------------------------*/
/**
 * Convert the elapsed number of cycles since the start of the VBL
 * into the corresponding HBL number and the cycle position in the current
 * HBL. We use the starting cycle position of the closest HBL to compute
 * the cycle position on the line (this allows to mix lines with different
 * values for nCyclesPerLine).
 * We can have 2 cases on the limit where the real video line count can be
 * different from nHBL :
 * - when reading video address between cycle 0 and 12, LineCycle will be <0,
 *   so we need to use the data from line nHBL-1
 * - if LineCycle >= nCyclesPerLine, this means the HBL int was not processed
 *   yet, so the video line number is in fact nHBL+1
 */

void	Video_ConvertPosition ( int FrameCycles , int *pHBL , int *pLineCycles )
{
}


void	Video_GetPosition ( int *pFrameCycles , int *pHBL , int *pLineCycles )
{
}


void	Video_GetPosition_OnWriteAccess ( int *pFrameCycles , int *pHBL , int *pLineCycles )
{
}


void	Video_GetPosition_OnReadAccess ( int *pFrameCycles , int *pHBL , int *pLineCycles )
{
}


/*-----------------------------------------------------------------------*/
/**
 * Calculate and return video address pointer.
 */
static Uint32 Video_CalculateAddress ( void )
{
	return 0;
}


/*-----------------------------------------------------------------------*/
/**
 * Calculate the cycle where the STF/STE's MMU starts reading
 * data to send them to the shifter.
 * On STE, if hscroll is used, prefetch will cause this position to
 * happen 16 cycles earlier.
 * This function should use the same logic as in Video_CalculateAddress.
 * NOTE : this function is not completly accurate, as even when there's
 * no hscroll (on STF) the mmu starts reading 16 cycles before display starts.
 * But it's good enough to emulate writing to ff8205/07/09 on STE.
 */
static int Video_GetMMUStartCycle ( int DisplayStartCycle )
{
	return 0;
}


/*-----------------------------------------------------------------------*/
/**
 * Write to VideoShifter (0xff8260), resolution bits
 */
static void Video_WriteToShifter ( Uint8 Res )
{
}



/*-----------------------------------------------------------------------*/
/**
 * Set some default values for DisplayStartCycle/DisplayEndCycle
 * when changing frequency in lo/med res (testing orders are important
 * because the line can already have some borders changed).
 * This is necessary as some freq changes can modify start/end
 * even if they're not made at the exact borders' positions.
 * These values will be modified later if some borders are changed.
 */
static void	Video_Sync_SetDefaultStartEnd ( Uint8 Freq , int HblCounterVideo , int LineCycles )
{
}


/*-----------------------------------------------------------------------*/
/**
 * Write to VideoSync (0xff820a), Hz setting
 */
void Video_Sync_WriteByte ( void )
{
}


/*-----------------------------------------------------------------------*/
/**
 * Compute the cycle position where the HBL should happen on each line.
 * In low/med res, the position depends on the video frequency (50/60 Hz)
 * In high res, the position is always the same.
 * This position also gives the number of CPU cycles per video line.
 */
static int Video_HBL_GetPos ( void )
{
	return 0;
}


/*-----------------------------------------------------------------------*/
/**
 * Compute the cycle position where the timer B should happen on each
 * visible line.
 * We compute Timer B position for the given LineNumber, using start/end
 * display cycles from ShifterLines[ LineNumber ].
 * The position depends on the start of line / end of line positions
 * (which depend on the current frequency / border tricks) and
 * on the value of the bit 3 in the MFP's AER.
 * If bit is 0, timer B will count end of line events (usual case),
 * but if bit is 1, timer B will count start of line events (eg Seven Gates Of Jambala)
 */
int Video_TimerB_GetPos ( int LineNumber )
{
	return 0;
}


/*-----------------------------------------------------------------------*/
/**
 * HBL interrupt : this occurs at the end of every line, on cycle 512 (in 50 Hz)
 * It takes 56 cycles to handle the 68000's exception.
 */
void Video_InterruptHandler_HBL ( void )
{
}


/*-----------------------------------------------------------------------*/
/**
 * Check at end of each HBL to see if any Shifter hardware tricks have been attempted
 * and copy the line to the screen buffer.
 * This is the place to check if top/bottom border were removed, as well as if some
 * left/right border changes were not validated before.
 * NOTE : the tests must be made with nHBL in ascending order.
 */
static void Video_EndHBL(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Set default values for the next HBL, depending on the current res/freq.
 * We set the number of cycles per line, as well as some default values
 * for display start/end cycle.
 */
static void Video_StartHBL(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * End Of Line interrupt
 * This interrupt is started on cycle position 404 in 50 Hz and on cycle
 * position 400 in 60 Hz. 50 Hz display ends at cycle 376 and 60 Hz displays
 * ends at cycle 372. This means the EndLine interrupt happens 28 cycles
 * after DisplayEndCycle.
 * Note that if bit 3 of MFP AER is 1, then timer B will count start of line
 * instead of end of line (at cycle 52+28 or 56+28)
 */
void Video_InterruptHandler_EndLine(void)
{
}




/*-----------------------------------------------------------------------*/
/**
 * Store whole palette on first line so have reference to work from
 */
static void Video_StoreFirstLinePalette(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Store resolution on each line (used to test if mixed low/medium resolutions)
 */
static void Video_StoreResolution(int y)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Copy one line of monochrome screen into buffer for conversion later.
 */
static void Video_CopyScreenLineMono(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Copy one line of color screen into buffer for conversion later.
 * Possible lines may be top/bottom border, and/or left/right borders.
 */
static void Video_CopyScreenLineColor(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Copy extended GEM resolution screen
 */
static void Video_CopyVDIScreen(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Clear raster line table to store changes in palette/resolution on a line
 * basic. Called once on VBL interrupt.
 */
void Video_SetScreenRasters(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Set pointers to HBLPalette tables to store correct colours/resolutions
 */
static void Video_SetHBLPaletteMaskPointers(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Set video shifter timing variables according to screen refresh rate.
 * Note: The following equation must be satisfied for correct timings:
 *
 *   nCyclesPerLine * nScanlinesPerFrame * nScreenRefreshRate = 8 MHz
 */
static void Video_ResetShifterTimings(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Clear the array indicating the state of each video line.
 */
static void Video_InitShifterLines ( void )
{
}


/*-----------------------------------------------------------------------*/
/**
 * Called on VBL, set registers ready for frame
 */
static void Video_ClearOnVBL(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Get width, height and bpp according to TT-Resolution
 */
void Video_GetTTRes(int *width, int *height, int *bpp)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Convert TT palette to SDL palette
 */
static void Video_UpdateTTPalette(int bpp)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Update TT palette and blit TT screen using VIDEL code.
 * @return  true if the screen contents changed
 */
bool Video_RenderTTScreen(void)
{
	return true;
}


/*-----------------------------------------------------------------------*/
/**
 * Draw screen (either with ST/STE shifter drawing functions or with
 * Videl drawing functions)
 */
static void Video_DrawScreen(void)
{
//        memcpy(pNEXTScreen, NEXTVideo, (1024*768)/8);
Screen_Draw();
}


/*-----------------------------------------------------------------------*/
/**
 * Start HBL, Timer B and VBL interrupts.
 */


/**
 * Start HBL or Timer B interrupt at position Pos. If position Pos was
 * already reached, then the interrupt is set on the next line.
 */

static void Video_AddInterrupt ( int Pos , interrupt_id Handler )
{
}


static void Video_AddInterruptHBL ( int Pos )
{
}


void Video_AddInterruptTimerB ( int Pos )
{
}


/**
 * Add some video interrupts to handle the first HBL and the first Timer B
 */
void Video_StartInterrupts ( int PendingCyclesOver )
{
	int CyclesPerVBL;
	
        CyclesPerVBL = CYCLES_PER_FRAME;
        CycInt_AddRelativeInterrupt(CyclesPerVBL, INT_CPU_CYCLE, INTERRUPT_VIDEO_VBL);
}


/*-----------------------------------------------------------------------*/
/**
 * VBL interrupt : set new interrupts, draw screen, generate sound,
 * reset counters, ...
 */
void Video_InterruptHandler_VBL ( void )
{
	CycInt_AcknowledgeInterrupt();
	Video_DrawScreen();
        Main_EventHandler();
        CycInt_AddRelativeInterrupt(CYCLES_PER_FRAME, INT_CPU_CYCLE, INTERRUPT_VIDEO_VBL);
}


/*-----------------------------------------------------------------------*/
/**
 * Write to video address base high, med and low register (0xff8201/03/0d).
 * On STE, when a program writes to high or med registers, base low register
 * is reset to zero.
 */
void Video_ScreenBaseSTE_WriteByte(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Read video address counter and update ff8205/07/09
 */
void Video_ScreenCounter_ReadByte(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 */
void Video_ScreenCounter_WriteByte(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Read video sync register (0xff820a)
 */
void Video_Sync_ReadByte(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Read video base address low byte (0xff820d). A plain ST can only store
 * screen addresses rounded to 256 bytes (i.e. no lower byte).
 */
void Video_BaseLow_ReadByte(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Read video line width register (0xff820f)
 */
void Video_LineWidth_ReadByte(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Read video shifter mode register (0xff8260)
 */
void Video_ShifterMode_ReadByte(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Read horizontal scroll register (0xff8265)
 */
void Video_HorScroll_Read(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Write video line width register (0xff820f) - STE only.
 * Content of LineWidth is added to the shifter counter when display is
 * turned off (start of the right border, usually at cycle 376)
 */
void Video_LineWidth_WriteByte(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 */
static void Video_ColorReg_WriteWord(Uint32 addr)
{
}

void Video_Color0_WriteWord(void)
{
}

void Video_Color1_WriteWord(void)
{
}

void Video_Color2_WriteWord(void)
{
}

void Video_Color3_WriteWord(void)
{
}

void Video_Color4_WriteWord(void)
{
}

void Video_Color5_WriteWord(void)
{
}

void Video_Color6_WriteWord(void)
{
}

void Video_Color7_WriteWord(void)
{
}

void Video_Color8_WriteWord(void)
{
}

void Video_Color9_WriteWord(void)
{
}

void Video_Color10_WriteWord(void)
{
}

void Video_Color11_WriteWord(void)
{
}

void Video_Color12_WriteWord(void)
{
}

void Video_Color13_WriteWord(void)
{
}

void Video_Color14_WriteWord(void)
{
}

void Video_Color15_WriteWord(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Write video shifter mode register (0xff8260)
 */
void Video_ShifterMode_WriteByte(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 */

void Video_HorScroll_Write_8264(void)
{
}

void Video_HorScroll_Write_8265(void)
{
}

void Video_HorScroll_Write(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Write to TT shifter mode register (0xff8262)
 */
void Video_TTShiftMode_WriteWord(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Write to TT color register (0xff8400)
 */
void Video_TTColorRegs_WriteWord(void)
{
}

/*-----------------------------------------------------------------------*/
/**
 * Write to ST color register on TT (0xff8240)
 */
void Video_TTColorSTRegs_WriteWord(void)
{
}
