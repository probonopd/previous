Uint32 nd_nbic_lget(Uint32 addr);
Uint16 nd_nbic_wget(Uint32 addr);
Uint8 nd_nbic_bget(Uint32 addr);
void nd_nbic_lput(Uint32 addr, Uint32 l);
void nd_nbic_wput(Uint32 addr, Uint16 w);
void nd_nbic_bput(Uint32 addr, Uint8 b);

void nd_nbic_init(void);
void nd_nbic_set_intstatus(bool set);
void nd_nbic_interrupt(void);
