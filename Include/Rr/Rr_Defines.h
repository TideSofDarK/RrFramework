#pragma once

#ifndef __cplusplus
    #include <stddef.h>
    #include <stdint.h>
    #include <stdbool.h>
#else
    #include <cstddef>
    #include <cstdint>
#endif

typedef char byte;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;
typedef float f32;
typedef double f64;
typedef intptr_t size;
typedef uintptr_t usize;
typedef const char* str;

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

#define Rr_Align(Num, Alignment) (((Num) + ((Alignment) - 1)) & ~((Alignment) - 1))
#define RR_SAFE_ALIGNMENT 16
