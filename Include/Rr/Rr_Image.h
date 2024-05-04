#pragma once

#include "Rr_Defines.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Asset Rr_Asset;
typedef struct Rr_StagingBuffer Rr_StagingBuffer;
typedef struct Rr_UploadContext Rr_UploadContext;
typedef struct Rr_Image Rr_Image;

size_t Rr_GetImageSizePNG(const Rr_Asset* Asset);
size_t Rr_GetImageSizeEXR(const Rr_Asset* Asset);

void Rr_DestroyImage(Rr_Renderer* Renderer, Rr_Image* AllocatedImage);

#ifdef __cplusplus
}
#endif
