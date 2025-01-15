#pragma once

#include "Rr_Buffer.h"
#include "Rr_Descriptor.h"
#include "Rr_Graph.h"
#include "Rr_Pipeline.h"
#include "Rr_Vulkan.h"
#include "Rr_Text.h"

#include <SDL3/SDL_atomic.h>

typedef struct Rr_SwapchainImage Rr_SwapchainImage;
struct Rr_SwapchainImage
{
    VkExtent2D Extent;
    VkImage Handle;
    VkImageView View;
    VkSampler Sampler;
    VkDeviceMemory Memory;
};

typedef struct Rr_Swapchain Rr_Swapchain;
struct Rr_Swapchain
{
    VkSwapchainKHR Handle;
    VkFormat Format;
    VkColorSpaceKHR ColorSpace;
    Rr_SwapchainImage Images[RR_MAX_SWAPCHAIN_IMAGE_COUNT];
    uint32_t ImageCount;
    VkExtent2D Extent;
    SDL_AtomicInt bResizePending;
};

typedef struct Rr_ImGui Rr_ImGui;
struct Rr_ImGui
{
    VkDescriptorPool DescriptorPool;
    Rr_Bool IsInitialized;
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
    VkImage CurrentSwapchainImage;

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

    Rr_Arena Arena;
};

typedef struct Rr_Renderer Rr_Renderer;
struct Rr_Renderer
{
    /* Vulkan Instance */
    VkInstance Instance;

    /* Presentation */
    VkSurfaceKHR Surface;
    Rr_Swapchain Swapchain;
    VkExtent2D SwapchainSize;

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
    struct
    {
        VkRenderPass ColorDepth;
        VkRenderPass ColorDepthLoad;
        VkRenderPass Depth;
        VkRenderPass DepthLoad;
    } RenderPasses;

    /* Dear ImGui */
    Rr_ImGui ImGui;

    /* Immediate Command Pool/Buffer */
    Rr_ImmediateMode ImmediateMode;

    /* Null Textures */
    struct
    {
        Rr_Image *White;
        Rr_Image *Normal;
    } NullTextures;

    /* Retired Semaphores */
    RR_SLICE_TYPE(struct Rr_RetiredSemaphore {
        VkSemaphore Semaphore;
        size_t FrameIndex;
    })
    RetiredSemaphoresSlice;

    /* Pending Loads */
    RR_SLICE_TYPE(Rr_PendingLoad) PendingLoadsSlice;

    /* Main Draw Target */
    Rr_DrawTarget *DrawTarget;

    /* Texture Samplers */
    VkSampler NearestSampler;
    VkSampler LinearSampler;

    /* Text Rendering */
    Rr_TextPipeline TextPipeline;
    Rr_Font *BuiltinFont;

    /* Generic Pipeline Layout */
    VkDescriptorSetLayout GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT];
    VkPipelineLayout GenericPipelineLayout;
};

extern void Rr_InitRenderer(Rr_App *App);

extern void Rr_InitImGui(Rr_App *App);

extern void Rr_CleanupRenderer(Rr_App *App);

extern void Rr_PrepareFrame(Rr_App *App);

extern void Rr_Draw(Rr_App *App);

extern Rr_Bool Rr_NewFrame(Rr_App *App, void *Window);

extern VkCommandBuffer Rr_BeginImmediate(Rr_Renderer *Renderer);

extern void Rr_EndImmediate(Rr_Renderer *Renderer);

extern Rr_Frame *Rr_GetCurrentFrame(Rr_Renderer *Renderer);

extern Rr_Bool Rr_IsUsingTransferQueue(Rr_Renderer *Renderer);

extern VkDeviceSize Rr_GetUniformAlignment(Rr_Renderer *Renderer);
