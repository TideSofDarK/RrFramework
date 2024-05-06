#pragma once

#include "Rr_Framework.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct Rr_Material
{
    Rr_Buffer* Buffer;
    Rr_Image* Textures[RR_MAX_TEXTURES_PER_MATERIAL];
    size_t TextureCount;
};

Rr_Material Rr_CreateMaterial(Rr_Renderer* Renderer, Rr_Image** Textures, size_t TextureCount);
void Rr_DestroyMaterial(Rr_Renderer* Renderer, Rr_Material* Material);

#ifdef __cplusplus
}
#endif
