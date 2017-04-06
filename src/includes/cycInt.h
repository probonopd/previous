/*
  Hatari - cycInt.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
 
 (SC) Simon Schubiger - removed all MFP related code. NeXT does not have an MFP
*/

#ifndef HATARI_CYCINT_H
#define HATARI_CYCINT_H

/* Interrupt handlers in system */
typedef enum
{
  INTERRUPT_NULL,
  INTERRUPT_VIDEO_VBL,
  INTERRUPT_HARDCLOCK,
  INTERRUPT_MOUSE,
  INTERRUPT_ESP,
  INTERRUPT_ESP_IO,
  INTERRUPT_M2R,
  INTERRUPT_R2M,
  INTERRUPT_MO,
  INTERRUPT_MO_IO,
  INTERRUPT_ECC_IO,
  INTERRUPT_ENET_IO,
  INTERRUPT_FLP_IO,
  INTERRUPT_SND_OUT,
  INTERRUPT_SND_IN,
  INTERRUPT_LP_IO,
  INTERRUPT_EVENT_LOOP,
  INTERRUPT_ND_VBL,
  INTERRUPT_ND_VIDEO_VBL,
  MAX_INTERRUPTS
} interrupt_id;

/* Event timer structure - keeps next timer to occur in structure so don't need
 * to check all entries */

enum {
    CYC_INT_NONE,
    CYC_INT_CPU,
    CYC_INT_US,
};

typedef struct
{
    int     type;   /* Type of time (CPU Cycles, microseconds) or NONE for inactive */
    int64_t time;   /* number of CPU cycles to go until interupt or absolute microsecond timeout until interrupt */
    void (*pFunction)(void);
} INTERRUPTHANDLER;

INTERRUPTHANDLER PendingInterrupt;

extern int64_t nCyclesMainCounter;

extern int usCheckCycles;

void CycInt_Reset(void);
void CycInt_MemorySnapShot_Capture(bool bSave);
void CycInt_AcknowledgeInterrupt(void);
void CycInt_AddRelativeInterruptCycles(int64_t CycleTime, interrupt_id Handler);
void CycInt_AddRelativeInterruptTicks(int64_t TickTime, interrupt_id Handler);
void CycInt_AddRelativeInterruptUs(int64_t us, interrupt_id Handler);
void CycInt_RemovePendingInterrupt(interrupt_id Handler);
bool CycInt_InterruptActive(interrupt_id Handler);
bool CycInt_SetNewInterruptUs(void);

#endif /* ifndef HATARI_CYCINT_H */
