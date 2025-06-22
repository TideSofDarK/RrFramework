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

#include <Rr/Rr_Renderer.h>

#include "Rr_Buffer.h"
#include "Rr_Graph.h"
#include "Rr_Image.h"
#include "Rr_Load.h"
#include "Rr_Pipeline.h"
#include "Rr_Vulkan.h"

#include <xxHash/xxhash.h>

typedef enum
{
    RR_RENDERER_OBJECT_TYPE_BUFFER,
    RR_RENDERER_OBJECT_TYPE_IMAGE,
    RR_RENDERER_OBJECT_TYPE_PIPELINE_LAYOUT,
    RR_RENDERER_OBJECT_TYPE_COMPUTE_PIPELINE,
    RR_RENDERER_OBJECT_TYPE_GRAPHICS_PIPELINE,
    RR_RENDERER_OBJECT_TYPE_SAMPLER,
} Rr_RendererObjectType;

typedef struct Rr_RendererObject Rr_RendererObject;
struct Rr_RendererObject
{
    void *Ptr;
    Rr_RendererObjectType Type;
};

#define RR_HIVE_TYPE      Rr_RendererObject
#define RR_HIVE_TYPE_NAME RendererObject
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

typedef struct Rr_SwapchainImage Rr_SwapchainImage;
struct Rr_SwapchainImage
{
    VkImage Handle;
    Rr_ImageViewStorage *ViewStorage;
    VkSemaphore EarlySemaphore;
    VkSemaphore LateSemaphore;
};

typedef struct Rr_Swapchain Rr_Swapchain;
struct Rr_Swapchain
{
    uint32_t PresentModeCount;
    Rr_PresentMode PresentModes[8];
    Rr_PresentMode PresentMode;
    VkSwapchainKHR Handle;
    VkFormat Format;
    VkColorSpaceKHR ColorSpace;
    VkExtent3D Extent;
    atomic_bool RecreatePending;
};

typedef struct Rr_ImmediateMode Rr_ImmediateMode;
struct Rr_ImmediateMode
{
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
};

typedef struct Rr_Frame Rr_Frame;
struct Rr_Frame
{
    Rr_Image2D *VirtualSwapchainImage;

    VkCommandPool CommandPool;
    VkCommandBuffer EarlyCommandBuffer;
    VkCommandBuffer LateCommandBuffer;

    VkSemaphore AcquireSemaphore;
    VkFence SubmitFence;

    Rr_DescriptorAllocator *DescriptorAllocator;

    Rr_Graph *Graph;

    struct
    {
        RR_ARRAY(Rr_Buffer *) Buffers;
        RR_ARRAY(Rr_Image *) Images;
        RR_ARRAY(Rr_Sampler *) Samplers;
        RR_ARRAY(Rr_ComputePipeline *) ComputePipelines;
        RR_ARRAY(Rr_GraphicsPipeline *) GraphicsPipelines;
    } UsedObjects;

    Rr_Arena *Arena;
};

extern void Rr_MarkBufferUsed(Rr_Frame *Frame, Rr_Buffer *Buffer);

extern void Rr_MarkImageUsed(Rr_Frame *Frame, Rr_Image *Image);

extern void Rr_MarkSamplerUsed(Rr_Frame *Frame, Rr_Sampler *Sampler);

extern void Rr_MarkComputePipelineUsed(
    Rr_Frame *Frame,
    Rr_ComputePipeline *ComputePipeline);

extern void Rr_MarkGraphicsPipelineUsed(
    Rr_Frame *Frame,
    Rr_GraphicsPipeline *GraphicsPipeline);

/* NOTE: To pass various attachment configurations around we use the following
 * order of image views (a.k.a. attachments):
 * 1) N color attachments
 * 2) M resolve attachments
 * 3) Depth/stencil attachment
 * M must be less or equal to N. Depth/stencil attachment may not be present. */

typedef struct Rr_FramebufferMapKey Rr_FramebufferMapKey;
struct Rr_FramebufferMapKey
{
    VkExtent3D Extent;
    uint32_t ColorAttachmentCount;
    uint32_t ResolveAttachmentCount;
    uint32_t DepthStencil;
    VkImageView ImageViews[RR_MAX_COLOR_ATTACHMENTS * 2 + 1];
};

typedef struct Rr_FramebufferMap Rr_FramebufferMap;
struct Rr_FramebufferMap
{
    Rr_FramebufferMapKey Key;
    VkFramebuffer Value;
    Rr_FramebufferMap *Children[4];
};

#define RR_HIVE_TYPE      Rr_FramebufferMap
#define RR_HIVE_TYPE_NAME FramebufferMap
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

typedef struct Rr_FramebufferStorage Rr_FramebufferStorage;
struct Rr_FramebufferStorage
{
    Rr_FramebufferMap *Map;
    Rr_FramebufferMapHive Hive;
};

extern VkFramebuffer Rr_GetVulkanFramebuffer(
    VkRenderPass RenderPass,
    Rr_FramebufferMapKey *Key);

extern void Rr_DestroyVulkanFramebuffers(VkImageView ImageView);

typedef struct Rr_RenderPassAttachment Rr_RenderPassAttachment;
struct Rr_RenderPassAttachment
{
    VkSampleCountFlagBits Samples;
    VkFormat Format;
    VkAttachmentLoadOp LoadOp;
    VkAttachmentStoreOp StoreOp;
};

typedef struct Rr_RenderPassMapKey Rr_RenderPassMapKey;
struct Rr_RenderPassMapKey
{
    uint32_t ColorAttachmentCount;
    uint32_t ResolveAttachmentCount;
    uint32_t DepthStencil;
    Rr_RenderPassAttachment Attachments[RR_MAX_COLOR_ATTACHMENTS * 2 + 1];
};

typedef struct Rr_RenderPassMap Rr_RenderPassMap;
struct Rr_RenderPassMap
{
    Rr_RenderPassMapKey Key;
    VkRenderPass Value;
    Rr_RenderPassMap *Children[4];
};

#define RR_HIVE_TYPE      Rr_RenderPassMap
#define RR_HIVE_TYPE_NAME RenderPassMap
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

typedef struct Rr_RenderPassStorage Rr_RenderPassStorage;
struct Rr_RenderPassStorage
{
    Rr_RenderPassMap *Map;
    Rr_RenderPassMapHive Hive;
};

extern VkRenderPass Rr_GetVulkanRenderPass(Rr_RenderPassMapKey *Key);

struct Rr_Renderer
{
    Rr_VulkanLoader Loader;
    Rr_Instance Instance;
    Rr_PhysicalDevice PhysicalDevice;
    Rr_Device Device;
    VkSurfaceKHR Surface;

    Rr_Swapchain Swapchain;
    RR_ARRAY(Rr_SwapchainImage) SwapchainImages;

    Rr_Queue GraphicsQueue;
    Rr_Queue TransferQueue;
    // Rr_Queue ComputeQueue;

    VmaAllocator Allocator;

    RR_ARRAY(VkSemaphore) Semaphores;
    RR_ARRAY(VkFence) Fences;

    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameIndex;  /* Current frame-in-flight index. */
    size_t FrameNumber; /* Total frames rendered. */

    Rr_FramebufferStorage FramebufferStorage;
    Rr_RenderPassStorage RenderPassStorage;

    RR_ARRAY(Rr_DescriptorSetLayout) DescriptorSetLayouts;

    Rr_ImmediateMode ImmediateMode;

    RR_ARRAY(Rr_PendingLoad) PendingLoads;

    Rr_Map *GlobalSync;
    RR_FREE_LIST(Rr_SyncState) SyncStates;

    RR_FREE_LIST(Rr_ImageViewStorage) ImageViewStorage;

    Rr_BufferHive Buffers;
    Rr_ImageHive Images;
    Rr_PipelineLayoutHive PipelineLayouts;
    Rr_ComputePipelineHive ComputePipelines;
    Rr_GraphicsPipelineHive GraphicsPipelines;
    Rr_SamplerHive Samplers;

    Rr_RendererObjectHive ReleasedObjects;

    Rr_Spinlock Lock;
    Rr_Arena *Arena;
};

extern void Rr_InitRenderer(void);

extern void Rr_WaitIdle(void);

extern void Rr_CleanupRenderer(void);

extern void Rr_SetSwapchainDirty(bool Dirty);

extern void Rr_NewFrame(void);

extern void Rr_DrawFrame(void);

extern VkCommandBuffer Rr_BeginImmediate(void);

extern void Rr_EndImmediate(void);

extern Rr_Frame *Rr_GetCurrentFrame(void);

extern bool Rr_IsUsingTransferQueue(void);

extern Rr_SyncState *Rr_GetSyncState(Rr_MapKey Key);

extern void Rr_ReturnSyncState(Rr_MapKey Key);

extern VkSemaphore Rr_GetVulkanSemaphore(void);

extern void Rr_ReturnVulkanSemaphore(VkSemaphore Semaphore);

extern VkFence Rr_GetVulkanFence(void);

extern void Rr_ReturnVulkanFence(VkFence Fence);

extern Rr_Renderer *gRenderer;
