#pragma once

#ifndef __cplusplus
    #include <stddef.h>
    #include <stdint.h>
#else
    #include <cstddef>
    #include <cstdint>
#endif

typedef char Rr_Byte;
typedef uint8_t Rr_U8;
typedef uint16_t Rr_U16;
typedef uint32_t Rr_U32;
typedef uint64_t Rr_U64;
typedef int8_t Rr_I8;
typedef int16_t Rr_I16;
typedef int32_t Rr_I32;
typedef int64_t Rr_I64;
typedef float Rr_F32;
typedef double Rr_F64;
typedef intptr_t Rr_ISize;
typedef uintptr_t Rr_USize;
typedef const char *Rr_CString;

typedef Rr_I32 Rr_Bool;
#define RR_FALSE 0
#define RR_TRUE 1

/* Renderer Configuration */
#define RR_FORCE_DISABLE_TRANSFER_QUEUE 0
#define RR_PERFORMANCE_COUNTER 1
#define RR_MAX_OBJECTS 128

/* Arenas */
#define RR_PER_FRAME_ARENA_SIZE (1024 * 1024 * 2)
#define RR_PERMANENT_ARENA_SIZE (1024 * 1024)
#define RR_SYNC_ARENA_SIZE (1024 * 1024)
#define RR_LOADING_THREAD_ARENA_SIZE (1024 * 1024)
#define RR_MAIN_THREAD_SCRATCH_ARENA_SIZE (1024 * 1024 * 2)
#define RR_LOADING_THREAD_SCRATCH_SIZE (1024 * 1024 * 32)

/* Misc */
#define RR_MAX_LAYOUT_BINDINGS 4
#define RR_MAX_SWAPCHAIN_IMAGE_COUNT 8
#define RR_FRAME_OVERLAP 2
#define RR_DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#define RR_PRERENDERED_DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#define RR_COLOR_FORMAT VK_FORMAT_R8G8B8A8_UNORM
#define RR_STAGING_BUFFER_SIZE ((1 << 20) * 16)

#define RR_SAFE_ALIGNMENT 16

#define Rr_Align(Num, Alignment) \
    (((Num) + ((Alignment) - 1)) & ~((Alignment) - 1))

#ifdef __cplusplus
    #define Rr_ReinterpretCast(Type, Expression) \
        reinterpret_cast<Type>(Expression)
    #define Rr_StaticCast(Type, Expression) static_cast<Type>(Expression)
    #define Rr_ConstCast(Type, Expression) const_cast<Type>(Expression)
#else
    #define Rr_ReinterpretCast(Type, Expression) ((Type)(Expression))
    #define Rr_StaticCast(Type, Expression) ((Type)(Expression))
    #define Rr_ConstCast(Type, Expression) ((Type)(Expression))
#endif
