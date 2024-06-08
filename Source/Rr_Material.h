#pragma once

#include "Rr/Rr_Material.h"

struct Rr_GenericPipeline;
struct Rr_Buffer;

struct Rr_Material
{
    struct Rr_GenericPipeline* GenericPipeline;
    struct Rr_Buffer* Buffer;
    Rr_Image* Textures[RR_MAX_TEXTURES_PER_MATERIAL];
    usize TextureCount;
    u8 bOwning;
};
