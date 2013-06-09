/* NeXT DMA Emulation */

typedef enum {
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
}DMA_CHANNEL;

int get_channel(Uint32 address);
int get_interrupt_type(int channel);

void DMA_CSR_Read(void);
void DMA_CSR_Write(void);

void DMA_Saved_Next_Read(void);
void DMA_Saved_Next_Write(void);
void DMA_Saved_Limit_Read(void);
void DMA_Saved_Limit_Write(void);
void DMA_Saved_Start_Read(void);
void DMA_Saved_Start_Write(void);
void DMA_Saved_Stop_Read(void);
void DMA_Saved_Stop_Write(void);

void DMA_Next_Read(void);
void DMA_Next_Write(void);
void DMA_Limit_Read(void);
void DMA_Limit_Write(void);
void DMA_Start_Read(void);
void DMA_Start_Write(void);
void DMA_Stop_Read(void);
void DMA_Stop_Write(void);

void DMA_Init_Read(void);
void DMA_Init_Write(void);
void DMA_Size_Read(void);
void DMA_Size_Write(void);


void dma_memory_write(Uint8 *buf, Uint32 size, int channel);
void dma_memory_read(Uint8 *buf, Uint32 *size, int channel);
void dma_clear_memory(Uint32 datalength);

/* Buffers */
#define DMA_BUFFER_SIZE 65536

/* dma read buffer */
Uint8 dma_read_buffer[DMA_BUFFER_SIZE];

/* dma write buffer */
Uint8 dma_write_buffer[DMA_BUFFER_SIZE];
