#include "main.h"
#include "dialog.h"
#include "configuration.h"
#include "m68000.h"
#include "sysdeps.h"
#include "audio.h"
#include "dma.h"
#include "snd.h"

#include <SDL.h>


/* Sound emulation SDL interface */
SDL_AudioDeviceID Audio_Input_Device;
SDL_AudioDeviceID Audio_Output_Device;

int nAudioFrequency = 44100;            /* Sound playback frequency */
bool bSoundOutputWorking = false;       /* Is sound output OK */
bool bSoundInputWorking = false;        /* Is sound input OK */
volatile bool bPlayingBuffer = false;   /* Is playing buffer? */
volatile bool bRecordingBuffer = false; /* Is recording buffer? */


/*-----------------------------------------------------------------------*/
/**
 * SDL audio callback functions - move sound between emulation and audio system.
 * Note: These functions will run in a separate thread.
 */
static void Audio_Output_CallBack(void *userdata, Uint8 *stream, int len)
{
    //printf("AUDIO CALLBACK, size = %i\n",len);
    sndout_queue_poll(stream, len);
}

static void Audio_Input_CallBack(void *userdata, Uint8 *stream, int len)
{
    /* FIXME: send data to emulator */
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
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        {
            Log_Printf(LOG_WARN, "Could not init audio output: %s\n", SDL_GetError());
            DlgAlert_Notice("Error: Can't open SDL audio subsystem.");
            bSoundOutputWorking = false;
            return;
        }
    }
    
    /* Set up SDL audio: */
    request.freq = 44100;           /* 44,1 kHz */
    request.format = AUDIO_S16MSB;	/* 16-Bit signed, big endian */
    request.channels = 2;			/* stereo */
    request.callback = Audio_Output_CallBack;
    request.userdata = NULL;
    request.samples = 1024;	/* buffer size in samples (4096 byte) */

    Audio_Output_Device = SDL_OpenAudioDevice(NULL, 0, &request, &granted, 0);
    if (Audio_Output_Device==0)	/* Open audio device */
    {
        Log_Printf(LOG_WARN, "Can't use audio: %s\n", SDL_GetError());
        DlgAlert_Notice("Error: Can't open audio output device. No sound output.");
        bSoundOutputWorking = false;
        return;
    }

    /* All OK */
    bSoundOutputWorking = true;
}

void Audio_Input_Init(void)
{
    SDL_AudioSpec request;    /* We fill in the desired SDL audio options here */
    SDL_AudioSpec granted;
    
    /* Init the SDL's audio subsystem: */
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        {
            Log_Printf(LOG_WARN, "Could not init audio input: %s\n", SDL_GetError());
            DlgAlert_Notice("Error: Can't open SDL audio subsystem.");
            bSoundInputWorking = false;
            return;
        }
    }
    
    /* Set up SDL audio: */
    request.freq = 8000;
    request.format = AUDIO_S16MSB;	/* 16-Bit signed, big endian */
    request.channels = 1;			/* mono */
    request.callback = Audio_Input_CallBack;
    request.userdata = NULL;
    request.samples = 1024;	/* buffer size in samples (2048 byte) */
    
    Audio_Input_Device = SDL_OpenAudioDevice(NULL, 0, &request, &granted, 0);
    if (Audio_Input_Device==0)	/* Open audio device */
    {
        Log_Printf(LOG_WARN, "Can't use audio: %s\n", SDL_GetError());
        DlgAlert_Notice("Error: Can't open audio input device. No sound recording.");
        bSoundInputWorking = false;
        return;
    }
    
    /* All OK */
    bSoundInputWorking = true;
}


/*-----------------------------------------------------------------------*/
/**
 * Free audio subsystem
 */
void Audio_Output_UnInit(void)
{
    if (bSoundOutputWorking)
    {
        /* Stop */
        Audio_Output_Enable(false);
        
        SDL_CloseAudioDevice(Audio_Output_Device);
        
        bSoundOutputWorking = false;
    }
}

void Audio_Input_UnInit(void)
{
    if (bSoundInputWorking)
    {
        /* Stop */
        Audio_Input_Enable(false);
        
        SDL_CloseAudioDevice(Audio_Input_Device);
        
        bSoundInputWorking = false;
    }
}


/*-----------------------------------------------------------------------*/
/**
 * Lock the audio sub system so that the callback function will not be called.
 */
void Audio_Output_Lock(void)
{
    SDL_LockAudioDevice(Audio_Output_Device);
}

void Audio_Input_Lock(void)
{
    SDL_LockAudioDevice(Audio_Input_Device);
}


/*-----------------------------------------------------------------------*/
/**
 * Unlock the audio sub system so that the callback function will be called again.
 */
void Audio_Output_Unlock(void)
{
    SDL_UnlockAudioDevice(Audio_Output_Device);
}

void Audio_Input_Unlock(void)
{
    SDL_UnlockAudioDevice(Audio_Input_Device);
}


/*-----------------------------------------------------------------------*/
/**
 * Set audio playback frequency variable, pass as PLAYBACK_xxxx
 */
void Audio_Output_SetFreq(int nNewFrequency)
{
    /* Do not reset sound system if nothing has changed! */
    if (nNewFrequency != nAudioFrequency)
    {
        Log_Printf(LOG_WARN, "[SDL Audio] Changing frequency from %i Hz to %i Hz",
                   nAudioFrequency,nNewFrequency);
        /* Set new frequency */
        nAudioFrequency = nNewFrequency;
        
        /* Re-open SDL audio interface if necessary: */
        if (bSoundOutputWorking)
        {
            Audio_Output_UnInit();
            Audio_Output_Init();
        }
    }
}


/*-----------------------------------------------------------------------*/
/**
 * Start/Stop sound buffer
 */
void Audio_Output_Enable(bool bEnable)
{
    if (bEnable && !bPlayingBuffer)
    {
        /* Start playing */
        SDL_PauseAudioDevice(Audio_Output_Device, false);
        bPlayingBuffer = true;
    }
    else if (!bEnable && bPlayingBuffer)
    {
        /* Stop from playing */
        SDL_PauseAudioDevice(Audio_Output_Device, true);
        bPlayingBuffer = false;
    }
}

void Audio_Input_Enable(bool bEnable)
{
    if (bEnable && !bRecordingBuffer)
    {
        /* Start playing */
        SDL_PauseAudioDevice(Audio_Input_Device, false);
        bRecordingBuffer = true;
    }
    else if (!bEnable && bPlayingBuffer)
    {
        /* Stop from playing */
        SDL_PauseAudioDevice(Audio_Input_Device, true);
        bRecordingBuffer = false;
    }
}
