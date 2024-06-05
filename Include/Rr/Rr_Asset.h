#pragma once

#include "Rr_Defines.h"

#ifdef __cplusplus
    #define RR_ASSET_EXTERN extern "C"
#else
    #define RR_ASSET_EXTERN extern
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Rr_Asset Rr_Asset;

#if defined(RR_USE_RC)

    #include "Windows.h"

struct Rr_Asset
{
    const byte* Data;
    usize Length;
    HRSRC Resource;
    HGLOBAL Memory;
};

typedef const char* Rr_AssetRef;

#else

struct Rr_Asset
{
    const char* Data;
    const char* End;
    usize Length;
};

typedef struct Rr_AssetRef
{
    const char* Start;
    const char* End;
} Rr_AssetRef;

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
    #define INCBIN(NAME, ABSOLUTE_PATH) \
        __asm__(".section " INCBIN_SECTION "\n" \
                ".global " "_incbin" "_" STR(NAME) "_start\n" \
                ".balign 16\n" \
                "_incbin" "_" STR(NAME) "_start:\n" \
                ".incbin \"" ABSOLUTE_PATH "\"\n" \
                \
                ".global " "_incbin" "_" STR(NAME) "_end\n" \
                ".balign 1\n" \
                "_incbin" "_" STR(NAME) "_end:\n" \
                ".byte 0\n" \
        ); \
        RR_ASSET_EXTERN  __attribute__((aligned(16))) const char incbin ## _ ## NAME ## _start[]; \
        RR_ASSET_EXTERN                               const char incbin ## _ ## NAME ## _end[]; \
        const Rr_AssetRef NAME = { \
            .Start = incbin ## _ ## NAME ## _start, \
            .End = incbin ## _ ## NAME ## _end, \
        };
    #else
    #define INCBIN(NAME, ABSOLUTE_PATH) \
        __asm__(".section " INCBIN_SECTION "\n" \
                ".global " STR(__USER_LABEL_PREFIX__) "incbin_" STR(NAME) "_start\n" \
                ".balign 16\n" \
                STR(__USER_LABEL_PREFIX__)"incbin_" STR(NAME) "_start:\n" \
                ".incbin \"" ABSOLUTE_PATH "\"\n" \
                \
                ".global " STR(__USER_LABEL_PREFIX__) "incbin_" STR(NAME) "_end\n" \
                ".balign 1\n" \
                STR(__USER_LABEL_PREFIX__)"incbin_" STR(NAME) "_end:\n" \
                ".byte 0\n" \
        ); \
        RR_ASSET_EXTERN  __attribute__((aligned(16))) const char incbin_ ## NAME ## _start[]; \
        RR_ASSET_EXTERN                               const char incbin_ ## NAME ## _end[];   \
        const Rr_AssetRef NAME = { \
            .Start = incbin_ ## NAME ## _start, \
            .End = incbin_ ## NAME ## _end, \
        };
    #endif
// clang-format on


    #ifdef __cplusplus
    #else
    #endif

//    #ifdef __cplusplus
//        #ifdef __clang__
//            #define RR_EXTERN_OR_INLINE extern
//        #else
//            #define RR_EXTERN_OR_INLINE inline
//        #endif
//
//        #define Rr_DefineAsset(NAME, PATH)                                                         \
//            INCBIN(NAME, STR(RR_ASSET_PATH), PATH);                                                \
//            RR_EXTERN_OR_INLINE const Rr_Asset incbin_##NAME##_cpp                                 \
//            {                                                                                      \
//                &(incbin_##NAME##_start[0]), (size_t)(incbin_##NAME##_end - incbin_##NAME##_start) \
//            }
//
//        #define RR_ASSET_EXTERN extern "C"
//    #else
//        #define Rr_DefineAsset(NAME, PATH) INCBIN(NAME, STR(RR_ASSET_PATH), PATH)
//    #endif
//
//    #define Rr_ExternAssetAs(VAR, NAME)                                             \
//        do                                                                          \
//        {                                                                           \
//            extern __attribute__((aligned(16))) const char incbin_##NAME##_start[]; \
//            extern const char incbin_##NAME##_end[];                                \
//            (&VAR)->Data = incbin_##NAME##_start;                                   \
//            (&VAR)->Length = (size_t)(incbin_##NAME##_end - incbin_##NAME##_start); \
//        }                                                                           \
//        while (0)
//
//    #ifdef __cplusplus
//        #define Rr_ExternAsset(NAME)                          \
//            const Rr_Asset NAME{};                            \
//            do                                                \
//            {                                                 \
//                extern const Rr_Asset incbin_##NAME##_cpp;    \
//                Rr_Asset& Temp = const_cast<Rr_Asset&>(NAME); \
//                Temp = incbin_##NAME##_cpp;                   \
//            }                                                 \
//            while (0)
//    #else
//        #define Rr_ExternAsset(NAME) \
//            Rr_Asset NAME;           \
//            Rr_ExternAssetAs(NAME, NAME)
//    #endif

#endif

extern Rr_Asset Rr_LoadAsset(Rr_AssetRef AssetRef);

#ifdef __cplusplus
}
#endif

// #undef RR_ASSET_EXTERN