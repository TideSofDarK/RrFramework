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

typedef struct Rr_SwapchainImage Rr_SwapchainImage;
struct Rr_SwapchainImage
{
    VkImage Handle;
    VkImageView View;
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
    Rr_AtomicInt RecreatePending;
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
    size_t CommandBufferIndex;

    VkSemaphore AcquireSemaphore;
    VkFence SubmitFence;

    Rr_DescriptorAllocator *DescriptorAllocator;

    Rr_Graph *Graph;

    RR_ARRAY(Rr_Image2D *) UsedImages;
    RR_ARRAY(Rr_Buffer *) UsedBuffers;

    Rr_Arena *Arena;
};

typedef struct Rr_CachedFramebuffer Rr_Framebuffer;
struct Rr_CachedFramebuffer
{
    VkFramebuffer Handle;
    uint32_t Hash;
};

typedef struct Rr_CachedRenderPass Rr_RenderPass;
struct Rr_CachedRenderPass
{
    VkRenderPass Handle;
    uint32_t Hash;
};

#define RR_HIVE_TYPE      Rr_Buffer
#define RR_HIVE_TYPE_NAME Buffer
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

#define RR_HIVE_TYPE      Rr_Image
#define RR_HIVE_TYPE_NAME Image
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

#define RR_HIVE_TYPE      Rr_PipelineLayout
#define RR_HIVE_TYPE_NAME PipelineLayout
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

#define RR_HIVE_TYPE      Rr_ComputePipeline
#define RR_HIVE_TYPE_NAME ComputePipeline
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

#define RR_HIVE_TYPE      Rr_GraphicsPipeline
#define RR_HIVE_TYPE_NAME GraphicsPipeline
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

#define RR_HIVE_TYPE      Rr_Sampler
#define RR_HIVE_TYPE_NAME Sampler
#define RR_HIVE_PREFIX    Rr_
#include <Rr/Rr_Hive.h>

struct Rr_Renderer
{
    void *Window;

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

    RR_ARRAY(Rr_RenderPass) RenderPasses;
    RR_ARRAY(Rr_Framebuffer) Framebuffers;
    RR_ARRAY(Rr_DescriptorSetLayout) DescriptorSetLayouts;

    Rr_ImmediateMode ImmediateMode;

    RR_ARRAY(Rr_PendingLoad) PendingLoadsArray;
    Rr_Map *GlobalSync;

    Rr_BufferHive BufferHive;
    Rr_ImageHive ImageHive;
    Rr_PipelineLayoutHive PipelineLayoutHive;
    Rr_ComputePipelineHive ComputePipelineHive;
    Rr_GraphicsPipelineHive GraphicsPipelineHive;
    Rr_SamplerHive SamplerHive;
    RR_FREE_LIST(Rr_SyncState) SyncStates;

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

typedef struct Rr_RenderPassAttachment Rr_RenderPassAttachment;
struct Rr_RenderPassAttachment
{
    VkFormat Format;
    Rr_LoadOp LoadOp;
    Rr_StoreOp StoreOp;
};

typedef struct Rr_RenderPassInfo Rr_RenderPassInfo;
struct Rr_RenderPassInfo
{
    Rr_RenderPassAttachment *Attachments;
    size_t AttachmentCount;
};

extern VkRenderPass Rr_GetVulkanRenderPass(Rr_RenderPassInfo *Info);

extern VkFramebuffer Rr_GetVulkanFramebuffer(
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent);

extern Rr_SyncState *Rr_GetSyncState(Rr_MapKey Key);

extern void Rr_ReturnSyncState(Rr_MapKey Key);

extern VkSemaphore Rr_GetVulkanSemaphore(void);

extern void Rr_ReturnVulkanSemaphore(VkSemaphore Semaphore);

extern VkFence Rr_GetVulkanFence(void);

extern void Rr_ReturnVulkanFence(VkFence Fence);

extern Rr_Renderer *gRenderer;
