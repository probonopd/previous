/* NeXT system registers emulation */

/* Interrupts */
#define INT_SOFT1       0x00000001  // level 1
#define INT_SOFT2       0x00000002  // level 2
#define INT_POWER       0x00000004  // level 3
#define INT_KEYMOUSE    0x00000008
#define INT_MONITOR     0x00000010
#define INT_VIDEO       0x00000020
#define INT_DSP_L3      0x00000040
#define INT_PHONE       0x00000080  // Floppy?
#define INT_SOUND_OVRUN 0x00000100
#define INT_EN_RX       0x00000200
#define INT_EN_TX       0x00000400
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
#define INT_EN_RX_DMA   0x08000000
#define INT_EN_TX_DMA   0x10000000
#define INT_TIMER       0x20000000
#define INT_PFAIL       0x40000000  // level 7
#define INT_NMI         0x80000000

/* Interrupt Level Masks */
#define INT_L7_MASK     0xC0000000
#define INT_L6_MASK     0x3FFC0000
#define INT_L5_MASK     0x00038000
#define INT_L4_MASK     0x00004000
#define INT_L3_MASK     0x00003FFC
#define INT_L2_MASK     0x00000002
#define INT_L1_MASK     0x00000001

#define SET_INT         1
#define RELEASE_INT     0

void set_interrupt(Uint32 intr, Uint8 state);
int get_interrupt_level(void);

void set_dsp_interrupt(Uint8 state);

void SID_Read(void);

void SCR_Reset(void);
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

void Hardclock_InterruptHandler(void);
void HardclockRead0(void);
void HardclockRead1(void);

void HardclockWrite0(void);
void HardclockWrite1(void);

void HardclockWriteCSR(void);
void HardclockReadCSR(void);

void System_Timer_Read(void);
void System_Timer_Write(void);

void ColorVideo_CMD_Write(void);
void color_video_interrupt(void);
