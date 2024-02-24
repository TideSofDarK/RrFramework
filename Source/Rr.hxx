#pragma once

#include <cglm/vec4.h>

#include "RrTypes.hxx"

#define FRAME_OVERLAP 2

typedef struct SComputeConstants
{
    vec4 Vec0;
    vec4 Vec1;
    vec4 Vec2;
    vec4 Vec3;
} SComputeConstants;

typedef struct SImGui
{
    b32 bInit;
    VkFence Fence;
    VkCommandBuffer CommandBuffer;
    VkCommandPool CommandPool;
    VkDescriptorPool DescriptorPool;
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

void Rr_Init(SRr* Rr, struct SDL_Window* Window);
void Rr_InitImGui(SRr* Rr, struct SDL_Window* Window);
void Rr_Cleanup(SRr* Rr);
void Rr_Draw(SRr* Rr);
void Rr_Resize(SRr* Rr, u32 Width, u32 Height);
