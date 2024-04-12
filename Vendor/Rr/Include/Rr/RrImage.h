#pragma once

#include "RrVulkan.h"
#include "RrDefines.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Asset Rr_Asset;
typedef struct Rr_Image Rr_Image;

Rr_Image Rr_CreateImage(Rr_Renderer* Renderer, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped);
Rr_Image Rr_CreateImage_FromPNG(Rr_Asset* Asset, Rr_Renderer* Renderer, VkImageUsageFlags Usage, b8 bMipMapped, VkImageLayout InitialLayout);
Rr_Image Rr_CreateImage_FromEXR(Rr_Asset* Asset, Rr_Renderer* Renderer);
void Rr_DestroyImage(Rr_Renderer* Renderer, Rr_Image* AllocatedImage);
