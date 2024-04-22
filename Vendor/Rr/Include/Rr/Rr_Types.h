#pragma once

#include <cglm/vec4.h>

#include <SDL3/SDL_atomic.h>

#include "Rr_Vulkan.h"
#include "Rr_Buffer.h"
#include "Rr_Core.h"
#include "Rr_Image.h"
#include "Rr_Descriptor.h"
#include "Rr_Defines.h"

typedef struct SDL_Window SDL_Window;
typedef struct Rr_AppConfig Rr_AppConfig;

typedef struct Rr_Vertex
{
    vec3 Position;
    f32 TexCoordX;
    vec3 Normal;
    f32 TexCoordY;
    vec4 Color;
} Rr_Vertex;

typedef struct Rr_Frame
{
    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;
    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;
    Rr_DescriptorAllocator DescriptorAllocator;
    Rr_StagingBuffer StagingBuffer;
} Rr_Frame;

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
    Rr_SwapchainImage Images[MAX_SWAPCHAIN_IMAGE_COUNT];
    u32 ImageCount;
    VkExtent2D Extent;
    SDL_AtomicInt bResizePending;
} Rr_Swapchain;

typedef struct Rr_DeviceQueue
{
    VkQueue Handle;
    u32 FamilyIndex;
} Rr_DeviceQueue;

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
    b8 bInit;
} Rr_ImGui;

typedef struct Rr_ImmediateMode
{
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
} Rr_ImmediateMode;

typedef struct Rr_DrawTarget
{
    VkExtent2D ReferenceResolution;
    VkExtent2D ActiveResolution;
    i32 Scale;
    Rr_Image ColorImage;
    Rr_Image DepthImage;
} Rr_DrawTarget;

typedef struct Rr_Renderer
{
    /* Essentials */
    VkInstance Instance;
    Rr_PhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkSurfaceKHR Surface;
    Rr_Swapchain Swapchain;

    /* Queues */
    Rr_DeviceQueue GraphicsQueue;
    Rr_DeviceQueue TransferQueue;
    Rr_DeviceQueue ComputeQueue;

    VmaAllocator Allocator;

    /* Frame Data */
    void* PerFrameDatas;
    size_t PerFrameDataSize;
    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameNumber;

    Rr_DescriptorAllocator GlobalDescriptorAllocator;

    Rr_DrawTarget DrawTarget;
    Rr_ImGui ImGui;
    Rr_ImmediateMode ImmediateMode;

    VkSampler NearestSampler;

    /* Generic Pipeline Layout */
    VkDescriptorSetLayout GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT];
    VkPipelineLayout GenericPipelineLayout;
} Rr_Renderer;
