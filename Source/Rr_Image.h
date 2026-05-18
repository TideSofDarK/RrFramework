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

#pragma once

#include <Rr/Rr_Image.h>

#include "Rr_Platform.h"
#include "Rr_Vulkan.h"

#include <vma/vk_mem_alloc.h>

struct Rr_Sampler
{
    VkSampler Handle;

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

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

typedef struct Rr_ImageView Rr_ImageView;
struct Rr_ImageView
{
    Rr_ImageViewKey Key;
    Rr_ImageView *Children[4];

    VkImageView Handle;
};

#define RR_HIVE_TYPE      Rr_ImageView
#define RR_HIVE_TYPE_NAME ImageView
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

typedef struct Rr_ImageViewStorage Rr_ImageViewStorage;
struct Rr_ImageViewStorage
{
    Rr_ImageViewHive Hive;
    Rr_ImageView *Map;
    Rr_Spinlock Lock;
};

extern Rr_ImageViewStorage *Rr_CreateImageViewStorage(void);

extern void Rr_DestroyImageViewStorage(
    Rr_ImageViewStorage *ViewStorage,
    bool DestroyFramebuffers);

typedef struct Rr_AllocatedImage Rr_AllocatedImage;
struct Rr_AllocatedImage
{
    VkImage Handle;
    Rr_ImageViewStorage *ViewStorage;
    VmaAllocation Allocation;
    struct Rr_Image *Container;
    Rr_SyncState SyncState;
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
    uint32_t LayerCount;
    uint32_t LevelCount;
    uint32_t AllocatedImageCount;
    Rr_AllocatedImage AllocatedImages[RR_FRAME_OVERLAP];

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

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
