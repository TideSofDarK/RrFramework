#pragma once

#include "RrCore.h"
#include "RrArray.h"

typedef struct
{
    const char* Data;
    size_t Length;
} SRrAsset;

typedef struct
{
    SRrArray Vertices;
    SRrArray Indices;
} SRrRawMesh;

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
#define INCBIN(name, file) \
    __asm__(".section " INCBIN_SECTION "\n" \
            ".global " "_incbin" "_" STR(name) "_start\n" \
            ".balign 16\n" \
            "_incbin" "_" STR(name) "_start:\n" \
            ".incbin \"" file "\"\n" \
            \
            ".global " "_incbin" "_" STR(name) "_end\n" \
            ".balign 1\n" \
            "_incbin" "_" STR(name) "_end:\n" \
            ".byte 0\n" \
    ); \
    extern  __attribute__((aligned(16))) const char incbin ## _ ## name ## _start[]; \
    extern                               const char incbin ## _ ## name ## _end[];
#else
#define INCBIN(name, file) \
    __asm__(".section " INCBIN_SECTION "\n" \
            ".global " STR(__USER_LABEL_PREFIX__) "incbin_" STR(name) "_start\n" \
            ".balign 16\n" \
            STR(__USER_LABEL_PREFIX__)"incbin_" STR(name) "_start:\n" \
            ".incbin \"" file "\"\n" \
            \
            ".global " STR(__USER_LABEL_PREFIX__) "incbin_" STR(name) "_end\n" \
            ".balign 1\n" \
            STR(__USER_LABEL_PREFIX__)"incbin_" STR(name) "_end:\n" \
            ".byte 0\n" \
    ); \
    extern  __attribute__((aligned(16))) const char incbin_ ## name ## _start[]; \
    extern                               const char incbin_ ## name ## _end[];
#endif
// clang-format on

// #ifdef __clang__
//     #define EXTERN_OR_INLINE extern
// #else
//     #define EXTERN_OR_INLINE inline
// #endif

/* @TODO: cplusplus support! */

#define RR_ASSET_PATH ""
#define RrAsset_Define(NAME, PATH) \
    INCBIN(NAME, RR_ASSET_PATH PATH)

#define RrAsset_Extern(VAR, NAME)                                               \
    {                                                                           \
        extern __attribute__((aligned(16))) const char incbin_##NAME##_start[]; \
        extern const char incbin_##NAME##_end[];                                \
        (VAR)->Data = incbin_##NAME##_start;                                    \
        (VAR)->Length = (incbin_##NAME##_end - incbin_##NAME##_start);          \
    }

SRrRawMesh RrRawMesh_FromOBJAsset(SRrAsset* Asset);
