/* Emulation of NCR53C90(A)
 Includes informations from QEMU-NeXT
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "esp.h"
#include "sysReg.h"
#include "dma.h"
#include "scsi.h"

#define LOG_ESPDMA_LEVEL    LOG_DEBUG   /* Print debugging messages for ESP DMA registers */
#define LOG_ESPCMD_LEVEL    LOG_DEBUG   /* Print debugging messages for ESP commands */
#define LOG_ESPREG_LEVEL    LOG_DEBUG   /* Print debugging messages for ESP registers */
#define LOG_ESPFIFO_LEVEL   LOG_DEBUG   /* Print debugging messages for ESP FIFO */


#define IO_SEG_MASK	0x1FFFF

typedef enum {
    DISCONNECTED,
    INITIATOR,
    TARGET
} SCSI_STATE;

SCSI_STATE esp_state;


/* ESP FIFO */
#define ESP_FIFO_SIZE 16
Uint8 esp_fifo_read(void);
void esp_fifo_write(Uint8 val);
void esp_fifo_clear(void);

/* ESP Command Register */
Uint8 esp_cmd_state;
#define ESP_CMD_INPROGRESS  0x01
#define ESP_CMD_WAITING     0x02
void esp_start_command(Uint8 cmd);
void esp_finish_command(void);
void esp_command_clear(void);
void esp_command_write(Uint8 cmd);

/* ESP Registers */
Uint8 writetranscountl;
Uint8 writetranscounth;
Uint8 fifo[ESP_FIFO_SIZE];
Uint8 command[2];
Uint8 status;
Uint8 selectbusid;
Uint8 intstatus;
Uint8 selecttimeout;
Uint8 seqstep;
Uint8 syncperiod;
Uint8 fifoflags;
Uint8 syncoffset;
Uint8 configuration;
Uint8 clockconv;
Uint8 esptest;

Uint32 esp_counter;


/* Command Register */
#define CMD_DMA      0x80
#define CMD_CMD      0x7f

#define CMD_TYP_MASK 0x70
#define CMD_TYP_MSC  0x00
#define CMD_TYP_TGT  0x20
#define CMD_TYP_INR  0x10
#define CMD_TYP_DIS  0x40

/* Miscellaneous Commands */
#define CMD_NOP      0x00
#define CMD_FLUSH    0x01
#define CMD_RESET    0x02
#define CMD_BUSRESET 0x03
/* Initiator Commands */
#define CMD_TI       0x10
#define CMD_ICCS     0x11
#define CMD_MSGACC   0x12
#define CMD_PAD      0x18
#define CMD_SATN     0x1a
/* Disconnected Commands */
#define CMD_RESEL    0x40
#define CMD_SEL      0x41
#define CMD_SELATN   0x42
#define CMD_SELATNS  0x43
#define CMD_ENSEL    0x44
#define CMD_DISSEL   0x45
/* Target Commands */
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

/* Status Register */
#define STAT_MASK    0xF8
#define STAT_PHASE   0x07

#define STAT_VGC     0x08
#define STAT_TC      0x10
#define STAT_PE      0x20
#define STAT_GE      0x40
#define STAT_INT     0x80

/* Bus ID Register */
#define BUSID_DID    0x07

/* Interrupt Status Register */
#define INTR_SEL     0x01
#define INTR_SELATN  0x02
#define INTR_RESEL   0x04
#define INTR_FC      0x08
#define INTR_BS      0x10
#define INTR_DC      0x20
#define INTR_ILL     0x40
#define INTR_RST     0x80

/* Sequence Step Register */
#define SEQ_0        0x00
#define SEQ_SELTIMEOUT 0x02
#define SEQ_CD       0x04

/* Configuration Register */
#define CFG1_RESREPT 0x40


/* ESP Status Variables */
Uint8 mode_dma;

/* Experimental */
#define ESP_CLOCK_FREQ  20      /* ESP is clocked at 20 MHz */
#define ESP_DELAY       100     /* Standard wait time for ESP interrupt (except bus reset and selection timeout) */


/* ESP DMA control and status registers */

void ESP_DMA_CTRL_Read(void) {
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = esp_dma.control;
 	Log_Printf(LOG_ESPDMA_LEVEL,"ESP DMA control read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_DMA_CTRL_Write(void) {
    Log_Printf(LOG_ESPDMA_LEVEL,"ESP DMA control write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    esp_dma.control = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
        
    if (esp_dma.control&ESPCTRL_FLUSH) {
        Log_Printf(LOG_ESPDMA_LEVEL, "flush DMA buffer\n");
		if (ConfigureParams.System.bTurbo) {
			tdma_flush_buffer(0);
		} else {
			dma_esp_flush_buffer();
		}
    }
    if (esp_dma.control&ESPCTRL_CHIP_TYPE) {
        Log_Printf(LOG_ESPDMA_LEVEL, "SCSI controller is WD33C92\n");
    } else {
        Log_Printf(LOG_ESPDMA_LEVEL, "SCSI controller is NCR53C90\n");
    }
    if (esp_dma.control&ESPCTRL_RESET) {
        Log_Printf(LOG_ESPDMA_LEVEL, "reset SCSI controller\n");
        esp_reset_hard();
    }
    if (esp_dma.control&ESPCTRL_DMA_READ) {
        Log_Printf(LOG_ESPDMA_LEVEL, "DMA from SCSI to mem\n");
    } else {
        Log_Printf(LOG_ESPDMA_LEVEL, "DMA from mem to SCSI\n");
    }
    if (esp_dma.control&ESPCTRL_MODE_DMA) {
        Log_Printf(LOG_ESPDMA_LEVEL, "mode DMA\n");
    } else {
        Log_Printf(LOG_ESPDMA_LEVEL, "mode PIO\n");
    }
    if (esp_dma.control&ESPCTRL_ENABLE_INT) {
        Log_Printf(LOG_ESPDMA_LEVEL, "Enable ESP interrupt");
        if (status&STAT_INT) {
            set_interrupt(INT_SCSI, SET_INT);
        }
    } else {
        Log_Printf(LOG_ESPDMA_LEVEL, "Block ESP interrupt");
        set_interrupt(INT_SCSI, RELEASE_INT);
    }
    switch (esp_dma.control&ESPCTRL_CLKMASK) {
        case ESPCTRL_CLK10MHz:
            Log_Printf(LOG_ESPDMA_LEVEL, "10 MHz clock\n");
            break;
        case ESPCTRL_CLK12MHz:
            Log_Printf(LOG_ESPDMA_LEVEL, "12.5 MHz clock\n");
            break;
        case ESPCTRL_CLK16MHz:
            Log_Printf(LOG_ESPDMA_LEVEL, "16.6 MHz clock\n");
            break;
        case ESPCTRL_CLK20MHz:
            Log_Printf(LOG_ESPDMA_LEVEL, "20 MHz clock\n");
            break;
        default:
            break;
    }
}

void ESP_DMA_FIFO_STAT_Read(void) {
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = esp_dma.status;
 	Log_Printf(LOG_ESPDMA_LEVEL,"ESP DMA FIFO status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_DMA_FIFO_STAT_Write(void) {
 	Log_Printf(LOG_ESPDMA_LEVEL,"ESP DMA FIFO status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    esp_dma.status = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
}

void ESP_DMA_set_status(void) { /* this is just a guess */
    if ((esp_dma.status&ESPSTAT_STATE_MASK) == ESPSTAT_STATE_D0S1) {
        //Log_Printf(LOG_WARN,"DMA in buffer 0, SCSI in buffer 1\n");
        esp_dma.status = (esp_dma.status&~ESPSTAT_STATE_MASK)|ESPSTAT_STATE_D1S0;
    } else {
        //Log_Printf(LOG_WARN,"DMA in buffer 1, SCSI in buffer 0\n");
        esp_dma.status = (esp_dma.status&~ESPSTAT_STATE_MASK)|ESPSTAT_STATE_D0S1;
    }
}

/* ESP Registers */

void ESP_TransCountL_Read(void) { // 0x02014000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=esp_counter&0xFF;
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP TransCountL read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_TransCountL_Write(void) {
    writetranscountl=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP TransCountL write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_TransCountH_Read(void) { // 0x02014001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=(esp_counter>>8)&0xFF;
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP TransCoundH read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_TransCountH_Write(void) {
    writetranscounth=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP TransCountH write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_FIFO_Read(void) { // 0x02014002
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = esp_fifo_read();
    Log_Printf(LOG_ESPREG_LEVEL,"ESP FIFO read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_FIFO_Write(void) {
    esp_fifo_write(IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
    Log_Printf(LOG_ESPREG_LEVEL,"ESP FIFO write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_Command_Read(void) { // 0x02014003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=command[0];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP Command read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_Command_Write(void) {
    esp_command_write(IoMem[IoAccessCurrentAddress & IO_SEG_MASK]);
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP Command write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_Status_Read(void) { // 0x02014004
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=(status&STAT_MASK)|(SCSIbus.phase&STAT_PHASE);
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP Status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_SelectBusID_Write(void) {
    selectbusid=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP SelectBusID write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_IntStatus_Read(void) { // 0x02014005
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=intstatus;
    Log_Printf(LOG_ESPREG_LEVEL,"ESP IntStatus read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (status&STAT_INT) {
            intstatus = 0x00;
            status &= ~(STAT_VGC | STAT_PE | STAT_GE);
            //seqstep = 0x00; /* FIXME: Is the data sheet really wrong with this? */
            esp_lower_irq();
    }
}

void ESP_SelectTimeout_Write(void) {
    selecttimeout=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP SelectTimeout write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_SeqStep_Read(void) { // 0x02014006
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=seqstep;
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP SeqStep read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_SyncPeriod_Write(void) {
    syncperiod=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP SyncPeriod write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_FIFOflags_Read(void) { // 0x02014007
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=fifoflags;
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP FIFOflags read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_SyncOffset_Write(void) {
    syncoffset=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP SyncOffset write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_Configuration_Read(void) { // 0x02014008
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=configuration;
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP Configuration read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_Configuration_Write(void) {
    configuration=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP Configuration write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_ClockConv_Write(void) { // 0x02014009
    clockconv=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP ClockConv write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void ESP_Test_Write(void) { // 0x0201400a
    esptest=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_ESPREG_LEVEL,"ESP Test write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

/* System reads this register to check if we use old or new SCSI controller.
 * Return 0 to report old chip. */
void ESP_Conf2_Read(void) { // 0x0201400b
    if (ConfigureParams.System.nSCSI == NCR53C90)
        IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = 0x00;
    Log_Printf(LOG_ESPREG_LEVEL,"ESP Configuration2 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}


/* Helper functions */

/* Functions for reading and writing ESP FIFO */
Uint8 esp_fifo_read(void) {
    int i;
    Uint8 val;
    
    if (fifoflags > 0) {
        val = fifo[0];
        for (i=0; i<(ESP_FIFO_SIZE-1); i++)
            fifo[i]=fifo[i+1];
        fifo[ESP_FIFO_SIZE-1] = 0x00;
        fifoflags--;
        Log_Printf(LOG_ESPFIFO_LEVEL,"ESP FIFO: Reading byte, val=%02x, size = %i", val, fifoflags);
    } else {
        val = 0x00;
        Log_Printf(LOG_WARN, "ESP FIFO read: FIFO is empty!\n");
    }
    return val;
}

void esp_fifo_write(Uint8 val) {
    if (fifoflags==ESP_FIFO_SIZE) {
        Log_Printf(LOG_WARN, "ESP FIFO write: FIFO overflow! Top of FIFO overwritten\n");
        fifo[fifoflags-1] = val;
        status |= STAT_GE;
    } else {
        fifoflags++;
        fifo[fifoflags-1] = val;
        Log_Printf(LOG_ESPFIFO_LEVEL,"ESP FIFO: Writing byte %i, val=%02x", fifoflags-1, fifo[fifoflags-1]);
    }
}

void esp_fifo_clear(void) {
    int i;
    for (i=0; i<ESP_FIFO_SIZE; i++) {
        fifo[i] = 0;
    }
    fifoflags &= 0xE0;
}

/* Functions for handling dual ranked command register */
void esp_command_write(Uint8 cmd) {
    if ((command[1]&CMD_CMD)==CMD_RESET && (cmd&CMD_CMD)!=CMD_NOP) {
        Log_Printf(LOG_WARN, "ESP command write: Chip reset in command register, not executing command.\n");
    } else {
        command[1] = cmd;
        
        if (esp_cmd_state&ESP_CMD_WAITING) {
            Log_Printf(LOG_WARN, "ESP command write: Error! Top of command register overwritten.\n");
            status |= STAT_GE;
        }
    }
    
    if ((command[1]&CMD_CMD)==CMD_RESET || (command[1]&CMD_CMD)==CMD_BUSRESET) {
        esp_start_command(command[1]);
        return;
    }
    
    if (esp_cmd_state&ESP_CMD_INPROGRESS) {
        esp_cmd_state |= ESP_CMD_WAITING;
    } else {
        command[0] = command[1];
        command[1] = 0x00;
        esp_start_command(command[0]);
    }
}

void esp_command_clear(void) {
    command[0] = 0x00;
    if ((command[1]&CMD_CMD)!=CMD_RESET) {
        command[1] = 0x00;
        esp_cmd_state &= ~ESP_CMD_WAITING;
    }
}

void esp_finish_command(void) {
    esp_cmd_state &= ~ESP_CMD_INPROGRESS;
    if (esp_cmd_state&ESP_CMD_WAITING) {
        command[0] = command[1];
        command[1] = 0x00;
        esp_cmd_state &= ~ESP_CMD_WAITING;
        esp_start_command(command[0]);
    }
}

void esp_start_command(Uint8 cmd) {
    esp_cmd_state |= ESP_CMD_INPROGRESS;
    
    /* Check if command is valid for actual state */
    if ((cmd&CMD_TYP_MASK)!=CMD_TYP_MSC) {
        if ((esp_state==TARGET && !(cmd&CMD_TYP_TGT)) ||
            (esp_state==INITIATOR && !(cmd&CMD_TYP_INR)) ||
            (esp_state==DISCONNECTED && !(cmd&CMD_TYP_DIS))) {
            Log_Printf(LOG_WARN, "ESP Command: Illegal command for actual ESP state ($%02X)!\n",cmd);
            esp_command_clear();
            intstatus |= INTR_ILL;
            CycInt_AddRelativeInterruptUs(ESP_DELAY, 20, INTERRUPT_ESP);
            return;
        }
    }
    
    /* Check if the command is a DMA command */
    if (cmd & CMD_DMA) {
        /* Load the internal counter on every DMA command, do not decrement actual registers! */
        esp_counter = writetranscountl | (writetranscounth << 8);
        if (esp_counter == 0) { /* 0 means maximum value */
            esp_counter = 0x10000;
        }
        status &= ~STAT_TC;
        mode_dma = 1;
    } else {
        mode_dma = 0;
    }
    
    switch (cmd & CMD_CMD) {
            /* Miscellaneous */
        case CMD_NOP:
            Log_Printf(LOG_ESPCMD_LEVEL, "ESP Command: NOP\n");
            esp_finish_command();
            break;
        case CMD_FLUSH:
            Log_Printf(LOG_ESPCMD_LEVEL,"ESP Command: flush FIFO\n");
            esp_flush_fifo();
            break;
        case CMD_RESET:
            Log_Printf(LOG_ESPCMD_LEVEL,"ESP Command: reset chip\n");
            esp_reset_hard();
            break;
        case CMD_BUSRESET:
            Log_Printf(LOG_ESPCMD_LEVEL, "ESP Command: reset SCSI bus\n");
            esp_bus_reset();
            break;
            /* Disconnected */
        case CMD_RESEL:
            Log_Printf(LOG_WARN, "ESP Command: reselect sequence\n");
            abort();
            break;
        case CMD_SEL:
            Log_Printf(LOG_ESPCMD_LEVEL, "ESP Command: select without ATN sequence\n");
            esp_select(false);
            break;
        case CMD_SELATN:
            Log_Printf(LOG_ESPCMD_LEVEL, "ESP Command: select with ATN sequence\n");
            esp_select(true);
            break;
        case CMD_SELATNS:
            Log_Printf(LOG_WARN, "ESP Command: select with ATN and stop sequence\n");
            abort();
            break;
        case CMD_ENSEL:
            Log_Printf(LOG_ESPCMD_LEVEL, "ESP Command: enable selection/reselection\n");
            esp_finish_command(); /* Our disk doesn't do reselections */
            break;
        case CMD_DISSEL:
            Log_Printf(LOG_WARN, "ESP Command: disable selection/reselection\n");
            abort();
            break;
            /* Initiator */
        case CMD_TI:
            Log_Printf(LOG_ESPCMD_LEVEL, "ESP Command: transfer information\n");
            esp_transfer_info();
            break;
        case CMD_ICCS:
            Log_Printf(LOG_ESPCMD_LEVEL, "ESP Command: initiator command complete sequence\n");
            esp_initiator_command_complete();
            break;
        case CMD_MSGACC:
            Log_Printf(LOG_ESPCMD_LEVEL, "ESP Command: message accepted\n");
            esp_message_accepted();
            break;
        case CMD_PAD:
            Log_Printf(LOG_ESPCMD_LEVEL, "ESP Command: transfer pad\n");
            esp_transfer_pad();
            break;
        case CMD_SATN:
            Log_Printf(LOG_WARN, "ESP Command: set ATN\n");
            abort();
            break;
            /* Target */
        case CMD_SEMSG:
        case CMD_SESTAT:
        case CMD_SEDAT:
        case CMD_DISSEQ:
        case CMD_TERMSEQ:
        case CMD_TCCS:
        case CMD_RMSGSEQ:
        case CMD_RCOMM:
        case CMD_RDATA:
        case CMD_RCSEQ:
            Log_Printf(LOG_WARN, "ESP Command: Target commands not emulated!\n");
            abort();
            break;
        case CMD_DIS:
            Log_Printf(LOG_WARN, "ESP Command: DISCONNECT not emulated!\n");
            abort();
            SCSIbus.phase = PHASE_ST;
            intstatus = INTR_DC;
            seqstep = SEQ_0;
            break;
            
        default:
            Log_Printf(LOG_WARN, "ESP Command: Illegal command!\n");
            esp_command_clear();
            intstatus |= INTR_ILL;
            CycInt_AddRelativeInterruptUs(ESP_DELAY, 20, INTERRUPT_ESP);
            break;
    }
}


/* This is the handler function for ESP delayed interrupts */
void ESP_InterruptHandler(void) {
	CycInt_AcknowledgeInterrupt();
    esp_raise_irq();
}


void esp_raise_irq(void) {
    if(!(status & STAT_INT)) {
        status |= STAT_INT;
        
        if (esp_dma.control&ESPCTRL_ENABLE_INT) {
            set_interrupt(INT_SCSI, SET_INT);
        }
        
        if (LOG_ESPCMD_LEVEL == LOG_WARN) {
            printf("[ESP] Raise IRQ: state=");
            switch (esp_state) {
                case DISCONNECTED: printf("disconnected"); break;
                case INITIATOR: printf("initiator"); break;
                case TARGET: printf("target"); break;
                default: printf("unknown"); break;
            }
            printf(", phase=");
            switch (SCSIbus.phase&STAT_PHASE) {
                case PHASE_DO: printf("data out"); break;
                case PHASE_DI: printf("data in"); break;
                case PHASE_CD: printf("command"); break;
                case PHASE_ST: printf("status"); break;
                case PHASE_MI: printf("msg in"); break;
                case PHASE_MO: printf("msg out"); break;
                default: printf("unknown"); break;
            }
            if (status&STAT_TC) {
                printf(", transfer complete");
            } else {
                printf(", transfer not complete");
            }
            printf(", sequence step=%i", seqstep);
            printf(", interrupt status:\n");
            if (intstatus&INTR_RST) printf("bus reset\n");
            if (intstatus&INTR_BS) printf("bus service\n");
            if (intstatus&INTR_DC) printf("disconnected\n");
            if (intstatus&INTR_FC) printf("function complete\n");
            if (intstatus&INTR_ILL) printf("illegal command\n");
            if (intstatus&INTR_RESEL) printf("reselected\n");
            if (intstatus&INTR_SEL) printf("selected\n");
            if (intstatus&INTR_SELATN) printf("selected with ATN\n");
        }
    }
}

void esp_lower_irq(void) {
    if (status & STAT_INT) {
        status &= ~STAT_INT;
        
        set_interrupt(INT_SCSI, RELEASE_INT);
        
        Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Lower IRQ\n");
        
        esp_finish_command();
    }
}

/* Functions */

/* Reset chip */
void esp_reset_hard(void) {
    Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Hard reset\n");

    clockconv = 0x02;
    configuration &= ~0xF8; // clear chip test mode, parity enable, parity test, scsi request/int disable, slow cable mode
    esp_fifo_clear();
    syncperiod = 0x05;
    syncoffset = 0x00;
    status &= ~STAT_INT; // release interrupt
    set_interrupt(INT_SCSI, RELEASE_INT);
    intstatus = 0x00;
    status &= ~(STAT_VGC | STAT_PE | STAT_GE); // clear transfer complete aka valid group code, parity error, gross error
    esp_reset_soft();
    esp_finish_command();
}



void esp_reset_soft(void) {
    status &= ~STAT_TC; /* clear transfer count zero */
    
    /* check, if this is complete */
    mode_dma = 0;
    esp_counter = 0; /* reset counter, but not actual registers! */
    
    seqstep = 0x00;
    
    /* writetranscountl, writetranscounth, selectbusid, selecttimeout are not initialized by reset */

    /* This part is "disconnect reset" */
    esp_command_clear();
    esp_state = DISCONNECTED;
}


/* Reset SCSI bus */
void esp_bus_reset(void) {
    
    esp_reset_soft();
    if (!(configuration & CFG1_RESREPT)) {
        intstatus = INTR_RST;
        SCSIbus.phase = PHASE_MI; /* CHECK: why message in phase? */
        Log_Printf(LOG_ESPCMD_LEVEL,"[ESP] SCSI bus reset raising IRQ (configuration=$%02X)\n",configuration);
        CycInt_AddRelativeInterruptUs(500, 0, INTERRUPT_ESP); /* CHECK: how is this delay defined? */
    } else {
        Log_Printf(LOG_ESPCMD_LEVEL,"[ESP] SCSI bus reset not interrupting (configuration=$%02X)\n",configuration);
        esp_finish_command();
    }
}


/* Flush FIFO */
void esp_flush_fifo(void) {
    esp_fifo_clear();
    esp_finish_command();
}


/* Select with or without ATN */
void esp_select(bool atn) {
    int cmd_size;
    Uint8 identify_msg = 0;
    Uint8 commandbuf[SCSI_CDB_MAX_SIZE];

    seqstep = 0;
    
    /* First select our target */
    Uint8 target = selectbusid & BUSID_DID; /* Get bus ID from register */
    bool timeout = SCSIdisk_Select(target);
    if (timeout) {
        /* If a timeout occurs, generate disconnect interrupt */
        intstatus = INTR_DC;
        esp_command_clear();
        esp_state = DISCONNECTED;
        int seltout = (selecttimeout * 8192 * clockconv) / ESP_CLOCK_FREQ; /* timeout in microseconds */
        Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Select: Target %i, timeout after %i microseconds",target,seltout);
        CycInt_AddRelativeInterruptUs(seltout, 0, INTERRUPT_ESP);
        return;
    }
    
    /* Next get our command */
    if(mode_dma == 1) {
        cmd_size = esp_counter;
        Log_Printf(LOG_WARN, "[ESP] Select: Reading command using DMA, size %i byte (not implemented!)",cmd_size);
        abort();
    } else {
        if (atn) { /* Read identify message from FIFO */
            SCSIbus.phase = PHASE_MO;
            seqstep = 1;
            identify_msg = esp_fifo_read();
            Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Select: Reading message from FIFO");
            Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Select: Identify Message: $%02X",identify_msg);
        }
        
        /* Read command from FIFO */
        SCSIbus.phase = PHASE_CD;
        seqstep = 3;
        for (cmd_size = 0; cmd_size < SCSI_CDB_MAX_SIZE && fifoflags > 0; cmd_size++) {
            commandbuf[cmd_size] = esp_fifo_read();
        }

        Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Select: Reading command from FIFO, size: %i byte",cmd_size);
    }
    
    Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Select: Target: %i",target);

    SCSIdisk_Receive_Command(commandbuf, identify_msg);
    seqstep = 4;
    esp_command_clear();

    intstatus = INTR_BS | INTR_FC;
    
    esp_state = INITIATOR;
    CycInt_AddRelativeInterruptUs(ESP_DELAY, 20, INTERRUPT_ESP);
}


/* DMA done: this is called as part of transfer info or transfer pad
 * after DMA transfer has completed. */

enum {
    ESP_IO_STATE_TRANSFERING,
    ESP_IO_STATE_FLUSHING,
    ESP_IO_STATE_DONE
} esp_io_state;

bool esp_transfer_done(bool write) {
    Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Transfer done: ESP counter = %i, SCSI residual bytes: %i",
               esp_counter,scsi_buffer.size);
    
    if (esp_counter == 0) { /* Transfer done */
        intstatus = INTR_FC;
        status |= STAT_TC;
        CycInt_AddRelativeInterruptUs(ESP_DELAY, 20, INTERRUPT_ESP);
        return true;
    } else if ((write && SCSIbus.phase!=PHASE_DI) || (!write && SCSIbus.phase!=PHASE_DO)) { /* Phase change detected */
        esp_command_clear();
        intstatus = INTR_BS;
        CycInt_AddRelativeInterruptUs(ESP_DELAY, 20, INTERRUPT_ESP);
        return true;
    } /* else continue transfering data, no interrupt */
    return false;
}


/* Transfer information */
void esp_transfer_info(void) {
    if(mode_dma) {
        esp_io_state=ESP_IO_STATE_TRANSFERING;
        CycInt_AddRelativeInterruptUs(SCSI_Seek_Time() + SCSI_Sector_Time(), 100, INTERRUPT_ESP_IO);
    } else {
        Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] start PIO transfer");
        switch (SCSIbus.phase) {
            case PHASE_DI:
                esp_fifo_write(SCSIdisk_Send_Data());
                CycInt_AddRelativeInterruptCycles(20, INTERRUPT_ESP);
                break;
            case PHASE_MI:
                CycInt_AddRelativeInterruptCycles(20, INTERRUPT_ESP);
                break;
            case PHASE_ST:
                /* FIXME: What should happen here? */
                Log_Printf(LOG_WARN, "[ESP] Error! Transfer info status phase");
                CycInt_AddRelativeInterruptCycles(20, INTERRUPT_ESP);
                break;
            default:
                Log_Printf(LOG_WARN, "[ESP] PIO transfer (unimplemented)");
                abort();
                break;
        }
    }
}
void ESP_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    
    switch (esp_io_state) {
        case ESP_IO_STATE_TRANSFERING:
            switch (SCSIbus.phase) {
                case PHASE_DI:
                    dma_esp_write_memory();
                    if (esp_transfer_done(true)) {
                        esp_io_state=ESP_IO_STATE_FLUSHING;
                    }
                    break;
                case PHASE_DO:
                    dma_esp_read_memory();
                    if (esp_transfer_done(false)) {
                        return;
                    }
                    break;
                    
                default:
                    break;
            }
            break;
        case ESP_IO_STATE_FLUSHING:
            Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Transfer done: Flushing DMA buffer.");
            dma_esp_write_memory();
            return;
            
        default:
            Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Transfer: Unkown state (%i).",esp_io_state);
            return;
    }
    
    CycInt_AddRelativeInterruptUs(100, 0, INTERRUPT_ESP_IO);
}


/* Transfer padding */
void esp_transfer_pad(void) {
    Log_Printf(LOG_ESPCMD_LEVEL, "[ESP] Transfer padding, ESP counter: %i bytes, SCSI resid: %i bytes\n",
               esp_counter, scsi_buffer.size);

    switch (SCSIbus.phase) {
        case PHASE_DI:
            while (SCSIbus.phase==PHASE_DI && esp_counter>0) {
                SCSIdisk_Send_Data();
                esp_counter--;
            }
            esp_transfer_done(true);
            break;
        case PHASE_DO:
            while (SCSIbus.phase==PHASE_DO && esp_counter>0) {
                SCSIdisk_Receive_Data(0);
                esp_counter--;
            }
            esp_transfer_done(false);
            break;
            
        default:
            abort();
            break;
    }
}


/* Initiator command complete */
void esp_initiator_command_complete(void) {
    
    if(mode_dma == 1) {
        Log_Printf(LOG_WARN, "ESP initiator command complete via DMA not implemented!");
        abort();
    } else {
        /* Receive status byte */
        esp_fifo_write(SCSIdisk_Send_Status()); /* Disk sets phase to msg in after status send */

        if (SCSIbus.phase!=PHASE_MI) { /* Stop sequence if no phase change to msg in occured */
            esp_command_clear();
            intstatus = INTR_BS;
            CycInt_AddRelativeInterruptUs(ESP_DELAY, 20, INTERRUPT_ESP);
            return;
        }
        
        /* Receive message byte */
        esp_fifo_write(SCSIdisk_Send_Message()); /* 0x00 = command complete */
    }

    intstatus = INTR_FC;
    CycInt_AddRelativeInterruptUs(ESP_DELAY, 20, INTERRUPT_ESP);
}


/* Message accepted */
void esp_message_accepted(void) {
    SCSIbus.phase = PHASE_ST; /* set at the end of iccs? */
    intstatus = INTR_BS;
    esp_state = DISCONNECTED; /* CHECK: only disconnected if message was cmd complete? */
    CycInt_AddRelativeInterruptUs(ESP_DELAY, 20, INTERRUPT_ESP);
}



#if 0 /* this is for target commands! */
/* Decode command to determine the command group and thus the
 * length of the incoming command. Set "valid group code" bit
 * in status register if the group is 0, 1, 5, 6, or 7 (group
 * 2 is also valid on NCR53C90A).
 */
Uint8 scsi_command_group = (commandbuf[0] & 0xE0) >> 5;
if(scsi_command_group < 3 || scsi_command_group > 4) {
    if(ConfigureParams.System.nSCSI == NCR53C90 && scsi_command_group == 2) {
        Log_Printf(LOG_WARN, "[ESP] Select: Invalid command group %i on NCR53C90\n", scsi_command_group);
        status &= ~STAT_VGC;
    } else {
        status |= STAT_VGC;
    }
} else {
    Log_Printf(LOG_WARN, "[ESP] Select: Invalid command group %i on NCR53C90A\n", scsi_command_group);
    status &= ~STAT_VGC;
}
#endif
