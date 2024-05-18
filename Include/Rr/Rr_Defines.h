#pragma once

#ifndef __cplusplus
#include <stddef.h>
    #include <stdbool.h>
#else
#include <cstddef>
#endif

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

/* Renderer Configuration */
#define RR_FORCE_UNIFIED_QUEUE 0
#define RR_PERFORMANCE_COUNTER 1
#define RR_MAX_OBJECTS 128

/* Pipelines */
#define RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES 5
#define RR_PIPELINE_SHADER_STAGES 2
#define RR_PIPELINE_MAX_COLOR_ATTACHMENTS 2
#define RR_PIPELINE_MAX_DRAW_SIZE 128

#define RR_MAX_LAYOUT_BINDINGS 4
#define RR_MAX_SWAPCHAIN_IMAGE_COUNT 8
#define RR_FRAME_OVERLAP 2
#define RR_DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#define RR_PRERENDERED_DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#define RR_COLOR_FORMAT VK_FORMAT_R8G8B8A8_UNORM
#define RR_STAGING_BUFFER_SIZE ((1 << 20) * 16)

/* @TODO: Make a header shared with GLSL! */
#define RR_MAX_TEXTURES_PER_MATERIAL 4
