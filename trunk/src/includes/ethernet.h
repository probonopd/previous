void EN_TX_Status_Read(void);
void EN_TX_Status_Write(void);
void EN_TX_Mask_Read(void);
void EN_TX_Mask_Write(void);
void EN_RX_Status_Read(void);
void EN_RX_Status_Write(void);
void EN_RX_Mask_Read(void);
void EN_RX_Mask_Write(void);
void EN_TX_Mode_Read(void);
void EN_TX_Mode_Write(void);
void EN_RX_Mode_Read(void);
void EN_RX_Mode_Write(void);
void EN_Reset_Write(void);
void EN_NodeID0_Read(void);
void EN_NodeID0_Write(void);
void EN_NodeID1_Read(void);
void EN_NodeID1_Write(void);
void EN_NodeID2_Read(void);
void EN_NodeID2_Write(void);
void EN_NodeID3_Read(void);
void EN_NodeID3_Write(void);
void EN_NodeID4_Read(void);
void EN_NodeID4_Write(void);
void EN_NodeID5_Read(void);
void EN_NodeID5_Write(void);
void EN_CounterLo_Read(void);
void EN_CounterHi_Read(void);


struct {
    Uint8 data[64*1024];
    int size;
    int limit;
} enet_tx_buffer;

struct {
    Uint8 data[64*1024];
    int size;
    int limit;
} enet_rx_buffer;

void ENET_IO_Handler(void);
void Ethernet_Reset(bool hard);
void enet_receive(Uint8 *pkt, int len);

/* Turbo ethernet controller */
void EN_Control_Read(void);
void EN_Control_Write(void);
