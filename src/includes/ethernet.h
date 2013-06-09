void Ethernet_Read(void);
void Ethernet_Write(void);

void Ethernet_Reset(void);

void Ethernet_Transmit(void);
void Ethernet_Receive(Uint8 *packet, Uint32 size);

bool Packet_Receiver_Me(Uint8 *packet);
bool Me(Uint8 *packet);
bool Multicast(Uint8 *packet);
bool Local_Multicast(Uint8 *packet);
bool Broadcast(Uint8 *packet);