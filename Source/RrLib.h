#pragma once

#include <stdlib.h>
#include <stdio.h>

#include "RrTypes.h"

#include <SDL_log.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#define VK_ASSERT(Expr)                                                                                                              \
    {                                                                                                                                \
        VkResult Result = Expr;                                                                                                      \
        if (Result != VK_SUCCESS)                                                                                                    \
        {                                                                                                                            \
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Assertion " #Expr " == VK_SUCCESS failed! Result is %s", string_VkResult(Result)); \
            abort();                                                                                                                 \
        }                                                                                                                            \
    }

/* =======================
 * Struct Creation Helpers
 * ======================= */

static VkRenderingAttachmentInfo GetRenderingAttachmentInfo(VkImageView View, VkClearValue* InClearValue, VkImageLayout Layout)
{
    VkRenderingAttachmentInfo Info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = View,
        .imageLayout = Layout,
        .loadOp = InClearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = InClearValue ? *InClearValue : (VkClearValue){ 0 }
    };

    return Info;
}

static VkRenderingInfo GetRenderingInfo(VkExtent2D RenderExtent, VkRenderingAttachmentInfo* ColorAttachment,
    VkRenderingAttachmentInfo* DepthAttachment)
{
    VkRenderingInfo Info = { 0 };
    Info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    Info.pNext = NULL;

    Info.renderArea = (VkRect2D){ .offset = (VkOffset2D){ 0, 0 }, .extent = RenderExtent };
    Info.layerCount = 1;
    Info.colorAttachmentCount = 1;
    Info.pColorAttachments = ColorAttachment;
    Info.pDepthAttachment = DepthAttachment;
    Info.pStencilAttachment = NULL;

    return Info;
}

static VkImageCreateInfo GetImageCreateInfo(VkFormat Format, VkImageUsageFlags UsageFlags, VkExtent3D Extent)
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
    };
    return Info;
}

static VkImageViewCreateInfo GetImageViewCreateInfo(VkFormat Format, VkImage Image, VkImageAspectFlags AspectFlags)
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

static VkFenceCreateInfo GetFenceCreateInfo(VkFenceCreateFlags Flags)
{
    VkFenceCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = Flags,
    };
    return Info;
}

static VkSemaphoreCreateInfo GetSemaphoreCreateInfo(VkSemaphoreCreateFlags Flags)
{
    VkSemaphoreCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = Flags,
    };
    return Info;
}

static VkCommandBufferBeginInfo GetCommandBufferBeginInfo(VkCommandBufferUsageFlags Flags)
{
    VkCommandBufferBeginInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = Flags,
        .pInheritanceInfo = NULL,
    };
    return Info;
}

static VkImageSubresourceRange GetImageSubresourceRange(VkImageAspectFlags AspectMask)
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

static VkSemaphoreSubmitInfo GetSemaphoreSubmitInfo(VkPipelineStageFlags2 StageMask, VkSemaphore Semaphore)
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

static VkCommandBufferAllocateInfo GetCommandBufferAllocateInfo(VkCommandPool CommandPool, u32 Count)
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

static VkSubmitInfo2 GetSubmitInfo(VkCommandBufferSubmitInfo* CommandBufferSubInfoPtr, VkSemaphoreSubmitInfo* SignalSemaphoreInfo,
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

/* ==============
 * Shader Loading
 * ============== */

static b32 LoadShaderModule(const char* Path, VkDevice Device, VkShaderModule* OutShaderModule)
{
    FILE* File = fopen(Path, "r");

    if (!File)
    {
        return FALSE;
    }

    fseek(File, 0L, SEEK_END);
    size_t FileSize = (size_t)ftell(File);

    size_t BufferSize = FileSize / sizeof(u32);
    u32* Buffer = StackAlloc(u32, BufferSize);

    rewind(File);
    fread(Buffer, sizeof(Buffer[0]), FileSize, File);
    fclose(File);

    VkShaderModuleCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .codeSize = BufferSize * sizeof(u32),
        .pCode = Buffer,
    };

    VkShaderModule ShaderModule;
    if (vkCreateShaderModule(Device, &CreateInfo, NULL, &ShaderModule) != VK_SUCCESS)
    {
        return FALSE;
    }

    *OutShaderModule = ShaderModule;

    return TRUE;
}

/* ==========
 * Operations
 * ========== */

static void TransitionImage(
    VkCommandBuffer CommandBuffer,
    VkImage Image,
    VkPipelineStageFlags2 SrcStageMask,
    VkAccessFlags2 SrcAccessMask,
    VkPipelineStageFlags2 DstStageMask,
    VkAccessFlags2 DstAccessMask,
    VkImageLayout CurrentLayout,
    VkImageLayout NewLayout)
{
    VkImageMemoryBarrier2 ImageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        .pNext = NULL,

        .srcStageMask = SrcStageMask,
        .srcAccessMask = SrcAccessMask,
        .dstStageMask = DstStageMask,
        .dstAccessMask = DstAccessMask,

        .oldLayout = CurrentLayout,
        .newLayout = NewLayout,

        .image = Image,
        .subresourceRange = GetImageSubresourceRange((NewLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
    };

    VkDependencyInfo DepInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &ImageBarrier,
    };

    vkCmdPipelineBarrier2(CommandBuffer, &DepInfo);
}

static void CopyImageToImage(VkCommandBuffer CommandBuffer, VkImage Source, VkImage Destination, VkExtent2D SrcSize, VkExtent2D DstSize)
{
    VkImageBlit2 BlitRegion = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = NULL,

        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },

        .srcOffsets = { {0}, { (i32)SrcSize.width, (i32)SrcSize.height, 1 } },

        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },

        .dstOffsets = { {0}, { (i32)DstSize.width, (i32)DstSize.height, 1 } },
    };

    VkBlitImageInfo2 BlitInfo = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = NULL,
        .srcImage = Source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = Destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &BlitRegion,
        .filter = VK_FILTER_LINEAR,
    };

    vkCmdBlitImage2(CommandBuffer, &BlitInfo);
}

/* ========================
 * SDescriptorLayoutBuilder
 * ======================== */

static void DescriptorLayoutBuilder_Add(SDescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type)
{
    if (Builder->Count >= MAX_LAYOUT_BINDINGS)
    {
        return;
    }
    Builder->Bindings[Builder->Count] = (VkDescriptorSetLayoutBinding){
        .binding = Binding,
        .descriptorType = Type,
        .descriptorCount = 1,
    };
    Builder->Count++;
}

static void DescriptorLayoutBuilder_Clear(SDescriptorLayoutBuilder* Builder)
{
    *Builder = (SDescriptorLayoutBuilder){ 0 };
}

static VkDescriptorSetLayout DescriptorLayoutBuilder_Build(SDescriptorLayoutBuilder* Builder, VkDevice Device, VkShaderStageFlags ShaderStageFlags)
{
    for (u32 Index = 0; Index < MAX_LAYOUT_BINDINGS; ++Index)
    {
        VkDescriptorSetLayoutBinding* Binding = &Builder->Bindings[Index];
        Binding->stageFlags |= ShaderStageFlags;
    }

    VkDescriptorSetLayoutCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = Builder->Count,
        .pBindings = Builder->Bindings,
    };

    VkDescriptorSetLayout Set;
    VK_ASSERT(vkCreateDescriptorSetLayout(Device, &Info, NULL, &Set));

    return Set;
}

/* ====================
 * SDescriptorAllocator
 * ==================== */

static void DescriptorAllocator_Init(SDescriptorAllocator* DescriptorAllocator, VkDevice device, u32 maxSets, SDescriptorPoolSizeRatio* poolRatios, u32 PoolRatioCount)
{
    VkDescriptorPoolSize* PoolSizes = StackAlloc(VkDescriptorPoolSize, PoolRatioCount);
    for (u32 Index = 0; Index < PoolRatioCount; --Index)
    {
        PoolSizes[Index] = (VkDescriptorPoolSize){
            .type = poolRatios[Index].Type,
            .descriptorCount = (u32)(poolRatios[Index].Ratio * (f32)maxSets),
        };
    }

    VkDescriptorPoolCreateInfo PoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = maxSets,
        .poolSizeCount = PoolRatioCount,
        .pPoolSizes = PoolSizes,
    };

    VK_ASSERT(vkCreateDescriptorPool(device, &PoolCreateInfo, NULL, &DescriptorAllocator->Pool));
}

static void DescriptorAllocator_ClearDescriptors(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    VK_ASSERT(vkResetDescriptorPool(Device, DescriptorAllocator->Pool, 0));
}

static void DescriptorAllocator_DestroyPool(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    vkDestroyDescriptorPool(Device, DescriptorAllocator->Pool, NULL);
}

static VkDescriptorSet DescriptorAllocator_Allocate(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo AllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = DescriptorAllocator->Pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet DescriptorSet;
    VK_ASSERT(vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet));

    return DescriptorSet;
}
