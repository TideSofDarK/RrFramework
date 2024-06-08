#pragma once

#include "Rr/Rr_Image.h"
#include "Rr/Rr_Asset.h"
#include "Rr/Rr_App.h"
#include "Rr_Memory.h"
#include "Rr_UploadContext.h"

#include <volk.h>

#include <vma/vk_mem_alloc.h>

struct Rr_Image
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
};

extern Rr_Image* Rr_CreateImage(
    Rr_App* App,
    VkExtent3D Extent,
    VkFormat Format,
    VkImageUsageFlags Usage,
    bool bMipMapped);
extern Rr_Image* Rr_CreateColorImageFromPNGMemory(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    byte* Data,
    usize DataSize,
    bool bMipMapped);
extern Rr_Image* Rr_CreateColorImageFromPNG(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_AssetRef AssetRef,
    bool bMipMapped,
    Rr_Arena* Arena);
extern Rr_Image* Rr_CreateDepthImageFromEXR(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Arena* Arena);
extern Rr_Image* Rr_CreateColorAttachmentImage(Rr_App* App, u32 Width, u32 Height);
extern Rr_Image* Rr_CreateDepthAttachmentImage(Rr_App* App, u32 Width, u32 Height);

extern void Rr_GetImageSizePNGMemory(
    byte* Data,
    usize DataSize,
    Rr_Arena* Arena,
    Rr_LoadSize* OutLoadSize);
extern void Rr_GetImageSizePNG(
    Rr_AssetRef Asset,
    Rr_Arena* Arena,
    Rr_LoadSize* OutLoadSize);
extern void Rr_GetImageSizeEXR(
    Rr_AssetRef Asset,
    Rr_Arena* Arena,
    Rr_LoadSize* OutLoadSize);
