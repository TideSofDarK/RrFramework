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

typedef struct SDL_Window SDL_Window;
typedef struct Rr_AppConfig Rr_AppConfig;
typedef struct Rr_GenericPipeline Rr_GenericPipeline;
typedef struct Rr_Material Rr_Material;
typedef struct Rr_App Rr_App;
typedef struct Rr_MeshBuffers Rr_MeshBuffers;

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
    VkExtent2D ReferenceResolution;
    VkExtent2D ActiveResolution;
    i32 Scale;
    Rr_Image ColorImage;
    Rr_Image DepthImage;
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
    Rr_DeviceQueue GraphicsQueue;
    Rr_DeviceQueue TransferQueue;
    Rr_DeviceQueue ComputeQueue;

    VmaAllocator Allocator;

    /* Frame Data */
    void* PerFrameDatas;
    size_t PerFrameDataSize;
    Rr_Frame Frames[RR_FRAME_OVERLAP];
    size_t FrameNumber;

    Rr_DescriptorAllocator GlobalDescriptorAllocator;

    Rr_DrawTarget DrawTarget;
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

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer);
void Rr_EndImmediate(Rr_Renderer* Renderer);

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
    const char* String;
    vec2 Position;
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
void Rr_DrawMesh(Rr_RenderingContext* RenderingContext, Rr_DrawMeshInfo* Info);
void Rr_DrawText(Rr_RenderingContext* RenderingContext, Rr_DrawTextInfo* Info);
void Rr_EndRendering(Rr_RenderingContext* RenderingContext);

static inline float Rr_GetAspectRatio(Rr_Renderer* Renderer)
{
    return (float)Renderer->DrawTarget.ActiveResolution.width / (float)Renderer->DrawTarget.ActiveResolution.height;
}

static inline size_t Rr_GetCurrentFrameIndex(Rr_Renderer* Renderer)
{
    return Renderer->FrameNumber % RR_FRAME_OVERLAP;
}

static inline Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* const Renderer)
{
    return &Renderer->Frames[Renderer->FrameNumber % RR_FRAME_OVERLAP];
}

static inline void* Rr_GetCurrentFrameData(Rr_Renderer* Renderer)
{
    return (char*)Renderer->PerFrameDatas + (Renderer->FrameNumber % RR_FRAME_OVERLAP) * Renderer->PerFrameDataSize;
}
