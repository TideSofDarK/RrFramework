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

extern void Rr_UploadStagingImage(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_Image *Image,
    VkImageAspectFlags Aspect,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Buffer *StagingBuffer,
    size_t StagingOffset,
    size_t StagingSize);

extern void Rr_UploadImage(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_Image *Image,
    VkImageAspectFlags Aspect,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Data Data);

extern Rr_Image *Rr_CreateImageRGBA8(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    char *Data,
    uint32_t Width,
    uint32_t Height,
    bool MipMapped);

Rr_Image *Rr_CreateImageRGBA8FromPNG(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    size_t DataSize,
    char *Data,
    bool MipMapped);

// extern Rr_Image *Rr_CreateDepthImageFromEXR(
//     Rr_App *App,
//     Rr_UploadContext *UploadContext,
//     Rr_AssetRef AssetRef,
//     Rr_Arena *Arena);

extern size_t Rr_GetImagePNGRGBA8Size(
    size_t DataSize,
    char *Data,
    Rr_Arena *Arena);

// extern void Rr_GetImageSizeEXR(Rr_AssetRef AssetRef, Rr_Arena *Arena,
// Rr_LoadSize *OutLoadSize);

extern Rr_AllocatedImage *Rr_GetCurrentAllocatedImage(
    Rr_App *App,
    Rr_Image *Image);
