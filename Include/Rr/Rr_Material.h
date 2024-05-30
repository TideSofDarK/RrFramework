#pragma once

#include "Rr_Defines.h"
#include "Rr_Image.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct Rr_App;
struct Rr_GenericPipeline;

/* @TODO: Make a header shared with GLSL! */
#define RR_MAX_TEXTURES_PER_MATERIAL 4

typedef struct Rr_Material Rr_Material;

Rr_Material* Rr_CreateMaterial(struct Rr_App* App, struct Rr_GenericPipeline* GenericPipeline, Rr_Image** Textures, usize TextureCount);
void Rr_DestroyMaterial(struct Rr_App* App, Rr_Material* Material);

#ifdef __cplusplus
}
#endif
