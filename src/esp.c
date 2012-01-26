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
#define STAT_PIO_MASK 0x06  // needed ??
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
#define SEQ_CD       0x04

/*Configuration Register */
#define CFG1_RESREPT 0x40

#define CFG2_ENFEA   0x40

#define TCHI_FAS100A 0x04

#define IO_SEG_MASK	0x1FFFF

/* SCSI DMA Command/Status Registers */
Uint8 csr_value0;
Uint8 csr_value1;

/* ESP Registers */
Uint8 readtranscountl;
Uint8 readtranscounth;
Uint8 writetranscountl;
Uint8 writetranscounth;
Uint8 fifo;
Uint8 command;
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

/* ESP Status Variables */
Uint8 irq_status;
Uint8 mode_dma;

/* ESP FIFO */
#define ESP_FIFO_SIZE 16
Uint8 esp_fifo[ESP_FIFO_SIZE];
Uint8 fifo_read_ptr;
Uint8 fifo_write_ptr;

/* Command buffer */
#define SCSI_CMD_BUF_SIZE 16
Uint8 commandbuf[SCSI_CMD_BUF_SIZE];
int command_len;


/* Experimental */

int target;
Uint32 dma;
Uint32 do_cmdvar;
Uint32 dma_counter;
Uint32 data_len;
Uint32 dma_left;
Uint32 cmdlen;
Uint32 async_len;
Uint8 *async_buf;
int dma_enabled = 1;
void (*dma_cb);
#define ESP_MAX_DEVS 7



void SCSI_CSR0_Read(void) {
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = csr_value0;
 	Log_Printf(LOG_WARN,"SCSI DMA control read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_CSR0_Write(void) {
    Log_Printf(LOG_WARN,"SCSI DMA control write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    csr_value0 = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
        
    if ((csr_value0 & 0x04) == 0x04) {
        Log_Printf(LOG_WARN, "flush FIFO\n");
    }
    if ((csr_value0 & 0x01) == 0x01) {
        Log_Printf(LOG_WARN, "scsi chip is WD33C92\n");
    } else {
        Log_Printf(LOG_WARN, "scsi chip is NCR53C90\n");
    }
    if ((csr_value0 & 0x02) == 0x02) {
        Log_Printf(LOG_WARN, "reset scsi chip\n");
        esp_reset_hard();
    }
    if ((csr_value0 & 0x08) == 0x08) {
        Log_Printf(LOG_WARN, "dma from scsi to mem\n");
    }
    if ((csr_value0 & 0x10) == 0x10) {
        set_interrupt(INT_SCSI_DMA, SET_INT);
        esp_raise_irq(); 
        Log_Printf(LOG_WARN, "mode DMA\n");
    }else{
        set_interrupt(INT_SCSI_DMA, RELEASE_INT);
        esp_lower_irq();
        Log_Printf(LOG_WARN, "mode PIO\n");
    }
    if ((csr_value0 & 0x20) == 0x20) {
        Log_Printf(LOG_WARN, "interrupt enable");
//        set_interrupt(INT_SCSI_DMA, SET_INT); is this correct?
//        esp_raise_irq();
    }
    switch (csr_value0 & 0xC0) {
        case 0x00:
            Log_Printf(LOG_WARN, "10 MHz clock\n");
            break;
        case 0x40:
            Log_Printf(LOG_WARN, "12.5 MHz clock\n");
            break;
        case 0xC0:
            Log_Printf(LOG_WARN, "16.6 MHz clock\n");
            break;
        case 0x80:
            Log_Printf(LOG_WARN, "20 MHz clock\n");
            break;
        default:
            break;
    }
}

void SCSI_CSR1_Read(void) {
    
    /*
     * dma fifo status register
     */
#define	S5RDMAS_STATE		0xc0	/* DMA/SCSI bank state */
#define	S5RDMAS_D0S0		0x00	/* DMA rdy for bank 0, SCSI in bank 0 */
#define	S5RDMAS_D0S1		0x40	/* DMA req for bank 0, SCSI in bank 1 */
#define	S5RDMAS_D1S1		0x80	/* DMA rdy for bank 1, SCSI in bank 1 */
#define	S5RDMAS_D1S0		0xc0	/* DMA req for bank 1, SCSI in bank 0 */
#define	S5RDMAS_OUTFIFOMASK	0x38	/* output fifo byte (INVERTED) */
#define	S5RDMAS_INFIFOMASK	0x07	/* input fifo byte (INVERTED) */
    
#define	S5RDMA_FIFOALIGNMENT	4	/* mass storage chip buffer size */

    
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = 0x80;
 	Log_Printf(LOG_WARN,"SCSI DMA FIFO status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_CSR1_Write(void) {
 	Log_Printf(LOG_WARN,"SCSI DMA FIFO status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    csr_value1 = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
}


/* ESP Registers */

void SCSI_TransCountL_Read(void) { // 0x02014000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=readtranscountl;
 	Log_Printf(LOG_WARN,"ESP TransCountL read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountL_Write(void) {
    writetranscountl=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP TransCountL write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountH_Read(void) { // 0x02014001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=readtranscounth;
 	Log_Printf(LOG_WARN,"ESP TransCoundH read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_TransCountH_Write(void) {
    writetranscounth=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP TransCountH write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_FIFO_Read(void) { // 0x02014002
    if ((fifo_write_ptr - fifo_read_ptr) > 0) {
        IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = esp_fifo[fifo_read_ptr];
        Log_Printf(LOG_WARN,"ESP FIFO read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
        Log_Printf(LOG_WARN,"ESP FIFO size = %i", fifo_write_ptr - fifo_read_ptr);
        fifo_read_ptr++;
        fifoflags = fifoflags - 1;
//        esp_raise_irq();
    } else {
        IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = 0x00;
        Log_Printf(LOG_WARN, "ESP FIFO is empty!\n");
    } 
}

void SCSI_FIFO_Write(void) {
    esp_fifo[fifo_write_ptr] = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
    fifo_write_ptr++;
    fifoflags = fifoflags + 1;
    Log_Printf(LOG_WARN,"ESP FIFO write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (fifo_write_ptr > (ESP_FIFO_SIZE - 1)) {
        Log_Printf(LOG_WARN, "ESP FIFO overflow! Resetting FIFO!\n");
        esp_flush_fifo();
    }
    Log_Printf(LOG_WARN,"ESP FIFO size = %i", fifo_write_ptr - fifo_read_ptr);
}

void SCSI_Command_Read(void) { // 0x02014003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=command;
 	Log_Printf(LOG_WARN,"ESP Command read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Command_Write(void) {
    command=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP Command write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (command & CMD_DMA) {
        readtranscountl = writetranscountl;
        readtranscounth = writetranscounth;
        status &= ~STAT_TC;
        mode_dma = 1;
    } else {
        mode_dma = 0;
    }
    
    status |= STAT_CD;
    
    switch (command & CMD_CMD) {
            /* Miscellaneous */
        case CMD_NOP:
            Log_Printf(LOG_WARN, "ESP Command: NOP\n");
            break;
        case CMD_FLUSH:
            Log_Printf(LOG_WARN,"ESP Command: flush FIFO\n");
            esp_flush_fifo();
            break;
        case CMD_RESET:
            Log_Printf(LOG_WARN,"ESP Command: reset chip\n");
            esp_reset_hard();
            break;
        case CMD_BUSRESET:
            //Reset all Devices on SCSI bus
            Log_Printf(LOG_WARN, "ESP Command: reset SCSI bus\n");
            esp_reset_soft();
            if (!(configuration & CFG1_RESREPT)) {
                intstatus = INTR_RST;
                Log_Printf(LOG_WARN,"Bus Reset raising IRQ\n");
                esp_raise_irq();
            }
            break;
            /* Disconnected */
        case CMD_SEL:
            Log_Printf(LOG_WARN, "ESP Command: select without ATN sequence\n");
            break;
        case CMD_SELATN:
            Log_Printf(LOG_WARN, "ESP Command: select with ATN sequence\n");
            handle_satn();
            break;
        case CMD_SELATNS:
            Log_Printf(LOG_WARN, "ESP Command: select with ATN and stop sequence\n");
            break;
        case CMD_ENSEL:
            Log_Printf(LOG_WARN, "ESP Command: enable selection/reselection\n");
            break;
            /* Initiator */
        case CMD_TI:
            Log_Printf(LOG_WARN, "ESP Command: transfer information\n");
            handle_ti();
            break;
        case CMD_ICCS:
            Log_Printf(LOG_WARN, "ESP Command: initiator command complete sequence\n");
            write_response();
            intstatus = INTR_FC;
            status |= STAT_MI;
            break;
        case CMD_MSGACC:
            Log_Printf(LOG_WARN, "ESP Command: message accepted\n");
            intstatus = INTR_DC;
            seqstep = 0x00;
            fifoflags = 0x00;
            esp_raise_irq();
            break;            
        case CMD_PAD:
            Log_Printf(LOG_WARN, "ESP Command: transfer pad\n");
            status = STAT_TC | STAT_DI;
            intstatus = 0x20;
            seqstep = SEQ_0;
            esp_raise_irq();
            break;
        case CMD_SATN:
            Log_Printf(LOG_WARN, "ESP Command: set ATN\n");
            break;
            /* Target */
        case CMD_SEMSG:
        case CMD_SESTAT:
        case CMD_SEDAT:
        case CMD_DISSEQ:
        case CMD_TERMSEQ:
        case CMD_TCCS:
        case CMD_DIS:
        case CMD_RMSGSEQ:
        case CMD_RCOMM:
        case CMD_RDATA:
        case CMD_RCSEQ:
            Log_Printf(LOG_WARN, "ESP Command: Target commands not supported!\n");
            break;
            
        default:
            Log_Printf(LOG_WARN, "ESP Command: Illegal command!\n");
            intstatus |= INTR_ILL;
            break;
    }
}

void SCSI_Status_Read(void) { // 0x02014004
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=status;
 	Log_Printf(LOG_WARN,"ESP Status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SelectBusID_Write(void) {
    selectbusid=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP SelectBusID write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_IntStatus_Read(void) { // 0x02014005
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=intstatus;
    Log_Printf(LOG_WARN,"ESP IntStatus read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if (irq_status == 1) {
            intstatus = 0x00;
            status &= ~(STAT_VGC | STAT_PE | STAT_GE);
            seqstep = SEQ_0;
            esp_lower_irq();
    }
}

void SCSI_SelectTimeout_Write(void) {
    selecttimeout=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP SelectTimeout write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SeqStep_Read(void) { // 0x02014006
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=seqstep;
 	Log_Printf(LOG_WARN,"ESP SeqStep read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SyncPeriod_Write(void) {
    syncperiod=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP SyncPeriod write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_FIFOflags_Read(void) { // 0x02014007
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=fifoflags;
 	Log_Printf(LOG_WARN,"ESP FIFOflags read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_SyncOffset_Write(void) {
    syncoffset=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP SyncOffset write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Configuration_Read(void) { // 0x02014008
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=configuration;
 	Log_Printf(LOG_WARN,"ESP Configuration read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Configuration_Write(void) {
    configuration=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP Configuration write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_ClockConv_Write(void) { // 0x02014009
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK]=clockconv;
 	Log_Printf(LOG_WARN,"ESP ClockConv write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void SCSI_Test_Write(void) {
    esptest=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_WARN,"ESP Test write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}



/* Functions */

void esp_reset_hard(void) {
    Log_Printf(LOG_WARN, "ESP hard reset\n");
    /* hard reset */
    clockconv = 0x02;
    configuration &= ~0xF8; // clear chip test mode, parity enable, parity test, scsi request/int disable, slow cable mode
    syncperiod = 0x05;
    syncoffset = 0x00;
    intstatus = 0x00;
    status &= ~(STAT_VGC | STAT_PE | STAT_GE); // need valid group code bit? clear transfer complete aka valid group code, parity error, gross error
    esp_flush_fifo();
    esp_reset_soft();
}

void esp_reset_soft(void) {
    status &= ~STAT_TC; // clear transfer count zero
    
    /* incomplete, need to work on everything below this point */
    
    dma_cb = NULL; // clear DMA interface - how to do this?
    mode_dma = 0;
    readtranscountl = 0x00;
    readtranscounth = 0x00;
    writetranscountl = 0x00;
    writetranscounth = 0x00;

    seqstep = 0x00;

    command = 0x00;
    selectbusid = 0x00;
    selecttimeout= 0x00;
    fifoflags = 0x00;
    esptest = 0x00;
    esp_lower_irq();
}

void esp_raise_irq(void) {
    if(!(status & STAT_INT)) {
        status |= STAT_INT;
        irq_status = 1;
        
        set_interrupt(INT_SCSI, SET_INT);
        
        Log_Printf(LOG_WARN, "Raise IRQ\n");
    }
}

void esp_lower_irq(void) {
    if (status & STAT_INT) {
        status &= ~STAT_INT;
        irq_status = 0;
        
        set_interrupt(INT_SCSI, RELEASE_INT);
        
        Log_Printf(LOG_WARN, "Lower IRQ\n");
    }
}

void esp_flush_fifo(void) {
    fifo_read_ptr = 0;
    fifo_write_ptr = 0;
    esp_fifo[0] = 0;
    fifoflags &= 0xE0;
}

Uint32 get_cmd (void) {
    target = selectbusid & BUSID_DID;
    
    if(mode_dma == 1) {
        command_len = readtranscountl | (readtranscounth << 8);
        dma_memory_read(command_len);
        memcpy(commandbuf, dma_read_buffer, command_len);
    } else {
        command_len = fifo_write_ptr - fifo_read_ptr;
        memcpy(commandbuf, esp_fifo, command_len);
        
        esp_flush_fifo();
    }
    Log_Printf(LOG_WARN, "get_cmd: len %i target %i", command_len, target);
    
//    if(current_req) {
//        scsi_req_cancel(current_req);
//        async_len = 0;
//    }
    
    if(target >= ESP_MAX_DEVS || SCSIcommand.nodevice == true) { // experimental
        status = 0;
        intstatus = INTR_DC;
        seqstep = SEQ_0;
        esp_raise_irq();
        return 0;
    }
    
//    current_dev = bus.devs[target];
    return command_len;
}

void do_cmd(void) {
    Uint8 busid = commandbuf[0];
    do_busid_cmd(busid);
}

void handle_satn(void) {
    Uint8 scsi_command_group;
    
/*    if(!(dma_enabled)) {
        dma_cb = handle_satn;
    }*/
    
    get_cmd(); // make get_cmd void function?
    
    /* Decode command to determine the command group and thus the
     * length of the incoming command. Set "valid group code" bit
     * in status register if the group is 0, 1, 5, 6, or 7 (group
     * 2 is also valid on NCR53C90A).
     */
    scsi_command_group = (commandbuf[1] & 0xE0) >> 5;
    if(scsi_command_group < 3 || scsi_command_group > 4) {
        if(ConfigureParams.System.nCpuLevel == 3 && scsi_command_group == 2) {
            Log_Printf(LOG_WARN, "Invalid command group %i on NCR53C90\n", scsi_command_group);
        } else {
            status |= STAT_VGC;
        }
    } else {
        Log_Printf(LOG_WARN, "Invalid command group %i on NCR53C90A\n", scsi_command_group);
    }

    if(command_len != 0)
        do_cmd();
}

void do_busid_cmd(Uint8 busid) {
    int lun;
    Log_Printf(LOG_WARN, "do_busid_cmd: busid $%02x",busid);
    lun = busid & 7;
    
    scsi_command_analyzer(commandbuf, command_len, target);
    data_len = SCSIcommand.transfer_data_len;
    
    if (data_len != 0) {
        Log_Printf(LOG_WARN, "executing command\n");
        status = STAT_TC;
        
        dma_left = 0;
        dma_counter = 0;
        
        if(SCSIcommand.transferdirection_todevice == 0) {
            Log_Printf(LOG_WARN, "DATA IN\n");
            status |= STAT_DI;
        } else {
            Log_Printf(LOG_WARN, "DATA OUT\n");
            status |= STAT_DO;
        }
    esp_transfer_data();
    }
    
    intstatus = INTR_BS | INTR_FC;
    seqstep = SEQ_CD;
    
    esp_raise_irq();   
}


void esp_transfer_data(void) {
    Log_Printf(LOG_WARN, "transfer %d/%d\n", dma_left, data_len);
    
//    async_len = len;
//    async_buf = scsi_req_get_buf(req);
    if(dma_left) {
        esp_do_dma();
    } else if(dma_counter != 0 && data_len <= 0) {
        esp_dma_done();
    }
}


void esp_do_dma(void) {
    
    Log_Printf(LOG_WARN, "call esp_do_dma\n");

    Uint32 len;
    int to_device;
    
    
    int dma_translen; // experimental
    dma_translen = readtranscountl | (readtranscounth << 8); // experimental
    Log_Printf(LOG_WARN, "call dma_write\n"); // experimental
    dma_memory_write(dma_write_buffer, dma_translen, NEXTDMA_SCSI);//experimental !!

    
    to_device = SCSIcommand.transferdirection_todevice;
    
    len = dma_left;
    
    if(do_cmdvar){
        abort();
    /*
        Log_Printf(LOG_WARN, "command len %d + %d\n", cmdlen, len);
        dma_memory_read(fifo_buf[cmdlen], len);
        fifo_size = 0;
        cmdlen = 0;
        do_cmdvar = 0;
        // do_cmd(cmd_buf);
        return;
     */
    }
    if(async_len == 0) {
//        return;
    }
    if(len > async_len) {
        len = async_len;
    }
    if(to_device) {
//        dma_memory_read(async_buf, len);
    } else {
        
        
//        dma_memory_write(async_buf, len);
    }
    
    dma_left -= len;
    async_buf += len;
    async_len -= len;
    
    if(to_device)
        data_len += len;
    else
        data_len -= len;
    
    if(async_len == 0) {
//        scsi_req_continue(current_req);
        
        if (to_device || dma_left != 0 || (fifo_write_ptr - fifo_read_ptr) == 0) {
//            return;
        }
    }
    
    esp_dma_done();
}

void esp_dma_done(void) {
    Log_Printf(LOG_WARN, "call esp_dma_done\n");
    status |= STAT_TC;
    intstatus = INTR_BS;
    seqstep = 0;
    fifoflags = 0;
    readtranscountl = 0;
    readtranscounth = 0;
    esp_raise_irq();
}

void handle_ti(void){
    Uint32 dma_len, min_len;
    
    dma_len = readtranscountl | (readtranscounth << 8);
    
    if(dma_len == 0) {
//    if(ESP_CFG2 & CFG2_ENFEA) {
//        dma_len = 0x1000000;
//    } else {
        dma_len = 0x10000;
//    }
    }
    
    dma_counter = dma_len;
    
    if(do_cmdvar)
        min_len = (dma_len < 32) ? dma_len : 32;
//    else if (data_len < 0)
//        min_len = (dma_len < (-data_len)) ? dma_len : data_len;
    else
        min_len = (dma_len > data_len) ? dma_len : data_len;
    Log_Printf(LOG_WARN, "Transfer Information len %d\n", min_len);
    
    if(mode_dma) {
        dma_left = min_len;
        status &= ~STAT_TC;
        esp_do_dma();
    }else if(do_cmdvar){
        Log_Printf(LOG_WARN, "Command len: %i\n",cmdlen);
        data_len = 0;
        cmdlen = 0;
        do_cmdvar = 0;
        do_cmd();
        return;
    }
}

void esp_command_complete (void) {
    Log_Printf(LOG_WARN, "SCSI Command complete\n");
    if (data_len != 0)
        Log_Printf(LOG_WARN, "SCSI Command completed unexpectedly\n");
    data_len = 0;
    dma_left = 0;
    async_len = 0;
    
//    if(status) {
//        Log_Printf(LOG_WARN, "Command failed\n");
//    }
//  status = status; return status, not status register!!
    status = STAT_ST;
    esp_dma_done();
}

void write_response(void) {
    Log_Printf(LOG_WARN, "Transfer status: $%02x\n", SCSIcommand.returnCode);
    esp_fifo[0] = SCSIcommand.returnCode; // status
    esp_fifo[1] = 0x00; // message
    
    if(mode_dma == 1) {
    dma_memory_write(esp_fifo, 2, NEXTDMA_SCSI);
        status = STAT_TC | STAT_ST;
        intstatus = INTR_BS | INTR_FC;
        seqstep = SEQ_CD;
    } else {
        fifo_read_ptr = 0;
        fifo_write_ptr = 2;
        fifoflags = (fifoflags & 0xE0) | 2;
    }
    esp_raise_irq();
}