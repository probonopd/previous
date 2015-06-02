#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "cycInt.h"
#include "audio.h"
#include "dma.h"
#include "snd.h"
#include "queue.h"

#define LOG_SND_LEVEL   LOG_DEBUG
#define LOG_VOL_LEVEL   LOG_DEBUG

/* Note: Buffer limit must not be greather than queue packet size and
 * must match requested.samples * samplesize (4) from audio.c
 */
#define SND_BUFFER_LIMIT 4096


/* queue prototypes */
queueADT	sndout_q;


/* Initialize the audio system */
bool sndout_inited;
bool sound_output_active = false;

void sound_init(void) {
    snd_buffer.limit=SND_BUFFER_LIMIT;
    snd_buffer.size=0;
    if (!sndout_inited && ConfigureParams.Sound.bEnableSound) {
        Log_Printf(LOG_WARN, "[Audio] Initializing audio device.");
        Audio_Output_Init();
        sndout_q = QueueCreate();
        sndout_inited=true;
    }
}

void sound_uninit(void) {
    if(sndout_inited) {
        Log_Printf(LOG_WARN, "[Audio] Uninitializing audio device.");
        sndout_inited=false;
        Audio_Output_UnInit();
        QueueDestroy(sndout_q);
    }
}

void Sound_Reset(void) {
    sound_uninit();
    sound_init();
    if (sound_output_active && sndout_inited) {
        Audio_Output_Enable(true);
    }
}


/* Start and stop sound output */
struct {
    Uint8 mode;
    Uint8 mute;
    Uint8 lowpass;
    Uint8 volume[2]; /* 0 = left, 1 = right */
} sndout_state;

/* Maximum volume (really is attenuation) */
#define SND_MAX_VOL 43

/* Valid modes */
#define SND_MODE_NORMAL 0x00
#define SND_MODE_DBL_RP 0x10
#define SND_MODE_DBL_ZF 0x30

/* Function prototypes */
void snd_send_samples(void);
void snd_make_normal_samples(Uint8 *buf, int len);
void snd_make_double_samples(Uint8 *buf, int len, bool repeat);
void snd_adjust_volume_and_lowpass(Uint8 *buf, int len);
void sndout_queue_put(Uint8 *buf, int len);

void snd_start_output(Uint8 mode) {
    sndout_state.mode = mode;
    /* Starting SDL Audio */
    if (sndout_inited) {
        Audio_Output_Enable(true);
    } else {
        Log_Printf(LOG_SND_LEVEL, "[Audio] Not starting. Audio device not initialized.");
    }
    /* Starting sound output loop */
    if (!sound_output_active) {
        Log_Printf(LOG_SND_LEVEL, "[Sound] Starting loop.");
        sound_output_active = true;
        CycInt_AddRelativeInterrupt(100, INT_CPU_CYCLE, INTERRUPT_SND_IO);
    } else { /* Even re-enable loop if we are already active. This lowers the delay. */
        Log_Printf(LOG_WARN, "[Sound] Restarting loop.");
        CycInt_AddRelativeInterrupt(100, INT_CPU_CYCLE, INTERRUPT_SND_IO);
    }
}

void snd_stop_output(void) {
    if (sound_output_active) {
        sound_output_active=false;
    }
}


/* Sound IO loop (reads via DMA from memory to queue) */
#define SND_DELAY   100000
int old_size;
int queue_size;

void SND_IO_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    
    old_size = snd_buffer.size;
    dma_sndout_read_memory();
    
    if (!sndout_inited || sndout_state.mute) {
        snd_buffer.limit = SND_BUFFER_LIMIT;
        snd_buffer.size = 0;
        if (!sound_output_active)
            return;
    } else {
        Audio_Output_Lock();
        queue_size = QueuePeek(sndout_q);
        Audio_Output_Unlock();
        if (queue_size<4) {
            if (snd_buffer.size==SND_BUFFER_LIMIT || snd_buffer.size==old_size) {
                Log_Printf(LOG_SND_LEVEL, "[Sound] %i samples ready.",snd_buffer.size/4);
                snd_buffer.limit = snd_buffer.size;
                snd_send_samples();
                snd_buffer.limit = SND_BUFFER_LIMIT;
                snd_buffer.size = 0; /* Must be 0 */
            }
            
            if (!sound_output_active)
                return;
        }
    } /* if queuepeek<4 */
    CycInt_AddRelativeInterrupt(SND_DELAY, INT_CPU_CYCLE, INTERRUPT_SND_IO);
}


/* These functions put samples to a buffer for further processing */
void snd_make_double_samples(Uint8 *buf, int len, bool repeat) {
    int i;
    for (i=0; i<(len*2); i++) {
        if (snd_buffer.size>0) {
            buf[i] = snd_buffer.data[snd_buffer.limit-snd_buffer.size];
            buf[i+4] = repeat ? buf[i] : 0; /* repeat or zero-fill */
            snd_buffer.size--;
        } else { /* Fill the rest with silence */
            buf[i] = buf[i+4] = 0;
        }
        if ((i&3)==3) i+=4;
    }
}

void snd_make_normal_samples(Uint8 *buf, int len) {
    int i;
    for (i=0; i<len; i++) {
        if (snd_buffer.size>0) {
            buf[i] = snd_buffer.data[snd_buffer.limit-snd_buffer.size];
            snd_buffer.size--;
        } else { /* Fill the rest with silence */
            buf[i] = 0;
        }
    }
}


/* This function processes and sends out our samples */
void snd_send_samples(void) {
    static Uint8 sndout_buffer[2*SND_BUFFER_LIMIT];

    switch (sndout_state.mode) {
        case SND_MODE_NORMAL:
            snd_make_normal_samples(sndout_buffer, SND_BUFFER_LIMIT);
            snd_adjust_volume_and_lowpass(sndout_buffer, SND_BUFFER_LIMIT);
            sndout_queue_put(sndout_buffer, SND_BUFFER_LIMIT);
            break;
        case SND_MODE_DBL_RP:
            snd_make_double_samples(sndout_buffer, SND_BUFFER_LIMIT, true);
            snd_adjust_volume_and_lowpass(sndout_buffer, 2*SND_BUFFER_LIMIT);
            sndout_queue_put(sndout_buffer, SND_BUFFER_LIMIT);
            sndout_queue_put(sndout_buffer+SND_BUFFER_LIMIT, SND_BUFFER_LIMIT);
            break;
        case SND_MODE_DBL_ZF:
            snd_make_double_samples(sndout_buffer, SND_BUFFER_LIMIT, false);
            snd_adjust_volume_and_lowpass(sndout_buffer, 2*SND_BUFFER_LIMIT);
            sndout_queue_put(sndout_buffer, SND_BUFFER_LIMIT);
            sndout_queue_put(sndout_buffer+SND_BUFFER_LIMIT, SND_BUFFER_LIMIT);
            break;

        default:
            Log_Printf(LOG_WARN, "[Sound] Error: Unknown sound output mode!");
            break;
    }
}


/* This function puts data to a queue for the audio system */
void sndout_queue_put(Uint8 *buf, int len) {
    struct queuepacket *p;
    p=(struct queuepacket *)malloc(sizeof(struct queuepacket));
    Audio_Output_Lock();
    p->len=len;
    memcpy(p->data, buf, p->len);
    QueueEnter(sndout_q,p);
    Audio_Output_Unlock();
    Log_Printf(LOG_SND_LEVEL, "[Sound] Output 1024 samples to queue");
}


/* This function is called from the audio system to poll data */
bool audio_flushed=false;

void sndout_queue_poll(Uint8 *buf, int len) {
    if (QueuePeek(sndout_q)>0) {
        struct queuepacket *qp;
        audio_flushed = false;
        qp=QueueDelete(sndout_q);
        Log_Printf(LOG_SND_LEVEL, "[Audio] Reading 1024 samples from queue.");
        memcpy(buf,qp->data,len);
        free(qp);
    } else if (!sound_output_active) {
        /* Last packet received, stop */
        if (audio_flushed) {
            Log_Printf(LOG_SND_LEVEL, "[Audio] Done. Stopping.");
            audio_flushed = false;
            Audio_Output_Enable(false);
        } else { /* Flush residual audio from device (required for SDL) */
            Log_Printf(LOG_SND_LEVEL, "[Audio] Done. Flushing.");
            audio_flushed = true;
            memset(buf, 0, len);
        }
    } else {
        Log_Printf(LOG_WARN, "[Audio] Not ready. No data on queue.");
        memset(buf, 0, len);
    }
}

#if 1 /* FIXME: Is this correct? */
/* This is a simple lowpass filter */
static Sint16 snd_lowpass_filter(Sint16 insample, bool left) {
    Sint16 outsample;
    static Sint16 lfiltersample[2] = {0,0};
    static Sint16 rfiltersample[2] = {0,0};
    
    if (left) {
        outsample = (lfiltersample[0] + (lfiltersample[1]<<1) + insample)>>2;
        lfiltersample[0] = lfiltersample[1];
        lfiltersample[1] = insample;
    } else {
        outsample = (rfiltersample[0] + (rfiltersample[1]<<1) + insample)>>2;
        rfiltersample[0] = rfiltersample[1];
        rfiltersample[1] = insample;
    }
    return outsample;
}
#endif

/* This function adjusts sound output volume */
void snd_adjust_volume_and_lowpass(Uint8 *buf, int len) {
    int i;
    Sint16 ldata, rdata;
    float ladjust, radjust;
    if (sndout_state.volume[0] || sndout_state.volume[1] || sndout_state.lowpass) {
        ladjust = (sndout_state.volume[0]==0)?1:(1-log(sndout_state.volume[0])/log(SND_MAX_VOL));
        radjust = (sndout_state.volume[1]==0)?1:(1-log(sndout_state.volume[1])/log(SND_MAX_VOL));
        
        for (i=0; i<len; i+=4) {
            ldata = (Sint16)((buf[i]<<8)|buf[i+1]);
            rdata = (Sint16)((buf[i+2]<<8)|buf[i+3]);
#if 1       /* Append lowpass filter */
            if (sndout_state.lowpass) {
                ldata = snd_lowpass_filter(ldata, true);
                rdata = snd_lowpass_filter(rdata, false);
            }
#endif
            ldata = ldata*ladjust;
            rdata = rdata*radjust;
            buf[i] = ldata>>8;
            buf[i+1] = ldata;
            buf[i+2] = rdata>>8;
            buf[i+3] = rdata;
        }
    }
}


/* Internal volume control register access (shifted in left to right)
 *
 * xxx ---- ----  unused bits
 * --- xx-- ----  channel (0x80 = right, 0x40 = left)
 * --- --xx xxxx  volume
 */

Uint8 tmp_vol;
Uint8 chan_lr;
int bit_num;

void snd_access_volume_reg(Uint8 databit) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Interface shift bit %i (%i).",bit_num,databit?1:0);
    
    if (bit_num<3) {
        /* nothing to do */
    } else if (bit_num<5) {
        chan_lr = (chan_lr<<1)|(databit?1:0);
    } else if (bit_num<11) {
        tmp_vol = (tmp_vol<<1)|(databit?1:0);
    }
    bit_num++;
}

void snd_volume_interface_reset(void) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Interface reset.");
    
    bit_num = 0;
    chan_lr = 0;
    tmp_vol = 0;
}

void snd_save_volume_reg(void) {
    if (bit_num!=11) {
        Log_Printf(LOG_WARN, "[Sound] Incomplete volume transfer (%i bits).",bit_num);
        return;
    }
    if (tmp_vol>SND_MAX_VOL) {
        Log_Printf(LOG_WARN, "[Sound] Volume limit exceeded (%i).",tmp_vol);
        tmp_vol=SND_MAX_VOL;
    }
    if (chan_lr&1) {
        Log_Printf(LOG_WARN, "[Sound] Setting volume of left channel to %i",tmp_vol);
        sndout_state.volume[0] = tmp_vol;
    }
    if (chan_lr&2) {
        Log_Printf(LOG_WARN, "[Sound] Setting volume of right channel to %i",tmp_vol);
        sndout_state.volume[1] = tmp_vol;
    }
}

/* This function fills the internal volume register */
#define SND_SPEAKER_ENABLE  0x10
#define SND_LOWPASS_ENABLE  0x08

#define SND_INTFC_CLOCK     0x04
#define SND_INTFC_DATA      0x02
#define SND_INTFC_STROBE    0x01

Uint8 old_data;

void snd_gpo_access(Uint8 data) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Control logic access: %02X",data);
    
    sndout_state.mute = data&SND_SPEAKER_ENABLE;
    sndout_state.lowpass = data&SND_LOWPASS_ENABLE;
    
    if (data&SND_INTFC_STROBE) {
        snd_save_volume_reg();
    } else if ((data&SND_INTFC_CLOCK) && !(old_data&SND_INTFC_CLOCK)) {
        snd_access_volume_reg(data&SND_INTFC_DATA);
    } else if ((data&SND_INTFC_CLOCK) == (old_data&SND_INTFC_CLOCK)) {
        snd_volume_interface_reset();
    }
    old_data = data;
}
