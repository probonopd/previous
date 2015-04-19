#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "bmap.h"


/* NeXT bmap chip emulation */


uae_u32  NEXTbmap[16];

uae_u32 bmap_get(uae_u32 addr);
void bmap_put(uae_u32 addr, uae_u32 val);


#define BMAP_DATA_RW    0xD

#define BMAP_TP         0x90000000

uae_u32 bmap_lget(uaecptr addr) {
    if (addr%4) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        abort();
    }

    return bmap_get(addr/4);
}

uae_u32 bmap_wget(uaecptr addr) {
    uae_u32 w = 0;
    uae_u32 val = bmap_get(addr/4);
    
    switch (addr%4) {
        case 0:
            w = val>>16;
            break;
        case 2:
            w = val;
            break;
        default:
            Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
            abort();
            break;
    }
    
    return w;
}

uae_u32 bmap_bget(uaecptr addr) {
    uae_u32 b = 0;
    uae_u32 val = bmap_get(addr/4);
    
    switch (addr%4) {
        case 0:
            b = val>>24;
            break;
        case 1:
            b = val>>16;
            break;
        case 2:
            b = val>>8;
            break;
        case 3:
            b = val;
            break;
        default:
            break;
    }
    
    return b;
}

void bmap_lput(uaecptr addr, uae_u32 l) {
    if (addr%4) {
        Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
        abort();
    }
        bmap_put(addr/4, l);
}

void bmap_wput(uaecptr addr, uae_u32 w) {
    uae_u32 val;
    
    switch (addr%4) {
        case 0:
            val = w<<16;
            break;
        case 2:
            val = w;
            break;
        default:
            Log_Printf(LOG_WARN, "[BMAP] Unaligned access.");
            abort();
            break;
    }
    
    bmap_put(addr/4, val);
}

void bmap_bput(uaecptr addr, uae_u32 b) {
    uae_u32 val = 0;
    
    switch (addr%4) {
        case 0:
            val = b<<24;
            break;
        case 1:
            val = b<<16;
            break;
        case 2:
            val = b<<8;
            break;
        case 3:
            val = b;
            break;
        default:
            break;
    }
    
    bmap_put(addr/4, val);
}


uae_u32 bmap_get(uae_u32 bmap_reg) {
    uae_u32 val;
    
    switch (bmap_reg) {
        case BMAP_DATA_RW:
            /* This is for detecing thin wire ethernet.
             * It prevents from switching ethernet
             * transceiver to loopback mode.
             */
            val = NEXTbmap[BMAP_DATA_RW]|0x20000000;
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
            if ((val&BMAP_TP) != (NEXTbmap[bmap_reg]&BMAP_TP)) {
                if ((val&BMAP_TP)==BMAP_TP) {
                    Log_Printf(LOG_WARN, "[BMAP] Switching to twisted pair ethernet.");
                } else if ((val&BMAP_TP)==0) {
                    Log_Printf(LOG_WARN, "[BMAP] Switching to thin ethernet.");
                }
            }
            break;
            
        default:
            break;
    }
    
    NEXTbmap[bmap_reg] = val;
}