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

static inline float Rr_ToLinearChannel(float Value)
{
    return Value <= 0.0031308f ? Value * 12.92f
                               : powf(Value, 1.0f / 2.4f) * 1.055f - 0.055f;
}

static inline float Rr_ToSRGBChannel(float Value)
{
    return Value <= 0.04045f ? Value / 12.92f
                             : powf((Value + 0.055f) / 1.055f, 2.4f);
}

static inline void Rr_UIToLinearColor(Rr_Vec4 *Color)
{
    Color->R = Rr_ToLinearChannel(Color->R);
    Color->G = Rr_ToLinearChannel(Color->G);
    Color->B = Rr_ToLinearChannel(Color->B);
}

static inline void Rr_UIToSRGBColor(Rr_Vec4 *Color)
{
    Color->R = Rr_ToSRGBChannel(Color->R);
    Color->G = Rr_ToSRGBChannel(Color->G);
    Color->B = Rr_ToSRGBChannel(Color->B);
}

static inline Rr_Vec3 Rr_U32ToRGB(uint32_t Color)
{
    Rr_Vec3 Result;

    Result.R = (float)(Color >> 24) / 255.0f;
    Result.G = (float)((Color >> 16) & (0x000000FF)) / 255.0f;
    Result.B = (float)((Color >> 8) & (0x000000FF)) / 255.0f;

    return Result;
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

static inline uint32_t Rr_RGBAToU32(Rr_Vec4 Color)
{
    uint32_t Result = 0;

    Result |= (uint32_t)(Color.R * 255.0f) << 24u;
    Result |= (uint32_t)(Color.G * 255.0f) << 16u;
    Result |= (uint32_t)(Color.B * 255.0f) << 8u;
    Result |= (uint32_t)(Color.A * 255.0f);

    return Result;
}

typedef struct Rr_UTF8Decoder Rr_UTF8Decoder;
struct Rr_UTF8Decoder
{
    size_t CodepointCount;
    size_t CStringCodepointIndex;
    size_t CStringParserIndex;
    const char *CString;
    uint32_t Codepoint;
    uint8_t Carry;
};

static inline uint32_t Rr_UTF8Decode(Rr_UTF8Decoder *Decoder)
{
    static const uint8_t READY = 128;
    static const uint8_t TWO = 192;
    static const uint8_t INV_TWO = 63;
    static const uint8_t THREE = 224;
    static const uint8_t FOUR = 240;
    static const uint8_t FIVE = 248;

    while (true)
    {
        if (Decoder->Carry > 0)
        {
            Decoder->Carry--;
            uint8_t Raw =
                (uint8_t)Decoder->CString[Decoder->CStringParserIndex++];
            Decoder->Codepoint |=
                ((uint32_t)(INV_TWO & Raw) << (Decoder->Carry * 6));

            if (Decoder->Carry == 0)
            {
                Decoder->CodepointCount++;
                return Decoder->Codepoint;
            }
        }
        else
        {
            if ((Decoder->CString[Decoder->CStringParserIndex] & FOUR) == FOUR)
            {
                Decoder->Codepoint =
                    (uint8_t)(~FIVE &
                              Decoder->CString[Decoder->CStringParserIndex]);
                Decoder->Codepoint <<= 3 * 6;
                Decoder->Carry = 3;
                Decoder->CStringCodepointIndex = Decoder->CStringParserIndex++;
                continue;
            }
            else if (
                (Decoder->CString[Decoder->CStringParserIndex] & THREE) ==
                THREE)
            {
                Decoder->Codepoint =
                    (uint8_t)(~FOUR &
                              Decoder->CString[Decoder->CStringParserIndex]);
                Decoder->Codepoint <<= 2 * 6;
                Decoder->Carry = 2;
                Decoder->CStringCodepointIndex = Decoder->CStringParserIndex++;
                continue;
            }
            else if (
                (Decoder->CString[Decoder->CStringParserIndex] & TWO) == TWO)
            {
                Decoder->Codepoint =
                    (uint8_t)(~THREE &
                              Decoder->CString[Decoder->CStringParserIndex]);
                Decoder->Codepoint <<= 1 * 6;
                Decoder->Carry = 1;
                Decoder->CStringCodepointIndex = Decoder->CStringParserIndex++;
                continue;
            }
            else
            {
                Decoder->CodepointCount++;
                Decoder->Codepoint =
                    (uint32_t)(Decoder->CString[Decoder->CStringParserIndex] &
                               ~READY);
                Decoder->CStringCodepointIndex = Decoder->CStringParserIndex++;
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
    while ((CString[CurrentOffset] & 0xC0) == 0x80);
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
    while ((CString[CurrentOffset] & 0xC0) == 0x80);
    return CurrentOffset;
}

static inline size_t Rr_PreviousUTF8WordOffset(
    const char *CString,
    size_t CurrentOffset)
{
    bool ReachedSpace = false;
    bool ReachedWord = false;
    bool SkippingSpaces = false;
    if (CurrentOffset > 0 && CString[CurrentOffset - 1] == '\n')
    {
        return CurrentOffset - 1;
    }
    while (CurrentOffset > 0)
    {
        if (ReachedWord)
        {
            if (CString[CurrentOffset - 1] == ' ')
            {
                break;
            }
            if (CString[CurrentOffset - 1] == '\n')
            {
                break;
            }
        }
        CurrentOffset--;
        if (ReachedSpace && CString[CurrentOffset] == '\n')
        {
            break;
        }
        if (CString[CurrentOffset] != ' ')
        {
            ReachedWord = true;
        }
        else
        {
            if (!ReachedSpace && CString[CurrentOffset] == ' ')
            {
                ReachedSpace = true;
            }
        }
    }
    return CurrentOffset;
}

static inline size_t Rr_NextUTF8WordOffset(
    const char *CString,
    size_t CurrentOffset)
{
    bool ReachedSpace = false;
    if (CString[CurrentOffset] == '\n')
    {
        ReachedSpace = true;
    }
    while (CString[CurrentOffset] != '\0')
    {
        CurrentOffset++;
        if (CString[CurrentOffset] == '\n')
        {
            break;
        }
        if (ReachedSpace)
        {
            if (CString[CurrentOffset] != ' ')
            {
                break;
            }
        }
        else
        {
            if (CString[CurrentOffset] == ' ')
            {
                ReachedSpace = true;
            }
        }
    }
    return CurrentOffset;
}

static inline size_t Rr_LastUTF8CharInWordOffset(
    const char *CString,
    size_t CurrentOffset)
{
    while (CString[CurrentOffset] != '\0' && CString[CurrentOffset] != '\n' &&
           CString[CurrentOffset] != ' ')
    {
        CurrentOffset++;
    }
    return CurrentOffset;
}

static inline size_t Rr_PreviousUTF8LFOffset(
    const char *CString,
    size_t CurrentOffset)
{
    while (CurrentOffset != 0 && CString[CurrentOffset] != '\n')
    {
        CurrentOffset--;
    }
    return CurrentOffset;
}

static inline size_t Rr_NextUTF8LFOffset(
    const char *CString,
    size_t CurrentOffset)
{
    while (CString[CurrentOffset] != '\0' && CString[CurrentOffset] != '\n')
    {
        CurrentOffset++;
    }
    return CurrentOffset;
}

#ifdef __cplusplus
}
#endif
