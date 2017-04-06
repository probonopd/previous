/*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68881 emulation
  *
  * Conversion routines for hosts knowing floating point format.
  *
  * Copyright 1996 Herman ten Brugge
  * Modified 2005 Peter Keunecke
  */

#ifndef FPP_H
#define FPP_H

#define __USE_ISOC9X  /* We might be able to pick up a NaN */

#include <math.h>
#include <float.h>

#include <softfloat.h>


#define	FPCR_ROUNDING_MODE	0x00000030
#define	FPCR_ROUND_NEAR		0x00000000
#define	FPCR_ROUND_ZERO		0x00000010
#define	FPCR_ROUND_MINF		0x00000020
#define	FPCR_ROUND_PINF		0x00000030

#define	FPCR_ROUNDING_PRECISION	0x000000c0
#define	FPCR_PRECISION_SINGLE	0x00000040
#define	FPCR_PRECISION_DOUBLE	0x00000080
#define FPCR_PRECISION_EXTENDED	0x00000000

extern uae_u32 fpp_get_fpsr (void);


/* Functions for setting host/library modes and getting status */
STATIC_INLINE void set_fp_mode(uae_u32 mode_control)
{
    float_detect_tininess = float_tininess_before_rounding;
    
    switch(mode_control & FPCR_ROUNDING_PRECISION) {
        case FPCR_PRECISION_SINGLE: // single
            floatx80_rounding_precision = 32;
            break;
        case FPCR_PRECISION_DOUBLE: // double
            floatx80_rounding_precision = 64;
            break;
        case FPCR_PRECISION_EXTENDED: // extended
            floatx80_rounding_precision = 80;
            break;
        default: // double
            floatx80_rounding_precision = 64;
            break;
    }

    switch(mode_control & FPCR_ROUNDING_MODE) {
        case FPCR_ROUND_NEAR: // to neareset
            float_rounding_mode = float_round_nearest_even;
            break;
        case FPCR_ROUND_ZERO: // to zero
            float_rounding_mode = float_round_to_zero;
            break;
        case FPCR_ROUND_MINF: // to minus
            float_rounding_mode = float_round_down;
            break;
        case FPCR_ROUND_PINF: // to plus
            float_rounding_mode = float_round_up;
            break;
    }
}
STATIC_INLINE void get_fp_status(uae_u32 *status)
{
    if (float_exception_flags & float_flag_signaling)
        *status |= 0x4000;
    if (float_exception_flags & float_flag_invalid)
        *status |= 0x2000;
    if (float_exception_flags & float_flag_overflow)
        *status |= 0x1000;
    if (float_exception_flags & float_flag_underflow)
        *status |= 0x0800;
    if (float_exception_flags & float_flag_divbyzero)
        *status |= 0x0400;
    if (float_exception_flags & float_flag_inexact)
        *status |= 0x0200;
    if (float_exception_flags & float_flag_decimal)
        *status |= 0x0100;
}
STATIC_INLINE void clear_fp_status(void)
{
    float_exception_flags = 0;
}

/* Helper functions */
STATIC_INLINE const char *fp_print(fptype *fx)
{
    static char fs[32];
    bool n, u, d;
    long double result = 0.0;
    int i;
    
    n = floatx80_is_negative(*fx);
    u = floatx80_is_unnormal(*fx);
    d = floatx80_is_denormal(*fx);
    
    if (floatx80_is_zero(*fx)) {
        sprintf(fs, "%c%#.17Le%s%s", n?'-':'+', (long double) 0.0, u?"U":"", d?"D":"");
    } else if (floatx80_is_infinity(*fx)) {
        sprintf(fs, "%c%s", n?'-':'+', "inf");
    } else if (floatx80_is_signaling_nan(*fx)) {
        sprintf(fs, "%c%s", n?'-':'+', "snan");
    } else if (floatx80_is_nan(*fx)) {
        sprintf(fs, "%c%s", n?'-':'+', "nan");
    } else {
        for (i = 63; i >= 0; i--) {
            if (fx->low & (((uae_u64)1)<<i)) {
                result += (long double) 1.0 / (((uae_u64)1)<<(63-i));
            }
        }
        result *= powl(2.0, (fx->high&0x7FFF) - 0x3FFF);
        sprintf(fs, "%c%#.17Le%s%s", n?'-':'+', result, u?"U":"", d?"D":"");
    }
    
    return fs;
}

/* Functions for detecting float type */
STATIC_INLINE bool fp_is_snan(fptype *fp)
{
    return floatx80_is_signaling_nan(*fp) != 0;
}
STATIC_INLINE void fp_unset_snan(fptype *fp)
{
    fp->low |= LIT64(0x4000000000000000);
}
STATIC_INLINE bool fp_is_nan (fptype *fp)
{
    return floatx80_is_nan(*fp) != 0;
}
STATIC_INLINE bool fp_is_infinity (fptype *fp)
{
    return floatx80_is_infinity(*fp) != 0;
}
STATIC_INLINE bool fp_is_zero(fptype *fp)
{
    return floatx80_is_zero(*fp) != 0;
}
STATIC_INLINE bool fp_is_neg(fptype *fp)
{
    return floatx80_is_negative(*fp) != 0;
}
STATIC_INLINE bool fp_is_denormal(fptype *fp)
{
    return floatx80_is_denormal(*fp) != 0;
}
STATIC_INLINE bool fp_is_unnormal(fptype *fp)
{
    return floatx80_is_unnormal(*fp) != 0;
}

/* Function for normalizing unnormals */
STATIC_INLINE void fp_normalize(fptype *fp)
{
    *fp = floatx80_normalize(*fp);
}

/* Functions for converting between float formats */
static const long double twoto32 = 4294967296.0;

STATIC_INLINE void to_native(long double *fp, fptype fpx)
{
    int expon;
    long double frac;
    
    expon = fpx.high & 0x7fff;
    
    if (floatx80_is_zero(fpx)) {
        *fp = floatx80_is_negative(fpx) ? -0.0 : +0.0;
        return;
    }
    if (floatx80_is_nan(fpx)) {
        *fp = sqrtl(-1);
        return;
    }
    if (floatx80_is_infinity(fpx)) {
        *fp = floatx80_is_negative(fpx) ? logl(0.0) : (1.0/0.0);
        return;
    }
    
    frac = (long double)fpx.low / (long double)(twoto32 * 2147483648.0);
    if (floatx80_is_negative(fpx))
        frac = -frac;
    *fp = ldexpl (frac, expon - 16383);
}

STATIC_INLINE void from_native(long double fp, fptype *fpx)
{
    int expon;
    long double frac;
    
    if (signbit(fp))
        fpx->high = 0x8000;
    else
        fpx->high = 0x0000;
    
    if (isnan(fp)) {
        fpx->high |= 0x7fff;
        fpx->low = LIT64(0xffffffffffffffff);
        return;
    }
    if (isinf(fp)) {
        fpx->high |= 0x7fff;
        fpx->low = LIT64(0x0000000000000000);
        return;
    }
    if (fp == 0.0) {
        fpx->low = LIT64(0x0000000000000000);
        return;
    }
    if (fp < 0.0)
        fp = -fp;
    
    frac = frexpl (fp, &expon);
    frac += 0.5 / (twoto32 * twoto32);
    if (frac >= 1.0) {
        frac /= 2.0;
        expon++;
    }
    fpx->high |= (expon + 16383 - 1) & 0x7fff;
    fpx->low = (bits64)(frac * (long double)(twoto32 * twoto32));
    
    while (!(fpx->low & LIT64( 0x8000000000000000))) {
        if (fpx->high == 0) {
            break;
        }
        fpx->low <<= 1;
        fpx->high--;
    }
}

STATIC_INLINE void to_single(fptype *fp, uae_u32 wrd1)
{
    float32 f = wrd1;
    *fp = float32_to_floatx80_allowunnormal(f);
}
STATIC_INLINE uae_u32 from_single(fptype *fp)
{
    float32 f = floatx80_to_float32(*fp);
    return f;
}

STATIC_INLINE void to_double(fptype *fp, uae_u32 wrd1, uae_u32 wrd2)
{
    float64 f = ((float64)wrd1 << 32) | wrd2;
    *fp = float64_to_floatx80_allowunnormal(f);
}
STATIC_INLINE void from_double(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2)
{
    float64 f = floatx80_to_float64(*fp);
    *wrd1 = f >> 32;
    *wrd2 = (uae_u32)f;
}

STATIC_INLINE void to_exten(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
    fp->high = (uae_u16)(wrd1 >> 16);
    fp->low = ((uae_u64)wrd2 << 32) | wrd3;
}
STATIC_INLINE void from_exten(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3)
{
    floatx80 f = floatx80_to_floatx80(*fp);
    *wrd1 = (uae_u32)(f.high << 16);
    *wrd2 = f.low >> 32;
    *wrd3 = (uae_u32)f.low;
}
STATIC_INLINE void to_exten_fmovem(fptype *fp, uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
    fp->high = (uae_u16)(wrd1 >> 16);
    fp->low = ((uae_u64)wrd2 << 32) | wrd3;
}
STATIC_INLINE void from_exten_fmovem(fptype *fp, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3)
{
    *wrd1 = (uae_u32)(fp->high << 16);
    *wrd2 = fp->low >> 32;
    *wrd3 = (uae_u32)fp->low;
}

STATIC_INLINE uae_s64 to_int(fptype *src, int size)
{
    switch (size) {
        case 0: return floatx80_to_int8(*src);
        case 1: return floatx80_to_int16(*src);
        case 2: return floatx80_to_int32(*src);
        default: return 0;
    }
}
STATIC_INLINE fptype from_int(uae_s32 src)
{
    return int32_to_floatx80(src);
}

STATIC_INLINE void to_pack(fptype *fp, uae_u32 *wrd)
{
    floatx80 f;
    int i;
    uae_s32 exp;
    uae_s64 mant;
    uae_u32 pack_exp, pack_int, pack_se, pack_sm;
    uae_u64 pack_frac;
    
    if (((wrd[0] >> 16) & 0x7fff) == 0x7fff) {
        // infinity has extended exponent and all 0 packed fraction
        // nans are copies bit by bit
        to_exten(fp, wrd[0], wrd[1], wrd[2]);
        return;
    }
    if (!(wrd[0] & 0xf) && !wrd[1] && !wrd[2]) {
        // exponent is not cared about, if mantissa is zero
        wrd[0] &= 0x80000000;
        to_exten(fp, wrd[0], wrd[1], wrd[2]);
        return;
    }
    
    pack_exp = (wrd[0] >> 16) & 0xFFF;              // packed exponent
    pack_int = wrd[0] & 0xF;                        // packed integer part
    pack_frac = ((uae_u64)wrd[1] << 32) | wrd[2];   // packed fraction
    pack_se = (wrd[0] >> 30) & 1;                   // sign of packed exponent
    pack_sm = (wrd[0] >> 31) & 1;                   // sign of packed significand
    
    exp = 0;
    
    for (i = 0; i < 3; i++) {
        exp *= 10;
        exp += (pack_exp >> (8 - i * 4)) & 0xF;
    }
    
    if (pack_se) {
        exp = -exp;
    }
    
    exp -= 16;
    
    if (exp < 0) {
        exp = -exp;
        pack_se = 1;
    }
    
    mant = pack_int;
    
    for (i = 0; i < 16; i++) {
        mant *= 10;
        mant += (pack_frac >> (60 - i * 4)) & 0xF;
    }

    f.high = exp & 0x3FFF;
    f.high |= pack_se ? 0x4000 : 0;
    f.high |= pack_sm ? 0x8000 : 0;
    f.low = mant;
    
    *fp = floatdecimal_to_floatx80(f);
}
STATIC_INLINE void from_pack(fptype *fp, uae_u32 *wrd, uae_s32 kfactor)
{
    floatx80 f = floatx80_to_floatdecimal(*fp, &kfactor);
    
    uae_u32 pack_exp, pack_exp4, pack_int, pack_se, pack_sm;
    uae_u64 pack_frac;
    
    uae_u32 exponent;
    uae_u64 significand;

    uae_s32 len;
    uae_u64 digit;
    
    if ((f.high & 0x7FFF) == 0x7FFF) {
        wrd[0] = (uae_u32)(f.high << 16);
        wrd[1] = f.low >> 32;
        wrd[2] = (uae_u32)f.low;
    } else {
        exponent = f.high & 0x3FFF;
        significand = f.low;
        
        pack_frac = 0;
        len = kfactor; // SoftFloat saved len to kfactor variable
        while (len > 0) {
            len--;
            digit = significand % 10;
            significand /= 10;
            if (len == 0) {
                pack_int = digit;
            } else {
                pack_frac |= digit << (64 - len * 4);
            }
        }
        
        pack_exp = 0;
        pack_exp4 = 0;
        len = 4;
        while (len > 0) {
            len--;
            digit = exponent % 10;
            exponent /= 10;
            if (len == 0) {
                pack_exp4 = digit;
            } else {
                pack_exp |= digit << (12 - len * 4);
            }
        }
        
        pack_se = f.high & 0x4000;
        pack_sm = f.high & 0x8000;
        
        wrd[0] = pack_exp << 16;
        wrd[0] |= pack_exp4 << 12;
        wrd[0] |= pack_int;
        wrd[0] |= pack_se ? 0x40000000 : 0;
        wrd[0] |= pack_sm ? 0x80000000 : 0;
        
        wrd[1] = pack_frac >> 32;
        wrd[2] = pack_frac & 0xffffffff;
    }
}

/* Functions for returning exception state data */
STATIC_INLINE fptype fp_get_internal_overflow(void)
{
    return getFloatInternalOverflow();
}
STATIC_INLINE fptype fp_get_internal_underflow(void)
{
    return getFloatInternalUnderflow();
}
STATIC_INLINE fptype fp_get_internal_round_all(void)
{
    return getFloatInternalRoundedAll();
}
STATIC_INLINE fptype fp_get_internal_round(void)
{
    return getFloatInternalRoundedSome();
}
STATIC_INLINE fptype fp_get_internal_round_exten(void)
{
    return getFloatInternalFloatx80();
}
STATIC_INLINE fptype fp_get_internal(void)
{
    return getFloatInternalUnrounded();
}
STATIC_INLINE uae_u32 fp_get_internal_grs(void)
{
    return (uae_u32)getFloatInternalGRS();
}

/* Functions for rounding */

// round to float with extended precision exponent
STATIC_INLINE void fp_round32(fptype *fp)
{
    *fp = floatx80_round32(*fp);
}

// round to double with extended precision exponent
STATIC_INLINE void fp_round64(fptype *fp)
{
    *fp = floatx80_round64(*fp);
}

// round to float
STATIC_INLINE void fp_round_single(fptype *fp)
{
    *fp = floatx80_round_to_float32(*fp);
}

// round to double
STATIC_INLINE void fp_round_double(fptype *fp)
{
    *fp = floatx80_round_to_float64(*fp);
}

// round to selected precision
STATIC_INLINE void fp_round(fptype *a)
{
    switch(floatx80_rounding_precision) {
        case 32:
            *a = floatx80_round_to_float32(*a);
            break;
        case 64:
            *a = floatx80_round_to_float64(*a);
            break;
        default:
            break;
    }
}

/* Arithmetic functions */
STATIC_INLINE void fp_move(fptype *a, fptype *b)
{
    *a = floatx80_move(*b);
}
STATIC_INLINE void fp_int(fptype *a, fptype *b)
{
    *a = floatx80_round_to_int(*b);
}
STATIC_INLINE void fp_intrz(fptype *a, fptype *b)
{
    *a = floatx80_round_to_int_toward_zero(*b);
}
STATIC_INLINE void fp_sqrt(fptype *a, fptype *b)
{
    *a = floatx80_sqrt(*b);
}
STATIC_INLINE void fp_lognp1(fptype *a, fptype *b)
{
    *a = floatx80_flognp1(*b);
}
STATIC_INLINE void fp_sin(fptype *a, fptype *b)
{
    *a = *b;
    floatx80_fsin(a);
}
STATIC_INLINE void fp_tan(fptype *a, fptype *b)
{
    *a = *b;
    floatx80_ftan(a);
}
STATIC_INLINE void fp_logn(fptype *a, fptype *b)
{
    *a = floatx80_flogn(*b);
}
STATIC_INLINE void fp_log10(fptype *a, fptype *b)
{
    *a = floatx80_flog10(*b);
}
STATIC_INLINE void fp_log2(fptype *a, fptype *b)
{
    *a = floatx80_flog2(*b);
}
STATIC_INLINE void fp_abs(fptype *a, fptype *b)
{
    *a = floatx80_abs(*b);
}
STATIC_INLINE void fp_neg(fptype *a, fptype *b)
{
    *a = floatx80_neg(*b);
}
STATIC_INLINE void fp_cos(fptype *a, fptype *b)
{
    *a = *b;
    floatx80_fcos(a);
}
STATIC_INLINE void fp_getexp(fptype *a, fptype *b)
{
    *a = floatx80_getexp(*b);
}
STATIC_INLINE void fp_getman(fptype *a, fptype *b)
{
    *a = floatx80_getman(*b);
}
STATIC_INLINE void fp_div(fptype *a, fptype *b)
{
    *a = floatx80_div(*a, *b);
}
STATIC_INLINE void fp_mod(fptype *a, fptype *b, uae_u64 *q, uae_s8 *s)
{
    *a = floatx80_mod(*a, *b, q, s);
}
STATIC_INLINE void fp_add(fptype *a, fptype *b)
{
    *a = floatx80_add(*a, *b);
}
STATIC_INLINE void fp_mul(fptype *a, fptype *b)
{
    *a = floatx80_mul(*a, *b);
}
STATIC_INLINE void fp_sgldiv(fptype *a, fptype *b)
{
    *a = floatx80_sgldiv(*a, *b);
}
STATIC_INLINE void fp_rem(fptype *a, fptype *b, uae_u64 *q, uae_s8 *s)
{
    *a = floatx80_rem(*a, *b, q, s);
}
STATIC_INLINE void fp_scale(fptype *a, fptype *b)
{
    *a = floatx80_scale(*a, *b);
}
STATIC_INLINE void fp_sglmul(fptype *a, fptype *b)
{
    *a = floatx80_sglmul(*a, *b);
}
STATIC_INLINE void fp_sub(fptype *a, fptype *b)
{
    *a = floatx80_sub(*a, *b);
}
STATIC_INLINE void fp_cmp(fptype *a, fptype *b)
{
    *a = floatx80_cmp(*a, *b);
}
STATIC_INLINE void fp_tst(fptype *a, fptype *b)
{
    *a = floatx80_tst(*b);
}

/* Functions with fixed precision */
STATIC_INLINE void fp_move_single(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 32;
    *a = floatx80_move(*b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_abs_single(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 32;
    *a = floatx80_abs(*b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_neg_single(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 32;
    *a = floatx80_neg(*b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_add_single(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 32;
    *a = floatx80_add(*a, *b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_sub_single(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 32;
    *a = floatx80_sub(*a, *b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_mul_single(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 32;
    *a = floatx80_mul(*a, *b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_div_single(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 32;
    *a = floatx80_div(*a, *b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_sqrt_single(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 32;
    *a = floatx80_sqrt(*b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_move_double(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 64;
    *a = floatx80_move(*b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_abs_double(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 64;
    *a = floatx80_abs(*b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_neg_double(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 64;
    *a = floatx80_neg(*b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_add_double(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 64;
    *a = floatx80_add(*a, *b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_sub_double(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 64;
    *a = floatx80_sub(*a, *b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_mul_double(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 64;
    *a = floatx80_mul(*a, *b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_div_double(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 64;
    *a = floatx80_div(*a, *b);
    floatx80_rounding_precision = oldprec;
}
STATIC_INLINE void fp_sqrt_double(fptype *a, fptype *b)
{
    int8 oldprec = floatx80_rounding_precision;
    floatx80_rounding_precision = 64;
    *a = floatx80_sqrt(*b);
    floatx80_rounding_precision = oldprec;
}


/* FIXME: create softfloat functions for following arithmetics */

STATIC_INLINE void fp_sinh(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_sinh_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = sinhl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_etoxm1(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_etoxm1_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = expm1l(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_tanh(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_tanh_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = tanhl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_atan(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_atan_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = atanl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_asin(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_asin_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = asinl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_atanh(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_atanh_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = atanhl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_etox(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_etox_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = expl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_twotox(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_twotox_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = powl(2.0, fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_tentox(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_tentox_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = powl(10.0, fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_cosh(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_cosh_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = coshl(fp);
    from_native(fp, a);
    fp_round(a);
}
STATIC_INLINE void fp_acos(fptype *a, fptype *b)
{
    flag e = 0;
    floatx80_acos_check(*a, &e);
    if (e) return;
    long double fp;
    to_native(&fp, *b);
    fp = acosl(fp);
    from_native(fp, a);
    fp_round(a);
}

#endif
