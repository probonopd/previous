#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "dimension.h"
#include "nd_vio.h"
#include "i860cfg.h"
#include "nd_sdl.h"
#include "host.h"


#define LOG_VID_LEVEL   LOG_WARN

/* --------- NEXTDIMENSION VIDEO I/O ---------- *
 *                                              *
 * Code for NeXTdimension video I/O. For now    *
 * only some dummy devices.                     */


#define ND_VID_DMCD     0x8A
#define ND_VID_DCSC0    0xE0
#define ND_VID_DCSC1    0xE2


/* SAA7191 (Digital Multistandard Colour Decoder - Square Pixel) */

#define ND_DMCD_NUM_REG 25

struct {
    uae_u8 addr;
    uae_u8 reg[ND_DMCD_NUM_REG];
} dmcd;


void dmcd_write(uae_u32 step, uae_u8 data) {
    if (step == 0) {
        dmcd.addr = data;
    } else {
        Log_Printf(LOG_VID_LEVEL, "[ND] DMCD: Writing register %i (%02X)", dmcd.addr, data);
        
        if (dmcd.addr < ND_DMCD_NUM_REG) {
            dmcd.reg[dmcd.addr] = data;
            dmcd.addr++;
        } else {
            Log_Printf(LOG_WARN, "[ND] DMCD: Illegal register (%i)", dmcd.addr);
        }
    }
}


/* SAA7192 (Digital Colour Space Converter) */

#define ND_DCSC_CTRL    0x00
#define ND_DCSC_CLUT    0x01

struct {
    uae_u8 addr;
    uae_u8 ctrl;
    uae_u8 lut_r[256];
    uae_u8 lut_g[256];
    uae_u8 lut_b[256];
} dcsc[2];


void dcsc_write(uae_u8 dev, uae_u32 step, uae_u8 data) {
    switch (step) {
        case 0:
            dcsc[dev].addr = data;
            break;
        case 1:
            if (dcsc[dev].addr == ND_DCSC_CTRL) {
                Log_Printf(LOG_VID_LEVEL, "[ND] DCSC%i: Writing control (%02X)", dev, data);
                
                dcsc[dev].ctrl = data;
                break;
            }
            /* else fall through */

        default:
            if (dcsc[dev].addr == ND_DCSC_CLUT) {
                Log_Printf(LOG_VID_LEVEL, "[ND] DCSC%i: Writing LUT at %i (%02X)", dev, step-1, data);

                dcsc[dev].lut_r[(step-1)&0xFF] = data;
                dcsc[dev].lut_g[(step-1)&0xFF] = data;
                dcsc[dev].lut_b[(step-1)&0xFF] = data;
            } else {
                Log_Printf(LOG_WARN, "[ND] DCSC%i: Unknown address (%02X)", dev, dcsc[dev].addr);
            }
            break;
    }
}


/* Receiver function (from IIC bus) */

void nd_video_dev_write(uae_u8 addr, uae_u32 step, uae_u8 data) {
    
    if (addr&0x01) {
        Log_Printf(LOG_WARN, "[ND] IIC bus: Unimplemented read from video device at %02X", addr);
    }
    
    if (step == 0) {
        Log_Printf(LOG_VID_LEVEL, "[ND] IIC bus: selecting device at %02X (subaddress: %02X)", addr, data);
    }
    
    switch (addr&0xFE) {
        case ND_VID_DMCD:
            dmcd_write(step, data);
            break;
        case ND_VID_DCSC0:
            dcsc_write(0, step, data);
            break;
        case ND_VID_DCSC1:
            dcsc_write(1, step, data);
            break;
            
        default:
            Log_Printf(LOG_WARN, "[ND] IIC bus: Unknown device (%02X)", addr);
            break;
    }
}
