#pragma once

#include "Rr/Rr_App.h"
#include "Rr/Rr_Asset.h"
#include "Rr/Rr_Image.h"
#include "Rr_Memory.h"
#include "Rr_UploadContext.h"

#include <vma/vk_mem_alloc.h>

struct Rr_Image
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
};

extern Rr_Image *Rr_CreateImage(
    Rr_App *App,
    VkExtent3D Extent,
    VkFormat Format,
    VkImageUsageFlags Usage,
    Rr_Bool bMipMapped);

extern Rr_Image *Rr_CreateColorImageFromMemory(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_Byte *Data,
    Rr_U32 Width,
    Rr_U32 Height,
    Rr_Bool bMipMapped);

extern Rr_Image *Rr_CreateColorImageFromPNGMemory(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_Byte *Data,
    Rr_USize DataSize,
    Rr_Bool bMipMapped);

extern Rr_Image *Rr_CreateColorImageFromPNG(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Bool bMipMapped,
    Rr_Arena *Arena);

extern Rr_Image *Rr_CreateDepthImageFromEXR(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Arena *Arena);

extern Rr_Image *Rr_CreateColorAttachmentImage(
    Rr_App *App,
    Rr_U32 Width,
    Rr_U32 Height);

extern Rr_Image *Rr_CreateDepthAttachmentImage(
    Rr_App *App,
    Rr_U32 Width,
    Rr_U32 Height);

extern void Rr_GetImageSizePNGMemory(
    Rr_Byte *Data,
    Rr_USize DataSize,
    Rr_Arena *Arena,
    Rr_LoadSize *OutLoadSize);

extern void Rr_GetImageSizePNG(
    Rr_AssetRef AssetRef,
    Rr_Arena *Arena,
    Rr_LoadSize *OutLoadSize);

extern void Rr_GetImageSizeEXR(
    Rr_AssetRef AssetRef,
    Rr_Arena *Arena,
    Rr_LoadSize *OutLoadSize);

typedef struct Rr_ImageBarrier Rr_ImageBarrier;
struct Rr_ImageBarrier
{
    VkCommandBuffer CommandBuffer;
    VkImage Image;
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout Layout;
};

extern void Rr_ChainImageBarrier_Aspect(
    Rr_ImageBarrier *TransitionImage,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout NewLayout,
    VkImageAspectFlagBits Aspect);

extern void Rr_ChainImageBarrier(
    Rr_ImageBarrier *TransitionImage,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout NewLayout);
