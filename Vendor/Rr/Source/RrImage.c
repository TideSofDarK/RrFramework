#include "RrImage.h"

#include <stb/stb_image.h>

#include "RrRenderer.h"
#include "RrAsset.h"
#include "RrTypes.h"
#include "RrHelpers.h"
#include "RrVulkan.h"
#include "RrBuffer.h"
#include "RrLib.h"

SAllocatedImage Rr_CreateImage(SRr* const Rr, VkExtent3D Extent, VkFormat Format, VkImageUsageFlags Usage, b8 bMipMapped)
{
    SAllocatedImage Image = { 0 };
    Image.Format = Format;
    Image.Extent = Extent;

    VkImageCreateInfo Info = GetImageCreateInfo(Image.Format, Usage, Image.Extent);

    if (bMipMapped)
    {
        Info.mipLevels = (u32)(SDL_floor(SDL_log(SDL_max(Extent.width, Extent.height)))) + 1;
    }

    VmaAllocationCreateInfo AllocationInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    vmaCreateImage(Rr->Allocator, &Info, &AllocationInfo, &Image.Handle, &Image.Allocation, NULL);

    VkImageAspectFlags AspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (Format == RR_DEPTH_FORMAT)
    {
        AspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo ViewInfo = GetImageViewCreateInfo(Image.Format, Image.Handle, AspectFlag);
    ViewInfo.subresourceRange.levelCount = Info.mipLevels;

    vkCreateImageView(Rr->Device, &ViewInfo, NULL, &Image.View);

    return Image;
}

SAllocatedImage Rr_LoadImageRGBA8(SRrAsset* Asset, SRr* const Rr, VkImageUsageFlags Usage, b8 bMipMapped, VkImageLayout InitialLayout)
{
    const i32 DesiredChannels = 4;
    i32 Channels;
    VkExtent3D Extent = {.depth=1};
    stbi_uc* ParsedImage = stbi_load_from_memory((stbi_uc*)Asset->Data, (i32)Asset->Length, (i32*)&Extent.width, (i32*)&Extent.height, &Channels, DesiredChannels);

    size_t DataSize = Extent.width * Extent.height * DesiredChannels;

    SAllocatedBuffer Buffer = { 0 };
    AllocatedBuffer_Init(&Buffer, Rr->Allocator, DataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO, true);

    SDL_memcpy(Buffer.AllocationInfo.pMappedData, ParsedImage, DataSize);
    stbi_image_free(ParsedImage);

    SAllocatedImage Image = Rr_CreateImage(Rr, Extent, RR_COLOR_FORMAT, Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, bMipMapped);

    Rr_BeginImmediate(Rr);
    STransitionImage Transition = {
        .Image = Image.Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .AccessMask = VK_ACCESS_2_NONE,
        .CommandBuffer = Rr->ImmediateMode.CommandBuffer,
    };
    TransitionImage_To(&Transition, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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

    vkCmdCopyBufferToImage(Rr->ImmediateMode.CommandBuffer, Buffer.Handle, Image.Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Copy);

    TransitionImage_To(&Transition, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT, InitialLayout);
    Rr_EndImmediate(Rr);

    AllocatedBuffer_Cleanup(&Buffer, Rr->Allocator);

    return Image;
}

void Rr_DestroyImage(SRr* const Rr, SAllocatedImage* AllocatedImage)
{
    if (AllocatedImage->Handle == VK_NULL_HANDLE)
    {
        return;
    }
    vkDestroyImageView(Rr->Device, AllocatedImage->View, NULL);
    vmaDestroyImage(Rr->Allocator, AllocatedImage->Handle, AllocatedImage->Allocation);
}
