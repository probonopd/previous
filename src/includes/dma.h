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

/* DMA Registers */
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

/* Turbo DMA functions */
void TDMA_CSR_Read(void);
void TDMA_CSR_Write(void);
void TDMA_Saved_Next_Read(void);
void tdma_flush_buffer(int channel);

/* Device functions */
void dma_esp_write_memory(void);
void dma_esp_read_memory(void);
void dma_esp_flush_buffer(void);

void dma_mo_write_memory(void);
void dma_mo_read_memory(void);

void dma_enet_write_memory(bool eop);
bool dma_enet_read_memory(void);

void dma_dsp_write_memory(Uint8 val);
Uint8 dma_dsp_read_memory(void);
bool dma_dsp_ready(void);

void dma_m2m(void);
void dma_m2m_write_memory(void);

void dma_scc_read_memory(void);

Uint8* dma_sndout_read_memory(int* len);
void   dma_sndout_intr(void);
int    dma_sndin_write_memory(void);

void dma_printer_read_memory(void);

/* M2M DMA IO handler */
void M2MDMA_IO_Handler(void);

/* Function for video interrupt */
void dma_video_interrupt(void);
