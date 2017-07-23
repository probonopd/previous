uae_u32 bmap_lget(uaecptr addr);
uae_u32 bmap_wget(uaecptr addr);
uae_u32 bmap_bget(uaecptr addr);

void bmap_lput(uaecptr addr, uae_u32 l);
void bmap_wput(uaecptr addr, uae_u32 w);
void bmap_bput(uaecptr addr, uae_u32 b);

void bmap_init(void);

extern int bmap_tpe_select;
