void SND_IO_Handler(void);
void Sound_Reset(void);

void sndout_queue_poll(Uint8 *buf, int len);
void snd_start_output(Uint8 mode);
void snd_stop_output(void);
void snd_gpo_access(Uint8 data);


struct {
    Uint8 data[4096];
    Uint32 size;
    Uint32 limit;
} snd_buffer;
