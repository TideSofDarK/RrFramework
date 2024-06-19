#include "Rr/Rr_Utility.h"

#include <math.h>
#include <string.h>

Rr_F32 Rr_WrapMax(Rr_F32 X, Rr_F32 Max)
{
    return fmodf(Max + fmodf(X, Max), Max);
}

Rr_F32 Rr_WrapMinMax(Rr_F32 X, Rr_F32 Min, Rr_F32 Max)
{
    return Min + Rr_WrapMax(X - Min, Max - Min);
}

Rr_U16 Rr_FloatToHalf(Rr_U32 X)
{
#define Bit(N)  ((uint32_t)1 << (N))
#define Mask(N) (((uint32_t)1 << (N)) - 1)
    Rr_U32 SignBit = X >> 31;
    Rr_U32 ExponentField = X >> 23 & Mask(8);
    Rr_U32 SignificandField = X & Mask(23);

    if (ExponentField == Mask(8))
    {
        if (SignificandField == 0)
        {
            return SignBit << 15 | Mask(5) << 10;
        }
        else
        {
            SignificandField >>= 23 - 10;
            if (SignificandField == 0)
            {
                SignificandField = 1;
            }
            return SignBit << 15 | Mask(5) << 10 | SignificandField;
        }
    }

    else if (ExponentField == 0)
    {
        return SignBit << 15;
    }
    else
    {
        Rr_I32 Exponent = (Rr_I32)ExponentField - 127 + 15;

        if (Exponent < -11)
        {
            return SignBit << 15;
        }

        uint32_t Significand = Bit(23) | SignificandField;

        if (Exponent < 1)
        {
            uint32_t T = Significand << (32 - (1 - Exponent) - 13);
            Significand >>= 1 - Exponent + 13;
            if (Bit(31) < T)
            {
                ++Significand;
            }
            if (Bit(31) == T)
            {
                Significand += Significand & 1;
            }
            if (Bit(10) <= Significand)
            {
                return SignBit << 15 | 1 << 10 | 0;
            }
            return SignBit << 15 | 0 << 10 | (Significand & Mask(10));
        }

        uint32_t T = Significand & Mask(13);
        if (Bit(12) < T || (Bit(12) == T && (Significand & Bit(13))))
        {
            Significand += Bit(13);
        }
        Significand >>= 13;
        if (Bit(11) <= Significand)
        {
            ++Exponent;
            Significand >>= 1;
        }

        if (31 <= Exponent)
        {
            return SignBit << 15 | Mask(5) << 10;
        }

        return SignBit << 15 | Exponent << 10 | (Significand & Mask(10));
    }
#undef Bit
#undef Mask
}

void Rr_PackVec4(Rr_Vec4 From, Rr_U32 *OutA, Rr_U32 *OutB)
{
    typedef union PackHelper
    {
        Rr_U32 UnsignedIntegerValue;
        Rr_F32 FloatValue;
    } PackHelper;

    PackHelper Helper[4];

    memcpy(Helper, From.Elements, sizeof(Rr_Vec4));

    Rr_U16 HalfValues[4];
    HalfValues[0] = Rr_FloatToHalf(Helper[0].UnsignedIntegerValue);
    HalfValues[1] = Rr_FloatToHalf(Helper[1].UnsignedIntegerValue);
    HalfValues[2] = Rr_FloatToHalf(Helper[2].UnsignedIntegerValue);
    HalfValues[3] = Rr_FloatToHalf(Helper[3].UnsignedIntegerValue);

    *OutA = (Rr_U32)HalfValues[0] | ((Rr_U32)HalfValues[1] << 16);
    *OutB = (Rr_U32)HalfValues[2] | ((Rr_U32)HalfValues[3] << 16);
}
