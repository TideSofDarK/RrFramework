#pragma once

#include "Rr_Framework.h"

#ifdef __cplusplus
extern "C"
{
#endif

size_t Rr_GetImageSizePNG(const Rr_Asset* Asset);
size_t Rr_GetImageSizeEXR(const Rr_Asset* Asset);

void Rr_DestroyImage(Rr_Renderer* Renderer, Rr_Image* AllocatedImage);

#ifdef __cplusplus
}
#endif
