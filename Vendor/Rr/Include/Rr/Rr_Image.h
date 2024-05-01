#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Defines.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Asset Rr_Asset;
typedef struct Rr_StagingBuffer Rr_StagingBuffer;

typedef struct Rr_Image
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
} Rr_Image;

size_t Rr_GetImageSizePNG(const Rr_Asset* Asset);
size_t Rr_GetImageSizeEXR(const Rr_Asset* Asset);

void Rr_UploadImage(
    const Rr_Renderer* Renderer,
    Rr_StagingBuffer* StagingBuffer,
    const VkCommandBuffer GraphicsCommandBuffer,
    const VkCommandBuffer TransferCommandBuffer,
    VkImage Image,
    VkExtent3D Extent,
    VkImageAspectFlags Aspect,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout DstLayout,
    const void* ImageData,
    const size_t ImageDataLength);

Rr_Image Rr_CreateImage(const Rr_Renderer* Renderer, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped);
Rr_Image Rr_CreateColorImageFromPNG(
    const Rr_Renderer* Renderer,
    VkCommandBuffer GraphicsCommandBuffer,
    VkCommandBuffer TransferCommandBuffer,
    Rr_StagingBuffer* StagingBuffer,
    const Rr_Asset* Asset,
    VkImageUsageFlags Usage,
    b8 bMipMapped,
    VkImageLayout InitialLayout);
Rr_Image Rr_CreateDepthImageFromEXR(
    const Rr_Renderer* Renderer,
    VkCommandBuffer GraphicsCommandBuffer,
    VkCommandBuffer TransferCommandBuffer,
    Rr_StagingBuffer* StagingBuffer,
    const Rr_Asset* Asset);
Rr_Image Rr_CreateColorAttachmentImage(const Rr_Renderer* Renderer, VkExtent3D Extent);
Rr_Image Rr_CreateDepthAttachmentImage(const Rr_Renderer* Renderer, VkExtent3D Extent);
void Rr_DestroyImage(const Rr_Renderer* Renderer, const Rr_Image* AllocatedImage);

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
