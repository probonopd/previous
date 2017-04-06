#define AUDIO_OUT_FREQUENCY  44100            /* Sound playback frequency */
#define AUDIO_IN_FREQUENCY   8012             /* Sound recording frequency */
#define AUDIO_BUFFER_SAMPLES 512

void SND_Out_Handler(void);
void SND_In_Handler(void);
void Sound_Reset(void);

Uint8 snd_make_ulaw(Sint16 sample);

void snd_start_output(Uint8 mode);
void snd_stop_output(void);
void snd_start_input(Uint8 mode);
void snd_stop_input(void);
bool snd_output_active(void);
bool snd_input_active(void);
void snd_gpo_access(Uint8 data);
