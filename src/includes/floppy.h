void FLP_StatA_Read(void);
void FLP_StatB_Read(void);
void FLP_DataOut_Read(void);
void FLP_DataOut_Write(void);
void FLP_Status_Read(void);
void FLP_DataRate_Write(void);
void FLP_FIFO_Read(void);
void FLP_FIFO_Write(void);
void FLP_DataIn_Read(void);
void FLP_Configuration_Write(void);
void FLP_Control_Read(void);
void FLP_Select_Write(void);

void FLP_IO_Handler(void);

void Floppy_Reset(void);
int Floppy_Insert(int drive);
void Floppy_Eject(int drive);

struct {
    Uint8 data[1024];
    Uint32 size;
    Uint32 limit;
} flp_buffer;

extern Uint8 floppy_select;
void set_floppy_select(Uint8 sel, bool osp);
