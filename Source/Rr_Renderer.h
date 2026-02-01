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

#include <Rr/Rr_Renderer.h>

#include "Rr_Buffer.h"
#include "Rr_Graph.h"
#include "Rr_Image.h"
#include "Rr_Pipeline.h"
#include "Rr_Profiler.h"
#include "Rr_Vulkan.h"

typedef struct Rr_SwapchainImage Rr_SwapchainImage;
struct Rr_SwapchainImage
{
    Rr_Image2D Container;
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
    bool RecreatePending;
    bool RecreateEventPending;
};

typedef struct Rr_CommandPools Rr_CommandPools;
struct Rr_CommandPools
{
    VkCommandPool Graphics;
    VkCommandPool Transfer;
    VkCommandPool Compute;
    Rr_CommandPools *Next;
};

typedef struct Rr_Frame Rr_Frame;
struct Rr_Frame
{
    VkCommandBuffer EarlyCommandBuffer;
    VkCommandBuffer LateCommandBuffer;
    VkSemaphore AcquireSemaphore;
    VkFence SubmitFence;
    VkQueryPool QueryPool;

    Rr_SwapchainImage *SwapchainImage;

    Rr_Graph *Graph;

    Rr_Profiler *Profiler;

    Rr_Arena *Arena;
};

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
    uint8_t ColorAttachmentCount;
    uint8_t ResolveAttachmentCount;
    uint8_t DepthStencil;
    uint8_t Padding;
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
#include "Rr_Hive.h"

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
    VkSampleCountFlags Samples;
    VkFormat Format;
    VkAttachmentLoadOp LoadOp;
    VkAttachmentStoreOp StoreOp;
};

typedef struct Rr_RenderPassMapKey Rr_RenderPassMapKey;
struct Rr_RenderPassMapKey
{
    uint8_t ColorAttachmentCount;
    uint8_t ResolveAttachmentCount;
    uint8_t ResolveMask;
    uint8_t DepthStencil;
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
#include "Rr_Hive.h"

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

    Rr_Queue MainQueue;
    Rr_Queue DedicatedTransferQueue;
    Rr_Queue AsyncComputeQueue;

    /* TODO: Make sure this doesn't need synchronization in general case. */
    VmaAllocator Allocator;

    RR_ARRAY(VkSemaphore) Semaphores;
    Rr_Spinlock SemaphoresLock;

    RR_ARRAY(VkFence) Fences;
    Rr_Spinlock FencesLock;

    Rr_CommandPools *FreeCommandPools;
    Rr_Spinlock CommandPoolsLock;

    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameIndex;  /* Current frame-in-flight index. */
    size_t FrameNumber; /* Total frames rendered. */
    double LastFrameMS;

    VkDescriptorPool EmptyDescriptorPool;
    VkDescriptorSet EmptyDescriptorSet;

    Rr_BufferHive Buffers;
    Rr_Spinlock BuffersLock;
    Rr_HandleHive ReleasedBuffers;
    Rr_Spinlock ReleasedBuffersLock;

    Rr_ImageHive Images;
    Rr_Spinlock ImagesLock;
    Rr_HandleHive ReleasedImages;
    Rr_Spinlock ReleasedImagesLock;
    RR_FREE_LIST(Rr_ImageViewStorage) ImageViewStorage;
    Rr_Spinlock ImageViewStorageLock;
    Rr_FramebufferStorage FramebufferStorage;
    Rr_Spinlock FramebufferStorageLock;

    Rr_PipelineLayoutHive PipelineLayouts;
    Rr_Spinlock PipelineLayoutsLock;
    Rr_HandleHive ReleasedPipelineLayouts;
    Rr_Spinlock ReleasedPipelineLayoutsLock;

    Rr_ComputePipelineHive ComputePipelines;
    Rr_Spinlock ComputePipelinesLock;
    Rr_HandleHive ReleasedComputePipelines;
    Rr_Spinlock ReleasedComputePipelinesLock;

    Rr_GraphicsPipelineHive GraphicsPipelines;
    Rr_Spinlock GraphicsPipelinesLock;
    Rr_HandleHive ReleasedGraphicsPipelines;
    Rr_Spinlock ReleasedGraphicsPipelinesLock;

    Rr_SamplerHive Samplers;
    Rr_Spinlock SamplersLock;
    Rr_HandleHive ReleasedSamplers;
    Rr_Spinlock ReleasedSamplersLock;

    Rr_DescriptorSetLayoutStorage DescriptorSetLayoutStorage;
    Rr_Spinlock DescriptorSetLayoutStorageLock;

    Rr_RenderPassStorage RenderPassStorage;
    Rr_Spinlock RenderPassStorageLock;

    Rr_DescriptorPoolList *DescriptorPoolList;
    Rr_Spinlock DescriptorPoolListLock;
    uint32_t DescriptorPoolListCount;

    Rr_Arena *Arena;
    Rr_Spinlock Lock;
};

extern void Rr_InitRenderer(const char *Title);

extern void Rr_WaitIdle(void);

extern void Rr_CleanupRenderer(void);

extern void Rr_SetSwapchainDirty(bool Dirty);

extern void Rr_NewFrame(void);

extern void Rr_DrawFrame(void);

extern Rr_Queue *Rr_GetQueue(Rr_QueueType QueueType);

extern Rr_Frame *Rr_GetPreviousFrame(void);

extern Rr_Frame *Rr_GetCurrentFrame(void);

extern VkSemaphore Rr_AcquireVulkanSemaphore(void);

extern void Rr_ReleaseVulkanSemaphore(VkSemaphore Semaphore);

extern VkFence Rr_AcquireVulkanFence(void);

extern void Rr_ReleaseVulkanFence(VkFence Fence);

extern Rr_CommandPools *Rr_AcquireCommandPools(void);

extern void Rr_ReleaseCommandPools(void);

extern void Rr_ConsumeNextObjectName(char Dst[RR_MAX_OBJECT_NAME_LENGTH]);

extern void Rr_SetVulkanObjectName(
    VkObjectType ObjectType,
    uint64_t Handle,
    const char *Name);

extern void Rr_BeginVulkanCommandBufferLabel(
    VkCommandBuffer CommandBuffer,
    const char *Name);

extern void Rr_EndVulkanCommandBufferLabel(VkCommandBuffer CommandBuffer);

extern void Rr_PrintDestroyMessage(
    const char *Type,
    const char *Name,
    void *Address);

extern Rr_Renderer *gRenderer;
