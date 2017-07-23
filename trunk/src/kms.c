/*  Previous - kms.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 
 Keyboard, Mouse and Sound logic Emulation.
 
 In real hardware this logic is located in the NeXT Megapixel Display 
 or Soundbox
 
 */

#include "ioMem.h"
#include "ioMemTables.h"
#include "m68000.h"
#include "kms.h"
#include "sysReg.h"
#include "dma.h"
#include "rtcnvram.h"
#include "snd.h"
#include "video.h"
#include "host.h"

#define LOG_KMS_LEVEL LOG_DEBUG
#define IO_SEG_MASK	0x1FFFF


struct {
    struct {
        Uint8 snd_dma;
        Uint8 km;
        Uint8 transmit;
        Uint8 cmd;
    } status;
    
    Uint32 data;
    Uint32 km_data;
} kms;

void KMS_Reset() {
    kms.status.snd_dma  = 0;
    kms.status.km       = 0;
    kms.status.transmit = 0;
    kms.status.cmd      = 0;
    kms.data            = 0;
    kms.km_data         = 0;
}

/* KMS control and status register (0x0200E000) 
 *
 * x--- ---- ---- ---- ---- ---- ---- ----  sound out enable (r/w)
 * -x-- ---- ---- ---- ---- ---- ---- ----  sound output request (r)
 * --x- ---- ---- ---- ---- ---- ---- ----  sound output underrun detected (r/w)
 * ---- x--- ---- ---- ---- ---- ---- ----  sound in enable (r/w)
 * ---- -x-- ---- ---- ---- ---- ---- ----  sound input request (r)
 * ---- --x- ---- ---- ---- ---- ---- ----  sound input overrun detected (r/w)
 *
 * ---- ---- x--- ---- ---- ---- ---- ----  keyboard interrupt (r)
 * ---- ---- -x-- ---- ---- ---- ---- ----  keyboard data received (r)
 * ---- ---- --x- ---- ---- ---- ---- ----  keyboard data overrun detected (r/w)
 * ---- ---- ---x ---- ---- ---- ---- ----  non-maskable interrupt received (tilde and left or right cmd key) (r/w)
 * ---- ---- ---- x--- ---- ---- ---- ----  kms interrupt (r)
 * ---- ---- ---- -x-- ---- ---- ---- ----  kms data received (r)
 * ---- ---- ---- --x- ---- ---- ---- ----  kms data overrun detected (r/w)
 *
 * ---- ---- ---- ---- x--- ---- ---- ----  dma sound out transmit pending (r)
 * ---- ---- ---- ---- -x-- ---- ---- ----  dma sound out transmit in progress (r)
 * ---- ---- ---- ---- --x- ---- ---- ----  cpu data transmit pending (r)
 * ---- ---- ---- ---- ---x ---- ---- ----  cpu data transmit in progress (r)
 * ---- ---- ---- ---- ---- x--- ---- ----  rtx_pend ???
 * ---- ---- ---- ---- ---- -x-- ---- ----  rtx ???
 * ---- ---- ---- ---- ---- --x- ---- ----  kms enable (return from reset state) (r/w)
 * ---- ---- ---- ---- ---- ---x ---- ----  loop back transmitter data (r/w)
 *
 * ---- ---- ---- ---- ---- ---- xxxx xxxx  command to append on kms data (r/w)
 *
 * ---x ---x ---- ---x ---- ---- ---- ----  zero bits
 */


#define SNDOUT_DMA_ENABLE   0x80
#define SNDOUT_DMA_REQUEST  0x40
#define SNDOUT_DMA_UNDERRUN 0x20
#define SNDIN_DMA_ENABLE    0x08
#define SNDIN_DMA_REQUEST   0x04
#define SNDIN_DMA_OVERRUN   0x02

#define KBD_INT             0x80
#define KBD_RECEIVED        0x40
#define KBD_OVERRUN         0x20
#define NMI_RECEIVED        0x10
#define KMS_INT             0x08
#define KMS_RECEIVED        0x04
#define KMS_OVERRUN         0x02

#define TX_DMA_PENDING      0x80
#define TX_DMA              0x40
#define TX_CPU_PENDING      0x20
#define TX_CPU              0x10
#define RTX_PEND            0x08
#define RTX                 0x04
#define KMS_ENABLE          0x02
#define TX_LOOP             0x01


/* KMS commands */

/* Host commands */
#define KMSCMD_RESET    0xFF
#define KMSCMD_ASNDOUT  0xC7    /* analog sound out */
#define KMSCMD_KMREG    0xC5    /* access keyboard or mouse register */
#define KMSCMD_CTRLOUT  0xC4    /* access volume control logic */
#define KMSCMD_VOLCTRL  0xC2    /* simplified access to volume control */

#define KMSCMD_SND_IN   0x03    /* sound in */
#define KMSCMD_SND_OUT  0x07    /* sound out */
#define KMSCMD_SIO_MASK 0xC7    /* mask for sound in/out */

#define SIO_ENABLE      0x08    /* 1=enable, 0=disable sound */
#define SIO_DBL_SMPL    0x10    /* 1=double sample, 0=normal */
#define SIO_ZERO        0x20    /* double sample by 1=zero filling, 0=repetition */

/* Commands from KMS board */
#define KMSCMD_CODEC_IN 0xC7    /* CODEC sound in */
#define KMSCMD_KBD_RECV 0xC6    /* receive data from keyboard/mouse */
#define KMSCMD_SO_REQ   0x07    /* sound out request */
#define KMSCMD_SO_UNDR  0x0F    /* sound out underrun */

void KMS_command(Uint8 command, Uint32 data);


/* Keyboard Registers */
#define KM_REG_MASK     0xE0
#define KM_READ         0x10
#define KM_RESET        0x0F
#define KM_SET_ADDR     0xEF
#define KM_ADDR_MASK    0x0E
#define KM_ADDR_MOUSE   0x01


/* Device mask
 *
 * xxxx xxxx xxxx xxxx xxxx xxxx xxxx ----  polled devices (7 device addresses, 4 bit each)
 * ---- ---- ---- ---- ---- ---- ---- xxxx  poll speed
 */

Uint32 km_address = 0;
Uint32 km_dev_msk = 0;

static void access_km_reg(Uint32 data) {
    Uint8 reg_addr = (data>>24)&0xFF;
    Uint8 reg_data = (data>>16)&0xFF;
    
    if (reg_addr==KM_RESET) {
        Log_Printf(LOG_KMS_LEVEL, "Keyboard/Mouse: Reset");
        kms_response();
        return;
    }
    if (reg_addr==KM_SET_ADDR) {
        Log_Printf(LOG_KMS_LEVEL, "Keyboard/Mouse: Set address to %i",(reg_data&KM_ADDR_MASK)>>1);
        km_address = reg_data&KM_ADDR_MASK;
        kms_response();
        return;
    }
    
    bool device_kbd = (reg_addr&KM_ADDR_MOUSE) ? false : true;
    int device_addr = (reg_addr&KM_ADDR_MASK)>>1;
    int device_reg = (reg_addr&KM_REG_MASK)>>5;
    bool read_reg = (reg_addr&KM_READ) ? true : false;
    
    Log_Printf(LOG_KMS_LEVEL, "%s %s %i, register %i",read_reg?"Reading":"Writing",
               device_kbd?"keyboard":"mouse",device_addr,device_reg);
    
    if (reg_addr&KM_READ) {
        switch (device_reg) {
            case 0:
                Log_Printf(LOG_KMS_LEVEL, "Poll device");
                break;
            case 7:
                Log_Printf(LOG_KMS_LEVEL, "Request device revision");
                break;
            default:
                Log_Printf(LOG_WARN, "Unknown device register");
                break;
        }
    } else { // device write
        switch (device_reg) {
            case 0:
                if (device_kbd) {
                    Log_Printf(LOG_KMS_LEVEL, "Turn %s keyboard LED1",(reg_data&1)?"on":"off");
                    Log_Printf(LOG_KMS_LEVEL, "Turn %s keyboard LED2",(reg_data&2)?"on":"off");
                    break;
                }                
            default:
                Log_Printf(LOG_WARN, "Unknown device register");
                break;
        }
    }
    kms_response();
}

void KMS_command(Uint8 command, Uint32 data) {
    switch (command) {
        case KMSCMD_KBD_RECV: // keyboard poll
            km_dev_msk=data;
            return;

        case KMSCMD_RESET:
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Reset");
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Data = %08X",data);
            break;
        case KMSCMD_ASNDOUT:
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Analog sound out");
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Data = %08X",data);
            break;
        case KMSCMD_KMREG:
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Access keyboard/mouse register");
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Data = %08X",data);
            access_km_reg(data);
            break;
        case KMSCMD_CTRLOUT:
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Access volume control logic");
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Data = %08X",data);
            snd_gpo_access(data>>24);
            break;
        case KMSCMD_VOLCTRL:
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Access volume control (simple)");
            Log_Printf(LOG_KMS_LEVEL, "[KMS] Data = %08X",data);
            break;
            
        default: // commands without data
            if ((command&KMSCMD_SIO_MASK)==KMSCMD_SND_OUT) {
                Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound out command:");

                if (command&SIO_ENABLE) {
                    Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound out enable.");
                    if (command&SIO_DBL_SMPL) {
                        Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound out double sample.");
                        if (command&SIO_ZERO) {
                            Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound out double sample by zero filling.");
                        } else {
                            Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound out double sample by repetition.");
                        }
                    } else {
                        Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound out normal sample.");
                    }
                    snd_start_output(command&(SIO_DBL_SMPL|SIO_ZERO));
                } else {
                    Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound out disable.");
                    kms.status.snd_dma &= ~(SNDOUT_DMA_UNDERRUN|SNDOUT_DMA_REQUEST);
                    snd_stop_output();
                }
            } else if ((command&KMSCMD_SIO_MASK)==KMSCMD_SND_IN) {
                Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound in command");
                
                if (command&SIO_ENABLE) {
                    Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound in enable.");
                    snd_start_input(command);
                } else {
                    Log_Printf(LOG_KMS_LEVEL, "[KMS] Sound in disable.");
                    kms.status.snd_dma &= ~(SNDIN_DMA_OVERRUN|SNDIN_DMA_REQUEST);
                    snd_stop_input();
                }
            } else {
                Log_Printf(LOG_WARN, "[KMS] Unknown command!");
            }
            return;
    }
}


void KMS_Ctrl_Snd_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    
    kms.status.snd_dma &= ~(SNDOUT_DMA_ENABLE|SNDIN_DMA_ENABLE);
    kms.status.snd_dma |= (val&(SNDOUT_DMA_ENABLE|SNDIN_DMA_ENABLE));
    
    if (val&SNDOUT_DMA_UNDERRUN && (!snd_output_active())) {
        kms.status.snd_dma &= ~(SNDOUT_DMA_UNDERRUN|SNDOUT_DMA_REQUEST);
        set_interrupt(INT_SOUND_OVRUN, RELEASE_INT);
    }
    if (val&SNDIN_DMA_OVERRUN && (!snd_input_active())) {
        kms.status.snd_dma &= ~(SNDIN_DMA_OVERRUN|SNDIN_DMA_REQUEST);
        set_interrupt(INT_SOUND_OVRUN, RELEASE_INT);
    }
}

void KMS_Stat_Snd_Read(void) {
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = kms.status.snd_dma;
}

void kms_sndout_underrun() {
    kms.status.snd_dma |=  SNDOUT_DMA_UNDERRUN|SNDOUT_DMA_REQUEST;
    set_interrupt(INT_SOUND_OVRUN, SET_INT);
}

void kms_sndin_overrun() {
    kms.status.snd_dma |=  SNDIN_DMA_OVERRUN|SNDIN_DMA_REQUEST;
    set_interrupt(INT_SOUND_OVRUN, SET_INT);
}

void KMS_Ctrl_KM_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    
    if (val&KBD_OVERRUN) {
        kms.status.km &= ~(KBD_RECEIVED|KBD_OVERRUN|KBD_INT);
        set_interrupt(INT_KEYMOUSE, RELEASE_INT);
    }
    if (val&NMI_RECEIVED) {
        kms.status.km &= ~NMI_RECEIVED;
        set_interrupt(INT_NMI, RELEASE_INT);
    }
    if (val&KMS_OVERRUN) {
        kms.status.km &= ~(KMS_RECEIVED|KMS_OVERRUN|KMS_INT);
        set_interrupt(INT_MONITOR, RELEASE_INT);
    }
}

void KMS_Stat_KM_Read(void) {
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = kms.status.km;
}

void KMS_Ctrl_TX_Write(void) {
    Uint8 val = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
    
    kms.status.transmit &= ~(KMS_ENABLE|TX_LOOP);
    kms.status.transmit |= (val&(KMS_ENABLE|TX_LOOP));
}

void KMS_Stat_TX_Read(void) {
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = kms.status.transmit;
}

void KMS_Ctrl_Cmd_Write(void) {
    kms.status.cmd = IoMem[IoAccessCurrentAddress&IO_SEG_MASK];
}

void KMS_Stat_Cmd_Read(void) {
    IoMem[IoAccessCurrentAddress&IO_SEG_MASK] = kms.status.cmd;
}


/* KMS data register (0x0200E004) */

void KMS_Data_Write(void) {
    kms.data = IoMem_ReadLong(IoAccessCurrentAddress&IO_SEG_MASK);
    KMS_command(kms.status.cmd, kms.data);
}

void KMS_Data_Read(void) {
    IoMem_WriteLong(IoAccessCurrentAddress&IO_SEG_MASK, kms.data);
}


/* KMS keyboard and mouse data register (0x0200E008) *
 *
 * x--- ---- ---- ---- ---- ---- ---- ----  always 0
 * -x-- ---- ---- ---- ---- ---- ---- ----  1 = no response error, 0 = normal event
 * --x- ---- ---- ---- ---- ---- ---- ----  1 = user poll, 0 = internal poll
 * ---x ---- ---- ---- ---- ---- ---- ----  1 = invalid/master, 0 = valid/slave (user/internal)
 * ---- xxxx ---- ---- ---- ---- ---- ----  device address (lowest bit 1 = mouse, 0 = keyboard)
 * ---- ---- xxxx xxxx ---- ---- ---- ----  chip revision: 0 = old, 1 = new, 2 = digital
 *
 * Mouse data:
 * ---- ---- ---- ---- xxxx xxx- ---- ----  mouse y
 * ---- ---- ---- ---- ---- ---x ---- ----  right button up (1) or down (0)
 * ---- ---- ---- ---- ---- ---- xxxx xxx-  mouse x
 * ---- ---- ---- ---- ---- ---- ---- ---x  left button up (1) or down (0)
 *
 * Keyboard data:
 * ---- ---- ---- ---- x--- ---- ---- ----  valid (1) or invalid (0)
 * ---- ---- ---- ---- -x-- ---- ---- ----  right alt
 * ---- ---- ---- ---- --x- ---- ---- ----  left alt
 * ---- ---- ---- ---- ---x ---- ---- ----  right command
 * ---- ---- ---- ---- ---- x--- ---- ----  left command
 * ---- ---- ---- ---- ---- -x-- ---- ----  right shift
 * ---- ---- ---- ---- ---- --x- ---- ----  left shift
 * ---- ---- ---- ---- ---- ---x ---- ----  control
 * ---- ---- ---- ---- ---- ---- x--- ----  key up (1) or down (0)
 * ---- ---- ---- ---- ---- ---- -xxx xxxx  keycode 
 */

#define NO_RESPONSE_ERR 0x40000000
#define USER_POLL       0x20000000
#define DEVICE_INVALID  0x10000000
#define DEVICE_MASTER   0x10000000

#define DEVICE_ADDR_MSK 0x0E000000
#define DEVICE_MOUSE    0x01000000

#define MOUSE_Y         0x0000FE00
#define MOUSE_RIGHT_UP  0x00000100
#define MOUSE_X         0x000000FE
#define MOUSE_LEFT_UP   0x00000001

#define KBD_KEY_VALID   0x00008000
#define KBD_MOD_MASK    0x00007F00
#define KBD_KEY_UP      0x00000080
#define KBD_KEY_MASK    0x0000007F


bool m_button_right = false;
bool m_button_left  = false;
bool m_move_left    = false;
bool m_move_up      = false;
int  m_move_x       = 0;
int  m_move_y       = 0;
int  m_move_dx      = 0;
int  m_move_dy      = 0;
void kms_mouse_move_step(void);


void KMS_KM_Data_Read(void) {
    IoMem_WriteLong(IoAccessCurrentAddress & IO_SEG_MASK, kms.km_data);
    
    kms.status.km &= ~(KBD_RECEIVED|KBD_INT);
    set_interrupt(INT_KEYMOUSE, RELEASE_INT);
}

static void kms_interrupt(void) {
    kms.status.cmd = KMSCMD_KBD_RECV;
    
    if (kms.status.km&KBD_RECEIVED) {
        kms.status.km |= KBD_OVERRUN;
    }
    kms.status.km |= (KBD_RECEIVED|KBD_INT);
    set_interrupt(INT_KEYMOUSE, SET_INT);
}

static bool kms_device_enabled(int dev_addr) {
    int i,mask;

    for (i=28; i>4; i-=4) {
        mask=(km_dev_msk>>i)&0xF;
        if(mask==dev_addr && mask!=0xF)
            return true;
    }
    Log_Printf(LOG_KMS_LEVEL, "[KMS] Device %i disabled (mask: %08X)",dev_addr,km_dev_msk);
    return false;
}

void kms_keydown(Uint8 modkeys, Uint8 keycode) {
    if ((keycode==0x26)&&(modkeys&0x18)) { /* backquote and one or both command keys */
        Log_Printf(LOG_WARN, "Keyboard initiated NMI!");
        set_interrupt(INT_NMI, SET_INT);
        return;
    }
    
    if ((keycode==0x25)&&((modkeys&0x28)==0x28)) { /* asterisk and left alt and left command key */
        Log_Printf(LOG_WARN, "Keyboard initiated CPU reset!");
        host_darkmatter(false);
        M68000_Reset(false);
        return;
    }
    
    if (keycode==0x58) { /* Power key */
        rtc_request_power_down();
        return;
    }
    
    if (kms_device_enabled(km_address)) {
        kms.km_data = (km_address<<24)|DEVICE_MASTER; /* keyboard */

        kms.km_data |= (modkeys<<8)|keycode|KBD_KEY_VALID;
        
        kms_interrupt();
    }
}

void kms_keyup(Uint8 modkeys, Uint8 keycode) {
    if (keycode==0x58) {
        rtc_stop_pdown_request();
        return;
    }
    
    if (kms_device_enabled(km_address)) {
        kms.km_data = (km_address<<24)|DEVICE_MASTER; /* keyboard */

        kms.km_data |= (modkeys<<8)|keycode|KBD_KEY_VALID|KBD_KEY_UP;
        
        kms_interrupt();
    }
}

void kms_mouse_button(bool left, bool down) {
    if (left) {
        m_button_left = down;
    } else {
        m_button_right = down;
    }
    
    if (kms_device_enabled(km_address|KM_ADDR_MOUSE)) {
        kms.km_data = (km_address|KM_ADDR_MOUSE)<<24; /* mouse */
        
        kms.km_data |= m_button_left?0:MOUSE_LEFT_UP;
        kms.km_data |= m_button_right?0:MOUSE_RIGHT_UP;
        
        kms_interrupt();
    }
}

#define MOUSE_STEP_FREQ 1000

void kms_mouse_move(int x, bool left, int y, bool up) {
    if (x<0 || y<0) abort();
    
    m_move_left = left;
    m_move_up   = up;

    int xsteps = x / 8; if(xsteps == 0) xsteps = 1;
    int ysteps = y / 8; if(ysteps == 0) ysteps = 1;
    
    m_move_x  = x;
    m_move_dx = x / xsteps;
    
    m_move_y  = y;
    m_move_dy = y / ysteps;
    
    CycInt_AddRelativeInterruptCycles(10, INTERRUPT_MOUSE);
}

void kms_mouse_move_step(void) {
    
    int x = m_move_x > m_move_dx ? m_move_dx : m_move_x;
    int y = m_move_y > m_move_dy ? m_move_dy : m_move_y;

    m_move_x -= x;
    m_move_y -= y;
    
    if (!m_move_left && x>0)  /* right */
        x=(0x40-x)|0x40;
    if (!m_move_up && y>0)    /* down */
        y=(0x40-y)|0x40;
    
    if (kms_device_enabled(km_address|KM_ADDR_MOUSE)) {
        kms.km_data = (km_address|KM_ADDR_MOUSE)<<24; /* mouse */
        
        kms.km_data |= (x<<1)&MOUSE_X;
        kms.km_data |= (y<<9)&MOUSE_Y;
        
        kms.km_data |= m_button_left?0:MOUSE_LEFT_UP;
        kms.km_data |= m_button_right?0:MOUSE_RIGHT_UP;
        
        kms_interrupt();
    }
}

void kms_response(void) {
    kms.km_data = km_address<<24; /* keyboard */
    kms.km_data |= USER_POLL;
    kms.km_data |= (NO_RESPONSE_ERR|DEVICE_INVALID); /* checked on real hardware */
    
    kms_interrupt();
}

void Mouse_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    
    if (m_move_x > 0 || m_move_y > 0) {
        kms_mouse_move_step();
        CycInt_AddRelativeInterruptUs((1000*1000)/MOUSE_STEP_FREQ, 0, INTERRUPT_MOUSE);
    }
}
