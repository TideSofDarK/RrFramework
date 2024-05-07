#pragma once

typedef struct Rr_Material Rr_Material;

#include "Rr_Image.h"

#ifdef __cplusplus
extern "C"
{
#endif

Rr_Material* Rr_CreateMaterial(Rr_Renderer* Renderer, Rr_Image** Textures, size_t TextureCount);
void Rr_DestroyMaterial(Rr_Renderer* Renderer, Rr_Material* Material);

#ifdef __cplusplus
}
#endif
