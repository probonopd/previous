/* SCSI Bus and Disk emulation */

/* SCSI phase */
#define PHASE_DO      0x00 /* data out */
#define PHASE_DI      0x01 /* data in */
#define PHASE_CD      0x02 /* command */
#define PHASE_ST      0x03 /* status */
#define PHASE_MO      0x06 /* message out */
#define PHASE_MI      0x07 /* message in */

struct {
    Uint8 target;
    Uint8 phase;
} SCSIbus;


/* Command Descriptor Block */
#define SCSI_CDB_MAX_SIZE 12


/* This buffer temporarily stores data to be written to memory or disk */

struct {
    Uint8 data[512]; /* FIXME: BLOCKSIZE */
    int limit;
    int size;
    bool disk;
} scsi_buffer;


void SCSI_Init(void);
void SCSI_Uninit(void);
void SCSI_Reset(void);
void SCSI_Insert(Uint8 target);
void SCSI_Eject(Uint8 target);

Uint8 SCSIdisk_Send_Status(void);
Uint8 SCSIdisk_Send_Message(void);
Uint8 SCSIdisk_Send_Data(void);
void SCSIdisk_Receive_Data(Uint8 val);
bool SCSIdisk_Select(Uint8 target);
void SCSIdisk_Receive_Command(Uint8 *commandbuf, Uint8 identify);

Sint64 SCSI_Seek_Time(void);
Sint64 SCSI_Sector_Time(void);
