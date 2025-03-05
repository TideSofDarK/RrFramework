#include "Rr_Renderer.h"

#include "Rr_App.h"
#include "Rr_BuiltinAssets.inc"
#include "Rr_Image.h"
#include "Rr_Log.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <xxHash/xxhash.h>

#include <assert.h>

static void Rr_CleanupSwapchain(Rr_Renderer *Renderer, VkSwapchainKHR Swapchain)
{
    Rr_Device *Device = &Renderer->Device;

    for(uint32_t Index = 0; Index < Renderer->Swapchain.Images.Capacity;
        Index++)
    {
        Device->DestroyFramebuffer(
            Renderer->Device.Handle,
            Renderer->Swapchain.Images.Data[Index].Framebuffer,
            NULL);
        Device->DestroyImageView(
            Renderer->Device.Handle,
            Renderer->Swapchain.Images.Data[Index].View,
            NULL);

        Rr_ReturnSynchronizationState(
            Renderer,
            (Rr_MapKey)Renderer->Swapchain.Images.Data[Index].Handle);
    }

    if(Swapchain != VK_NULL_HANDLE)
    {
        Device->DestroySwapchainKHR(Renderer->Device.Handle, Swapchain, NULL);
    }
}

static bool Rr_IsSwapchainDirty(Rr_Renderer *Renderer)
{
    return Rr_GetAtomicInt(&Renderer->Swapchain.RecreatePending);
}

void Rr_SetSwapchainDirty(Rr_Renderer *Renderer, bool Dirty)
{
    Rr_SetAtomicInt(&Renderer->Swapchain.RecreatePending, Dirty);
}

static bool Rr_InitSwapchain(
    Rr_Renderer *Renderer,
    uint32_t *Width,
    uint32_t *Height)
{
    Rr_Instance *Instance = &Renderer->Instance;
    Rr_Device *Device = &Renderer->Device;

    VkSwapchainKHR OldSwapchain = Renderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfaceCapabilities;
    Instance->GetPhysicalDeviceSurfaceCapabilitiesKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &SurfaceCapabilities);

    if(SurfaceCapabilities.currentExtent.width == 0 ||
       SurfaceCapabilities.currentExtent.height == 0)
    {
        return false;
    }
    if(SurfaceCapabilities.currentExtent.width == UINT32_MAX)
    {
        Renderer->Swapchain.Extent.width = *Width;
        Renderer->Swapchain.Extent.height = *Height;
    }
    else
    {
        Renderer->Swapchain.Extent.width =
            SurfaceCapabilities.currentExtent.width;
        Renderer->Swapchain.Extent.height =
            SurfaceCapabilities.currentExtent.height;
        *Width = SurfaceCapabilities.currentExtent.width;
        *Height = SurfaceCapabilities.currentExtent.height;
    }
    Renderer->Swapchain.Extent.depth = 1;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    uint32_t PresentModeCount;
    Instance->GetPhysicalDeviceSurfacePresentModesKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &PresentModeCount,
        NULL);
    assert(PresentModeCount > 0);

    VkPresentModeKHR *PresentModes =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkPresentModeKHR, PresentModeCount);
    Instance->GetPhysicalDeviceSurfacePresentModesKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &PresentModeCount,
        PresentModes);

    VkPresentModeKHR SwapchainPresentMode;
    switch(Renderer->Swapchain.PresentMode)
    {
        case RR_PRESENT_MODE_FIFO_RELAXED:
            SwapchainPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            break;
        case RR_PRESENT_MODE_IMMEDIATE:
            SwapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        case RR_PRESENT_MODE_MAILBOX:
            SwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        default:
            SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            break;
    }
    bool PresentModeAvailable = false;
    for(uint32_t Index = 0; Index < PresentModeCount; Index++)
    {
        if(PresentModes[Index] == SwapchainPresentMode)
        {
            PresentModeAvailable = true;
            break;
        }
    }
    if(PresentModeAvailable == false)
    {
        SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        Renderer->Swapchain.PresentMode = RR_PRESENT_MODE_FIFO;
    }

    uint32_t DesiredNumberOfSwapchainImages =
        5; /* Should be safe enough for any present mode and Wayland. */
    if(SurfaceCapabilities.maxImageCount != 0)
    {
        DesiredNumberOfSwapchainImages = RR_CLAMP(
            RR_FRAME_OVERLAP + 1,
            SurfaceCapabilities.maxImageCount,
            DesiredNumberOfSwapchainImages);
    }

    VkSurfaceTransformFlagsKHR PreTransform;
    if(SurfaceCapabilities.supportedTransforms &
       VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        PreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        PreTransform = SurfaceCapabilities.currentTransform;
    }

    uint32_t FormatCount;
    Instance->GetPhysicalDeviceSurfaceFormatsKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &FormatCount,
        NULL);
    assert(FormatCount > 0);

    VkSurfaceFormatKHR *SurfaceFormats =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkSurfaceFormatKHR, FormatCount);
    Instance->GetPhysicalDeviceSurfaceFormatsKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &FormatCount,
        SurfaceFormats);

    bool PreferredFormatFound = false;
    for(uint32_t Index = 0; Index < FormatCount; Index++)
    {
        VkSurfaceFormatKHR *SurfaceFormat = &SurfaceFormats[Index];

        if(SurfaceFormat->format == VK_FORMAT_B8G8R8A8_UNORM ||
           SurfaceFormat->format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            Renderer->Swapchain.Format = SurfaceFormat->format;
            Renderer->Swapchain.ColorSpace = SurfaceFormat->colorSpace;
            PreferredFormatFound = true;
            break;
        }
    }

    if(!PreferredFormatFound)
    {
        RR_ABORT("No preferred surface format found!");
    }

    VkCompositeAlphaFlagBitsKHR CompositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR CompositeAlphaFlags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for(uint32_t Index = 0; Index < RR_ARRAY_COUNT(CompositeAlphaFlags);
        Index++)
    {
        VkCompositeAlphaFlagBitsKHR CompositeAlphaFlag =
            CompositeAlphaFlags[Index];
        if(SurfaceCapabilities.supportedCompositeAlpha & CompositeAlphaFlag)
        {
            CompositeAlpha = CompositeAlphaFlag;
            break;
        }
    }

    VkSwapchainCreateInfoKHR SwapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = Renderer->Surface,
        .minImageCount = DesiredNumberOfSwapchainImages,
        .imageFormat = Renderer->Swapchain.Format,
        .imageColorSpace = Renderer->Swapchain.ColorSpace,
        .imageExtent = { Renderer->Swapchain.Extent.width,
                         Renderer->Swapchain.Extent.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = (VkSurfaceTransformFlagBitsKHR)PreTransform,
        .compositeAlpha = CompositeAlpha,
        .presentMode = SwapchainPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = OldSwapchain,
    };

    if(SurfaceCapabilities.supportedUsageFlags &
       VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        SwapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if(SurfaceCapabilities.supportedUsageFlags &
       VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        SwapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    Device->CreateSwapchainKHR(
        Renderer->Device.Handle,
        &SwapchainCreateInfo,
        NULL,
        &Renderer->Swapchain.Handle);

    Rr_CleanupSwapchain(Renderer, OldSwapchain);

    /* Acquire Swapchain Images */

    uint32_t ImageCount = 0;
    Device->GetSwapchainImagesKHR(
        Renderer->Device.Handle,
        Renderer->Swapchain.Handle,
        &ImageCount,
        NULL);

    VkImage *Images = RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkImage, ImageCount);

    Device->GetSwapchainImagesKHR(
        Renderer->Device.Handle,
        Renderer->Swapchain.Handle,
        &ImageCount,
        Images);

    /* Initialize Present Pipeline If Needed */

    if(Renderer->PresentRenderPass == VK_NULL_HANDLE)
    {
        Rr_PipelineBinding PipelineBinding = {
            .Binding = 0,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
        };
        Rr_PipelineBindingSet PipelineBindingSet = {
            .BindingCount = 1,
            .Bindings = &PipelineBinding,
            .Stages = RR_SHADER_STAGE_FRAGMENT_BIT,
        };
        Renderer->PresentLayout =
            Rr_CreatePipelineLayout(Renderer, 1, &PipelineBindingSet);

        Rr_ColorTargetInfo ColorTargets[1] = { 0 };
        ColorTargets[0].Blend.ColorWriteMask = RR_COLOR_COMPONENT_ALL;
        ColorTargets[0].Format = Rr_GetSwapchainFormat(Renderer);

        Rr_GraphicsPipelineCreateInfo PipelineInfo = { 0 };
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_NONE;
        PipelineInfo.Rasterizer.FrontFace = RR_FRONT_FACE_CLOCKWISE;
        PipelineInfo.Layout = Renderer->PresentLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(RR_BUILTIN_PRESENT_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(RR_BUILTIN_PRESENT_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = ColorTargets;

        Renderer->PresentPipeline =
            Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

        Rr_RenderPassAttachment Attachment = {
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Format = Renderer->Swapchain.Format,
        };
        Rr_RenderPassInfo RenderPassInfo = { .AttachmentCount = 1,
                                             .Attachments = &Attachment };
        Renderer->PresentRenderPass =
            Rr_GetRenderPass(Renderer, &RenderPassInfo);
    }

    /* Create Framebuffers And Image Views */

    RR_RESERVE_SLICE(&Renderer->Swapchain.Images, ImageCount, Renderer->Arena);
    Renderer->Swapchain.Images.Count = ImageCount;

    VkImageViewCreateInfo ImageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = Renderer->Swapchain.Format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkFramebufferCreateInfo FramebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .attachmentCount = 1,
        .width = *Width,
        .height = *Height,
        .layers = 1,
        .renderPass = Renderer->PresentRenderPass,
    };

    for(uint32_t Index = 0; Index < ImageCount; Index++)
    {
        Rr_SwapchainImage *Image = Renderer->Swapchain.Images.Data + Index;

        Image->Handle = Images[Index];

        ImageViewCreateInfo.image = Image->Handle;
        Device->CreateImageView(
            Renderer->Device.Handle,
            &ImageViewCreateInfo,
            NULL,
            &Image->View);

        FramebufferCreateInfo.pAttachments = &Image->View;
        Device->CreateFramebuffer(
            Renderer->Device.Handle,
            &FramebufferCreateInfo,
            NULL,
            &Image->Framebuffer);

        Rr_SyncState *SyncState =
            Rr_GetSynchronizationState(Renderer, (Rr_MapKey)Image->Handle);
        SyncState->StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    Rr_DestroyScratch(Scratch);

    return true;
}

static void Rr_InitFrames(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;
    Rr_Frame *Frames = Renderer->Frames;

    VkFenceCreateInfo FenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkSemaphoreCreateInfo SemaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    for(size_t Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame *Frame = &Frames[Index];

        /* Command Pool */

        VkCommandPoolCreateInfo CommandPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        };
        Device->CreateCommandPool(
            Device->Handle,
            &CommandPoolCreateInfo,
            NULL,
            &Frame->CommandPool);

        /* Command Buffers */

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .commandPool = Frame->CommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        Device->AllocateCommandBuffers(
            Device->Handle,
            &CommandBufferAllocateInfo,
            &Frame->EarlyCommandBuffer);
        Device->AllocateCommandBuffers(
            Device->Handle,
            &CommandBufferAllocateInfo,
            &Frame->LateCommandBuffer);

        /* Synchronization */

        Device->CreateFence(
            Device->Handle,
            &FenceCreateInfo,
            NULL,
            &Frame->RenderFence);
        Device->CreateSemaphore(
            Device->Handle,
            &SemaphoreCreateInfo,
            NULL,
            &Frame->SwapchainSemaphore);
        Device->CreateSemaphore(
            Device->Handle,
            &SemaphoreCreateInfo,
            NULL,
            &Frame->EarlySemaphore);
        Device->CreateSemaphore(
            Device->Handle,
            &SemaphoreCreateInfo,
            NULL,
            &Frame->LateSemaphore);

        /* Descriptor Allocator */

        Rr_DescriptorPoolSizeRatio Ratios[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 32 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 32 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, 32 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32 },
        };
        Frame->DescriptorAllocator = Rr_CreateDescriptorAllocator(
            Device,
            1024,
            Ratios,
            RR_ARRAY_COUNT(Ratios),
            Renderer->Arena);

        Frame->Arena = Rr_CreateDefaultArena();
    }
}

static void Rr_CleanupFrames(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    for(size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame *Frame = &Renderer->Frames[Index];

        Device->DestroyCommandPool(Device->Handle, Frame->CommandPool, NULL);

        Device->DestroyFence(Device->Handle, Frame->RenderFence, NULL);
        Device->DestroySemaphore(Device->Handle, Frame->EarlySemaphore, NULL);
        Device->DestroySemaphore(Device->Handle, Frame->LateSemaphore, NULL);
        Device->DestroySemaphore(
            Device->Handle,
            Frame->SwapchainSemaphore,
            NULL);

        Rr_DestroyDescriptorAllocator(&Frame->DescriptorAllocator, Device);

        Rr_DestroyArena(Frame->Arena);
    }
}

static void Rr_InitVMA(Rr_Renderer *Renderer)
{
    Rr_Instance *Instance = &Renderer->Instance;
    Rr_Device *Device = &Renderer->Device;

    VmaVulkanFunctions VulkanFunctions = {
        .vkGetPhysicalDeviceProperties = Instance->GetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties =
            Instance->GetPhysicalDeviceMemoryProperties,
        .vkGetPhysicalDeviceMemoryProperties2KHR =
            Instance->GetPhysicalDeviceMemoryProperties2,
        .vkAllocateMemory = Device->AllocateMemory,
        .vkFreeMemory = Device->FreeMemory,
        .vkMapMemory = Device->MapMemory,
        .vkUnmapMemory = Device->UnmapMemory,
        .vkFlushMappedMemoryRanges = Device->FlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = Device->InvalidateMappedMemoryRanges,
        .vkBindBufferMemory = Device->BindBufferMemory,
        .vkBindImageMemory = Device->BindImageMemory,
        .vkGetBufferMemoryRequirements = Device->GetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = Device->GetImageMemoryRequirements,
        .vkCreateBuffer = Device->CreateBuffer,
        .vkDestroyBuffer = Device->DestroyBuffer,
        .vkCreateImage = Device->CreateImage,
        .vkDestroyImage = Device->DestroyImage,
        .vkCmdCopyBuffer = Device->CmdCopyBuffer,
        // .vkGetBufferMemoryRequirements2KHR =
        // Device->GetBufferMemoryRequirements2,
        // .vkGetImageMemoryRequirements2KHR =
        // Device->GetImageMemoryRequirements2, .vkBindBufferMemory2KHR =
        // Device->BindBufferMemory2, .vkBindImageMemory2KHR =
        // Device->BindImageMemory2,
    };
    VmaAllocatorCreateInfo AllocatorInfo = {
        .flags = 0,
        .physicalDevice = Renderer->PhysicalDevice.Handle,
        .device = Renderer->Device.Handle,
        .pVulkanFunctions = &VulkanFunctions,
        .instance = Renderer->Instance.Handle,
    };
    vmaCreateAllocator(&AllocatorInfo, &Renderer->Allocator);
}

static void Rr_InitImmediateMode(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;
    Rr_ImmediateMode *ImmediateMode = &Renderer->ImmediateMode;

    VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
    };
    Device->CreateCommandPool(
        Device->Handle,
        &CommandPoolInfo,
        NULL,
        &ImmediateMode->CommandPool);

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .commandPool = ImmediateMode->CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    Device->AllocateCommandBuffers(
        Device->Handle,
        &CommandBufferAllocateInfo,
        &ImmediateMode->CommandBuffer);

    VkFenceCreateInfo FenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    Device->CreateFence(
        Device->Handle,
        &FenceCreateInfo,
        NULL,
        &ImmediateMode->Fence);
}

static void Rr_CleanupImmediateMode(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    Device->DestroyCommandPool(
        Device->Handle,
        Renderer->ImmediateMode.CommandPool,
        NULL);
    Device->DestroyFence(Device->Handle, Renderer->ImmediateMode.Fence, NULL);
}

/* @TODO: Move to queue initialization? */
static void Rr_InitTransientCommandPools(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    Device->CreateCommandPool(
        Device->Handle,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        },
        NULL,
        &Renderer->GraphicsQueue.TransientCommandPool);

    Device->CreateCommandPool(
        Device->Handle,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
        },
        NULL,
        &Renderer->TransferQueue.TransientCommandPool);
}

static void Rr_CleanupTransientCommandPools(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;
    Device->DestroyCommandPool(
        Device->Handle,
        Renderer->GraphicsQueue.TransientCommandPool,
        NULL);
    Device->DestroyCommandPool(
        Device->Handle,
        Renderer->TransferQueue.TransientCommandPool,
        NULL);
}

// static void Rr_InitNullTextures(Rr_App *App)
// {
//     Rr_Renderer *Renderer = &App->Renderer;
//
//     VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);
//     Rr_WriteBuffer StagingBuffer = { .Buffer = Rr_CreateMappedBuffer(App,
//     256, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
//                                      .Offset = 0 };
//     Rr_UploadContext UploadContext = {
//         .StagingBuffer = &StagingBuffer,
//         .TransferCommandBuffer = CommandBuffer,
//     };
//     uint32_t WhiteData = 0xffffffff;
//     Renderer->NullTextures.White = Rr_CreateColorImageFromMemory(App,
//     &UploadContext, (char *)&WhiteData, 1, 1, false); uint32_t NormalData =
//     0xffff8888; Renderer->NullTextures.Normal =
//         Rr_CreateColorImageFromMemory(App, &UploadContext, (char
//         *)&NormalData, 1, 1, false);
//     Rr_EndImmediate(Renderer);
//
//     Rr_DestroyBuffer(App, StagingBuffer.Buffer);
// }

Rr_Renderer *Rr_CreateRenderer(Rr_App *App)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_Renderer *Renderer = RR_ALLOC_TYPE(Arena, Rr_Renderer);
    Renderer->Arena = Arena;

    SDL_Window *Window = App->Window;
    // Rr_AppConfig *Config = App->Config;

    Rr_InitLoader(&Renderer->Loader);
    Rr_InitInstance(
        &Renderer->Loader,
        SDL_GetWindowTitle(App->Window),
        &Renderer->Instance);
    Rr_InitSurface(Window, &Renderer->Instance, &Renderer->Surface);
    Rr_InitDeviceAndQueues(
        &Renderer->Instance,
        Renderer->Surface,
        &Renderer->PhysicalDevice,
        &Renderer->Device,
        &Renderer->GraphicsQueue,
        &Renderer->TransferQueue);

    Rr_InitVMA(Renderer);
    Rr_InitTransientCommandPools(Renderer);
    uint32_t Width, Height;
    SDL_GetWindowSizeInPixels(Window, (int32_t *)&Width, (int32_t *)&Height);
    Rr_InitSwapchain(Renderer, &Width, &Height);
    Rr_InitFrames(Renderer);
    Rr_InitImmediateMode(Renderer);
    // Rr_InitNullTextures(App);
    // Rr_InitTextRenderer(App);

    Rr_DestroyScratch(Scratch);

    return Renderer;
}

bool Rr_NewFrame(Rr_App *App, void *Window)
{
    Rr_Renderer *Renderer = App->Renderer;
    Rr_Device *Device = &Renderer->Device;

    if(Rr_IsSwapchainDirty(Renderer) == true)
    {
        Device->DeviceWaitIdle(Device->Handle);

        int32_t Width, Height;
        SDL_GetWindowSizeInPixels(Window, &Width, &Height);

        bool Minimized = (SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED);

        if(!Minimized && Width > 0 && Height > 0 &&
           Rr_InitSwapchain(Renderer, (uint32_t *)&Width, (uint32_t *)&Height))
        {
            Rr_SetSwapchainDirty(Renderer, false);

            return true;
        }

        return false;
    }
    return true;
}

void Rr_DestroyRenderer(Rr_App *App, Rr_Renderer *Renderer)
{
    Rr_Instance *Instance = &Renderer->Instance;
    Rr_Device *Device = &Renderer->Device;

    Device->DeviceWaitIdle(Device->Handle);

    App->Config->CleanupFunc(App, App->UserData);

    // Rr_CleanupTextRenderer(App);

    for(size_t Index = 0; Index < Renderer->RenderPasses.Count; ++Index)
    {
        Device->DestroyRenderPass(
            Device->Handle,
            Renderer->RenderPasses.Data[Index].Handle,
            NULL);
    }

    for(size_t Index = 0; Index < Renderer->Framebuffers.Count; ++Index)
    {
        Device->DestroyFramebuffer(
            Device->Handle,
            Renderer->Framebuffers.Data[Index].Handle,
            NULL);
    }

    Rr_CleanupFrames(Renderer);

    // Rr_DestroyImage(App, Renderer->NullTextures.White);
    // Rr_DestroyImage(App, Renderer->NullTextures.Normal);

    Rr_CleanupTransientCommandPools(Renderer);
    Rr_CleanupImmediateMode(Renderer);

    Rr_CleanupSwapchain(Renderer, Renderer->Swapchain.Handle);
    Rr_DestroyGraphicsPipeline(Renderer, Renderer->PresentPipeline);
    Rr_DestroyPipelineLayout(Renderer, Renderer->PresentLayout);

    for(size_t Index = 0; Index < Renderer->DescriptorSetLayouts.Count; ++Index)
    {
        Rr_DescriptorSetLayout *DescriptorSetLayout =
            Renderer->DescriptorSetLayouts.Data + Index;
        Device->DestroyDescriptorSetLayout(
            Device->Handle,
            DescriptorSetLayout->Handle,
            NULL);
    }

    vmaDestroyAllocator(Renderer->Allocator);

    Instance->DestroySurfaceKHR(Instance->Handle, Renderer->Surface, NULL);
    Device->DestroyDevice(Device->Handle, NULL);

    Instance->DestroyInstance(Instance->Handle, NULL);

    Rr_DestroyArena(Renderer->Arena);
}

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    Rr_ImmediateMode *ImmediateMode = &Renderer->ImmediateMode;
    Device->ResetFences(Device->Handle, 1, &ImmediateMode->Fence);
    Device->ResetCommandBuffer(ImmediateMode->CommandBuffer, 0);

    VkCommandBufferBeginInfo BeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };
    Device->BeginCommandBuffer(ImmediateMode->CommandBuffer, &BeginInfo);

    return ImmediateMode->CommandBuffer;
}

void Rr_EndImmediate(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    Rr_ImmediateMode *ImmediateMode = &Renderer->ImmediateMode;

    Device->EndCommandBuffer(ImmediateMode->CommandBuffer);

    VkSubmitInfo SubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &ImmediateMode->CommandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
    };

    Device->QueueSubmit(
        Renderer->GraphicsQueue.Handle,
        1,
        &SubmitInfo,
        ImmediateMode->Fence);
    Device->WaitForFences(
        Device->Handle,
        1,
        &ImmediateMode->Fence,
        true,
        UINT64_MAX);
}

static void Rr_ProcessPendingLoads(Rr_App *App)
{
    Rr_Renderer *Renderer = App->Renderer;

    if(Rr_TryLockSpinLock(&App->SyncArena.Lock))
    {
        for(size_t Index = 0; Index < Renderer->PendingLoadsSlice.Count;
            ++Index)
        {
            Rr_PendingLoad *PendingLoad =
                &Renderer->PendingLoadsSlice.Data[Index];
            PendingLoad->LoadingCallback(App, PendingLoad->UserData);
        }
        RR_EMPTY_SLICE(&Renderer->PendingLoadsSlice);

        Rr_UnlockSpinLock(&App->SyncArena.Lock);
    }
}

void Rr_PrepareFrame(Rr_App *App)
{
    Rr_Renderer *Renderer = App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_ResetArena(Frame->Arena);

    /* Make sure virtual swapchain image has latest extent and format values. */

    Frame->VirtualSwapchainImage = RR_ALLOC_TYPE(Frame->Arena, Rr_Image);
    *Frame->VirtualSwapchainImage = (Rr_Image){
        .AllocatedImageCount = 1,
        .Extent = Renderer->Swapchain.Extent,
        .Format = Renderer->Swapchain.Format,
        .AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    Frame->Graph = RR_ALLOC_TYPE(Frame->Arena, Rr_Graph);
    Frame->Graph->Arena = Frame->Arena;
    Frame->Graph->SwapchainImageResourceIndex =
        Rr_GetGraphImageHandle(Frame->Graph, Frame->VirtualSwapchainImage)
            ->Values.Index;

    Rr_ProcessPendingLoads(App);
}

void Rr_DrawFrame(Rr_App *App)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Renderer *Renderer = App->Renderer;
    Rr_Device *Device = &Renderer->Device;
    Rr_Swapchain *Swapchain = &Renderer->Swapchain;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Device->WaitForFences(
        Device->Handle,
        1,
        &Frame->RenderFence,
        true,
        1000000000);
    Device->ResetFences(Device->Handle, 1, &Frame->RenderFence);

    Rr_ResetDescriptorAllocator(&Frame->DescriptorAllocator, Device);

    /* Acquire swapchain image. */

    uint32_t SwapchainImageIndex;
    VkResult Result = Device->AcquireNextImageKHR(
        Device->Handle,
        Swapchain->Handle,
        1000000000,
        Frame->SwapchainSemaphore,
        VK_NULL_HANDLE,
        &SwapchainImageIndex);
    if(Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Rr_SetSwapchainDirty(Renderer, true);
        return;
    }
    if(Result == VK_SUBOPTIMAL_KHR)
    {
        Rr_SetSwapchainDirty(Renderer, true);
    }
    assert(Result >= 0);

    VkImage SwapchainImage =
        Renderer->Swapchain.Images.Data[SwapchainImageIndex].Handle;

    /* Now that we acquired swapchain image index we can
     * put real handles to virtual swapchain image which
     * will be used by the graph. */

    Frame->VirtualSwapchainImage->AllocatedImages[0] = (Rr_AllocatedImage){
        .View = Renderer->Swapchain.Images.Data[SwapchainImageIndex].View,
        .Handle = SwapchainImage,
        .Container = Frame->VirtualSwapchainImage,
    };
    Frame->SwapchainFramebuffer =
        Renderer->Swapchain.Images.Data[SwapchainImageIndex].Framebuffer;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };

    /* Execute Frame Graph */

    Device->BeginCommandBuffer(
        Frame->EarlyCommandBuffer,
        &CommandBufferBeginInfo);
    Device->BeginCommandBuffer(
        Frame->LateCommandBuffer,
        &CommandBufferBeginInfo);

    Rr_ExecuteGraph(Renderer, Frame->Graph, Scratch.Arena);

    Device->EndCommandBuffer(Frame->EarlyCommandBuffer);

    /* Always transition swapchain image to present layout. */

    Rr_SyncState *SyncState =
        Rr_GetSynchronizationState(Renderer, (Rr_MapKey)SwapchainImage);
    Device->CmdPipelineBarrier(
        Frame->LateCommandBuffer,
        SyncState->StageMask,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &(VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image = SwapchainImage,
            .oldLayout = SyncState->Specific.Layout,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcAccessMask = SyncState->AccessMask,
            .dstAccessMask = 0,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        });
    SyncState->AccessMask = 0;
    SyncState->StageMask =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; /* Doesn't seems right. */
    SyncState->Specific.Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    Device->EndCommandBuffer(Frame->LateCommandBuffer);

    /* Submit frame command buffer and queue present. */

    VkSubmitInfo SubmitInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &Frame->EarlyCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &Frame->EarlySemaphore,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &Frame->LateCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &Frame->LateSemaphore,
            .waitSemaphoreCount = 2,
            .pWaitSemaphores =
                (VkSemaphore[]){
                    Frame->EarlySemaphore,
                    Frame->SwapchainSemaphore,
                },
            .pWaitDstStageMask =
                (VkPipelineStageFlags[]){
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                },
        },
    };

    Rr_LockSpinLock(&Renderer->GraphicsQueue.Lock);

    Device->QueueSubmit(
        Renderer->GraphicsQueue.Handle,
        2,
        SubmitInfos,
        Frame->RenderFence);

    Rr_UnlockSpinLock(&Renderer->GraphicsQueue.Lock);

    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->LateSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    Result =
        Device->QueuePresentKHR(Renderer->GraphicsQueue.Handle, &PresentInfo);
    if(Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Rr_SetSwapchainDirty(Renderer, true);
    }

    Renderer->FrameNumber++;
    Renderer->CurrentFrameIndex = Renderer->FrameNumber % RR_FRAME_OVERLAP;

    Rr_DestroyScratch(Scratch);
}

Rr_Frame *Rr_GetCurrentFrame(Rr_Renderer *Renderer)
{
    return &Renderer->Frames[Renderer->CurrentFrameIndex];
}

bool Rr_IsUsingTransferQueue(Rr_Renderer *Renderer)
{
    return Renderer->TransferQueue.Handle != VK_NULL_HANDLE;
}

size_t Rr_GetUniformAlignment(Rr_Renderer *Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits
        .minUniformBufferOffsetAlignment;
}

size_t Rr_GetStorageAlignment(Rr_Renderer *Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits
        .minStorageBufferOffsetAlignment;
}

size_t Rr_GetMaxComputeSharedMemorySize(Rr_Renderer *Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits
        .maxComputeSharedMemorySize;
}

size_t Rr_GetMaxComputeWorkgroupInvocations(Rr_Renderer *Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits
        .maxComputeWorkGroupInvocations;
}

Rr_Graph *Rr_GetGraph(Rr_Renderer *Renderer)
{
    return Rr_GetCurrentFrame(Renderer)->Graph;
}

Rr_Arena *Rr_GetFrameArena(Rr_Renderer *Renderer)
{
    return Rr_GetCurrentFrame(Renderer)->Arena;
}

Rr_TextureFormat Rr_GetSwapchainFormat(Rr_Renderer *Renderer)
{
    return Rr_GetTextureFormat(Renderer->Swapchain.Format);
}

Rr_IntVec2 Rr_GetSwapchainSize(Rr_Renderer *Renderer)
{
    return (Rr_IntVec2){
        (int32_t)Renderer->Swapchain.Extent.width,
        (int32_t)Renderer->Swapchain.Extent.height,
    };
}

Rr_Image *Rr_GetSwapchainImage(Rr_Renderer *Renderer)
{
    return Rr_GetCurrentFrame(Renderer)->VirtualSwapchainImage;
}

bool Rr_SetSwapchainPresentMode(
    Rr_Renderer *Renderer,
    Rr_PresentMode PresentMode)
{
    Renderer->Swapchain.PresentMode = PresentMode;
    Rr_SetSwapchainDirty(Renderer, true);

    return true;
}

VkRenderPass Rr_GetRenderPass(Rr_Renderer *Renderer, Rr_RenderPassInfo *Info)
{
    assert(Info != NULL);

    uint32_t Hash = XXH32(
        Info->Attachments,
        sizeof(Rr_RenderPassAttachment) * Info->AttachmentCount,
        0);

    for(size_t Index = 0; Index < Renderer->RenderPasses.Count; ++Index)
    {
        if(Renderer->RenderPasses.Data[Index].Hash == Hash)
        {
            return Renderer->RenderPasses.Data[Index].Handle;
        }
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkAttachmentDescription *Attachments = RR_ALLOC_TYPE_COUNT(
        Scratch.Arena,
        VkAttachmentDescription,
        Info->AttachmentCount);

    size_t ColorCount = 0;
    VkAttachmentReference *ColorReferences = RR_ALLOC_TYPE_COUNT(
        Scratch.Arena,
        VkAttachmentReference,
        Info->AttachmentCount);
    VkAttachmentReference *DepthReference = NULL;

    for(size_t Index = 0; Index < Info->AttachmentCount; ++Index)
    {
        Rr_RenderPassAttachment *Attachment = &Info->Attachments[Index];
        if(Rr_IsVulkanDepthFormat(Attachment->Format))
        {
            if(DepthReference != NULL)
            {
                RR_ABORT("Can't have more than one depth attachment!");
            }
            DepthReference =
                RR_ALLOC(Scratch.Arena, sizeof(VkAttachmentDescription));
            DepthReference->attachment = Index;
            DepthReference->layout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            Attachments[Index] = (VkAttachmentDescription){
                .samples = 1,
                .format = Attachment->Format,
                .initialLayout =
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .flags = 0,
                .loadOp = Rr_GetLoadOp(Attachment->LoadOp),
                .storeOp = Rr_GetStoreOp(Attachment->StoreOp),
            };
        }
        else
        {
            ColorCount++;
            ColorReferences[Index] = (VkAttachmentReference){
                .attachment = Index,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            Attachments[Index] = (VkAttachmentDescription){
                .samples = 1,
                .format = Attachment->Format,
                .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .flags = 0,
                .loadOp = Rr_GetLoadOp(Attachment->LoadOp),
                .storeOp = Rr_GetStoreOp(Attachment->StoreOp),
            };
        }
    }

    VkSubpassDescription SubpassDescription = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = ColorCount,
        .pColorAttachments = ColorReferences,
        .pDepthStencilAttachment = DepthReference,
        .pResolveAttachments = NULL,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    VkRenderPassCreateInfo RenderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .attachmentCount = Info->AttachmentCount,
        .pAttachments = Attachments,
        .subpassCount = 1,
        .pSubpasses = &SubpassDescription,
        .dependencyCount = 0,
        .pDependencies = NULL,
    };

    VkRenderPass RenderPass = VK_NULL_HANDLE;

    Rr_Device *Device = &Renderer->Device;

    Device->CreateRenderPass(
        Device->Handle,
        &RenderPassCreateInfo,
        NULL,
        &RenderPass);

    *RR_PUSH_SLICE(&Renderer->RenderPasses, Renderer->Arena) = (Rr_RenderPass){
        .Handle = RenderPass,
        .Hash = Hash,
    };

    Rr_DestroyScratch(Scratch);

    return RenderPass;
}

static VkFramebuffer Rr_GetFramebufferInternal(
    Rr_Renderer *Renderer,
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    size_t QuerySize = sizeof(VkImageView) * ImageViewCount +
                       sizeof(VkExtent3D) + sizeof(RenderPass); /* NOLINT */
    void *Query = RR_ALLOC(Scratch.Arena, QuerySize);
    memcpy(Query, ImageViews, sizeof(VkImageView) * ImageViewCount);
    memcpy(
        ((char *)Query) + sizeof(VkImageView) * ImageViewCount,
        &Extent,
        sizeof(VkExtent3D));
    memcpy(
        ((char *)Query) + sizeof(VkImageView) * ImageViewCount + sizeof(Extent),
        &RenderPass,
        sizeof(RenderPass)); /* NOLINT */

    uint32_t Hash = XXH32(Query, QuerySize, 0);

    VkFramebuffer Framebuffer = VK_NULL_HANDLE;

    for(size_t Index = 0; Index < Renderer->Framebuffers.Count; ++Index)
    {
        Rr_Framebuffer *CachedFramebuffer = Renderer->Framebuffers.Data + Index;

        if(CachedFramebuffer->Hash == Hash)
        {
            return CachedFramebuffer->Handle;
        }
    }

    VkFramebufferCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = RenderPass,
        .height = Extent.height,
        .width = Extent.width,
        .layers = Extent.depth,
        .attachmentCount = ImageViewCount,
        .pAttachments = ImageViews,
    };

    Rr_Device *Device = &Renderer->Device;

    Device->CreateFramebuffer(Device->Handle, &CreateInfo, NULL, &Framebuffer);

    *RR_PUSH_SLICE(&Renderer->Framebuffers, Renderer->Arena) = (Rr_Framebuffer){
        .Handle = Framebuffer,
        .Hash = Hash,
    };

    Rr_DestroyScratch(Scratch);

    return Framebuffer;
}

VkFramebuffer Rr_GetFramebufferViews(
    Rr_Renderer *Renderer,
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent)
{
    return Rr_GetFramebufferInternal(
        Renderer,
        RenderPass,
        ImageViews,
        ImageViewCount,
        Extent,
        NULL);
}

VkFramebuffer Rr_GetFramebuffer(
    Rr_Renderer *Renderer,
    VkRenderPass RenderPass,
    Rr_Image *Images,
    size_t ImageCount,
    VkExtent3D Extent)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkImageView *ImageViews =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkImageView, ImageCount);

    for(size_t Index = 0; Index < ImageCount; ++Index)
    {
        ImageViews[Index] =
            Rr_GetCurrentAllocatedImage(Renderer, Images + Index)->View;
    }

    VkFramebuffer Framebuffer = Rr_GetFramebufferInternal(
        Renderer,
        RenderPass,
        ImageViews,
        ImageCount,
        Extent,
        Scratch.Arena);

    Rr_DestroyScratch(Scratch);

    return Framebuffer;
}

Rr_SyncState *Rr_GetSynchronizationState(Rr_Renderer *Renderer, Rr_MapKey Key)
{
    Rr_SyncState **SyncStateRef =
        RR_UPSERT(&Renderer->GlobalSync, Key, Renderer->Arena);
    if(*SyncStateRef != NULL)
    {
        return *SyncStateRef;
    }
    *SyncStateRef =
        RR_GET_FREE_LIST_ITEM(&Renderer->SyncStates, Renderer->Arena);
    Rr_SyncState *SyncState = *SyncStateRef;
    RR_ZERO_PTR(SyncState);
    return SyncState;
}

void Rr_ReturnSynchronizationState(Rr_Renderer *Renderer, Rr_MapKey Key)
{
    Rr_SyncState **SyncStateRef =
        RR_UPSERT(&Renderer->GlobalSync, Key, Renderer->Arena);
    if(*SyncStateRef != NULL)
    {
        RR_RETURN_FREE_LIST_ITEM(&Renderer->SyncStates, *SyncStateRef);
    }
    *SyncStateRef = NULL;
}
