#pragma once

#include <cglm/vec3.h>
#include <cglm/vec4.h>

#include <SDL3/SDL_atomic.h>

#include "Rr_Asset.h"
#include "Rr_Vulkan.h"
#include "Rr_Descriptor.h"
#include "Rr_Buffer.h"
#include "Rr_Image.h"
#include "Rr_Text.h"
#include "Rr_Load.h"

typedef enum Rr_DrawTextFlags
{
    RR_DRAW_TEXT_FLAGS_ANIMATION_BIT = 1
} Rr_DrawTextFlags;

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Mutex SDL_Mutex;
typedef struct Rr_AppConfig Rr_AppConfig;
typedef struct Rr_GenericPipeline Rr_GenericPipeline;
typedef struct Rr_Material Rr_Material;
typedef struct Rr_App Rr_App;
typedef struct Rr_MeshBuffers Rr_MeshBuffers;
typedef struct Rr_LoadingContext Rr_LoadingContext;
typedef struct Rr_PendingLoad Rr_PendingLoad;

typedef struct Rr_Vertex
{
    vec3 Position;
    f32 TexCoordX;
    vec3 Normal;
    f32 TexCoordY;
    vec4 Color;
} Rr_Vertex;

typedef struct Rr_Frame
{
    VkCommandPool CommandPool;
    VkCommandBuffer MainCommandBuffer;
    VkSemaphore SwapchainSemaphore;
    VkSemaphore RenderSemaphore;
    VkFence RenderFence;
    Rr_DescriptorAllocator DescriptorAllocator;
    Rr_StagingBuffer StagingBuffer;
} Rr_Frame;

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

typedef struct Rr_DeviceQueue
{
    VkQueue Handle;
    u32 FamilyIndex;
} Rr_DeviceQueue;

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
        VkQueue Queue;
        u32 FamilyIndex;
    } Graphics;

    struct
    {
        VkCommandPool TransientCommandPool;
        VkQueue Queue;
        u32 FamilyIndex;
    } Transfer;

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

void Rr_InitRenderer(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_CleanupRenderer(Rr_App* App);
void Rr_Draw(Rr_App* App);
b8 Rr_NewFrame(Rr_Renderer* Renderer, SDL_Window* Window);

VkCommandBuffer Rr_BeginImmediate(const Rr_Renderer* Renderer);
void Rr_EndImmediate(const Rr_Renderer* Renderer);

/* Rendering */
typedef struct Rr_BeginRenderingInfo
{
    Rr_GenericPipeline* Pipeline;
    Rr_Image* InitialColor;
    Rr_Image* InitialDepth;
    Rr_Image* AdditionalAttachment;
    const void* GlobalsData;
} Rr_BeginRenderingInfo;
typedef struct Rr_DrawMeshInfo
{
    Rr_Material* Material;
    Rr_MeshBuffers* MeshBuffers;
    const void* DrawData;
} Rr_DrawMeshInfo;
typedef struct Rr_DrawTextInfo
{
    Rr_Font* Font;
    Rr_String* String;
    vec2 Position;
    f32 Size;
    Rr_DrawTextFlags Flags;
} Rr_DrawTextInfo;
typedef struct Rr_RenderingContext
{
    Rr_BeginRenderingInfo* Info;
    Rr_Renderer* Renderer;
    VkCommandBuffer CommandBuffer;
    Rr_DrawMeshInfo* DrawMeshArray;
    Rr_DrawTextInfo* DrawTextArray;
} Rr_RenderingContext;
Rr_RenderingContext Rr_BeginRendering(Rr_Renderer* Renderer, Rr_BeginRenderingInfo* Info);
void Rr_DrawMesh(Rr_RenderingContext* RenderingContext, const Rr_DrawMeshInfo* Info);
void Rr_DrawText(Rr_RenderingContext* RenderingContext, const Rr_DrawTextInfo* Info);
void Rr_EndRendering(Rr_RenderingContext* RenderingContext);

static float Rr_GetAspectRatio(const Rr_Renderer* Renderer)
{
    return (float)Renderer->ActiveResolution.width / (float)Renderer->ActiveResolution.height;
}

static size_t Rr_GetCurrentFrameIndex(const Rr_Renderer* Renderer)
{
    return Renderer->FrameNumber % RR_FRAME_OVERLAP;
}

static Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* const Renderer)
{
    return &Renderer->Frames[Renderer->FrameNumber % RR_FRAME_OVERLAP];
}

static void* Rr_GetCurrentFrameData(const Rr_Renderer* Renderer)
{
    return (char*)Renderer->PerFrameDatas + (Renderer->FrameNumber % RR_FRAME_OVERLAP) * Renderer->PerFrameDataSize;
}
