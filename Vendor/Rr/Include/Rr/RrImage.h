#pragma once

#include "RrVulkan.h"
#include "RrDefines.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Asset Rr_Asset;
typedef struct Rr_Image Rr_Image;

Rr_Image Rr_CreateImage(Rr_Renderer* Rr, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped);
Rr_Image Rr_LoadImageRGBA8(Rr_Asset* Asset, Rr_Renderer* Rr, VkImageUsageFlags Usage, b8 bMipMapped, VkImageLayout InitialLayout);
void Rr_DestroyImage(Rr_Renderer* Rr, Rr_Image* AllocatedImage);
