#pragma once

#include "Rr_Defines.h"
#include "Rr_Memory.h"
#include "Rr_Pipeline.h"
#include "Rr_Vulkan.h"
#include "Rr_Text.h"
#include "Rr_Buffer.h"
#include "Rr_Descriptor.h"
#include "Rr_Draw.h"
#include "Rr_Math.h"

#include <SDL3/SDL.h>

typedef struct Rr_AcquireBarriers Rr_AcquireBarriers;
struct Rr_AcquireBarriers
{
    usize ImageMemoryBarrierCount;
    usize BufferMemoryBarrierCount;
    VkImageMemoryBarrier* ImageMemoryBarriers;
    VkBufferMemoryBarrier* BufferMemoryBarriers;
};

struct Rr_UploadContext
{
    Rr_WriteBuffer StagingBuffer;
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

struct Rr_Primitive
{
    Rr_Buffer* IndexBuffer;
    Rr_Buffer* VertexBuffer;
    u32 IndexCount;
};

/* Material at index 0 is matched with primitive at index 0 etc... */
struct Rr_StaticMesh
{
    Rr_Primitive* Primitives[RR_MESH_MAX_PRIMITIVES];
    Rr_Material* Materials[RR_MESH_MAX_PRIMITIVES];
    u8 PrimitiveCount;
    u8 MaterialCount;
};

struct Rr_SkeletalMesh
{
    Rr_Primitive* Primitives[RR_MESH_MAX_PRIMITIVES];
    Rr_Material* Materials[RR_MESH_MAX_PRIMITIVES];
    u8 PrimitiveCount;
    u8 MaterialCount;
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
    Rr_GenericPipeline* GenericPipeline;
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

typedef struct Rr_ImGui
{
    VkDescriptorPool DescriptorPool;
    bool bInitiated;
} Rr_ImGui;

typedef struct Rr_ImmediateMode Rr_ImmediateMode;
struct Rr_ImmediateMode
{
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
};

struct Rr_DrawTarget
{
    Rr_Image* ColorImage;
    Rr_Image* DepthImage;
    VkFramebuffer Framebuffer;
};

/**
 * Pipeline
 */
struct Rr_GenericPipeline
{
    VkPipeline Handle;
    Rr_GenericPipelineSizes Sizes;
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
    usize PushConstantsSize;
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
    Rr_Vec4 Palette[RR_TEXT_MAX_COLORS];
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
typedef struct Rr_DrawPrimitiveInfo Rr_DrawPrimitiveInfo;
struct Rr_DrawPrimitiveInfo
{
    Rr_Material* Material;
    Rr_Primitive* Primitive;
    u32 OffsetIntoDrawBuffer;
};

typedef struct Rr_DrawTextInfo Rr_DrawTextInfo;
struct Rr_DrawTextInfo
{
    Rr_Font* Font;
    Rr_String String;
    Rr_Vec2 Position;
    f32 Size;
    Rr_DrawTextFlags Flags;
};

typedef Rr_SliceType(Rr_DrawTextInfo) Rr_DrawTextSlice;
typedef Rr_SliceType(Rr_DrawPrimitiveInfo) Rr_DrawPrimitiveSlice;

/* @TODO: Separate generic and builtin stuff! */
struct Rr_RenderingContext
{
    Rr_Renderer* Renderer;
    Rr_DrawContextInfo Info;
    Rr_DrawTextSlice DrawTextSlice;
    Rr_DrawPrimitiveSlice PrimitivesSlice;
    byte GlobalsData[RR_PIPELINE_MAX_GLOBALS_SIZE];
    Rr_Arena* Arena;
};

/**
 * Renderer
 */
struct Rr_Frame
{
    Rr_Arena Arena;

    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;
    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;

    Rr_DescriptorAllocator DescriptorAllocator;
    Rr_WriteBuffer StagingBuffer;
    Rr_WriteBuffer CommonBuffer;
    Rr_WriteBuffer DrawBuffer;

    Rr_SliceType(Rr_PendingLoad) PendingLoadsSlice;
    Rr_SliceType(Rr_RenderingContext) RenderingContextsSlice;
    Rr_SliceType(VkSemaphore) RetiredSemaphoresSlice;
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

    /* Queues */
    // bool bUnifiedQueue;
    Rr_Queue UnifiedQueue;
    Rr_Queue TransferQueue;
    SDL_Mutex* UnifiedQueueMutex;

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

    /* Main Draw Target */
    Rr_DrawTarget* DrawTarget;

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
    size_t ObjectCount;
};

/* Load */
typedef struct Rr_LoadingThread Rr_LoadingThread;
struct Rr_LoadingThread
{
    Rr_Arena Arena;
    Rr_SliceType(Rr_LoadingContext) LoadingContextsSlice;
    SDL_Thread* Handle;
    SDL_Semaphore* Semaphore;
    SDL_Mutex* Mutex;
};

struct Rr_LoadingContext
{
    Rr_App* App;
    bool bAsync;
    SDL_Semaphore* Semaphore;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
    Rr_LoadTask* Tasks;
    usize TaskCount;
};

/* App */
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
    Rr_LoadingThread LoadingThread;
    Rr_Arena PermanentArena;
    SDL_TLSID ScratchArenaTLS;
    SDL_Window* Window;
    SDL_AtomicInt bExit;
};
