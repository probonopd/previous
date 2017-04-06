typedef uae_u32 (*nd_mem_get_func)(uaecptr) REGPARAM;
typedef void (*nd_mem_put_func)(uaecptr, uae_u32) REGPARAM;

typedef struct {
	/* These ones should be self-explanatory... */
	mem_get_func lget, wget, bget;
	mem_put_func lput, wput, bput;
	mem_get_func cs8geti;
	int flags;
} nd_addrbank;

#define nd_bankindex(addr) (((uaecptr)(addr)) >> 16)

#define nd_call_mem_get_func(func, addr) ((*func)(addr))
#define nd_call_mem_put_func(func, addr, v) ((*func)(addr, v))

extern nd_addrbank *nd_mem_banks[65536];

#define nd_get_mem_bank(addr) (*nd_mem_banks[bankindex(addr)])
#define nd_put_mem_bank(addr, b) (nd_mem_banks[bankindex(addr)] = (b))

#define nd_longget(addr) (nd_call_mem_get_func(nd_get_mem_bank(addr).lget, addr))
#define nd_wordget(addr) (nd_call_mem_get_func(nd_get_mem_bank(addr).wget, addr))
#define nd_byteget(addr) (nd_call_mem_get_func(nd_get_mem_bank(addr).bget, addr))
#define nd_longput(addr,l) (nd_call_mem_put_func(nd_get_mem_bank(addr).lput, addr, l))
#define nd_wordput(addr,w) (nd_call_mem_put_func(nd_get_mem_bank(addr).wput, addr, w))
#define nd_byteput(addr,b) (nd_call_mem_put_func(nd_get_mem_bank(addr).bput, addr, b))
#define nd_cs8get(addr) (nd_call_mem_get_func(nd_get_mem_bank(addr).cs8geti, addr))

void nd_memory_init(void);
