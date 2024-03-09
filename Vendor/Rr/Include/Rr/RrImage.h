#pragma once

#include "RrCore.h"
#include "RrVulkan.h"

typedef struct SRr SRr;
typedef struct SRrAsset SRrAsset;

typedef struct SAllocatedImage
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
} SAllocatedImage;

SAllocatedImage Rr_CreateImage(SRr* const Rr, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped);
SAllocatedImage Rr_LoadImageRGBA8(SRrAsset* Asset, SRr* const Rr, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped);
void Rr_DestroyAllocatedImage(SRr* const Rr, SAllocatedImage* AllocatedImage);
