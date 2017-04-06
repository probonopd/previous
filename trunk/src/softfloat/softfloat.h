
/*============================================================================

This C header file is part of the SoftFloat IEC/IEEE Floating-point Arithmetic
Package, Release 2b.

Written by John R. Hauser.  This work was made possible in part by the
International Computer Science Institute, located at Suite 600, 1947 Center
Street, Berkeley, California 94704.  Funding was partially provided by the
National Science Foundation under grant MIP-9311980.  The original version
of this code was written as part of a project to build a fixed-point vector
processor in collaboration with the University of California at Berkeley,
overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
is available through the Web page `http://www.cs.berkeley.edu/~jhauser/
arithmetic/SoftFloat.html'.

THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort has
been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT TIMES
RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO PERSONS
AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ALL LOSSES,
COSTS, OR OTHER PROBLEMS THEY INCUR DUE TO THE SOFTWARE, AND WHO FURTHERMORE
EFFECTIVELY INDEMNIFY JOHN HAUSER AND THE INTERNATIONAL COMPUTER SCIENCE
INSTITUTE (possibly via similar legal warning) AGAINST ALL LOSSES, COSTS, OR
OTHER PROBLEMS INCURRED BY THEIR CUSTOMERS AND CLIENTS DUE TO THE SOFTWARE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) the source code for the derivative work includes prominent notice that
the work is derivative, and (2) the source code includes prominent notice with
these four paragraphs for those parts of this code that are retained.

=============================================================================*/

#ifndef SOFTFLOAT_H
#define SOFTFLOAT_H
/*----------------------------------------------------------------------------
| The macro `FLOATX80' must be defined to enable the extended double-precision
| floating-point format `floatx80'.  If this macro is not defined, the
| `floatx80' type will not be defined, and none of the functions that either
| input or output the `floatx80' type will be defined.  The same applies to
| the `FLOAT128' macro and the quadruple-precision format `float128'.
*----------------------------------------------------------------------------*/
#define FLOATX80
#define FLOAT128

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point types.
*----------------------------------------------------------------------------*/

#include "mamesf.h"

typedef bits32 float32;
typedef bits64 float64;
#ifdef FLOATX80
typedef struct {
	bits16 high;
	bits64 low;
} floatx80;
#endif
#ifdef FLOAT128
typedef struct {
	bits64 high, low;
} float128;
#endif

/*----------------------------------------------------------------------------
| Primitive arithmetic functions, including multi-word arithmetic, and
| division and square root approximations.  (Can be specialized to target if
| desired.)
*----------------------------------------------------------------------------*/
#include "softfloat-macros.h"

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point underflow tininess-detection mode.
*----------------------------------------------------------------------------*/
extern int8 float_detect_tininess;
enum {
	float_tininess_after_rounding  = 0,
	float_tininess_before_rounding = 1
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point rounding mode.
*----------------------------------------------------------------------------*/
extern int8 float_rounding_mode;
enum {
	float_round_nearest_even = 0,
	float_round_to_zero      = 1,
	float_round_down         = 2,
	float_round_up           = 3
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point exception flags.
*----------------------------------------------------------------------------*/
extern int8 float_exception_flags;
enum {
	float_flag_invalid = 0x01, float_flag_denormal = 0x02, float_flag_divbyzero = 0x04, float_flag_overflow = 0x08,
    float_flag_underflow = 0x10, float_flag_inexact = 0x20, float_flag_signaling = 0x40, float_flag_decimal = 0x80
};

/*----------------------------------------------------------------------------
 | Variables for storing sign, exponent and significand of internal extended 
 | double-precision floating-point value for external use.
 *----------------------------------------------------------------------------*/
extern flag floatx80_internal_sign;
extern int32 floatx80_internal_exp;
extern bits64 floatx80_internal_sig0;
extern bits64 floatx80_internal_sig1;
extern int8 floatx80_internal_precision;
extern int8 floatx80_internal_mode;

/*----------------------------------------------------------------------------
 | Function for getting sign, exponent and significand of extended
 | double-precision floating-point intermediate result for external use.
 *----------------------------------------------------------------------------*/
floatx80 getFloatInternalOverflow( void );
floatx80 getFloatInternalUnderflow( void );
floatx80 getFloatInternalRoundedAll( void );
floatx80 getFloatInternalRoundedSome( void );
floatx80 getFloatInternalUnrounded( void );
floatx80 getFloatInternalFloatx80( void );
bits64 getFloatInternalGRS( void );

/*----------------------------------------------------------------------------
| Routine to raise any or all of the software IEC/IEEE floating-point
| exception flags.
*----------------------------------------------------------------------------*/
void float_raise( int8 );

/*----------------------------------------------------------------------------
 | The pattern for a default generated single-precision NaN.
 *----------------------------------------------------------------------------*/
#define float32_default_nan 0x7FFFFFFF

/*----------------------------------------------------------------------------
 | The pattern for a default generated double-precision NaN.
 *----------------------------------------------------------------------------*/
#define float64_default_nan LIT64( 0x7FFFFFFFFFFFFFFF )

#ifdef FLOATX80
/*----------------------------------------------------------------------------
 | The pattern for a default generated extended double-precision NaN.  The
 | `high' and `low' values hold the most- and least-significant bits,
 | respectively.
 *----------------------------------------------------------------------------*/
#define floatx80_default_nan_high 0x7FFF
#define floatx80_default_nan_low  LIT64( 0xFFFFFFFFFFFFFFFF )
#endif
#ifdef FLOAT128
/*----------------------------------------------------------------------------
 | The pattern for a default generated quadruple-precision NaN.  The `high' and
 | `low' values hold the most- and least-significant bits, respectively.
 *----------------------------------------------------------------------------*/
#define float128_default_nan_high LIT64( 0x7FFFFFFFFFFFFFFF )
#define float128_default_nan_low  LIT64( 0xFFFFFFFFFFFFFFFF )
#endif
#ifdef FLOATX80
/*----------------------------------------------------------------------------
 | The pattern for a default generated extended double-precision infinity.
 *----------------------------------------------------------------------------*/
#define floatx80_default_infinity_low  LIT64( 0x0000000000000000 )
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE integer-to-floating-point conversion routines.
*----------------------------------------------------------------------------*/
float32 int32_to_float32( int32 );
float64 int32_to_float64( int32 );
#ifdef FLOATX80
floatx80 int32_to_floatx80( int32 );
#endif
#ifdef FLOAT128
float128 int32_to_float128( int32 );
#endif
float32 int64_to_float32( int64 );
float64 int64_to_float64( int64 );
#ifdef FLOATX80
floatx80 int64_to_floatx80( int64 );
#endif
#ifdef FLOAT128
float128 int64_to_float128( int64 );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision conversion routines.
*----------------------------------------------------------------------------*/
int32 float32_to_int32( float32 );
int32 float32_to_int32_round_to_zero( float32 );
int64 float32_to_int64( float32 );
int64 float32_to_int64_round_to_zero( float32 );
float64 float32_to_float64( float32 );
#ifdef FLOATX80
floatx80 float32_to_floatx80( float32 );
floatx80 float32_to_floatx80_allowunnormal( float32 );
#endif
#ifdef FLOAT128
float128 float32_to_float128( float32 );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision operations.
*----------------------------------------------------------------------------*/
float32 float32_round_to_int( float32 );
float32 float32_add( float32, float32 );
float32 float32_sub( float32, float32 );
float32 float32_mul( float32, float32 );
float32 float32_div( float32, float32 );
float32 float32_rem( float32, float32 );
float32 float32_sqrt( float32 );
flag float32_eq( float32, float32 );
flag float32_le( float32, float32 );
flag float32_lt( float32, float32 );
flag float32_eq_signaling( float32, float32 );
flag float32_le_quiet( float32, float32 );
flag float32_lt_quiet( float32, float32 );
flag float32_is_signaling_nan( float32 );

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision conversion routines.
*----------------------------------------------------------------------------*/
int32 float64_to_int32( float64 );
int32 float64_to_int32_round_to_zero( float64 );
int64 float64_to_int64( float64 );
int64 float64_to_int64_round_to_zero( float64 );
float32 float64_to_float32( float64 );
#ifdef FLOATX80
floatx80 float64_to_floatx80( float64 );
floatx80 float64_to_floatx80_allowunnormal( float64 );
#endif
#ifdef FLOAT128
float128 float64_to_float128( float64 );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision operations.
*----------------------------------------------------------------------------*/
float64 float64_round_to_int( float64 );
float64 float64_add( float64, float64 );
float64 float64_sub( float64, float64 );
float64 float64_mul( float64, float64 );
float64 float64_div( float64, float64 );
float64 float64_rem( float64, float64 );
float64 float64_sqrt( float64 );
flag float64_eq( float64, float64 );
flag float64_le( float64, float64 );
flag float64_lt( float64, float64 );
flag float64_eq_signaling( float64, float64 );
flag float64_le_quiet( float64, float64 );
flag float64_lt_quiet( float64, float64 );
flag float64_is_signaling_nan( float64 );

#ifdef FLOATX80

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision conversion routines.
*----------------------------------------------------------------------------*/
int32 floatx80_to_int32( floatx80 );
#ifdef SOFTFLOAT_68K
int16 floatx80_to_int16( floatx80 );
int8 floatx80_to_int8( floatx80 );
#endif
int32 floatx80_to_int32_round_to_zero( floatx80 );
int64 floatx80_to_int64( floatx80 );
int64 floatx80_to_int64_round_to_zero( floatx80 );
float32 floatx80_to_float32( floatx80 );
float64 floatx80_to_float64( floatx80 );
#ifdef SOFTFLOAT_68K
floatx80 floatx80_to_floatx80( floatx80 );
floatx80 floatdecimal_to_floatx80( floatx80 );
floatx80 floatx80_to_floatdecimal(floatx80, int32*);
#endif
#ifdef FLOAT128
float128 floatx80_to_float128( floatx80 );
#endif
bits64 extractFloatx80Frac( floatx80 a );
int32 extractFloatx80Exp( floatx80 a );
flag extractFloatx80Sign( floatx80 a );

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision rounding precision.  Valid
| values are 32, 64, and 80.
*----------------------------------------------------------------------------*/
extern int8 floatx80_rounding_precision;

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision operations.
*----------------------------------------------------------------------------*/
floatx80 floatx80_round_to_int( floatx80 );
#ifdef SOFTFLOAT_68K
floatx80 floatx80_round_to_int_toward_zero( floatx80 );
floatx80 floatx80_round_to_float32( floatx80 );
floatx80 floatx80_round_to_float64( floatx80 );
floatx80 floatx80_round32( floatx80 );
floatx80 floatx80_round64( floatx80 );
floatx80 floatx80_normalize( floatx80 );
#endif
floatx80 floatx80_add( floatx80, floatx80 );
floatx80 floatx80_sub( floatx80, floatx80 );
floatx80 floatx80_mul( floatx80, floatx80 );
floatx80 floatx80_div( floatx80, floatx80 );
#ifndef SOFTFLOAT_68K
floatx80 floatx80_rem( floatx80, floatx80 );
#endif
floatx80 floatx80_sqrt( floatx80 );

flag floatx80_eq( floatx80, floatx80 );
flag floatx80_le( floatx80, floatx80 );
flag floatx80_lt( floatx80, floatx80 );
flag floatx80_eq_signaling( floatx80, floatx80 );
flag floatx80_le_quiet( floatx80, floatx80 );
flag floatx80_lt_quiet( floatx80, floatx80 );

flag floatx80_is_signaling_nan( floatx80 );
flag floatx80_is_nan( floatx80 );
#ifdef SOFTFLOAT_68K
flag floatx80_is_zero( floatx80 );
flag floatx80_is_infinity( floatx80 );
flag floatx80_is_negative( floatx80 );
flag floatx80_is_denormal( floatx80 );
flag floatx80_is_unnormal( floatx80 );
flag floatx80_is_normal( floatx80 );

// functions are in fsincos.c
int floatx80_fsincos(floatx80 a, floatx80 *sin_a, floatx80 *cos_a);
int floatx80_fsin(floatx80 *a);
int floatx80_fcos(floatx80 *a);
int floatx80_ftan(floatx80 *a);

// functions are in fyl2x.c
floatx80 floatx80_flognp1(floatx80 a);
floatx80 floatx80_flogn(floatx80 a);
floatx80 floatx80_flog2(floatx80 a);
floatx80 floatx80_flog10(floatx80 a);

// functions are in softfloat.c
floatx80 floatx80_move( floatx80 a );
floatx80 floatx80_abs( floatx80 a );
floatx80 floatx80_neg( floatx80 a );
floatx80 floatx80_getexp( floatx80 a );
floatx80 floatx80_getman( floatx80 a );
floatx80 floatx80_scale(floatx80 a, floatx80 b);
floatx80 floatx80_rem( floatx80 a, floatx80 b, bits64 *q, flag *s );
floatx80 floatx80_mod( floatx80 a, floatx80 b, bits64 *q, flag *s );
floatx80 floatx80_sglmul( floatx80 a, floatx80 b );
floatx80 floatx80_sgldiv( floatx80 a, floatx80 b );
floatx80 floatx80_cmp( floatx80 a, floatx80 b );
floatx80 floatx80_tst( floatx80 a );

// functions are in softfloat_extension.c
floatx80 floatx80_acos_check(floatx80 a, flag *e);
floatx80 floatx80_asin_check(floatx80 a, flag *e);
floatx80 floatx80_atan_check(floatx80 a, flag *e);
floatx80 floatx80_atanh_check(floatx80 a, flag *e);
floatx80 floatx80_cos_check(floatx80 a, flag *e);
floatx80 floatx80_cosh_check(floatx80 a, flag *e);
floatx80 floatx80_etox_check(floatx80 a, flag *e);
floatx80 floatx80_etoxm1_check(floatx80 a, flag *e);
floatx80 floatx80_log10_check(floatx80 a, flag *e);
floatx80 floatx80_log2_check(floatx80 a, flag *e);
floatx80 floatx80_logn_check(floatx80 a, flag *e);
floatx80 floatx80_lognp1_check(floatx80 a, flag *e);
floatx80 floatx80_sin_check(floatx80 a, flag *e);
floatx80 floatx80_sinh_check(floatx80 a, flag *e);
floatx80 floatx80_tan_check(floatx80 a, flag *e);
floatx80 floatx80_tanh_check(floatx80 a, flag *e);
floatx80 floatx80_tentox_check(floatx80 a, flag *e);
floatx80 floatx80_twotox_check(floatx80 a, flag *e);
#endif

// functions originally internal to softfloat.c
void normalizeFloatx80Subnormal( bits64 aSig, int32 *zExpPtr, bits64 *zSigPtr );
floatx80 packFloatx80( flag zSign, int32 zExp, bits64 zSig );
floatx80 roundAndPackFloatx80(int8 roundingPrecision, flag zSign, int32 zExp, bits64 zSig0, bits64 zSig1);

// functions are in softfloat-specialize.h
floatx80 propagateFloatx80NaNOneArg( floatx80 a );
floatx80 propagateFloatx80NaN( floatx80 a, floatx80 b );

#endif

#ifdef FLOAT128

/*----------------------------------------------------------------------------
| Software IEC/IEEE quadruple-precision conversion routines.
*----------------------------------------------------------------------------*/
int32 float128_to_int32( float128 );
int32 float128_to_int32_round_to_zero( float128 );
int64 float128_to_int64( float128 );
int64 float128_to_int64_round_to_zero( float128 );
float32 float128_to_float32( float128 );
float64 float128_to_float64( float128 );
#ifdef FLOATX80
floatx80 float128_to_floatx80( float128 );
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE quadruple-precision operations.
*----------------------------------------------------------------------------*/
float128 float128_round_to_int( float128 );
float128 float128_add( float128, float128 );
float128 float128_sub( float128, float128 );
float128 float128_mul( float128, float128 );
float128 float128_div( float128, float128 );
float128 float128_rem( float128, float128 );
float128 float128_sqrt( float128 );
flag float128_eq( float128, float128 );
flag float128_le( float128, float128 );
flag float128_lt( float128, float128 );
flag float128_eq_signaling( float128, float128 );
flag float128_le_quiet( float128, float128 );
flag float128_lt_quiet( float128, float128 );
flag float128_is_signaling_nan( float128 );

// functions originally internal to softfloat.c
float128 packFloat128( flag zSign, int32 zExp, bits64 zSig0, bits64 zSig1 );
float128 roundAndPackFloat128( flag zSign, int32 zExp, bits64 zSig0, bits64 zSig1, bits64 zSig2 );
float128 normalizeRoundAndPackFloat128( flag zSign, int32 zExp, bits64 zSig0, bits64 zSig1 );

#endif

#endif //SOFTFLOAT_H