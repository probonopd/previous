/*  Previous - ethernet.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

   Network adapter for non-turbo NeXT machines.

*/

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "sysReg.h"
#include "dma.h"
#include "ethernet.h"
#include "cycInt.h"
#include "statusbar.h"

#define LOG_EN_LEVEL        LOG_WARN
#define LOG_EN_REG_LEVEL    LOG_WARN
#define IO_SEG_MASK	0x1FFFF


struct {
    Uint8 tx_status;
    Uint8 tx_mask;
    Uint8 tx_mode;
    Uint8 rx_status;
    Uint8 rx_mask;
    Uint8 rx_mode;
    Uint8 reset;
    
    Uint8 mac_addr[6];
} enet;

bool enet_stopped;

#define TXSTAT_READY        0x80    /* r */
#define TXSTAT_NET_BUSY     0x40    /* r */
#define TXSTAT_TX_RECVD     0x20    /* r */
#define TXSTAT_SHORTED      0x10    /* r */
#define TXSTAT_UNDERFLOW    0x08    /* rw */
#define TXSTAT_COLL         0x04    /* rw */
#define TXSTAT_16COLLS      0x02    /* rw */
#define TXSTAT_PAR_ERR      0x01    /* rw */

#define TXMASK_PKT_RDY      0x80
#define TXMASK_TX_RECVD     0x20
#define TXMASK_UNDERFLOW    0x08
#define TXMASK_COLL         0x04
#define TXMASK_16COLLS      0x02
#define TXMASK_PAR_ERR      0x01

#define RXSTAT_PKT_OK       0x80    /* rw */
#define RXSTAT_RESET_PKT    0x10    /* r */
#define RXSTAT_SHORT_PKT    0x08    /* rw */
#define RXSTAT_ALIGN_ERR    0x04    /* rw */
#define RXSTAT_CRC_ERR      0x02    /* rw */
#define RXSTAT_OVERFLOW     0x01    /* rw */

#define RXMASK_PKT_OK       0x80
#define RXMASK_RESET_PKT    0x10
#define RXMASK_SHORT_PKT    0x80
#define RXMASK_ALIGN_ERR    0x40
#define RXMASK_CRC_ERR      0x20
#define RXMASK_OVERFLOW     0x10

#define TXMODE_COLL_ATMPT   0xF0    /* r */
#define TXMODE_IGNORE_PAR   0x08    /* rw */
#define TXMODE_TM           0x04    /* rw */
#define TXMODE_DIS_LOOP     0x02    /* rw */
#define TXMODE_DIS_CONTNT   0x01    /* rw */

#define RXMODE_TEST_CRC     0x80
#define RXMODE_ADDR_SIZE    0x10
#define RXMODE_ENA_SHORT    0x08
#define RXMODE_ENA_RST      0x04
#define RXMODE_MATCH_MODE   0x03

#define RX_NOPACKETS        0   // Accept no packets
#define RX_LIMITED          1   // Accept broadcast/limited
#define RX_NORMAL           2   // Accept broadcast/multicast
#define RX_PROMISCUOUS      3   // Accept all packets

#define EN_RESET            0x80    /* w */


void enet_reset(void);


void EN_TX_Status_Read(void) { // 0x02006000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.tx_status;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_TX_Status_Write(void) {
    Uint8 val=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    enet.tx_status&=~(val&0x0F);
    
    if ((enet.tx_status&enet.tx_mask&0x0F)==0 || (enet.tx_status&enet.tx_mask&0x0F)==TXSTAT_READY) {
        set_interrupt(INT_EN_TX, RELEASE_INT);
    }
}

void EN_TX_Mask_Read(void) { // 0x02006001
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.tx_mask&0xAF;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter masks read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_TX_Mask_Write(void) {
    enet.tx_mask=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter masks write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if ((enet.tx_status&enet.tx_mask&0x0F)==0 || (enet.tx_status&enet.tx_mask&0x0F)==TXSTAT_READY) {
        set_interrupt(INT_EN_TX, RELEASE_INT);
    }
}

void EN_RX_Status_Read(void) { // 0x02006002
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.rx_status;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_RX_Status_Write(void) {
    Uint8 val=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    enet.rx_status&=~(val&0x8F);
    
    if ((enet.rx_status&enet.rx_mask&0x8F)==0) {
        set_interrupt(INT_EN_RX, RELEASE_INT);
    }
}

void EN_RX_Mask_Read(void) { // 0x02006003
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.rx_mask&0x9F;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver masks read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_RX_Mask_Write(void) {
    enet.rx_mask=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver masks write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    
    if ((enet.rx_status&enet.rx_mask&0x8F)==0) {
        set_interrupt(INT_EN_RX, RELEASE_INT);
    }
}

void EN_TX_Mode_Read(void) { // 0x02006004
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.tx_mode;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter mode read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_TX_Mode_Write(void) {
    enet.tx_mode=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter mode write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_RX_Mode_Read(void) { // 0x02006005
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.rx_mode;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver mode read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_RX_Mode_Write(void) {
    enet.rx_mode=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver mode write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_Reset_Write(void) { // 0x02006006
    enet.reset=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Reset write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
    enet_reset();
}

void EN_NodeID0_Read(void) { // 0x02006008
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[0];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 0 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID0_Write(void) {
    enet.mac_addr[0]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 0 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID1_Read(void) { // 0x02006009
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[1];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 1 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID1_Write(void) {
    enet.mac_addr[1]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 1 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID2_Read(void) { // 0x0200600a
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[2];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 2 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID2_Write(void) {
    enet.mac_addr[2]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 2 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID3_Read(void) { // 0x0200600b
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[3];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 3 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID3_Write(void) {
    enet.mac_addr[3]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 3 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID4_Read(void) { // 0x0200600c
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[4];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 4 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID4_Write(void) {
    enet.mac_addr[4]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 4 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID5_Read(void) { // 0x0200600d
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.mac_addr[5];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 5 read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_NodeID5_Write(void) {
    enet.mac_addr[5]=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] MAC byte 5 write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_CounterLo_Read(void) { // 0x02006007
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = ((enet_tx_buffer.limit-enet_tx_buffer.size)*8)&0xFF; /* FIXME: counter value */
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver mode read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_CounterHi_Read(void) { // 0x0200600f
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = (((enet_tx_buffer.limit-enet_tx_buffer.size)*8)>>8)&0x3F; /* FIXME: counter value */
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Receiver mode read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void enet_tx_interrupt(Uint8 intr) {
    enet.tx_status|=intr;
    if (enet.tx_status&enet.tx_mask) {
        set_interrupt(INT_EN_TX, SET_INT);
    }
}

void enet_rx_interrupt(Uint8 intr) {
    enet.rx_status|=intr;
    if (enet.rx_status&enet.rx_mask) {
        set_interrupt(INT_EN_RX, SET_INT);
    }
}

/* Functions to find out if we are intended to receive a packet */
bool recv_multicast(Uint8 *packet) {
    if (packet[0]&0x01)
        return true;
    else
        return false;
}

bool recv_local_multicast(Uint8 *packet) {
    if (packet[0]&0x01 &&
        (packet[0]&0xFE) == enet.mac_addr[0] &&
        packet[1] == enet.mac_addr[1] &&
        packet[2] == enet.mac_addr[2])
        return true;
    else
        return false;
}

bool recv_me(Uint8 *packet) {
    if (packet[0] == enet.mac_addr[0] &&
        packet[1] == enet.mac_addr[1] &&
        packet[2] == enet.mac_addr[2] &&
        packet[3] == enet.mac_addr[3] &&
        packet[4] == enet.mac_addr[4] &&
        (packet[5] == enet.mac_addr[5] || (enet.rx_mode&RXMODE_ADDR_SIZE)))
        return true;
    else
        return false;
}

bool recv_broadcast(Uint8 *packet) {
    if (packet[0] == 0xFF &&
        packet[1] == 0xFF &&
        packet[2] == 0xFF &&
        packet[3] == 0xFF &&
        packet[4] == 0xFF &&
        packet[5] == 0xFF)
        return true;
    else
        return false;
}

bool enet_packet_for_me(Uint8 *packet) {
    switch (enet.rx_mode&RXMODE_MATCH_MODE) {
        case RX_NOPACKETS:
            return false;
            
        case RX_NORMAL:
            if (recv_broadcast(packet) || recv_me(packet) || recv_local_multicast(packet))
                return true;
            else
                return false;
            
        case RX_LIMITED:
            if (recv_broadcast(packet) || recv_me(packet) || recv_multicast(packet))
                return true;
            else
                return false;
            
        case RX_PROMISCUOUS:
            return true;
            
        default: return false;
    }
}

bool enet_packet_from_me(Uint8 *packet) {
    if (packet[6] == enet.mac_addr[0] &&
        packet[7] == enet.mac_addr[1] &&
        packet[8] == enet.mac_addr[2] &&
        packet[9] == enet.mac_addr[3] &&
        packet[10] == enet.mac_addr[4] &&
        packet[11] == enet.mac_addr[5])
        return true;
    else
        return false;
}

void print_buf(Uint8 *buf, Uint32 size) {
    int i;
    for (i=0; i<size; i++) {
        if (i==14 || (i-14)%16==0) {
            printf("\n");
        }
        printf("%02X ",buf[i]);
    }
    printf("\n");
}


#define ENET_FRAMESIZE_MIN  64      /* 46 byte data and 14 byte header, 4 byte CRC */
#define ENET_FRAMESIZE_MAX  1518    /* 1500 byte data and 14 byte header, 4 byte CRC */

/* Ethernet periodic check */
#define ENET_IO_DELAY   50000
#define ENET_IO_SHORT   250

enum {
    RECV_STATE_WAITING,
    RECV_STATE_RECEIVING
} receiver_state;

enum {
    TMIT_STATE_WAITING,
    TMIT_STATE_TRANSMITTING
} transmitter_state;

/* TODO: Add support for DMA chaining of packets */

void ENET_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    
    if (enet.reset&EN_RESET) {
        Log_Printf(LOG_WARN, "Stopping Ethernet Transmitter/Receiver");
        enet_stopped=true;
        return;
    }
    
    /* Receive packet */
    switch (receiver_state) {
        case RECV_STATE_WAITING:
            /* TODO: Receive from real network! */
            if (enet_rx_buffer.size>0 && enet_packet_for_me(enet_rx_buffer.data)) {
                Log_Printf(LOG_EN_LEVEL, "[EN] Receiving packet from %02X:%02X:%02X:%02X:%02X:%02X",
                           enet_rx_buffer.data[6], enet_rx_buffer.data[7], enet_rx_buffer.data[8],
                           enet_rx_buffer.data[9], enet_rx_buffer.data[10], enet_rx_buffer.data[11]);
                print_buf(enet_rx_buffer.data, enet_rx_buffer.size);
                enet_rx_buffer.size+=4;
                enet_rx_buffer.limit+=4;
                receiver_state = RECV_STATE_RECEIVING;
                /* Fall through to receiving state */
            } else
                break;
        case RECV_STATE_RECEIVING:
            Statusbar_BlinkLed(DEVICE_LED_ENET);
            enet.rx_status&=~RXSTAT_PKT_OK;
            if (enet_rx_buffer.size>=ENET_FRAMESIZE_MIN || enet.rx_mode&RXMODE_ENA_SHORT) {
                dma_enet_write_memory();
                if (enet_rx_buffer.size>0) {
                    Log_Printf(LOG_EN_LEVEL, "[EN] Receiving packet: Transfer not complete!");
                    enet_rx_buffer.size=0;                  /* HACK: */
                    receiver_state = RECV_STATE_WAITING;    /* HACK: prevent loop until chaining is implemented */
                    break; /* Loop in receiving state */
                } else {
                    Log_Printf(LOG_EN_LEVEL, "[EN] Receiving packet: Transfer complete.");
                    enet_rx_interrupt(RXSTAT_PKT_OK);
                    if (enet_packet_from_me(enet_rx_buffer.data)) {
                        enet_tx_interrupt(TXSTAT_TX_RECVD);
                    }
                }
            } else {
                Log_Printf(LOG_EN_LEVEL, "[EN] Received packet is short (%i byte)",enet_rx_buffer.size);
                enet_rx_interrupt(RXSTAT_SHORT_PKT);
            }
            receiver_state = RECV_STATE_WAITING;
            break;
        default:
            break;
    }

    /* Send packet */
    if (enet.tx_status&TXSTAT_READY) {
        dma_enet_read_memory();
        if (enet_tx_buffer.size>15) {
            Statusbar_BlinkLed(DEVICE_LED_ENET);
            enet_tx_buffer.size-=15;
            Log_Printf(LOG_EN_LEVEL, "[EN] Sending packet to %02X:%02X:%02X:%02X:%02X:%02X",
                       enet_tx_buffer.data[0], enet_tx_buffer.data[1], enet_tx_buffer.data[2],
                       enet_tx_buffer.data[3], enet_tx_buffer.data[4], enet_tx_buffer.data[5]);
            print_buf(enet_tx_buffer.data, enet_tx_buffer.size);
            if (enet.tx_mode&TXMODE_DIS_LOOP) {
                /* TODO: Send to real network! */
                enet_tx_buffer.size=0;
            } else {
                /* Loop back */
                memcpy(enet_rx_buffer.data, enet_tx_buffer.data, enet_tx_buffer.size);
                enet_rx_buffer.size=enet_rx_buffer.limit=enet_tx_buffer.size;
                enet_tx_buffer.size=0;
                //enet_tx_interrupt(TXSTAT_TX_RECVD);
            }
        }
    }
    //CycInt_AddRelativeInterrupt(ENET_IO_DELAY, INT_CPU_CYCLE, INTERRUPT_ENET_IO);
    CycInt_AddRelativeInterrupt(receiver_state==RECV_STATE_WAITING?ENET_IO_DELAY:ENET_IO_SHORT, INT_CPU_CYCLE, INTERRUPT_ENET_IO);
}

void enet_reset(void) {
    if (enet.reset&EN_RESET) {
        enet.tx_status=TXSTAT_READY;
    } else if (enet_stopped==true) {
        Log_Printf(LOG_WARN, "Starting Ethernet Transmitter/Receiver");
        enet_stopped=false;
        CycInt_AddRelativeInterrupt(ENET_IO_DELAY, INT_CPU_CYCLE, INTERRUPT_ENET_IO);
    }
}

void Ethernet_Reset(void) {
    enet.reset=EN_RESET;
    enet_stopped=true;
    
    enet_rx_buffer.size=enet_tx_buffer.size=0;
    enet_rx_buffer.limit=enet_tx_buffer.limit=2048;
}
