
STATIC_INLINE uae_u32 get_word_prefetch (int o)
{
	uae_u32 v = regs.irc;
	regs.irc = get_wordi (m68k_getpc () + o);
	return v;
}
STATIC_INLINE uae_u32 get_long_prefetch (int o)
{
	uae_u32 v = get_word_prefetch (o) << 16;
	v |= get_word_prefetch (o + 2);
	return v;
}

#ifdef CPUEMU_20

STATIC_INLINE void checkcycles_ce020 (void)
{
	regs.ce020memcycles = 0;
}

STATIC_INLINE uae_u32 mem_access_delay_long_read_ce020 (uaecptr addr)
{
	checkcycles_ce020 ();
	return get_long (addr);
}

STATIC_INLINE uae_u32 mem_access_delay_longi_read_ce020 (uaecptr addr)
{
	checkcycles_ce020 ();
	return get_longi (addr);
}

STATIC_INLINE uae_u32 mem_access_delay_word_read_ce020 (uaecptr addr)
{
	checkcycles_ce020 ();
	return get_word (addr);
}

STATIC_INLINE uae_u32 mem_access_delay_wordi_read_ce020 (uaecptr addr)
{
	checkcycles_ce020 ();
	return get_wordi (addr);
}

STATIC_INLINE uae_u32 mem_access_delay_byte_read_ce020 (uaecptr addr)
{
	checkcycles_ce020 ();
	return get_byte (addr);
}

STATIC_INLINE void mem_access_delay_byte_write_ce020 (uaecptr addr, uae_u32 v)
{
	checkcycles_ce020 ();
	put_byte (addr, v);
}

STATIC_INLINE void mem_access_delay_word_write_ce020 (uaecptr addr, uae_u32 v)
{
	checkcycles_ce020 ();
	put_word (addr, v);
}

STATIC_INLINE void mem_access_delay_long_write_ce020 (uaecptr addr, uae_u32 v)
{
	checkcycles_ce020 ();
	put_long (addr, v);
}

STATIC_INLINE uae_u32 get_long_ce020 (uaecptr addr)
{
	return mem_access_delay_long_read_ce020 (addr);
}
STATIC_INLINE uae_u32 get_word_ce020 (uaecptr addr)
{
	return mem_access_delay_word_read_ce020 (addr);
}
STATIC_INLINE uae_u32 get_byte_ce020 (uaecptr addr)
{
	return mem_access_delay_byte_read_ce020 (addr);
}

STATIC_INLINE void put_long_ce020 (uaecptr addr, uae_u32 v)
{
	mem_access_delay_long_write_ce020 (addr, v);
}
STATIC_INLINE void put_word_ce020 (uaecptr addr, uae_u32 v)
{
	mem_access_delay_word_write_ce020 (addr, v);
}
STATIC_INLINE void put_byte_ce020 (uaecptr addr, uae_u32 v)
{
	mem_access_delay_byte_write_ce020 (addr, v);
}

uae_u32 get_word_ce020_prefetch (int);

STATIC_INLINE uae_u32 get_long_ce020_prefetch (int o)
{
	uae_u32 v;
	v = get_word_ce020_prefetch (o) << 16;
	v |= get_word_ce020_prefetch (o + 2);
	return v;
}

STATIC_INLINE uae_u32 next_iword_020ce (void)
{
	uae_u32 r = get_word_ce020_prefetch (0);
	m68k_incpc (2);
	return r;
}
STATIC_INLINE uae_u32 next_ilong_020ce (void)
{
	uae_u32 r = get_long_ce020_prefetch (0);
	m68k_incpc (4);
	return r;
}

STATIC_INLINE void m68k_do_bsr_ce020 (uaecptr oldpc, uae_s32 offset)
{
	m68k_areg (regs, 7) -= 4;
	put_long_ce020 (m68k_areg (regs, 7), oldpc);
	m68k_incpc (offset);
}
STATIC_INLINE void m68k_do_rts_ce020 (void)
{
	m68k_setpc (get_long_ce020 (m68k_areg (regs, 7)));
	m68k_areg (regs, 7) += 4;
}
#endif

#ifdef CPUEMU_21

uae_u32 get_word_ce030_prefetch (int);
void write_dcache030 (uaecptr, uae_u32, int);
uae_u32 read_dcache030 (uaecptr, int);

STATIC_INLINE void put_long_ce030 (uaecptr addr, uae_u32 v)
{
	write_dcache030 (addr, v, 2);
	mem_access_delay_long_write_ce020 (addr, v);
}
STATIC_INLINE void put_word_ce030 (uaecptr addr, uae_u32 v)
{
	write_dcache030 (addr, v, 1);
	mem_access_delay_word_write_ce020 (addr, v);
}
STATIC_INLINE void put_byte_ce030 (uaecptr addr, uae_u32 v)
{
	write_dcache030 (addr, v, 0);
	mem_access_delay_byte_write_ce020 (addr, v);
}
STATIC_INLINE uae_u32 get_long_ce030 (uaecptr addr)
{
	return read_dcache030 (addr, 2);
}
STATIC_INLINE uae_u32 get_word_ce030 (uaecptr addr)
{
	return read_dcache030 (addr, 1);
}
STATIC_INLINE uae_u32 get_byte_ce030 (uaecptr addr)
{
	return read_dcache030 (addr, 0);
}

STATIC_INLINE uae_u32 get_long_ce030_prefetch (int o)
{
	uae_u32 v;
	v = get_word_ce030_prefetch (o) << 16;
	v |= get_word_ce030_prefetch (o + 2);
	return v;
}

STATIC_INLINE uae_u32 next_iword_030ce (void)
{
	uae_u32 r = get_word_ce030_prefetch (0);
	m68k_incpc (2);
	return r;
}
STATIC_INLINE uae_u32 next_ilong_030ce (void)
{
	uae_u32 r = get_long_ce030_prefetch (0);
	m68k_incpc (4);
	return r;
}

STATIC_INLINE void m68k_do_bsr_ce030 (uaecptr oldpc, uae_s32 offset)
{
	m68k_areg (regs, 7) -= 4;
	put_long_ce030 (m68k_areg (regs, 7), oldpc);
	m68k_incpc (offset);
}
STATIC_INLINE void m68k_do_rts_ce030 (void)
{
	m68k_setpc (get_long_ce030 (m68k_areg (regs, 7)));
	m68k_areg (regs, 7) += 4;
}
#endif

#ifdef CPUEMU_12

STATIC_INLINE void ipl_fetch (void)
{
	regs.ipl = regs.ipl_pin;
}

STATIC_INLINE uae_u32 mem_access_delay_word_read (uaecptr addr)
{
	return get_word (addr);
}
STATIC_INLINE uae_u32 mem_access_delay_wordi_read (uaecptr addr)
{
	return get_wordi (addr);
}

STATIC_INLINE uae_u32 mem_access_delay_byte_read (uaecptr addr)
{
	return get_byte (addr);
}
STATIC_INLINE void mem_access_delay_byte_write (uaecptr addr, uae_u32 v)
{
	put_byte (addr, v);
}
STATIC_INLINE void mem_access_delay_word_write (uaecptr addr, uae_u32 v)
{
	put_word (addr, v);
}

STATIC_INLINE uae_u32 get_long_ce (uaecptr addr)
{
	uae_u32 v = mem_access_delay_word_read (addr) << 16;
	v |= mem_access_delay_word_read (addr + 2);
	return v;
}
STATIC_INLINE uae_u32 get_word_ce (uaecptr addr)
{
	return mem_access_delay_word_read (addr);
}
STATIC_INLINE uae_u32 get_wordi_ce (uaecptr addr)
{
	return mem_access_delay_wordi_read (addr);
}
STATIC_INLINE uae_u32 get_byte_ce (uaecptr addr)
{
	return mem_access_delay_byte_read (addr);
}
STATIC_INLINE uae_u32 get_word_ce_prefetch (int o)
{
	uae_u32 v = regs.irc;
	regs.irc = get_wordi_ce (m68k_getpc () + o);
	return v;
}

STATIC_INLINE void put_long_ce (uaecptr addr, uae_u32 v)
{
	mem_access_delay_word_write (addr, v >> 16);
	mem_access_delay_word_write (addr + 2, v);
}
STATIC_INLINE void put_word_ce (uaecptr addr, uae_u32 v)
{
	mem_access_delay_word_write (addr, v);
}
STATIC_INLINE void put_byte_ce (uaecptr addr, uae_u32 v)
{
	mem_access_delay_byte_write (addr, v);
}

STATIC_INLINE void m68k_do_rts_ce (void)
{
	uaecptr pc;
	pc = get_word_ce (m68k_areg (regs, 7)) << 16;
	pc |= get_word_ce (m68k_areg (regs, 7) + 2);
	m68k_areg (regs, 7) += 4;
	if (pc & 1)
		exception3 (0x4e75, m68k_getpc ());
	else
		m68k_setpc (pc);
}

STATIC_INLINE void m68k_do_bsr_ce (uaecptr oldpc, uae_s32 offset)
{
	m68k_areg (regs, 7) -= 4;
	put_word_ce (m68k_areg (regs, 7), oldpc >> 16);
	put_word_ce (m68k_areg (regs, 7) + 2, oldpc);
	m68k_incpc (offset);
}

STATIC_INLINE void m68k_do_jsr_ce (uaecptr oldpc, uaecptr dest)
{
	m68k_areg (regs, 7) -= 4;
	put_word_ce (m68k_areg (regs, 7), oldpc >> 16);
	put_word_ce (m68k_areg (regs, 7) + 2, oldpc);
	m68k_setpc (dest);
}
#endif
