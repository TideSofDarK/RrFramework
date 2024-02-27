#pragma once

#define VK_NO_PROTOTYPES

#include <cglm/vec4.h>

#include <vk_mem_alloc.h>

#include "Core.h"

#define MAX_LAYOUT_BINDINGS 4
#define MAX_SWAPCHAIN_IMAGE_COUNT 8
#define PIPELINE_SHADER_STAGES 2
#define FRAME_OVERLAP 2

typedef struct SDL_Window SDL_Window;

typedef struct STransitionImage
{
    VkCommandBuffer CommandBuffer;
    VkImage Image;
    VkPipelineStageFlags2 StageMask;
    VkAccessFlags2 AccessMask;
    VkImageLayout Layout;
} STransitionImage;

typedef struct SFrameData
{
    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;
    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;
} SFrameData;

typedef struct SPipelineBuilder
{
    VkPipelineShaderStageCreateInfo ShaderStages[PIPELINE_SHADER_STAGES];
    u32 ShaderStageCount;
    VkPipelineInputAssemblyStateCreateInfo InputAssembly;
    VkPipelineRasterizationStateCreateInfo Rasterizer;
    VkPipelineColorBlendAttachmentState ColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo Multisampling;
    VkPipelineLayout PipelineLayout;
    VkPipelineDepthStencilStateCreateInfo DepthStencil;
    VkPipelineRenderingCreateInfo RenderInfo;

    VkFormat ColorAttachmentFormat;
} SPipelineBuilder;

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
} SSwapchain;

typedef struct SRendererQueue
{
    VkQueue Handle;
    u32 FamilyIndex;
} SQueue;

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

typedef struct SComputeConstants
{
    vec4 Vec0;
    vec4 Vec1;
    vec4 Vec2;
    vec4 Vec3;
} SComputeConstants;

typedef struct SImGui
{
    b32 bInit;
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
    VkDescriptorPool DescriptorPool;
} SImmediateMode;

typedef struct SRr
{
    VkInstance Instance;
    SPhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkSurfaceKHR Surface;
    SSwapchain Swapchain;

    SQueue GraphicsQueue;
    SQueue TransferQueue;
    SQueue ComputeQueue;

    VmaAllocator Allocator;

    VkDebugUtilsMessengerEXT Messenger;

    SFrameData Frames[FRAME_OVERLAP];
    size_t FrameNumber;

    SDescriptorAllocator GlobalDescriptorAllocator;

    SAllocatedImage DrawImage;
    VkExtent2D DrawExtent;
    VkDescriptorSet DrawImageDescriptors;
    VkDescriptorSetLayout DrawImageDescriptorLayout;
    VkPipeline GradientPipeline;
    VkPipelineLayout GradientPipelineLayout;

    SImmediateMode ImmediateMode;

    VkPipeline TrianglePipeline;
    VkPipelineLayout TrianglePipelineLayout;
} SRr;
