#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "bmap.h"


/* NeXT bmap chip emulation */


uae_u32  NEXTbmap[16];

int bmap_tpe_select = 0;

uae_u32 bmap_get(uae_u32 addr);
void bmap_put(uae_u32 addr, uae_u32 val);


#define BMAP_DATA_RW    0xD

#define BMAP_TPE_RXSEL  0x80000000
#define BMAP_HEARTBEAT  0x20000000
#define BMAP_TPE_ILBC   0x10000000
#define BMAP_TPE        (BMAP_TPE_RXSEL|BMAP_TPE_ILBC)

uae_u32 bmap_lget(uaecptr addr) {
    uae_u32 l;
    
    if (addr&3) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        return 0;
    }
    
    l = bmap_get(addr>>2);

    return l;
}

uae_u32 bmap_wget(uaecptr addr) {
    uae_u32 w;
    int shift;
    
    if (addr&1) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        return 0;
    }

    shift = (2 - (addr&2)) * 8;
    
    w = bmap_get(addr>>2);
    w >>= shift;
    w &= 0xFFFF;
    
    return w;
}

uae_u32 bmap_bget(uaecptr addr) {
    uae_u32 b;
    int shift;
    
    shift = (3 - (addr&3)) * 8;
    
    b = bmap_get(addr>>2);
    b >>= shift;
    b &= 0xFF;
    
    return b;
}

void bmap_lput(uaecptr addr, uae_u32 l) {
    if (addr&3) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        return;
    }
    
    bmap_put(addr>>2, l);
}

void bmap_wput(uaecptr addr, uae_u32 w) {
    uae_u32 val;
    int shift;
    
    if (addr&1) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        return;
    }

    shift = (2 - (addr&2)) * 8;
    
    val = NEXTbmap[addr>>2];
    val &= ~(0xFFFF << shift);
    val |= w << shift;
    
    bmap_put(addr>>2, val);
}

void bmap_bput(uaecptr addr, uae_u32 b) {
    uae_u32 val;
    int shift;
    
    shift = (3 - (addr&3)) * 8;
    
    val = NEXTbmap[addr>>2];
    val &= ~(0xFF << shift);
    val |= b << shift;
    
    bmap_put(addr>>2, val);
}


uae_u32 bmap_get(uae_u32 bmap_reg) {
    uae_u32 val;
    
    switch (bmap_reg) {
        case BMAP_DATA_RW:
            /* This is for detecing thin wire ethernet.
             * It prevents from switching ethernet
             * transceiver to loopback mode.
             */
            val = NEXTbmap[BMAP_DATA_RW];
            
            if (ConfigureParams.Ethernet.bEthernetConnected && ConfigureParams.Ethernet.bTwistedPair) {
                val &= ~BMAP_HEARTBEAT;
            } else {
                val |= BMAP_HEARTBEAT;
            }
            break;
            
        default:
            val = NEXTbmap[bmap_reg];
            break;
    }
    
    return val;
}

void bmap_put(uae_u32 bmap_reg, uae_u32 val) {
    switch (bmap_reg) {
        case BMAP_DATA_RW:
            if ((val&BMAP_TPE) != (NEXTbmap[bmap_reg]&BMAP_TPE)) {
                if ((val&BMAP_TPE)==BMAP_TPE) {
                    Log_Printf(LOG_WARN, "[BMAP] Switching to twisted pair ethernet.");
                    bmap_tpe_select = 1;
                } else if ((val&BMAP_TPE)==0) {
                    Log_Printf(LOG_WARN, "[BMAP] Switching to thin ethernet.");
                    bmap_tpe_select = 0;
                }
            }
            break;
            
        default:
            break;
    }
    
    NEXTbmap[bmap_reg] = val;
}

void bmap_init(void) {
    int i;
    
    for (i = 0; i < 16; i++) {
        NEXTbmap[i] = 0;
    }
    bmap_tpe_select = 0;
}
