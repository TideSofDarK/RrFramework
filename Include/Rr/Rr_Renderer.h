#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Memory.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    RR_SHADER_STAGE_VERTEX_BIT = (1 << 0),
    RR_SHADER_STAGE_FRAGMENT_BIT = (1 << 1),
} Rr_ShaderStage;

extern Rr_Arena *Rr_GetFrameArena(Rr_App *App);

#ifdef __cplusplus
}
#endif
