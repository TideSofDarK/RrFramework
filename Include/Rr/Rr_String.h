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

#include <Rr/Rr_Memory.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_String Rr_String;
struct Rr_String
{
    uint32_t *Data;
    size_t Length;
};

extern Rr_String Rr_CreateString(
    const char *CString,
    size_t LengthHint,
    Rr_Arena *Arena);

#define RR_STRING(CString, Arena) \
    Rr_CreateString(CString, sizeof(CString), Arena)

extern Rr_String Rr_CreateEmptyString(size_t Length, Rr_Arena *Arena);

extern void Rr_UpdateString(
    Rr_String *String,
    size_t MaxLength,
    const char *CString,
    size_t LengthHint);

#ifdef __cplusplus
}
#endif
