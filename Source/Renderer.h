#pragma once

#define VK_NO_PROTOTYPES

#include <assert.h>
#include <vk_mem_alloc.h>

#include "Core.h"

#define VK_ASSERT(expr)             \
    {                               \
        assert(expr == VK_SUCCESS); \
    }

typedef struct SApp SApp;

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
    SVulkanImage* Images;
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
    SRendererQueue GraphicsQueue;
    SRendererQueue TransferQueue;
    SRendererQueue ComputeQueue;
    VmaAllocator Allocator;
    VkSurfaceKHR Surface;
    SRendererSwapchain Swapchain;
    VkDebugUtilsMessengerEXT Messenger;
} SRenderer;

void RendererInit(SApp* App);
void RendererCleanup(SApp* App);
