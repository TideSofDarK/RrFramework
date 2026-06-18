/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#pragma once

#include "Rr_RHI.h"

#include "Rr_Atomic.h"
#include "Rr_Memory.h"
#include "Rr_Profiler.h"

#include <Rr/Rr_Arena.h>
#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Log.h>

#define VK_NO_PROTOTYPES
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
    Rr_HashTrie *VulkanHandleToBarrier;
};

typedef struct Rr_SyncState Rr_SyncState;
struct Rr_SyncState
{
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout Layout;
    uint32_t QueueFamilyIndex;
};

static const Rr_SyncState RR_EMPTY_SYNC = {
    .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    .QueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
};

typedef struct Rr_VulkanBinding Rr_VulkanBinding;
struct Rr_VulkanBinding
{
    uint32_t Index;
    VkDescriptorType Type;
    VkPipelineStageFlags Stages;
    uint32_t Count;
    VkFormat ImageFormat;
};

struct Rr_Queue
{
    VkQueue Handle;
    VkQueueFamilyProperties FamilyProperties;
    uint32_t FamilyIndex;
    bool TimestampsEnabled;
    Rr_Spinlock Lock;
};

typedef struct Rr_PhysicalDevice Rr_PhysicalDevice;
struct Rr_PhysicalDevice
{
    VkPhysicalDevice Handle;

    VkPhysicalDeviceFeatures Features;
    VkPhysicalDeviceProperties Properties;
    VkPhysicalDeviceMemoryProperties MemoryProperties;
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

    /* VK_KHR_surface */

    PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        GetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
        GetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;

#ifdef RR_USE_GPU_DEBUG_UTILS
    /* VK_EXT_debug_utils */

    PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabelEXT;
    PFN_vkCmdInsertDebugUtilsLabelEXT CmdInsertDebugUtilsLabelEXT;
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
    PFN_vkQueueBeginDebugUtilsLabelEXT QueueBeginDebugUtilsLabelEXT;
    PFN_vkQueueEndDebugUtilsLabelEXT QueueEndDebugUtilsLabelEXT;
    PFN_vkQueueInsertDebugUtilsLabelEXT QueueInsertDebugUtilsLabelEXT;
    PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;
    PFN_vkSetDebugUtilsObjectTagEXT SetDebugUtilsObjectTagEXT;
    PFN_vkSubmitDebugUtilsMessageEXT SubmitDebugUtilsMessageEXT;
#endif
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
    char const *ApplicationName,
    Rr_Instance *Instance);

extern void Rr_InitSurface(Rr_Instance *Instance, VkSurfaceKHR *Surface);

extern void Rr_InitDeviceAndQueues(
    Rr_Instance *Instance,
    VkSurfaceKHR Surface,
    Rr_PhysicalDevice *PhysicalDevice,
    Rr_Device *Device,
    Rr_Queue *GraphicsQueue,
    Rr_Queue *TransferQueue);

/* NOTE: To pass various attachment configurations around we use the following
 * order of image views (a.k.a. attachments):
 * 1) N color attachments
 * 2) M resolve attachments
 * 3) Depth/stencil attachment
 * M must be less or equal to N. Depth/stencil attachment may not be present. */

typedef struct Rr_FramebufferKey Rr_FramebufferKey;
struct Rr_FramebufferKey
{
    VkExtent3D Extent;
    uint8_t ColorAttachmentCount;
    uint8_t ResolveAttachmentCount;
    uint8_t DepthStencil;
    uint8_t Padding;
    VkRenderPass RenderPass;
    VkImageView ImageViews[RR_MAX_COLOR_ATTACHMENTS * 2 + 1];
};

#define RR_HASH_MAP_PREFIX     Rr_
#define RR_HASH_MAP_NAME       FramebufferMap
#define RR_HASH_MAP_KEY_TYPE   Rr_FramebufferKey
#define RR_HASH_MAP_VALUE_TYPE VkFramebuffer
#include "Rr_HashMap.h"

extern VkFramebuffer Rr_GetFramebuffer(Rr_FramebufferKey *Key);

extern void Rr_DestroyFramebuffers(VkImageView ImageView);

typedef struct Rr_RenderPassAttachment Rr_RenderPassAttachment;
struct Rr_RenderPassAttachment
{
    VkSampleCountFlags Samples;
    VkFormat Format;
    VkAttachmentLoadOp LoadOp;
    VkAttachmentStoreOp StoreOp;
};

typedef struct Rr_RenderPassKey Rr_RenderPassKey;
struct Rr_RenderPassKey
{
    uint8_t ColorAttachmentCount;
    uint8_t ResolveAttachmentCount;
    uint8_t ResolveMask;
    uint8_t DepthStencil;
    Rr_RenderPassAttachment Attachments[RR_MAX_COLOR_ATTACHMENTS * 2 + 1];
};

#define RR_HASH_MAP_PREFIX     Rr_
#define RR_HASH_MAP_NAME       RenderPassMap
#define RR_HASH_MAP_KEY_TYPE   Rr_RenderPassKey
#define RR_HASH_MAP_VALUE_TYPE VkRenderPass
#include "Rr_HashMap.h"

extern VkRenderPass Rr_GetRenderPass(Rr_RenderPassKey const *Key);

/*
 * Descriptors
 */

#define RR_DESCRIPTOR_POOL_SIZE 128

typedef struct Rr_DescriptorPoolList Rr_DescriptorPoolList;
struct Rr_DescriptorPoolList
{
    VkDescriptorPool Handle;
    Rr_DescriptorPoolList *Next;
};

extern Rr_DescriptorPoolList *Rr_AcquireDescriptorPoolList(void);

extern void Rr_ReleaseDescriptorPoolList(Rr_DescriptorPoolList *List);

extern void Rr_AllocateDescriptorSets(
    Rr_DescriptorPoolList *List,
    uint32_t Count,
    VkDescriptorSetLayout *Layouts,
    VkDescriptorSet *OutSets);

typedef struct Rr_DescriptorsState Rr_DescriptorsState;
struct Rr_DescriptorsState
{
    Rr_Device *Device;
    VkCommandBuffer CommandBuffer;
    VkDescriptorSet EmptyDescriptorSet;
    Rr_DescriptorPoolList *DescriptorPoolList;
    Rr_PipelineLayout *Layout;
    VkDescriptorSet Sets[RR_MAX_SETS];
    bool Dirty[RR_MAX_SETS];
};

extern void Rr_InvalidateDescriptorsState(
    Rr_DescriptorsState *State,
    Rr_PipelineLayout *Layout);

extern void Rr_WriteImageDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkImageView View,
    VkImageLayout Layout,
    VkSampler Sampler);

extern void Rr_WriteBufferDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkBuffer Handle,
    uint64_t Size,
    uint64_t Offset);

extern void Rr_WriteSamplerDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkSampler Sampler);

extern void Rr_ApplyDescriptorsState(
    Rr_DescriptorsState *State,
    VkPipelineBindPoint BindPoint);

/*
 * Buffer
 */

struct Rr_Chunk;
struct Rr_Range;

typedef struct Rr_AllocatedBuffer Rr_AllocatedBuffer;
struct Rr_AllocatedBuffer
{
    VkBuffer Handle;
    struct Rr_Chunk *Chunk;
    struct Rr_Range *Range;
    void *MappedData;
    Rr_SyncState SyncState;
};

struct Rr_Buffer
{
    Rr_BufferFlags Flags;
    VkDeviceSize Size;
    VkBufferUsageFlags Usage;
    uint32_t AllocatedBufferCount;
    Rr_AllocatedBuffer AllocatedBuffers[RR_FRAME_OVERLAP];

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

    Rr_AtomicInt RefCount;
};

#define RR_HIVE_TYPE      Rr_Buffer
#define RR_HIVE_TYPE_NAME Buffer
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroyBuffer(Rr_Buffer *Buffer);

extern Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(Rr_Buffer *Buffer);

/*
 * Image
 */

typedef struct Rr_ImageViewKey Rr_ImageViewKey;
struct Rr_ImageViewKey
{
    VkImageSubresourceRange SubresourceRange;
    VkImageViewType Type;
    VkFormat Format;
};

#define RR_HASH_MAP_PREFIX     Rr_
#define RR_HASH_MAP_NAME       ImageViewMap
#define RR_HASH_MAP_KEY_TYPE   Rr_ImageViewKey
#define RR_HASH_MAP_VALUE_TYPE VkImageView
#include "Rr_HashMap.h"

extern Rr_ImageViewMap *Rr_CreateImageViewMap(void);

extern void Rr_DestroyImageViewMap(
    Rr_ImageViewMap *ImageViewMap,
    bool DestroyFramebuffers);

typedef struct Rr_AllocatedImage Rr_AllocatedImage;
struct Rr_AllocatedImage
{
    VkImage Handle;
    Rr_ImageViewMap *ImageViewMap;
    Rr_Spinlock ImageViewMapLock;
    struct Rr_Chunk *Chunk;
    struct Rr_Range *Range;
    struct Rr_Image *Container;
    Rr_SyncState SyncState;
};

extern VkImageView Rr_GetVulkanImageView(
    Rr_AllocatedImage *AllocatedImage,
    Rr_ImageViewKey const *Key);

struct Rr_Image
{
    VkExtent3D Extent;
    VkImageAspectFlags AspectFlags;
    VkFormat Format;
    VkSampleCountFlags SampleCount;
    Rr_ImageFlags Flags;
    uint32_t LayerCount;
    uint32_t LevelCount;
    uint32_t AllocatedImageCount; /* Always 1 for now. */
    Rr_AllocatedImage AllocatedImages[RR_FRAME_OVERLAP];

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

    Rr_AtomicInt RefCount;
};

typedef struct Rr_Image Rr_Image;

#define RR_HIVE_TYPE      Rr_Image
#define RR_HIVE_TYPE_NAME Image
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroyImage(Rr_Image *Image);

extern Rr_AllocatedImage *Rr_GetCurrentAllocatedImage(Rr_Image *Image);

/*
 * Sampler
 */

struct Rr_Sampler
{
    VkSampler Handle;

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

    Rr_AtomicInt RefCount;
};

#define RR_HIVE_TYPE      Rr_Sampler
#define RR_HIVE_TYPE_NAME Sampler
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroySampler(Rr_Sampler *Sampler);

/*
 * Pipelines
 */

typedef struct Rr_DescriptorSetLayoutKey Rr_DescriptorSetLayoutKey;
struct Rr_DescriptorSetLayoutKey
{
    uint32_t BindingCount;
    Rr_VulkanBinding Bindings[RR_MAX_BINDINGS];
};

typedef struct Rr_DescriptorSetLayout Rr_DescriptorSetLayout;
struct Rr_DescriptorSetLayout
{
    Rr_DescriptorSetLayoutKey Key;
    Rr_DescriptorSetLayout *Children[4];

    VkDescriptorSetLayout Handle;
};

#define RR_HIVE_TYPE      Rr_DescriptorSetLayout
#define RR_HIVE_TYPE_NAME DescriptorSetLayout
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

typedef struct Rr_DescriptorSetLayoutStorage Rr_DescriptorSetLayoutStorage;
struct Rr_DescriptorSetLayoutStorage
{
    Rr_DescriptorSetLayout *Map;
    Rr_DescriptorSetLayoutHive Hive;
};

extern Rr_DescriptorSetLayout *Rr_GetDescriptorSetLayout(
    Rr_DescriptorSetLayoutKey const *Key);

typedef struct Rr_PipelineLayoutKey Rr_PipelineLayoutKey;
struct Rr_PipelineLayoutKey
{
    uint32_t DescriptorSetLayoutCount;
    Rr_DescriptorSetLayout *DescriptorSetLayouts[RR_MAX_SETS];
};

struct Rr_PipelineLayout
{
    Rr_PipelineLayoutKey Key;
    Rr_PipelineLayout *Children[4];

    VkPipelineLayout Handle;
};

#define RR_HIVE_TYPE      Rr_PipelineLayout
#define RR_HIVE_TYPE_NAME PipelineLayout
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

typedef struct Rr_PipelineLayoutStorage Rr_PipelineLayoutStorage;
struct Rr_PipelineLayoutStorage
{
    Rr_PipelineLayout *Map;
    Rr_PipelineLayoutHive Hive;
};

extern Rr_PipelineLayout *Rr_GetPipelineLayout(
    size_t BindingSetCount,
    Rr_BindingSet const *BindingSets);

struct Rr_ComputePipeline
{
    VkPipeline Handle;
    Rr_PipelineLayout *Layout;

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

    Rr_AtomicInt RefCount;
};

#define RR_HIVE_TYPE      Rr_ComputePipeline
#define RR_HIVE_TYPE_NAME ComputePipeline
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroyComputePipeline(Rr_ComputePipeline *ComputePipeline);

struct Rr_GraphicsPipeline
{
    VkPipeline Handle;
    Rr_PipelineLayout *Layout;
    uint32_t ColorAttachmentCount;
    bool HasDepthStencil;

    char Name[RR_MAX_OBJECT_NAME_LENGTH];

    Rr_AtomicInt RefCount;
};

#define RR_HIVE_TYPE      Rr_GraphicsPipeline
#define RR_HIVE_TYPE_NAME GraphicsPipeline
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

extern void Rr_DestroyGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipelin);

/*
 * Allocator
 */

#define RR_BIG_CHUNK_SIZE   RR_MEBIBYTES(256)
#define RR_SMALL_CHUNK_SIZE RR_MEBIBYTES(64)

typedef struct Rr_Range Rr_Range;
struct Rr_Range
{
    VkDeviceSize Offset;
    VkDeviceSize Size;
    VkDeviceSize AlignedOffset;
    bool Free;

    Rr_Range *Previous;
    Rr_Range *Next;
    Rr_Range *PreviousFree;
    Rr_Range *NextFree;
};

#define RR_HIVE_TYPE               Rr_Range
#define RR_HIVE_TYPE_NAME          Range
#define RR_HIVE_PREFIX             Rr_
#define RR_HIVE_MIN_BLOCK_CAPACITY 64
#include "Rr_Hive.h"

typedef struct Rr_Chunk Rr_Chunk;
struct Rr_Chunk
{
    VkDeviceSize Size;
    VkDeviceMemory Memory;
    uint32_t SoftAllocationCount;
    uint32_t MappingCount;
    uint32_t MemoryTypeIndex;
    bool Dedicated;
    void *MappedData;

    Rr_Range *FirstRange;
    Rr_Range *FirstFreeRange;
};

#define RR_HIVE_TYPE      Rr_Chunk
#define RR_HIVE_TYPE_NAME Chunk
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

typedef struct Rr_MemoryType Rr_MemoryType;
struct Rr_MemoryType
{
    VkDeviceSize HeapSize;
    bool DeviceLocalHeap;
    VkDeviceSize ChunkSize;
    VkMemoryPropertyFlags PropertyFlags;
    Rr_ChunkHive ChunkHive;
};

typedef struct Rr_Allocator Rr_Allocator;
struct Rr_Allocator
{
    VkDeviceSize BufferImageGranularity;
    VkDeviceSize NonCoherentAtomSize;
    VkDeviceSize BigChunkSize;
    VkDeviceSize SmallChunkSize;
    uint32_t MemoryTypeCount;
    Rr_MemoryType *MemoryTypes;
    Rr_RangeHive RangeHive;
    uint32_t HardAllocationCount;
    uint32_t SoftAllocationCount;
    Rr_Spinlock Lock;
};

extern void Rr_InitAllocator(
    Rr_Allocator *Allocator,
    Rr_PhysicalDevice *PhysicalDevice);

extern void Rr_CleanupAllocator(Rr_Allocator *Allocator);

extern bool Rr_AllocBufferMemory(
    Rr_Allocator *Allocator,
    struct Rr_Buffer *Buffer);

extern void Rr_FreeBufferMemory(
    Rr_Allocator *Allocator,
    struct Rr_Buffer *Buffer);

extern void *Rr_MapAllocatedBufferMemory(
    Rr_Allocator *Allocator,
    struct Rr_AllocatedBuffer *AllocatedBuffer);

extern void Rr_UnmapAllocatedBufferMemory(
    Rr_Allocator *Allocator,
    struct Rr_AllocatedBuffer *AllocatedBuffer);

extern void Rr_FlushAllocatedBufferMemory(
    Rr_Allocator *Allocator,
    struct Rr_AllocatedBuffer *AllocatedBuffer,
    size_t Offset,
    size_t Size);

extern bool Rr_AllocImageMemory(
    Rr_Allocator *Allocator,
    struct Rr_Image *Image);

extern void Rr_FreeImageMemory(Rr_Allocator *Allocator, struct Rr_Image *Image);

/*
 * Swapchain and Presentation
 */

typedef struct Rr_SwapchainImage Rr_SwapchainImage;
struct Rr_SwapchainImage
{
    Rr_Image2D Container;
    VkSemaphore EarlySemaphore;
    VkSemaphore LateSemaphore;
};

typedef struct Rr_Swapchain Rr_Swapchain;
struct Rr_Swapchain
{
    uint32_t PresentModeCount;
    Rr_PresentMode PresentModes[8];
    Rr_PresentMode PresentMode;
    VkSwapchainKHR Handle;
    VkFormat Format;
    VkColorSpaceKHR ColorSpace;
    VkExtent3D Extent;
    bool RecreatePending;
    bool Recreated;
    bool Unavailable;
};

struct Rr_CommandPools
{
    VkCommandPool Graphics;
    VkCommandPool Transfer;
    VkCommandPool Compute;
    Rr_CommandPools *Next;
};

struct Rr_Frame
{
    VkCommandBuffer EarlyCommandBuffer;
    VkCommandBuffer LateCommandBuffer;
    VkSemaphore AcquireSemaphore;
    VkFence SubmitFence;
    VkQueryPool QueryPool;
    Rr_Profiler *Profiler;
    Rr_SwapchainImage *SwapchainImage;
    Rr_Graph *Graph;
    Rr_Arena *Arena;
};

struct Rr_RHI
{
    Rr_VulkanLoader Loader;
    Rr_Instance Instance;
    Rr_PhysicalDevice PhysicalDevice;
    Rr_Device Device;
    VkSurfaceKHR Surface;

    Rr_Swapchain Swapchain;
    RR_ARRAY(Rr_SwapchainImage) SwapchainImages;

    Rr_Queue MainQueue;
    Rr_Queue DedicatedTransferQueue;
    Rr_Queue AsyncComputeQueue;

    Rr_Allocator Allocator;

    RR_ARRAY(VkSemaphore) Semaphores;
    Rr_Spinlock SemaphoresLock;

    RR_ARRAY(VkFence) Fences;
    Rr_Spinlock FencesLock;

    Rr_CommandPools *FreeCommandPools;
    Rr_Spinlock CommandPoolsLock;

    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameIndex;  /* Current frame-in-flight index. */
    size_t FrameNumber; /* Total frames rendered. */
    double LastFrameMS;

    VkDescriptorPool EmptyDescriptorPool;
    VkDescriptorSet EmptyDescriptorSet;

    Rr_BufferHive Buffers;
    Rr_Spinlock BuffersLock;
    Rr_HandleSet ReleasedBuffers;
    Rr_Spinlock ReleasedBuffersLock;

    Rr_ImageHive Images;
    Rr_Spinlock ImagesLock;
    Rr_HandleSet ReleasedImages;
    Rr_Spinlock ReleasedImagesLock;
    RR_FREE_LIST(Rr_ImageViewMap) ImageViewMaps;
    Rr_Spinlock ImageViewMapsLock;
    Rr_FramebufferMap FramebufferMap;
    Rr_Spinlock FramebufferMapLock;

    Rr_SamplerHive Samplers;
    Rr_Spinlock SamplersLock;
    Rr_HandleSet ReleasedSamplers;
    Rr_Spinlock ReleasedSamplersLock;

    Rr_DescriptorSetLayoutStorage DescriptorSetLayoutStorage;
    Rr_Spinlock DescriptorSetLayoutStorageLock;

    Rr_PipelineLayoutStorage PipelineLayoutStorage;
    Rr_Spinlock PipelineLayoutStorageLock;

    Rr_ComputePipelineHive ComputePipelines;
    Rr_Spinlock ComputePipelinesLock;
    Rr_HandleSet ReleasedComputePipelines;
    Rr_Spinlock ReleasedComputePipelinesLock;

    Rr_GraphicsPipelineHive GraphicsPipelines;
    Rr_Spinlock GraphicsPipelinesLock;
    Rr_HandleSet ReleasedGraphicsPipelines;
    Rr_Spinlock ReleasedGraphicsPipelinesLock;

    Rr_RenderPassMap RenderPassMap;
    Rr_Spinlock RenderPassMapLock;

    Rr_DescriptorPoolList *DescriptorPoolList;
    Rr_Spinlock DescriptorPoolListLock;
    uint32_t DescriptorPoolListCount;
};

extern VkSemaphore Rr_AcquireVulkanSemaphore(void);

extern void Rr_ReleaseVulkanSemaphore(VkSemaphore Semaphore);

extern VkFence Rr_AcquireVulkanFence(void);

extern void Rr_ReleaseVulkanFence(VkFence Fence);

extern void Rr_SetVulkanObjectName(
    VkObjectType ObjectType,
    uint64_t Handle,
    const char *Name);

extern void Rr_BeginVulkanCommandBufferLabel(
    VkCommandBuffer CommandBuffer,
    const char *Name);

extern void Rr_EndVulkanCommandBufferLabel(VkCommandBuffer CommandBuffer);

/*
 * Conversions
 */

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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid pipeline binding type!");
    }

    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid stencil op!");
    }

    return VK_STENCIL_OP_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid compare op!");
    }

    return VK_COMPARE_OP_MAX_ENUM;
}

static inline VkStencilOpState Rr_ToVulkanStencilOpState(
    Rr_StencilOpState const *State,
    Rr_DepthStencil const *DepthStencil)
{
    return (VkStencilOpState){
        .compareOp = Rr_ToVulkanCompareOp(State->CompareOp),
        .failOp = Rr_ToVulkanStencilOp(State->FailOp),
        .passOp = Rr_ToVulkanStencilOp(State->PassOp),
        .depthFailOp = Rr_ToVulkanStencilOp(State->DepthFailOp),
        .writeMask = DepthStencil->WriteMask,
        .compareMask = DepthStencil->CompareMask,
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid polygon mode!");
    }

    return VK_POLYGON_MODE_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid cull mode!");
    }

    return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid front face!");
    }

    return VK_FRONT_FACE_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid blend factor!");
    }

    return VK_BLEND_FACTOR_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid blend op!");
    }

    return VK_BLEND_OP_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid topology!");
    }

    return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

static inline VkFormat Rr_ToVulkanFormat(Rr_Format Format)
{
    switch (Format)
    {
        case RR_FORMAT_UNDEFINED:
            return VK_FORMAT_UNDEFINED;
            /* INT */
        case RR_FORMAT_INT:
            return VK_FORMAT_R32_SINT;
        case RR_FORMAT_INT2:
            return VK_FORMAT_R32G32_SINT;
        case RR_FORMAT_INT3:
            return VK_FORMAT_R32G32B32_SINT;
        case RR_FORMAT_INT4:
            return VK_FORMAT_R32G32B32A32_SINT;
            /* UINT */
        case RR_FORMAT_UINT:
            return VK_FORMAT_R32_UINT;
        case RR_FORMAT_UINT2:
            return VK_FORMAT_R32G32_UINT;
        case RR_FORMAT_UINT3:
            return VK_FORMAT_R32G32B32_UINT;
        case RR_FORMAT_UINT4:
            return VK_FORMAT_R32G32B32A32_UINT;
            /* FLOAT */
        case RR_FORMAT_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case RR_FORMAT_FLOAT2:
            return VK_FORMAT_R32G32_SFLOAT;
        case RR_FORMAT_FLOAT3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case RR_FORMAT_FLOAT4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid format!");
    }

    return VK_FORMAT_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid border color!");
    }

    return VK_BORDER_COLOR_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid sampler address mode!");
    }

    return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid sampler mipmap mode!");
    }

    return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid filter!");
    }

    return VK_FILTER_MAX_ENUM;
}

static inline Rr_ImageFormat Rr_ToImageFormat(VkFormat ImageFormat)
{
    switch (ImageFormat)
    {
        case VK_FORMAT_UNDEFINED:
            return RR_IMAGE_FORMAT_UNDEFINED;
            /* R8G8 */
        case VK_FORMAT_R8G8_UNORM:
            return RR_IMAGE_FORMAT_R8G8_UNORM;
        case VK_FORMAT_R8G8_UINT:
            return RR_IMAGE_FORMAT_R8G8_UINT;
        case VK_FORMAT_R8G8_SINT:
            return RR_IMAGE_FORMAT_R8G8_SINT;
        case VK_FORMAT_R8G8_SRGB:
            return RR_IMAGE_FORMAT_R8G8_SRGB;
            /* R8G8B8 */
        case VK_FORMAT_R8G8B8_UNORM:
            return RR_IMAGE_FORMAT_R8G8B8_UNORM;
        case VK_FORMAT_R8G8B8_UINT:
            return RR_IMAGE_FORMAT_R8G8B8_UINT;
        case VK_FORMAT_R8G8B8_SINT:
            return RR_IMAGE_FORMAT_R8G8B8_SINT;
        case VK_FORMAT_R8G8B8_SRGB:
            return RR_IMAGE_FORMAT_R8G8B8_SRGB;
            /* B8G8R8 */
        case VK_FORMAT_B8G8R8_UNORM:
            return RR_IMAGE_FORMAT_B8G8R8_UNORM;
        case VK_FORMAT_B8G8R8_UINT:
            return RR_IMAGE_FORMAT_B8G8R8_UINT;
        case VK_FORMAT_B8G8R8_SINT:
            return RR_IMAGE_FORMAT_B8G8R8_SINT;
        case VK_FORMAT_B8G8R8_SRGB:
            return RR_IMAGE_FORMAT_B8G8R8_SRGB;
            /* R8G8B8A8 */
        case VK_FORMAT_R8G8B8A8_UNORM:
            return RR_IMAGE_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8A8_UINT:
            return RR_IMAGE_FORMAT_R8G8B8A8_UINT;
        case VK_FORMAT_R8G8B8A8_SINT:
            return RR_IMAGE_FORMAT_R8G8B8A8_SINT;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return RR_IMAGE_FORMAT_R8G8B8A8_SRGB;
            /* B8G8R8A8 */
        case VK_FORMAT_B8G8R8A8_UNORM:
            return RR_IMAGE_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_UINT:
            return RR_IMAGE_FORMAT_B8G8R8A8_UINT;
        case VK_FORMAT_B8G8R8A8_SINT:
            return RR_IMAGE_FORMAT_B8G8R8A8_SINT;
        case VK_FORMAT_B8G8R8A8_SRGB:
            return RR_IMAGE_FORMAT_B8G8R8A8_SRGB;
            /* A8B8G8R8 */
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            return RR_IMAGE_FORMAT_A8B8G8R8_UNORM_PACK32;
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            return RR_IMAGE_FORMAT_A8B8G8R8_SRGB_PACK32;
            /* */
        case VK_FORMAT_R16G16_SFLOAT:
            return RR_IMAGE_FORMAT_R16G16_SFLOAT;
        case VK_FORMAT_D16_UNORM:
            return RR_IMAGE_FORMAT_D16_UNORM;
        case VK_FORMAT_D32_SFLOAT:
            return RR_IMAGE_FORMAT_D32_SFLOAT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return RR_IMAGE_FORMAT_D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return RR_IMAGE_FORMAT_D32_SFLOAT_S8_UINT;
        case VK_FORMAT_R32_UINT:
            return RR_IMAGE_FORMAT_R32_UINT;
        case VK_FORMAT_R32_SINT:
            return RR_IMAGE_FORMAT_R32_SINT;
        case VK_FORMAT_R32_SFLOAT:
            return RR_IMAGE_FORMAT_R32_SFLOAT;
        case VK_FORMAT_R32G32_SFLOAT:
            return RR_IMAGE_FORMAT_R32G32_SFLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return RR_IMAGE_FORMAT_R32G32B32A32_SFLOAT;
        default:
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid image format!");
    }

    return 0;
}

static VkFormat Rr_ToVulkanImageFormat(Rr_ImageFormat ImageFormat)
{
    switch (ImageFormat)
    {
        case RR_IMAGE_FORMAT_UNDEFINED:
            return VK_FORMAT_UNDEFINED;
            /* R8G8 */
        case RR_IMAGE_FORMAT_R8G8_UNORM:
            return VK_FORMAT_R8G8_UNORM;
        case RR_IMAGE_FORMAT_R8G8_UINT:
            return VK_FORMAT_R8G8_UINT;
        case RR_IMAGE_FORMAT_R8G8_SINT:
            return VK_FORMAT_R8G8_SINT;
        case RR_IMAGE_FORMAT_R8G8_SRGB:
            return VK_FORMAT_R8G8_SRGB;
            /* R8G8B8 */
        case RR_IMAGE_FORMAT_R8G8B8_UNORM:
            return VK_FORMAT_R8G8B8_UNORM;
        case RR_IMAGE_FORMAT_R8G8B8_UINT:
            return VK_FORMAT_R8G8B8_UINT;
        case RR_IMAGE_FORMAT_R8G8B8_SINT:
            return VK_FORMAT_R8G8B8_SINT;
        case RR_IMAGE_FORMAT_R8G8B8_SRGB:
            return VK_FORMAT_R8G8B8_SRGB;
            /* B8G8R8 */
        case RR_IMAGE_FORMAT_B8G8R8_UNORM:
            return VK_FORMAT_B8G8R8_UNORM;
        case RR_IMAGE_FORMAT_B8G8R8_UINT:
            return VK_FORMAT_B8G8R8_UINT;
        case RR_IMAGE_FORMAT_B8G8R8_SINT:
            return VK_FORMAT_B8G8R8_SINT;
        case RR_IMAGE_FORMAT_B8G8R8_SRGB:
            return VK_FORMAT_B8G8R8_SRGB;
            /* R8G8B8A8 */
        case RR_IMAGE_FORMAT_R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case RR_IMAGE_FORMAT_R8G8B8A8_SINT:
            return VK_FORMAT_R8G8B8A8_SINT;
        case RR_IMAGE_FORMAT_R8G8B8A8_UINT:
            return VK_FORMAT_R8G8B8A8_UINT;
        case RR_IMAGE_FORMAT_R8G8B8A8_SRGB:
            return VK_FORMAT_R8G8B8A8_SRGB;
            /* B8G8R8A8 */
        case RR_IMAGE_FORMAT_B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case RR_IMAGE_FORMAT_B8G8R8A8_UINT:
            return VK_FORMAT_B8G8R8A8_UINT;
        case RR_IMAGE_FORMAT_B8G8R8A8_SINT:
            return VK_FORMAT_B8G8R8A8_SINT;
        case RR_IMAGE_FORMAT_B8G8R8A8_SRGB:
            return VK_FORMAT_B8G8R8A8_SRGB;
            /* A8B8G8R8 */
        case RR_IMAGE_FORMAT_A8B8G8R8_UNORM_PACK32:
            return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
        case RR_IMAGE_FORMAT_A8B8G8R8_SRGB_PACK32:
            return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
            /* */
        case RR_IMAGE_FORMAT_R16G16_SFLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case RR_IMAGE_FORMAT_D16_UNORM:
            return VK_FORMAT_D16_UNORM;
        case RR_IMAGE_FORMAT_D32_SFLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case RR_IMAGE_FORMAT_D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case RR_IMAGE_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case RR_IMAGE_FORMAT_R32_UINT:
            return VK_FORMAT_R32_UINT;
        case RR_IMAGE_FORMAT_R32_SINT:
            return VK_FORMAT_R32_SINT;
        case RR_IMAGE_FORMAT_R32_SFLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case RR_IMAGE_FORMAT_R32G32_SFLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        case RR_IMAGE_FORMAT_R32G32B32A32_SFLOAT:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid image format!");
    }

    return VK_FORMAT_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid index type!");
    }

    return 0;
}

static inline bool Rr_IsVulkanDepthFormat(VkFormat Format)
{
    return Format == VK_FORMAT_D32_SFLOAT ||
           Format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           Format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static inline VkExtent3D Rr_ToVulkanExtent3D(Rr_IntVec3 Extent)
{
    return (VkExtent3D){
        .width = (uint32_t)Extent.X,
        .height = (uint32_t)Extent.Y,
        .depth = (uint32_t)Extent.Z,
    };
}

static inline VkOffset3D Rr_ToVulkanOffset3D(Rr_IntVec3 Offset)
{
    return (VkOffset3D){
        .x = Offset.X,
        .y = Offset.Y,
        .z = Offset.Z,
    };
}

static inline VkImageAspectFlags Rr_ToVulkanImageAspect(Rr_ImageAspect Aspect)
{
    VkImageAspectFlags Result = 0;
    if (Aspect & RR_IMAGE_ASPECT_COLOR_BIT)
    {
        Result |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if (Aspect & RR_IMAGE_ASPECT_DEPTH_BIT)
    {
        Result |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (Aspect & RR_IMAGE_ASPECT_STENCIL_BIT)
    {
        Result |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    return Result;
}

static inline VkAttachmentLoadOp Rr_ToVulkanLoadOp(Rr_LoadOp LoadOp)
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid load op!");
    }

    return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
}

static inline VkAttachmentStoreOp Rr_ToVulkanStoreOp(Rr_StoreOp StoreOp)
{
    switch (StoreOp)
    {
        case RR_STORE_OP_STORE:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case RR_STORE_OP_DONT_CARE:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default:
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid store op!");
    }

    return VK_ATTACHMENT_STORE_OP_MAX_ENUM;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid present mode!");
    }

    return VK_PRESENT_MODE_MAX_ENUM_KHR;
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
            Rr_LogError(RR_LOG_CATEGORY_RHI, "Invalid present mode!");
    }

    return 0;
}

static inline void Rr_ToVulkanBindings(
    size_t BindingCount,
    Rr_Binding const *Bindings,
    Rr_VulkanBinding *OutVulkanBindings)
{
    for (size_t Index = 0; Index < BindingCount; ++Index)
    {
        Rr_Binding const *Binding = &Bindings[Index];
        if (Binding->Type != RR_BINDING_TYPE_INVALID)
        {
            Rr_VulkanBinding *VulkanBinding = &OutVulkanBindings[Index];
            VulkanBinding->Index = Binding->Index;
            VulkanBinding->Type = Rr_ToVulkanDescriptorType(Binding->Type);
            VulkanBinding->Stages =
                Rr_ToVulkanShaderStageFlags(Binding->Stages);
            VulkanBinding->Count = Binding->Count ? Binding->Count : 1;
            VulkanBinding->ImageFormat =
                Rr_ToVulkanImageFormat(Binding->ImageFormat);
        }
    }
}
