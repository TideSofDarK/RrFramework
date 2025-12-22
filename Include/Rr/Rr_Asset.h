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

#ifndef RR_ASSET_H
#define RR_ASSET_H

#include <Rr/Rr_Defines.h>

typedef struct Rr_Asset Rr_Asset;
struct Rr_Asset
{
    size_t Size;
    const void *Pointer;
};

#if defined(RR_USE_RC)

typedef struct Rr_AssetRef
{
    const char *Name;
} Rr_AssetRef;

#else

typedef struct Rr_AssetRef
{
    char *Start;
    char *End;
} Rr_AssetRef;

#define RR_STR2(X) #X
#define RR_STR(X)  RR_STR2(X)

#ifdef _WIN32
#define RR_INCBIN_SECTION ".rdata, \"dr\""
#elif defined(__APPLE__)
#define RR_INCBIN_SECTION "__TEXT,__const"
// #define RR_INCBIN_SECTION ".const_data"
#else
#define RR_INCBIN_SECTION ".rodata"
#endif

// clang-format off
    #ifdef __APPLE__
    #define RR_INCBIN(NAME, ABSOLUTE_PATH) \
        __asm__(".section " RR_INCBIN_SECTION "\n" \
                ".global " "_incbin" "_" RR_STR(NAME) "_start\n" \
                ".balign 16\n" \
                "_incbin" "_" RR_STR(NAME) "_start:\n" \
                ".incbin \"" ABSOLUTE_PATH "\"\n" \
                ".global " "_incbin" "_" RR_STR(NAME) "_end\n" \
                ".balign 1\n" \
                "_incbin" "_" RR_STR(NAME) "_end:\n" \
                ".byte 0\n" \
                ".text\n" \
        )
    #else
    #define RR_INCBIN(NAME, ABSOLUTE_PATH) \
        __asm__(".section " RR_INCBIN_SECTION "\n" \
                ".global " "incbin_" RR_STR(NAME) "_start\n" \
                ".balign 16\n" \
                "incbin_" RR_STR(NAME) "_start:\n" \
                ".incbin \"" ABSOLUTE_PATH "\"\n" \
                ".global " "incbin_" RR_STR(NAME) "_end\n" \
                ".balign 1\n" \
                "incbin_" RR_STR(NAME) "_end:\n" \
                ".byte 0\n" \
                ".text\n" \
        )
    #endif
// clang-format on

#define RR_INCBIN_REF(NAME)                                           \
    extern __attribute__((aligned(16))) char incbin_##NAME##_start[]; \
    extern char incbin_##NAME##_end[];                                \
    Rr_AssetRef NAME = {                                              \
        .Start = incbin_##NAME##_start,                               \
        .End = incbin_##NAME##_end,                                   \
    }

#endif

RR_EXTERN Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef);

#endif