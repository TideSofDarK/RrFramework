#pragma once

#include <volk.h>

#include "Core.h"

#define MAX_SWAPCHAIN_IMAGE_COUNT 8

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
