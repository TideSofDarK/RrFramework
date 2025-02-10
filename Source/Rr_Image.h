#pragma once

#include <Rr/Rr_Image.h>

#include "Rr_Memory.h"
#include "Rr_UploadContext.h"

#include <Rr/Rr_App.h>
#include <Rr/Rr_Asset.h>

#include <vma/vk_mem_alloc.h>

struct Rr_Sampler
{
    VkSampler Handle;
};

typedef struct Rr_AllocatedImage Rr_AllocatedImage;
struct Rr_AllocatedImage
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    Rr_Image *Container;
};

struct Rr_Image
{
    VkExtent3D Extent;
    VkFormat Format;
    VkImageAspectFlags AspectFlags;
    size_t AllocatedImageCount;
    Rr_AllocatedImage AllocatedImages[RR_MAX_FRAME_OVERLAP];
};

extern Rr_Image *Rr_CreateColorImageFromMemory(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    char *Data,
    uint32_t Width,
    uint32_t Height,
    bool MipMapped);

extern Rr_Image *Rr_CreateColorImageFromPNGMemory(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    size_t DataSize,
    char *Data,
    bool MipMapped);

extern Rr_Image *Rr_CreateColorImageFromPNG(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    bool MipMapped,
    Rr_Arena *Arena);

extern Rr_Image *Rr_CreateDepthImageFromEXR(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Arena *Arena);

extern Rr_Image *Rr_CreateDepthAttachmentImage(Rr_App *App, uint32_t Width, uint32_t Height);

extern void Rr_GetImageSizePNGMemory(size_t DataSize, char *Data, Rr_Arena *Arena, Rr_LoadSize *OutLoadSize);

extern void Rr_GetImageSizePNG(Rr_AssetRef AssetRef, Rr_Arena *Arena, Rr_LoadSize *OutLoadSize);

extern void Rr_GetImageSizeEXR(Rr_AssetRef AssetRef, Rr_Arena *Arena, Rr_LoadSize *OutLoadSize);

extern Rr_AllocatedImage *Rr_GetCurrentAllocatedImage(Rr_App *App, Rr_Image *Image);
