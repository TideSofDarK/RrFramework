#include "Rr_Image.h"

#include "Rr_App.h"
#include "Rr_Log.h"
#include "Rr_UploadContext.h"

#include <stb/stb_image.h>

#include <tinyexr/tinyexr.h>

#include <assert.h>

Rr_Sampler *Rr_CreateSampler(Rr_Renderer *Renderer, Rr_SamplerInfo *Info)
{
    assert(Info != NULL);

    Rr_Device *Device = &Renderer->Device;

    Rr_Sampler *Sampler =
        RR_GET_FREE_LIST_ITEM(&Renderer->Samplers, Renderer->Arena);

    VkSamplerCreateInfo SamplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
        .magFilter = Rr_GetVulkanFilter(Info->MagFilter),
        .minFilter = Rr_GetVulkanFilter(Info->MinFilter),
        .mipmapMode = Rr_GetVulkanSamplerMipmapMode(Info->MipmapMode),
        .addressModeU = Rr_GetVulkanSamplerAddressMode(Info->AddressModeU),
        .addressModeV = Rr_GetVulkanSamplerAddressMode(Info->AddressModeV),
        .addressModeW = Rr_GetVulkanSamplerAddressMode(Info->AddressModeW),
        .mipLodBias = Info->MipLodBias,
        .anisotropyEnable = Info->AnisotropyEnable,
        .maxAnisotropy = Info->MaxAnisotropy,
        .compareEnable = Info->CompareEnable,
        .compareOp = Rr_GetVulkanCompareOp(Info->CompareOp),
        .minLod = Info->MinLod,
        .maxLod = Info->MaxLod,
        .borderColor = Rr_GetVulkanBorderColor(Info->BorderColor),
        .unnormalizedCoordinates = Info->UnnormalizedCoordinates,
    };

    Device->CreateSampler(Device->Handle, &SamplerInfo, NULL, &Sampler->Handle);

    return Sampler;
}

void Rr_DestroySampler(Rr_Renderer *Renderer, Rr_Sampler *Sampler)
{
    assert(Sampler != NULL && Sampler->Handle != VK_NULL_HANDLE);

    Rr_Device *Device = &Renderer->Device;

    Device->DestroySampler(Device->Handle, Sampler->Handle, NULL);

    RR_RETURN_FREE_LIST_ITEM(&Renderer->Samplers, Sampler);
}

void Rr_UploadStagingImage(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    Rr_Image *Image,
    VkImageAspectFlags Aspect,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Buffer *StagingBuffer,
    size_t StagingOffset,
    size_t StagingSize)
{
    Rr_Device *Device = &Renderer->Device;

    VkCommandBuffer CommandBuffer = UploadContext->CommandBuffer;

    Rr_AllocatedBuffer *AllocatedStagingBuffer =
        StagingBuffer->AllocatedBuffers;

    for(size_t AllocatedIndex = 0; AllocatedIndex < Image->AllocatedImageCount;
        ++AllocatedIndex)
    {
        Rr_AllocatedImage *AllocatedImage =
            Image->AllocatedImages + AllocatedIndex;

        VkImageSubresourceRange SubresourceRange = (VkImageSubresourceRange){
            .aspectMask = Aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        };

        Device->CmdPipelineBarrier(
            CommandBuffer,
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
                .image = AllocatedImage->Handle,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            });

        VkBufferImageCopy BufferImageCopy = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = Aspect,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageExtent = Image->Extent,
        };

        Device->CmdCopyBufferToImage(
            CommandBuffer,
            AllocatedStagingBuffer->Handle,
            AllocatedImage->Handle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &BufferImageCopy);

        Device->CmdPipelineBarrier(
            CommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            DstState.StageMask,
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &(VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = AllocatedImage->Handle,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = DstState.Specific.Layout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = DstState.AccessMask,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            });

        if(UploadContext->UseAcquireBarriers)
        {
            *RR_PUSH_SLICE(
                &UploadContext->ReleaseImageMemoryBarriers,
                UploadContext->Arena) = (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = AllocatedImage->Handle,
                .oldLayout = DstState.Specific.Layout,
                .newLayout = DstState.Specific.Layout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = DstState.AccessMask,
                .dstAccessMask = 0,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
            };

            *RR_PUSH_SLICE(
                &UploadContext->AcquireImageMemoryBarriers,
                UploadContext->Arena) = (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = AllocatedImage->Handle,
                .oldLayout = DstState.Specific.Layout,
                .newLayout = DstState.Specific.Layout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = 0,
                .dstAccessMask = DstState.AccessMask,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
            };
        }
    }
}

void Rr_UploadImage(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    Rr_Image *Image,
    VkImageAspectFlags Aspect,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Data Data)
{
    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        Renderer,
        Data.Size,
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    *RR_PUSH_SLICE(&UploadContext->StagingBuffers, UploadContext->Arena) =
        StagingBuffer;

    Rr_AllocatedBuffer *AllocatedStagingBuffer =
        StagingBuffer->AllocatedBuffers;
    memcpy(
        AllocatedStagingBuffer->AllocationInfo.pMappedData,
        Data.Pointer,
        Data.Size);

    Rr_UploadStagingImage(
        Renderer,
        UploadContext,
        Image,
        Aspect,
        SrcState,
        DstState,
        StagingBuffer,
        0,
        0);
}

Rr_Image *Rr_CreateImage(
    Rr_Renderer *Renderer,
    Rr_IntVec3 Extent,
    Rr_TextureFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);
    assert(Extent.Depth >= 1);

    Rr_Device *Device = &Renderer->Device;

    Rr_Image *Image = RR_GET_FREE_LIST_ITEM(&Renderer->Images, Renderer->Arena);
    Image->Flags = Flags;
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
    if(RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_MIP_MAPPED_BIT))
    {
        MipLevels =
            (uint32_t)floorf(logf(RR_MAX(Extent.Width, Extent.Height))) + 1;
    }

    Image->AllocatedImageCount = 1;
    if(RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_PER_FRAME_BIT) ||
       RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_READBACK_BIT))
    {
        Image->AllocatedImageCount = RR_FRAME_OVERLAP;
    }

    VkImageUsageFlags UsageFlags = 0;
    if(RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_STORAGE_BIT))
    {
        UsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if(RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_SAMPLED_BIT))
    {
        UsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if(RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT))
    {
        UsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if(RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        UsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if(RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_TRANSFER_BIT))
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

    Image->AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if(Image->Format == VK_FORMAT_D16_UNORM_S8_UINT ||
       Image->Format == VK_FORMAT_D24_UNORM_S8_UINT ||
       Image->Format == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        Image->AspectFlags =
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else if(
        Image->Format == VK_FORMAT_D16_UNORM ||
        Image->Format == VK_FORMAT_D32_SFLOAT ||
        Image->Format == VK_FORMAT_X8_D24_UNORM_PACK32)
    {
        Image->AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

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

        VkImageViewCreateInfo ImageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .image = AllocatedImage->Handle,
            .viewType = ImageViewType,
            .format = Image->Format,
            .subresourceRange = {
                .aspectMask = Image->AspectFlags,
                .baseMipLevel = 0,
                .layerCount = MipLevels,
                .baseArrayLayer = 0,
                .levelCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };

        Device->CreateImageView(
            Device->Handle,
            &ImageViewCreateInfo,
            NULL,
            &AllocatedImage->View);
    }

    return Image;
}

void Rr_DestroyImage(Rr_Renderer *Renderer, Rr_Image *Image)
{
    if(Image == NULL)
    {
        return;
    }

    Rr_Device *Device = &Renderer->Device;

    for(size_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Device->DestroyImageView(
            Device->Handle,
            Image->AllocatedImages[Index].View,
            NULL);
        vmaDestroyImage(
            Renderer->Allocator,
            Image->AllocatedImages[Index].Handle,
            Image->AllocatedImages[Index].Allocation);
    }

    RR_RETURN_FREE_LIST_ITEM(&Renderer->Images, Image);
}

Rr_IntVec3 Rr_GetImageExtent3D(Rr_Image *Image)
{
    return (Rr_IntVec3){
        .Width = Image->Extent.width,
        .Height = Image->Extent.height,
        .Depth = Image->Extent.depth,
    };
}

Rr_IntVec2 Rr_GetImageExtent2D(Rr_Image *Image)
{
    return (Rr_IntVec2){
        .Width = Image->Extent.width,
        .Height = Image->Extent.height,
    };
}

size_t Rr_GetImagePNGRGBA8Size(size_t DataSize, char *Data, Rr_Arena *Arena)
{
    int32_t DesiredChannels = 4;
    int32_t Width;
    int32_t Height;
    int32_t Channels;
    stbi_info_from_memory(
        (stbi_uc *)Data,
        (int32_t)DataSize,
        &Width,
        &Height,
        &Channels);

    return Width * Height * DesiredChannels;
}

Rr_Image *Rr_CreateImageRGBA8(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    char *Data,
    uint32_t Width,
    uint32_t Height)
{
    int32_t DesiredChannels = 4;
    Rr_IntVec3 Extent = { .Width = Width, .Height = Height, .Depth = 1 };
    size_t DataSize = Extent.Width * Extent.Height * DesiredChannels;

    Rr_Image *ColorImage = Rr_CreateImage(
        Renderer,
        Extent,
        RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
        RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);

    Rr_UploadImage(
        Renderer,
        UploadContext,
        ColorImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        },
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .Specific.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        RR_MAKE_DATA(DataSize, Data));

    return ColorImage;
}

Rr_Image *Rr_CreateImageRGBA8FromPNG(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    size_t DataSize,
    char *Data)
{
    int32_t DesiredChannels = 4;
    int32_t Channels;
    Rr_IntVec3 Extent = { .Depth = 1 };
    stbi_uc *ParsedData = stbi_load_from_memory(
        (stbi_uc *)Data,
        (int32_t)DataSize,
        (int32_t *)&Extent.Width,
        (int32_t *)&Extent.Height,
        &Channels,
        DesiredChannels);
    size_t ParsedSize = Extent.Width * Extent.Height * DesiredChannels;

    Rr_Image *ColorImage = Rr_CreateImage(
        Renderer,
        Extent,
        RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
        RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);

    Rr_UploadImage(
        Renderer,
        UploadContext,
        ColorImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        },
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .Specific.Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        RR_MAKE_DATA(ParsedSize, ParsedData));

    stbi_image_free(ParsedData);

    return ColorImage;
}

// Rr_Image *Rr_CreateDepthImageFromEXR(
//     Rr_App *App,
//     Rr_UploadContext *UploadContext,
//     Rr_AssetRef AssetRef,
//     Rr_Arena *Arena)
// {
//     Rr_Asset Asset = Rr_LoadAsset(AssetRef);
//
//     const char *Error;
//
//     EXRVersion Version;
//     int32_t Result = ParseEXRVersionFromMemory(&Version, (unsigned char
//     *)Asset.Pointer, Asset.Size); if(Result != 0)
//     {
//         RR_ABORT("Error opening EXR file!");
//     }
//
//     EXRHeader Header;
//     Result = ParseEXRHeaderFromMemory(&Header, &Version, (unsigned char
//     *)Asset.Pointer, Asset.Size, &Error); if(Result != 0)
//     {
//         RR_ABORT("Error opening EXR file: %s", Error);
//         // FreeEXRErrorMessage(Error);
//     }
//
//     EXRImage Image;
//     InitEXRImage(&Image);
//
//     Result = LoadEXRImageFromMemory(&Image, &Header, (unsigned char
//     *)Asset.Pointer, Asset.Size, &Error); if(Result != 0)
//     {
//         RR_ABORT("Error opening EXR file: %s", Error);
//         // FreeEXRHeader(&Header);
//         // FreeEXRErrorMessage(Error);
//     }
//
//     /* Calculate depth (https://en.wikipedia.org/wiki/Z-buffering) */
//
//     float Near = 0.5f;
//     float Far = 50.0f;
//     float FarPlusNear = Far + Near;
//     float FarMinusNear = Far - Near;
//     float FTimesNear = Far * Near;
//     for(int32_t Index = 0; Index < Image.width * Image.height; Index++)
//     {
//         float *Current = (float *)Image.images[0] + Index;
//         float ZReciprocal = 1.0f / *Current;
//         float Depth = FarPlusNear / FarMinusNear + ZReciprocal * ((-2.0f *
//         FTimesNear) / (FarMinusNear)); Depth = (Depth + 1.0f) / 2.0f;
//         if(Depth > 1.0f)
//         {
//             Depth = 1.0f;
//         }
//         *Current = Depth;
//     }
//
//     Rr_IntVec3 Extent = {
//         .Width = Image.width,
//         .Height = Image.height,
//         .Depth = 1,
//     };
//
//     // size_t DataSize = Extent.Width * Extent.Height * sizeof(float);
//
//     Rr_Image *DepthImage = Rr_CreateImage(
//         App,
//         Extent,
//         RR_TEXTURE_FORMAT_D32_SFLOAT,
//         RR_IMAGE_USAGE_SAMPLED | RR_IMAGE_USAGE_TRANSFER,
//         false);
//
//     // Rr_UploadImage(
//     //     App,
//     //     UploadContext,
//     //     DepthImage->AllocatedImages[0].Handle,
//     //     Rr_GetVulkanExtent3D(&Extent),
//     //     VK_IMAGE_ASPECT_DEPTH_BIT,
//     //     VK_PIPELINE_STAGE_TRANSFER_BIT,
//     //     VK_ACCESS_TRANSFER_READ_BIT,
//     //     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//     //     Image.images[0],
//     //     DataSize);
//
//     FreeEXRHeader(&Header);
//     FreeEXRImage(&Image);
//
//     return DepthImage;
// }

// Rr_Image *Rr_GetDummyColorTexture(Rr_App *App)
// {
//     return App->Renderer.NullTextures.White;
// }
//
// Rr_Image *Rr_GetDummyNormalTexture(Rr_App *App)
// {
//     return App->Renderer.NullTextures.Normal;
// }

Rr_AllocatedImage *Rr_GetCurrentAllocatedImage(
    Rr_Renderer *Renderer,
    Rr_Image *Image)
{
    size_t AllocatedImageIndex =
        Renderer->CurrentFrameIndex % Image->AllocatedImageCount;
    return &Image->AllocatedImages[AllocatedImageIndex];
}
