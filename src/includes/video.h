/*
  Hatari - video.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
 
 (SC) Simon Schubiger - most of it removed because it's not needed for NeXT emulation
*/

#ifndef HATARI_VIDEO_H
#define HATARI_VIDEO_H

/*--------------------------------------------------------------*/
/* Functions prototypes						*/
/*--------------------------------------------------------------*/

void	Video_MemorySnapShot_Capture(bool bSave);

void Video_Reset(void);
void	Video_Reset_Glue(void);

void	Video_StartInterrupts ( int PendingCyclesOver );
void	Video_InterruptHandler_VBL(void);

#endif  /* HATARI_VIDEO_H */
