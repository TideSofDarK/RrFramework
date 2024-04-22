#pragma once

#include "RrVulkan.h"
#include "RrDefines.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Asset Rr_Asset;

typedef struct Rr_Image
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
} Rr_Image;

Rr_Image Rr_CreateImage(Rr_Renderer* Renderer, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped);
Rr_Image Rr_CreateImageFromPNG(Rr_Asset* Asset, Rr_Renderer* Renderer, VkImageUsageFlags Usage, b8 bMipMapped, VkImageLayout InitialLayout);
Rr_Image Rr_CreateImageFromEXR(Rr_Asset* Asset, Rr_Renderer* Renderer);
Rr_Image Rr_CreateColorAttachmentImage(Rr_Renderer* Renderer, VkExtent3D Extent);
void Rr_DestroyImage(Rr_Renderer* Renderer, Rr_Image* AllocatedImage);

typedef struct Rr_ImageBarrier
{
    VkCommandBuffer CommandBuffer;
    VkImage Image;
    VkPipelineStageFlags2 StageMask;
    VkAccessFlags2 AccessMask;
    VkImageLayout Layout;
} Rr_ImageBarrier;

void Rr_ChainImageBarrier_Aspect(
    Rr_ImageBarrier* TransitionImage,
    VkPipelineStageFlags2 DstStageMask,
    VkAccessFlags2 DstAccessMask,
    VkImageLayout NewLayout,
    VkImageAspectFlagBits Aspect);
void Rr_ChainImageBarrier(
    Rr_ImageBarrier* TransitionImage,
    VkPipelineStageFlags2 DstStageMask,
    VkAccessFlags2 DstAccessMask,
    VkImageLayout NewLayout);
