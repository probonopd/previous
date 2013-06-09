
#ifndef MMU_COMMON_H
#define MMU_COMMON_H

#if 0
struct m68k_exception {
	int prb;
	m68k_exception (int exc) : prb (exc) {}
	operator int() { return prb; }
};
#define SAVE_EXCEPTION
#define RESTORE_EXCEPTION
#define TRY(var) try
#define CATCH(var) catch(m68k_exception var)
#define THROW(n) throw m68k_exception(n)
#define THROW_AGAIN(var) throw

#else
/* we are in plain C, just use a stack of long jumps */
#include <setjmp.h>
extern jmp_buf __exbuf;
extern int     __exvalue;
#define TRY(DUMMY)       __exvalue=setjmp(__exbuf);       \
                  if (__exvalue==0) { __pushtry(&__exbuf);
#define CATCH(x)  __poptry(); } else { fprintf(stderr,"Gotcha! %d %s in %d\n",__exvalue,__FILE__,__LINE__);
#define ENDTRY    __poptry();}
#define THROW(x) if (__is_catched()) {fprintf(stderr,"Longjumping %s in %d\n",__FILE__,__LINE__);longjmp(__exbuf,x);}
#define THROW_AGAIN(var) if (__is_catched()) longjmp(*__poptry(),__exvalue)
#define SAVE_EXCEPTION
#define RESTORE_EXCEPTION
jmp_buf* __poptry(void);
void __pushtry(jmp_buf *j);
int __is_catched(void);

typedef  int m68k_exception;

#endif

/* special status word (access error stack frame) */
/* this is only valid for 68040 */
/* TODO: correctly handle SSW on 68030 */
#define MMU_SSW_TM		0x0007
#define MMU_SSW_TT		0x0018
#define MMU_SSW_SIZE	0x0060
#define MMU_SSW_SIZE_B	0x0020
#define MMU_SSW_SIZE_W	0x0040
#define MMU_SSW_SIZE_L	0x0000
#define MMU_SSW_RW		0x0100
#define MMU_SSW_LK		0x0200
#define MMU_SSW_ATC		0x0400
#define MMU_SSW_MA		0x0800
#define MMU_SSW_CM	0x1000
#define MMU_SSW_CT	0x2000
#define MMU_SSW_CU	0x4000
#define MMU_SSW_CP	0x8000

#define MMU030_SSW_FC       0x8000
#define MMU030_SSW_FB       0x4000
#define MMU030_SSW_RC       0x2000
#define MMU030_SSW_RB       0x1000
#define MMU030_SSW_DF       0x0100
#define MMU030_SSW_RM       0x0080
#define MMU030_SSW_RW       0x0040
#define MMU030_SSW_SIZE_MASK    0x0030
#define MMU030_SSW_SIZE_B       0x0010 /* correct ? */
#define MMU030_SSW_SIZE_W       0x0020 /* correct ? */
#define MMU030_SSW_SIZE_L       0x0000 /* correct ? */
#define MMU030_SSW_FC_MASK      0x0007


#define ALWAYS_INLINE __inline

// take care of 2 kinds of alignement, bus size and page
static inline bool is_unaligned(uaecptr addr, int size)
{
    return unlikely((addr & (size - 1)) && (addr ^ (addr + size - 1)) & 0x1000);
}

static ALWAYS_INLINE void phys_put_long(uaecptr addr, uae_u32 l)
{
    longput(addr, l);
}
static ALWAYS_INLINE void phys_put_word(uaecptr addr, uae_u32 w)
{
    wordput(addr, w);
}
static ALWAYS_INLINE void phys_put_byte(uaecptr addr, uae_u32 b)
{
    byteput(addr, b);
}
static ALWAYS_INLINE uae_u32 phys_get_long(uaecptr addr)
{
    return longget (addr);
}
static ALWAYS_INLINE uae_u32 phys_get_word(uaecptr addr)
{
    return wordget (addr);
}
static ALWAYS_INLINE uae_u32 phys_get_byte(uaecptr addr)
{
    return byteget (addr);
}

#endif
