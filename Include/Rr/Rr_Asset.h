/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef RR_ASSET_H
#define RR_ASSET_H

#include <Rr/Rr_Defines.h>

typedef struct Rr_Asset Rr_Asset;
struct Rr_Asset
{
    size_t Size;
    void const *Data;
};

#if defined(RR_USE_RC)

typedef struct Rr_AssetRef
{
    char const *Name;
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

#ifdef __cplusplus
extern "C" {
#endif

extern Rr_Asset RR_CC Rr_LoadAsset(Rr_AssetRef AssetRef);

#ifdef __cplusplus
}
#endif

#endif
