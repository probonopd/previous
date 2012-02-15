/* SCSI Bus and Disk emulation */


/* Opcodes */
/* The following are multi-sector transfers with seek implied */
#define HD_VERIFY_TRACK    0x05               /* Verify track */
#define HD_FORMAT_TRACK    0x06               /* Format track */
#define HD_READ_SECTOR     0x08               /* Read sector */
#define HD_READ_SECTOR1    0x28               /* Read sector (class 1) */
#define HD_WRITE_SECTOR    0x0A               /* Write sector */
#define HD_WRITE_SECTOR1   0x2A               /* Write sector (class 1) */

/* other codes */
#define HD_TEST_UNIT_RDY   0x00               /* Test unit ready */
#define HD_FORMAT_DRIVE    0x04               /* Format the whole drive */
#define HD_SEEK            0x0B               /* Seek */
#define HD_CORRECTION      0x0D               /* Correction */
#define HD_INQUIRY         0x12               /* Inquiry */
#define HD_MODESELECT      0x15               /* Mode select */
#define HD_MODESENSE       0x1A               /* Mode sense */
#define HD_REQ_SENSE       0x03               /* Request sense */
#define HD_SHIP            0x1B               /* Ship drive */
#define HD_READ_CAPACITY1  0x25               /* Read capacity (class 1) */

/* Status codes */
#define HD_STATUS_OK       0x00               /* OK */
#define HD_STATUS_ERROR    0x02               /* Check Condition */
#define HD_STATUS_CONDMET  0x04               /* Condition Met */
#define HD_STATUS_BUSY     0x08               /* Busy */

/* Error codes for REQUEST SENSE: */
#define HD_REQSENS_OK       0x00              /* OK return status */
#define HD_REQSENS_NOSECTOR 0x01              /* No index or sector */
#define HD_REQSENS_WRITEERR 0x03              /* Write fault */
#define HD_REQSENS_OPCODE   0x20              /* Opcode not supported */
#define HD_REQSENS_INVADDR  0x21              /* Invalid block address */
#define HD_REQSENS_INVARG   0x24              /* Invalid argument */
#define HD_REQSENS_NODRIVE  0x25              /* Invalid drive */


typedef struct {
    int readCount;    /* count of number of command bytes written */
    unsigned char source_busid;
    unsigned char target;
    unsigned char lun;
    unsigned char opcode;
    bool nodevice;
    bool timeout;
    int transfer_data_len;
    int transferdirection_todevice;
    bool extended;
    
    int byteCount;             /* count of number of command bytes written */
    unsigned char command[10];
    short int returnCode;      /* return code from the HDC operation */
} SCSICOMMAND;

/* SCSI globals */
SCSICOMMAND SCSIcommand;

/* Mode Pages */
#define MODEPAGE_MAX_SIZE 24
typedef struct {
    Uint8 current[MODEPAGE_MAX_SIZE];
    Uint8 changeable[MODEPAGE_MAX_SIZE];
    Uint8 modepage[MODEPAGE_MAX_SIZE]; // default values
    Uint8 saved[MODEPAGE_MAX_SIZE];
    Uint8 pagesize;
} MODEPAGE;




void SCSI_Init(void);
void SCSI_Uninit(void);
void scsi_command_analyzer(Uint8 command[], int size, int target,int lun);

/* Helpers */
int SCSI_GetTransferLength(void);
unsigned long SCSI_GetOffset(void);
int SCSI_GetCount(void);
MODEPAGE SCSI_GetModePage(Uint8 pagecode);


void SCSI_Emulate_Command(void);

/* SCSI Commands */
void SCSI_Inquiry (void);
void SCSI_StartStop(void);
void SCSI_TestUnitReady(void);
void SCSI_ReadCapacity(void);
void SCSI_ReadSector(void);
void SCSI_RequestSense(void);
void SCSI_ModeSense(void);
