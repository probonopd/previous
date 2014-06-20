/* NeXT DMA Emulation 
 * Contains informations from QEMU-NeXT
 * NeXT Integrated Channel Processor (ISP) consists of 12 channel processors
 * with 128 bytes internal buffer for each channel.
 * 12 channels:
 * SCSI, Sound in, Sound out, Optical disk, Printer, SCC, DSP, 
 * Ethernet transmit, Ethernet receive, Video, Memory to register, Register to memory
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "scsi.h"
#include "esp.h"
#include "mo.h"
#include "scc.h"
#include "sysReg.h"
#include "dma.h"
#include "configuration.h"
#include "ethernet.h"
#include "mmu_common.h"



#define LOG_DMA_LEVEL LOG_DEBUG

#define IO_SEG_MASK	0x1FFFF

enum {
    CHANNEL_SCSI,       // 0x00000010
    CHANNEL_SOUNDOUT,   // 0x00000040
    CHANNEL_DISK,       // 0x00000050
    CHANNEL_SOUNDIN,    // 0x00000080
    CHANNEL_PRINTER,    // 0x00000090
    CHANNEL_SCC,        // 0x000000c0
    CHANNEL_DSP,        // 0x000000d0
    CHANNEL_EN_TX,      // 0x00000110
    CHANNEL_EN_RX,      // 0x00000150
    CHANNEL_VIDEO,      // 0x00000180
    CHANNEL_M2R,        // 0x000001d0
    CHANNEL_R2M         // 0x000001c0
} DMA_CHANNEL;

int get_channel(Uint32 address);
int get_interrupt_type(int channel);
void dma_interrupt(int channel);
void dma_initialize_buffer(int channel);


struct {
    Uint8 csr;
    Uint32 saved_next;
    Uint32 saved_limit;
    Uint32 saved_start;
    Uint32 saved_stop;
    Uint32 next;
    Uint32 limit;
    Uint32 start;
    Uint32 stop;
    
    Uint8 direction;
} dma[12];


/* DMA internal buffers */
#define DMA_BURST_SIZE  16

int espdma_buf_size = 0;
int espdma_buf_limit = 0;
Uint8 espdma_buf[DMA_BURST_SIZE];
int modma_buf_size = 0;
int modma_buf_limit = 0;
Uint8 modma_buf[DMA_BURST_SIZE];
int enrxdma_buf_size = 0;
int enrxdma_buf_limit = 0;
Uint8 enrxdma_buf[DMA_BURST_SIZE];


/* Read and write CSR bits for 68030 based NeXT Computer. */

/* read CSR bits */
#define DMA_ENABLE      0x01   /* enable dma transfer */
#define DMA_SUPDATE     0x02   /* single update */
#define DMA_COMPLETE    0x08   /* current dma has completed */
#define DMA_BUSEXC      0x10   /* bus exception occurred */
/* write CSR bits */
#define DMA_SETENABLE   0x01   /* set enable */
#define DMA_SETSUPDATE  0x02   /* set single update */
#define DMA_M2DEV       0x00   /* dma from mem to dev */
#define DMA_DEV2M       0x04   /* dma from dev to mem */
#define DMA_CLRCOMPLETE 0x08   /* clear complete conditional */
#define DMA_RESET       0x10   /* clr cmplt, sup, enable */
#define DMA_INITBUF     0x20   /* initialize DMA buffers */

/* CSR masks */
#define DMA_CMD_MASK    (DMA_SETENABLE|DMA_SETSUPDATE|DMA_CLRCOMPLETE|DMA_RESET|DMA_INITBUF)
#define DMA_STAT_MASK   (DMA_ENABLE|DMA_SUPDATE|DMA_COMPLETE|DMA_BUSEXC)


/* Read and write CSR bits for 68040 based Machines.
 * We convert these to 68030 values before using in functions.
 * read CSR bits *
 #define DMA_ENABLE      0x01000000
 #define DMA_SUPDATE     0x02000000
 #define DMA_COMPLETE    0x08000000
 #define DMA_BUSEXC      0x10000000
 * write CSR bits *
 #define DMA_SETENABLE   0x00010000
 #define DMA_SETSUPDATE  0x00020000
 #define DMA_M2DEV       0x00000000
 #define DMA_DEV2M       0x00040000
 #define DMA_CLRCOMPLETE 0x00080000
 #define DMA_RESET       0x00100000
 #define DMA_INITBUF     0x00200000
 */



static inline Uint32 dma_getlong(Uint8 *buf, Uint32 pos) {
	return (buf[pos] << 24) | (buf[pos+1] << 16) | (buf[pos+2] << 8) | buf[pos+3];
}

static inline void dma_putlong(Uint32 val, Uint8 *buf, Uint32 pos) {
	buf[pos] = val >> 24;
	buf[pos+1] = val >> 16;
	buf[pos+2] = val >> 8;
	buf[pos+3] = val;
}


int get_channel(Uint32 address) {
    int channel = address&IO_SEG_MASK;

    switch (channel) {
        case 0x010: Log_Printf(LOG_DMA_LEVEL,"channel SCSI:"); return CHANNEL_SCSI; break;
        case 0x040: Log_Printf(LOG_DMA_LEVEL,"channel Sound Out:"); return CHANNEL_SOUNDOUT; break;
        case 0x050: Log_Printf(LOG_DMA_LEVEL,"channel MO Disk:"); return CHANNEL_DISK; break;
        case 0x080: Log_Printf(LOG_DMA_LEVEL,"channel Sound in:"); return CHANNEL_SOUNDIN; break;
        case 0x090: Log_Printf(LOG_DMA_LEVEL,"channel Printer:"); return CHANNEL_PRINTER; break;
        case 0x0c0: Log_Printf(LOG_DMA_LEVEL,"channel SCC:"); return CHANNEL_SCC; break;
        case 0x0d0: Log_Printf(LOG_DMA_LEVEL,"channel DSP:"); return CHANNEL_DSP; break;
        case 0x110: Log_Printf(LOG_DMA_LEVEL,"channel Ethernet Tx:"); return CHANNEL_EN_TX; break;
        case 0x150: Log_Printf(LOG_DMA_LEVEL,"channel Ethernet Rx:"); return CHANNEL_EN_RX; break;
        case 0x180: Log_Printf(LOG_DMA_LEVEL,"channel Video:"); return CHANNEL_VIDEO; break;
        case 0x1d0: Log_Printf(LOG_DMA_LEVEL,"channel M2R:"); return CHANNEL_M2R; break;
        case 0x1c0: Log_Printf(LOG_DMA_LEVEL,"channel R2M:"); return CHANNEL_R2M; break;
            
        default:
            Log_Printf(LOG_WARN, "Unknown DMA channel!\n");
            return -1;
            break;
    }
}

int get_interrupt_type(int channel) {
    switch (channel) {
        case CHANNEL_SCSI: return INT_SCSI_DMA; break;
        case CHANNEL_SOUNDOUT: return INT_SND_OUT_DMA; break;
        case CHANNEL_DISK: return INT_DISK_DMA; break;
        case CHANNEL_SOUNDIN: return INT_SND_IN_DMA; break;
        case CHANNEL_PRINTER: return INT_PRINTER_DMA; break;
        case CHANNEL_SCC: return INT_SCC_DMA; break;
        case CHANNEL_DSP: return INT_DSP_DMA; break;
        case CHANNEL_EN_TX: return INT_EN_TX_DMA; break;
        case CHANNEL_EN_RX: return INT_EN_RX_DMA; break;
        case CHANNEL_VIDEO: return INT_VIDEO; break;
        case CHANNEL_M2R: return INT_M2R_DMA; break;
        case CHANNEL_R2M: return INT_R2M_DMA; break;
                        
        default:
            Log_Printf(LOG_WARN, "Unknown DMA interrupt!\n");
            return 0;
            break;
    }
}

void DMA_CSR_Read(void) { // 0x02000010, length of register is byte on 68030 based NeXT Computer
    int channel = get_channel(IoAccessCurrentAddress);
    
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = dma[channel].csr;
    IoMem[(IoAccessCurrentAddress+1) & IO_SEG_MASK] = IoMem[(IoAccessCurrentAddress+2) & IO_SEG_MASK] = IoMem[(IoAccessCurrentAddress+3) & IO_SEG_MASK] = 0x00; // just to be sure
    Log_Printf(LOG_DMA_LEVEL,"DMA CSR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].csr, m68k_getpc());
}

void DMA_CSR_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress);
    int interrupt = get_interrupt_type(channel);
    Uint8 writecsr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK]|IoMem[(IoAccessCurrentAddress+1) & IO_SEG_MASK]|IoMem[(IoAccessCurrentAddress+2) & IO_SEG_MASK]|IoMem[(IoAccessCurrentAddress+3) & IO_SEG_MASK];

    Log_Printf(LOG_DMA_LEVEL,"DMA CSR write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, writecsr, m68k_getpc());
    
    /* For debugging */
    if(writecsr&DMA_DEV2M)
        Log_Printf(LOG_DMA_LEVEL,"DMA from dev to mem");
    else
        Log_Printf(LOG_DMA_LEVEL,"DMA from mem to dev");
    
    switch (writecsr&DMA_CMD_MASK) {
        case DMA_RESET:
            Log_Printf(LOG_DMA_LEVEL,"DMA reset"); break;
        case DMA_INITBUF:
            Log_Printf(LOG_DMA_LEVEL,"DMA initialize buffers"); break;
        case (DMA_RESET | DMA_INITBUF):
        case (DMA_RESET | DMA_INITBUF | DMA_CLRCOMPLETE):
            Log_Printf(LOG_DMA_LEVEL,"DMA reset and initialize buffers"); break;
        case DMA_CLRCOMPLETE:
            Log_Printf(LOG_DMA_LEVEL,"DMA end chaining"); break;
        case (DMA_SETSUPDATE | DMA_CLRCOMPLETE):
            Log_Printf(LOG_DMA_LEVEL,"DMA continue chaining"); break;
        case DMA_SETENABLE:
            Log_Printf(LOG_DMA_LEVEL,"DMA start single transfer"); break;
        case (DMA_SETENABLE | DMA_SETSUPDATE):
        case (DMA_SETENABLE | DMA_SETSUPDATE | DMA_CLRCOMPLETE):
            Log_Printf(LOG_DMA_LEVEL,"DMA start chaining"); break;
        case 0:
            Log_Printf(LOG_DMA_LEVEL,"DMA no command"); break;
        default:
            Log_Printf(LOG_DMA_LEVEL,"DMA: unknown command!"); break;
    }

    /* Handle CSR bits */
    dma[channel].direction = writecsr&DMA_DEV2M;

    if (writecsr&DMA_RESET) {
        dma[channel].csr &= ~(DMA_COMPLETE | DMA_SUPDATE | DMA_ENABLE);
    }
    if (writecsr&DMA_INITBUF) {
        dma_initialize_buffer(channel);
    }
    if (writecsr&DMA_SETSUPDATE) {
        dma[channel].csr |= DMA_SUPDATE;
    }
    if (writecsr&DMA_SETENABLE) {
        dma[channel].csr |= DMA_ENABLE;
        switch (channel) {
            case CHANNEL_M2R:
            case CHANNEL_R2M:
                if (dma[channel].next==dma[channel].limit) {
                    dma[channel].csr&= ~DMA_ENABLE;
                }
                if ((dma[CHANNEL_M2R].csr&DMA_ENABLE)&&(dma[CHANNEL_R2M].csr&DMA_ENABLE)) {
                    /* Enable Memory to Memory DMA, if read and write channels are enabled */
                    dma_m2m_write_memory();
                }
                break;
                
            default: break;
        }
    }
    if (writecsr&DMA_CLRCOMPLETE) {
        dma[channel].csr &= ~DMA_COMPLETE;
    }

    set_interrupt(interrupt, RELEASE_INT); // experimental
}

void DMA_Saved_Next_Read(void) { // 0x02004000
    int channel = get_channel(IoAccessCurrentAddress-0x3FF0);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_next);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SNext read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_next, m68k_getpc());
}

void DMA_Saved_Next_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FF0);
    dma[channel].saved_next = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SNext write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_next, m68k_getpc());
}

void DMA_Saved_Limit_Read(void) { // 0x02004004
    int channel = get_channel(IoAccessCurrentAddress-0x3FF4);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_limit);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SLimit read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_limit, m68k_getpc());
}

void DMA_Saved_Limit_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FF4);
    dma[channel].saved_limit = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SLimit write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_limit, m68k_getpc());
}

void DMA_Saved_Start_Read(void) { // 0x02004008
    int channel = get_channel(IoAccessCurrentAddress-0x3FF8);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_start);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SStart read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_start, m68k_getpc());
}

void DMA_Saved_Start_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FF8);
    dma[channel].saved_start = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SStart write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_start, m68k_getpc());
}

void DMA_Saved_Stop_Read(void) { // 0x0200400c
    int channel = get_channel(IoAccessCurrentAddress-0x3FFC);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].saved_stop);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SStop read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_stop, m68k_getpc());
}

void DMA_Saved_Stop_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x3FFC);
    dma[channel].saved_stop = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SStop write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].saved_stop, m68k_getpc());
}

void DMA_Next_Read(void) { // 0x02004010
    int channel = get_channel(IoAccessCurrentAddress-0x4000);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].next);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Next read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].next, m68k_getpc());
}

void DMA_Next_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4000);
    dma[channel].next = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA Next write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].next, m68k_getpc());
}

void DMA_Limit_Read(void) { // 0x02004014
    int channel = get_channel(IoAccessCurrentAddress-0x4004);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].limit);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Limit read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].limit, m68k_getpc());
}

void DMA_Limit_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4004);
    dma[channel].limit = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA Limit write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].limit, m68k_getpc());
}

void DMA_Start_Read(void) { // 0x02004018
    int channel = get_channel(IoAccessCurrentAddress-0x4008);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].start);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Start read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].start, m68k_getpc());
}

void DMA_Start_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4008);
    dma[channel].start = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA Start write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].start, m68k_getpc());
}

void DMA_Stop_Read(void) { // 0x0200401c
    int channel = get_channel(IoAccessCurrentAddress-0x400C);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].stop);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Stop read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].stop, m68k_getpc());
}

void DMA_Stop_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x400C);
    dma[channel].stop = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA Stop write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].stop, m68k_getpc());
}

void DMA_Init_Read(void) { // 0x02004210
    int channel = get_channel(IoAccessCurrentAddress-0x4200);
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma[channel].next);
 	Log_Printf(LOG_DMA_LEVEL,"DMA Init read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].next, m68k_getpc());
}

void DMA_Init_Write(void) {
    int channel = get_channel(IoAccessCurrentAddress-0x4200);
    dma[channel].next = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    dma_initialize_buffer(channel);
    Log_Printf(LOG_DMA_LEVEL,"DMA Init write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma[channel].next, m68k_getpc());
}

/* Initialize DMA internal buffer */

void dma_initialize_buffer(int channel) {
    switch (channel) {
        case CHANNEL_SCSI:
            esp_dma.status = 0x00; /* just a guess */
            espdma_buf_size = espdma_buf_limit = 0;
            break;
        case CHANNEL_DISK:
            modma_buf_size = modma_buf_limit = 0;
            break;
        case CHANNEL_EN_RX:
            enrxdma_buf_size = enrxdma_buf_limit = 0;
            break;
        default:
            break;
    }
}

/* DMA interrupt functions */

void dma_interrupt(int channel) {
    int interrupt = get_interrupt_type(channel);
    
    /* If we have reached limit, generate an interrupt and set the flags */
    if (dma[channel].next==dma[channel].limit) {
        
        dma[channel].csr |= DMA_COMPLETE;
        
        if (dma[channel].csr & DMA_SUPDATE) { /* if we are in chaining mode */
            dma[channel].next = dma[channel].start;
            dma[channel].limit = dma[channel].stop;
            /* Set bits in CSR */
            dma[channel].csr &= ~DMA_SUPDATE; /* 1st done */
        } else {
            dma[channel].csr &= ~DMA_ENABLE; /* all done */
        }
        set_interrupt(interrupt, SET_INT);
    } else if (dma[channel].csr&DMA_BUSEXC) {
        set_interrupt(interrupt, SET_INT);
    }
}

void dma_enet_interrupt(int channel) {
    int interrupt = get_interrupt_type(channel);
    
    /* If we have reached limit, generate an interrupt and set the flags */
    dma[channel].csr |= DMA_COMPLETE;
    
    if (dma[channel].csr & DMA_SUPDATE) { /* if we are in chaining mode */
        /* Save pointers */
        dma[channel].saved_limit = dma[channel].next;
        dma[channel].saved_next = dma[channel].start;   /* ? */
        dma[channel].saved_start = dma[channel].start;  /* ? */
        dma[channel].saved_stop = dma[channel].stop;    /* ? */
        /* Update pointers */
        dma[channel].next = dma[channel].start;
        dma[channel].limit = dma[channel].stop;
        /* Set bits in CSR */
        dma[channel].csr &= ~DMA_SUPDATE; /* 1st done */
    } else {
        dma[channel].csr &= ~DMA_ENABLE; /* all done */
    }
    set_interrupt(interrupt, SET_INT);
}

/* Functions for delayed interrupts */

/* Handler functions for DMA M2M delyed interrupts */
void M2RDMA_InterruptHandler(void) {
    CycInt_AcknowledgeInterrupt();
    dma_interrupt(CHANNEL_M2R);
}
void R2MDMA_InterruptHandler(void) {
    CycInt_AcknowledgeInterrupt();
    dma_interrupt(CHANNEL_R2M);
}


/* DMA Read and Write Memory Functions */

/* Channel SCSI */
void dma_esp_write_memory(void) {
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Write to memory at $%08x, %i bytes (ESP counter %i)",
               dma[CHANNEL_SCSI].next,dma[CHANNEL_SCSI].limit-dma[CHANNEL_SCSI].next,esp_counter);

    if (!(dma[CHANNEL_SCSI].csr&DMA_ENABLE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Error! DMA not enabled!");
        return;
    }
    if ((dma[CHANNEL_SCSI].limit%DMA_BURST_SIZE) || (dma[CHANNEL_SCSI].next%4)) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Error! Bad alignment! (Next: $%08X, Limit: $%08X)",
                   dma[CHANNEL_SCSI].next, dma[CHANNEL_SCSI].limit);
        abort();
    }

    /* TODO: Find out how we should handle non burst-size aligned start address. 
     * End address is always burst-size aligned. For now we use a hack. */
    
    TRY(prb) {
        if (espdma_buf_size>0) {
            Log_Printf(LOG_WARN, "[DMA] Channel SCSI: %i residual bytes in DMA buffer.", espdma_buf_size);
            while (espdma_buf_size>0) {
                NEXTMemory_WriteLong(dma[CHANNEL_SCSI].next, dma_getlong(espdma_buf, espdma_buf_limit-espdma_buf_size));
                dma[CHANNEL_SCSI].next+=4;
                espdma_buf_size-=4;
            }
        }

        /* This is a hack to handle non-burstsize-aligned DMA start */
        if (dma[CHANNEL_SCSI].next%DMA_BURST_SIZE) {
            Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Start memory address is not 16 byte aligned ($%08X).",
                       dma[CHANNEL_SCSI].next);
            while ((dma[CHANNEL_SCSI].next+espdma_buf_size)%DMA_BURST_SIZE && esp_counter>0 && SCSIbus.phase==PHASE_DI) {
                espdma_buf[espdma_buf_size]=SCSIdisk_Send_Data();
                esp_counter--;
                espdma_buf_size++;
            }
            espdma_buf_limit=espdma_buf_size;
            while (espdma_buf_size>0) {
                NEXTMemory_WriteLong(dma[CHANNEL_SCSI].next, dma_getlong(espdma_buf, espdma_buf_limit-espdma_buf_size));
                dma[CHANNEL_SCSI].next+=4;
                espdma_buf_size-=4;
            }
        }

        while (dma[CHANNEL_SCSI].next<=dma[CHANNEL_SCSI].limit && !(espdma_buf_size%DMA_BURST_SIZE)) {
            /* Fill DMA internal buffer */
            while (espdma_buf_size<DMA_BURST_SIZE && esp_counter>0 && SCSIbus.phase==PHASE_DI) {
                espdma_buf[espdma_buf_size]=SCSIdisk_Send_Data();
                esp_counter--;
                espdma_buf_size++;
            }
            ESP_DMA_set_status();
            
            /* If buffer is full, burst write to memory */
            if (espdma_buf_size==DMA_BURST_SIZE && dma[CHANNEL_SCSI].next<dma[CHANNEL_SCSI].limit) {
                while (espdma_buf_size>0) {
                    NEXTMemory_WriteLong(dma[CHANNEL_SCSI].next, dma_getlong(espdma_buf, DMA_BURST_SIZE-espdma_buf_size));
                    dma[CHANNEL_SCSI].next+=4;
                    espdma_buf_size-=4;
                }
            } else { /* else do not write the bytes to memory but keep them inside the buffer */ 
                Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Residual bytes in DMA buffer: %i bytes",espdma_buf_size);
                espdma_buf_limit=espdma_buf_size;
                break;
            }
        }
    } CATCH(prb) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Bus error while writing to %08x",dma[CHANNEL_SCSI].next);
        dma[CHANNEL_SCSI].csr &= ~DMA_ENABLE;
        dma[CHANNEL_SCSI].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
    
    dma_interrupt(CHANNEL_SCSI);
}

void dma_esp_flush_buffer(void) {

    TRY(prb) {
        if (dma[CHANNEL_SCSI].next<dma[CHANNEL_SCSI].limit && espdma_buf_size>0) {
            Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Flush buffer to memory at $%08x, 4 bytes",dma[CHANNEL_SCSI].next);

            /* Write one long word to memory */
            NEXTMemory_WriteLong(dma[CHANNEL_SCSI].next, dma_getlong(espdma_buf, espdma_buf_limit-espdma_buf_size));
            dma[CHANNEL_SCSI].next+=4;
            espdma_buf_size-=4;
        } else if (espdma_buf_size==0) {
            Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Not flushing buffer (empty)");
        }
    } CATCH(prb) {
        dma[CHANNEL_SCSI].csr &= ~DMA_ENABLE;
        dma[CHANNEL_SCSI].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
}

void dma_esp_read_memory(void) {    
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Read from memory at $%08x, %i bytes (ESP counter %i)",
               dma[CHANNEL_SCSI].next,dma[CHANNEL_SCSI].limit-dma[CHANNEL_SCSI].next,esp_counter);
    
    if (!(dma[CHANNEL_SCSI].csr&DMA_ENABLE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Error! DMA not enabled!");
        return;
    }
    if ((dma[CHANNEL_SCSI].limit%DMA_BURST_SIZE) || (dma[CHANNEL_SCSI].next%4)) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Error! Bad alignment! (Next: $%08X, Limit: $%08X)",
                   dma[CHANNEL_SCSI].next, dma[CHANNEL_SCSI].limit);
        abort();
    }
    
    /* TODO: Find out how we should handle non burst-size aligned start address.
     * End address should be always burst-size aligned. For now we use a hack. */
    
    TRY(prb) {
        if (espdma_buf_size>0) {
            Log_Printf(LOG_WARN, "[DMA] Channel SCSI: %i residual bytes in DMA buffer.", espdma_buf_size);
            while (espdma_buf_size>0) {
                SCSIdisk_Receive_Data(espdma_buf[espdma_buf_limit-espdma_buf_size]);
                esp_counter--;
                espdma_buf_size--;
            }
        }

        /* This is a hack to handle non-burstsize-aligned DMA start */
        if (dma[CHANNEL_SCSI].next%DMA_BURST_SIZE) {
            Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Start memory address is not 16 byte aligned ($%08X).",
                       dma[CHANNEL_SCSI].next);
            while (dma[CHANNEL_SCSI].next%DMA_BURST_SIZE) {
                dma_putlong(NEXTMemory_ReadLong(dma[CHANNEL_SCSI].next), espdma_buf, espdma_buf_size);
                dma[CHANNEL_SCSI].next+=4;
                espdma_buf_size+=4;
            }
            espdma_buf_limit=espdma_buf_size;
            while (espdma_buf_size>0 && esp_counter>0 && SCSIbus.phase==PHASE_DO) {
                SCSIdisk_Receive_Data(espdma_buf[espdma_buf_limit-espdma_buf_size]);
                esp_counter--;
                espdma_buf_size--;
            }
        }

        while (dma[CHANNEL_SCSI].next<dma[CHANNEL_SCSI].limit && espdma_buf_size==0) {
            /* Read data from memory to internal DMA buffer */
            while (espdma_buf_size<DMA_BURST_SIZE) {
                dma_putlong(NEXTMemory_ReadLong(dma[CHANNEL_SCSI].next), espdma_buf, espdma_buf_size);
                dma[CHANNEL_SCSI].next+=4;
                espdma_buf_size+=4;
            }
            /* Empty DMA internal buffer */
            while (espdma_buf_size>0 && esp_counter>0 && SCSIbus.phase==PHASE_DO) {
                SCSIdisk_Receive_Data(espdma_buf[DMA_BURST_SIZE-espdma_buf_size]);
                esp_counter--;
                espdma_buf_size--;
            }
            ESP_DMA_set_status();
        }
    } CATCH(prb) {
        Log_Printf(LOG_WARN, "[DMA] Channel SCSI: Bus error while writing to %08x",dma[CHANNEL_SCSI].next+espdma_buf_size);
        dma[CHANNEL_SCSI].csr &= ~DMA_ENABLE;
        dma[CHANNEL_SCSI].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
    
    if (espdma_buf_size!=0) {
        espdma_buf_limit=espdma_buf_size;
        Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Residual bytes in DMA buffer: %i bytes",espdma_buf_size);
    }
    if (SCSIbus.phase==PHASE_DO) {
        Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCSI: Warning! Data not yet written to disk.");
        if (espdma_buf_size!=0) {
            Log_Printf(LOG_WARN, "[DMA] Channel SCSI: WARNING: Loss of data in DMA buffer possible!");
        }
    }
    
    dma_interrupt(CHANNEL_SCSI);
}


/* Channel MO */
void dma_mo_write_memory(void) {
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel MO: Write to memory at $%08x, %i bytes",
               dma[CHANNEL_DISK].next,dma[CHANNEL_DISK].limit-dma[CHANNEL_DISK].next);
    
    if (!(dma[CHANNEL_DISK].csr&DMA_ENABLE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Error! DMA not enabled!");
        return;
    }
    if ((dma[CHANNEL_DISK].limit%DMA_BURST_SIZE) || (dma[CHANNEL_DISK].next%4)) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Error! Bad alignment! (Next: $%08X, Limit: $%08X)",
                   dma[CHANNEL_DISK].next, dma[CHANNEL_DISK].limit);
        abort();
    }
    /* TODO: Find out how we should handle non burst-size aligned start address.
     * End address is always burst-size aligned. For now we use a hack. */
    
    TRY(prb) {
        if (modma_buf_size>0) {
            Log_Printf(LOG_WARN, "[DMA] Channel MO: %i residual bytes in DMA buffer.", modma_buf_size);
            while (modma_buf_size>=4) {
                NEXTMemory_WriteLong(dma[CHANNEL_DISK].next, dma_getlong(modma_buf, modma_buf_limit-modma_buf_size));
                dma[CHANNEL_DISK].next+=4;
                modma_buf_size-=4;
            }
        }
        
        /* This is a hack to handle non-burstsize-aligned DMA start */
        if (dma[CHANNEL_DISK].next%DMA_BURST_SIZE) {
            Log_Printf(LOG_WARN, "[DMA] Channel MO: Start memory address is not 16 byte aligned ($%08X).",
                       dma[CHANNEL_DISK].next);
            while ((dma[CHANNEL_DISK].next+modma_buf_size)%DMA_BURST_SIZE && ecc_buffer[eccout].size>0) {
                modma_buf[modma_buf_size]=ecc_buffer[eccout].data[ecc_buffer[eccout].limit-ecc_buffer[eccout].size];
                ecc_buffer[eccout].size--;
                modma_buf_size++;
            }
            modma_buf_limit=modma_buf_size;
            while (modma_buf_size>=4) {
                NEXTMemory_WriteLong(dma[CHANNEL_DISK].next, dma_getlong(modma_buf, modma_buf_limit-modma_buf_size));
                dma[CHANNEL_DISK].next+=4;
                modma_buf_size-=4;
            }
        }
        
        while (dma[CHANNEL_DISK].next<=dma[CHANNEL_DISK].limit && !(modma_buf_size%DMA_BURST_SIZE)) {
            /* Fill DMA internal buffer */
            while (modma_buf_size<DMA_BURST_SIZE && ecc_buffer[eccout].size>0) {
                modma_buf[modma_buf_size]=ecc_buffer[eccout].data[ecc_buffer[eccout].limit-ecc_buffer[eccout].size];
                ecc_buffer[eccout].size--;
                modma_buf_size++;
            }
                        
            /* If buffer is full, burst write to memory */
            if (modma_buf_size==DMA_BURST_SIZE && dma[CHANNEL_DISK].next<dma[CHANNEL_DISK].limit) {
                while (modma_buf_size>0) {
                    NEXTMemory_WriteLong(dma[CHANNEL_DISK].next, dma_getlong(modma_buf, DMA_BURST_SIZE-modma_buf_size));
                    dma[CHANNEL_DISK].next+=4;
                    modma_buf_size-=4;
                }
            } else { /* else do not write the bytes to memory but keep them inside the buffer */
                Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel MO: Residual bytes in DMA buffer: %i bytes",modma_buf_size);
                modma_buf_limit=modma_buf_size;
                break;
            }
        }
    } CATCH(prb) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Bus error while writing to %08x",dma[CHANNEL_DISK].next);
        dma[CHANNEL_DISK].csr &= ~DMA_ENABLE;
        dma[CHANNEL_DISK].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
    
    dma_interrupt(CHANNEL_DISK);
}

void dma_mo_flush_buffer(void) {
    if (dma[CHANNEL_DISK].next<dma[CHANNEL_DISK].limit && modma_buf_size==DMA_BURST_SIZE) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Flush buffer to memory at $%08x",dma[CHANNEL_DISK].next);
        while (modma_buf_size>0) {
            NEXTMemory_WriteLong(dma[CHANNEL_DISK].next, dma_getlong(modma_buf, DMA_BURST_SIZE-modma_buf_size));
            dma[CHANNEL_DISK].next+=4;
            modma_buf_size-=4;
        }
    }

    dma_interrupt(CHANNEL_DISK);
}

void dma_mo_read_memory(void) {
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel MO: Read from memory at $%08x, %i bytes",
               dma[CHANNEL_DISK].next,dma[CHANNEL_DISK].limit-dma[CHANNEL_DISK].next);
    
    if (!(dma[CHANNEL_DISK].csr&DMA_ENABLE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Error! DMA not enabled!");
        return;
    }
    if ((dma[CHANNEL_DISK].limit%DMA_BURST_SIZE) || (dma[CHANNEL_DISK].next%4)) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Error! Bad alignment! (Next: $%08X, Limit: $%08X)",
                   dma[CHANNEL_DISK].next, dma[CHANNEL_DISK].limit);
        abort();
    }
    
    /* TODO: Find out how we should handle non burst-size aligned start address.
     * End address should be always burst-size aligned. For now we use a hack. */
    
    TRY(prb) {
        if (modma_buf_size>0) {
            Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel MO: %i residual bytes in DMA buffer.", modma_buf_size);
            while (modma_buf_size>0) {
                ecc_buffer[eccin].data[ecc_buffer[eccin].size]=modma_buf[modma_buf_limit-modma_buf_size];
                ecc_buffer[eccin].size++;
                modma_buf_size--;
            }
        }

        /* This is a hack to handle non-burstsize-aligned DMA start */
        if (dma[CHANNEL_DISK].next%DMA_BURST_SIZE) {
            Log_Printf(LOG_WARN, "[DMA] Channel MO: Start memory address is not 16 byte aligned ($%08X).",
                       dma[CHANNEL_DISK].next);
            while (dma[CHANNEL_DISK].next%DMA_BURST_SIZE && modma_buf_size<DMA_BURST_SIZE) {
                dma_putlong(NEXTMemory_ReadLong(dma[CHANNEL_DISK].next), modma_buf, modma_buf_size);
                dma[CHANNEL_DISK].next+=4;
                modma_buf_size+=4;
            }
            modma_buf_limit=modma_buf_size;
            while (modma_buf_size>0) {
                ecc_buffer[eccin].data[ecc_buffer[eccin].size]=modma_buf[modma_buf_limit-modma_buf_size];
                ecc_buffer[eccin].size++;
                modma_buf_size--;
            }
        }

        while (dma[CHANNEL_DISK].next<dma[CHANNEL_DISK].limit && modma_buf_size==0) {
            /* Read data from memory to internal DMA buffer */
            while (modma_buf_size<DMA_BURST_SIZE) {
                dma_putlong(NEXTMemory_ReadLong(dma[CHANNEL_DISK].next), modma_buf, modma_buf_size);
                dma[CHANNEL_DISK].next+=4;
                modma_buf_size+=4;
            }
            /* Empty DMA internal buffer */
            while (modma_buf_size>0 && ecc_buffer[eccin].size<ecc_buffer[eccin].limit) {
                ecc_buffer[eccin].data[ecc_buffer[eccin].size]=modma_buf[DMA_BURST_SIZE-modma_buf_size];
                ecc_buffer[eccin].size++;
                modma_buf_size--;
            }
        }
    } CATCH(prb) {
        Log_Printf(LOG_WARN, "[DMA] Channel MO: Bus error while writing to %08x",dma[CHANNEL_DISK].next);
        dma[CHANNEL_DISK].csr &= ~DMA_ENABLE;
        dma[CHANNEL_DISK].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
    
    if (modma_buf_size!=0) {
        Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel MO: Residual bytes in DMA buffer: %i bytes",modma_buf_size);
        modma_buf_limit=modma_buf_size;
    }

    dma_interrupt(CHANNEL_DISK);
}


/* Channel Ethernet */
#define EN_EOP      0x80000000 /* end of packet */
#define EN_BOP      0x40000000 /* beginning of packet */
#define ENADDR(x)   ((x)&~(EN_EOP|EN_BOP))

void dma_enet_write_memory(void) {
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel Ethernet Receive: Write to memory at $%08x, %i bytes",
               dma[CHANNEL_EN_RX].next,dma[CHANNEL_EN_RX].limit-dma[CHANNEL_EN_RX].next);
    
    if (!(dma[CHANNEL_EN_RX].csr&DMA_ENABLE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel Ethernet Receive: Error! DMA not enabled!");
        return;
    }
    if ((dma[CHANNEL_EN_RX].limit%DMA_BURST_SIZE) || (dma[CHANNEL_EN_RX].next%DMA_BURST_SIZE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel Ethernet Receive: Error! Bad alignment! (Next: $%08X, Limit: $%08X)",
                   dma[CHANNEL_EN_RX].next, dma[CHANNEL_EN_RX].limit);
        abort();
    }
    /* TODO: Find out how we should handle non burst-size aligned start address.
     * End address is always burst-size aligned. For now we use a hack. */

    TRY(prb) {
        if (enrxdma_buf_size>0) {
            Log_Printf(LOG_WARN, "[DMA] Channel Ethernet Receive: %i residual bytes in DMA buffer.", enrxdma_buf_size);
#if 1 /* FIXME: How to process residual bytes for ethernet channel? */
            while (enrxdma_buf_size>0) {
                NEXTMemory_WriteByte(dma[CHANNEL_EN_RX].next, enrxdma_buf[enrxdma_buf_limit-enrxdma_buf_size]);
                dma[CHANNEL_EN_RX].next++;
                enrxdma_buf_size--;
            }
#else
            Log_Printf(LOG_WARN, "[DMA] Channel Ethernet Receive: Discarding in DMA buffer!");
            enrxdma_buf_size=enrxdma_buf_limit=0;
#endif
        }

        while (dma[CHANNEL_EN_RX].next</*=*/dma[CHANNEL_EN_RX].limit && !(enrxdma_buf_size%DMA_BURST_SIZE)) {
            /* Fill DMA internal buffer */
            while (enrxdma_buf_size<DMA_BURST_SIZE && enet_rx_buffer.size>0) {
                enrxdma_buf[enrxdma_buf_size]=enet_rx_buffer.data[enet_rx_buffer.limit-enet_rx_buffer.size];
                enet_rx_buffer.size--;
                enrxdma_buf_size++;
            }
            
            /* If buffer is full, burst write to memory */
            if (enrxdma_buf_size==DMA_BURST_SIZE && dma[CHANNEL_EN_RX].next<dma[CHANNEL_EN_RX].limit) {
                while (enrxdma_buf_size>0) {
                    NEXTMemory_WriteLong(dma[CHANNEL_EN_RX].next, dma_getlong(enrxdma_buf, DMA_BURST_SIZE-enrxdma_buf_size));
                    dma[CHANNEL_EN_RX].next+=4;
                    enrxdma_buf_size-=4;
                }
            } else { /* else do not write the bytes to memory but keep them inside the buffer */
#if 1
                Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel Ethernet Receive: Auto-flushing residual bytes in DMA buffer: %i bytes",enrxdma_buf_size);
                enrxdma_buf_limit=enrxdma_buf_size;
                while (enrxdma_buf_size>0 && dma[CHANNEL_EN_RX].next<dma[CHANNEL_EN_RX].limit) {
                    NEXTMemory_WriteByte(dma[CHANNEL_EN_RX].next, enrxdma_buf[enrxdma_buf_limit-enrxdma_buf_size]);
                    dma[CHANNEL_EN_RX].next++;
                    enrxdma_buf_size--;
                }
                enrxdma_buf_limit=enrxdma_buf_size;
#endif
                break;
            }
        }
    } CATCH(prb) {
        Log_Printf(LOG_WARN, "[DMA] Channel Ethernet Receive: Bus error while writing to %08x",dma[CHANNEL_EN_RX].next);
        dma[CHANNEL_EN_RX].csr &= ~DMA_ENABLE;
        dma[CHANNEL_EN_RX].csr |= (DMA_COMPLETE|DMA_BUSEXC);
    } ENDTRY
    
    /* FIXME: DMA controller sets EN_BOP flag in next register on last byte 
     * of packet under certain conditions when chaining */

    dma_enet_interrupt(CHANNEL_EN_RX);
}

void dma_enet_read_memory(void) { /* This channel does not use DMA buffering */
    if (dma[CHANNEL_EN_TX].csr&DMA_ENABLE) {
        Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel Ethernet Transmit: Read from memory at $%08x, %i bytes",
                   dma[CHANNEL_EN_TX].next,ENADDR(dma[CHANNEL_EN_TX].limit)-dma[CHANNEL_EN_TX].next);
        
        TRY(prb) {
            while (dma[CHANNEL_EN_TX].next<ENADDR(dma[CHANNEL_EN_TX].limit) && enet_tx_buffer.size<enet_tx_buffer.limit) {
                enet_tx_buffer.data[enet_tx_buffer.size]=NEXTMemory_ReadByte(dma[CHANNEL_EN_TX].next);
                enet_tx_buffer.size++;
                dma[CHANNEL_EN_TX].next++;
            }
        } CATCH(prb) {
            Log_Printf(LOG_WARN, "[DMA] Channel Ethernet Transmit: Bus error while writing to %08x",dma[CHANNEL_EN_TX].next);
            dma[CHANNEL_EN_TX].csr &= ~DMA_ENABLE;
            dma[CHANNEL_EN_TX].csr |= (DMA_COMPLETE|DMA_BUSEXC);
        } ENDTRY
        
        dma_enet_interrupt(CHANNEL_EN_TX);
    }
}


/* Memory to Memory */
#define DMA_M2M_CYCLES    1//((DMA_BURST_SIZE * 3) / 4)

void dma_m2m_write_memory(void) {
    int i;
    int time = 0;
    Uint32 m2m_buffer[DMA_BURST_SIZE/4];
    
    if (((dma[CHANNEL_R2M].limit-dma[CHANNEL_R2M].next)%DMA_BURST_SIZE) ||
        ((dma[CHANNEL_M2R].limit-dma[CHANNEL_M2R].next)%DMA_BURST_SIZE)) {
        Log_Printf(LOG_WARN, "[DMA] Channel M2M: Error! Memory not burst size aligned!");
    }
    
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel M2M: Copying %i bytes from $%08X to $%08X.",
               dma[CHANNEL_R2M].limit-dma[CHANNEL_R2M].next,dma[CHANNEL_M2R].next,dma[CHANNEL_R2M].next);
    
    while (dma[CHANNEL_R2M].next<dma[CHANNEL_R2M].limit) {
        time+=DMA_M2M_CYCLES;

        if (dma[CHANNEL_M2R].next<dma[CHANNEL_M2R].limit) {
            TRY(prb) {
                /* (Re)fill the buffer, if there is still data to read */
                for (i=0; i<DMA_BURST_SIZE; i+=4) {
                    m2m_buffer[i/4]=NEXTMemory_ReadLong(dma[CHANNEL_M2R].next+i);
                }
                dma[CHANNEL_M2R].next+=DMA_BURST_SIZE;
            } CATCH(prb) {
                Log_Printf(LOG_WARN, "[DMA] Channel M2M: Bus error while reading from %08x",dma[CHANNEL_M2R].next+i);
                dma[CHANNEL_M2R].csr &= ~DMA_ENABLE;
                dma[CHANNEL_M2R].csr |= (DMA_COMPLETE|DMA_BUSEXC);
            } ENDTRY
            
            if ((dma[CHANNEL_M2R].next==dma[CHANNEL_M2R].limit)||(dma[CHANNEL_M2R].csr&DMA_BUSEXC)) {
                CycInt_AddRelativeInterrupt(time/4, INT_CPU_CYCLE, INTERRUPT_M2R);
            }
        }
        
        TRY(prb) {
            /* Write the contents of the buffer to memory */
            for (i=0; i<DMA_BURST_SIZE; i+=4) {
                NEXTMemory_WriteLong(dma[CHANNEL_R2M].next+i, m2m_buffer[i/4]);
            }
            dma[CHANNEL_R2M].next+=DMA_BURST_SIZE;
        } CATCH(prb) {
            Log_Printf(LOG_WARN, "[DMA] Channel M2M: Bus error while writing to %08x",dma[CHANNEL_R2M].next+i);
            dma[CHANNEL_R2M].csr &= ~DMA_ENABLE;
            dma[CHANNEL_R2M].csr |= (DMA_COMPLETE|DMA_BUSEXC);
        } ENDTRY
    }
    CycInt_AddRelativeInterrupt(time/4, INT_CPU_CYCLE, INTERRUPT_R2M);
}



/* ---------------------- DMA Scratchpad ---------------------- */

/* This is used to interrupt at vertical screen retrace.
 * TODO: find out how the interrupt is generated in real
 * hardware using the Limit register of the DMA chip.
 * (0xEA * 1024 = visible videomem size)
 */


/* Interrupt Handler (called from Video_InterruptHandler in video.c) */
void dma_video_interrupt(void) {
    if (dma[CHANNEL_VIDEO].limit==0xEA) {
        set_interrupt(INT_VIDEO, SET_INT); /* interrupt is released by writing to CSR */
    } else if (dma[CHANNEL_VIDEO].limit && dma[CHANNEL_VIDEO].limit!=0xEA) {
        abort();
    }
}


/* FIXME: This is just for passing power-on test. Add real SCC and Sound channels later. */

void dma_scc_read_memory(void) {
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel SCC: Read from memory at $%08x, %i bytes",
               dma[CHANNEL_SCC].next,dma[CHANNEL_SCC].limit-dma[CHANNEL_SCC].next);
    while (dma[CHANNEL_SCC].next<dma[CHANNEL_SCC].limit) {
        scc_buf[0]=NEXTMemory_ReadByte(dma[CHANNEL_SCC].next);
        dma[CHANNEL_SCC].next++;
    }
    
    dma_interrupt(CHANNEL_SCC);
}

void dma_sndout_read_memory(void) {
    Log_Printf(LOG_DMA_LEVEL, "[DMA] Channel Sound out: Read from memory at $%08x, %i bytes",
               dma[CHANNEL_SOUNDOUT].next,dma[CHANNEL_SOUNDOUT].limit-dma[CHANNEL_SOUNDOUT].next);
    while (dma[CHANNEL_SOUNDOUT].next<dma[CHANNEL_SOUNDOUT].limit) {
        NEXTMemory_ReadByte(dma[CHANNEL_SOUNDOUT].next); /* for now just discard data */
        dma[CHANNEL_SOUNDOUT].next++;
    }
    
    dma_interrupt(CHANNEL_SOUNDOUT);
}
