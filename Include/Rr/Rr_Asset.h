#pragma once

#include <Rr/Rr_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Asset Rr_Asset;
struct Rr_Asset
{
    char *Data;
    size_t Length;
};

#if defined(RR_USE_RC)

typedef const char *Rr_AssetRef;

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

extern Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef);

#ifdef __cplusplus
}
#endif
