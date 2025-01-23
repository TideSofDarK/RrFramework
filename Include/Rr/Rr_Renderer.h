#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_Memory.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RR_DEPTH_FORMAT             VK_FORMAT_D32_SFLOAT
#define RR_PRERENDERED_DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#define RR_COLOR_FORMAT             VK_FORMAT_R8G8B8A8_UNORM

typedef enum
{
    RR_SHADER_STAGE_VERTEX_BIT = (1 << 0),
    RR_SHADER_STAGE_FRAGMENT_BIT = (1 << 1),
} Rr_ShaderStage;

extern Rr_Arena *Rr_GetFrameArena(Rr_App *App);

extern Rr_Image *Rr_GetSwapchainImage(Rr_App *App);

#ifdef __cplusplus
}
#endif
