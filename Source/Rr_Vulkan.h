#pragma once

#include <Rr/Rr_Math.h>
#include <Rr/Rr_Memory.h>
#include <Rr/Rr_Platform.h>
#include <Rr/Rr_Renderer.h>

#include <vma/vk_mem_alloc.h>
#include <volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>

typedef struct Rr_Queue Rr_Queue;
struct Rr_Queue
{
    VkCommandPool TransientCommandPool;
    VkQueue Handle;
    uint32_t FamilyIndex;
    Rr_SpinLock Lock;
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

extern Rr_PhysicalDevice Rr_SelectPhysicalDevice(
    VkInstance Instance,
    VkSurfaceKHR Surface,
    uint32_t *OutGraphicsQueueFamilyIndex,
    uint32_t *OutTransferQueueFamilyIndex,
    Rr_Arena *Arena);

extern void Rr_InitDeviceAndQueues(
    VkPhysicalDevice PhysicalDevice,
    uint32_t GraphicsQueueFamilyIndex,
    uint32_t TransferQueueFamilyIndex,
    VkDevice *OutDevice,
    VkQueue *OutGraphicsQueue,
    VkQueue *OutTransferQueue);

extern void Rr_BlitColorImage(
    VkCommandBuffer CommandBuffer,
    VkImage Source,
    VkImage Destination,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    VkImageAspectFlags AspectMask);

static VkExtent2D Rr_ToExtent2D(VkExtent3D *Extent)
{
    return (VkExtent2D){ .height = Extent->height, .width = Extent->width };
}

static VkExtent3D Rr_ToVulkanExtent3D(Rr_IntVec3 *Extent)
{
    return (VkExtent3D){ .width = Extent->Width, .height = Extent->Height, .depth = Extent->Depth };
}

static VkSemaphoreCreateInfo Rr_GetSemaphoreCreateInfo(VkSemaphoreCreateFlags Flags)
{
    VkSemaphoreCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = Flags,
    };
    return Info;
}

static VkImageSubresourceRange Rr_GetImageSubresourceRange(VkImageAspectFlags AspectMask)
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

static VkCommandBufferSubmitInfo GetCommandBufferSubmitInfo(VkCommandBuffer CommandBuffer)
{
    VkCommandBufferSubmitInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = NULL,
        .commandBuffer = CommandBuffer,
        .deviceMask = 0,
    };

    return Info;
}

static VkCommandBufferAllocateInfo GetCommandBufferAllocateInfo(VkCommandPool CommandPool, uint32_t Count)
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

static VkShaderStageFlags Rr_GetVulkanShaderStageFlags(Rr_ShaderStage ShaderStage)
{
    VkShaderStageFlags ShaderStageFlags = 0;
    if((ShaderStage & RR_SHADER_STAGE_VERTEX_BIT) != 0)
    {
        ShaderStageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if((ShaderStage & RR_SHADER_STAGE_FRAGMENT_BIT) != 0)
    {
        ShaderStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    return ShaderStageFlags;
}

static VkCompareOp Rr_GetVulkanCompareOp(Rr_CompareOp CompareOp)
{
    switch(CompareOp)
    {
        case RR_COMPARE_OP_NEVER:
            return VK_COMPARE_OP_NEVER;
        case RR_COMPARE_OP_LESS:
            return VK_COMPARE_OP_LESS;
        case RR_COMPARE_OP_EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case RR_COMPARE_OP_LESS_OR_EQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case RR_COMPARE_OP_GREATER:
            return VK_COMPARE_OP_GREATER;
        case RR_COMPARE_OP_NOT_EQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        case RR_COMPARE_OP_GREATER_OR_EQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case RR_COMPARE_OP_ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        default:
            return 0;
    }
}
