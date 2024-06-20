#pragma once

#include "Rr/Rr_Defines.h"

#include <vk_mem_alloc.h>
#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include <SDL3/SDL_thread.h>

typedef struct Rr_Queue Rr_Queue;
struct Rr_Queue
{
    VkCommandPool TransientCommandPool;
    VkQueue Handle;
    Rr_U32 FamilyIndex;
    SDL_SpinLock Lock;
};

typedef struct Rr_PhysicalDevice Rr_PhysicalDevice;
struct Rr_PhysicalDevice
{
    VkPhysicalDevice Handle;

    VkPhysicalDeviceFeatures Features;
    VkPhysicalDeviceProperties2 Properties;
    VkPhysicalDeviceMemoryProperties MemoryProperties;
    VkPhysicalDeviceSubgroupProperties SubgroupProperties;
};

extern Rr_PhysicalDevice Rr_CreatePhysicalDevice(
    VkInstance Instance,
    VkSurfaceKHR Surface,
    Rr_U32 *OutGraphicsQueueFamilyIndex,
    Rr_U32 *OutTransferQueueFamilyIndex);

extern void Rr_InitDeviceAndQueues(
    VkPhysicalDevice PhysicalDevice,
    Rr_U32 GraphicsQueueFamilyIndex,
    Rr_U32 TransferQueueFamilyIndex,
    VkDevice *OutDevice,
    VkQueue *OutGraphicsQueue,
    VkQueue *OutTransferQueue);

extern void Rr_BlitDepthImage(
    VkCommandBuffer CommandBuffer,
    VkImage Source,
    VkImage Destination,
    VkExtent2D SrcSize,
    VkExtent2D DstSize);

extern void Rr_BlitColorImage(
    VkCommandBuffer CommandBuffer,
    VkImage Source,
    VkImage Destination,
    VkExtent2D SrcSize,
    VkExtent2D DstSize);

static inline VkExtent2D GetExtent2D(VkExtent3D Extent)
{
    return (VkExtent2D){ .height = Extent.height, .width = Extent.width };
}

static inline VkPipelineShaderStageCreateInfo GetShaderStageInfo(
    VkShaderStageFlagBits Stage,
    VkShaderModule Module)
{
    VkPipelineShaderStageCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .pName = "main",
        .stage = Stage,
        .module = Module
    };

    return Info;
}

static inline VkImageCreateInfo GetImageCreateInfo(
    VkFormat Format,
    VkImageUsageFlags UsageFlags,
    VkExtent3D Extent)
{
    VkImageCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = Format,
        .extent = Extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = UsageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    return Info;
}

static inline VkImageViewCreateInfo GetImageViewCreateInfo(
    VkFormat Format,
    VkImage Image,
    VkImageAspectFlags AspectFlags)
{
    VkImageViewCreateInfo Info = { .sType =
                                       VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                   .pNext = NULL,
                                   .image = Image,
                                   .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                   .format = Format,
                                   .subresourceRange = {
                                       .aspectMask = AspectFlags,
                                       .baseMipLevel = 0,
                                       .levelCount = 1,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1,
                                   } };

    return Info;
}

static inline VkFenceCreateInfo GetFenceCreateInfo(VkFenceCreateFlags Flags)
{
    VkFenceCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = Flags,
    };
    return Info;
}

static inline VkSemaphoreCreateInfo GetSemaphoreCreateInfo(
    VkSemaphoreCreateFlags Flags)
{
    VkSemaphoreCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = Flags,
    };
    return Info;
}

static inline VkCommandBufferBeginInfo GetCommandBufferBeginInfo(
    VkCommandBufferUsageFlags Flags)
{
    VkCommandBufferBeginInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = Flags,
        .pInheritanceInfo = NULL,
    };
    return Info;
}

static inline VkImageSubresourceRange GetImageSubresourceRange(
    VkImageAspectFlags AspectMask)
{
    VkImageSubresourceRange ImageSubresourceRange = {
        .aspectMask = AspectMask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };

    return ImageSubresourceRange;
}

static inline VkSemaphoreSubmitInfo GetSemaphoreSubmitInfo(
    VkPipelineStageFlags2 StageMask,
    VkSemaphore Semaphore)
{
    VkSemaphoreSubmitInfo Info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
        .semaphore = Semaphore,
        .value = 1,
        .stageMask = StageMask,
        .deviceIndex = 0,
    };

    return Info;
}

static inline VkCommandBufferSubmitInfo GetCommandBufferSubmitInfo(
    VkCommandBuffer CommandBuffer)
{
    VkCommandBufferSubmitInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = NULL,
        .commandBuffer = CommandBuffer,
        .deviceMask = 0,
    };

    return Info;
}

static inline VkCommandBufferAllocateInfo GetCommandBufferAllocateInfo(
    VkCommandPool CommandPool,
    Rr_U32 Count)
{
    VkCommandBufferAllocateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .commandPool = CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = Count,
    };

    return Info;
}
