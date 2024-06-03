#include "Rr_ImageTools.h"

#include "Rr_Vulkan.h"
#include "Rr_Buffer.h"
#include "Rr_Types.h"
#include "Rr_Object.h"
#include "Rr_Asset.h"

#include <SDL3/SDL.h>

#include <stb/stb_image.h>

#include <tinyexr/tinyexr.h>

Rr_Image* Rr_CreateImage(
    Rr_App* App,
    VkExtent3D Extent,
    VkFormat Format,
    VkImageUsageFlags Usage,
    bool bMipMapped)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_Image* Image = Rr_CreateObject(Renderer);
    Image->Format = Format;
    Image->Extent = Extent;

    VkImageCreateInfo Info = GetImageCreateInfo(Image->Format, Usage, Image->Extent);

    if (bMipMapped)
    {
        Info.mipLevels = (u32)(SDL_floor(SDL_log(SDL_max(Extent.width, Extent.height)))) + 1;
    }

    const VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    vmaCreateImage(Renderer->Allocator, &Info, &AllocationCreateInfo, &Image->Handle, &Image->Allocation, NULL);

    VkImageAspectFlags AspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (Format == RR_DEPTH_FORMAT)
    {
        AspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo ViewInfo = GetImageViewCreateInfo(Image->Format, Image->Handle, AspectFlag);
    ViewInfo.subresourceRange.levelCount = Info.mipLevels;

    vkCreateImageView(Renderer->Device, &ViewInfo, NULL, &Image->View);

    return Image;
}

static void UploadImage(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    VkImage Image,
    VkExtent3D Extent,
    VkImageAspectFlags Aspect,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout DstLayout,
    const void* ImageData,
    const size_t ImageDataLength)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_WriteBuffer* StagingBuffer = &UploadContext->StagingBuffer;
    VkCommandBuffer TransferCommandBuffer = UploadContext->TransferCommandBuffer;

    const size_t BufferOffset = StagingBuffer->Offset;
    SDL_memcpy((char*)StagingBuffer->Buffer->AllocationInfo.pMappedData + BufferOffset, ImageData, ImageDataLength);
    StagingBuffer->Offset += ImageDataLength;

    const VkImageSubresourceRange SubresourceRange = GetImageSubresourceRange(Aspect);

    vkCmdPipelineBarrier(
        TransferCommandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &(VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .image = Image,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .subresourceRange = SubresourceRange,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        });

    const VkBufferImageCopy Copy = {
        .bufferOffset = BufferOffset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = Aspect,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageExtent = Extent,
    };

    vkCmdCopyBufferToImage(TransferCommandBuffer, StagingBuffer->Buffer->Handle, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Copy);

    if (!UploadContext->bUseAcquireBarriers)
    {
        vkCmdPipelineBarrier(
            TransferCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            DstStageMask,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &(VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = Image,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = DstLayout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = DstAccessMask,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            });
    }
    else
    {
        UploadContext->ReleaseBarriers.ImageMemoryBarriers[UploadContext->AcquireBarriers.ImageMemoryBarrierCount] =
            (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = Image,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = DstLayout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = 0,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->UnifiedQueue.FamilyIndex
            };
        UploadContext->ReleaseBarriers.ImageMemoryBarrierCount++;

        UploadContext->AcquireBarriers.ImageMemoryBarriers[UploadContext->AcquireBarriers.ImageMemoryBarrierCount] =
            (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = Image,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = DstLayout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = 0,
                .dstAccessMask = DstAccessMask,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->UnifiedQueue.FamilyIndex
            };
        UploadContext->AcquireBarriers.ImageMemoryBarrierCount++;
    }
}

size_t Rr_GetImageSizePNG(const Rr_Asset* Asset, Rr_Arena* Arena)
{
    const i32 DesiredChannels = 4;
    i32 Width;
    i32 Height;
    i32 Channels;
    stbi_info_from_memory((stbi_uc*)Asset->Data, (i32)Asset->Length, &Width, &Height, &Channels);

    return Width * Height * DesiredChannels;
}

size_t Rr_GetImageSizeEXR(const Rr_Asset* Asset, Rr_Arena* Arena)
{
    return 0;
}

Rr_Image* Rr_CreateColorImageFromPNG(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_Asset* Asset,
    bool bMipMapped,
    Rr_Arena* Arena)
{
    const i32 DesiredChannels = 4;
    i32 Channels;
    VkExtent3D Extent = { .depth = 1 };
    stbi_uc* ParsedImage = stbi_load_from_memory((stbi_uc*)Asset->Data, (i32)Asset->Length, (i32*)&Extent.width, (i32*)&Extent.height, &Channels, DesiredChannels);
    const size_t DataSize = Extent.width * Extent.height * DesiredChannels;

    Rr_Image* ColorImage = Rr_CreateImage(
        App,
        Extent,
        RR_COLOR_FORMAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        bMipMapped);

    UploadImage(App,
        UploadContext,
        ColorImage->Handle,
        Extent,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        ParsedImage,
        DataSize);

    stbi_image_free(ParsedImage);

    return ColorImage;
}

Rr_Image* Rr_CreateDepthImageFromEXR(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_Asset* Asset,
    Rr_Arena* Arena)
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

    const VkExtent3D Extent = { .width = Image.width, .height = Image.height, .depth = 1 };

    const size_t DataSize = Extent.width * Extent.height * sizeof(f32);

    Rr_Image* DepthImage = Rr_CreateImage(App, Extent, RR_PRERENDERED_DEPTH_FORMAT, Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);

    UploadImage(App,
        UploadContext,
        DepthImage->Handle,
        Extent,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        Image.images[0],
        DataSize);

    FreeEXRHeader(&Header);
    FreeEXRImage(&Image);

    return DepthImage;
}

Rr_Image* Rr_CreateColorAttachmentImage(Rr_App* App, u32 Width, u32 Height)
{
    return Rr_CreateImage(
        App,
        (VkExtent3D){ Width, Height, 1 },
        RR_COLOR_FORMAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        false);
}

Rr_Image* Rr_CreateDepthAttachmentImage(Rr_App* App, u32 Width, u32 Height)
{
    return Rr_CreateImage(
        App,
        (VkExtent3D){ Width, Height, 1 },
        RR_DEPTH_FORMAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        false);
}

