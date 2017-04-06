
/*============================================================================

This C source file is an extension to the SoftFloat IEC/IEEE Floating-point 
Arithmetic Package, Release 2a.

=============================================================================*/

#include "softfloat.h"

/*----------------------------------------------------------------------------
| Methods for converting decimal floats to binary extended precision floats.
*----------------------------------------------------------------------------*/

INLINE void round128to64(flag aSign, int32 *aExp, bits64 *aSig0, bits64 *aSig1)
{
    flag increment;
    int32 zExp;
    bits64 zSig0, zSig1;
    
    zExp = *aExp;
    zSig0 = *aSig0;
    zSig1 = *aSig1;
    
    increment = ( (sbits64) zSig1 < 0 );
    if (float_rounding_mode != float_round_nearest_even) {
        if (float_rounding_mode == float_round_to_zero) {
            increment = 0;
        } else {
            if (aSign) {
                increment = (float_rounding_mode == float_round_down) && zSig1;
            } else {
                increment = (float_rounding_mode == float_round_up) && zSig1;
            }
        }
    }
    
    if (increment) {
        ++zSig0;
        if (zSig0 == 0) {
            ++zExp;
            zSig0 = LIT64(0x8000000000000000);
        } else {
            zSig0 &= ~ (((bits64) (zSig1<<1) == 0) & (float_rounding_mode == float_round_nearest_even));
        }
    } else {
        if ( zSig0 == 0 ) zExp = 0;
    }
    
    *aExp = zExp;
    *aSig0 = zSig0;
    *aSig1 = 0;
}

INLINE void mul128by128round(int32 *aExp, bits64 *aSig0, bits64 *aSig1, int32 bExp, bits64 bSig0, bits64 bSig1)
{
    int32 zExp;
    bits64 zSig0, zSig1, zSig2, zSig3;
    
    zExp = *aExp;
    zSig0 = *aSig0;
    zSig1 = *aSig1;
    
    round128to64(0, &bExp, &bSig0, &bSig1);
    
    zExp += bExp - 0x3FFE;
    mul128To256(zSig0, zSig1, bSig0, bSig1, &zSig0, &zSig1, &zSig2, &zSig3);
    zSig1 |= (zSig2 | zSig3) != 0;
    if ( 0 < (sbits64) zSig0 ) {
        shortShift128Left( zSig0, zSig1, 1, &zSig0, &zSig1 );
        --zExp;
    }
    *aExp = zExp;
    *aSig0 = zSig0;
    *aSig1 = zSig1;
    
    round128to64(0, aExp, aSig0, aSig1);
}

INLINE void mul128by128(int32 *aExp, bits64 *aSig0, bits64 *aSig1, int32 bExp, bits64 bSig0, bits64 bSig1)
{
    int32 zExp;
    bits64 zSig0, zSig1, zSig2, zSig3;
    
    zExp = *aExp;
    zSig0 = *aSig0;
    zSig1 = *aSig1;

    zExp += bExp - 0x3FFE;
    mul128To256(zSig0, zSig1, bSig0, bSig1, &zSig0, &zSig1, &zSig2, &zSig3);
    zSig1 |= (zSig2 | zSig3) != 0;
    if ( 0 < (sbits64) zSig0 ) {
        shortShift128Left( zSig0, zSig1, 1, &zSig0, &zSig1 );
        --zExp;
    }
    *aExp = zExp;
    *aSig0 = zSig0;
    *aSig1 = zSig1;
}

INLINE void div128by128(int32 *paExp, bits64 *paSig0, bits64 *paSig1, int32 bExp, bits64 bSig0, bits64 bSig1)
{
    int32 zExp, aExp;
    bits64 zSig0, zSig1, aSig0, aSig1;
    bits64 rem0, rem1, rem2, rem3, term0, term1, term2, term3;
    
    aExp = *paExp;
    aSig0 = *paSig0;
    aSig1 = *paSig1;
    
    zExp = aExp - bExp + 0x3FFE;
    if ( le128( bSig0, bSig1, aSig0, aSig1 ) ) {
        shift128Right( aSig0, aSig1, 1, &aSig0, &aSig1 );
        ++zExp;
    }
    zSig0 = estimateDiv128To64( aSig0, aSig1, bSig0 );
    mul128By64To192( bSig0, bSig1, zSig0, &term0, &term1, &term2 );
    sub192( aSig0, aSig1, 0, term0, term1, term2, &rem0, &rem1, &rem2 );
    while ( (sbits64) rem0 < 0 ) {
        --zSig0;
        add192( rem0, rem1, rem2, 0, bSig0, bSig1, &rem0, &rem1, &rem2 );
    }
    zSig1 = estimateDiv128To64( rem1, rem2, bSig0 );
    if ( ( zSig1 & 0x3FFF ) <= 4 ) {
        mul128By64To192( bSig0, bSig1, zSig1, &term1, &term2, &term3 );
        sub192( rem1, rem2, 0, term1, term2, term3, &rem1, &rem2, &rem3 );
        while ( (sbits64) rem1 < 0 ) {
            --zSig1;
            add192( rem1, rem2, rem3, 0, bSig0, bSig1, &rem1, &rem2, &rem3 );
        }
        zSig1 |= ( ( rem1 | rem2 | rem3 ) != 0 );
    }

    *paExp = zExp;
    *paSig0 = zSig0;
    *paSig1 = zSig1;
}

INLINE void tentoint128(flag mSign, flag eSign, int32 *aExp, bits64 *aSig0, bits64 *aSig1, int32 scale)
{
    int8 save_rounding_mode;
    int32 mExp;
    bits64 mSig0, mSig1;
    
    save_rounding_mode = float_rounding_mode;
    switch (float_rounding_mode) {
        case float_round_nearest_even:
            break;
        case float_round_down:
            if (mSign != eSign) {
                float_rounding_mode = float_round_up;
            }
            break;
        case float_round_up:
            if (mSign != eSign) {
                float_rounding_mode = float_round_down;
            }
            break;
        case float_round_to_zero:
            if (eSign == 0) {
                float_rounding_mode = float_round_down;
            } else {
                float_rounding_mode = float_round_up;
            }
            break;
        default:
            break;
    }
    
    *aExp = 0x3FFF;
    *aSig0 = LIT64(0x8000000000000000);
    *aSig1 = 0;

    mExp = 0x4002;
    mSig0 = LIT64(0xA000000000000000);
    mSig1 = 0;
    
    while (scale) {
        if (scale & 1) {
            mul128by128round(aExp, aSig0, aSig1, mExp, mSig0, mSig1);
        }
        mul128by128(&mExp, &mSig0, &mSig1, mExp, mSig0, mSig1);
        scale >>= 1;
    }
    
    float_rounding_mode = save_rounding_mode;
}

INLINE int64 tentointdec(int32 scale)
{
    bits64 decM, decX;
    
    decX = 1;
    decM = 10;
    
    while (scale) {
        if (scale & 1) {
            decX *= decM;
        }
        decM *= decM;
        scale >>= 1;
    }
    
    return decX;
}

INLINE int64 float128toint64(flag zSign, int32 zExp, bits64 zSig0, bits64 zSig1)
{
    int8 roundingMode;
    flag roundNearestEven, increment;
    int64 z;
    
    shift128RightJamming(zSig0, zSig1, 0x403E - zExp, &zSig0, &zSig1);

    roundingMode = float_rounding_mode;
    roundNearestEven = (roundingMode == float_round_nearest_even);
    increment = ((sbits64)zSig1 < 0);
    if (!roundNearestEven) {
        if (roundingMode == float_round_to_zero) {
            increment = 0;
        } else {
            if (zSign) {
                increment = (roundingMode == float_round_down ) && zSig1;
            } else {
                increment = (roundingMode == float_round_up ) && zSig1;
            }
        }
    }
    if (increment) {
        ++zSig0;
        zSig0 &= ~ (((bits64)(zSig1<<1) == 0) & roundNearestEven);
    }
    z = zSig0;
    if (zSig1) float_raise(float_flag_inexact);
    return z;
}

INLINE int32 getDecimalExponent(int32 aExp, bits64 aSig)
{
    flag zSign;
    int32 zExp, shiftCount;
    bits64 zSig0, zSig1;
    
    if (aSig == 0 || aExp == 0x3FFF) {
        return 0;
    }
    if (aExp < 0) {
        return -4932;
    }

    aSig ^= LIT64(0x8000000000000000);
    aExp -= 0x3FFF;
    zSign = (aExp < 0);
    aExp = zSign ? -aExp : aExp;
    shiftCount = 31 - countLeadingZeros32(aExp);
    zExp = 0x3FFF + shiftCount;
    
    if (shiftCount < 0) {
        shortShift128Left(aSig, 0, -shiftCount, &zSig0, &zSig1);
    } else {
        shift128Right(aSig, 0, shiftCount, &zSig0, &zSig1);
        aSig = (bits64)aExp << (63 - shiftCount);
        if (zSign) {
            sub128(aSig, 0, zSig0, zSig1, &zSig0, &zSig1);
        } else {
            add128(aSig, 0, zSig0, zSig1, &zSig0, &zSig1);
        }
    }
    
    shiftCount = countLeadingZeros64(zSig0);
    shortShift128Left(zSig0, zSig1, shiftCount, &zSig0, &zSig1);
    zExp -= shiftCount;
    mul128by128(&zExp, &zSig0, &zSig1, 0x3FFD, LIT64(0x9A209A84FBCFF798), LIT64(0x8F8959AC0B7C9178));
    
    shiftCount = 0x403E - zExp;
    shift128RightJamming(zSig0, zSig1, shiftCount, &zSig0, &zSig1);

    if ((sbits64)zSig1 < 0) {
        ++zSig0;
        zSig0 &= ~((bits64)(zSig1<<1) == 0);
    }
    
    zExp = zSign ? -zSig0 : zSig0;

    return zExp;
}

/*----------------------------------------------------------------------------
| Decimal to binary
*----------------------------------------------------------------------------*/

floatx80 floatdecimal_to_floatx80(floatx80 a)
{
    flag decSign, zSign, decExpSign;
    int32 decExp, zExp, xExp, shiftCount;
    bits64 decSig, zSig0, zSig1, xSig0, xSig1;
    
    decSign = extractFloatx80Sign(a);
    decExp = extractFloatx80Exp(a);
    decSig = extractFloatx80Frac(a);
    
    if (decExp == 0x7FFF) return a;
    
    if (decExp == 0 && decSig == 0) return a;
    
    decExpSign = (decExp >> 14) & 1;
    decExp &= 0x3FFF;
    
    shiftCount = countLeadingZeros64( decSig );
    zExp = 0x403E - shiftCount;
    zSig0 = decSig << shiftCount;
    zSig1 = 0;
    zSign = decSign;
    
    tentoint128(decSign, decExpSign, &xExp, &xSig0, &xSig1, decExp);
    
    if (decExpSign) {
        div128by128(&zExp, &zSig0, &zSig1, xExp, xSig0, xSig1);
    } else {
        mul128by128(&zExp, &zSig0, &zSig1, xExp, xSig0, xSig1);
    }
    
    if (zSig1) float_raise(float_flag_decimal);
    round128to64(zSign, &zExp, &zSig0, &zSig1);
    
    return packFloatx80( zSign, zExp, zSig0 );
}


/*----------------------------------------------------------------------------
 | Binary to decimal
 *----------------------------------------------------------------------------*/

floatx80 floatx80_to_floatdecimal(floatx80 a, int32 *k)
{
    flag aSign, decSign;
    int32 aExp, decExp, zExp, xExp;
    bits64 aSig, decSig, decX, zSig0, zSig1, xSig0, xSig1;
    flag ictr, lambda;
    int32 kfactor, ilog, iscale, len;
    
    aSign = extractFloatx80Sign(a);
    aExp = extractFloatx80Exp(a);
    aSig = extractFloatx80Frac(a);
    
    if (aExp == 0x7FFF) {
        if ((bits64) (aSig<<1)) return propagateFloatx80NaNOneArg(a);
        return a;
    }
    
    if (aExp == 0) {
        if (aSig == 0) return packFloatx80(aSign, 0, 0);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }

    kfactor = *k;
    
    ilog = getDecimalExponent(aExp, aSig);
    
    ictr = 0;
    
try_again:

    if (kfactor > 0) {
        if (kfactor > 17) {
            kfactor = 17;
            float_raise(float_flag_invalid);
        }
        len = kfactor;
    } else {
        len = ilog + 1 - kfactor;
        if (len > 17) {
            len = 17;
        }
        if (len < 1) {
            len = 1;
        }
        if (kfactor > ilog) {
            ilog = kfactor;
        }
    }
#ifdef SOFTFLOAT_DECIMAL_DEBUG
    printf("ILOG = %i, LEN = %i\n", ilog, len);
#endif

    lambda = 0;
    iscale = ilog + 1 - len;
    
    if (iscale < 0) {
        lambda = 1;
        iscale = -iscale;
    }
#ifdef SOFTFLOAT_DECIMAL_DEBUG
    printf("ISCALE = %i, LAMBDA = %i\n",iscale,lambda);
#endif
    
    tentoint128(lambda, 0, &xExp, &xSig0, &xSig1, iscale);
    
    zExp = aExp;
    zSig0 = aSig;
    zSig1 = 0;
    
    if (lambda) {
        mul128by128(&zExp, &zSig0, &zSig1, xExp, xSig0, xSig1);
    } else {
        div128by128(&zExp, &zSig0, &zSig1, xExp, xSig0, xSig1);
    }
#ifdef SOFTFLOAT_DECIMAL_DEBUG
    printf("BEFORE: zExp = %04x, zSig0 = %16llx, zSig1 = %16llx\n",zExp,zSig0,zSig1);
#endif
    
    decSig = float128toint64(aSign, zExp, zSig0, zSig1);

#ifdef SOFTFLOAT_DECIMAL_DEBUG
    printf("AFTER: decSig = %llu\n",decSig);
#endif
    
    if (ictr == 0) {

        decX = tentointdec(len - 1);
        
        if (decSig < decX) { // z < x
            ilog -= 1;
            ictr = 1;
            goto try_again;
        }
        
        decX *= 10;
        
        if (decSig > decX) { // z > x
            ilog += 1;
            ictr = 1;
            goto try_again;
        }
    }
    
    decSign = aSign;
    decExp = (ilog < 0) ? -ilog : ilog;
    if (decExp > 999) {
        float_raise(float_flag_invalid);
    }
    if (ilog < 0) decExp |= 0x4000;
    
    *k = len;
    
    return packFloatx80(decSign, decExp, decSig);
}
