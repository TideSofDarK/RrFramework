#pragma once

#include <cglm/vec4.h>

#define VK_NO_PROTOTYPES
#include <vk_mem_alloc.h>

#include <SDL3/SDL_atomic.h>

#include "RrCore.h"
#include "RrInput.h"
#include "RrImage.h"
#include "RrAsset.h"
#include "RrDescriptor.h"
#include "RrDefines.h"

typedef struct SDL_Window SDL_Window;
typedef struct Rr_AppConfig Rr_AppConfig;

/* Renderer Types */
typedef struct Rr_Buffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
} Rr_Buffer;

typedef struct Rr_Image
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
} Rr_Image;

typedef struct Rr_Vertex
{
    vec3 Position;
    f32 TexCoordX;
    vec3 Normal;
    f32 TexCoordY;
    vec4 Color;
} Rr_Vertex;

typedef struct Rr_MeshBuffers
{
    Rr_Buffer IndexBuffer;
    Rr_Buffer VertexBuffer;
    VkDeviceAddress VertexBufferAddress;
} Rr_MeshBuffers;

typedef struct Rr_PushConstants3D
{
    mat4 ViewProjection;
    VkDeviceAddress VertexBufferAddress;
} Rr_PushConstants3D;

typedef struct Rr_ImageBarrier
{
    VkCommandBuffer CommandBuffer;
    VkImage Image;
    VkPipelineStageFlags2 StageMask;
    VkAccessFlags2 AccessMask;
    VkImageLayout Layout;
} Rr_ImageBarrier;

typedef struct Rr_PerFrameData Rr_PerFrameData;

typedef struct Rr_Frame
{
    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;
    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;
    SDescriptorAllocator DescriptorAllocator;
} Rr_Frame;

typedef struct Rr_PipelineBuilder
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
} Rr_PipelineBuilder;

typedef struct Rr_SwapchainImage
{
    VkExtent2D Extent;
    VkImage Handle;
    VkImageView View;
    VkSampler Sampler;
    VkDeviceMemory Memory;
} Rr_SwapchainImage;

typedef struct Rr_Swapchain
{
    VkSwapchainKHR Handle;
    VkFormat Format;
    VkColorSpaceKHR ColorSpace;
    Rr_SwapchainImage Images[MAX_SWAPCHAIN_IMAGE_COUNT];
    u32 ImageCount;
    VkExtent2D Extent;
    SDL_AtomicInt bResizePending;
} Rr_Swapchain;

typedef struct Rr_DeviceQueue
{
    VkQueue Handle;
    u32 FamilyIndex;
} Rr_DeviceQueue;

typedef struct Rr_PhysicalDevice
{
    VkPhysicalDevice Handle;

    VkPhysicalDeviceFeatures Features;
    VkPhysicalDeviceMemoryProperties MemoryProperties;
    VkPhysicalDeviceSubgroupProperties SubgroupProperties;
} Rr_PhysicalDevice;

typedef struct Rr_DescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding Bindings[RR_MAX_LAYOUT_BINDINGS];
    u32 Count;
} Rr_DescriptorLayoutBuilder;

typedef struct SComputeConstants
{
    vec4 Vec0;
    vec4 Vec1;
    vec4 Vec2;
    vec4 Vec3;
} SComputeConstants;

typedef struct Rr_ImGui
{
    VkDescriptorPool DescriptorPool;
    b8 bInit;
} Rr_ImGui;

typedef struct Rr_ImmediateMode
{
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
} Rr_ImmediateMode;

typedef struct Rr_DrawTarget
{
    VkExtent2D ReferenceResolution;
    VkExtent2D ActiveResolution;
    i32 Scale;
    Rr_Image ColorImage;
    Rr_Image DepthImage;
    VkDescriptorSet DescriptorSet;
    VkDescriptorSetLayout DescriptorSetLayout;
} Rr_DrawTarget;

typedef struct Rr_RawMesh
{
    Rr_Array Vertices;
    Rr_Array Indices;
} Rr_RawMesh;

typedef struct Rr_Renderer
{
    VkInstance Instance;
    Rr_PhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkSurfaceKHR Surface;
    Rr_Swapchain Swapchain;

    Rr_DeviceQueue GraphicsQueue;
    Rr_DeviceQueue TransferQueue;
    Rr_DeviceQueue ComputeQueue;

    VmaAllocator Allocator;

    Rr_PerFrameData* PerFrameDatas;
    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameNumber;

    SDescriptorAllocator GlobalDescriptorAllocator;

    Rr_DrawTarget DrawTarget;

    VkPipeline GradientPipeline;
    VkPipelineLayout GradientPipelineLayout;

    Rr_ImGui ImGui;

    Rr_ImmediateMode ImmediateMode;

    VkPipeline MeshPipeline;
    VkPipelineLayout MeshPipelineLayout;

    Rr_MeshBuffers Mesh;
    Rr_RawMesh RawMesh;
    Rr_Image NoiseImage;

    VkSampler NearestSampler;
} Rr_Renderer;

typedef struct Rr_FrameTime
{
#ifdef RR_PERFORMANCE_COUNTER
    struct
    {
        f64 FPS;
        u64 Frames;
        u64 StartTime;
        u64 UpdateFrequency;
        f64 CountPerSecond;
    } PerformanceCounter;
#endif

    u64 TargetFramerate;
    u64 StartTime;
} Rr_FrameTime;

typedef struct Rr_App
{
    SDL_AtomicInt bExit;
    SDL_Window* Window;
    Rr_InputConfig InputConfig;
    Rr_InputState InputState;
    Rr_Renderer Renderer;
    Rr_FrameTime FrameTime;
    Rr_AppConfig* Config;
} Rr_App;
