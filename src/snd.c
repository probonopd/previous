#include "main.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "cycInt.h"
#include "audio.h"
#include "dma.h"
#include "snd.h"
#include "kms.h"

#define LOG_SND_LEVEL   LOG_DEBUG
#define LOG_VOL_LEVEL   LOG_DEBUG

/* Initialize the audio system */
static bool   sndout_inited;
static bool   sound_output_active = false;
static bool   sndin_inited;
static bool   sound_input_active = false;
static Uint8* snd_buffer = NULL;

static void sound_init(void) {
    if(snd_buffer)
        free(snd_buffer);
    snd_buffer = NULL;
    if (!sndout_inited && ConfigureParams.Sound.bEnableSound) {
        Log_Printf(LOG_WARN, "[Audio] Initializing audio device.");
        Audio_Output_Init();
        sndout_inited=true;
    }
}

static void sound_uninit(void) {
    if(snd_buffer)
        free(snd_buffer);
    snd_buffer = NULL;
    if(sndout_inited) {
        Log_Printf(LOG_WARN, "[Audio] Uninitializing audio device.");
        sndout_inited=false;
        Audio_Output_UnInit();
    }
}

void Sound_Reset(void) {
    sound_uninit();
    sound_init();
    if (sound_output_active && sndout_inited) {
        Audio_Output_Enable(true);
    }
}

void Sound_Pause(bool pause) {
    if (pause) {
        if (sndout_inited) {
            Log_Printf(LOG_WARN, "[Audio] Uninitializing audio output device (pause).");
            sndout_inited=false;
            Audio_Output_UnInit();
        }
        if (sndin_inited) {
            Log_Printf(LOG_WARN, "[Audio] Uninitializing audio input device (pause).");
            sndin_inited=false;
            Audio_Input_UnInit();
        }
    } else {
        if (!sndout_inited && ConfigureParams.Sound.bEnableSound) {
            Log_Printf(LOG_WARN, "[Audio] Initializing audio output device (resume).");
            Audio_Output_Init();
            sndout_inited=true;
        }
        if (!sndin_inited && sound_input_active && ConfigureParams.Sound.bEnableSound) {
            Log_Printf(LOG_WARN, "[Audio] Initializing audio input device (resume).");
            Audio_Input_Init();
            sndin_inited=true;
        }
        if (sound_output_active && sndout_inited) {
            Audio_Output_Enable(true);
        }
        if (sound_input_active && sndin_inited) {
            Audio_Input_Enable(true);
        }
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
int  snd_send_samples(Uint8* bufffer, int len);
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
        Log_Printf(LOG_SND_LEVEL, "[Audio] Not starting. Audio output device not initialized.");
    }
    /* Starting sound output loop */
    if (!sound_output_active) {
        Log_Printf(LOG_SND_LEVEL, "[Sound] Starting output loop.");
        sound_output_active = true;
        CycInt_AddRelativeInterruptCycles(10, INTERRUPT_SND_OUT);
    } else { /* Even re-enable loop if we are already active. This lowers the delay. */
        Log_Printf(LOG_DEBUG, "[Sound] Restarting output loop.");
        CycInt_AddRelativeInterruptCycles(10, INTERRUPT_SND_OUT);
    }
}

void snd_stop_output(void) {
    sound_output_active=false;
}

void snd_start_input(Uint8 mode) {
    
    /* Starting SDL Audio */
    if (sndin_inited) {
        Audio_Input_Enable(true);
    } else if (ConfigureParams.Sound.bEnableSound) {
        sndin_inited = true;
        Audio_Input_Init();
        Audio_Input_Enable(true);
    }
    /* Starting sound output loop */
    if (!sound_input_active) {
        Log_Printf(LOG_SND_LEVEL, "[Sound] Starting input loop.");
        sound_input_active = true;
        CycInt_AddRelativeInterruptCycles(10, INTERRUPT_SND_IN);
    } else { /* Even re-enable loop if we are already active. This lowers the delay. */
        Log_Printf(LOG_DEBUG, "[Sound] Restarting input loop.");
        CycInt_AddRelativeInterruptCycles(10, INTERRUPT_SND_IN);
    }
}

void snd_stop_input(void) {
    sound_input_active=false;
    sndin_inited = false;
    Audio_Input_UnInit();
}

/* Sound IO loops */

static void do_dma_sndout_intr(void) {
    if(snd_buffer) {
        dma_sndout_intr();
        free(snd_buffer);
        snd_buffer = NULL;
    }
}

/*
 At a playback rate of 44.1kHz a sample takes about 23 microseconds.
 Assuming that the emulation runs at least 1/3 as fast as a real m68k
 checking the sound queue every 8 microseconds should be ok.
*/
static const int SND_CHECK_DELAY = 8;
void SND_Out_Handler(void) {
    int len;

    CycInt_AcknowledgeInterrupt();
    
    if (!sound_output_active) {
        return;
    }

    if (sndout_inited && Audio_Output_Queue_Size() > AUDIO_BUFFER_SAMPLES * 2) {
        CycInt_AddRelativeInterruptUs(SND_CHECK_DELAY * AUDIO_BUFFER_SAMPLES, 0, INTERRUPT_SND_OUT);
        return;
    }
    
    do_dma_sndout_intr();
    snd_buffer = dma_sndout_read_memory(&len);
    
    if (len) {
        len = snd_send_samples(snd_buffer, len);
        len = (len / 4) + 1;
        CycInt_AddRelativeInterruptUs(SND_CHECK_DELAY * len, 0, INTERRUPT_SND_OUT);
    } else {
        kms_sndout_underrun();
        /* Call do_dma_sndout_intr() a little bit later */
        CycInt_AddRelativeInterruptUs(100, 0, INTERRUPT_SND_OUT);
    }
}

bool snd_output_active() {
    return sound_output_active;
}

void SND_In_Handler(void) {
    CycInt_AcknowledgeInterrupt();
    
    int dma_done = dma_sndin_write_memory();
	
	if (dma_done) {
		if(snd_input_active()) {
			kms_sndin_overrun();
		}
	} else {
		CycInt_AddRelativeInterruptUs(10000, 0, INTERRUPT_SND_IN);
	}
}

bool snd_input_active() {
    return sound_input_active;
}

/* This functions generates 8-bit ulaw samples from 16 bit pcm audio */
#define BIAS 0x84               /* define the add-in bias for 16 bit samples */
#define CLIP 32635

Uint8 snd_make_ulaw(Sint16 sample) {
	static Sint16 exp_lut[256] = {
		0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
		4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
	};
	Sint16 sign, exponent, mantissa;
	Uint8 ulawbyte;
	
	/** get the sample into sign-magnitude **/
	sign = (sample >> 8) & 0x80;        /* set aside the sign */
	if (sign != 0) {
		sample = -sample;         /* get magnitude */
	}
	/* sample can be zero because we can overflow in the inversion,
	 * checking against the unsigned version solves this */
	if (((Uint16) sample) > CLIP)
		sample = CLIP;            /* clip the magnitude */
	
	/** convert from 16 bit linear to ulaw **/
	sample = sample + BIAS;
	exponent = exp_lut[(sample >> 7) & 0xFF];
	mantissa = (sample >> (exponent + 3)) & 0x0F;
	ulawbyte = ~(sign | (exponent << 4) | mantissa);
	
	return ulawbyte;
}


/* These functions put samples to a buffer for further processing */
void snd_make_double_samples(Uint8 *buffer, int len, bool repeat) {
    for (int i=len - 4; i >= 0; i -= 4) {
        buffer[i*2+7] = repeat ? buffer[i+3] : 0; /* repeat or zero-fill */
        buffer[i*2+6] = repeat ? buffer[i+2] : 0; /* repeat or zero-fill */
        buffer[i*2+5] = repeat ? buffer[i+1] : 0; /* repeat or zero-fill */
        buffer[i*2+4] = repeat ? buffer[i+0] : 0; /* repeat or zero-fill */
        buffer[i*2+3] =          buffer[i+3];
        buffer[i*2+2] =          buffer[i+2];
        buffer[i*2+1] =          buffer[i+1];
        buffer[i*2+0] =          buffer[i+0];
    }
}


void snd_make_normal_samples(Uint8 *buffer, int len) {
    // do nothing
}


/* This function processes and sends out our samples */
int snd_send_samples(Uint8* buffer, int len) {
    switch (sndout_state.mode) {
        case SND_MODE_NORMAL:
            snd_make_normal_samples(buffer, len);
            snd_adjust_volume_and_lowpass(buffer, len);
            Audio_Output_Queue(buffer, len);
            return len;
        case SND_MODE_DBL_RP:
            snd_make_double_samples(buffer, len, true);
            snd_adjust_volume_and_lowpass(buffer, 2*len);
            Audio_Output_Queue(buffer, len);
            Audio_Output_Queue(buffer+len, len);
            return 2*len;
        case SND_MODE_DBL_ZF:
            snd_make_double_samples(buffer, len, false);
            snd_adjust_volume_and_lowpass(buffer, 2*len);
            Audio_Output_Queue(buffer, len);
            Audio_Output_Queue(buffer+len, len);
            return 2*len;
        default:
            Log_Printf(LOG_WARN, "[Sound] Error: Unknown sound output mode!");
            return 0;
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
    if (sndout_state.mute) {
        for (i=0; i<len; i++) {
            buf[i] = 0;
        }
    } else if (sndout_state.volume[0] || sndout_state.volume[1] || sndout_state.lowpass) {
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

static void snd_access_volume_reg(Uint8 databit) {
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

static void snd_volume_interface_reset(void) {
    Log_Printf(LOG_VOL_LEVEL, "[Sound] Interface reset.");
    
    bit_num = 0;
    chan_lr = 0;
    tmp_vol = 0;
}

static void snd_save_volume_reg(void) {
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
