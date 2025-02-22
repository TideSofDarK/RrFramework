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

#if UINTPTR_MAX == UINT32_MAX
#define RR_HALF_UINTPTR uint16_t
#elif UINTPTR_MAX == UINT64_MAX
#define RR_HALF_UINTPTR uint32_t
#else
#error "Unrecognized UINTPTR_MAX value!"
#endif

#define RR_UNUSED(Var) (void)Var

#define RR_HAS_BIT(Value, Bit) ((Value & Bit) != 0)

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
    size_t Size;
    void *Pointer;
};

/* Make Data Helper */

#ifdef __cplusplus
#define RR_MAKE_DATA_STRUCT(Struct) { sizeof(Struct), &(Struct) }
#define RR_MAKE_DATA_ARRAY(Array)   { sizeof(Array), (Array) }
#define RR_MAKE_DATA(Size, Pointer) { Size, Pointer }
#else
#define RR_MAKE_DATA_STRUCT(Struct) \
    (Rr_Data)                       \
    {                               \
        sizeof(Struct), &(Struct)   \
    }
#define RR_MAKE_DATA_ARRAY(Struct) \
    (Rr_Data)                      \
    {                              \
        sizeof(Struct), (Struct)   \
    }
#define RR_MAKE_DATA(Size, Pointer) \
    (Rr_Data)                       \
    {                               \
        Size, Pointer               \
    }
#endif

/* Alignment */

#define RR_SAFE_ALIGNMENT 16
#define RR_ALIGN_POW2(Num, Alignment) \
    (((Num) + ((Alignment) - 1)) & ~((Alignment) - 1))

/* Renderer Configuration */

#define RR_FORCE_DISABLE_TRANSFER_QUEUE 0
#define RR_PERFORMANCE_COUNTER          1
#define RR_MAX_OBJECTS                  128
#define RR_MAX_FRAME_OVERLAP            3
#define RR_FRAME_OVERLAP                2
#define RR_STAGING_BUFFER_SIZE          RR_MEGABYTES(16)

/* Arenas */

#define RR_PER_FRAME_ARENA_SIZE           RR_MEGABYTES(2)
#define RR_PERMANENT_ARENA_SIZE           RR_MEGABYTES(1)
#define RR_SYNC_ARENA_SIZE                RR_MEGABYTES(1)
#define RR_LOADING_THREAD_ARENA_SIZE      RR_MEGABYTES(1)
#define RR_MAIN_THREAD_SCRATCH_ARENA_SIZE RR_MEGABYTES(2)
#define RR_LOADING_THREAD_SCRATCH_SIZE    RR_MEGABYTES(32)
