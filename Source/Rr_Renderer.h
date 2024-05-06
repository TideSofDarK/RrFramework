#pragma once

#include <cglm/vec3.h>
#include <cglm/vec4.h>

#include <SDL3/SDL_atomic.h>

#include "Rr_Framework.h"
#include "Rr_Image.h"
#include "Rr_Text.h"
#include "Rr_Load.h"
#include "Rr_Vulkan.h"
#include "Rr_Descriptor.h"

typedef struct Rr_SwapchainImage
{
    VkExtent2D Extent;
    VkImage Handle;
    VkImageView View;
    VkSampler Sampler;
    VkDeviceMemory Memory;
} Rr_SwapchainImage;

typedef struct Rr_Swapchain
{
    VkSwapchainKHR Handle;
    VkFormat Format;
    VkColorSpaceKHR ColorSpace;
    Rr_SwapchainImage Images[RR_MAX_SWAPCHAIN_IMAGE_COUNT];
    u32 ImageCount;
    VkExtent2D Extent;
    SDL_AtomicInt bResizePending;
} Rr_Swapchain;

typedef struct Rr_PhysicalDevice
{
    VkPhysicalDevice Handle;

    VkPhysicalDeviceFeatures Features;
    VkPhysicalDeviceProperties2 Properties;
    VkPhysicalDeviceMemoryProperties MemoryProperties;
    VkPhysicalDeviceSubgroupProperties SubgroupProperties;
} Rr_PhysicalDevice;

typedef struct Rr_ImGui
{
    VkDescriptorPool DescriptorPool;
    bool bInitiated;
} Rr_ImGui;

typedef struct Rr_ImmediateMode
{
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
} Rr_ImmediateMode;

typedef struct Rr_DrawTarget
{
    Rr_Image* ColorImage;
    Rr_Image* DepthImage;
    VkFramebuffer Framebuffer;
} Rr_DrawTarget;

struct Rr_Frame
{
    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;
    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;
    Rr_DescriptorAllocator DescriptorAllocator;
    Rr_StagingBuffer* StagingBuffer;
    VkSemaphore* RetiredSemaphoresArray;
    Rr_DrawTarget DrawTarget;
};

struct Rr_Renderer
{
    /* Essentials */
    VkInstance Instance;
    Rr_PhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkSurfaceKHR Surface;
    Rr_Swapchain Swapchain;

    /* Queues */
    u32 bUnifiedQueue;

    struct
    {
        SDL_Mutex* Mutex;
        Rr_PendingLoad* PendingLoads;
        VkCommandPool TransientCommandPool;
        VkQueue Handle;
        u32 FamilyIndex;
    } UnifiedQueue;

    struct
    {
        VkCommandPool TransientCommandPool;
        VkQueue Handle;
        u32 FamilyIndex;
    } TransferQueue;

    VmaAllocator Allocator;

    /* Frames */
    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameNumber;

    Rr_DescriptorAllocator GlobalDescriptorAllocator;

    VkExtent2D ReferenceResolution;
    VkExtent2D ActiveResolution;
    i32 Scale;

    VkRenderPass RenderPass;

    Rr_ImGui ImGui;
    Rr_ImmediateMode ImmediateMode;

    /* Texture Samplers */
    VkSampler NearestSampler;
    VkSampler LinearSampler;

    /* Text Rendering */
    Rr_TextPipeline TextPipeline;
    Rr_Font BuiltinFont;

    /* Generic Pipeline Layout */
    VkDescriptorSetLayout GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT];
    VkPipelineLayout GenericPipelineLayout;
};

void Rr_InitRenderer(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_CleanupRenderer(Rr_App* App);
void Rr_Draw(Rr_App* App);
bool Rr_NewFrame(Rr_Renderer* Renderer, SDL_Window* Window);

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer);
void Rr_EndImmediate(Rr_Renderer* Renderer);

size_t Rr_GetCurrentFrameIndex(Rr_Renderer* Renderer);
Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer);
