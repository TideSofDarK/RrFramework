#pragma once

#include "Rr_Defines.h"
#include "Rr_Vulkan.h"
#include "Rr_Text.h"
#include "Rr_Array.h"
#include "Rr_Buffer.h"
#include "Rr_Descriptor.h"
#include "Rr_Draw.h"
#include "Rr_Math.h"

#include <SDL3/SDL.h>

typedef struct Rr_AcquireBarriers
{
    VkImageMemoryBarrier* ImageMemoryBarriersArray;
    VkBufferMemoryBarrier* BufferMemoryBarriersArray;
} Rr_AcquireBarriers;

struct Rr_UploadContext
{
    Rr_StagingBuffer* StagingBuffer;
    VkCommandBuffer TransferCommandBuffer;
    Rr_AcquireBarriers AcquireBarriers;
    bool bUseAcquireBarriers;
};

typedef struct Rr_PendingLoad
{
    Rr_AcquireBarriers Barriers;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
    VkSemaphore Semaphore;
} Rr_PendingLoad;

struct Rr_MeshBuffers
{
    Rr_Buffer* IndexBuffer;
    Rr_Buffer* VertexBuffer;
};

/* Material at index 0 is matched with primitive at index 0 etc... */
struct Rr_StaticMesh
{
    Rr_MeshBuffers MeshBuffers[RR_MESH_MAX_PRIMITIVES];
    Rr_Material* Materials[RR_MESH_MAX_PRIMITIVES];
    size_t MeshBuffersCount;
    size_t MaterialCount;
};

struct Rr_SkeletalMesh
{
    Rr_Material* Material;
    Rr_MeshBuffers* MeshBuffers;
};

struct Rr_Image
{
    VkImage Handle;
    VkImageView View;
    VmaAllocation Allocation;
    VkExtent3D Extent;
    VkFormat Format;
};

struct Rr_Material
{
    Rr_Buffer* Buffer;
    Rr_Image* Textures[RR_MAX_TEXTURES_PER_MATERIAL];
    size_t TextureCount;
};

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

/**
 * Pipeline
 */
struct Rr_GenericPipeline
{
    VkPipeline Handle;

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

/**
 * Text Rendering
 */
typedef enum Rr_TextPipelineDescriptorSet
{
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
} Rr_TextPipelineDescriptorSet;

typedef struct Rr_Glyph
{
    u32 AtlasXY;
    u32 AtlasWH;
    u32 PlaneLB;
    u32 PlaneRT;
} Rr_Glyph;

typedef struct Rr_TextPerInstanceVertexInput
{
    Rr_Vec2 Advance;
    u32 Unicode;
} Rr_TextPerInstanceVertexInput;

typedef struct Rr_TextGlobalsLayout
{
    float Time;
    float Reserved;
    Rr_Vec2 ScreenSize;
    Rr_Vec4 Pallete[RR_TEXT_MAX_COLORS];
} Rr_TextGlobalsLayout;

typedef struct Rr_TextFontLayout
{
    float Advance;
    float DistanceRange;
    Rr_Vec2 AtlasSize;
    Rr_Glyph Glyphs[RR_TEXT_MAX_GLYPHS];
} Rr_TextFontLayout;

typedef struct Rr_TextPushConstants
{
    Rr_Vec2 PositionScreenSpace;
    f32 Size;
    u32 Flags;
    Rr_Vec4 ReservedB;
    Rr_Vec4 ReservedC;
    Rr_Vec4 ReservedD;
    Rr_Mat4 ReservedE;
} Rr_TextPushConstants;

struct Rr_Font
{
    Rr_Image* Atlas;
    Rr_Buffer* Buffer;
    f32* Advances;
    f32 LineHeight;
    f32 DefaultSize;
};

typedef struct Rr_TextPipeline
{
    VkDescriptorSetLayout DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    VkPipeline Handle;
    VkPipelineLayout Layout;
    Rr_Buffer* QuadBuffer;
    Rr_Buffer* GlobalsBuffers[RR_FRAME_OVERLAP];
    Rr_Buffer* TextBuffers[RR_FRAME_OVERLAP];
} Rr_TextPipeline;

/**
 * Draw
 */
typedef struct Rr_DrawMeshInfo
{
    Rr_Material* Material;
    Rr_StaticMesh* StaticMesh;
    u32 OffsetIntoDrawBuffer;
} Rr_DrawMeshInfo;

typedef struct Rr_DrawTextInfo
{
    Rr_Font* Font;
    Rr_String String;
    Rr_Vec2 Position;
    f32 Size;
    Rr_DrawTextFlags Flags;
} Rr_DrawTextInfo;

struct Rr_RenderingContext
{
    Rr_BeginRenderingInfo* Info;
    Rr_Renderer* Renderer;
    Rr_DrawMeshInfo* DrawMeshArray;
    Rr_DrawTextInfo* DrawTextArray;
    Rr_Buffer* CommonBuffer;
    size_t CommonBufferOffset;
    Rr_Buffer* DrawBuffer;
    size_t DrawBufferOffset;
};

/**
 * Renderer
 */
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
    struct
    {
        Rr_Buffer* Common;
        Rr_Buffer* Draw;
    } Buffers;
};

struct Rr_Renderer
{
    /* Vulkan Instance */
    VkInstance Instance;

    /* Presentation */
    VkSurfaceKHR Surface;
    Rr_Swapchain Swapchain;

    /* Device */
    Rr_PhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkDeviceSize UniformAlignment;

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
    size_t CurrentFrameIndex;

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
    Rr_Font* BuiltinFont;

    /* Generic Pipeline Layout */
    VkDescriptorSetLayout GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT];
    VkPipelineLayout GenericPipelineLayout;

    /* Object Storage */
    void* Storage;
    void* NextObject;
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
    Rr_Renderer Renderer;
    Rr_AppConfig* Config;
    Rr_InputConfig InputConfig;
    Rr_InputState InputState;
    Rr_FrameTime FrameTime;
    SDL_Window* Window;
    SDL_AtomicInt bExit;
};
