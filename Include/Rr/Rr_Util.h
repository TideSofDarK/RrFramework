#pragma once

#include <cglm/ivec2.h>
#include <cglm/vec2.h>
#include <cglm/vec4.h>

#include "Rr_Defines.h"

static inline size_t Rr_Align(size_t Value, size_t Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

static inline u16 Rr_FloatToHalf(u32 x)
{
#define Bit(N) ((uint32_t)1 << (N))
#define Mask(N) (((uint32_t)1 << (N)) - 1)
    uint32_t SignBit = x >> 31;
    uint32_t ExponentField = x >> 23 & Mask(8);
    uint32_t SignificandField = x & Mask(23);

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
        int Exponent = (int)ExponentField - 127 + 15;

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
        if (Bit(12) < T || Bit(12) == T && (Significand & Bit(13)))
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

static inline void Rr_PackVec4(vec4 From, u32* OutA, u32* OutB)
{
    typedef union PackHelper
    {
        u32 UnsignedIntegerValue;
        f32 FloatValue;
    } PackHelper;

    PackHelper Helper[4];

    SDL_memcpy(Helper, From, sizeof(vec4));

    u16 HalfValues[4];
    HalfValues[0] = Rr_FloatToHalf(Helper[0].UnsignedIntegerValue);
    HalfValues[1] = Rr_FloatToHalf(Helper[1].UnsignedIntegerValue);
    HalfValues[2] = Rr_FloatToHalf(Helper[2].UnsignedIntegerValue);
    HalfValues[3] = Rr_FloatToHalf(Helper[3].UnsignedIntegerValue);

    *OutA = (u32)HalfValues[0] | ((u32)HalfValues[1] << 16);
    *OutB = (u32)HalfValues[2] | ((u32)HalfValues[3] << 16);
}
