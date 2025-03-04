#pragma once

#include <Rr/Rr_Renderer.h>

#include "Rr_Graph.h"
#include "Rr_Load.h"
#include "Rr_Pipeline.h"
#include "Rr_Text.h"
#include "Rr_Vulkan.h"

#include <SDL3/SDL_atomic.h>

typedef struct Rr_SwapchainImage Rr_SwapchainImage;
struct Rr_SwapchainImage
{
    VkImage Handle;
    VkImageView View;
    VkFramebuffer Framebuffer;
};

typedef struct Rr_Swapchain Rr_Swapchain;
struct Rr_Swapchain
{
    VkSwapchainKHR Handle;
    VkFormat Format;
    VkColorSpaceKHR ColorSpace;
    VkExtent3D Extent;
    SDL_AtomicInt ResizePending;
    RR_SLICE(Rr_SwapchainImage) Images;
};

typedef struct Rr_ImmediateMode Rr_ImmediateMode;
struct Rr_ImmediateMode
{
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
};

typedef struct Rr_Frame Rr_Frame;
struct Rr_Frame
{
    VkFramebuffer SwapchainFramebuffer;
    Rr_Image *VirtualSwapchainImage;

    VkCommandPool CommandPool;
    VkCommandBuffer EarlyCommandBuffer;
    VkCommandBuffer LateCommandBuffer;
    size_t CommandBufferIndex;

    VkSemaphore SwapchainSemaphore;
    VkSemaphore EarlySemaphore;
    VkSemaphore LateSemaphore;
    VkFence RenderFence;

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
    /* Vulkan Core */

    Rr_VulkanLoader Loader;
    Rr_Instance Instance;
    Rr_PhysicalDevice PhysicalDevice;
    Rr_Device Device;
    VkSurfaceKHR Surface;

    /* Presentation */

    Rr_Swapchain Swapchain;
    Rr_GraphicsPipeline *PresentPipeline;
    Rr_PipelineLayout *PresentLayout;
    VkRenderPass PresentRenderPass;

    /* Queues */

    Rr_Queue GraphicsQueue;
    Rr_Queue TransferQueue;
    // Rr_Queue ComputeQueue;

    /* Vulkan Memory Allocator */

    VmaAllocator Allocator;

    /* Frames */

    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameNumber;
    size_t CurrentFrameIndex;

    /* Hashed structures. */

    RR_SLICE(Rr_RenderPass) RenderPasses;
    RR_SLICE(Rr_Framebuffer) Framebuffers;
    RR_SLICE(Rr_DescriptorSetLayout) DescriptorSetLayouts;

    /* Immediate Command Pool/Buffer */

    Rr_ImmediateMode ImmediateMode;

    /* Null Textures */

    // struct
    // {
    //     Rr_Image *White;
    //     Rr_Image *Normal;
    // } NullTextures;

    /* Pending Loads */

    RR_SLICE(Rr_PendingLoad) PendingLoadsSlice;

    /* Text Rendering */

    Rr_TextPipeline TextPipeline;
    Rr_Font *BuiltinFont;

    /* Global Synchronization Map */

    Rr_Map *GlobalSync;

    /* Storage */

    RR_FREE_LIST(Rr_Buffer) Buffers;
    // RR_FREE_LIST(Rr_Primitive) Primitives;
    // RR_FREE_LIST(Rr_StaticMesh) StaticMeshes;
    // RR_FREE_LIST(Rr_SkeletalMesh) SkeletalMeshes;
    RR_FREE_LIST(Rr_Image) Images;
    RR_FREE_LIST(Rr_Font) Fonts;
    RR_FREE_LIST(Rr_PipelineLayout) PipelineLayouts;
    RR_FREE_LIST(Rr_ComputePipeline) ComputePipelines;
    RR_FREE_LIST(Rr_GraphicsPipeline) GraphicsPipelines;
    RR_FREE_LIST(Rr_Sampler) Samplers;
    RR_FREE_LIST(Rr_SyncState) SyncStates;

    /* Arena */

    Rr_Arena *Arena;
};

extern Rr_Renderer *Rr_CreateRenderer(Rr_App *App);

extern void Rr_DestroyRenderer(Rr_App *App, Rr_Renderer *Renderer);

extern void Rr_PrepareFrame(Rr_App *App);

extern void Rr_DrawFrame(Rr_App *App);

extern bool Rr_NewFrame(Rr_App *App, void *Window);

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

extern VkRenderPass Rr_GetRenderPass(
    Rr_Renderer *Renderer,
    Rr_RenderPassInfo *Info);

extern VkFramebuffer Rr_GetFramebuffer(
    Rr_Renderer *Renderer,
    VkRenderPass RenderPass,
    Rr_Image *Images,
    size_t ImageCount,
    VkExtent3D Extent);

extern VkFramebuffer Rr_GetFramebufferViews(
    Rr_Renderer *Renderer,
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent);

extern Rr_SyncState *Rr_GetSynchronizationState(
    Rr_Renderer *Renderer,
    Rr_MapKey Key);

extern void Rr_ReturnSynchronizationState(Rr_Renderer *Renderer, Rr_MapKey Key);
