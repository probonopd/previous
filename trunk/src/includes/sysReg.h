/* NeXT system registers emulation */


#define INT_SOFT1       0x00000001  // level 1
#define INT_SOFT2       0x00000002  // level 2
#define INT_POWER       0x00000004  // level 3
#define INT_KEYMOUSE    0x00000008
#define INT_MONITOR     0x00000010
#define INT_VIDEO       0x00000020
#define INT_DSP_L3      0x00000040
#define INT_PHONE       0x00000080  // Floppy?
#define INT_SOUND_OVRUN 0x00000100
#define INT_ENETR       0x00000200
#define INT_ENETX       0x00000400
#define INT_PRINTER     0x00000800
#define INT_SCSI        0x00001000
#define INT_DISK        0x00002000  // in color systems this is INT_C16VIDEO
#define INT_DSP_L4      0x00004000  // level 4
#define INT_BUS         0x00008000  // level 5
#define INT_REMOTE      0x00010000
#define INT_SCC         0x00020000
#define INT_R2M_DMA     0x00040000  // level 6
#define INT_M2R_DMA     0x00080000
#define INT_DSP_DMA     0x00100000
#define INT_SCC_DMA     0x00200000
#define INT_SND_IN_DMA  0x00400000
#define INT_SND_OUT_DMA 0x00800000
#define INT_PRINTER_DMA 0x01000000
#define INT_DISK_DMA    0x02000000
#define INT_SCSI_DMA    0x04000000
#define INT_ENETR_DMA   0x08000000
#define INT_ENETX_DMA   0x10000000
#define INT_TIMER       0x20000000
#define INT_PFAIL       0x40000000  // level 7
#define INT_NMI         0x80000000

#define SET_INT         0x01
#define RELEASE_INT     0x00

void SID_Read(void);

void SCR1_Read0(void);
void SCR1_Read1(void);
void SCR1_Read2(void);
void SCR1_Read3(void);

void SCR2_Read0(void);
void SCR2_Write0(void);
void SCR2_Read1(void);
void SCR2_Write1(void);
void SCR2_Read2(void);
void SCR2_Write2(void);
void SCR2_Read3(void);
void SCR2_Write3(void);

void IntRegStatRead(void);
void IntRegStatWrite(void);
void IntRegMaskRead(void);
void IntRegMaskWrite(void);

void set_interrupt(Uint32, Uint8);
