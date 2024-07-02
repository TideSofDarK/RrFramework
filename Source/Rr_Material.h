#pragma once

#include "Rr/Rr_Material.h"

struct Rr_Buffer;

struct Rr_Material
{
    Rr_GenericPipeline *GenericPipeline;
    struct Rr_Buffer *Buffer;
    Rr_Image *Textures[RR_MAX_TEXTURES_PER_MATERIAL];
    uintptr_t TextureCount;
    uint8_t bOwning;
};
