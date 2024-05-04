#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Types.h"

Rr_Image* Rr_CreateImage(
    Rr_Renderer* Renderer,
    VkExtent3D Extent,
    VkFormat Format,
    VkImageUsageFlags Usage,
    b8 bMipMapped);
Rr_Image* Rr_CreateColorImageFromPNG(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    Rr_Asset* Asset,
    VkImageUsageFlags Usage,
    b8 bMipMapped,
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
