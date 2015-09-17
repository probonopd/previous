Uint32 nextbus_slot_lget(Uint32 addr);
Uint32 nextbus_slot_wget(Uint32 addr);
Uint32 nextbus_slot_bget(Uint32 addr);
void nextbus_slot_lput(Uint32 addr, Uint32 val);
void nextbus_slot_wput(Uint32 addr, Uint32 val);
void nextbus_slot_bput(Uint32 addr, Uint32 val);

Uint32 nextbus_board_lget(Uint32 addr);
Uint32 nextbus_board_wget(Uint32 addr);
Uint32 nextbus_board_bget(Uint32 addr);
void nextbus_board_lput(Uint32 addr, Uint32 val);
void nextbus_board_wput(Uint32 addr, Uint32 val);
void nextbus_board_bput(Uint32 addr, Uint32 val);

Uint32 nbic_reg_lget(Uint32 addr);
Uint32 nbic_reg_wget(Uint32 addr);
Uint32 nbic_reg_bget(Uint32 addr);
void nbic_reg_lput(Uint32 addr, Uint32 val);
void nbic_reg_wput(Uint32 addr, Uint32 val);
void nbic_reg_bput(Uint32 addr, Uint32 val);

void nextbus_init(void);
