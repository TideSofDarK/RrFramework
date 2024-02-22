#pragma once

#include "RendererTypes.hxx"

SRendererSwapchain EmptySwapchain()
{
    return {
        .Handle = VK_NULL_HANDLE,
        // .VkFormat = ,
        // .VkColorSpaceKHR ColorSpace,
        // .SVulkanImage * Images,
        // .VkExtent2D Extent,
    };
}

SRendererQueue EmptyQueue()
{
    return {
        .Handle = VK_NULL_HANDLE,
        .FamilyIndex = UINT32_MAX
    };
}

SPhysicalDevice EmptyPhysicalDevice()
{
    return {
        .Handle = VK_NULL_HANDLE,
        .Features = {},
        .MemoryProperties = {}
    };
}

VkFenceCreateInfo GetFenceCreateInfo(VkFenceCreateFlags Flags)
{
    VkFenceCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = Flags,
    };
    return Info;
}

VkSemaphoreCreateInfo GetSemaphoreCreateInfo(VkSemaphoreCreateFlags Flags)
{
    VkSemaphoreCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = Flags,
    };
    return Info;
}

VkCommandBufferBeginInfo GetCommandBufferBeginInfo(VkCommandBufferUsageFlags Flags)
{
    VkCommandBufferBeginInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = Flags,
        .pInheritanceInfo = nullptr,
    };
    return Info;
}

VkImageSubresourceRange GetImageSubresourceRange(VkImageAspectFlags AspectMask)
{
    VkImageSubresourceRange SubImage = {
        .aspectMask = AspectMask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };

    return SubImage;
}

void TransitionImage(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout CurrentLayout, VkImageLayout NewLayout)
{
    VkImageMemoryBarrier2 ImageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        .pNext = nullptr,

        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,

        .oldLayout = CurrentLayout,
        .newLayout = NewLayout,

        .image = Image,
        .subresourceRange = GetImageSubresourceRange((NewLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
    };

    VkDependencyInfo DepInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &ImageBarrier,
    };

    vkCmdPipelineBarrier2(CommandBuffer, &DepInfo);
}

VkSemaphoreSubmitInfo GetSemaphoreSubmitInfo(VkPipelineStageFlags2 StageMask, VkSemaphore Semaphore)
{
    VkSemaphoreSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = Semaphore,
        .value = 1,
        .stageMask = StageMask,
        .deviceIndex = 0,
    };

    return submitInfo;
}

VkCommandBufferSubmitInfo GetCommandBufferSubmitInfo(VkCommandBuffer CommandBuffer)
{
    VkCommandBufferSubmitInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = CommandBuffer,
        .deviceMask = 0,
    };

    return info;
}

VkSubmitInfo2 GetSubmitInfo(VkCommandBufferSubmitInfo* CommandBufferSubInfoPtr, VkSemaphoreSubmitInfo* SignalSemaphoreInfo,
    VkSemaphoreSubmitInfo* WaitSemaphoreInfo)
{
    VkSubmitInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .waitSemaphoreInfoCount = WaitSemaphoreInfo == nullptr ? 0u : 1u,
        .pWaitSemaphoreInfos = WaitSemaphoreInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = CommandBufferSubInfoPtr,
        .signalSemaphoreInfoCount = SignalSemaphoreInfo == nullptr ? 0u : 1u,
        .pSignalSemaphoreInfos = SignalSemaphoreInfo,
    };

    return info;
}
