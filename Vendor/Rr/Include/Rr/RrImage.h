#pragma once

#include "RrVulkan.h"
#include "RrDefines.h"

typedef struct SRr SRr;
typedef struct SRrAsset SRrAsset;
typedef struct SAllocatedImage SAllocatedImage;

SAllocatedImage Rr_CreateImage(SRr* Rr, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped);
SAllocatedImage Rr_LoadImageRGBA8(SRrAsset* Asset, SRr* Rr, VkImageUsageFlags Usage, b8 bMipMapped);
void Rr_DestroyImage(SRr* Rr, SAllocatedImage* AllocatedImage);
