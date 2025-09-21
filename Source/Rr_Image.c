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

#include "Rr_Renderer.h"

#include <stb/stb_image.h>

#include <xxHash/xxhash.h>

#include <assert.h>

Rr_Sampler *Rr_CreateSampler(Rr_SamplerInfo *Info)
{
    assert(Info != NULL);

    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->SamplersLock);

    Rr_SamplerHiveIterator It = Rr_PushSamplerIntoHiveLocked(
        &gRenderer->Samplers,
        gRenderer->Arena,
        &gRenderer->Lock);
    Rr_Sampler *Sampler = It.Element;

    Rr_UnlockSpinlock(&gRenderer->SamplersLock);

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
        .minLod = Info->MinLod,
        .maxLod = Info->MaxLod,
        .borderColor = Rr_ToVulkanBorderColor(Info->BorderColor),
        .unnormalizedCoordinates = Info->UnnormalizedCoordinates,
    };

    VkResult Result = Device->CreateSampler(
        Device->Handle,
        &SamplerInfo,
        NULL,
        &Sampler->Handle);
    assert(Result == VK_SUCCESS);

#ifdef RR_DEBUG
    Rr_SetVulkanObjectName(
        VK_OBJECT_TYPE_SAMPLER,
        (uint64_t)Sampler->Handle,
        Sampler->Name);
#endif

    return Sampler;
}

void Rr_ReleaseSampler(Rr_Sampler *Sampler)
{
    if (Sampler == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRenderer->ReleasedSamplersLock);

    *Rr_PushHandleIntoHiveLocked(
         &gRenderer->ReleasedSamplers,
         gRenderer->Arena,
         &gRenderer->Lock)
         .Element = Sampler;

    Rr_UnlockSpinlock(&gRenderer->ReleasedSamplersLock);
}

void Rr_DestroySampler(Rr_Sampler *Sampler)
{
    assert(Sampler != NULL && Sampler->Handle != VK_NULL_HANDLE);

    Rr_PrintDestroyMessage("Rr_Sampler", Sampler->Name, Sampler);

    Rr_Device *Device = &gRenderer->Device;

    Device->DestroySampler(Device->Handle, Sampler->Handle, NULL);

    Rr_LockSpinlock(&gRenderer->SamplersLock);

    Rr_SamplerHiveIterator It =
        Rr_GetSamplerHiveIterator(&gRenderer->Samplers, Sampler);
    Rr_RemoveFromSamplerHive(&gRenderer->Samplers, &It);

    Rr_UnlockSpinlock(&gRenderer->SamplersLock);
}

Rr_ImageViewStorage *Rr_CreateImageViewStorage(void)
{
    Rr_LockSpinlock(&gRenderer->ImageViewStorageLock);
    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_ImageViewStorage *ViewStorage =
        RR_GET_FREE_LIST_ITEM(&gRenderer->ImageViewStorage, gRenderer->Arena);

    Rr_UnlockSpinlock(&gRenderer->Lock);
    Rr_UnlockSpinlock(&gRenderer->ImageViewStorageLock);

    ViewStorage->Map = NULL;
    Rr_ClearImageViewMapHive(&ViewStorage->Hive);

    return ViewStorage;
}

void Rr_DestroyImageViewStorage(Rr_ImageViewStorage *ViewStorage)
{
    Rr_Device *Device = &gRenderer->Device;

    for (Rr_ImageViewMapHiveIterator It = ViewStorage->Hive.Begin;
         It.Element != ViewStorage->Hive.End.Element;)
    {
        Rr_ImageViewMap *Map = It.Element;
        if (Map->Value != VK_NULL_HANDLE)
        {
            Rr_DestroyVulkanFramebuffers(Map->Value);
            Device->DestroyImageView(Device->Handle, Map->Value, NULL);
            Map->Value = VK_NULL_HANDLE;
        }
        Rr_AdvanceImageViewMapHiveIterator(&It);
    }

    Rr_LockSpinlock(&gRenderer->ImageViewStorageLock);

    RR_RETURN_FREE_LIST_ITEM(&gRenderer->ImageViewStorage, ViewStorage);

    Rr_UnlockSpinlock(&gRenderer->ImageViewStorageLock);
}

VkImageView Rr_GetVulkanImageView(
    Rr_AllocatedImage *AllocatedImage,
    Rr_ImageViewKey *Key)
{
    VkImageView *ImageViewRef = NULL;

    Rr_LockSpinlock(&AllocatedImage->ViewStorage->Lock);

    Rr_ImageViewMap **MapRef = &AllocatedImage->ViewStorage->Map;
    for (uint64_t Hash = XXH64(Key, sizeof(Rr_ImageViewKey), 0); *MapRef;
         Hash <<= 2)
    {
        if ((*MapRef)->Value == VK_NULL_HANDLE)
        {
            (*MapRef)->Key = *Key;
            ImageViewRef = &(*MapRef)->Value;

            goto Found;
        }
        else if (memcmp(&(*MapRef)->Key, Key, sizeof(Rr_ImageViewKey)) == 0)
        {
            ImageViewRef = &(*MapRef)->Value;

            goto Found;
        }
        MapRef = &(*MapRef)->Children[Hash >> 62];
    }
    *MapRef = Rr_PushImageViewMapIntoHiveLocked(
                  &AllocatedImage->ViewStorage->Hive,
                  gRenderer->Arena,
                  &gRenderer->Lock)
                  .Element;
    (*MapRef)->Key = *Key;
    (*MapRef)->Value = VK_NULL_HANDLE;
    RR_ZERO_PTR((*MapRef)->Children);
    ImageViewRef = &(*MapRef)->Value;

Found:

    Rr_UnlockSpinlock(&AllocatedImage->ViewStorage->Lock);

    if (*ImageViewRef != VK_NULL_HANDLE)
    {
        return *ImageViewRef;
    }

    Rr_Device *Device = &gRenderer->Device;

    VkImageViewCreateInfo ImageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = AllocatedImage->Handle,
        .viewType = Key->Type,
        .format = Key->Format != VK_FORMAT_UNDEFINED
                      ? Key->Format
                      : AllocatedImage->Container->Format,
        .subresourceRange = Key->SubresourceRange,
    };

    VkResult Result = Device->CreateImageView(
        Device->Handle,
        &ImageViewCreateInfo,
        NULL,
        ImageViewRef);
    assert(Result == VK_SUCCESS);

#ifdef RR_DEBUG
    if (AllocatedImage->Container->Name[0] != '\0')
    {
        char NameBuffer[128];
        snprintf(
            NameBuffer,
            63,
            "%s_ImageView_%d^%d_%d^%d",
            AllocatedImage->Container->Name,
            Key->SubresourceRange.baseArrayLayer,
            Key->SubresourceRange.baseArrayLayer +
                Key->SubresourceRange.layerCount,
            Key->SubresourceRange.baseMipLevel,
            Key->SubresourceRange.baseMipLevel +
                Key->SubresourceRange.levelCount);

        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_IMAGE_VIEW,
            (uint64_t)*ImageViewRef,
            NameBuffer);
    }
#endif

    return *ImageViewRef;
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
    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->ImagesLock);

    Rr_ImageHiveIterator It = Rr_PushImageIntoHiveLocked(
        &gRenderer->Images,
        gRenderer->Arena,
        &gRenderer->Lock);
    Rr_Image *Image = It.Element;

    Rr_UnlockSpinlock(&gRenderer->ImagesLock);

    Image->Flags = Flags;
    Image->Format = Rr_ToVulkanImageFormat(Format);
    Image->Extent.width = (uint32_t)Extent.Width;
    Image->Extent.height = (uint32_t)Extent.Height;
    Image->Extent.depth = (uint32_t)Extent.Depth;

    Rr_ConsumeNextObjectName(Image->Name);

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

    if (RR_HAS_BIT(Flags, RR_IMAGE_FLAGS_MUTABLE_FORMAT_BIT))
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
        .mipLevels = MipLevels,
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
        AllocatedImage->Container = Image;

        VkResult Result = vmaCreateImage(
            gRenderer->Allocator,
            &ImageCreateInfo,
            &AllocationCreateInfo,
            &AllocatedImage->Handle,
            &AllocatedImage->Allocation,
            NULL);
        assert(Result == VK_SUCCESS);

        AllocatedImage->ViewStorage = Rr_CreateImageViewStorage();

#ifdef RR_DEBUG
        char ObjectName[32];
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

    Rr_LockSpinlock(&gRenderer->ReleasedImagesLock);

    *Rr_PushHandleIntoHiveLocked(
         &gRenderer->ReleasedImages,
         gRenderer->Arena,
         &gRenderer->Lock)
         .Element = Image;

    Rr_UnlockSpinlock(&gRenderer->ReleasedImagesLock);
}

void Rr_DestroyImage(Rr_Image *Image)
{
    assert(Image && Image->AllocatedImageCount > 0);

    Rr_PrintDestroyMessage("Rr_Image", Image->Name, Image);

    Rr_Device *Device = &gRenderer->Device;

    for (uint32_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Rr_AllocatedImage *AllocatedImage = &Image->AllocatedImages[Index];

        Rr_DestroyImageViewStorage(AllocatedImage->ViewStorage);

        Rr_LockSpinlock(&gRenderer->SyncStateStorageLock);

        Rr_EraseSyncState(
            &gRenderer->SyncStateStorage,
            (uint64_t)AllocatedImage->Handle);

        Rr_UnlockSpinlock(&gRenderer->SyncStateStorageLock);

        vmaDestroyImage(
            gRenderer->Allocator,
            AllocatedImage->Handle,
            AllocatedImage->Allocation);
    }

    Rr_LockSpinlock(&gRenderer->ImagesLock);

    Rr_ImageHiveIterator It =
        Rr_GetImageHiveIterator(&gRenderer->Images, Image);
    Rr_RemoveFromImageHive(&gRenderer->Images, &It);

    Rr_UnlockSpinlock(&gRenderer->ImagesLock);
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

Rr_IntVec3 Rr_GetImage3DExtent(Rr_Image3D *Image3D)
{
    return (Rr_IntVec3){
        .Width = (int32_t)Image3D->Extent.width,
        .Height = (int32_t)Image3D->Extent.height,
        .Depth = (int32_t)Image3D->Extent.depth,
    };
}

Rr_Image2D *Rr_CreateSTBImage2D(
    struct Rr_Graph *Graph,
    Rr_ImageFormat Format,
    size_t DataSize,
    const char *Data)
{
    assert(
        Format == RR_IMAGE_FORMAT_R8G8B8A8_SRGB ||
        Format == RR_IMAGE_FORMAT_R8G8B8A8_UNORM ||
        Format == RR_IMAGE_FORMAT_B8G8R8A8_UNORM);

    Rr_IntVec2 ImageSize;
    int32_t ImageChannels;
    char *ImageData = (char *)stbi_load_from_memory(
        (stbi_uc *)Data,
        (int32_t)DataSize,
        &ImageSize.Width,
        &ImageSize.Height,
        &ImageChannels,
        4);

    size_t ImageDataSize = 4 * (size_t)(ImageSize.Width * ImageSize.Height);

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        ImageDataSize,
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);

    Rr_Image2D *Image2D = Rr_CreateImage2D(
        ImageSize,
        Format,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

    memcpy(Rr_GetMappedBufferData(StagingBuffer), ImageData, ImageDataSize);

    stbi_image_free(ImageData);

    Rr_CopyBufferToImage2D(
        Rr_GetGraph(),
        StagingBuffer,
        0,
        ImageSize,
        Image2D,
        0);

    Rr_ReleaseBuffer(StagingBuffer);

    return Image2D;
}

Rr_AllocatedImage *Rr_GetCurrentAllocatedImage(Rr_Image *Image)
{
    return &Image->AllocatedImages
                [gRenderer->FrameIndex % Image->AllocatedImageCount];
}
