/*  Previous - iethernet.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

   Network adapter for the NEXT. 

   constants and struct netBSD mb8795reg.h file

*/

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "esp.h"
#include "sysReg.h"
#include "dma.h"
#include "scsi.h"
#include "ethernet.h"

#define LOG_EN_LEVEL LOG_WARN
#define IO_SEG_MASK	0x1FFFF


/* Function Prototypes */
void EnTx_Lower_IRQ(void);
void EnRx_Lower_IRQ(void);
void EnTx_Raise_IRQ(void);
void EnRx_Raise_IRQ(void);


/* Ethernet Regisers */
#define EN_TXSTAT   0
#define EN_TXMASK   1
#define EN_RXSTAT   2
#define EN_RXMASK   3
#define EN_TXMODE   4
#define EN_RXMODE   5

#define EN_RESET    6


/* Ethernet Register Constants */
#define TXSTAT_RDY          0x80    // Ready for Packet

#define RXSTAT_OK           0x80    // Packet received is correct

#define TXMODE_NOLOOP       0x02    // Loop back control disabled

#define RXMODE_ADDRSIZE     0x10    // Reduces NODE match to 5 chars
#define RXMODE_NOPACKETS    0x00    // Accept no packets
#define RXMODE_LIMITED      0x01    // Accept Broadcast/limited
#define RXMODE_NORMAL       0x02    // Accept Broadcast/multicasts
#define RXMODE_PROMISCUOUS  0x03    // Accept all packets
#define RXMODE_RESETENABLE  0x04    // One for reset packet enable

#define RESET_VAL           0x80    // Generate Reset

typedef struct {
    Uint8 tx_status;
    Uint8 tx_mask;
    Uint8 tx_mode;
    Uint8 rx_status;
    Uint8 rx_mask;
    Uint8 rx_mode;
} ETHERNET_CONTROLLER;

ETHERNET_CONTROLLER ethernet;

Uint8 ethernet_buffer[1600];


Uint8 MACaddress[6];

void Ethernet_Read(void) {
	Uint8 reg = IoAccessCurrentAddress&0x0F;
	Uint8 val;
    
	switch (reg) {
		case EN_TXSTAT:
			val = ethernet.tx_status;
			break;
        case EN_TXMASK:
            val = ethernet.tx_mask;
            break;
        case EN_RXSTAT:
            val = ethernet.rx_status;
            break;
        case EN_RXMASK:
            val = ethernet.rx_mask;
            break;
        case EN_TXMODE:
            val = ethernet.tx_mode;
            break;
        case EN_RXMODE:
            val = ethernet.rx_mode;
            break;
        case EN_RESET:
            break;
            
        /* Read MAC Address */
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            val = MACaddress[reg-8];
            break;
        default:
            break;
	}
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = val;
    
    Log_Printf(LOG_EN_LEVEL, "[Ethernet] read reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
}

void Ethernet_Write(void) {
	Uint8 val = IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
	Uint8 reg = IoAccessCurrentAddress&0xF;
    
    Log_Printf(LOG_EN_LEVEL, "[Ethernet] write reg %d val %02x PC=%x %s at %d",reg,val,m68k_getpc(),__FILE__,__LINE__);
    
    switch (reg) {
        case EN_TXSTAT:
            if (val == 0xFF)
                ethernet.tx_status = 0x80; // ? temp hack
            else
                ethernet.tx_status = val; //ethernet.tx_status & (0xF0 | ~val); ?
//            EnTx_Raise_IRQ(); // check irq
			break;
        case EN_TXMASK:
            ethernet.tx_mask = val & 0xAF;
//            EnTx_Raise_IRQ(); // check irq
            break;
        case EN_RXSTAT:
            if (val == 0xFF)
                ethernet.rx_status = 0x00;
            else
                ethernet.rx_status = val; //ethernet.rx_status & (0x07 | ~val); ?
//            EnRx_Raise_IRQ(); // check irq
            break;
        case EN_RXMASK:
            ethernet.rx_mask = val & 0x9F;
//            EnRx_Raise_IRQ(); // check irq
            break;
        case EN_TXMODE:
            ethernet.tx_mode = val;
            break;
        case EN_RXMODE:
            ethernet.rx_mode = val;
            break;
        case EN_RESET:
            if (val&RESET_VAL)
                Ethernet_Reset();
            break;

        /* Write MAC Address */
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            MACaddress[reg-8] = val;
            break;
            
        default:
            break;
    }
}


void Ethernet_Reset(void) {
    ethernet.tx_status = TXSTAT_RDY;
    ethernet.tx_mask = 0x00;
    ethernet.tx_mode = 0x00;
    ethernet.rx_status = 0x00;
    ethernet.rx_mask = 0x00;
    ethernet.rx_mode = 0x00;
    
    EnTx_Lower_IRQ();
    EnRx_Lower_IRQ();
    
    // txlen = rxlen = txcount = 0;
    // set_promisc(true);
    // start_send();
}

void Ethernet_Transmit(void) {
    Uint32 size;
    dma_memory_read(ethernet_buffer, &size, CHANNEL_EN_TX);
    ethernet.tx_status = TXSTAT_RDY;
    EnTx_Raise_IRQ();
    
    Log_Printf(LOG_EN_LEVEL, "Ethernet: Send packet to %02X:%02X:%02X:%02X:%02X:%02X",
               ethernet_buffer[0], ethernet_buffer[1], ethernet_buffer[2],
               ethernet_buffer[3], ethernet_buffer[4], ethernet_buffer[5]);
    
    if (!(ethernet.tx_mode&TXMODE_NOLOOP)) { // if loop back is not disabled
        Ethernet_Receive(ethernet_buffer, size); // loop back
    }
    
    // TODO: send packet to real network
}

void Ethernet_Receive(Uint8 *packet, Uint32 size) {
    if (Packet_Receiver_Me(packet)) {
        Log_Printf(LOG_EN_LEVEL, "Ethernet: Receive packet from %02X:%02X:%02X:%02X:%02X:%02X",
                   packet[6], packet[7], packet[8], packet[9], packet[10], packet[11]);
                   
        dma_memory_write(packet, size, CHANNEL_EN_RX); // loop back for experiment
        ethernet.rx_status |= RXSTAT_OK; // packet received is correct
        EnRx_Raise_IRQ();
    }
}


void EnTx_Lower_IRQ(void) {
    set_interrupt(INT_EN_TX, RELEASE_INT);
}

void EnRx_Lower_IRQ(void) {
    set_interrupt(INT_EN_RX, RELEASE_INT);
}

void EnTx_Raise_IRQ(void) {
    set_interrupt(INT_EN_TX, SET_INT);
}

void EnRx_Raise_IRQ(void) {
    set_interrupt(INT_EN_RX, SET_INT);
}


/* Functions to find out if we are intended to receive a packet */

bool Packet_Receiver_Me(Uint8 *packet) {
    switch (ethernet.rx_mode&0x03) {
        case RXMODE_NOPACKETS:
            return false;
            
        case RXMODE_NORMAL:
            if (Broadcast(packet) || Me(packet) || Local_Multicast(packet))
                return true;
            else
                return false;
            
        case RXMODE_LIMITED:
            if (Broadcast(packet) || Me(packet) || Multicast(packet))
                return true;
            else
                return false;
            
        case RXMODE_PROMISCUOUS:
            return true;
            
        default: return false;
    }
}

bool Multicast(Uint8 *packet) {
    if (packet[0]&0x01)
        return true;
    else 
        return false;
}

bool Local_Multicast(Uint8 *packet) {
    if (packet[0]&0x01 &&
        (packet[0]&0xFE) == MACaddress[0] //&&
        //        packet[1] == MACaddress[1] &&
        //        packet[2] == MACaddress[2]
        )
        return true;
    else 
        return false;
}

bool Me(Uint8 *packet) {
    if (packet[0] == MACaddress[0] &&
        //        packet[1] == MACaddress[1] &&
        //        packet[2] == MACaddress[2] &&
        //        packet[3] == MACaddress[3] &&
        packet[4] == MACaddress[4] &&
        (packet[5] == MACaddress[5] || (ethernet.rx_mode&RXMODE_ADDRSIZE)))
        return true;
    else 
        return false;
}

bool Broadcast(Uint8 *packet) {
    if (packet[0] == 0xFF &&
        //        packet[1] == 0xFF &&
        //        packet[2] == 0xFF &&
        //        packet[3] == 0xFF &&
        packet[4] == 0xFF &&
        packet[5] == 0xFF)
        return true;
    else 
        return false;
}
