#include "main.h"
#include "dialog.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "audio.h"
#include "dma.h"
#include "snd.h"
#include "host.h"

#include <SDL.h>


/* Sound emulation SDL interface */
static SDL_AudioDeviceID Audio_Input_Device;
static SDL_AudioDeviceID Audio_Output_Device;

static bool          bSoundOutputWorking = false; /* Is sound output OK */
static bool          bSoundInputWorking  = false; /* Is sound input OK */
static bool          bSoundOutAlertShown = false;
static bool          bSoundInAlertShown  = false;
static bool          bPlayingBuffer      = false; /* Is playing buffer? */
static bool          bRecordingBuffer    = false; /* Is recording buffer? */
#define              REC_BUFFER_SZ       16  /* Recording buffer size in power of two */
static const  Uint32 REC_BUFFER_MASK     = (1<<REC_BUFFER_SZ) - 1;
static Uint8         recBuffer[1<<REC_BUFFER_SZ];
static Uint32        recBufferWr         = 0;
static Uint32        recBufferRd         = 0;
static lock_t        recBufferLock;

void Audio_Output_Queue(Uint8* data, int len) {
    int chunkSize = AUDIO_BUFFER_SAMPLES;
    while(len > 0) {
        if(len < chunkSize) chunkSize = len;
        SDL_QueueAudio(Audio_Output_Device, data, chunkSize);
        data += chunkSize;
        len  -= chunkSize;
    }
}

Uint32 Audio_Output_Queue_Size() {
    return SDL_GetQueuedAudioSize(Audio_Output_Device) / 4;
}

/*-----------------------------------------------------------------------*/
/**
 * SDL audio callback functions - move sound between emulation and audio system.
 * Note: These functions will run in a separate thread.
 */

static void Audio_Input_CallBack(void *userdata, Uint8 *stream, int len) {
    Log_Printf(LOG_WARN, "Audio_Input_CallBack %d", len);
    if(len == 0) return;
    Audio_Input_Lock();
    while(len--)
        recBuffer[recBufferWr++&REC_BUFFER_MASK] = *stream++;
	recBufferWr &= REC_BUFFER_MASK;
	recBufferWr &= ~1; /* Just to be sure */
    Audio_Input_Unlock();
}

void Audio_Input_Lock() {
    host_lock(&recBufferLock);
}

/* 
 * Initialize recording buffer with silence to compensate for time gap
 * between Audio_Input_Enable and first call of Audio_Input_CallBack.
 */
#define AUDIO_RECBUF_INIT	0 /* 16000 byte = 1 second */

void Audio_Input_InitBuf() {
	recBufferRd = 0;
	for (recBufferWr = 0; recBufferWr < AUDIO_RECBUF_INIT; recBufferWr++) {
		recBuffer[recBufferWr] = 0;
	}
}

int Audio_Input_Read() {
	Sint16 sample = 0;
	
    if (bSoundInputWorking) {
		if ((recBufferRd&REC_BUFFER_MASK)==(recBufferWr&REC_BUFFER_MASK)) {
			return -1;
		} else {
			sample = ((recBuffer[recBufferRd&REC_BUFFER_MASK]<<8)|recBuffer[(recBufferRd&REC_BUFFER_MASK)+1]);
			recBufferRd += 2;
			recBufferRd &= REC_BUFFER_MASK;
			return snd_make_ulaw(sample);
		}
	} else {
        return 0; // silence
	}
}

void Audio_Input_Unlock() {
    host_unlock(&recBufferLock);
}

static bool check_audio(int requested, int granted, const char* attribute) {
    if(requested != granted)
        Log_Printf(LOG_WARN, "[Audio] %s mismatch. Requested:%d, granted:%d", attribute, requested, granted);
    return requested == granted;
}

/*-----------------------------------------------------------------------*/
/**
 * Initialize the audio subsystem. Return true if all OK.
 */
void Audio_Output_Init(void)
{
    SDL_AudioSpec request;    /* We fill in the desired SDL audio options here */
    SDL_AudioSpec granted;
    
    /* Init the SDL's audio subsystem: */
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            Log_Printf(LOG_WARN, "[Audio] Could not init audio output: %s\n", SDL_GetError());
            DlgAlert_Notice("Error: Can't open SDL audio subsystem.");
            bSoundOutputWorking = false;
            return;
        }
    }
    
    /* Set up SDL audio: */
    request.freq     = AUDIO_OUT_FREQUENCY; /* 44,1 kHz */
    request.format   = AUDIO_S16MSB;        /* 16-Bit signed, big endian */
    request.channels = 2;                   /* stereo */
    request.callback = NULL;
    request.userdata = NULL;
    request.samples  = AUDIO_BUFFER_SAMPLES; /* buffer size in samples */

    Audio_Output_Device = SDL_OpenAudioDevice(NULL, 0, &request, &granted, 0);
    if (Audio_Output_Device==0)	/* Open audio device */ {
        Log_Printf(LOG_WARN, "[Audio] Can't use audio: %s\n", SDL_GetError());
        if(!bSoundOutAlertShown) {
            DlgAlert_Notice("Error: Can't open audio output device. No sound output.");
            bSoundOutAlertShown = true;
        }
        bSoundOutputWorking = false;
        return;
    }
    bSoundOutputWorking = true;
    bSoundOutputWorking &= check_audio(request.freq,     granted.freq,     "freq");
    bSoundOutputWorking &= check_audio(request.format,   granted.format,   "format");
    bSoundOutputWorking &= check_audio(request.channels, granted.channels, "channels");
    bSoundOutputWorking &= check_audio(request.samples,  granted.samples,  "samples");

    if(!(bSoundOutputWorking)) {
        DlgAlert_Notice("Error: Can't open audio output device. No sound output.");
    }
}

void Audio_Input_Init(void) {    
    SDL_AudioSpec request;    /* We fill in the desired SDL audio options here */
    SDL_AudioSpec granted;
    
    /* Init the SDL's audio subsystem: */
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            Log_Printf(LOG_WARN, "Could not init audio input: %s\n", SDL_GetError());
            DlgAlert_Notice("Error: Can't open SDL audio subsystem.");
            bSoundInputWorking = false;
            return;
        }
    }
    
    /* Set up SDL audio: */
    request.freq     = AUDIO_IN_FREQUENCY; /* 8kHz */
    request.format   = AUDIO_S16MSB;	   /* 16-Bit signed, big endian */
    request.channels = 1;			       /* mono */
    request.callback = Audio_Input_CallBack;
    request.userdata = NULL;
    request.samples  = AUDIO_BUFFER_SAMPLES; /* buffer size in samples */
    
    Audio_Input_Device = SDL_OpenAudioDevice(NULL, 1, &request, &granted, 0); /* Open audio device */
    
    if (Audio_Input_Device==0){
        Log_Printf(LOG_WARN, "Can't use audio: %s\n", SDL_GetError());
        if(!bSoundInAlertShown) {
            DlgAlert_Notice("Error: Can't open audio input device. No sound recording (will record silence instead).");
            bSoundInAlertShown = true;
        }
        bSoundInputWorking = false;
        return;
    }
    
    bSoundInputWorking = true;
    bSoundInputWorking &= check_audio(request.freq,     granted.freq,     "freq");
    bSoundInputWorking &= check_audio(request.format,   granted.format,   "format");
    bSoundInputWorking &= check_audio(request.channels, granted.channels, "channels");
    bSoundInputWorking &= check_audio(request.samples,  granted.samples,  "samples");
	
	if(!(bSoundInputWorking)) {
		DlgAlert_Notice("Error: Can't open audio input device. No sound recording (will record silence instead).");
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Free audio subsystem
 */
void Audio_Output_UnInit(void) {
    if (bSoundOutputWorking) {
        /* Stop */
        Audio_Output_Enable(false);
        
        SDL_CloseAudioDevice(Audio_Output_Device);
        
        bSoundOutputWorking = false;
    }
}

void Audio_Input_UnInit(void) {
    if (bSoundInputWorking) {
        /* Stop */
        Audio_Input_Enable(false);
        
        SDL_CloseAudioDevice(Audio_Input_Device);
        
        bSoundInputWorking = false;
    }
}

/*-----------------------------------------------------------------------*/
/**
 * Start/Stop sound buffer
 */
void Audio_Output_Enable(bool bEnable) {
    if (bEnable && !bPlayingBuffer) {
        /* Start playing */
        SDL_PauseAudioDevice(Audio_Output_Device, false);
        bPlayingBuffer = true;
    }
    else if (!bEnable && bPlayingBuffer) {
        /* Stop from playing */
        SDL_PauseAudioDevice(Audio_Output_Device, true);
        bPlayingBuffer = false;
    }
}

void Audio_Input_Enable(bool bEnable) {
    if (bEnable && !bRecordingBuffer) {
        /* Start recording */
		Audio_Input_InitBuf();
        SDL_PauseAudioDevice(Audio_Input_Device, false);
        bRecordingBuffer = true;
    }
    else if (!bEnable && bRecordingBuffer) {
        /* Stop recording */
        SDL_PauseAudioDevice(Audio_Input_Device, true);
        bRecordingBuffer = false;
    }
}
