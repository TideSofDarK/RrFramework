/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "Rr_Image.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_RHI
#include "Rr_LogMacro.h"

#include "Rr_RHI.h"

#include <assert.h>
#include <stdio.h>

Rr_Sampler *Rr_CreateSampler(Rr_SamplerInfo *Info)
{
    assert(Info != NULL);

    Rr_Device *Device = &gRHI->Device;

    Rr_LockSpinlock(&gRHI->SamplersLock);

    Rr_Sampler *Sampler =
        Rr_PushSamplerIntoHive(&gRHI->Samplers, Rr_GetPermanent()).Element;

    Rr_UnlockSpinlock(&gRHI->SamplersLock);

    RR_ZERO_PTR(Sampler);

    Rr_ConsumeNextObjectName(Sampler->Name);

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
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
        .borderColor = Rr_ToVulkanBorderColor(Info->BorderColor),
        .unnormalizedCoordinates = Info->UnnormalizedCoordinates,
    };

    VkResult Result = Device->CreateSampler(
        Device->Handle,
        &SamplerInfo,
        NULL,
        &Sampler->Handle);
    assert(Result == VK_SUCCESS);

    Rr_SetVulkanObjectName(
        VK_OBJECT_TYPE_SAMPLER,
        (uint64_t)Sampler->Handle,
        Sampler->Name);

    return Sampler;
}

void Rr_ReleaseSampler(Rr_Sampler *Sampler)
{
    if (Sampler == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->ReleasedSamplersLock);

    Rr_InsertIntoHandleSet(
        &gRHI->ReleasedSamplers,
        (Rr_Handle const *)&Sampler,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->ReleasedSamplersLock);
}

void Rr_DestroySampler(Rr_Sampler *Sampler)
{
    assert(Sampler != NULL && Sampler->Handle != VK_NULL_HANDLE);

    Rr_Device *Device = &gRHI->Device;

    Device->DestroySampler(Device->Handle, Sampler->Handle, NULL);

    Rr_LockSpinlock(&gRHI->SamplersLock);

    Rr_SamplerHiveIterator It =
        Rr_GetSamplerHiveIterator(&gRHI->Samplers, Sampler);
    Rr_RemoveFromSamplerHive(&gRHI->Samplers, &It);

    Rr_UnlockSpinlock(&gRHI->SamplersLock);
}

Rr_ImageViewMap *Rr_CreateImageViewMap(void)
{
    Rr_Arena *Arena = Rr_GetPermanent();

    Rr_LockSpinlock(&gRHI->ImageViewMapsLock);

    Rr_ImageViewMap *ImageViewMap =
        RR_GET_FREE_LIST_ITEM(&gRHI->ImageViewMaps, Arena);
    if (!ImageViewMap->Capacity)
    {
        Rr_InitImageViewMap(ImageViewMap, Arena);
    }

    Rr_UnlockSpinlock(&gRHI->ImageViewMapsLock);

    return ImageViewMap;
}

void Rr_DestroyImageViewMap(
    Rr_ImageViewMap *ImageViewMap,
    bool DestroyFramebuffers)
{
    Rr_Device *Device = &gRHI->Device;

    Rr_ImageViewMapIterator It = Rr_BeginInImageViewMap(ImageViewMap);
    while (!Rr_IsImageViewMapEnd(It))
    {
        VkImageView Handle = It.Data->Value;
        if (DestroyFramebuffers)
        {
            Rr_DestroyFramebuffers(Handle);
        }
        Device->DestroyImageView(Device->Handle, Handle, NULL);
        It = Rr_EraseFromImageViewMap(It);
    }

    Rr_LockSpinlock(&gRHI->ImageViewMapsLock);

    RR_RETURN_FREE_LIST_ITEM(&gRHI->ImageViewMaps, ImageViewMap);

    Rr_UnlockSpinlock(&gRHI->ImageViewMapsLock);
}

VkImageView Rr_GetVulkanImageView(
    Rr_AllocatedImage *AllocatedImage,
    Rr_ImageViewKey const *Key)
{
    Rr_LockSpinlock(&AllocatedImage->ImageViewMapLock);

    Rr_ImageViewMapIterator It =
        Rr_FindInImageViewMap(AllocatedImage->ImageViewMap, Key);
    if (!Rr_IsImageViewMapEnd(It))
    {
        Rr_UnlockSpinlock(&AllocatedImage->ImageViewMapLock);

        return It.Data->Value;
    }

    Rr_UnlockSpinlock(&AllocatedImage->ImageViewMapLock);

    Rr_Device *Device = &gRHI->Device;

    VkImageViewCreateInfo ImageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = AllocatedImage->Handle,
        .viewType = Key->Type,
        .format = Key->Format != VK_FORMAT_UNDEFINED
                      ? Key->Format
                      : AllocatedImage->Container->Format,
        .subresourceRange = Key->SubresourceRange,
    };

    VkImageView Handle = VK_NULL_HANDLE;
    VkResult Result = Device->CreateImageView(
        Device->Handle,
        &ImageViewCreateInfo,
        NULL,
        &Handle);
    assert(Result == VK_SUCCESS);

#ifdef RR_DEBUG
    if (AllocatedImage->Container->Name[0] != '\0')
    {
        char NameBuffer[128];
        uint32_t LayerCount =
            Key->SubresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS
                ? AllocatedImage->Container->LayerCount
                : Key->SubresourceRange.layerCount;
        uint32_t LevelCount =
            Key->SubresourceRange.levelCount == VK_REMAINING_MIP_LEVELS
                ? AllocatedImage->Container->LevelCount
                : Key->SubresourceRange.levelCount;
        snprintf(
            NameBuffer,
            sizeof(NameBuffer) - 1,
            "Rr.Image.%s.View.%d-%d/%d-%d",
            AllocatedImage->Container->Name,
            Key->SubresourceRange.baseArrayLayer,
            Key->SubresourceRange.baseArrayLayer + LayerCount,
            Key->SubresourceRange.baseMipLevel,
            Key->SubresourceRange.baseMipLevel + LevelCount);

        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_IMAGE_VIEW,
            (uint64_t)Handle,
            NameBuffer);
    }
#endif

    Rr_LockSpinlock(&AllocatedImage->ImageViewMapLock);

    Rr_InsertIntoImageViewMap(
        AllocatedImage->ImageViewMap,
        Key,
        &Handle,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&AllocatedImage->ImageViewMapLock);

    return Handle;
}

static inline VkSampleCountFlagBits Rr_ToVulkanSampleCountFlagBits(
    Rr_ImageFlags ImageFlags)
{
    /* TODO: Hardcoded offset. */
    return ImageFlags >> 9;
}

static Rr_Image *Rr_CreateImage(
    Rr_IntVec3 Extent,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags,
    uint32_t LayerCount,
    VkImageType ImageType,
    VkImageCreateFlags AdditionalFlags)
{
    Rr_LockSpinlock(&gRHI->ImagesLock);

    Rr_Image *Image =
        Rr_PushImageIntoHive(&gRHI->Images, Rr_GetPermanent()).Element;

    Rr_UnlockSpinlock(&gRHI->ImagesLock);

    uint32_t LevelCount = 1;
    if (Flags & RR_IMAGE_FLAGS_MIP_MAPPED_BIT)
    {
        int32_t Max = RR_MAX(RR_MAX(Extent.Width, Extent.Height), Extent.Depth);
        LevelCount = (uint32_t)floorf(log2f((float)Max)) + 1;
    }

    *Image = (Rr_Image){
        .Flags = Flags,
        .Format = Rr_ToVulkanImageFormat(Format),
        .Extent.width = (uint32_t)Extent.Width,
        .Extent.height = (uint32_t)Extent.Height,
        .Extent.depth = (uint32_t)Extent.Depth,
        .LayerCount = LayerCount,
        .LevelCount = LevelCount,
    };

    Rr_ConsumeNextObjectName(Image->Name);

    Image->AllocatedImageCount = 1;
    if (Flags & RR_IMAGE_FLAGS_PER_FRAME_BIT)
    {
        Image->AllocatedImageCount = RR_FRAME_OVERLAP;
    }

    VkImageUsageFlags UsageFlags = 0;
    if (Flags & RR_IMAGE_FLAGS_STORAGE_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (Flags & RR_IMAGE_FLAGS_SAMPLED_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (Flags & RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (Flags & RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if (Flags & RR_IMAGE_FLAGS_TRANSFER_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (Flags & RR_IMAGE_FLAGS_MUTABLE_FORMAT_BIT)
    {
        AdditionalFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }

    /* TODO: Some kind of real usage must be enforced aside from TRANSFER_*. */

    Image->SampleCount = Rr_ToVulkanSampleCountFlagBits(Flags);
    if (Image->SampleCount == 0)
    {
        Image->SampleCount = VK_SAMPLE_COUNT_1_BIT;
    }
    VkImageCreateInfo ImageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = AdditionalFlags,
        .imageType = ImageType,
        .format = Image->Format,
        .extent = Image->Extent,
        .mipLevels = LevelCount,
        .arrayLayers = LayerCount,
        .samples = Image->SampleCount,
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
        AllocatedImage->SyncState = RR_EMPTY_SYNC;
        AllocatedImage->Container = Image;

        VkResult Result = vmaCreateImage(
            gRHI->VMA,
            &ImageCreateInfo,
            &AllocationCreateInfo,
            &AllocatedImage->Handle,
            &AllocatedImage->Allocation,
            NULL);

        assert(Result == VK_SUCCESS);

        AllocatedImage->ImageViewMap = Rr_CreateImageViewMap();

#ifdef RR_USE_GPU_DEBUG_UTILS
        char ObjectName[RR_MAX_OBJECT_NAME_LENGTH];
        if (snprintf(
                ObjectName,
                sizeof(ObjectName) - 1,
                "%s#%d",
                Image->Name,
                Index))
        {
        }
        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_IMAGE,
            (uint64_t)AllocatedImage->Handle,
            ObjectName);
#endif
    }

    return (Rr_Image *)Image;
}

void Rr_ReleaseImage(Rr_Image *Image)
{
    if (Image == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->ReleasedImagesLock);

    Rr_InsertIntoHandleSet(
        &gRHI->ReleasedImages,
        (Rr_Handle const *)&Image,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->ReleasedImagesLock);
}

void Rr_DestroyImage(Rr_Image *Image)
{
    assert(Image && Image->AllocatedImageCount > 0);

    bool DestroyFramebuffers =
        (Image->Flags & RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT) ||
        (Image->Flags & RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);

    for (uint32_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Rr_AllocatedImage *AllocatedImage = &Image->AllocatedImages[Index];

        Rr_DestroyImageViewMap(
            AllocatedImage->ImageViewMap,
            DestroyFramebuffers);

        vmaDestroyImage(
            gRHI->VMA,
            AllocatedImage->Handle,
            AllocatedImage->Allocation);
    }

    Rr_LockSpinlock(&gRHI->ImagesLock);

    Rr_ImageHiveIterator It = Rr_GetImageHiveIterator(&gRHI->Images, Image);
    Rr_RemoveFromImageHive(&gRHI->Images, &It);

    Rr_UnlockSpinlock(&gRHI->ImagesLock);
}

Rr_Image2D *Rr_CreateImage2D(
    Rr_IntVec2 Extent,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);

    return (Rr_Image2D *)Rr_CreateImage(
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        Format,
        Flags,
        1,
        VK_IMAGE_TYPE_2D,
        0);
}

Rr_Image2DArray *Rr_CreateImage2DArray(
    Rr_IntVec2 Extent,
    uint32_t ArrayCount,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);
    assert(ArrayCount >= 1);

    return (Rr_Image2DArray *)Rr_CreateImage(
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        Format,
        Flags,
        ArrayCount,
        VK_IMAGE_TYPE_2D,
        0);
}

Rr_Image3D *Rr_CreateImage3D(
    Rr_IntVec3 Extent,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);
    assert(Extent.Depth >= 1);

    return (Rr_Image3D *)
        Rr_CreateImage(Extent, Format, Flags, 1, VK_IMAGE_TYPE_3D, 0);
}

Rr_ImageCube *Rr_CreateImageCube(
    Rr_IntVec2 Extent,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);

    return (Rr_ImageCube *)Rr_CreateImage(
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        Format,
        Flags,
        6,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
}

Rr_ImageFormat Rr_GetImageFormat(struct Rr_Image *Image)
{
    return Rr_ToImageFormat(Image->Format);
}

Rr_IntVec2 Rr_GetImage2DExtent(Rr_Image2D *Image2D)
{
    return (Rr_IntVec2){
        .Width = (int32_t)Image2D->Extent.width,
        .Height = (int32_t)Image2D->Extent.height,
    };
}

float Rr_GetImage2DAspect(Rr_Image2D *Image)
{
    return (float)Image->Extent.width / (float)Image->Extent.height;
}

Rr_IntVec3 Rr_GetImageExtent(Rr_Image *Image)
{
    return (Rr_IntVec3){
        .Width = (int32_t)Image->Extent.width,
        .Height = (int32_t)Image->Extent.height,
        .Depth = (int32_t)Image->Extent.depth,
    };
}

Rr_AllocatedImage *Rr_GetCurrentAllocatedImage(Rr_Image *Image)
{
    return &Image->AllocatedImages
                [gRHI->FrameIndex % Image->AllocatedImageCount];
}
