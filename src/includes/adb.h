Uint32 adb_lget(Uint32 addr);
Uint16 adb_wget(Uint32 addr);
Uint8 adb_bget(Uint32 addr);

void adb_lput(Uint32 addr, Uint32 l);
void adb_wput(Uint32 addr, Uint16 w);
void adb_bput(Uint32 addr, Uint8 b);

void ADB_Reset(void);
