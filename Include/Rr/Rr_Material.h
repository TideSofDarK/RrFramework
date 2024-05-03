#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Defines.h"
#include "Rr_Buffer.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Image Rr_Image;

typedef struct Rr_Material
{
    Rr_Buffer Buffer;
    Rr_Image* Textures[RR_MAX_TEXTURES_PER_MATERIAL];
    size_t TextureCount;
} Rr_Material;

Rr_Material Rr_CreateMaterial(const Rr_Renderer* Renderer, Rr_Image** Textures, size_t TextureCount);
void Rr_DestroyMaterial(const Rr_Renderer* Renderer, const Rr_Material* Material);
