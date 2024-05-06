#pragma once

#include "Rr_Framework.h"
#include "Rr_Vulkan.h"

struct Rr_Image
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
};

Rr_Image* Rr_CreateImage(
    Rr_Renderer* Renderer,
    VkExtent3D Extent,
    VkFormat Format,
    VkImageUsageFlags Usage,
    bool bMipMapped);
Rr_Image* Rr_CreateColorImageFromPNG(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    Rr_Asset* Asset,
    VkImageUsageFlags Usage,
    bool bMipMapped,
    VkImageLayout InitialLayout);
Rr_Image* Rr_CreateDepthImageFromEXR(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    Rr_Asset* Asset);
Rr_Image* Rr_CreateColorAttachmentImage(Rr_Renderer* Renderer, VkExtent3D Extent);
Rr_Image* Rr_CreateDepthAttachmentImage(Rr_Renderer* Renderer, VkExtent3D Extent);

typedef struct Rr_ImageBarrier
{
    VkCommandBuffer CommandBuffer;
    VkImage Image;
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout Layout;
} Rr_ImageBarrier;

void Rr_ChainImageBarrier_Aspect(
    Rr_ImageBarrier* TransitionImage,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout NewLayout,
    VkImageAspectFlagBits Aspect);
void Rr_ChainImageBarrier(
    Rr_ImageBarrier* TransitionImage,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout NewLayout);

size_t Rr_GetImageSizePNG(const Rr_Asset* Asset);
size_t Rr_GetImageSizeEXR(const Rr_Asset* Asset);


