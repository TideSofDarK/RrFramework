#pragma once

#ifndef __cplusplus
    #include "Rr_Core.h"
#endif
// #include "RrArray.h"

typedef struct Rr_Asset
{
    const char* Data;
    size_t Length;
} Rr_Asset;

#define STR2(x) #x
#define STR(x) STR2(x)

#ifdef _WIN32
    #define INCBIN_SECTION ".rdata, \"dr\""
#elif defined(__APPLE__)
    #define INCBIN_SECTION "__TEXT,__const"
// #define INCBIN_SECTION ".const_data"
#else
    #define INCBIN_SECTION ".rodata"
#endif

// clang-format off
#ifdef __APPLE__
#define INCBIN(NAME, ASSET_DIRECTORY, ASSET_FILE) \
    __asm__(".section " INCBIN_SECTION "\n" \
            ".global " "_incbin" "_" STR(NAME) "_start\n" \
            ".balign 16\n" \
            "_incbin" "_" STR(NAME) "_start:\n" \
            ".incbin \"" ASSET_DIRECTORY ASSET_FILE "\"\n" \
            \
            ".global " "_incbin" "_" STR(NAME) "_end\n" \
            ".balign 1\n" \
            "_incbin" "_" STR(NAME) "_end:\n" \
            ".byte 0\n" \
    ); \
    extern  __attribute__((aligned(16))) const char incbin ## _ ## NAME ## _start[]; \
    extern                               const char incbin ## _ ## NAME ## _end[]
#else
#define INCBIN(NAME, ASSET_DIRECTORY, ASSET_FILE) \
    __asm__(".section " INCBIN_SECTION "\n" \
            ".global " STR(__USER_LABEL_PREFIX__) "incbin_" STR(NAME) "_start\n" \
            ".balign 16\n" \
            STR(__USER_LABEL_PREFIX__)"incbin_" STR(NAME) "_start:\n" \
            ".incbin \"" ASSET_DIRECTORY ASSET_FILE "\"\n" \
            \
            ".global " STR(__USER_LABEL_PREFIX__) "incbin_" STR(NAME) "_end\n" \
            ".balign 1\n" \
            STR(__USER_LABEL_PREFIX__)"incbin_" STR(NAME) "_end:\n" \
            ".byte 0\n" \
    ); \
    extern  __attribute__((aligned(16))) const char incbin_ ## NAME ## _start[]; \
    extern                               const char incbin_ ## NAME ## _end[]
#endif
// clang-format on

// #ifdef __clang__
//     #define EXTERN_OR_INLINE extern
// #else
//     #define EXTERN_OR_INLINE inline
// #endif

/* @TODO: cplusplus support! */

#define RrAsset_Define(NAME, PATH) INCBIN(NAME, STR(RR_ASSET_PATH), PATH)

#define RrAsset_Extern(VAR, NAME)                                               \
    do                                                                          \
    {                                                                           \
        extern __attribute__((aligned(16))) const char incbin_##NAME##_start[]; \
        extern const char incbin_##NAME##_end[];                                \
        (VAR)->Data = incbin_##NAME##_start;                                    \
        (VAR)->Length = (size_t)(incbin_##NAME##_end - incbin_##NAME##_start);  \
    }                                                                           \
    while (0)
