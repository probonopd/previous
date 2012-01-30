/* NeXT DMA Emulation 
 * Contains informations from QEMU-NeXT
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "esp.h"
#include "sysReg.h"
#include "dma.h"
#include "configuration.h"


#define LOG_DMA_LEVEL LOG_WARN

#define IO_SEG_MASK	0x1FFFF

/* read CSR bits */
#define DMA_ENABLE      0x01000000 /* enable dma transfer */
#define DMA_SUPDATE     0x02000000 /* single update */
#define DMA_COMPLETE    0x08000000 /* current dma has completed */
#define DMA_BUSEXC      0x10000000 /* bus exception occurred */
/* write CSR bits */
#define DMA_SETENABLE   0x00010000 /* set enable */
#define DMA_SETSUPDATE  0x00020000 /* set single update */
#define DMA_M2DEV       0x00000000 /* dma from mem to dev */
#define DMA_DEV2M       0x00040000 /* dma from dev to mem */
#define DMA_CLRCOMPLETE 0x00080000 /* clear complete conditional */
#define DMA_RESET       0x00100000 /* clr cmplt, sup, enable */
#define DMA_INITBUF     0x00200000 /* initialize DMA buffers */


/* read and write CSR bits for 68030 based systems (we convert these to 68040 values before using in functions):
 read:
 #define DMA_ENABLE      0x01
 #define DMA_SUPDATE     0x02
 #define DMA_COMPLETE    0x08
 #define DMA_BUSEXC      0x10
 write:
 #define DMA_SETENABLE   0x01
 #define DMA_SETSUPDATE  0x02
 #define DMA_M2DEV       0x00
 #define DMA_DEV2M       0x04
 #define DMA_CLRCOMPLETE 0x08
 #define DMA_RESET       0x10
 #define DMA_INITBUF     0x20
 */



/* Address variables */
Uint32 read_dma_scsi_csr=0;
Uint32 write_dma_scsi_csr=0;
Uint32 dma_saved_next;
Uint32 dma_saved_limit;
Uint32 dma_saved_start;
Uint32 dma_saved_stop;
Uint32 dma_next;
Uint32 dma_limit;
Uint32 dma_start;
Uint32 dma_stop;
Uint32 dma_init;
Uint32 dma_size;


void DMA_SCSI_CSR_Read(void) { // 0x02000010, lengh of register is byte on 68030 ?
    if(ConfigureParams.System.nCpuLevel == 3) { // for 68030, not sure if this is correct
        IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = read_dma_scsi_csr >> 24;
        Log_Printf(LOG_DMA_LEVEL,"DMA SCSI CSR read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, read_dma_scsi_csr >> 24, m68k_getpc());
    } else {
        IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, read_dma_scsi_csr);
        Log_Printf(LOG_DMA_LEVEL,"DMA SCSI CSR read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, read_dma_scsi_csr, m68k_getpc());
    }
}

void DMA_SCSI_CSR_Write(void) {
    if(ConfigureParams.System.nCpuLevel == 3) { // for 68030, not sure if this is correct
        write_dma_scsi_csr = IoMem[IoAccessCurrentAddress & IO_SEG_MASK] << 16;
        Log_Printf(LOG_DMA_LEVEL,"DMA SCSI CSR write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress,write_dma_scsi_csr >> 16, m68k_getpc());
    } else {
        write_dma_scsi_csr = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
        Log_Printf(LOG_DMA_LEVEL,"DMA SCSI CSR write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, write_dma_scsi_csr, m68k_getpc());
    }
     
    if(write_dma_scsi_csr & DMA_DEV2M) {
        if(ConfigureParams.System.nCpuLevel == 3) {
            read_dma_scsi_csr |= (0x04 << 24); // use old DMA_DEV2M value for 68030, not sure if this is correct
        } else {
            read_dma_scsi_csr |= DMA_DEV2M;
        }
        Log_Printf(LOG_DMA_LEVEL,"DMA from dev to mem");
    }
    if(write_dma_scsi_csr & DMA_SETENABLE) {
        read_dma_scsi_csr |= DMA_ENABLE;
        Log_Printf(LOG_DMA_LEVEL,"DMA enable transfer");
    }
    if(write_dma_scsi_csr & DMA_SETSUPDATE) {
        read_dma_scsi_csr |= DMA_SUPDATE;
        Log_Printf(LOG_DMA_LEVEL,"DMA set single update");
    }
    if(write_dma_scsi_csr & DMA_CLRCOMPLETE) {
        read_dma_scsi_csr &= ~DMA_COMPLETE;
        Log_Printf(LOG_DMA_LEVEL,"DMA clear complete conditional");

	set_interrupt(INT_SCSI_DMA, RELEASE_INT); // also somewhat experimental...
    }
    if(write_dma_scsi_csr & DMA_RESET) {
        read_dma_scsi_csr &= ~(DMA_COMPLETE | DMA_SUPDATE | DMA_ENABLE | DMA_DEV2M);
        Log_Printf(LOG_WARN,"DMA reset");
	set_interrupt(INT_SCSI_DMA, RELEASE_INT); // also somewhat experimental...
    }
    if(write_dma_scsi_csr & DMA_INITBUF) { // needs to be filled
        Log_Printf(LOG_DMA_LEVEL,"DMA initialize buffers");
    }
}

void DMA_SCSI_Saved_Next_Read(void) { // 0x02004000
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_saved_next);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SNext read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_saved_next, m68k_getpc());
}

void DMA_SCSI_Saved_Next_Write(void) {
    dma_saved_next = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SNext write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_saved_next, m68k_getpc());
}

void DMA_SCSI_Saved_Limit_Read(void) { // 0x02004004
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_saved_limit);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SLimit read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_saved_limit, m68k_getpc());
}

void DMA_SCSI_Saved_Limit_Write(void) {
    dma_saved_limit = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SLimit write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_saved_limit, m68k_getpc());
}

void DMA_SCSI_Saved_Start_Read(void) { // 0x02004008
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_saved_start);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SStart read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_saved_start, m68k_getpc());
}

void DMA_SCSI_Saved_Start_Write(void) {
    dma_saved_start = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SStart write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_saved_start, m68k_getpc());
}

void DMA_SCSI_Saved_Stop_Read(void) { // 0x0200400c
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_saved_stop);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SStop read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_saved_stop, m68k_getpc());
}

void DMA_SCSI_Saved_Stop_Write(void) {
    dma_saved_stop = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI SStop write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_saved_stop, m68k_getpc());
}

void DMA_SCSI_Next_Read(void) { // 0x02004010
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_next);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Next read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_next, m68k_getpc());
}

void DMA_SCSI_Next_Write(void) {
    dma_next = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Next write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_next, m68k_getpc());
}

void DMA_SCSI_Limit_Read(void) { // 0x02004014
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_limit);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Limit read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_limit, m68k_getpc());
}

void DMA_SCSI_Limit_Write(void) {
    dma_limit = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Limit write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_limit, m68k_getpc());
}

void DMA_SCSI_Start_Read(void) { // 0x02004018
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_start);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Start read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_start, m68k_getpc());
}

void DMA_SCSI_Start_Write(void) {
    dma_start = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Start write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_start, m68k_getpc());
}

void DMA_SCSI_Stop_Read(void) { // 0x0200401c
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_stop);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Stop read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_stop, m68k_getpc());
}

void DMA_SCSI_Stop_Write(void) {
    dma_stop = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Stop write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_stop, m68k_getpc());
}

void DMA_SCSI_Init_Read(void) { // 0x02004210
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_init);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Init read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_init, m68k_getpc());
}

void DMA_SCSI_Init_Write(void) {
    dma_init = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Init write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_init, m68k_getpc());
}

void DMA_SCSI_Size_Read(void) { // 0x02004214
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, dma_size);
 	Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Size read at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_size, m68k_getpc());
}

void DMA_SCSI_Size_Write(void) {
    dma_size = IoMem_ReadLong(IoAccessCurrentAddress & IO_SEG_MASK);
    Log_Printf(LOG_DMA_LEVEL,"DMA SCSI Size write at $%08x val=$%08x PC=$%08x\n", IoAccessCurrentAddress, dma_size, m68k_getpc());
}



/* DMA Functions */

/*void copy_to_scsidma_buffer(Uint8 device_outbuf[], int outbuf_size) {
    memcpy(dma_buffer, device_outbuf, outbuf_size);
}*/

void dma_clear_memory(Uint32 datalength) {
    Uint32 start_addr;
    Uint32 end_addr;
    
    if(dma_init == 0)
        start_addr = dma_next;
    else
        start_addr = dma_init;
    
    end_addr = start_addr + datalength;
    
    NEXTMemory_Clear(start_addr, end_addr);
}

void dma_memory_read(Uint32 datalength) {
    Uint32 base_addr;
    Uint32 lencount;
    
    if(dma_init == 0)
        base_addr = dma_next;
    else
        base_addr = dma_init;
    
    for (lencount = 0; lencount < datalength; lencount++) {
        dma_read_buffer[lencount] = NEXTMemory_ReadByte(base_addr + lencount);
    }
}


void dma_memory_write(Uint8 *buf, int size, int dma_channel) {
    Uint32 base_addr;
    Uint8 align = 16;
    int size_count = 0;
    Uint32 write_addr;
    
    if(dma_channel == NEXTDMA_ENRX || dma_channel == NEXTDMA_ENTX)
        align = 32;
        
    if((size % align) != 0) {
        size -= size % align;
        size += align;
    }

    
    if(dma_init == 0)
        base_addr = dma_next;
    else
        base_addr = dma_init;
    
    for (size_count = 0; size_count < size; size_count++) {
        write_addr = base_addr + size_count;
        NEXTMemory_WriteByte(write_addr, buf[size_count]);
    }
    
    /* Test read/write */
//    Log_Printf(LOG_DMA_LEVEL, "DMA Write Test: $%02x,$%02x,$%02x,$%02x\n", NEXTMemory_ReadByte(base_addr),NEXTMemory_ReadByte(base_addr+16),NEXTMemory_ReadByte(base_addr+32),NEXTMemory_ReadByte(base_addr+384));
//    NEXTMemory_WriteByte(base_addr, 0x77);
//    Uint8 testvar = NEXTMemory_ReadByte(base_addr);
//    Log_Printf(LOG_DMA_LEVEL, "Write Test: $%02x at $%08x", testvar, base_addr);
    
    dma_init = 0;
    
    /* saved limit is checked to calculate packet size
     by both the rom and netbsd */ 
    dma_saved_limit = dma_next + size;
    dma_saved_next  = dma_next;
    
    if(!(read_dma_scsi_csr & DMA_SUPDATE)) {
        dma_next = dma_start;
        dma_limit = dma_stop;
    }

    read_dma_scsi_csr |= DMA_COMPLETE;
    
    set_interrupt(INT_SCSI_DMA, SET_INT);
//    set_interrupt(INT_SCSI_DMA, RELEASE_INT);
}
