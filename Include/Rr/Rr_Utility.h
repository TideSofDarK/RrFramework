/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <Rr/Rr_Math.h>

#ifdef __cplusplus
#include <cstring>
#else
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

static size_t Rr_NextPowerOfTwo(size_t Number)
{
    return (size_t)1 << (size_t)ceil(log2((double)Number));
}

static float Rr_WrapMax(float X, float Max)
{
    return fmodf(Max + fmodf(X, Max), Max);
}

static float Rr_WrapMinMax(float X, float Min, float Max)
{
    return Min + Rr_WrapMax(X - Min, Max - Min);
}

static uint16_t Rr_FloatToHalf(uint32_t X)
{
#define Bit(N)  ((uint32_t)1 << (N))
#define Mask(N) (((uint32_t)1 << (N)) - 1)
    uint32_t SignBit = X >> 31;
    uint32_t ExponentField = X >> 23 & Mask(8);
    uint32_t SignificandField = X & Mask(23);

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
        int32_t Exponent = (int32_t)ExponentField - 127 + 15;

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

static void Rr_PackVec4(Rr_Vec4 From, uint32_t *OutA, uint32_t *OutB)
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

static inline Rr_Vec4 Rr_U32ToRGBA(uint32_t Color)
{
    Rr_Vec4 Result;

    Result.R = (float)(Color >> 24) / 255.0f;
    Result.G = (float)((Color >> 16) & (0x000000FF)) / 255.0f;
    Result.B = (float)((Color >> 8) & (0x000000FF)) / 255.0f;
    Result.A = (float)(Color & (0x000000FF)) / 255.0f;

    return Result;
}

static inline uint32_t Rr_RGBAtoU32(Rr_Vec4 Color)
{
    uint32_t Result = 0;

    Result |= (uint8_t)(Color.R * 255.0f) << 24;
    Result |= (uint8_t)(Color.G * 255.0f) << 16;
    Result |= (uint8_t)(Color.B * 255.0f) << 8;
    Result |= (uint8_t)(Color.A * 255.0f);

    return Result;
}

typedef struct Rr_UTF8Decoder Rr_UTF8Decoder;
struct Rr_UTF8Decoder
{
    size_t CodepointIndex;
    size_t CStringIndex;
    const char *CString;
    uint32_t Codepoint;
    uint8_t Carry;
};

static inline uint32_t Rr_UTF8Decode(Rr_UTF8Decoder *Decoder)
{
    static const uint8_t READY = 128;
    static const uint8_t TWO = 192;
    static const uint8_t THREE = 224;
    static const uint8_t FOUR = 240;
    static const uint8_t FIVE = 248;

    while (true)
    {
        if (Decoder->Carry > 0)
        {
            Decoder->Carry--;
            Decoder->Codepoint |=
                (uint8_t)((~TWO & Decoder->CString[Decoder->CStringIndex])
                          << (Decoder->Carry * 6));

            if (Decoder->Carry == 0)
            {
                Decoder->CodepointIndex++;
                Decoder->CStringIndex++;
                return Decoder->Codepoint;
            }
        }
        else
        {
            if ((Decoder->CString[Decoder->CStringIndex] & FOUR) == FOUR)
            {
                Decoder->Codepoint =
                    (uint8_t)(~FIVE & Decoder->CString[Decoder->CStringIndex]);
                Decoder->Codepoint <<= 3 * 6;
                Decoder->Carry = 3;
                Decoder->CStringIndex++;
                continue;
            }
            else if ((Decoder->CString[Decoder->CStringIndex] & THREE) == THREE)
            {
                Decoder->Codepoint =
                    (uint8_t)(~FOUR & Decoder->CString[Decoder->CStringIndex]);
                Decoder->Codepoint <<= 2 * 6;
                Decoder->Carry = 2;
                Decoder->CStringIndex++;
                continue;
            }
            else if ((Decoder->CString[Decoder->CStringIndex] & TWO) == TWO)
            {
                Decoder->Codepoint =
                    (uint8_t)(~THREE & Decoder->CString[Decoder->CStringIndex]);
                Decoder->Codepoint <<= 1 * 6;
                Decoder->Carry = 1;
                Decoder->CStringIndex++;
                continue;
            }
            else
            {
                Decoder->CodepointIndex++;
                Decoder->Codepoint =
                    Decoder->CString[Decoder->CStringIndex] & ~READY;
                Decoder->CStringIndex++;
                return Decoder->Codepoint;
            }
        }
    }
}

static inline size_t Rr_PreviousUTF8CodepointOffset(
    const char *CString,
    size_t CurrentOffset)
{
    do
    {
        CurrentOffset--;
    }
    while ((*(CString + CurrentOffset) & 0xC0) == 0x80);
    return CurrentOffset;
}

static inline size_t Rr_NextUTF8CodepointOffset(
    const char *CString,
    size_t CurrentOffset)
{
    do
    {
        CurrentOffset++;
    }
    while ((*(CString + CurrentOffset) & 0xC0) == 0x80);
    return CurrentOffset;
}

static inline size_t Rr_PreviousUTF8LFOffset(
    const char *CString,
    size_t CurrentOffset)
{
    size_t Total = 0;
    do
    {
        Total++;
        if (CurrentOffset == 0)
        {
            break;
        }
        CurrentOffset--;
    }
    while (*(CString + CurrentOffset) != '\n');
    return Total;
}

static inline size_t Rr_NextUTF8LFOffset(
    const char *CString,
    size_t CurrentOffset)
{
    size_t Total = 0;
    do
    {
        CurrentOffset++;
        Total++;
    }
    while (*(CString + CurrentOffset) != '\0' &&
           *(CString + CurrentOffset) != '\n');
    return Total;
}

#ifdef __cplusplus
}
#endif
