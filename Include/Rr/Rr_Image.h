#pragma once

#include "Rr_App.h"
#include "Rr_Defines.h"
#include "Rr_Asset.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct Rr_UploadContext;

typedef struct Rr_Image Rr_Image;

Rr_Image* Rr_CreateColorImageFromPNG(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_Asset* Asset,
    bool bMipMapped);
Rr_Image* Rr_CreateDepthImageFromEXR(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_Asset* Asset);
Rr_Image* Rr_CreateColorAttachmentImage(Rr_App* App, u32 Width, u32 Height);
Rr_Image* Rr_CreateDepthAttachmentImage(Rr_App* App, u32 Width, u32 Height);
void Rr_DestroyImage(Rr_App* App, Rr_Image* AllocatedImage);

size_t Rr_GetImageSizePNG(const Rr_Asset* Asset);
size_t Rr_GetImageSizeEXR(const Rr_Asset* Asset);

#ifdef __cplusplus
}
#endif
