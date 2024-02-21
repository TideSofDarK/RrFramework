#pragma once

#define VK_NO_PROTOTYPES

#include "RendererTypes.h"

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

    // VmaAllocator Allocator;

    VkDebugUtilsMessengerEXT Messenger;

    u32 CommandBufferCount;

    SFrameData Frames[FRAME_OVERLAP];
    u64 FrameNumber;
} SRenderer;

void RendererInit(SApp* App);
void RendererCleanup(SRenderer* Renderer);
void RendererDraw(SRenderer* Renderer);
