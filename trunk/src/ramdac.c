/*  Previous - ramdac.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Brooktree Bt463 RAMDAC emulation.
 
 This chip was is used in color NeXTstations and NeXTdimension.
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "configuration.h"
#include "ramdac.h"
#include "sysReg.h"

#define IO_SEG_MASK 0x1FFFF

#define LOG_RAMDAC_LEVEL    LOG_DEBUG


/* Bt 463 RAMDAC */

#define BT_ADDR_MASK    0x0FFF
#define BT_ADDR_CCR     0x0100
#define BT_ADDR_REG     0x0200
#define BT_ADDR_WTT     0x0300

#define BT463_ID    0x2A
#define BT463_REV   0x0A

#define BT_REG_ID   0x0
#define BT_REG_CR0  0x1
#define BT_REG_CR1  0x2
#define BT_REG_CR2  0x3
#define BT_REG_RM0  0x5
#define BT_REG_RM1  0x6
#define BT_REG_RM2  0x7
#define BT_REG_RM3  0x8
#define BT_REG_BM0  0x9
#define BT_REG_BM1  0xa
#define BT_REG_BM2  0xb
#define BT_REG_BM3  0xc
#define BT_REG_TEST 0xd
#define BT_REG_ISR  0xe
#define BT_REG_OSR  0xf
#define BT_REG_REV  0x20



static void bt463_autoinc(bt463* ramdac) {
    ramdac->idx++;
    if(ramdac->idx > 2) {
        ramdac->idx = 0;
        ramdac->addr++;
        ramdac->addr &= 0x0FFF;
    }
}

static void bt463_autoinc_reg(bt463* ramdac) {
    ramdac->idx = 0;
    ramdac->addr++;
    ramdac->addr &= 0x0FFF;
}

/* BT463 Registers */
uae_u32 bt463_read_reg(bt463* ramdac) {
    uae_u32 result = 0;
    
    if ((ramdac->addr&0xFF)<0x10) {
        switch (ramdac->addr&0x0F) {
            case BT_REG_ID:
                result = BT463_ID;
                bt463_autoinc_reg(ramdac);
                break;
            case BT_REG_ISR:
            case BT_REG_OSR:
                result = ramdac->reg[(ramdac->addr&0x0F)*3+ramdac->idx];
                bt463_autoinc(ramdac);
                break;
            default:
                result = ramdac->reg[(ramdac->addr&0x0F)*3];
                bt463_autoinc_reg(ramdac);
                break;
        }
    } else if ((ramdac->addr&0xFF)==BT_REG_REV){
        result = BT463_REV;
        bt463_autoinc_reg(ramdac);
    }

    return result;
}

void bt463_write_reg(bt463* ramdac, uae_u32 val) {
   
    if ((ramdac->addr&0xFF)<0x10) {
        switch (ramdac->addr&0x0F) {
            case BT_REG_ID:
                bt463_autoinc_reg(ramdac);
                break;
            case BT_REG_ISR:
            case BT_REG_OSR:
                ramdac->reg[(ramdac->addr&0x0F)*3+ramdac->idx] = val & 0xFF;
                bt463_autoinc(ramdac);
                break;
            default:
                ramdac->reg[(ramdac->addr&0x0F)*3] = val & 0xFF;
                bt463_autoinc_reg(ramdac);
                break;
        }
    }
}

/* BT463 Cursor Color */
uae_u32 bt463_read_ccr(bt463* ramdac) {
    uae_u32 result = 0;
    
    if ((ramdac->addr&0xFF)<2) {
        result = ramdac->ccr[(ramdac->addr&1)*3+ramdac->idx];
        bt463_autoinc(ramdac);
    }
    
    return result;
}

void bt463_write_ccr(bt463* ramdac, uae_u32 val) {
    
    if ((ramdac->addr&0xFF)<4) {
        ramdac->ccr[(ramdac->addr&3)*3+ramdac->idx] = val & 0xFF;
        bt463_autoinc(ramdac);
    }
}

/* BT463 Window Type Table */
uae_u32 bt463_read_wtt(bt463* ramdac) {
    uae_u32 result = 0;
    
    if ((ramdac->addr&0xFF)<0x10) {
        switch (ramdac->idx) {
            case 0:
                ramdac->wtt_tmp = ramdac->wtt[ramdac->addr&0x0F];
                result = ramdac->wtt_tmp & 0xFF;
                break;
            case 1:
                result = (ramdac->wtt_tmp >> 8) & 0xFF;
                break;
            case 2:
                result = (ramdac->wtt_tmp >> 16) & 0xFF;
                break;
            default:
                break;
        }
        bt463_autoinc(ramdac);
    }
    
    return result;
}

void bt463_write_wtt(bt463* ramdac, uae_u32 val) {
    
    if ((ramdac->addr&0xFF)<0x10) {
        switch (ramdac->idx) {
            case 0:
                ramdac->wtt_tmp = val & 0x0000FF;
                break;
            case 1:
                ramdac->wtt_tmp |= (val << 8) & 0x00FF00;
                break;
            case 2:
                ramdac->wtt_tmp |= (val << 16) & 0xFF0000;
                ramdac->wtt[ramdac->addr&0x0F] = ramdac->wtt_tmp;
                break;
            default:
                break;
        }
        bt463_autoinc(ramdac);
    }
}

/* BT463 Palette RAM */
uae_u32 bt463_read_palette(bt463* ramdac) {
    uae_u32 result = 0;
    
    if (ramdac->addr<0x210) {
        result = ramdac->ram[ramdac->addr*3+ramdac->idx];
    }
    bt463_autoinc(ramdac);
    
    return result;
}

void bt463_write_palette(bt463* ramdac, uae_u32 val) {
    
    if (ramdac->addr<0x210) {
        ramdac->ram[ramdac->addr*3+ramdac->idx] = val & 0xFF;
    }
    bt463_autoinc(ramdac);
}



/* BT463 Host Interface */
uae_u32 bt463_bget(bt463* ramdac, uaecptr addr) {

    switch(addr & 0xF) {
        case 0:
            ramdac->idx = 0;
            return ramdac->addr & 0xFF;
        case 0x4:
            ramdac->idx = 0;
            return (ramdac->addr >> 8) & 0x0F;
        case 0x8:
            switch (ramdac->addr&0x0F00) {
                case BT_ADDR_CCR:
                    return bt463_read_ccr(ramdac);
                case BT_ADDR_REG:
                    return bt463_read_reg(ramdac);
                case BT_ADDR_WTT:
                    return bt463_read_wtt(ramdac);
                    
                default: break;
            }
            break;
        case 0xC:
            return bt463_read_palette(ramdac);
    }
    return 0;
}

void bt463_bput(bt463* ramdac, uaecptr addr, uae_u32 b) {
    switch(addr & 0xF) {
        case 0x0:
            ramdac->addr &= 0x0F00;
            ramdac->addr |= b & 0xFF;
            ramdac->idx = 0;
            break;
        case 0x4:
            ramdac->addr &= 0x00FF;
            ramdac->addr |= (b & 0x0F) << 8;
            ramdac->idx = 0;
            break;
        case 0x8:
            switch (ramdac->addr&0x0F00) {
                case BT_ADDR_CCR:
                    bt463_write_ccr(ramdac, b);
                    break;
                case BT_ADDR_REG:
                    bt463_write_reg(ramdac, b);
                    break;
                case BT_ADDR_WTT:
                    bt463_write_wtt(ramdac, b);
                    break;
                    
                default:
                    break;
            }
            break;
        case 0xC:
            bt463_write_palette(ramdac, b);
            break;
    }
}
