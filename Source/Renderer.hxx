#pragma once

#define VK_NO_PROTOTYPES

#include "RendererTypes.hxx"

#define FRAME_OVERLAP 2

typedef struct SApp SApp;

typedef struct SRenderer
{
    VkInstance Instance;
    SPhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkSurfaceKHR Surface;
    SRendererSwapchain Swapchain;

    SRendererQueue GraphicsQueue;
    SRendererQueue TransferQueue;
    SRendererQueue ComputeQueue;

    VmaAllocator Allocator;

    VkDebugUtilsMessengerEXT Messenger;

    SFrameData Frames[FRAME_OVERLAP];
    u64 FrameNumber;

    SAllocatedImage DrawImage;
    VkExtent2D DrawExtent;
} SRenderer;

void Renderer_Init(SRenderer* Renderer, struct SDL_Window* Window);
void Renderer_Cleanup(SRenderer* Renderer);
void Renderer_Draw(SRenderer* Renderer);
void Renderer_Resize(SRenderer* Renderer, u32 Width, u32 Height);
