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

#ifdef __cplusplus
}
#endif
