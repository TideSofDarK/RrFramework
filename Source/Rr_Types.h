#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Buffer.h"
#include "Rr_Defines.h"
#include "Rr_Input.h"
#include "Rr_Asset.h"
#include "Rr_Load.h"
#include "Rr_Array.h"
#include "Rr_Descriptor.h"
#include "Rr_Text_Internal.h"
#include "Rr_Load_Internal.h"

typedef struct SDL_Semaphore SDL_Semaphore;
typedef struct SDL_Thread SDL_Thread;

typedef u32 Rr_MeshIndexType;

struct Rr_Buffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
};

struct Rr_StagingBuffer
{
    Rr_Buffer* Buffer;
    size_t CurrentOffset;
};

struct Rr_GenericPipelineBuffers
{
    Rr_Buffer* Globals;
    Rr_Buffer* Material;
    Rr_Buffer* Draw;
};

struct Rr_GenericPipeline
{
    VkPipeline Handle;

    Rr_GenericPipelineBuffers* Buffers[RR_FRAME_OVERLAP];

    size_t GlobalsSize;
    size_t MaterialSize;
    size_t DrawSize;
};

struct Rr_PipelineBuilder
{
    struct
    {
        u32 VertexInputStride;
    } VertexInput[2];
    VkVertexInputAttributeDescription Attributes[RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES];

    VkPipelineInputAssemblyStateCreateInfo InputAssembly;
    VkPipelineRasterizationStateCreateInfo Rasterizer;
    VkFormat ColorAttachmentFormats[RR_PIPELINE_MAX_COLOR_ATTACHMENTS];
    VkPipelineColorBlendAttachmentState ColorBlendAttachments[RR_PIPELINE_MAX_COLOR_ATTACHMENTS];
    VkPipelineMultisampleStateCreateInfo Multisampling;
    VkPipelineDepthStencilStateCreateInfo DepthStencil;
    VkPipelineRenderingCreateInfo RenderInfo;
    Rr_Asset VertexShaderSPV;
    Rr_Asset FragmentShaderSPV;
    size_t PushConstantsSize;
};

struct Rr_PendingLoad
{
    Rr_AcquireBarriers Barriers;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
    VkSemaphore Semaphore;
};

struct Rr_LoadingContext
{
    bool bAsync;
    Rr_LoadStatus Status;
    Rr_Renderer* Renderer;
    Rr_LoadTask* Tasks;
    SDL_Thread* Thread;
    SDL_Semaphore* Semaphore;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
};

struct Rr_DescriptorSetLayout
{
    VkDescriptorSetLayout Layout;
};

struct Rr_DescriptorPoolSizeRatio
{
    VkDescriptorType Type;
    f32 Ratio;
};

struct Rr_DescriptorAllocator
{
    Rr_DescriptorPoolSizeRatio* Ratios;
    VkDescriptorPool* FullPools;
    VkDescriptorPool* ReadyPools;
    size_t SetsPerPool;
};

typedef enum Rr_DescriptorWriterEntryType
{
    RR_DESCRIPTOR_WRITER_ENTRY_TYPE_BUFFER,
    RR_DESCRIPTOR_WRITER_ENTRY_TYPE_IMAGE
} Rr_DescriptorWriterEntryType;

typedef struct Rr_DescriptorWriterEntry
{
    Rr_DescriptorWriterEntryType Type;
    size_t Index;
} Rr_DescriptorWriterEntry;

struct Rr_DescriptorWriter
{
    VkDescriptorImageInfo* ImageInfos;
    VkDescriptorBufferInfo* BufferInfos;
    Rr_DescriptorWriterEntry* Entries;
    VkWriteDescriptorSet* Writes;
};

struct Rr_RawMesh
{
    Rr_Array Vertices;
    Rr_Array Indices;
};

struct Rr_StaticMesh
{
    size_t IndexCount;
    Rr_Buffer* IndexBuffer;
    Rr_Buffer* VertexBuffer;
};

struct Rr_Image
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
};

typedef struct Rr_Vertex
{
    vec3 Position;
    f32 TexCoordX;
    vec3 Normal;
    f32 TexCoordY;
    vec4 Color;
} Rr_Vertex;

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
    Rr_SwapchainImage Images[RR_MAX_SWAPCHAIN_IMAGE_COUNT];
    u32 ImageCount;
    VkExtent2D Extent;
    SDL_AtomicInt bResizePending;
} Rr_Swapchain;

typedef struct Rr_PhysicalDevice
{
    VkPhysicalDevice Handle;

    VkPhysicalDeviceFeatures Features;
    VkPhysicalDeviceProperties2 Properties;
    VkPhysicalDeviceMemoryProperties MemoryProperties;
    VkPhysicalDeviceSubgroupProperties SubgroupProperties;
} Rr_PhysicalDevice;

typedef struct Rr_ImGui
{
    VkDescriptorPool DescriptorPool;
    bool bInitiated;
} Rr_ImGui;

typedef struct Rr_ImmediateMode
{
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
} Rr_ImmediateMode;

typedef struct Rr_DrawTarget
{
    Rr_Image* ColorImage;
    Rr_Image* DepthImage;
    VkFramebuffer Framebuffer;
} Rr_DrawTarget;

struct Rr_Frame
{
    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;
    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;
    Rr_DescriptorAllocator DescriptorAllocator;
    Rr_StagingBuffer* StagingBuffer;
    VkSemaphore* RetiredSemaphoresArray;
    Rr_DrawTarget DrawTarget;
};

struct Rr_Renderer
{
    /* Essentials */
    VkInstance Instance;
    Rr_PhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkSurfaceKHR Surface;
    Rr_Swapchain Swapchain;

    /* Queues */
    u32 bUnifiedQueue;

    struct
    {
        SDL_Mutex* Mutex;
        Rr_PendingLoad* PendingLoads;
        VkCommandPool TransientCommandPool;
        VkQueue Handle;
        u32 FamilyIndex;
    } UnifiedQueue;

    struct
    {
        VkCommandPool TransientCommandPool;
        VkQueue Handle;
        u32 FamilyIndex;
    } TransferQueue;

    VmaAllocator Allocator;

    /* Frames */
    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameNumber;

    Rr_DescriptorAllocator GlobalDescriptorAllocator;

    VkExtent2D ReferenceResolution;
    VkExtent2D ActiveResolution;
    i32 Scale;

    VkRenderPass RenderPass;

    Rr_ImGui ImGui;
    Rr_ImmediateMode ImmediateMode;

    /* Texture Samplers */
    VkSampler NearestSampler;
    VkSampler LinearSampler;

    /* Text Rendering */
    Rr_TextPipeline TextPipeline;
    Rr_Font BuiltinFont;

    /* Generic Pipeline Layout */
    VkDescriptorSetLayout GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT];
    VkPipelineLayout GenericPipelineLayout;
};

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

struct Rr_App
{
    SDL_AtomicInt bExit;
    SDL_Window* Window;
    Rr_InputConfig InputConfig;
    Rr_InputState InputState;
    Rr_Renderer* Renderer;
    Rr_FrameTime FrameTime;
    Rr_AppConfig* Config;
};
