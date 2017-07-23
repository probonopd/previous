/*  Previous - ethernet.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

   Network adapter for non-turbo and turbo NeXT machines.

*/

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "sysReg.h"
#include "dma.h"
#include "bmap.h"
#include "ethernet.h"
#include "enet_slirp.h"
#include "cycInt.h"
#include "statusbar.h"


#define LOG_EN_LEVEL        LOG_DEBUG
#define LOG_EN_REG_LEVEL    LOG_DEBUG
#define LOG_EN_DATA 0

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

#define EN_RESET            0x80    /* w */


void enet_reset(void);


void EN_TX_Status_Read(void) { // 0x02006000
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = enet.tx_status;
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter status read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_TX_Status_Write(void) {
    Uint8 val=IoMem[IoAccessCurrentAddress & IO_SEG_MASK];
 	Log_Printf(LOG_EN_REG_LEVEL,"[EN] Transmitter status write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
	if (ConfigureParams.System.bTurbo) {
		enet.tx_status&=~val;
	} else {
		enet.tx_status&=~(val&0x0F);
	}
	
    if ((enet.tx_status&enet.tx_mask&0x0F)==0) {
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
    
    if ((enet.tx_status&enet.tx_mask&0x0F)==0) {
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

static void enet_tx_interrupt(Uint8 intr) {
    enet.tx_status|=intr;
    if (enet.tx_status&enet.tx_mask) {
        set_interrupt(INT_EN_TX, SET_INT);
    }
}

static void enet_rx_interrupt(Uint8 intr) {
    enet.rx_status|=intr;
    if (enet.rx_status&enet.rx_mask) {
        set_interrupt(INT_EN_RX, SET_INT);
    }
}

/* Functions to find out if we are intended to receive a packet */

/* Non-turbo */
#define RX_NOPACKETS        0   // Accept no packets
#define RX_LIMITED          1   // Accept broadcast/limited
#define RX_NORMAL           2   // Accept broadcast/multicast
#define RX_PROMISCUOUS      3   // Accept all packets

/* Turbo */
#define RX_ENABLED      0x80    // Accept packets
#define RX_ANY          0x01    // Accept any packets
#define RX_OWN          0x02    // Accept own packets

static bool recv_multicast(Uint8 *packet) {
    if (packet[0]&0x01)
        return true;
    else
        return false;
}

static bool recv_local_multicast(Uint8 *packet) {
    if (packet[0]&0x01 &&
        (packet[0]&0xFE) == enet.mac_addr[0] &&
        packet[1] == enet.mac_addr[1] &&
        packet[2] == enet.mac_addr[2])
        return true;
    else
        return false;
}

static bool recv_me(Uint8 *packet) {
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

static bool recv_me_turbo(Uint8 *packet) {
    if (packet[0] == enet.mac_addr[0] &&
        packet[1] == enet.mac_addr[1] &&
        packet[2] == enet.mac_addr[2] &&
        packet[3] == enet.mac_addr[3] &&
        packet[4] == enet.mac_addr[4] &&
        packet[5] == enet.mac_addr[5])
        return true;
    else
        return false;
}

static bool recv_broadcast(Uint8 *packet) {
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

static bool enet_packet_for_me(Uint8 *packet) {
    
    if (ConfigureParams.System.bTurbo) {
        if (enet.rx_mode&RX_ENABLED) {
            if (enet.rx_mode&RX_ANY) {
                return true;
            } else if (enet.rx_mode&RX_OWN) {
                if (recv_broadcast(packet) || recv_me_turbo(packet)) {
                    return true;
                }
            } else {
                if (recv_broadcast(packet)) {
                    return true;
                }
            }
        }
        return false;
    }
    
    switch (enet.rx_mode&RXMODE_MATCH_MODE) {
        case RX_NOPACKETS:
            return false;
            
        case RX_LIMITED:
            if (recv_broadcast(packet) || recv_me(packet) || recv_local_multicast(packet))
                return true;
            else
                return false;
            
        case RX_NORMAL:
            if (recv_broadcast(packet) || recv_me(packet) || recv_multicast(packet))
                return true;
            else
                return false;
            
        case RX_PROMISCUOUS:
            return true;
            
        default: return false;
    }
}

void enet_receive(Uint8 *pkt, int len) {
    if (enet_packet_for_me(pkt)) {
#if 1   /* Hack for short packets from SLIRP */
        if (len<60) {
            Log_Printf(LOG_WARN, "[EN] HACK: short packet received (%i byte). Fixed.", len);
            len = 60;
        }
#endif
        memcpy(enet_rx_buffer.data,pkt,len);
        enet_rx_buffer.size=enet_rx_buffer.limit=len;
		enet.tx_status |= TXSTAT_NET_BUSY;
    } else {
        Log_Printf(LOG_WARN, "[EN] Packet is not for me.");
    }
}

static void print_buf(Uint8 *buf, Uint32 size) {
#if LOG_EN_DATA
    int i;
    for (i=0; i<size; i++) {
        if (i==14 || (i-14)%16==0) {
            printf("\n");
        }
        printf("%02X ",buf[i]);
    }
    printf("\n");
#endif
}


#define ENET_FRAMESIZE_MIN  64      /* 46 byte data and 14 byte header, 4 byte CRC */
#define ENET_FRAMESIZE_MAX  1518    /* 1500 byte data and 14 byte header, 4 byte CRC */

/* Ethernet periodic check */
#define ENET_IO_DELAY   500     /* use 500 for NeXT hardware test, 20 for status test */
#define ENET_IO_SHORT   40      /* use 40 for 68030 hardware test */

enum {
    RECV_STATE_WAITING,
    RECV_STATE_RECEIVING
} receiver_state;

bool tx_done;
bool rx_chain;
int old_size;
int en_state;

#define EN_DISCONNECTED	0
#define EN_LOOPBACK		1
#define EN_THINWIRE		2
#define EN_TWISTEDPAIR	3

/* Fujitsu ethernet controller */
static int enet_state(void) {
	if (ConfigureParams.System.nMachineType == NEXT_CUBE030) {
		if (enet.tx_mode&TXMODE_DIS_LOOP) {
			if (ConfigureParams.Ethernet.bEthernetConnected) {
				return EN_THINWIRE;
			}
		} else {
			return EN_LOOPBACK;
		}
	} else if (bmap_tpe_select) {
		if (ConfigureParams.Ethernet.bEthernetConnected) {
			if (ConfigureParams.Ethernet.bTwistedPair) {
				return EN_TWISTEDPAIR;
			}
		}
	} else {
		if (enet.tx_mode&TXMODE_DIS_LOOP) {
			if (ConfigureParams.Ethernet.bEthernetConnected) {
				if (!ConfigureParams.Ethernet.bTwistedPair) {
					return EN_THINWIRE;
				}
			}
		} else {
			return EN_LOOPBACK;
		}
	}
	return EN_DISCONNECTED;
}

static void enet_io(void) {
	
	en_state = enet_state();
	
	/* Receive packet */
	switch (receiver_state) {
		case RECV_STATE_WAITING:
			if (enet_rx_buffer.size>0) {
				Statusbar_BlinkLed(DEVICE_LED_ENET);
				Log_Printf(LOG_EN_LEVEL, "[EN] Receiving packet from %02X:%02X:%02X:%02X:%02X:%02X",
						   enet_rx_buffer.data[6], enet_rx_buffer.data[7], enet_rx_buffer.data[8],
						   enet_rx_buffer.data[9], enet_rx_buffer.data[10], enet_rx_buffer.data[11]);
				print_buf(enet_rx_buffer.data, enet_rx_buffer.size);
				enet_rx_buffer.size+=4;
				enet_rx_buffer.limit+=4;
				rx_chain = false;
				enet.rx_status&=~RXSTAT_PKT_OK;
				if (enet_rx_buffer.size<ENET_FRAMESIZE_MIN && !(enet.rx_mode&RXMODE_ENA_SHORT)) {
					Log_Printf(LOG_WARN, "[EN] Received packet is short (%i byte)",enet_rx_buffer.size);
					enet_rx_interrupt(RXSTAT_SHORT_PKT);
					enet_rx_buffer.size = 0;
					enet.tx_status &= ~TXSTAT_NET_BUSY;
					break; /* Keep on waiting for a good packet */
				} else /* Fall through to receiving state */
					receiver_state = RECV_STATE_RECEIVING;
			} else if (en_state == EN_THINWIRE || en_state == EN_TWISTEDPAIR) {
				/* Receive from real world network */
				enet_slirp_queue_poll();
				break;
			} else
				break;
		case RECV_STATE_RECEIVING:
			if (enet_rx_buffer.size>0) {
				old_size = enet_rx_buffer.size;
				dma_enet_write_memory(rx_chain);
				if (enet_rx_buffer.size==old_size) {
					Log_Printf(LOG_WARN, "[EN] Receiving packet: Error! Receiver overflow (DMA disabled)!");
					enet_rx_interrupt(RXSTAT_OVERFLOW);
					rx_chain = false;
					enet_rx_buffer.size = 0;
					enet.tx_status &= ~TXSTAT_NET_BUSY;
					receiver_state = RECV_STATE_WAITING;
					break; /* Go back to waiting state */
				}
				if (enet_rx_buffer.size>0) {
					Log_Printf(LOG_WARN, "[EN] Receiving packet: Transfer not complete!");
					rx_chain = true;
					break; /* Loop in receiving state */
				} else { /* done */
					Log_Printf(LOG_EN_LEVEL, "[EN] Receiving packet: Transfer complete.");
					rx_chain = false;
					enet_rx_interrupt(RXSTAT_PKT_OK);
					if (en_state == EN_LOOPBACK) { /* same for thin wire loopback? */
						enet_tx_interrupt(TXSTAT_TX_RECVD);
					}
					enet.tx_status &= ~TXSTAT_NET_BUSY;
					receiver_state = RECV_STATE_WAITING;
				}
			}
			break;
			
		default:
			break;
	}
	
	/* Send packet */
	if (enet.tx_status&TXSTAT_READY) {
		if (en_state != EN_DISCONNECTED) {
			if (enet.tx_status&TXSTAT_NET_BUSY) {
				/* Wait until network is free */
				Log_Printf(LOG_WARN, "[EN] Network is busy. Transmission delayed.");
			} else {
				old_size = enet_tx_buffer.size;
				tx_done=dma_enet_read_memory();
				if (enet_tx_buffer.size>0) {
					enet.tx_status &= ~TXSTAT_TX_RECVD;
					if (enet_tx_buffer.size==old_size && !tx_done) {
						Log_Printf(LOG_WARN, "[EN] Sending packet: Error! Transmitter underflow (no EOP)!");
						enet_tx_interrupt(TXSTAT_UNDERFLOW);
						enet_tx_buffer.size=0;
					} else if (enet_tx_buffer.size>15) {
						enet_tx_buffer.size-=15;
					} else if (tx_done) {
						Log_Printf(LOG_WARN, "[EN] Transmitter error: Early EOP!");
						enet_tx_buffer.size=0;
						tx_done = false;
					}
				}
				if (tx_done) {
					Statusbar_BlinkLed(DEVICE_LED_ENET);
					Log_Printf(LOG_EN_LEVEL, "[EN] Sending packet to %02X:%02X:%02X:%02X:%02X:%02X",
							   enet_tx_buffer.data[0], enet_tx_buffer.data[1], enet_tx_buffer.data[2],
							   enet_tx_buffer.data[3], enet_tx_buffer.data[4], enet_tx_buffer.data[5]);
					print_buf(enet_tx_buffer.data, enet_tx_buffer.size);
					if (en_state == EN_LOOPBACK) {
						/* Loop back */
						Log_Printf(LOG_WARN, "[EN] Loopback packet.");
						enet_receive(enet_tx_buffer.data, enet_tx_buffer.size);
					} else {
						/* Send to real world network */
						enet_slirp_input(enet_tx_buffer.data,enet_tx_buffer.size);
						/* Simultaneously receive packet on thin ethernet */
						if (en_state == EN_THINWIRE) {
							enet_receive(enet_tx_buffer.data, enet_tx_buffer.size);
						}
					}
					enet_tx_buffer.size=0;
				}
			}
		}
	}
}

/* AT&T ethernet controller for turbo systems */
#define TXMODE_ENABLE	0x80
#define RXMODE_ENABLE	0x80
#define TXMODE_LOOP     0x02
#define TXMODE_TPE      0x04
#define ENCTRL_TPE      0x40

void EN_Control_Read(void) { // 0x02006006
    Uint8 val = enet.reset;
    if (ConfigureParams.Ethernet.bEthernetConnected && ConfigureParams.Ethernet.bTwistedPair) {
        val &= ~ENCTRL_TPE;
    } else {
        val |= ENCTRL_TPE;
    }
    IoMem[IoAccessCurrentAddress & IO_SEG_MASK] = val;
	Log_Printf(LOG_EN_REG_LEVEL,"[newEN] Control read at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
}

void EN_Control_Write(void) {
	enet.reset=(IoMem[IoAccessCurrentAddress & IO_SEG_MASK])&EN_RESET;
	Log_Printf(LOG_EN_REG_LEVEL,"[newEN] Control write at $%08x val=$%02x PC=$%08x\n", IoAccessCurrentAddress, IoMem[IoAccessCurrentAddress & IO_SEG_MASK], m68k_getpc());
	enet_reset();
}

static int new_enet_state(void) {
	if (enet.tx_mode&TXMODE_LOOP) {
		return EN_LOOPBACK;
	} else if (ConfigureParams.Ethernet.bEthernetConnected) {
		if (enet.tx_mode&TXMODE_TPE) {
			if (ConfigureParams.Ethernet.bTwistedPair) {
				return EN_TWISTEDPAIR;
			}
		} else {
			if (!ConfigureParams.Ethernet.bTwistedPair) {
				return EN_THINWIRE;
			}
		}
	}
	return EN_DISCONNECTED;
}

static void new_enet_io(void) {
	
	en_state = new_enet_state();
	
	/* Receive packet */
	switch (receiver_state) {
		case RECV_STATE_WAITING:
			if (enet_rx_buffer.size>0) {
				Statusbar_BlinkLed(DEVICE_LED_ENET);
				Log_Printf(LOG_EN_LEVEL, "[newEN] Receiving packet from %02X:%02X:%02X:%02X:%02X:%02X",
						   enet_rx_buffer.data[6], enet_rx_buffer.data[7], enet_rx_buffer.data[8],
						   enet_rx_buffer.data[9], enet_rx_buffer.data[10], enet_rx_buffer.data[11]);
				print_buf(enet_rx_buffer.data, enet_rx_buffer.size);
				enet_rx_buffer.size+=4;
				enet_rx_buffer.limit+=4;
				rx_chain = false;
				enet.rx_status&=~RXSTAT_PKT_OK;
				if (enet_rx_buffer.size<ENET_FRAMESIZE_MIN && !(enet.rx_mode&RXMODE_ENA_SHORT)) {
					Log_Printf(LOG_WARN, "[newEN] Received packet is short (%i byte)",enet_rx_buffer.size);
					enet_rx_interrupt(RXSTAT_SHORT_PKT);
					enet_rx_buffer.size = 0;
					enet.tx_status &= ~TXSTAT_NET_BUSY;
					break; /* Keep on waiting for a good packet */
				} else /* Fall through to receiving state */
					receiver_state = RECV_STATE_RECEIVING;
			} else if (en_state == EN_THINWIRE || en_state == EN_TWISTEDPAIR) {
				/* Receive from real world network */
				enet_slirp_queue_poll();
				break;
			} else
				break;
		case RECV_STATE_RECEIVING:
			if (enet_rx_buffer.size>0) {
				old_size = enet_rx_buffer.size;
				dma_enet_write_memory(rx_chain);
				if (enet_rx_buffer.size==old_size) {
					Log_Printf(LOG_WARN, "[newEN] Receiving packet: Error! Receiver overflow (DMA disabled)!");
					enet_rx_interrupt(RXSTAT_OVERFLOW);
					rx_chain = false;
					enet_rx_buffer.size = 0;
					enet.tx_status &= ~TXSTAT_NET_BUSY;
					receiver_state = RECV_STATE_WAITING;
					break; /* Go back to waiting state */
				}
				if (enet_rx_buffer.size>0) {
					Log_Printf(LOG_WARN, "[newEN] Receiving packet: Transfer not complete!");
					rx_chain = true;
					break; /* Loop in receiving state */
				} else { /* done */
					Log_Printf(LOG_EN_LEVEL, "[newEN] Receiving packet: Transfer complete.");
					rx_chain = false;
					enet_rx_interrupt(RXSTAT_PKT_OK);
					if (en_state == EN_LOOPBACK) {
						enet_tx_interrupt(TXSTAT_TX_RECVD);
					}
					enet.tx_status &= ~TXSTAT_NET_BUSY;
					receiver_state = RECV_STATE_WAITING;
				}
			}
			break;
			
		default:
			break;
	}
	
	/* Send packet */
	if (enet.tx_mode&TXMODE_ENABLE) {
		if (en_state != EN_DISCONNECTED) {
			if (enet.tx_status&TXSTAT_NET_BUSY) {
				/* Wait until network is free */
				Log_Printf(LOG_WARN, "[EN] Network is busy. Transmission delayed.");
			} else {
				dma_enet_read_memory();
				if (enet_tx_buffer.size>0) {
					Statusbar_BlinkLed(DEVICE_LED_ENET);
					Log_Printf(LOG_EN_LEVEL, "[newEN] Sending packet to %02X:%02X:%02X:%02X:%02X:%02X",
							   enet_tx_buffer.data[0], enet_tx_buffer.data[1], enet_tx_buffer.data[2],
							   enet_tx_buffer.data[3], enet_tx_buffer.data[4], enet_tx_buffer.data[5]);
					print_buf(enet_tx_buffer.data, enet_tx_buffer.size);
					enet.tx_status &= ~TXSTAT_TX_RECVD;
					if (en_state == EN_LOOPBACK) {
						/* Loop back */
						Log_Printf(LOG_WARN, "[newEN] Loopback packet.");
						enet_receive(enet_tx_buffer.data, enet_tx_buffer.size);
					} else {
						/* Send to real world network */
						enet_slirp_input(enet_tx_buffer.data,enet_tx_buffer.size);
						/* Simultaneously receive packet on thin ethernet */
						if (en_state == EN_THINWIRE) {
							enet_receive(enet_tx_buffer.data, enet_tx_buffer.size);
						}
					}
					enet_tx_buffer.size=0;
					enet_tx_interrupt(TXSTAT_READY);
				}
			}
		} else { /* disconnected - strange, but required by ROM and 2.2 kernel */
			if (ConfigureParams.Ethernet.bEthernetConnected) {
				if (!ConfigureParams.Ethernet.bTwistedPair) {
					enet_tx_interrupt(TXSTAT_READY);
				}
			}
		}
		enet.tx_status |= TXSTAT_READY; /* really? */
	}
}

void ENET_IO_Handler(void) {
	CycInt_AcknowledgeInterrupt();
	
	if (enet.reset&EN_RESET) {
		Log_Printf(LOG_WARN, "Stopping Ethernet Transmitter/Receiver");
		enet_stopped=true;
		/* Stop SLIRP */
		if (ConfigureParams.Ethernet.bEthernetConnected) {
			enet_slirp_stop();
		}
		return;
	}
	
	if (ConfigureParams.System.bTurbo) {
		new_enet_io();
	} else {
		enet_io();
	}
	
	CycInt_AddRelativeInterruptUs(receiver_state==RECV_STATE_WAITING?ENET_IO_DELAY:ENET_IO_SHORT, 0, INTERRUPT_ENET_IO);
}

void enet_reset(void) {
    if (enet.reset&EN_RESET) {
        enet.tx_status=ConfigureParams.System.bTurbo?0:TXSTAT_READY;
    } else if (enet_stopped==true) {
        Log_Printf(LOG_WARN, "Starting Ethernet Transmitter/Receiver");
        enet_stopped=false;
        CycInt_AddRelativeInterruptUs(ENET_IO_DELAY, 0, INTERRUPT_ENET_IO);
        /* Start SLIRP */
        if (ConfigureParams.Ethernet.bEthernetConnected) {
            enet_slirp_start();
        }
    }
}

void Ethernet_Reset(bool hard) {
    if (hard) {
        enet.reset=EN_RESET;
        enet_stopped=true;
        enet_rx_buffer.size=enet_tx_buffer.size=0;
        enet_rx_buffer.limit=enet_tx_buffer.limit=64*1024;
        enet.tx_status=ConfigureParams.System.bTurbo?0:TXSTAT_READY;
        /* Stop SLIRP */
        enet_slirp_stop();
    } else {
        if (ConfigureParams.Ethernet.bEthernetConnected && !(enet.reset&EN_RESET)) {
            /* Start SLIRP */
            enet_slirp_start();
        } else {
            /* Stop SLIRP */
            enet_slirp_stop();
        }
    }
}
