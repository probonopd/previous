/* ESP DMA control and status registers */

struct {
    Uint8 control;
    Uint8 status;
} esp_dma;

#define ESPCTRL_CLKMASK     0xc0    /* clock selection bits */
#define ESPCTRL_CLK10MHz    0x00
#define ESPCTRL_CLK12MHz    0x40
#define ESPCTRL_CLK16MHz    0xc0
#define ESPCTRL_CLK20MHz    0x80
#define ESPCTRL_ENABLE_INT  0x20    /* enable ESP interrupt */
#define ESPCTRL_MODE_DMA    0x10    /* select mode: 1 = dma, 0 = pio */
#define ESPCTRL_DMA_READ    0x08    /* select direction: 1 = scsi>mem, 0 = mem>scsi */
#define ESPCTRL_FLUSH       0x04    /* flush DMA buffer */
#define ESPCTRL_RESET       0x02    /* hard reset ESP */
#define ESPCTRL_CHIP_TYPE   0x01    /* select chip type: 1 = WD33C92, 0 = NCR53C90(A) */

#define ESPSTAT_STATE_MASK  0xc0
#define ESPSTAT_STATE_D0S0  0x00    /* DMA ready   for buffer 0, SCSI in buffer 0 */
#define ESPSTAT_STATE_D0S1  0x40    /* DMA request for buffer 0, SCSI in buffer 1 */
#define ESPSTAT_STATE_D1S1  0x80    /* DMA ready   for buffer 1, SCSI in buffer 1 */
#define ESPSTAT_STATE_D1S0  0xc0    /* DMA request for buffer 1, SCSI in buffer 0 */
#define ESPSTAT_OUTFIFO_MSK 0x38    /* output fifo byte (inverted) */
#define ESPSTAT_INFIFO_MSK  0x07    /* input fifo byte (inverted) */


void ESP_DMA_CTRL_Read(void);
void ESP_DMA_CTRL_Write(void);
void ESP_DMA_FIFO_STAT_Read(void);
void ESP_DMA_FIFO_STAT_Write(void);

void ESP_DMA_set_status(void);



void ESP_TransCountL_Read(void); 
void ESP_TransCountL_Write(void); 
void ESP_TransCountH_Read(void);
void ESP_TransCountH_Write(void); 
void ESP_FIFO_Read(void);
void ESP_FIFO_Write(void); 
void ESP_Command_Read(void); 
void ESP_Command_Write(void); 
void ESP_Status_Read(void);
void ESP_SelectBusID_Write(void); 
void ESP_IntStatus_Read(void);
void ESP_SelectTimeout_Write(void); 
void ESP_SeqStep_Read(void);
void ESP_SyncPeriod_Write(void); 
void ESP_FIFOflags_Read(void);
void ESP_SyncOffset_Write(void); 
void ESP_Configuration_Read(void); 
void ESP_Configuration_Write(void); 
void ESP_ClockConv_Write(void);
void ESP_Test_Write(void);

void ESP_Conf2_Read(void);

void esp_reset_hard(void);
void esp_reset_soft(void);
void esp_bus_reset(void);
void esp_flush_fifo(void);

void esp_message_accepted(void);
void esp_initiator_command_complete(void);
void esp_transfer_info(void);
void esp_transfer_pad(void);
void esp_select(bool atn);

void esp_raise_irq(void);
void esp_lower_irq(void);

bool esp_transfer_done(bool write);


extern Uint32 esp_counter;

void ESP_InterruptHandler(void);
void ESP_IO_Handler(void);