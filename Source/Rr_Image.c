#include "Rr_Image.h"

#include "Rr_UploadContext.h"
#include "Rr_App.h"

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

    Rr_Image* Image = Rr_CreateObject(&App->ObjectStorage);
    Image->Format = Format;
    Image->Extent = Extent;

    VkImageCreateInfo Info = GetImageCreateInfo(Image->Format, Usage, Image->Extent);

    if (bMipMapped)
    {
        Info.mipLevels = (u32)(floorf(logf(SDL_max(Extent.width, Extent.height)))) + 1;
    }

    VmaAllocationCreateInfo AllocationCreateInfo = {
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

void Rr_DestroyImage(Rr_App* App, Rr_Image* AllocatedImage)
{
    if (AllocatedImage == NULL)
    {
        return;
    }

    Rr_Renderer* Renderer = &App->Renderer;

    vkDestroyImageView(Renderer->Device, AllocatedImage->View, NULL);
    vmaDestroyImage(Renderer->Allocator, AllocatedImage->Handle, AllocatedImage->Allocation);

    Rr_DestroyObject(&App->ObjectStorage, AllocatedImage);
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
    void* ImageData,
    size_t ImageDataLength)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_WriteBuffer* StagingBuffer = &UploadContext->StagingBuffer;
    VkCommandBuffer TransferCommandBuffer = UploadContext->TransferCommandBuffer;

    size_t BufferOffset = StagingBuffer->Offset;
    memcpy((char*)StagingBuffer->Buffer->AllocationInfo.pMappedData + BufferOffset, ImageData, ImageDataLength);
    StagingBuffer->Offset += ImageDataLength;

    VkImageSubresourceRange SubresourceRange = GetImageSubresourceRange(Aspect);

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

    VkBufferImageCopy Copy = {
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

    if (UploadContext->bUseAcquireBarriers)
    {
        UploadContext->ReleaseBarriers.ImageMemoryBarriers[UploadContext->AcquireBarriers.ImageMemoryBarrierCount] =
            (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = Image,
                .oldLayout = DstLayout,
                .newLayout = DstLayout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = DstAccessMask,
                .dstAccessMask = 0,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex
            };
        UploadContext->ReleaseBarriers.ImageMemoryBarrierCount++;

        UploadContext->AcquireBarriers.ImageMemoryBarriers[UploadContext->AcquireBarriers.ImageMemoryBarrierCount] =
            (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = Image,
                .oldLayout = DstLayout,
                .newLayout = DstLayout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = 0,
                .dstAccessMask = DstAccessMask,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex
            };
        UploadContext->AcquireBarriers.ImageMemoryBarrierCount++;
    }
}

void Rr_GetImageSizePNGMemory(
    byte* Data,
    usize DataSize,
    Rr_Arena* Arena,
    struct Rr_LoadSize* OutLoadSize)
{
    i32 DesiredChannels = 4;
    i32 Width;
    i32 Height;
    i32 Channels;
    stbi_info_from_memory((stbi_uc*)Data, (i32)DataSize, &Width, &Height, &Channels);

    OutLoadSize->StagingBufferSize += Width * Height * DesiredChannels;
    OutLoadSize->ImageCount += 1;
}

void Rr_GetImageSizePNG(Rr_AssetRef AssetRef, Rr_Arena* Arena, Rr_LoadSize* OutLoadSize)
{
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    Rr_GetImageSizePNGMemory(Asset.Data, Asset.Length, Arena, OutLoadSize);
}

void Rr_GetImageSizeEXR(Rr_AssetRef AssetRef, Rr_Arena* Arena, Rr_LoadSize* OutLoadSize)
{
    OutLoadSize->StagingBufferSize += 0;
    OutLoadSize->ImageCount += 1;
}

Rr_Image* Rr_CreateColorImageFromPNGMemory(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    byte* Data,
    usize DataSize,
    bool bMipMapped)
{
    i32 DesiredChannels = 4;
    i32 Channels;
    VkExtent3D Extent = { .depth = 1 };
    stbi_uc* ParsedImage = stbi_load_from_memory(
        (stbi_uc*)Data,
        (i32)DataSize,
        (i32*)&Extent.width,
        (i32*)&Extent.height,
        &Channels,
        DesiredChannels);
    size_t ParsedSize = Extent.width * Extent.height * DesiredChannels;

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
        ParsedSize);

    stbi_image_free(ParsedImage);

    return ColorImage;
}

Rr_Image* Rr_CreateColorImageFromPNG(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_AssetRef AssetRef,
    bool bMipMapped,
    Rr_Arena* Arena)
{
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    return Rr_CreateColorImageFromPNGMemory(
        App,
        UploadContext,
        Asset.Data,
        Asset.Length,
        bMipMapped);
}

Rr_Image* Rr_CreateDepthImageFromEXR(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Arena* Arena)
{
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    VkImageUsageFlags Usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    int Result;
    str Error;

    EXRVersion Version;
    Result = ParseEXRVersionFromMemory(&Version, (unsigned char*)Asset.Data, Asset.Length);
    if (Result != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error opening EXR file!");
        abort();
    }

    EXRHeader Header;
    Result = ParseEXRHeaderFromMemory(&Header, &Version, (unsigned char*)Asset.Data, Asset.Length, &Error);
    if (Result != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error opening EXR file: %s", Error);
        FreeEXRErrorMessage(Error);
        abort();
    }

    EXRImage Image;
    InitEXRImage(&Image);

    Result = LoadEXRImageFromMemory(&Image, &Header, (unsigned char*)Asset.Data, Asset.Length, &Error);
    if (Result != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Error opening EXR file: %s", Error);
        FreeEXRHeader(&Header);
        FreeEXRErrorMessage(Error);
        abort();
    }

    /* Calculate depth (https://en.wikipedia.org/wiki/Z-buffering) */
    float Near = 0.5f;
    float Far = 50.0f;
    float FarPlusNear = Far + Near;
    float FarMinusNear = Far - Near;
    float FTimesNear = Far * Near;
    for (int Index = 0; Index < Image.width * Image.height; Index++)
    {
        float* Current = ((float*)Image.images[0]) + Index;
        float ZReciprocal = 1.0f / *Current;
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
