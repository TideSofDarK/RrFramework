#pragma once

#include "Rr_App.h"
#include "Rr_Image.h"
#include "Rr_Asset.h"
#include "Rr_Memory.h"

struct Rr_UploadContext;

extern Rr_Image* Rr_CreateColorImageFromPNG(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_Asset* Asset,
    bool bMipMapped,
    Rr_Arena* Arena);
extern Rr_Image* Rr_CreateDepthImageFromEXR(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_Asset* Asset,
    Rr_Arena* Arena);
extern Rr_Image* Rr_CreateColorAttachmentImage(Rr_App* App, u32 Width, u32 Height);
extern Rr_Image* Rr_CreateDepthAttachmentImage(Rr_App* App, u32 Width, u32 Height);

extern size_t Rr_GetImageSizePNG(const Rr_Asset* Asset, Rr_Arena* Arena);
extern size_t Rr_GetImageSizeEXR(const Rr_Asset* Asset, Rr_Arena* Arena);
