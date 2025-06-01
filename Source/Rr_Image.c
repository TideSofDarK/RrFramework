/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Rr_Image.h"

#include "Rr_Buffer.h"
#include "Rr_Renderer.h"
#include "Rr_UploadContext.h"

#include <stb/stb_image.h>

#include <assert.h>

Rr_Sampler *Rr_CreateSampler(Rr_SamplerInfo *Info)
{
    assert(Info != NULL);

    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_SamplerHiveIterator It =
        Rr_PushSamplerIntoHive(&gRenderer->SamplerHive, gRenderer->Arena);
    Rr_Sampler *Sampler = It.Element;

    Rr_UnlockSpinlock(&gRenderer->Lock);

    VkSamplerCreateInfo SamplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
        .magFilter = Rr_ToVulkanFilter(Info->MagFilter),
        .minFilter = Rr_ToVulkanFilter(Info->MinFilter),
        .mipmapMode = Rr_ToVulkanSamplerMipmapMode(Info->MipmapMode),
        .addressModeU = Rr_ToVulkanSamplerAddressMode(Info->AddressModeU),
        .addressModeV = Rr_ToVulkanSamplerAddressMode(Info->AddressModeV),
        .addressModeW = Rr_ToVulkanSamplerAddressMode(Info->AddressModeW),
        .mipLodBias = Info->MipLodBias,
        .anisotropyEnable = Info->AnisotropyEnable,
        .maxAnisotropy = Info->MaxAnisotropy,
        .compareEnable = Info->CompareEnable,
        .compareOp = Rr_ToVulkanCompareOp(Info->CompareOp),
        .minLod = Info->MinLod,
        .maxLod = Info->MaxLod,
        .borderColor = Rr_ToVulkanBorderColor(Info->BorderColor),
        .unnormalizedCoordinates = Info->UnnormalizedCoordinates,
    };

    Device->CreateSampler(Device->Handle, &SamplerInfo, NULL, &Sampler->Handle);

    return Sampler;
}

void Rr_DestroySampler(Rr_Sampler *Sampler)
{
    assert(Sampler != NULL && Sampler->Handle != VK_NULL_HANDLE);

    Rr_Device *Device = &gRenderer->Device;

    Device->DestroySampler(Device->Handle, Sampler->Handle, NULL);

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_SamplerHiveIterator It = Rr_GetSamplerHiveIterator(&gRenderer->SamplerHive, Sampler);
    Rr_RemoveFromSamplerHive(&gRenderer->SamplerHive, &It);

    Rr_UnlockSpinlock(&gRenderer->Lock);
}

void Rr_UploadStagingImage2D(
    Rr_UploadContext *UploadContext,
    Rr_Image2D *Image,
    VkImageAspectFlags Aspect,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Buffer *StagingBuffer,
    size_t StagingOffset,
    size_t StagingSize)
{
    Rr_Device *Device = &gRenderer->Device;

    VkCommandBuffer CommandBuffer = UploadContext->CommandBuffer;

    Rr_AllocatedBuffer *AllocatedStagingBuffer =
        StagingBuffer->AllocatedBuffers;

    for (size_t AllocatedIndex = 0; AllocatedIndex < Image->AllocatedImageCount;
         ++AllocatedIndex)
    {
        Rr_AllocatedImage *AllocatedImage =
            Image->AllocatedImages + AllocatedIndex;

        VkImageSubresourceRange SubresourceRange = {
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
            .imageExtent = (VkExtent3D){ Image->Extent.width, Image->Extent.height, 1},
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
                .newLayout = DstState.Layout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = DstState.AccessMask,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            });

        if (UploadContext->UseAcquireBarriers)
        {
            *RR_PUSH_INTO_ARRAY(
                &UploadContext->ReleaseImageMemoryBarriers,
                UploadContext->Arena) = (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = AllocatedImage->Handle,
                .oldLayout = DstState.Layout,
                .newLayout = DstState.Layout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = DstState.AccessMask,
                .dstAccessMask = 0,
                .srcQueueFamilyIndex = gRenderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = gRenderer->GraphicsQueue.FamilyIndex,
            };

            *RR_PUSH_INTO_ARRAY(
                &UploadContext->AcquireImageMemoryBarriers,
                UploadContext->Arena) = (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .image = AllocatedImage->Handle,
                .oldLayout = DstState.Layout,
                .newLayout = DstState.Layout,
                .subresourceRange = SubresourceRange,
                .srcAccessMask = 0,
                .dstAccessMask = DstState.AccessMask,
                .srcQueueFamilyIndex = gRenderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = gRenderer->GraphicsQueue.FamilyIndex,
            };
        }
    }
}

void Rr_UploadImage2D(
    Rr_UploadContext *UploadContext,
    Rr_Image2D *Image,
    VkImageAspectFlags Aspect,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Data Data)
{
    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        Data.Size,
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    *RR_PUSH_INTO_ARRAY(&UploadContext->StagingBuffers, UploadContext->Arena) =
        StagingBuffer;

    Rr_AllocatedBuffer *AllocatedStagingBuffer =
        StagingBuffer->AllocatedBuffers;
    memcpy(AllocatedStagingBuffer->MappedData, Data.Pointer, Data.Size);

    Rr_UploadStagingImage2D(
        UploadContext,
        Image,
        Aspect,
        SrcState,
        DstState,
        StagingBuffer,
        0,
        0);
}

static Rr_Image *Rr_CreateImage(
    Rr_IntVec3 Extent,
    Rr_TextureFormat Format,
    Rr_ImageFlags Flags,
    uint32_t LayerCount,
    VkImageCreateFlags AdditionalFlags)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_ImageHiveIterator It =
        Rr_PushImageIntoHive(&gRenderer->ImageHive, gRenderer->Arena);
    Rr_Image *Image = It.Element;

    Rr_UnlockSpinlock(&gRenderer->Lock);

    Image->Flags = Flags;
    Image->Format = Rr_ToVulkanTextureFormat(Format);
    Image->Extent.width = Extent.Width;
    Image->Extent.height = Extent.Height;

    VkImageType ImageType = VK_IMAGE_TYPE_3D;
    VkImageViewType ImageViewType = VK_IMAGE_VIEW_TYPE_3D;
    if (RR_HAS_BIT(AdditionalFlags, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT))
    {
        ImageType = VK_IMAGE_TYPE_2D;
        ImageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
        assert(LayerCount == 6 && "Cubemap requires exactly 6 layers!");
    }
    else if (Extent.Height == 1)
    {
        ImageType = VK_IMAGE_TYPE_1D;
        ImageViewType = VK_IMAGE_VIEW_TYPE_1D;
    }
    else
    {
        ImageType = VK_IMAGE_TYPE_2D;
        ImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    }

    uint32_t MipLevels = 1;
    if (RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_MIP_MAPPED_BIT))
    {
        MipLevels =
            (uint32_t)floorf(logf((float)RR_MAX(Extent.Width, Extent.Height))) +
            1;
    }

    Image->AllocatedImageCount = 1;
    if (RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_PER_FRAME_BIT) ||
        RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_READBACK_BIT))
    {
        Image->AllocatedImageCount = RR_FRAME_OVERLAP;
    }

    VkImageUsageFlags UsageFlags = 0;
    if (RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_STORAGE_BIT))
    {
        UsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_SAMPLED_BIT))
    {
        UsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT))
    {
        UsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        UsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if (RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_TRANSFER_BIT))
    {
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    /* TODO: Some kind of real usage must be enforced aside from TRANSFER_*. */

    VkImageCreateInfo ImageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = AdditionalFlags,
        .imageType = ImageType,
        .format = Image->Format,
        .extent = (VkExtent3D){ Image->Extent.width, Image->Extent.height, 1 },
        .mipLevels = MipLevels,
        .arrayLayers = LayerCount,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = UsageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    Image->AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (Image->Format == VK_FORMAT_D16_UNORM_S8_UINT ||
        Image->Format == VK_FORMAT_D24_UNORM_S8_UINT ||
        Image->Format == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        Image->AspectFlags =
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else if (
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

    for (uint32_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Rr_AllocatedImage *AllocatedImage = Image->AllocatedImages + Index;
        AllocatedImage->Container = Image;

        vmaCreateImage(
            gRenderer->Allocator,
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
                .baseArrayLayer = 0,
                .layerCount = LayerCount,
                .baseMipLevel = 0,
                .levelCount = MipLevels,
            },
        };

        Device->CreateImageView(
            Device->Handle,
            &ImageViewCreateInfo,
            NULL,
            &AllocatedImage->View);
    }

    return (Rr_Image *)Image;
}

void Rr_DestroyImage(Rr_Image *Image)
{
    if (Image == NULL)
    {
        return;
    }

    Rr_Device *Device = &gRenderer->Device;

    for (uint32_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Device->DestroyImageView(
            Device->Handle,
            Image->AllocatedImages[Index].View,
            NULL);
        vmaDestroyImage(
            gRenderer->Allocator,
            Image->AllocatedImages[Index].Handle,
            Image->AllocatedImages[Index].Allocation);
    }

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_ImageHiveIterator It =
        Rr_GetImageHiveIterator(&gRenderer->ImageHive, Image);
    Rr_RemoveFromImageHive(&gRenderer->ImageHive, &It);

    Rr_UnlockSpinlock(&gRenderer->Lock);
}

Rr_Image2D *Rr_CreateImage2D(
    Rr_IntVec2 Extent,
    Rr_TextureFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);

    return (Rr_Image2D *)Rr_CreateImage(
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        Format,
        Flags,
        1,
        0);
}

void Rr_DestroyImage2D(Rr_Image2D *Image)
{
    Rr_DestroyImage((Rr_Image *)Image);
}

Rr_Image3D *Rr_CreateImage3D(
    Rr_IntVec3 Extent,
    Rr_TextureFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);
    assert(Extent.Depth >= 1);

    return (Rr_Image3D *)Rr_CreateImage(Extent, Format, Flags, 1, 0);
}

void Rr_DestroyImage3D(Rr_Image3D *Image)
{
    Rr_DestroyImage((Rr_Image *)Image);
}

Rr_ImageCube *Rr_CreateImageCube(
    Rr_IntVec2 Extent,
    Rr_TextureFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);

    return (Rr_ImageCube *)Rr_CreateImage(
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        Format,
        Flags,
        6,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
}

void Rr_DestroyCubemap(Rr_ImageCube *Cubemap)
{
    Rr_DestroyImage((Rr_Image *)Cubemap);
}

/* Rr_IntVec3 Rr_GetImage3DExtent(Rr_Image3D *Image) */
/* { */
/*     return (Rr_IntVec3){ */
/*         .Width = Image->Extent.width, */
/*         .Height = Image->Extent.height, */
/*         .Depth = Image->Extent.depth, */
/*     }; */
/* } */

Rr_IntVec2 Rr_GetImage2DExtent(Rr_Image2D *Image)
{
    return (Rr_IntVec2){
        .Width = Image->Extent.width,
        .Height = Image->Extent.height,
    };
}

float Rr_GetImage2DAspect(Rr_Image2D *Image)
{
    return (float)Image->Extent.width / (float)Image->Extent.height;
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

Rr_Image2D *Rr_CreateImage2DRGBA8(
    Rr_UploadContext *UploadContext,
    char *Data,
    uint32_t Width,
    uint32_t Height)
{
    int32_t DesiredChannels = 4;
    Rr_IntVec2 Extent = { .Width = Width, .Height = Height };
    size_t DataSize = Extent.Width * Extent.Height * DesiredChannels;

    Rr_Image2D *ColorImage = Rr_CreateImage2D(
        Extent,
        RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
        RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);

    Rr_UploadImage2D(
        UploadContext,
        ColorImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        },
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        RR_MAKE_DATA(DataSize, Data));

    return ColorImage;
}

Rr_Image2D *Rr_CreateImage2DRGBA8FromPNG(
    Rr_UploadContext *UploadContext,
    size_t DataSize,
    char *Data)
{
    int32_t DesiredChannels = 4;
    int32_t Channels;
    Rr_IntVec2 Extent;
    stbi_uc *ParsedData = stbi_load_from_memory(
        (stbi_uc *)Data,
        (int32_t)DataSize,
        (int32_t *)&Extent.Width,
        (int32_t *)&Extent.Height,
        &Channels,
        DesiredChannels);
    size_t ParsedSize = Extent.Width * Extent.Height * DesiredChannels;

    Rr_Image2D *ColorImage = Rr_CreateImage2D(
        Extent,
        RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
        RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);

    Rr_UploadImage2D(
        UploadContext,
        ColorImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        },
        (Rr_SyncState){
            .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            .AccessMask = VK_ACCESS_SHADER_READ_BIT,
            .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        RR_MAKE_DATA(ParsedSize, ParsedData));

    stbi_image_free(ParsedData);

    return ColorImage;
}

Rr_AllocatedImage *Rr_GetCurrentImage(Rr_Image *Image)
{
    uint32_t AllocatedImageIndex =
        gRenderer->FrameIndex % Image->AllocatedImageCount;
    return &Image->AllocatedImages[AllocatedImageIndex];
}
