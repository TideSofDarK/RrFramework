/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "Rr_Log.h"
#include "Rr_Memory.h"
#include "Rr_Platform.h"

#include <Rr/Rr_Image.h>
#include <Rr/Rr_Math.h>
#include <Rr/Rr_Pipeline.h>
#include <Rr/Rr_Renderer.h>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#define RR_VULKAN_VERSION VK_MAKE_API_VERSION(0, 1, 1, 0)

#define RR_VULKAN_EARLY_STAGES             \
    (VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | \
     VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT)

#define RR_VULKAN_WRITES                                                 \
    (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | \
     VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |                      \
     VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT |           \
     VK_ACCESS_MEMORY_WRITE_BIT)

typedef struct Rr_BufferMemoryBarrier Rr_BufferMemoryBarrier;
struct Rr_BufferMemoryBarrier
{
    VkPipelineStageFlags SrcStageMask;
    VkPipelineStageFlags DstStageMask;
    VkAccessFlags SrcAccessMask;
    VkAccessFlags DstAccessMask;
    uint32_t SrcQueueFamilyIndex;
    uint32_t DstQueueFamilyIndex;
    VkBuffer Buffer;
    VkDeviceSize Offset;
    VkDeviceSize Size;
};

typedef struct Rr_ImageMemoryBarrier Rr_ImageMemoryBarrier;
struct Rr_ImageMemoryBarrier
{
    VkPipelineStageFlags SrcStageMask;
    VkPipelineStageFlags DstStageMask;
    VkAccessFlags SrcAccessMask;
    VkAccessFlags DstAccessMask;
    VkImageLayout OldLayout;
    VkImageLayout NewLayout;
    uint32_t SrcQueueFamilyIndex;
    uint32_t DstQueueFamilyIndex;
    VkImage Image;
    VkImageSubresourceRange SubresourceRange;
};

typedef struct Rr_BarrierBatch Rr_BarrierBatch;
struct Rr_BarrierBatch
{
    RR_ARRAY(Rr_ImageMemoryBarrier) ImageBarriers;
    RR_ARRAY(Rr_BufferMemoryBarrier) BufferBarriers;
    Rr_Map *VulkanHandleToBarrier;
};

typedef struct Rr_SyncState Rr_SyncState;
struct Rr_SyncState
{
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout Layout;
    uint32_t QueueFamilyIndex;
};

typedef struct Rr_Queue Rr_Queue;
struct Rr_Queue
{
    VkCommandPool TransientCommandPool;
    VkQueue Handle;
    uint32_t FamilyIndex;
    Rr_Spinlock Lock;
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

typedef struct Rr_Device Rr_Device;
struct Rr_Device
{
    VkDevice Handle;

    /* Vulkan 1.0 */

    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkCmdBeginQuery CmdBeginQuery;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
    PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer;
    PFN_vkCmdBindPipeline CmdBindPipeline;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
    PFN_vkCmdBlitImage CmdBlitImage;
    PFN_vkCmdClearAttachments CmdClearAttachments;
    PFN_vkCmdClearColorImage CmdClearColorImage;
    PFN_vkCmdClearDepthStencilImage CmdClearDepthStencilImage;
    PFN_vkCmdCopyBuffer CmdCopyBuffer;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;
    PFN_vkCmdCopyImage CmdCopyImage;
    PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer;
    PFN_vkCmdCopyQueryPoolResults CmdCopyQueryPoolResults;
    PFN_vkCmdDispatch CmdDispatch;
    PFN_vkCmdDispatchIndirect CmdDispatchIndirect;
    PFN_vkCmdDraw CmdDraw;
    PFN_vkCmdDrawIndexed CmdDrawIndexed;
    PFN_vkCmdDrawIndexedIndirect CmdDrawIndexedIndirect;
    PFN_vkCmdDrawIndirect CmdDrawIndirect;
    PFN_vkCmdEndQuery CmdEndQuery;
    PFN_vkCmdEndRenderPass CmdEndRenderPass;
    PFN_vkCmdExecuteCommands CmdExecuteCommands;
    PFN_vkCmdFillBuffer CmdFillBuffer;
    PFN_vkCmdNextSubpass CmdNextSubpass;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
    PFN_vkCmdPushConstants CmdPushConstants;
    PFN_vkCmdResetEvent CmdResetEvent;
    PFN_vkCmdResetQueryPool CmdResetQueryPool;
    PFN_vkCmdResolveImage CmdResolveImage;
    PFN_vkCmdSetBlendConstants CmdSetBlendConstants;
    PFN_vkCmdSetDepthBias CmdSetDepthBias;
    PFN_vkCmdSetDepthBounds CmdSetDepthBounds;
    PFN_vkCmdSetEvent CmdSetEvent;
    PFN_vkCmdSetLineWidth CmdSetLineWidth;
    PFN_vkCmdSetScissor CmdSetScissor;
    PFN_vkCmdSetStencilCompareMask CmdSetStencilCompareMask;
    PFN_vkCmdSetStencilReference CmdSetStencilReference;
    PFN_vkCmdSetStencilWriteMask CmdSetStencilWriteMask;
    PFN_vkCmdSetViewport CmdSetViewport;
    PFN_vkCmdUpdateBuffer CmdUpdateBuffer;
    PFN_vkCmdWaitEvents CmdWaitEvents;
    PFN_vkCmdWriteTimestamp CmdWriteTimestamp;
    PFN_vkCreateBuffer CreateBuffer;
    PFN_vkCreateBufferView CreateBufferView;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkCreateComputePipelines CreateComputePipelines;
    PFN_vkCreateDescriptorPool CreateDescriptorPool;
    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
    PFN_vkCreateEvent CreateEvent;
    PFN_vkCreateFence CreateFence;
    PFN_vkCreateFramebuffer CreateFramebuffer;
    PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
    PFN_vkCreateImage CreateImage;
    PFN_vkCreateImageView CreateImageView;
    PFN_vkCreatePipelineCache CreatePipelineCache;
    PFN_vkCreatePipelineLayout CreatePipelineLayout;
    PFN_vkCreateQueryPool CreateQueryPool;
    PFN_vkCreateRenderPass CreateRenderPass;
    PFN_vkCreateSampler CreateSampler;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkCreateShaderModule CreateShaderModule;
    PFN_vkDestroyBuffer DestroyBuffer;
    PFN_vkDestroyBufferView DestroyBufferView;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
    PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
    PFN_vkDestroyDevice DestroyDevice;
    PFN_vkDestroyEvent DestroyEvent;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkDestroyFramebuffer DestroyFramebuffer;
    PFN_vkDestroyImage DestroyImage;
    PFN_vkDestroyImageView DestroyImageView;
    PFN_vkDestroyPipeline DestroyPipeline;
    PFN_vkDestroyPipelineCache DestroyPipelineCache;
    PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
    PFN_vkDestroyQueryPool DestroyQueryPool;
    PFN_vkDestroyRenderPass DestroyRenderPass;
    PFN_vkDestroySampler DestroySampler;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkDestroyShaderModule DestroyShaderModule;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;
    PFN_vkFreeCommandBuffers FreeCommandBuffers;
    PFN_vkFreeDescriptorSets FreeDescriptorSets;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkGetDeviceMemoryCommitment GetDeviceMemoryCommitment;
    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkGetEventStatus GetEventStatus;
    PFN_vkGetFenceStatus GetFenceStatus;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkGetImageSparseMemoryRequirements GetImageSparseMemoryRequirements;
    PFN_vkGetImageSubresourceLayout GetImageSubresourceLayout;
    PFN_vkGetPipelineCacheData GetPipelineCacheData;
    PFN_vkGetQueryPoolResults GetQueryPoolResults;
    PFN_vkGetRenderAreaGranularity GetRenderAreaGranularity;
    PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
    PFN_vkMapMemory MapMemory;
    PFN_vkMergePipelineCaches MergePipelineCaches;
    PFN_vkQueueBindSparse QueueBindSparse;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkQueueWaitIdle QueueWaitIdle;
    PFN_vkResetCommandBuffer ResetCommandBuffer;
    PFN_vkResetCommandPool ResetCommandPool;
    PFN_vkResetDescriptorPool ResetDescriptorPool;
    PFN_vkResetEvent ResetEvent;
    PFN_vkResetFences ResetFences;
    PFN_vkSetEvent SetEvent;
    PFN_vkUnmapMemory UnmapMemory;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
    PFN_vkWaitForFences WaitForFences;

    /* VK_KHR_swapchain */

    PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR;
};

typedef struct Rr_Instance Rr_Instance;
struct Rr_Instance
{
    VkInstance Handle;

    /* Vulkan 1.0 */

    PFN_vkCreateDevice CreateDevice;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
    PFN_vkEnumerateDeviceLayerProperties EnumerateDeviceLayerProperties;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures;
    PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties
        GetPhysicalDeviceImageFormatProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties
        GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties
        GetPhysicalDeviceSparseImageFormatProperties;

    /* Vulkan 1.1 */

    PFN_vkEnumeratePhysicalDeviceGroups EnumeratePhysicalDeviceGroups;
    PFN_vkGetPhysicalDeviceExternalBufferProperties
        GetPhysicalDeviceExternalBufferProperties;
    PFN_vkGetPhysicalDeviceExternalFenceProperties
        GetPhysicalDeviceExternalFenceProperties;
    PFN_vkGetPhysicalDeviceExternalSemaphoreProperties
        GetPhysicalDeviceExternalSemaphoreProperties;
    PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2;
    PFN_vkGetPhysicalDeviceFormatProperties2 GetPhysicalDeviceFormatProperties2;
    PFN_vkGetPhysicalDeviceImageFormatProperties2
        GetPhysicalDeviceImageFormatProperties2;
    PFN_vkGetPhysicalDeviceMemoryProperties2 GetPhysicalDeviceMemoryProperties2;
    PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties2
        GetPhysicalDeviceQueueFamilyProperties2;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties2
        GetPhysicalDeviceSparseImageFormatProperties2;

    /* VK_KHR_surface */

    PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        GetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
        GetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
};

typedef struct Rr_VulkanLoader Rr_VulkanLoader;
struct Rr_VulkanLoader
{
    void *LibraryHandle;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;

    PFN_vkCreateInstance CreateInstance;
    PFN_vkEnumerateInstanceExtensionProperties
        EnumerateInstanceExtensionProperties;
    PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;
};

extern void Rr_InitLoader(Rr_VulkanLoader *Loader);

extern void Rr_InitInstance(
    Rr_VulkanLoader *Loader,
    const char *ApplicationName,
    Rr_Instance *Instance);

extern void Rr_InitSurface(Rr_Instance *Instance, VkSurfaceKHR *Surface);

extern void Rr_InitDeviceAndQueues(
    Rr_Instance *Instance,
    VkSurfaceKHR Surface,
    Rr_PhysicalDevice *PhysicalDevice,
    Rr_Device *Device,
    Rr_Queue *GraphicsQueue,
    Rr_Queue *TransferQueue);

extern void Rr_BlitColorImage(
    Rr_Device *Device,
    VkCommandBuffer CommandBuffer,
    VkImage Source,
    VkImage Destination,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    VkImageAspectFlags AspectMask);

static VkDescriptorType Rr_ToVulkanDescriptorType(Rr_BindingType Type)
{
    switch (Type)
    {
        case RR_BINDING_TYPE_SAMPLER:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case RR_BINDING_TYPE_SAMPLED_IMAGE:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case RR_BINDING_TYPE_STORAGE_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RR_BINDING_TYPE_UNIFORM_BUFFER:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case RR_BINDING_TYPE_STORAGE_IMAGE:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        default:
            RR_ABORT("Invalid pipeline binding type!");
    }
}

static inline VkStencilOp Rr_ToVulkanStencilOp(Rr_StencilOp StencilOp)
{
    switch (StencilOp)
    {
        case RR_STENCIL_OP_KEEP:
            return VK_STENCIL_OP_KEEP;
        case RR_STENCIL_OP_ZERO:
            return VK_STENCIL_OP_ZERO;
        case RR_STENCIL_OP_REPLACE:
            return VK_STENCIL_OP_REPLACE;
        case RR_STENCIL_OP_INCREMENT_AND_CLAMP:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case RR_STENCIL_OP_DECREMENT_AND_CLAMP:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case RR_STENCIL_OP_INVERT:
            return VK_STENCIL_OP_INVERT;
        case RR_STENCIL_OP_INCREMENT_AND_WRAP:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case RR_STENCIL_OP_DECREMENT_AND_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default:
            RR_ABORT("Invalid stencil op!");
    }
}

static inline VkShaderStageFlags Rr_ToVulkanShaderStageFlags(
    Rr_ShaderStage ShaderStage)
{
    VkShaderStageFlags ShaderStageFlags = 0;
    if ((ShaderStage & RR_SHADER_STAGE_VERTEX_BIT) != 0)
    {
        ShaderStageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if ((ShaderStage & RR_SHADER_STAGE_FRAGMENT_BIT) != 0)
    {
        ShaderStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if ((ShaderStage & RR_SHADER_STAGE_COMPUTE_BIT) != 0)
    {
        ShaderStageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
    }
    return ShaderStageFlags;
}

static inline VkCompareOp Rr_ToVulkanCompareOp(Rr_CompareOp CompareOp)
{
    switch (CompareOp)
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
            RR_ABORT("Invalid compare op!");
    }
}

static inline VkStencilOpState Rr_ToVulkanStencilOpState(
    Rr_StencilOpState State,
    Rr_DepthStencil *DepthStencil)
{
    return (VkStencilOpState){
        .compareOp = Rr_ToVulkanCompareOp(State.CompareOp),
        .failOp = Rr_ToVulkanStencilOp(State.FailOp),
        .passOp = Rr_ToVulkanStencilOp(State.PassOp),
        .depthFailOp = Rr_ToVulkanStencilOp(State.DepthFailOp),
        .writeMask = DepthStencil->WriteMask,
        .compareMask = DepthStencil->CompareMask,
        .reference = 0,
    };
}

static inline VkPolygonMode Rr_ToVulkanPolygonMode(Rr_PolygonMode PolygonMode)
{
    switch (PolygonMode)
    {
        case RR_POLYGON_MODE_FILL:
            return VK_POLYGON_MODE_FILL;
        case RR_POLYGON_MODE_LINE:
            return VK_POLYGON_MODE_LINE;
        default:
            RR_ABORT("Invalid polygon mode!");
    }
}

static inline VkCullModeFlagBits Rr_ToVulkanCullMode(Rr_CullMode CullMode)
{
    switch (CullMode)
    {
        case RR_CULL_MODE_NONE:
            return VK_CULL_MODE_NONE;
        case RR_CULL_MODE_FRONT:
            return VK_CULL_MODE_FRONT_BIT;
        case RR_CULL_MODE_BACK:
            return VK_CULL_MODE_BACK_BIT;
        default:
            RR_ABORT("Invalid cull mode!");
    }
}

static inline VkFrontFace Rr_ToVulkanFrontFace(Rr_FrontFace FrontFace)
{
    switch (FrontFace)
    {
        case RR_FRONT_FACE_COUNTER_CLOCKWISE:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        case RR_FRONT_FACE_CLOCKWISE:
            return VK_FRONT_FACE_CLOCKWISE;
        default:
            RR_ABORT("Invalid front face!");
    }
}

static inline VkBlendFactor Rr_ToVulkanBlendFactor(Rr_BlendFactor BlendFactor)
{
    switch (BlendFactor)
    {
        case RR_BLEND_FACTOR_ZERO:
            return VK_BLEND_FACTOR_ZERO;
        case RR_BLEND_FACTOR_ONE:
            return VK_BLEND_FACTOR_ONE;
        case RR_BLEND_FACTOR_SRC_COLOR:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case RR_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case RR_BLEND_FACTOR_DST_COLOR:
            return VK_BLEND_FACTOR_DST_COLOR;
        case RR_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case RR_BLEND_FACTOR_SRC_ALPHA:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case RR_BLEND_FACTOR_DST_ALPHA:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case RR_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case RR_BLEND_FACTOR_CONSTANT_COLOR:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case RR_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case RR_BLEND_FACTOR_SRC_ALPHA_SATURATE:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        default:
            RR_ABORT("Invalid blend factor!");
    }
}

static inline VkBlendOp Rr_ToVulkanBlendOp(Rr_BlendOp BlendOp)
{
    switch (BlendOp)
    {
        case RR_BLEND_OP_ADD:
            return VK_BLEND_OP_ADD;
        case RR_BLEND_OP_SUBTRACT:
            return VK_BLEND_OP_SUBTRACT;
        case RR_BLEND_OP_REVERSE_SUBTRACT:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case RR_BLEND_OP_MIN:
            return VK_BLEND_OP_MIN;
        case RR_BLEND_OP_MAX:
            return VK_BLEND_OP_MAX;
        default:
            RR_ABORT("Invalid blend op!");
    }
}

static inline VkPrimitiveTopology Rr_ToVulkanPrimitiveTopology(
    Rr_Topology Topology)
{
    switch (Topology)
    {
        case RR_TOPOLOGY_POINT_LIST:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case RR_TOPOLOGY_LINE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case RR_TOPOLOGY_LINE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case RR_TOPOLOGY_TRIANGLE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case RR_TOPOLOGY_TRIANGLE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        default:
            RR_ABORT("Invalid topology!");
    }
}

static inline VkFormat Rr_ToVulkanFormat(Rr_Format Format)
{
    switch (Format)
    {
        case RR_FORMAT_UNDEFINED:
            return VK_FORMAT_UNDEFINED;
        case RR_FORMAT_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case RR_FORMAT_UINT:
            return VK_FORMAT_R32_UINT;
        case RR_FORMAT_VEC2:
            return VK_FORMAT_R32G32_SFLOAT;
        case RR_FORMAT_VEC3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case RR_FORMAT_VEC4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            RR_ABORT("Invalid format!");
    }
}

static inline size_t Rr_GetFormatSize(Rr_Format Format)
{
    switch (Format)
    {
        case RR_FORMAT_FLOAT:
            return sizeof(float);
        case RR_FORMAT_UINT:
            return sizeof(uint32_t);
        case RR_FORMAT_VEC2:
            return sizeof(float) * 2;
        case RR_FORMAT_VEC3:
            return sizeof(float) * 3;
        case RR_FORMAT_VEC4:
            return sizeof(float) * 4;
        default:
            RR_ABORT("Invalid format!");
    }
}

static inline VkBorderColor Rr_ToVulkanBorderColor(Rr_BorderColor BorderColor)
{
    switch (BorderColor)
    {
        case RR_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
            return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case RR_BORDER_COLOR_INT_TRANSPARENT_BLACK:
            return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        case RR_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case RR_BORDER_COLOR_INT_OPAQUE_BLACK:
            return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        case RR_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
            return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        case RR_BORDER_COLOR_INT_OPAQUE_WHITE:
            return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        default:
            RR_ABORT("Invalid border color!");
    }
}

static inline VkSamplerAddressMode Rr_ToVulkanSamplerAddressMode(
    Rr_SamplerAddressMode SamplerAddressMode)
{
    switch (SamplerAddressMode)
    {
        case RR_SAMPLER_ADDRESS_MODE_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case RR_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case RR_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default:
            RR_ABORT("Invalid sampler address mode!");
    }
}

static inline VkSamplerMipmapMode Rr_ToVulkanSamplerMipmapMode(
    Rr_SamplerMipmapMode SamplerMipmapMode)
{
    switch (SamplerMipmapMode)
    {
        case RR_SAMPLER_MIPMAP_MODE_NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case RR_SAMPLER_MIPMAP_MODE_LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:
            RR_ABORT("Invalid sampler mipmap mode!");
    }
}

static inline VkFilter Rr_ToVulkanFilter(Rr_Filter Filter)
{
    switch (Filter)
    {
        case RR_FILTER_NEAREST:
            return VK_FILTER_NEAREST;
        case RR_FILTER_LINEAR:
            return VK_FILTER_LINEAR;
        default:
            RR_ABORT("Invalid filter!");
    }
}

static Rr_TextureFormat Rr_ToTextureFormat(VkFormat TextureFormat)
{
    switch (TextureFormat)
    {
        case VK_FORMAT_UNDEFINED:
            return RR_TEXTURE_FORMAT_UNDEFINED;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return RR_TEXTURE_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return RR_TEXTURE_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return RR_TEXTURE_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:
            return RR_TEXTURE_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_D32_SFLOAT:
            return RR_TEXTURE_FORMAT_D32_SFLOAT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return RR_TEXTURE_FORMAT_D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return RR_TEXTURE_FORMAT_D32_SFLOAT_S8_UINT;
        default:
            RR_ABORT("Invalid texture format!");
    }
}

static VkFormat Rr_ToVulkanTextureFormat(Rr_TextureFormat TextureFormat)
{
    switch (TextureFormat)
    {
        case RR_TEXTURE_FORMAT_UNDEFINED:
            return VK_FORMAT_UNDEFINED;
        case RR_TEXTURE_FORMAT_R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case RR_TEXTURE_FORMAT_R8G8B8A8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case RR_TEXTURE_FORMAT_B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case RR_TEXTURE_FORMAT_B8G8R8A8_SRGB:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case RR_TEXTURE_FORMAT_D32_SFLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case RR_TEXTURE_FORMAT_D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case RR_TEXTURE_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case RR_TEXTURE_FORMAT_R8G8B8A8_UINT:
            return VK_FORMAT_R8G8B8A8_UINT;
        case RR_TEXTURE_FORMAT_R8G8B8A8_SINT:
            return VK_FORMAT_R8G8B8A8_SINT;
        case RR_TEXTURE_FORMAT_R32_UINT:
            return VK_FORMAT_R32_UINT;
        case RR_TEXTURE_FORMAT_R32_SINT:
            return VK_FORMAT_R32_SINT;
        default:
            RR_ABORT("Invalid texture format!");
    }
}

static inline VkIndexType Rr_ToVulkanIndexType(Rr_IndexType IndexType)
{
    switch (IndexType)
    {
        case RR_INDEX_TYPE_UINT8:
            return VK_INDEX_TYPE_UINT8;
        case RR_INDEX_TYPE_UINT16:
            return VK_INDEX_TYPE_UINT16;
        case RR_INDEX_TYPE_UINT32:
            return VK_INDEX_TYPE_UINT32;
        default:
            RR_ABORT("Invalid index type!");
    }
}

static inline bool Rr_IsVulkanDepthFormat(VkFormat Format)
{
    return Format == VK_FORMAT_D32_SFLOAT ||
           Format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           Format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static inline VkImageAspectFlags Rr_ToVulkanImageAspect(Rr_ImageAspect Aspect)
{
    VkImageAspectFlags Result = 0;
    if (RR_HAS_BIT(Aspect, RR_IMAGE_ASPECT_COLOR_BIT))
    {
        Result |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if (RR_HAS_BIT(Aspect, RR_IMAGE_ASPECT_DEPTH_BIT))
    {
        Result |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (RR_HAS_BIT(Aspect, RR_IMAGE_ASPECT_STENCIL_BIT))
    {
        Result |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    return Result;
}

static VkAttachmentLoadOp Rr_ToVulkanLoadOp(Rr_LoadOp LoadOp)
{
    switch (LoadOp)
    {
        case RR_LOAD_OP_CLEAR:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case RR_LOAD_OP_LOAD:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case RR_LOAD_OP_DONT_CARE:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        default:
            RR_ABORT("Invalid load op!");
    }
}

static VkAttachmentStoreOp Rr_ToVulkanStoreOp(Rr_StoreOp StoreOp)
{
    switch (StoreOp)
    {
        case RR_STORE_OP_STORE:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case RR_STORE_OP_DONT_CARE:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default:
            RR_ABORT("Invalid store op!");
    }
}

static inline VkPresentModeKHR Rr_ToVulkanPresentMode(
    Rr_PresentMode PresentMode)
{
    switch (PresentMode)
    {
        case RR_PRESENT_MODE_FIFO_RELAXED:
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        case RR_PRESENT_MODE_IMMEDIATE:
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case RR_PRESENT_MODE_MAILBOX:
            return VK_PRESENT_MODE_MAILBOX_KHR;
        case RR_PRESENT_MODE_FIFO:
            return VK_PRESENT_MODE_FIFO_KHR;
        default:
            RR_ABORT("Invalid present mode!");
    }
}

static inline Rr_PresentMode Rr_ToPresentMode(
    VkPresentModeKHR VulkanPresentMode)
{
    switch (VulkanPresentMode)
    {
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            return RR_PRESENT_MODE_FIFO_RELAXED;
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
            return RR_PRESENT_MODE_IMMEDIATE;
        case VK_PRESENT_MODE_MAILBOX_KHR:
            return RR_PRESENT_MODE_MAILBOX;
        case VK_PRESENT_MODE_FIFO_KHR:
            return RR_PRESENT_MODE_FIFO;
        default:
            RR_ABORT("Invalid present mode!");
    }
}
