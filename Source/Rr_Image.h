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

#include "Rr_Platform.h"

#include <vma/vk_mem_alloc.h>

struct Rr_Sampler
{
    VkSampler Handle;

    char Name[32];

    Rr_AtomicInt RefCount;
};

#define RR_HIVE_TYPE      Rr_Sampler
#define RR_HIVE_TYPE_NAME Sampler
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroySampler(Rr_Sampler *Sampler);

typedef struct Rr_ImageViewKey Rr_ImageViewKey;
struct Rr_ImageViewKey
{
    VkImageSubresourceRange SubresourceRange;
    VkImageViewType Type;
    VkFormat Format;
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
#include "Rr_Hive.h"

typedef struct Rr_ImageViewStorage Rr_ImageViewStorage;
struct Rr_ImageViewStorage
{
    Rr_ImageViewMapHive Hive;
    Rr_ImageViewMap *Map;
    Rr_Spinlock Lock;
};

extern Rr_ImageViewStorage *Rr_CreateImageViewStorage(void);

extern void Rr_DestroyImageViewStorage(Rr_ImageViewStorage *ViewStorage);

typedef struct Rr_AllocatedImage Rr_AllocatedImage;
struct Rr_AllocatedImage
{
    VkImage Handle;
    Rr_ImageViewStorage *ViewStorage;
    VmaAllocation Allocation;
    struct Rr_Image *Container;
};

extern VkImageView Rr_GetVulkanImageView(
    Rr_AllocatedImage *AllocatedImage,
    Rr_ImageViewKey *Key);

struct Rr_Image
{
    VkExtent3D Extent;
    VkImageAspectFlags AspectFlags;
    VkFormat Format;
    VkSampleCountFlags SampleCount;
    Rr_ImageFlags Flags;
    uint32_t AllocatedImageCount;
    Rr_AllocatedImage AllocatedImages[RR_FRAME_OVERLAP];

    char Name[32];

    Rr_AtomicInt RefCount;
};

typedef struct Rr_Image Rr_Image;

#define RR_HIVE_TYPE      Rr_Image
#define RR_HIVE_TYPE_NAME Image
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroyImage(Rr_Image *Image);

extern Rr_Image2D *Rr_CreateSTBImage2D(
    struct Rr_Graph *Graph,
    Rr_ImageFormat Format,
    size_t DataSize,
    const char *Data);

extern Rr_AllocatedImage *Rr_GetCurrentAllocatedImage(Rr_Image *Image);
