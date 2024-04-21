#pragma once

#include "RrVulkan.h"
#include "RrTypes.h"
#include "RrDefines.h"
#include "RrBuffer.h"

typedef struct Rr_Material
{
    Rr_Buffer Buffer;
    Rr_Image* Textures[RR_MAX_TEXTURES_PER_MATERIAL];
    size_t TextureCount;
} Rr_Material;

Rr_Material Rr_CreateMaterial(Rr_Renderer* Renderer, Rr_Image** Textures, size_t TextureCount);
void Rr_DestroyMaterial(Rr_Renderer* Renderer, Rr_Material* Material);
