#pragma once

#define VK_NO_PROTOTYPES

#include <cglm/vec4.h>

#include <vk_mem_alloc.h>

#include "RrCore.h"
#include "RrImage.h"
#include "RrAsset.h"
#include "RrDescriptor.h"

#include <SDL3/SDL_atomic.h>

#define MAX_LAYOUT_BINDINGS 4
#define MAX_SWAPCHAIN_IMAGE_COUNT 8
#define PIPELINE_SHADER_STAGES 2
#define FRAME_OVERLAP 2

typedef struct SDL_Window SDL_Window;

typedef u32 MeshIndexType;

typedef struct SRrSceneData
{
    mat4 View;
    mat4 Proj;
    mat4 ViewProj;
    vec4 AmbientColor;
} SRrSceneData;

typedef struct
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
} SAllocatedBuffer;

typedef struct
{
    vec3 Position;
    f32 TexCoordX;
    vec3 Normal;
    f32 TexCoordY;
    vec4 Color;
} SRrVertex;

typedef struct
{
    SAllocatedBuffer IndexBuffer;
    SAllocatedBuffer VertexBuffer;
    VkDeviceAddress VertexBufferAddress;
} SRrMeshBuffers;

typedef struct
{
    mat4 ViewProjection;
    VkDeviceAddress VertexBufferAddress;
} SRrPushConstants3D;

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
    SDescriptorAllocator DescriptorAllocator;
    SAllocatedBuffer SceneDataBuffer;
} SRrFrame;

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
    VkExtent2D Extent;
    VkImage Handle;
    VkImageView View;
    VkSampler Sampler;
    VkDeviceMemory Memory;
} SVulkanImage;

typedef struct SSwapchain
{
    VkSwapchainKHR Handle;
    VkFormat Format;
    VkColorSpaceKHR ColorSpace;
    SVulkanImage Images[MAX_SWAPCHAIN_IMAGE_COUNT];
    u32 ImageCount;
    VkExtent2D Extent;
    SDL_AtomicInt bResizePending;
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

typedef struct SDescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding Bindings[MAX_LAYOUT_BINDINGS];
    u32 Count;
} SDescriptorLayoutBuilder;

typedef struct SComputeConstants
{
    vec4 Vec0;
    vec4 Vec1;
    vec4 Vec2;
    vec4 Vec3;
} SComputeConstants;

typedef struct
{
    b32 bInit;
    VkDescriptorPool DescriptorPool;
} SImGui;

typedef struct
{
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
} SImmediateMode;

typedef struct
{
    SAllocatedImage ColorImage;
    SAllocatedImage DepthImage;
    VkExtent2D ActiveExtent;
    VkDescriptorSet DescriptorSet;
    VkDescriptorSetLayout DescriptorSetLayout;
} SRrDrawTarget;

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

    SRrFrame Frames[FRAME_OVERLAP];
    size_t FrameNumber;

    SDescriptorAllocator GlobalDescriptorAllocator;

    SRrDrawTarget DrawTarget;

    VkPipeline GradientPipeline;
    VkPipelineLayout GradientPipelineLayout;

    SImGui ImGui;

    SImmediateMode ImmediateMode;

    VkPipeline MeshPipeline;
    VkPipelineLayout MeshPipelineLayout;

    SRrMeshBuffers Mesh;
    SRrRawMesh RawMesh;

    SRrSceneData SceneData;
    VkDescriptorSetLayout SceneDataLayout;
} SRr;
