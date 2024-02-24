#pragma once

#include "RrTypes.hxx"

#include <fstream>
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

VkRenderingAttachmentInfo GetRenderingAttachmentInfo(VkImageView View, VkClearValue* ClearValue, VkImageLayout Layout)
{
    VkRenderingAttachmentInfo Info = {};
    Info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    Info.pNext = nullptr;
    Info.imageView = View;
    Info.imageLayout = Layout;
    Info.loadOp = ClearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    Info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (ClearValue)
    {
        Info.clearValue = *ClearValue;
    }

    return Info;
}

VkRenderingInfo GetRenderingInfo(VkExtent2D RenderExtent, VkRenderingAttachmentInfo* ColorAttachment,
    VkRenderingAttachmentInfo* DepthAttachment)
{
    VkRenderingInfo Info = {};
    Info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    Info.pNext = nullptr;

    Info.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, RenderExtent };
    Info.layerCount = 1;
    Info.colorAttachmentCount = 1;
    Info.pColorAttachments = ColorAttachment;
    Info.pDepthAttachment = DepthAttachment;
    Info.pStencilAttachment = nullptr;

    return Info;
}

VkImageCreateInfo GetImageCreateInfo(VkFormat Format, VkImageUsageFlags UsageFlags, VkExtent3D Extent)
{
    VkImageCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
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

VkImageViewCreateInfo GetImageViewCreateInfo(VkFormat Format, VkImage Image, VkImageAspectFlags AspectFlags)
{
    VkImageViewCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
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

VkSemaphoreSubmitInfo GetSemaphoreSubmitInfo(VkPipelineStageFlags2 StageMask, VkSemaphore Semaphore)
{
    VkSemaphoreSubmitInfo Info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = Semaphore,
        .value = 1,
        .stageMask = StageMask,
        .deviceIndex = 0,
    };

    return Info;
}

VkCommandBufferSubmitInfo GetCommandBufferSubmitInfo(VkCommandBuffer CommandBuffer)
{
    VkCommandBufferSubmitInfo Info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = CommandBuffer,
        .deviceMask = 0,
    };

    return Info;
}

VkCommandBufferAllocateInfo GetCommandBufferAllocateInfo(VkCommandPool CommandPool, u32 Count)
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

VkSubmitInfo2 GetSubmitInfo(VkCommandBufferSubmitInfo* CommandBufferSubInfoPtr, VkSemaphoreSubmitInfo* SignalSemaphoreInfo,
    VkSemaphoreSubmitInfo* WaitSemaphoreInfo)
{
    VkSubmitInfo2 Info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .waitSemaphoreInfoCount = WaitSemaphoreInfo == nullptr ? 0u : 1u,
        .pWaitSemaphoreInfos = WaitSemaphoreInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = CommandBufferSubInfoPtr,
        .signalSemaphoreInfoCount = SignalSemaphoreInfo == nullptr ? 0u : 1u,
        .pSignalSemaphoreInfos = SignalSemaphoreInfo,
    };

    return Info;
}

/* ==============
 * Shader Loading
 * ============== */

b32 LoadShaderModule(const char* Path, VkDevice Device, VkShaderModule* OutShaderModule)
{
    std::ifstream File(Path, std::ios::ate | std::ios::binary);

    if (!File.is_open())
    {
        return false;
    }

    size_t fileSize = (size_t)File.tellg();

    u32 BufferSize = fileSize / sizeof(u32);
    u32* Buffer = StackAlloc(u32, BufferSize);

    File.seekg(0);
    File.read((char*)Buffer, (i64)fileSize);
    File.close();

    VkShaderModuleCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .codeSize = BufferSize * sizeof(u32),
        .pCode = Buffer,
    };

    VkShaderModule ShaderModule;
    if (vkCreateShaderModule(Device, &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
    {
        return false;
    }

    *OutShaderModule = ShaderModule;

    return true;
}

/* ==========
 * Operations
 * ========== */

void TransitionImage(
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

        .pNext = nullptr,

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
        .pNext = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &ImageBarrier,
    };

    vkCmdPipelineBarrier2(CommandBuffer, &DepInfo);
}

void CopyImageToImage(VkCommandBuffer CommandBuffer, VkImage Source, VkImage Destination, VkExtent2D SrcSize, VkExtent2D DstSize)
{
    VkImageBlit2 BlitRegion = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = nullptr,

        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },

        .srcOffsets = { {}, { (i32)SrcSize.width, (i32)SrcSize.height, 1 } },

        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },

        .dstOffsets = { {}, { (i32)DstSize.width, (i32)DstSize.height, 1 } },
    };

    VkBlitImageInfo2 BlitInfo = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
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

void DescriptorLayoutBuilder_Add(SDescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type)
{
    if (Builder->Count >= MAX_LAYOUT_BINDINGS)
    {
        return;
    }
    Builder->Bindings[Builder->Count] = {
        .binding = Binding,
        .descriptorType = Type,
        .descriptorCount = 1,
    };
    Builder->Count++;
}

void DescriptorLayoutBuilder_Clear(SDescriptorLayoutBuilder* Builder)
{
    *Builder = {};
}

VkDescriptorSetLayout DescriptorLayoutBuilder_Build(SDescriptorLayoutBuilder* Builder, VkDevice Device, VkShaderStageFlags ShaderStageFlags)
{
    for (auto& Binding : Builder->Bindings)
    {
        Binding.stageFlags |= ShaderStageFlags;
    }

    VkDescriptorSetLayoutCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = Builder->Count,
        .pBindings = Builder->Bindings,
    };

    VkDescriptorSetLayout Set;
    VK_ASSERT(vkCreateDescriptorSetLayout(Device, &Info, nullptr, &Set));

    return Set;
}

/* ====================
 * SDescriptorAllocator
 * ==================== */

void DescriptorAllocator_Init(SDescriptorAllocator* DescriptorAllocator, VkDevice device, u32 maxSets, SDescriptorPoolSizeRatio* poolRatios, u32 PoolRatioCount)
{
    VkDescriptorPoolSize* PoolSizes = StackAlloc(VkDescriptorPoolSize, PoolRatioCount);
    for (u32 Index = 0; Index < PoolRatioCount; --Index)
    {
        PoolSizes[Index] = {
            .type = poolRatios[Index].Type,
            .descriptorCount = uint32_t(poolRatios[Index].Ratio * (f32)maxSets),
        };
    }

    VkDescriptorPoolCreateInfo PoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = maxSets,
        .poolSizeCount = PoolRatioCount,
        .pPoolSizes = PoolSizes,
    };

    VK_ASSERT(vkCreateDescriptorPool(device, &PoolCreateInfo, nullptr, &DescriptorAllocator->Pool));
}

void DescriptorAllocator_ClearDescriptors(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    VK_ASSERT(vkResetDescriptorPool(Device, DescriptorAllocator->Pool, 0));
}

void DescriptorAllocator_DestroyPool(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    vkDestroyDescriptorPool(Device, DescriptorAllocator->Pool, nullptr);
}

VkDescriptorSet DescriptorAllocator_Allocate(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo AllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = DescriptorAllocator->Pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet DescriptorSet;
    VK_ASSERT(vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet));

    return DescriptorSet;
}
