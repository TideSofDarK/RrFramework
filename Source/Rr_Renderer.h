#pragma once

#include <Rr/Rr_Renderer.h>

#include "Rr_Graph.h"
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
    Rr_Image *VirtualSwapchainImage;

    VkCommandPool CommandPool;
    VkCommandBuffer EarlyCommandBuffer;
    VkCommandBuffer LateCommandBuffer;
    size_t CommandBufferIndex;

    VkSemaphore AcquireSemaphore;
    VkFence SubmitFence;

    Rr_DescriptorAllocator DescriptorAllocator;

    Rr_Graph *Graph;

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
    size_t FrameNumber;
    size_t CurrentFrameIndex;

    RR_ARRAY(Rr_RenderPass) RenderPasses;
    RR_ARRAY(Rr_Framebuffer) Framebuffers;
    RR_ARRAY(Rr_DescriptorSetLayout) DescriptorSetLayouts;

    Rr_ImmediateMode ImmediateMode;

    RR_ARRAY(Rr_PendingLoad) PendingLoadsArray;

    Rr_Map *GlobalSync;

    RR_FREE_LIST(Rr_Buffer) Buffers;
    RR_FREE_LIST(Rr_Image) Images;
    RR_FREE_LIST(Rr_PipelineLayout) PipelineLayouts;
    RR_FREE_LIST(Rr_ComputePipeline) ComputePipelines;
    RR_FREE_LIST(Rr_GraphicsPipeline) GraphicsPipelines;
    RR_FREE_LIST(Rr_Sampler) Samplers;
    RR_FREE_LIST(Rr_SyncState) SyncStates;

    Rr_Arena *Arena;
};

extern Rr_Renderer *Rr_CreateRenderer(void);

extern void Rr_WaitIdle(Rr_Renderer *Renderer);

extern void Rr_DestroyRenderer(Rr_Renderer *Renderer);

extern void Rr_SetSwapchainDirty(Rr_Renderer *Renderer, bool Dirty);

extern void Rr_NewFrame(void);

extern void Rr_DrawFrame(void);

extern VkCommandBuffer Rr_BeginImmediate(Rr_Renderer *Renderer);

extern void Rr_EndImmediate(Rr_Renderer *Renderer);

extern Rr_Frame *Rr_GetCurrentFrame(Rr_Renderer *Renderer);

extern bool Rr_IsUsingTransferQueue(Rr_Renderer *Renderer);

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

extern VkRenderPass Rr_GetVulkanRenderPass(
    Rr_Renderer *Renderer,
    Rr_RenderPassInfo *Info);

extern VkFramebuffer Rr_GetVulkanFramebuffer(
    Rr_Renderer *Renderer,
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent);

extern Rr_SyncState *Rr_GetSyncState(Rr_Renderer *Renderer, Rr_MapKey Key);

extern void Rr_ReturnSyncState(Rr_Renderer *Renderer, Rr_MapKey Key);

extern VkSemaphore Rr_GetVulkanSemaphore(Rr_Renderer *Renderer);

extern void Rr_ReturnVulkanSemaphore(
    Rr_Renderer *Renderer,
    VkSemaphore Semaphore);

extern VkFence Rr_GetVulkanFence(Rr_Renderer *Renderer);

extern void Rr_ReturnVulkanFence(Rr_Renderer *Renderer, VkFence Fence);
