#pragma once

#include <Rr/Rr_Renderer.h>

#include "Rr_Buffer.h"
#include "Rr_Descriptor.h"
#include "Rr_Graph.h"
#include "Rr_Pipeline.h"
#include "Rr_Text.h"
#include "Rr_Vulkan.h"

#include <SDL3/SDL_atomic.h>

#define RR_MAX_SWAPCHAIN_IMAGE_COUNT 8

typedef struct Rr_Swapchain Rr_Swapchain;
struct Rr_Swapchain
{
    VkSwapchainKHR Handle;
    Rr_TextureFormat Format;
    VkColorSpaceKHR ColorSpace;
    Rr_Image Images[RR_MAX_SWAPCHAIN_IMAGE_COUNT];
    uint32_t ImageCount;
    VkExtent2D Extent;
    SDL_AtomicInt ResizePending;
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
    Rr_Image *SwapchainImage;
    VkPipelineStageFlags SwapchainImageStage;

    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;

    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;

    Rr_DescriptorAllocator DescriptorAllocator;

    Rr_WriteBuffer StagingBuffer;
    Rr_WriteBuffer CommonBuffer;

    Rr_WriteBuffer PerDrawBuffer;
    VkDescriptorSet PerDrawDescriptor;

    Rr_Graph Graph;

    Rr_Arena *Arena;
};

typedef struct Rr_CachedFramebuffer Rr_CachedFramebuffer;
struct Rr_CachedFramebuffer
{
    VkFramebuffer Handle;
    uint32_t Hash;
};

typedef struct Rr_CachedRenderPass Rr_CachedRenderPass;
struct Rr_CachedRenderPass
{
    VkRenderPass Handle;
    uint32_t Hash;
};

typedef struct Rr_Renderer Rr_Renderer;
struct Rr_Renderer
{
    /* Vulkan Instance */

    VkInstance Instance;

    /* Presentation */

    VkSurfaceKHR Surface;
    Rr_Swapchain Swapchain;

    /* Device */

    Rr_PhysicalDevice PhysicalDevice;
    VkDevice Device;

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

    Rr_DescriptorAllocator GlobalDescriptorAllocator;

    /* Render Passes */

    RR_SLICE(Rr_CachedRenderPass) RenderPasses;
    RR_SLICE(Rr_CachedFramebuffer) Framebuffers;

    /* Immediate Command Pool/Buffer */

    Rr_ImmediateMode ImmediateMode;

    /* Null Textures */

    struct
    {
        Rr_Image *White;
        Rr_Image *Normal;
    } NullTextures;

    /* Retired Semaphores */

    RR_SLICE(struct Rr_RetiredSemaphore {
        VkSemaphore Semaphore;
        size_t FrameIndex;
    })
    RetiredSemaphoresSlice;

    /* Pending Loads */

    RR_SLICE(Rr_PendingLoad) PendingLoadsSlice;

    /* Texture Samplers */

    VkSampler NearestSampler;
    VkSampler LinearSampler;

    /* Text Rendering */

    Rr_TextPipeline TextPipeline;
    Rr_Font *BuiltinFont;

    /* Global Synchronization Map */
    Rr_Map *GlobalSync;
};

extern void Rr_InitRenderer(Rr_App *App);

extern void Rr_CleanupRenderer(Rr_App *App);

extern void Rr_PrepareFrame(Rr_App *App);

extern void Rr_Draw(Rr_App *App);

extern bool Rr_NewFrame(Rr_App *App, void *Window);

extern VkCommandBuffer Rr_BeginImmediate(Rr_Renderer *Renderer);

extern void Rr_EndImmediate(Rr_Renderer *Renderer);

extern Rr_Frame *Rr_GetCurrentFrame(Rr_Renderer *Renderer);

extern bool Rr_IsUsingTransferQueue(Rr_Renderer *Renderer);

extern VkDeviceSize Rr_GetUniformAlignment(Rr_Renderer *Renderer);

/* @TODO: Add format? */
typedef struct Rr_Attachment Rr_Attachment;
struct Rr_Attachment
{
    Rr_LoadOp LoadOp;
    Rr_StoreOp StoreOp;
    bool Depth;
};

typedef struct Rr_RenderPassInfo Rr_RenderPassInfo;
struct Rr_RenderPassInfo
{
    Rr_Attachment *Attachments;
    size_t AttachmentCount;
};

extern VkRenderPass Rr_GetRenderPass(Rr_App *App, Rr_RenderPassInfo *Info);

extern VkFramebuffer Rr_GetFramebuffer(
    Rr_App *App,
    VkRenderPass RenderPass,
    Rr_Image *Images,
    size_t ImageCount,
    VkExtent3D Extent);

extern VkFramebuffer Rr_GetFramebufferViews(
    Rr_App *App,
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent);

extern Rr_TextureFormat Rr_GetTextureFormat(VkFormat TextureFormat);

extern VkFormat Rr_GetVulkanTextureFormat(Rr_TextureFormat TextureFormat);
