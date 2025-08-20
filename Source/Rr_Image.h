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

#pragma once

#include <Rr/Rr_Image.h>

#include "Rr_UploadContext.h"

#include <Rr/Rr_App.h>
#include <Rr/Rr_Asset.h>

#include <xxHash/xxhash.h>

#include <vma/vk_mem_alloc.h>

struct Rr_Sampler
{
    VkSampler Handle;
    Rr_AtomicInt RefCount;
};

#define RR_HIVE_TYPE      Rr_Sampler
#define RR_HIVE_TYPE_NAME Sampler
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

extern void Rr_DestroySampler(Rr_Sampler *Sampler);

typedef struct Rr_ImageViewKey Rr_ImageViewKey;
struct Rr_ImageViewKey
{
    VkImageSubresourceRange SubresourceRange;
    VkImageViewType Type;
};

typedef struct Rr_ImageViewMap Rr_ImageViewMap;
struct Rr_ImageViewMap
{
    Rr_ImageViewMap *Children[4];
    Rr_ImageViewKey Key;
    VkImageView Value;
};

#define RR_HIVE_TYPE      Rr_ImageViewMap
#define RR_HIVE_TYPE_NAME ImageViewMap
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

typedef struct Rr_ImageViewStorage Rr_ImageViewStorage;
struct Rr_ImageViewStorage
{
    Rr_ImageViewMapHive Hive;
    Rr_ImageViewMap *Map;
};

typedef struct Rr_AllocatedImage Rr_AllocatedImage;
struct Rr_AllocatedImage
{
    VkImage Handle;
    Rr_ImageViewStorage *ViewStorage;
    VmaAllocation Allocation;
    struct Rr_Image *Container;
};

extern Rr_ImageViewStorage *Rr_CreateImageViewStorage(void);

extern void Rr_DestroyImageViewStorage(Rr_ImageViewStorage *ViewStorage);

extern VkImageView Rr_GetVulkanImageView(
    Rr_AllocatedImage *AllocatedImage,
    Rr_ImageViewKey *Key);

struct Rr_Image
{
    VkExtent3D Extent;
    VkImageAspectFlags AspectFlags;
    VkFormat Format;
    Rr_ImageFlags Flags;
    uint32_t AllocatedImageCount;
    Rr_AllocatedImage AllocatedImages[RR_FRAME_OVERLAP];

    Rr_AtomicInt RefCount;
};

typedef struct Rr_Image Rr_Image;

#define RR_HIVE_TYPE      Rr_Image
#define RR_HIVE_TYPE_NAME Image
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

extern void Rr_DestroyImage(Rr_Image *Image);

extern void Rr_UploadStagingImage2D(
    Rr_UploadContext *UploadContext,
    Rr_Image2D *Image,
    VkImageAspectFlags Aspect,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    struct Rr_Buffer *StagingBuffer,
    uint64_t StagingOffset,
    uint64_t StagingSize);

extern void Rr_UploadImage2D(
    Rr_UploadContext *UploadContext,
    Rr_Image2D *Image,
    VkImageAspectFlags Aspect,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Data Data);

extern Rr_Image2D *Rr_CreateImage2DRGBA8(
    Rr_UploadContext *UploadContext,
    char *Data,
    uint32_t Width,
    uint32_t Height);

Rr_Image2D *Rr_CreateImage2DRGBA8FromPNG(
    Rr_UploadContext *UploadContext,
    uint64_t DataSize,
    char *Data,
    bool Linear);

extern size_t Rr_GetImagePNGRGBA8Size(
    size_t DataSize,
    char *Data,
    Rr_Arena *Arena);

extern Rr_AllocatedImage *Rr_GetCurrentImage(Rr_Image *Image);
