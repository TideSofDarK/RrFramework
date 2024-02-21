#pragma once

#define VK_NO_PROTOTYPES

#include <assert.h>
#include <vk_mem_alloc.h>

#include "Core.h"

#define VK_ASSERT(expr)             \
    {                               \
        assert(expr == VK_SUCCESS); \
    }

#define MAX_SWAPCHAIN_IMAGE_COUNT 8
#define FRAME_OVERLAP 2

typedef struct SApp SApp;

typedef struct SFrameData
{
    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;
    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;
} SFrameData;

typedef struct SVulkanImage
{
    VkImage Handle;
    VkImageView View;
    VkSampler Sampler;
    VkDeviceMemory Memory;
} SVulkanImage;

typedef struct SRendererSwapchain
{
    VkSwapchainKHR Handle;
    VkFormat Format;
    VkColorSpaceKHR ColorSpace;
    SVulkanImage Images[MAX_SWAPCHAIN_IMAGE_COUNT];
    u32 ImageCount;
    VkExtent2D Extent;
} SRendererSwapchain;

typedef struct SRendererQueue
{
    VkQueue Handle;
    u32 FamilyIndex;
} SRendererQueue;

typedef struct SPhysicalDevice
{
    VkPhysicalDevice Handle;

    VkPhysicalDeviceFeatures Features;
    VkPhysicalDeviceMemoryProperties MemoryProperties;
} SPhysicalDevice;

typedef struct SRenderer
{
    VkInstance Instance;
    SPhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkSurfaceKHR Surface;
    SRendererSwapchain Swapchain;

    SRendererQueue GraphicsQueue;
    SRendererQueue TransferQueue;
    SRendererQueue ComputeQueue;

    VmaAllocator Allocator;

    VkDebugUtilsMessengerEXT Messenger;

    u32 CommandBufferCount;

    SFrameData Frames[FRAME_OVERLAP];
    u64 FrameNumber;
} SRenderer;

void RendererInit(SApp* App);
void RendererCleanup(SRenderer* Renderer);
void RendererDraw(SRenderer* Renderer);
