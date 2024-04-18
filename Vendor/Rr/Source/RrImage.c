#include "RrImage.h"

#include "RrMemory.h"

#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC Rr_Malloc
#define STBI_REALLOC Rr_Realloc
#define STBI_FREE Rr_Free
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <tinyexr/tinyexr.h>

#include "RrRenderer.h"
#include "RrAsset.h"
#include "RrTypes.h"
#include "RrHelpers.h"
#include "RrVulkan.h"
#include "RrBuffer.h"
#include "RrLib.h"

static Rr_Image Rr_CreateImage_Internal(Rr_Renderer* const Renderer, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, VmaAllocationCreateInfo AllocationCreateInfo, b8 bMipMapped)
{
    Rr_Image Image = { 0 };
    Image.Format = Format;
    Image.Extent = Extent;

    VkImageCreateInfo Info = GetImageCreateInfo(Image.Format, Usage, Image.Extent);

    if (bMipMapped)
    {
        Info.mipLevels = (u32)(SDL_floor(SDL_log(SDL_max(Extent.width, Extent.height)))) + 1;
    }

    VkResult Result = vmaCreateImage(Renderer->Allocator, &Info, &AllocationCreateInfo, &Image.Handle, &Image.Allocation, NULL);

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

Rr_Image Rr_CreateImage(Rr_Renderer* const Renderer, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped)
{
    VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    return Rr_CreateImage_Internal(Renderer, Extent, Format, Usage, AllocationCreateInfo, bMipMapped);
}

Rr_Image Rr_CreateImageFromPNG(Rr_Asset* Asset, Rr_Renderer* const Renderer, VkImageUsageFlags Usage, b8 bMipMapped, VkImageLayout InitialLayout)
{
    const i32 DesiredChannels = 4;
    i32 Channels;
    VkExtent3D Extent = { .depth = 1 };
    stbi_uc* ParsedImage = stbi_load_from_memory((stbi_uc*)Asset->Data, (i32)Asset->Length, (i32*)&Extent.width, (i32*)&Extent.height, &Channels, DesiredChannels);

    size_t DataSize = Extent.width * Extent.height * DesiredChannels;

    Rr_Buffer Buffer = Rr_CreateBuffer(Renderer->Allocator, DataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true);

    SDL_memcpy(Buffer.AllocationInfo.pMappedData, ParsedImage, DataSize);
    stbi_image_free(ParsedImage);

    Rr_Image Image = Rr_CreateImage(Renderer, Extent, RR_COLOR_FORMAT, Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, bMipMapped);

    Rr_BeginImmediate(Renderer);
    Rr_ImageBarrier Transition = {
        .Image = Image.Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .AccessMask = VK_ACCESS_2_NONE,
        .CommandBuffer = Renderer->ImmediateMode.CommandBuffer,
    };
    Rr_ChainImageBarrier(&Transition, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy Copy = {
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

    Rr_DestroyBuffer(&Buffer, Renderer->Allocator);

    return Image;
}

Rr_Image Rr_CreateImageFromEXR(Rr_Asset* Asset, Rr_Renderer* const Renderer)
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
        float* const Current = ((float*)Image.images[1]) + Index;
        const float ZReciprocal = 1.0f / *Current;
        const float Depth = FarPlusNear / FarMinusNear + ZReciprocal * ((-2.0f * FTimesNear) / (FarMinusNear));
        *Current = (Depth + 1.0f) / 2.0f;
    }

    VkExtent3D Extent = { .width = Image.width, .height = Image.height, .depth = 1 };

    size_t DataSize = Extent.width * Extent.height * sizeof(f32);

    Rr_Buffer Buffer = Rr_CreateBuffer(Renderer->Allocator, DataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true);

    SDL_memcpy(Buffer.AllocationInfo.pMappedData, Image.images[1], DataSize);

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

    Rr_DestroyBuffer(&Buffer, Renderer->Allocator);

    FreeEXRHeader(&Header);
    FreeEXRImage(&Image);

    return DepthImage;
}

Rr_Image Rr_CreateColorAttachmentImage(Rr_Renderer* Renderer, VkExtent3D Extent)
{
    VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    Rr_Image Image = Rr_CreateImage_Internal(Renderer, Extent, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, AllocationCreateInfo, false);

    //    Rr_BeginImmediate(Renderer);
    //    Rr_ImageBarrier Transition = {
    //        .Image = DepthImage.Handle,
    //        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
    //        .StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    //        .AccessMask = VK_ACCESS_2_NONE,
    //        .CommandBuffer = Renderer->ImmediateMode.CommandBuffer,
    //    };
    //    Rr_ChainImageBarrier_Aspect(
    //        &Transition,
    //        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    //        VK_ACCESS_2_TRANSFER_WRITE_BIT,
    //        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //        VK_IMAGE_ASPECT_DEPTH_BIT);
    //    Rr_EndImmediate(Renderer);

    return Image;
}

void Rr_DestroyImage(Rr_Renderer* const Renderer, Rr_Image* AllocatedImage)
{
    if (AllocatedImage->Handle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyImageView(Renderer->Device, AllocatedImage->View, NULL);
    vmaDestroyImage(Renderer->Allocator, AllocatedImage->Handle, AllocatedImage->Allocation);
}
