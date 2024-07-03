#pragma once

#include "Rr/Rr_App.h"
#include "Rr/Rr_Image.h"
#include "Rr/Rr_Pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @TODO: Make a header shared with GLSL! */
#define RR_MAX_TEXTURES_PER_MATERIAL 4

typedef struct Rr_Material Rr_Material;

Rr_Material *Rr_CreateMaterial(
    Rr_App *App,
    Rr_GenericPipeline *GenericPipeline,
    Rr_Image **Textures,
    size_t TextureCount);

void Rr_DestroyMaterial(Rr_App *App, Rr_Material *Material);

#ifdef __cplusplus
}
#endif
