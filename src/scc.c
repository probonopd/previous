/*  Previous - scc.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Serial Communication Controller (Zilog 8530) Emulation.
 
 Based on MESS source code.
 
 Port to Previous incomplete. Hacked to pass power-on test --> see SCC_Reset()
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "scc.h"
#include "sysReg.h"
#include "dma.h"

#define LOG_SCC_LEVEL LOG_WARN
#define IO_SEG_MASK	0x1FFFF

#define SCC_BUFSIZE 10  // what is actual buffer size of our controller?


/* Variables */
bool MasterIRQEnable;
int lastIRQStat;
typedef enum {
    IRQ_NONE,
    IRQ_B_TX,
    IRQ_A_TX,
    IRQ_B_EXT,
    IRQ_A_EXT
} IRQ_TYPES;

IRQ_TYPES IRQType;

typedef struct {
    Uint8 rreg[16];
    Uint8 wreg[16];
    bool txIRQEnable;
    bool txIRQPending;
    bool extIRQEnable;
    bool extIRQPending;
    bool rxIRQEnable;
    bool rxIRQPending;
    bool rxEnable;
    bool txEnable;
    bool syncHunt;
    bool txUnderrun;
    
    Uint8 rx_buf[SCC_BUFSIZE];
    Uint8 tx_buf[SCC_BUFSIZE];
    Uint32 rx_buf_size;
    Uint32 tx_buf_size;
} SCC_CHANNEL;

SCC_CHANNEL channel[2];

int IRQV;

bool write_reg_pointer; // true --> byte written is number of register, false --> byte gets written to register
Uint8 regnumber;


void SCC_Read(void) {
    bool data;
    Uint8 ch;
    Uint8 reg;
        
    if (IoAccessCurrentAddress&0x1)
        ch = 1; // Channel A
    else
        ch = 0; // Channel B
    
    if (IoAccessCurrentAddress&0x2)
        data = true;
    else
        data = false;
    
    if (data) {
        IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = channel[ch].rx_buf[0];
        Log_Printf(LOG_SCC_LEVEL, "SCC %c, Data read: %02x\n", ch == 1?'A':'B', channel[ch].rx_buf[0]);
    } else {
        reg = regnumber;
        IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = channel[ch].rreg[reg];
        Log_Printf(LOG_SCC_LEVEL, "SCC %c, Reg%i read: %02x\n", ch == 1?'A':'B', reg, channel[ch].rreg[reg]);
    }
    write_reg_pointer = true;
}


void SCC_Write(void) {
    bool data;
    Uint8 ch;
    Uint8 reg;
    Uint8 val;
    
    if (IoAccessCurrentAddress&0x1)
        ch = 1;
    else
        ch = 0;
    
    if (IoAccessCurrentAddress&0x2)
        data = true;
    else
        data = false;
    
    if (data) {
        channel[ch].rx_buf[0] = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
        Log_Printf(LOG_SCC_LEVEL, "SCC %c, Data write: %02x\n", ch == 1?'A':'B', channel[ch].rx_buf[0]);
        channel[ch].rreg[R_STATUS] = RR0_TXEMPTY|RR0_RXAVAIL; // Tx buffer empty | Rx Character Available
        return;
    }
    
    if (write_reg_pointer == true) {
        regnumber = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
        write_reg_pointer = false;
        return;
    } else if (write_reg_pointer == false) {
        reg = regnumber;
        val = channel[ch].wreg[reg] = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
        Log_Printf(LOG_SCC_LEVEL, "SCC %c, Reg%i write: %02x\n", ch == 1?'A':'B', reg, channel[ch].wreg[reg]);
    
        switch (reg) {
            case W_INIT:
                switch ((val>>3) & 7) {
                    case 2: SCC_Interrupt(); break; // Reset Ext/Status Interrupts
                    case 5: channel[0].txIRQPending = false; break; // Reset pending Tx Interrupt
                    case 0: // Nothing
                    case 3: // Send SDLC abort
                    case 4: // Enable interrupt on next char Rx
                    case 6: // Error reset
                    case 7: // Reset Interrupt Under Service
                        break; // Not handled
                    default: break;
                }
                break;
                
            case W_MODE: // Tx/Rx IRQ and data transfer mode definition
                channel[ch].extIRQEnable = (val&1)?true:false; // External Interrupt Enable
                channel[ch].txIRQEnable = (val&2)?true:false; // Transmit Interrupt Enable
                channel[ch].rxIRQEnable = ((val&0x18)==0x18)?true:false; // Interrupt on Special only
                SCC_Interrupt();
                
                if (val&0x40 && val&0x80) {
                    dma_memory_read(channel[ch].rx_buf, &channel[ch].rx_buf_size, CHANNEL_SCC);
                    channel[ch].rreg[R_STATUS] = RR0_RXAVAIL; // Rx Character Available
                }
                break;
                
            case W_INTVEC: // Interrupt vector
                IRQV = channel[ch].rreg[R_INTVEC] = val;
                break;
                
            case W_RECCONT: // Rx parameters and controls
                channel[ch].rxEnable = channel[ch].wreg[reg]&1; // Rx Enable
                channel[ch].syncHunt = (channel[ch].wreg[reg]&0x10)?true:false; // Enter Hunt Mode
                break;
                
            case W_TRANSCONT: // Tx parameters and controls
                channel[ch].rxEnable = channel[ch].wreg[reg]&8;
                if (channel[ch].txEnable)
                    channel[ch].rreg[R_STATUS] |= RR0_TXEMPTY; // Tx Empty
                
            case W_MISCMODE:  // Tx/Rx miscellaneous parameters and modes
            case W_SYNCCHARA: // sync chars/SDLC address field
            case W_SYNCCHARF: // sync char/SDLC flag
                break;
                
            case W_TRANSBUF:
                channel[ch].tx_buf[0] = val;
                break;
                
            case W_MASTERINT: // Master Interrupt Control
                MasterIRQEnable = (val&8)?true:false;
                SCC_Interrupt();
                
                /* Reset channels */
                switch ((val>>6)&3) {
                    case 1: SCC_ResetChannel(0); break; // Reset Channel B
                    case 2: SCC_ResetChannel(1); break; // Reset Channel A
                    case 3: // Hardware Reset
                        SCC_Reset();
                        SCC_Interrupt();
                        break;
                    default: break;
                }
                break;
                
            case W_MISCCONT: // Miscellaneous transmitter/receiver control bits
            case W_CLOCK:    // Clock mode control
            case W_BRG_LOW:  // Lower byte of baud rate generator
            case W_BRG_HIGH: // Upper byte of baud rate generator
                break;
                
            case W_MISC: // Miscellaneous control bits
                if (val&0x01) // Baud rate generator enable?
                {} // later
                break;
                
            case W_EXTSTAT: // later ...
                break;
        }        
    }
    write_reg_pointer = true;
}



/* Functions */

void SCC_Interrupt(void)
{
	int irqstat;
    
	irqstat = 0;
	if (MasterIRQEnable)
	{
		if ((channel[0].txIRQEnable) && (channel[0].txIRQPending))
		{
			IRQType = IRQ_B_TX;
			irqstat = 1;
		}
		else if ((channel[1].txIRQEnable) && (channel[1].txIRQPending))
		{
			IRQType = IRQ_A_TX;
			irqstat = 1;
		}
		else if ((channel[0].extIRQEnable) && (channel[0].extIRQPending))
		{
			IRQType = IRQ_B_EXT;
			irqstat = 1;
		}
		else if ((channel[1].extIRQEnable) && (channel[1].extIRQPending))
		{
			IRQType = IRQ_A_EXT;
			irqstat = 1;
		}
	}
	else
	{
		IRQType = IRQ_NONE;
	}
    
    //  printf("SCC: irqstat %d, last %d\n", irqstat, lastIRQStat);
    //  printf("ch0: en %d pd %d  ch1: en %d pd %d\n", channel[0].txIRQEnable, channel[0].txIRQPending, channel[1].txIRQEnable, channel[1].txIRQPending);
    
	// don't spam the driver with unnecessary transitions
	if (irqstat != lastIRQStat)
	{
		lastIRQStat = irqstat;
        
		// tell the driver the new IRQ line status if possible
		Log_Printf(LOG_SCC_LEVEL, "SCC8530 IRQ status => %d\n", irqstat);

        if (irqstat) {
            set_interrupt(INT_SCC, SET_INT);
            Log_Printf(LOG_SCC_LEVEL, "SCC: Raise IRQ");
        }else{
            set_interrupt(INT_SCC, RELEASE_INT);
            Log_Printf(LOG_SCC_LEVEL, "SCC: Lower IRQ");
        }
        
//		if(!intrq_cb.isnull())
//			intrq_cb(irqstat);
	}
}


void SCC_ResetChannel(int ch)
{
//	emu_timer *timersave = channel[ch].baudtimer;
    
	memset(&channel[ch], 0, sizeof(SCC_CHANNEL));
    
	channel[ch].txUnderrun = 1;
//	channel[ch].baudtimer = timersave;
    
//	channel[ch].baudtimer->adjust(attotime::never, ch);
}

void SCC_InitChannel(int ch)
{
	channel[ch].syncHunt = 1;
}

void SCC_Reset(void) {
    Log_Printf(LOG_WARN, "SCC: Device Reset (Hacked!)");
    IRQType = IRQ_NONE;
    MasterIRQEnable = false;
    IRQV = 0;
    
    SCC_InitChannel(0);
    SCC_InitChannel(1);
    SCC_ResetChannel(0);
    SCC_ResetChannel(1);
    
    write_reg_pointer = true;
    regnumber = 0;
    
    /*--- Hack to pass power-on test ---*/
    channel[0].rreg[R_STATUS] = 0xFF;
    channel[1].rreg[R_STATUS] = 0xFF;
}