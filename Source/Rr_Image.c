#include "Rr_Image.h"

#include "Rr_App.h"
#include "Rr_Log.h"
#include "Rr_UploadContext.h"

#include <stb/stb_image.h>

#include <tinyexr/tinyexr.h>

#include <assert.h>

static VkExtent3D Rr_GetVulkanExtent3D(Rr_IntVec3 *Extent)
{
    return (VkExtent3D){ .width = Extent->Width, .height = Extent->Height, .depth = Extent->Depth };
}

static void Rr_UploadImage(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    VkImage Image,
    VkExtent3D Extent,
    VkImageAspectFlags Aspect,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout DstLayout,
    void *ImageData,
    size_t ImageDataLength)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_WriteBuffer *StagingBuffer = UploadContext->StagingBuffer;
    VkCommandBuffer TransferCommandBuffer = UploadContext->TransferCommandBuffer;

    VkDeviceSize BufferOffset = StagingBuffer->Offset;
    memcpy((char *)StagingBuffer->Buffer->AllocationInfo.pMappedData + BufferOffset, ImageData, ImageDataLength);
    StagingBuffer->Offset += ImageDataLength;

    VkImageSubresourceRange SubresourceRange = Rr_GetImageSubresourceRange(Aspect);

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

    vkCmdCopyBufferToImage(
        TransferCommandBuffer,
        StagingBuffer->Buffer->Handle,
        Image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &Copy);

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

    if(UploadContext->UseAcquireBarriers)
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
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
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
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
            };
        UploadContext->AcquireBarriers.ImageMemoryBarrierCount++;
    }
}

Rr_Image *Rr_CreateImage(Rr_App *App, Rr_IntVec3 Extent, Rr_TextureFormat Format, Rr_ImageUsage Usage, bool MipMapped)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);
    assert(Extent.Depth >= 1);

    Rr_Renderer *Renderer = &App->Renderer;

    Rr_Image *Image = RR_GET_FREE_LIST_ITEM(&App->Renderer.Images, App->PermanentArena);
    Image->Format = Rr_GetVulkanTextureFormat(Format);
    Image->Extent = *(VkExtent3D *)&Extent;

    VkImageType ImageType = VK_IMAGE_TYPE_3D;
    VkImageViewType ImageViewType = VK_IMAGE_VIEW_TYPE_3D;
    if(Extent.Depth == 1)
    {
        ImageType = VK_IMAGE_TYPE_2D;
        ImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    }
    if(Extent.Height == 1)
    {
        ImageType = VK_IMAGE_TYPE_1D;
        ImageViewType = VK_IMAGE_VIEW_TYPE_1D;
    }

    uint32_t MipLevels = 1;
    if(MipMapped)
    {
        MipLevels = (uint32_t)floorf(logf(RR_MAX(Extent.Width, Extent.Height))) + 1;
    }

    Image->AllocatedImageCount = 1;

    VkImageUsageFlags UsageFlags = 0;
    if((RR_IMAGE_USAGE_STORAGE & Usage) != 0)
    {
        UsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        Image->AllocatedImageCount = RR_FRAME_OVERLAP;
    }
    if((RR_IMAGE_USAGE_SAMPLED & Usage) != 0)
    {
        UsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if((RR_IMAGE_USAGE_COLOR_ATTACHMENT & Usage) != 0)
    {
        UsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        Image->AllocatedImageCount = RR_FRAME_OVERLAP;
    }
    if((RR_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT & Usage) != 0)
    {
        UsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if((RR_IMAGE_USAGE_TRANSFER & Usage) != 0)
    {
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VkImageCreateInfo ImageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = ImageType,
        .format = Image->Format,
        .extent = Image->Extent,
        .mipLevels = MipLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = UsageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkImageAspectFlags AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if(Image->Format == VK_FORMAT_D16_UNORM_S8_UINT || Image->Format == VK_FORMAT_D24_UNORM_S8_UINT ||
       Image->Format == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else if(
        Image->Format == VK_FORMAT_D16_UNORM || Image->Format == VK_FORMAT_D32_SFLOAT ||
        Image->Format == VK_FORMAT_X8_D24_UNORM_PACK32)
    {
        AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VmaAllocationCreateInfo AllocationCreateInfo = { .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                                     .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    for(size_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Rr_AllocatedImage *AllocatedImage = Image->AllocatedImages + Index;
        AllocatedImage->Container = Image;

        vmaCreateImage(
            Renderer->Allocator,
            &ImageCreateInfo,
            &AllocationCreateInfo,
            &AllocatedImage->Handle,
            &AllocatedImage->Allocation,
            NULL);

        VkImageViewCreateInfo ImageViewCreateInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                      .pNext = NULL,
                                                      .image = AllocatedImage->Handle,
                                                      .viewType = ImageViewType,
                                                      .format = Image->Format,
                                                      .subresourceRange = {
                                                          .aspectMask = AspectFlags,
                                                          .baseMipLevel = 0,
                                                          .layerCount = MipLevels,
                                                          .baseArrayLayer = 0,
                                                          .levelCount = 1,
                                                      }, };

        vkCreateImageView(Renderer->Device, &ImageViewCreateInfo, NULL, &AllocatedImage->View);
    }

    return Image;
}

void Rr_DestroyImage(Rr_App *App, Rr_Image *Image)
{
    if(Image == NULL)
    {
        return;
    }

    Rr_Renderer *Renderer = &App->Renderer;

    for(size_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        vkDestroyImageView(Renderer->Device, Image->AllocatedImages[Index].View, NULL);
        vmaDestroyImage(
            Renderer->Allocator,
            Image->AllocatedImages[Index].Handle,
            Image->AllocatedImages[Index].Allocation);
    }

    RR_RETURN_FREE_LIST_ITEM(&App->Renderer.Images, Image);
}

void Rr_GetImageSizePNGMemory(char *Data, size_t DataSize, Rr_Arena *Arena, Rr_LoadSize *OutLoadSize)
{
    int32_t DesiredChannels = 4;
    int32_t Width;
    int32_t Height;
    int32_t Channels;
    stbi_info_from_memory((stbi_uc *)Data, (int32_t)DataSize, &Width, &Height, &Channels);

    OutLoadSize->StagingBufferSize += Width * Height * DesiredChannels;
    OutLoadSize->ImageCount += 1;
}

void Rr_GetImageSizePNG(Rr_AssetRef AssetRef, Rr_Arena *Arena, Rr_LoadSize *OutLoadSize)
{
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    Rr_GetImageSizePNGMemory(Asset.Data, Asset.Size, Arena, OutLoadSize);
}

void Rr_GetImageSizeEXR(Rr_AssetRef AssetRef, Rr_Arena *Arena, Rr_LoadSize *OutLoadSize)
{
    OutLoadSize->StagingBufferSize += 0;
    OutLoadSize->ImageCount += 1;
}

Rr_Image *Rr_CreateColorImageFromMemory(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    char *Data,
    uint32_t Width,
    uint32_t Height,
    bool MipMapped)
{
    int32_t DesiredChannels = 4;
    Rr_IntVec3 Extent = { .Width = Width, .Height = Height, .Depth = 1 };
    size_t DataSize = Extent.Width * Extent.Height * DesiredChannels;

    Rr_Image *ColorImage = Rr_CreateImage(
        App,
        Extent,
        RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
        RR_IMAGE_USAGE_SAMPLED | RR_IMAGE_USAGE_TRANSFER,
        MipMapped);

    Rr_UploadImage(
        App,
        UploadContext,
        ColorImage->AllocatedImages[0].Handle,
        Rr_GetVulkanExtent3D(&Extent),
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        Data,
        DataSize);

    return ColorImage;
}

Rr_Image *Rr_CreateColorImageFromPNGMemory(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    char *Data,
    size_t DataSize,
    bool MipMapped)
{
    int32_t DesiredChannels = 4;
    int32_t Channels;
    Rr_IntVec3 Extent = { .Depth = 1 };
    stbi_uc *ParsedImage = stbi_load_from_memory(
        (stbi_uc *)Data,
        (int32_t)DataSize,
        (int32_t *)&Extent.Width,
        (int32_t *)&Extent.Height,
        &Channels,
        DesiredChannels);
    size_t ParsedSize = Extent.Width * Extent.Height * DesiredChannels;

    Rr_Image *ColorImage = Rr_CreateImage(
        App,
        Extent,
        RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
        RR_IMAGE_USAGE_SAMPLED | RR_IMAGE_USAGE_TRANSFER,
        MipMapped);

    Rr_UploadImage(
        App,
        UploadContext,
        ColorImage->AllocatedImages[0].Handle,
        Rr_GetVulkanExtent3D(&Extent),
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        ParsedImage,
        ParsedSize);

    stbi_image_free(ParsedImage);

    return ColorImage;
}

Rr_Image *Rr_CreateColorImageFromPNG(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    bool MipMapped,
    Rr_Arena *Arena)
{
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    return Rr_CreateColorImageFromPNGMemory(App, UploadContext, Asset.Data, Asset.Size, MipMapped);
}

Rr_Image *Rr_CreateDepthImageFromEXR(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Arena *Arena)
{
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    const char *Error;

    EXRVersion Version;
    int32_t Result = ParseEXRVersionFromMemory(&Version, (unsigned char *)Asset.Data, Asset.Size);
    if(Result != 0)
    {
        Rr_LogAbort("Error opening EXR file!");
    }

    EXRHeader Header;
    Result = ParseEXRHeaderFromMemory(&Header, &Version, (unsigned char *)Asset.Data, Asset.Size, &Error);
    if(Result != 0)
    {
        Rr_LogAbort("Error opening EXR file: %s", Error);
        // FreeEXRErrorMessage(Error);
    }

    EXRImage Image;
    InitEXRImage(&Image);

    Result = LoadEXRImageFromMemory(&Image, &Header, (unsigned char *)Asset.Data, Asset.Size, &Error);
    if(Result != 0)
    {
        Rr_LogAbort("Error opening EXR file: %s", Error);
        // FreeEXRHeader(&Header);
        // FreeEXRErrorMessage(Error);
    }

    /* Calculate depth (https://en.wikipedia.org/wiki/Z-buffering) */
    float Near = 0.5f;
    float Far = 50.0f;
    float FarPlusNear = Far + Near;
    float FarMinusNear = Far - Near;
    float FTimesNear = Far * Near;
    for(int32_t Index = 0; Index < Image.width * Image.height; Index++)
    {
        float *Current = (float *)Image.images[0] + Index;
        float ZReciprocal = 1.0f / *Current;
        float Depth = FarPlusNear / FarMinusNear + ZReciprocal * ((-2.0f * FTimesNear) / (FarMinusNear));
        Depth = (Depth + 1.0f) / 2.0f;
        if(Depth > 1.0f)
        {
            Depth = 1.0f;
        }
        *Current = Depth;
    }

    Rr_IntVec3 Extent = {
        .Width = Image.width,
        .Height = Image.height,
        .Depth = 1,
    };

    size_t DataSize = Extent.Width * Extent.Height * sizeof(float);

    Rr_Image *DepthImage = Rr_CreateImage(
        App,
        Extent,
        RR_TEXTURE_FORMAT_D32_SFLOAT,
        RR_IMAGE_USAGE_SAMPLED | RR_IMAGE_USAGE_TRANSFER,
        false);

    Rr_UploadImage(
        App,
        UploadContext,
        DepthImage->AllocatedImages[0].Handle,
        Rr_GetVulkanExtent3D(&Extent),
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

Rr_Image *Rr_CreateColorAttachmentImage(Rr_App *App, uint32_t Width, uint32_t Height)
{
    return Rr_CreateImage(
        App,
        (Rr_IntVec3){ Width, Height, 1 },
        RR_TEXTURE_FORMAT_B8G8R8A8_UNORM,
        RR_IMAGE_USAGE_COLOR_ATTACHMENT | RR_IMAGE_USAGE_TRANSFER,
        false);
}

Rr_Image *Rr_CreateDepthAttachmentImage(Rr_App *App, uint32_t Width, uint32_t Height)
{
    return Rr_CreateImage(
        App,
        (Rr_IntVec3){ Width, Height, 1 },
        RR_TEXTURE_FORMAT_D32_SFLOAT,
        RR_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT | RR_IMAGE_USAGE_TRANSFER,
        false);
}

Rr_Image *Rr_GetDummyColorTexture(Rr_App *App)
{
    return App->Renderer.NullTextures.White;
}

Rr_Image *Rr_GetDummyNormalTexture(Rr_App *App)
{
    return App->Renderer.NullTextures.Normal;
}

void Rr_ChainImageBarrier_Aspect(
    Rr_ImageBarrier *TransitionImage,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout NewLayout,
    VkImageAspectFlagBits Aspect)
{
    vkCmdPipelineBarrier(
        TransitionImage->CommandBuffer,
        TransitionImage->StageMask,
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
            .image = TransitionImage->Image,
            .oldLayout = TransitionImage->Layout,
            .newLayout = NewLayout,
            .subresourceRange = Rr_GetImageSubresourceRange(Aspect),
            .srcAccessMask = TransitionImage->AccessMask,
            .dstAccessMask = DstAccessMask,
        });

    TransitionImage->Layout = NewLayout;
    TransitionImage->StageMask = DstStageMask;
    TransitionImage->AccessMask = DstAccessMask;
}

void Rr_ChainImageBarrier(
    Rr_ImageBarrier *TransitionImage,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    VkImageLayout NewLayout)
{
    Rr_ChainImageBarrier_Aspect(
        TransitionImage,
        DstStageMask,
        DstAccessMask,
        NewLayout,
        NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                      : VK_IMAGE_ASPECT_COLOR_BIT);
}

Rr_AllocatedImage *Rr_GetCurrentAllocatedImage(Rr_App *App, Rr_Image *Image)
{
    size_t AllocatedImageIndex = App->Renderer.CurrentFrameIndex % Image->AllocatedImageCount;
    return &Image->AllocatedImages[AllocatedImageIndex];
}
