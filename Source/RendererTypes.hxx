#pragma once

#define VK_NO_PROTOTYPES

#include <volk.h>
#include <vk_mem_alloc.h>

#include "Core.hxx"

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
    VkPhysicalDeviceSubgroupProperties SubgroupProperties;
} SPhysicalDevice;

typedef struct SAllocatedImage
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
} SAllocatedImage;

#define MAX_LAYOUT_BINDINGS 4

typedef struct SDescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding Bindings[MAX_LAYOUT_BINDINGS];
    u32 Count;
} SDescriptorLayoutBuilder;

typedef struct SDescriptorPoolSizeRatio
{
    VkDescriptorType Type;
    f32 Ratio;
} SDescriptorPoolSizeRatio;

typedef struct SDescriptorAllocator
{
    VkDescriptorPool Pool;
} SDescriptorAllocator;
