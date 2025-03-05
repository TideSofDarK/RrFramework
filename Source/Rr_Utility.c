#include <Rr/Rr_Utility.h>

#include <math.h>
#include <string.h>

size_t Rr_NextPowerOfTwo(size_t Number)
{
    return 1 << (size_t)ceil(log2(Number));
}

float Rr_WrapMax(float X, float Max)
{
    return fmodf(Max + fmodf(X, Max), Max);
}

float Rr_WrapMinMax(float X, float Min, float Max)
{
    return Min + Rr_WrapMax(X - Min, Max - Min);
}

uint16_t Rr_FloatToHalf(uint32_t X)
{
#define Bit(N)  ((uint32_t)1 << (N))
#define Mask(N) (((uint32_t)1 << (N)) - 1)
    uint32_t SignBit = X >> 31;
    uint32_t ExponentField = X >> 23 & Mask(8);
    uint32_t SignificandField = X & Mask(23);

    if(ExponentField == Mask(8))
    {
        if(SignificandField == 0)
        {
            return SignBit << 15 | Mask(5) << 10;
        }
        else
        {
            SignificandField >>= 23 - 10;
            if(SignificandField == 0)
            {
                SignificandField = 1;
            }
            return SignBit << 15 | Mask(5) << 10 | SignificandField;
        }
    }

    else if(ExponentField == 0)
    {
        return SignBit << 15;
    }
    else
    {
        int32_t Exponent = (int32_t)ExponentField - 127 + 15;

        if(Exponent < -11)
        {
            return SignBit << 15;
        }

        uint32_t Significand = Bit(23) | SignificandField;

        if(Exponent < 1)
        {
            uint32_t T = Significand << (32 - (1 - Exponent) - 13);
            Significand >>= 1 - Exponent + 13;
            if(Bit(31) < T)
            {
                ++Significand;
            }
            if(Bit(31) == T)
            {
                Significand += Significand & 1;
            }
            if(Bit(10) <= Significand)
            {
                return SignBit << 15 | 1 << 10 | 0;
            }
            return SignBit << 15 | 0 << 10 | (Significand & Mask(10));
        }

        uint32_t T = Significand & Mask(13);
        if(Bit(12) < T || (Bit(12) == T && (Significand & Bit(13))))
        {
            Significand += Bit(13);
        }
        Significand >>= 13;
        if(Bit(11) <= Significand)
        {
            ++Exponent;
            Significand >>= 1;
        }

        if(31 <= Exponent)
        {
            return SignBit << 15 | Mask(5) << 10;
        }

        return SignBit << 15 | Exponent << 10 | (Significand & Mask(10));
    }
#undef Bit
#undef Mask
}

void Rr_PackVec4(Rr_Vec4 From, uint32_t *OutA, uint32_t *OutB)
{
    typedef union PackHelper
    {
        uint32_t UnsignedIntegerValue;
        float FloatValue;
    } PackHelper;

    PackHelper Helper[4];

    memcpy(Helper, From.Elements, sizeof(Rr_Vec4));

    uint16_t HalfValues[4];
    HalfValues[0] = Rr_FloatToHalf(Helper[0].UnsignedIntegerValue);
    HalfValues[1] = Rr_FloatToHalf(Helper[1].UnsignedIntegerValue);
    HalfValues[2] = Rr_FloatToHalf(Helper[2].UnsignedIntegerValue);
    HalfValues[3] = Rr_FloatToHalf(Helper[3].UnsignedIntegerValue);

    *OutA = (uint32_t)HalfValues[0] | ((uint32_t)HalfValues[1] << 16);
    *OutB = (uint32_t)HalfValues[2] | ((uint32_t)HalfValues[3] << 16);
}

Rr_Vec4 Rr_FitRect(Rr_Vec4 Src, Rr_Vec4 Dst)
{
    float X = 0;
    float Y = 0;
    float Width = 0;
    float Height = 0;

    float DstWidth = Dst.Width;
    float DstHeight = Dst.Height;
    float SrcWidth = Src.Width;
    float SrcHeight = Dst.Height;
    float DstRatio = DstWidth / DstHeight;
    float SrcRatio = SrcWidth / SrcHeight;

    if(DstRatio > SrcRatio)
    {
        Width = (SrcWidth / SrcHeight) * DstHeight;
        Height = DstHeight;
    }
    else
    {
        Width = DstWidth;
        Height = (SrcHeight / SrcWidth) * DstWidth;
    }
    X = (DstWidth / 2.0f) - (Width / 2.0f);
    Y = (DstHeight / 2.0f) - (Height / 2.0f);

    Rr_Vec4 Result = { X, Y, Width, Height };

    return Result;
}

Rr_IntVec4 Rr_FitIntRect(Rr_IntVec4 Src, Rr_IntVec4 Dst)
{
    float X = 0;
    float Y = 0;
    float Width = 0;
    float Height = 0;

    float DstWidth = Dst.Width;
    float DstHeight = Dst.Height;
    float SrcWidth = Src.Width;
    float SrcHeight = Dst.Height;
    float DstRatio = DstWidth / DstHeight;
    float SrcRatio = SrcWidth / SrcHeight;

    if(DstRatio > SrcRatio)
    {
        Width = (SrcWidth / SrcHeight) * DstHeight;
        Height = DstHeight;
    }
    else
    {
        Width = DstWidth;
        Height = (SrcHeight / SrcWidth) * DstWidth;
    }
    X = (DstWidth / 2.0f) - (Width / 2.0f);
    Y = (DstHeight / 2.0f) - (Height / 2.0f);

    Rr_IntVec4 Result = { X, Y, Width, Height };

    return Result;
}
