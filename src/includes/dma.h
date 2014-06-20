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

/* Device functions */
void dma_esp_write_memory(void);
void dma_esp_flush_buffer(void);
void dma_esp_read_memory(void);

void dma_mo_write_memory(void);
void dma_mo_flush_buffer(void);
void dma_mo_read_memory(void);

void dma_enet_write_memory(void);
void dma_enet_read_memory(void);

void dma_m2m_write_memory(void);

void dma_scc_read_memory(void);

void dma_sndout_read_memory(void);

/* Delayed DMA interrupt handlers */
void M2RDMA_InterruptHandler(void);
void R2MDMA_InterruptHandler(void);

/* Function for video interrupt */
void dma_video_interrupt(void);
