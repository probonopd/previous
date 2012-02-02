/* Command Register */
#define CMD_DMA      0x80
#define CMD_CMD      0x7f

#define CMD_NOP      0x00
#define CMD_FLUSH    0x01
#define CMD_RESET    0x02
#define CMD_BUSRESET 0x03
#define CMD_TI       0x10
#define CMD_ICCS     0x11
#define CMD_MSGACC   0x12
#define CMD_PAD      0x18
#define CMD_SATN     0x1a
#define CMD_SEL      0x41
#define CMD_SELATN   0x42
#define CMD_SELATNS  0x43
#define CMD_ENSEL    0x44

#define CMD_SEMSG    0x20
#define CMD_SESTAT   0x21
#define CMD_SEDAT    0x22
#define CMD_DISSEQ   0x23
#define CMD_TERMSEQ  0x24
#define CMD_TCCS     0x25
#define CMD_DIS      0x27
#define CMD_RMSGSEQ  0x28
#define CMD_RCOMM    0x29
#define CMD_RDATA    0x2A
#define CMD_RCSEQ    0x2B

/* Interrupt Status Register */
#define STAT_DO      0x00
#define STAT_DI      0x01
#define STAT_CD      0x02
#define STAT_ST      0x03
#define STAT_MO      0x06
#define STAT_MI      0x07

#define STAT_MASK    0xF8

#define STAT_VGC     0x08
#define STAT_TC      0x10
#define STAT_PE      0x20
#define STAT_GE      0x40
#define STAT_INT     0x80

/* Bus ID Register */
#define BUSID_DID    0x07

/* Interrupt Register */
#define INTR_SEL     0x01
#define INTR_SELATN  0x02
#define INTR_RESEL   0x04
#define INTR_FC      0x08
#define INTR_BS      0x10
#define INTR_DC      0x20
#define INTR_ILL     0x40
#define INTR_RST     0x80

/*Sequence Step Register */
#define SEQ_0        0x00
#define SEQ_SELTIMEOUT 0x02
#define SEQ_CD       0x04

/*Configuration Register */
#define CFG1_RESREPT 0x40

#define CFG2_ENFEA   0x40

#define TCHI_FAS100A 0x04


void SCSI_CSR0_Read(void); 
void SCSI_CSR0_Write(void);
void SCSI_CSR1_Read(void);
void SCSI_CSR1_Write(void);

void SCSI_TransCountL_Read(void); 
void SCSI_TransCountL_Write(void); 
void SCSI_TransCountH_Read(void);
void SCSI_TransCountH_Write(void); 
void SCSI_FIFO_Read(void);
void SCSI_FIFO_Write(void); 
void SCSI_Command_Read(void); 
void SCSI_Command_Write(void); 
void SCSI_Status_Read(void);
void SCSI_SelectBusID_Write(void); 
void SCSI_IntStatus_Read(void);
void SCSI_SelectTimeout_Write(void); 
void SCSI_SeqStep_Read(void);
void SCSI_SyncPeriod_Write(void); 
void SCSI_FIFOflags_Read(void);
void SCSI_SyncOffset_Write(void); 
void SCSI_Configuration_Read(void); 
void SCSI_Configuration_Write(void); 
void SCSI_ClockConv_Write(void);
void SCSI_Test_Write(void);


void esp_reset_hard(void);
void esp_reset_soft(void);
void esp_flush_fifo(void);
void handle_satn(void);
void handle_ti(void);
void esp_raise_irq(void);
void esp_lower_irq(void);
Uint32 get_cmd(void);
void do_cmd(void);
void do_busid_cmd(Uint8);
void esp_do_dma(void);
void esp_dma_done(void);
void esp_transfer_data(void);
void esp_command_complete(void);
void write_response(void);
