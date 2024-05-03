#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Buffer.h"
#include "Rr_Defines.h"
#include "Rr_Input.h"
#include "Rr_Asset.h"
#include "Rr_Load.h"

typedef struct Rr_Buffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
} Rr_Buffer;

typedef struct Rr_StagingBuffer
{
    Rr_Buffer* Buffer;
    size_t CurrentOffset;
} Rr_StagingBuffer;

typedef struct Rr_GenericPipelineBuffers
{
    Rr_Buffer* Globals;
    Rr_Buffer* Material;
    Rr_Buffer* Draw;
} Rr_GenericPipelineBuffers;

typedef struct Rr_GenericPipeline
{
    VkPipeline Handle;

    Rr_GenericPipelineBuffers* Buffers[RR_FRAME_OVERLAP];

    size_t GlobalsSize;
    size_t MaterialSize;
    size_t DrawSize;
} Rr_GenericPipeline;

typedef struct Rr_PipelineBuilder
{
    struct
    {
        u32 VertexInputStride;
    } VertexInput[2];
    VkVertexInputAttributeDescription Attributes[RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES];
    u32 CurrentVertexInputAttribute;
    u32 bHasPerVertexBinding;
    u32 bHasPerInstanceBinding;

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
} Rr_PipelineBuilder;

typedef struct Rr_AcquireBarriers
{
    VkImageMemoryBarrier* ImageMemoryBarriersArray;
    VkBufferMemoryBarrier* BufferMemoryBarriersArray;
} Rr_AcquireBarriers;

typedef struct Rr_UploadContext
{
    Rr_StagingBuffer* StagingBuffer;
    VkCommandBuffer TransferCommandBuffer;
    Rr_AcquireBarriers AcquireBarriers;
    u32 bUnifiedQueue;
} Rr_UploadContext;

typedef struct Rr_PendingLoad
{
    Rr_AcquireBarriers Barriers;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
    VkSemaphore Semaphore;
} Rr_PendingLoad;

typedef struct Rr_LoadingContext
{
    u32 bAsync;
    Rr_LoadStatus Status;
    Rr_Renderer* Renderer;
    Rr_LoadTask* Tasks;
    SDL_Thread* Thread;
    SDL_Semaphore* Semaphore;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
} Rr_LoadingContext;

typedef struct Rr_DescriptorSetLayout
{
    VkDescriptorSetLayout Layout;
} Rr_DescriptorSetLayout;

typedef struct Rr_DescriptorPoolSizeRatio
{
    VkDescriptorType Type;
    f32 Ratio;
} Rr_DescriptorPoolSizeRatio;

typedef struct Rr_DescriptorAllocator
{
    Rr_DescriptorPoolSizeRatio* Ratios;
    VkDescriptorPool* FullPools;
    VkDescriptorPool* ReadyPools;
    size_t SetsPerPool;
} Rr_DescriptorAllocator;

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

typedef struct Rr_DescriptorWriter
{
    VkDescriptorImageInfo* ImageInfos;
    VkDescriptorBufferInfo* BufferInfos;
    Rr_DescriptorWriterEntry* Entries;
    VkWriteDescriptorSet* Writes;
} Rr_DescriptorWriter;

typedef struct Rr_Frame
{
    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;
    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;
    Rr_DescriptorAllocator DescriptorAllocator;
    Rr_StagingBuffer* StagingBuffer;
    VkSemaphore* RetiredSemaphoresArray;
} Rr_Frame;

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
    Rr_Image ColorImage;
    Rr_Image DepthImage;
    VkFramebuffer Framebuffer;
} Rr_DrawTarget;

typedef struct Rr_Renderer
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

    /* Frame Data */
    void* PerFrameDatas;
    size_t PerFrameDataSize;
    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameNumber;

    Rr_DescriptorAllocator GlobalDescriptorAllocator;

    VkExtent2D ReferenceResolution;
    VkExtent2D ActiveResolution;
    i32 Scale;
    Rr_DrawTarget DrawTargets[RR_FRAME_OVERLAP];

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
    Rr_Renderer* Renderer;
    Rr_FrameTime FrameTime;
    Rr_AppConfig* Config;
} Rr_App;
