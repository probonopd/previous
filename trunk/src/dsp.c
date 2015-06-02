/* DSP emulation */


#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "sysReg.h"
#include "dma.h"
#include "dsp.h"
#include "cycInt.h"


#define LOG_DSP_REG_LEVEL   LOG_WARN

#define IO_SEG_MASK	0x1FFFF


/* Controller */
struct {
    Uint8 icr;      /*  */
    Uint8 cvr;      /*  */
    Uint8 isr;      /*  */
    Uint8 ivr;      /*  */
    Uint8 data[4];  /*  */
} dsp;


/* Register bits */

#define ICR_INIT    0x80
#define ICR_HM1     0x40
#define ICR_HM0     0x20
#define ICR_HF1     0x10
#define ICR_HF0     0x08
#define ICR_TREQ    0x02
#define ICR_RREQ    0x01

#define CVR_HC      0x80
#define CVR_HV      0x1F

#define ISR_HREQ    0x80
#define ISR_DMA     0x40
#define ISR_HF3     0x10
#define ISR_HF2     0x08
#define ISR_TRDY    0x04
#define ISR_TXDE    0x02
#define ISR_RXDF    0x01


void DSP_ICR_Read(void) { // 0x02008000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = 0x7F;//dsp.icr;
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] ICR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_ICR_Write(void) {
    dsp.icr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] ICR write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_CVR_Read(void) { // 0x02008001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = 0xFF;//dsp.cvr;
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] CVR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_CVR_Write(void) {
    dsp.cvr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] CVR write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_ISR_Read(void) { // 0x02008002
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = 0xFF;//dsp.isr;
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] ISR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_ISR_Write(void) {
    dsp.isr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] ISR write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_IVR_Read(void) { // 0x02008003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = 0xFF;//dsp.ivr;
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] IVR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_IVR_Write(void) {
    dsp.ivr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] IVR write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_Data0_Read(void) { // 0x02008004
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = dsp.data[0];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] Data0 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_Data0_Write(void) {
    dsp.data[0] = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] Data0 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_Data1_Read(void) { // 0x02008005
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = dsp.data[1];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] Data1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_Data1_Write(void) {
    dsp.data[1] = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] Data1 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_Data2_Read(void) { // 0x02008006
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = dsp.data[2];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] Data2 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_Data2_Write(void) {
    dsp.data[2] = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] Data2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_Data3_Read(void) { // 0x02008007
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = dsp.data[3];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] Data3 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void DSP_Data3_Write(void) {
    dsp.data[3] = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    Log_Printf(LOG_DSP_REG_LEVEL,"[DSP] Data3 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}
