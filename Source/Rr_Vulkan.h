#pragma once

#define VK_NO_PROTOTYPES

#include <vk_mem_alloc.h>
#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#include "Rr_App.h"

void Rr_InitRenderer(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_CleanupRenderer(Rr_App* App);
void Rr_Draw(Rr_App* App);
bool Rr_NewFrame(Rr_Renderer* Renderer, void* Window);

size_t Rr_GetCurrentFrameIndex(Rr_Renderer* Renderer);
Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer);

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer);
void Rr_EndImmediate(Rr_Renderer* Renderer);

VkPipeline Rr_BuildPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    VkPipelineLayout PipelineLayout);

/**
 * Various Vulkan utilities
 */
static inline VkExtent2D GetExtent2D(VkExtent3D Extent)
{
    return (VkExtent2D){ .height = Extent.height, .width = Extent.width };
}

static inline VkPipelineShaderStageCreateInfo GetShaderStageInfo(VkShaderStageFlagBits Stage, VkShaderModule Module)
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

static inline VkRenderingAttachmentInfo GetRenderingAttachmentInfo_Color(VkImageView View, VkImageLayout Layout, VkClearValue* InClearValue)
{
    VkRenderingAttachmentInfo Info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = View,
        .imageLayout = Layout,
        .loadOp = InClearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = InClearValue ? *InClearValue : (VkClearValue){ .color = { { 0.0f, 0.0f, 0.0f, 1.0f } } }
    };

    return Info;
}

static inline VkRenderingAttachmentInfo GetRenderingAttachmentInfo_Depth(
    VkImageView View,
    VkImageLayout Layout,
    bool bClear)
{
    VkRenderingAttachmentInfo Info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = View,
        .imageLayout = Layout,
        .loadOp = bClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = (VkClearValue){ .depthStencil.depth = 1.0f }
    };

    return Info;
}

static inline VkRenderingInfo GetRenderingInfo(
    VkExtent2D RenderExtent,
    VkRenderingAttachmentInfo* ColorAttachment,
    VkRenderingAttachmentInfo* DepthAttachment)
{
    VkRenderingInfo Info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = NULL,
        .renderArea = (VkRect2D){ .offset = (VkOffset2D){ 0, 0 }, .extent = RenderExtent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = ColorAttachment,
        .pDepthAttachment = DepthAttachment,
        .pStencilAttachment = NULL,
    };

    return Info;
}

static inline VkImageCreateInfo GetImageCreateInfo(VkFormat Format, VkImageUsageFlags UsageFlags, VkExtent3D Extent)
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

static inline VkImageViewCreateInfo GetImageViewCreateInfo(VkFormat Format, VkImage Image, VkImageAspectFlags AspectFlags)
{
    VkImageViewCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
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
        }
    };

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

static inline VkSemaphoreCreateInfo GetSemaphoreCreateInfo(VkSemaphoreCreateFlags Flags)
{
    VkSemaphoreCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = Flags,
    };
    return Info;
}

static inline VkCommandBufferBeginInfo GetCommandBufferBeginInfo(VkCommandBufferUsageFlags Flags)
{
    VkCommandBufferBeginInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = Flags,
        .pInheritanceInfo = NULL,
    };
    return Info;
}

static inline VkImageSubresourceRange GetImageSubresourceRange(VkImageAspectFlags AspectMask)
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

static inline VkSemaphoreSubmitInfo GetSemaphoreSubmitInfo(VkPipelineStageFlags2 StageMask, VkSemaphore Semaphore)
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

static inline VkCommandBufferSubmitInfo GetCommandBufferSubmitInfo(VkCommandBuffer CommandBuffer)
{
    VkCommandBufferSubmitInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = NULL,
        .commandBuffer = CommandBuffer,
        .deviceMask = 0,
    };

    return Info;
}

static inline VkCommandBufferAllocateInfo GetCommandBufferAllocateInfo(VkCommandPool CommandPool, u32 Count)
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

static inline VkSubmitInfo2 GetSubmitInfo(VkCommandBufferSubmitInfo* CommandBufferSubInfoPtr, VkSemaphoreSubmitInfo* SignalSemaphoreInfo,
                                          VkSemaphoreSubmitInfo* WaitSemaphoreInfo)
{
    VkSubmitInfo2 Info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = NULL,
        .waitSemaphoreInfoCount = WaitSemaphoreInfo == NULL ? 0u : 1u,
        .pWaitSemaphoreInfos = WaitSemaphoreInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = CommandBufferSubInfoPtr,
        .signalSemaphoreInfoCount = SignalSemaphoreInfo == NULL ? 0u : 1u,
        .pSignalSemaphoreInfos = SignalSemaphoreInfo,
    };

    return Info;
}
