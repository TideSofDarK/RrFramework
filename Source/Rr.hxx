#pragma once

#include "RrTypes.hxx"

#define FRAME_OVERLAP 2

typedef struct SImmediateMode
{
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
} SImmediateMode;

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

    VkDebugUtilsMessengerEXT Messenger;

    SFrameData Frames[FRAME_OVERLAP];
    u64 FrameNumber;

    SDescriptorAllocator GlobalDescriptorAllocator;

    SAllocatedImage DrawImage;
    VkExtent2D DrawExtent;
    VkDescriptorSet DrawImageDescriptors;
    VkDescriptorSetLayout DrawImageDescriptorLayout;
	VkPipeline GradientPipeline;
	VkPipelineLayout GradientPipelineLayout;

    SImmediateMode ImmediateMode;
} SRr;

void RR_Init(SRr* Rr, struct SDL_Window* Window);
void RR_Cleanup(SRr* Rr);
void RR_Draw(SRr* Rr);
void RR_Resize(SRr* Rr, u32 Width, u32 Height);
