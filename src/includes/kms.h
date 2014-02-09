void KMS_Ctrl_Snd_Write(void);
void KMS_Stat_Snd_Read(void);
void KMS_Ctrl_KM_Write(void);
void KMS_Stat_KM_Read(void);
void KMS_Ctrl_TX_Write(void);
void KMS_Stat_TX_Read(void);
void KMS_Ctrl_Cmd_Write(void);
void KMS_Stat_Cmd_Read(void);

void KMS_Data_Write(void);
void KMS_Data_Read(void);

void KMS_KM_Data_Read(void);

void kms_keydown(Uint8 modkeys, Uint8 keycode);
void kms_keyup(Uint8 modkeys, Uint8 keycode);