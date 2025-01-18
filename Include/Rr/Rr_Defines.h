#pragma once

#ifndef __cplusplus
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#else
#include <cstddef>
#include <cstdint>
#endif

#ifdef __cplusplus
#define RR_EXTERN extern "C"
#else
#define RR_EXTERN extern
#endif

#define RR_MIN(A, B)      (((A) < (B)) ? (A) : (B))
#define RR_MAX(A, B)      (((A) > (B)) ? (A) : (B))
#define RR_CLAMP(A, X, B) (((X) < (A)) ? (A) : ((X) > (B)) ? (B) : (X))

#define RR_ZERO(x)     memset(&(x), 0, sizeof((x)))
#define RR_ZERO_PTR(x) memset((x), 0, sizeof(*(x)))

#define RR_KILOBYTES(Value) ((Value) * 1024ull)
#define RR_MEGABYTES(Value) ((Value) * RR_KILOBYTES(1024))
#define RR_GIGABYTES(Value) ((Value) * RR_MEGABYTES(1024))

#ifdef __cplusplus
#define RR_REINTERPRET_CAST(Type, Expression) reinterpret_cast<Type>(Expression)
#define RR_STATIC_CAST(Type, Expression)      static_cast<Type>(Expression)
#define RR_CONST_CAST(Type, Expression)       const_cast<Type>(Expression)
#else
#define RR_REINTERPRET_CAST(Type, Expression) ((Type)(Expression))
#define RR_STATIC_CAST(Type, Expression)      ((Type)(Expression))
#define RR_CONST_CAST(Type, Expression)       ((Type)(Expression))
#endif

#define RR_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

typedef struct Rr_Data Rr_Data;
struct Rr_Data
{
    void *Pointer;
    size_t Size;
};

/* Macro Overloading */

#define RR_EXPAND(X)                          X
#define RR_GET_MACRO_2(_1, _2, Name, ...)     Name
#define RR_GET_MACRO_3(_1, _2, _3, Name, ...) Name

/* Make Data Helper */

#ifdef __cplusplus
#define RR_MAKE_DATA_STRUCT(Struct)              { &(Struct), sizeof(Struct) }
#define RR_MAKE_DATA_POINTER_SIZE(Pointer, Size) { Pointer, Size }
#else
#define RR_MAKE_DATA_STRUCT(Struct) \
    (Rr_Data)                       \
    {                               \
        &(Struct), sizeof(Struct)   \
    }
#define RR_MAKE_DATA_POINTER_SIZE(Pointer, Size) \
    (Rr_Data)                                    \
    {                                            \
        Pointer, Size                            \
    }
#endif

#define RR_MAKE_DATA(...) \
    RR_EXPAND(RR_GET_MACRO_2(__VA_ARGS__, RR_MAKE_DATA_POINTER_SIZE, RR_MAKE_DATA_STRUCT)(__VA_ARGS__))

/* Alignment */

#define RR_SAFE_ALIGNMENT             16
#define RR_ALIGN_POW2(Num, Alignment) (((Num) + ((Alignment) - 1)) & ~((Alignment) - 1))

/* Renderer Configuration */

#define RR_FORCE_DISABLE_TRANSFER_QUEUE 0
#define RR_PERFORMANCE_COUNTER          1
#define RR_MAX_OBJECTS                  128
#define RR_MAX_LAYOUT_BINDINGS          4
#define RR_MAX_SWAPCHAIN_IMAGE_COUNT    8
#define RR_FRAME_OVERLAP                2
#define RR_DEPTH_FORMAT                 VK_FORMAT_D32_SFLOAT
#define RR_PRERENDERED_DEPTH_FORMAT     VK_FORMAT_D32_SFLOAT
#define RR_COLOR_FORMAT                 VK_FORMAT_R8G8B8A8_UNORM
#define RR_STAGING_BUFFER_SIZE          RR_MEGABYTES(16)

/* Arenas */

#define RR_PER_FRAME_ARENA_SIZE           RR_MEGABYTES(2)
#define RR_PERMANENT_ARENA_SIZE           RR_MEGABYTES(1)
#define RR_SYNC_ARENA_SIZE                RR_MEGABYTES(1)
#define RR_LOADING_THREAD_ARENA_SIZE      RR_MEGABYTES(1)
#define RR_MAIN_THREAD_SCRATCH_ARENA_SIZE RR_MEGABYTES(2)
#define RR_LOADING_THREAD_SCRATCH_SIZE    RR_MEGABYTES(32)
