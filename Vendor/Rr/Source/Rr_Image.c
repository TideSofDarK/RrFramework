#include "Rr_Image.h"
#include "Rr_Memory.h"

#include <stdlib.h>

#include <SDL3/SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC Rr_Malloc
#define STBI_REALLOC Rr_Realloc
#define STBI_FREE Rr_Free
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <tinyexr/tinyexr.h>

#include "Rr_Renderer.h"
#include "Rr_Asset.h"
#include "Rr_Helpers.h"
#include "Rr_Vulkan.h"
#include "Rr_Buffer.h"

static Rr_Image Rr_CreateImage_Internal(const Rr_Renderer* Renderer, const VkExtent3D Extent, const VkFormat Format, const VkImageUsageFlags Usage, const VmaAllocationCreateInfo AllocationCreateInfo, const b8 bMipMapped)
{
    Rr_Image Image = { 0 };
    Image.Format = Format;
    Image.Extent = Extent;

    VkImageCreateInfo Info = GetImageCreateInfo(Image.Format, Usage, Image.Extent);

    if (bMipMapped)
    {
        Info.mipLevels = (u32)(SDL_floor(SDL_log(SDL_max(Extent.width, Extent.height)))) + 1;
    }

    vmaCreateImage(Renderer->Allocator, &Info, &AllocationCreateInfo, &Image.Handle, &Image.Allocation, NULL);

    VkImageAspectFlags AspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (Format == RR_DEPTH_FORMAT)
    {
        AspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo ViewInfo = GetImageViewCreateInfo(Image.Format, Image.Handle, AspectFlag);
    ViewInfo.subresourceRange.levelCount = Info.mipLevels;

    vkCreateImageView(Renderer->Device, &ViewInfo, NULL, &Image.View);

    return Image;
}

Rr_Image Rr_CreateImage(const Rr_Renderer* Renderer, const VkExtent3D Extent, const VkFormat Format, const VkImageUsageFlags Usage, const b8 bMipMapped)
{
    const VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    return Rr_CreateImage_Internal(Renderer, Extent, Format, Usage, AllocationCreateInfo, bMipMapped);
}

Rr_Image Rr_CreateImageFromPNG(const Rr_Renderer* Renderer, const Rr_Asset* Asset, const VkImageUsageFlags Usage, const b8 bMipMapped, const VkImageLayout InitialLayout)
{
    const i32 DesiredChannels = 4;
    i32 Channels;
    VkExtent3D Extent = { .depth = 1 };
    stbi_uc* ParsedImage = stbi_load_from_memory((stbi_uc*)Asset->Data, (i32)Asset->Length, (i32*)&Extent.width, (i32*)&Extent.height, &Channels, DesiredChannels);

    const size_t DataSize = Extent.width * Extent.height * DesiredChannels;

    Rr_Buffer Buffer = Rr_CreateMappedBuffer(Renderer, DataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    SDL_memcpy(Buffer.AllocationInfo.pMappedData, ParsedImage, DataSize);
    stbi_image_free(ParsedImage);

    const Rr_Image Image = Rr_CreateImage(Renderer, Extent, RR_COLOR_FORMAT, Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, bMipMapped);

    Rr_BeginImmediate(Renderer);
    Rr_ImageBarrier Transition = {
        .Image = Image.Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .AccessMask = VK_ACCESS_2_NONE,
        .CommandBuffer = Renderer->ImmediateMode.CommandBuffer,
    };
    Rr_ChainImageBarrier(&Transition, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    const VkBufferImageCopy Copy = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageExtent = Extent,
    };

    vkCmdCopyBufferToImage(Renderer->ImmediateMode.CommandBuffer, Buffer.Handle, Image.Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Copy);

    Rr_ChainImageBarrier(&Transition, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT, InitialLayout);
    Rr_EndImmediate(Renderer);

    Rr_DestroyBuffer(Renderer, &Buffer);

    return Image;
}

Rr_Image Rr_CreateDepthImageFromEXR(Rr_Asset* Asset, Rr_Renderer* const Renderer)
{
    VkImageUsageFlags Usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    int Result;
    const char* Error;

    EXRVersion Version;
    Result = ParseEXRVersionFromMemory(&Version, (const unsigned char*)Asset->Data, Asset->Length);
    if (Result != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error opening EXR file!");
        abort();
    }

    EXRHeader Header;
    Result = ParseEXRHeaderFromMemory(&Header, &Version, (const unsigned char*)Asset->Data, Asset->Length, &Error);
    if (Result != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error opening EXR file: %s", Error);
        FreeEXRErrorMessage(Error);
        abort();
    }

    EXRImage Image;
    InitEXRImage(&Image);

    Result = LoadEXRImageFromMemory(&Image, &Header, (const unsigned char*)Asset->Data, Asset->Length, &Error);
    if (Result != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error opening EXR file: %s", Error);
        FreeEXRHeader(&Header);
        FreeEXRErrorMessage(Error);
        abort();
    }

    /* Calculate depth (https://en.wikipedia.org/wiki/Z-buffering) */
    const float Near = 0.5f;
    const float Far = 50.0f;
    const float FarPlusNear = Far + Near;
    const float FarMinusNear = Far - Near;
    const float FTimesNear = Far * Near;
    for (int Index = 0; Index < Image.width * Image.height; Index++)
    {
        float* const Current = ((float*)Image.images[0]) + Index;
        const float ZReciprocal = 1.0f / *Current;
        float Depth = FarPlusNear / FarMinusNear + ZReciprocal * ((-2.0f * FTimesNear) / (FarMinusNear));
        Depth = (Depth + 1.0f) / 2.0f;
        if (Depth > 1.0f)
        {
            Depth = 1.0f;
        }
        *Current = Depth;
    }

    VkExtent3D Extent = { .width = Image.width, .height = Image.height, .depth = 1 };

    size_t DataSize = Extent.width * Extent.height * sizeof(f32);

    Rr_Buffer Buffer = Rr_CreateBuffer(Renderer, DataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true);

    SDL_memcpy(Buffer.AllocationInfo.pMappedData, Image.images[0], DataSize);

    Rr_Image DepthImage = Rr_CreateImage(Renderer, Extent, RR_PRERENDERED_DEPTH_FORMAT, Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);

    Rr_BeginImmediate(Renderer);
    Rr_ImageBarrier Transition = {
        .Image = DepthImage.Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .AccessMask = VK_ACCESS_2_NONE,
        .CommandBuffer = Renderer->ImmediateMode.CommandBuffer,
    };
    Rr_ChainImageBarrier_Aspect(
        &Transition,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    VkBufferImageCopy Copy = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageExtent = Extent,
    };

    vkCmdCopyBufferToImage(Renderer->ImmediateMode.CommandBuffer, Buffer.Handle, DepthImage.Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Copy);

    Rr_ChainImageBarrier_Aspect(
        &Transition,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);
    Rr_EndImmediate(Renderer);

    Rr_DestroyBuffer(Renderer, &Buffer);

    FreeEXRHeader(&Header);
    FreeEXRImage(&Image);

    return DepthImage;
}

Rr_Image Rr_CreateColorAttachmentImage(const Rr_Renderer* Renderer, const VkExtent3D Extent)
{
    const VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    const Rr_Image Image = Rr_CreateImage_Internal(
        Renderer,
        Extent,
        RR_COLOR_FORMAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        AllocationCreateInfo,
        false);

    return Image;
}

Rr_Image Rr_CreateDepthAttachmentImage(const Rr_Renderer* Renderer, const VkExtent3D Extent)
{
    const VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    const Rr_Image Image = Rr_CreateImage_Internal(
        Renderer,
        Extent,
        RR_DEPTH_FORMAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        AllocationCreateInfo,
        false);

    return Image;
}

void Rr_DestroyImage(const Rr_Renderer* const Renderer, const Rr_Image* AllocatedImage)
{
    if (AllocatedImage->Handle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyImageView(Renderer->Device, AllocatedImage->View, NULL);
    vmaDestroyImage(Renderer->Allocator, AllocatedImage->Handle, AllocatedImage->Allocation);
}

void Rr_ChainImageBarrier_Aspect(
    Rr_ImageBarrier* TransitionImage,
    const VkPipelineStageFlags2 DstStageMask,
    const VkAccessFlags2 DstAccessMask,
    const VkImageLayout NewLayout,
    const VkImageAspectFlagBits Aspect)
{
    VkImageMemoryBarrier2 ImageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        .pNext = NULL,

        .srcStageMask = TransitionImage->StageMask,
        .srcAccessMask = TransitionImage->AccessMask,
        .dstStageMask = DstStageMask,
        .dstAccessMask = DstAccessMask,

        .oldLayout = TransitionImage->Layout,
        .newLayout = NewLayout,

        .image = TransitionImage->Image,
        .subresourceRange = GetImageSubresourceRange(Aspect),
    };

    const VkDependencyInfo DepInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &ImageBarrier,
    };

    vkCmdPipelineBarrier2(TransitionImage->CommandBuffer, &DepInfo);

    TransitionImage->Layout = NewLayout;
    TransitionImage->StageMask = DstStageMask;
    TransitionImage->AccessMask = DstAccessMask;
}

void Rr_ChainImageBarrier(

    Rr_ImageBarrier* TransitionImage,
    const VkPipelineStageFlags2 DstStageMask,
    const VkAccessFlags2 DstAccessMask,
    const VkImageLayout NewLayout)
{
    Rr_ChainImageBarrier_Aspect(
        TransitionImage,
        DstStageMask,
        DstAccessMask,
        NewLayout,
        (NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);
}
