/* NeXT DMA Emulation */


enum next_dma_chan {
    NEXTDMA_FD,
    NEXTDMA_ENRX,
    NEXTDMA_ENTX,
    NEXTDMA_SCSI,
    NEXTDMA_SCC,
    NEXTDMA_SND
};

void DMA_SCSI_CSR_Read(void);
void DMA_SCSI_CSR_Write(void);

void DMA_SCSI_Saved_Next_Read(void);
void DMA_SCSI_Saved_Next_Write(void);
void DMA_SCSI_Saved_Limit_Read(void);
void DMA_SCSI_Saved_Limit_Write(void);
void DMA_SCSI_Saved_Start_Read(void);
void DMA_SCSI_Saved_Start_Write(void);
void DMA_SCSI_Saved_Stop_Read(void);
void DMA_SCSI_Saved_Stop_Write(void);

void DMA_SCSI_Next_Read(void);
void DMA_SCSI_Next_Write(void);
void DMA_SCSI_Limit_Read(void);
void DMA_SCSI_Limit_Write(void);
void DMA_SCSI_Start_Read(void);
void DMA_SCSI_Start_Write(void);
void DMA_SCSI_Stop_Read(void);
void DMA_SCSI_Stop_Write(void);

void DMA_SCSI_Init_Read(void);
void DMA_SCSI_Init_Write(void);
void DMA_SCSI_Size_Read(void);
void DMA_SCSI_Size_Write(void);


void nextdma_write(Uint8 *buf, int size, int type);
void copy_to_scsidma_buffer(Uint8 device_outbuf[], int return_length);
void dma_memory_read(Uint32 datalength);
void dma_clear_memory(Uint32 datalength);

/* Buffers */

/* scsi dma read buffer */
#define DMA_READBUF_MAX 65536
Uint8 dma_read_buffer[DMA_READBUF_MAX];

/* dma write buffer */
#define DMA_BUFFER_SIZE 65536
Uint8 dma_write_buffer[DMA_BUFFER_SIZE];
