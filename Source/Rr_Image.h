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

#include <vma/vk_mem_alloc.h>

struct Rr_Sampler
{
    VkSampler Handle;
};

typedef struct Rr_Image Rr_Image;
struct Rr_Image
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    struct Rr_ImageContainer *Container;
};

#define RR_DEFINE_IMAGE_TYPE(Name)                      \
    struct Name                                         \
    {                                                   \
        VkExtent3D Extent;                              \
        VkImageAspectFlags AspectFlags;                 \
        VkFormat Format;                                \
        Rr_ImageFlags Flags;                            \
        size_t AllocatedImageCount;                     \
        Rr_Image AllocatedImages[RR_MAX_FRAME_OVERLAP]; \
    }

typedef struct Rr_ImageContainer Rr_ImageContainer;
RR_DEFINE_IMAGE_TYPE(Rr_ImageContainer);
RR_DEFINE_IMAGE_TYPE(Rr_Image2D);
RR_DEFINE_IMAGE_TYPE(Rr_Image2DArray);
RR_DEFINE_IMAGE_TYPE(Rr_Image3D);
RR_DEFINE_IMAGE_TYPE(Rr_Image3DArray);
RR_DEFINE_IMAGE_TYPE(Rr_Cubemap);

#undef RR_DEFINE_IMAGE_TYPE

extern void Rr_UploadStagingImage2D(
    Rr_UploadContext *UploadContext,
    Rr_Image2D *Image,
    VkImageAspectFlags Aspect,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    struct Rr_Buffer *StagingBuffer,
    size_t StagingOffset,
    size_t StagingSize);

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
    size_t DataSize,
    char *Data);

extern size_t Rr_GetImagePNGRGBA8Size(
    size_t DataSize,
    char *Data,
    Rr_Arena *Arena);

extern Rr_Image *Rr_GetCurrentImage(Rr_ImageContainer *Container);
