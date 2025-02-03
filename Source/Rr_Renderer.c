#include "Rr_Renderer.h"

#include "Rr_App.h"
#include "Rr_Buffer.h"
#include "Rr_BuiltinAssets.inc"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_UI.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <xxHash/xxhash.h>

#include <assert.h>

static void Rr_CleanupSwapchain(Rr_App *App, VkSwapchainKHR Swapchain)
{
    Rr_Renderer *Renderer = &App->Renderer;

    for(uint32_t Index = 0; Index < Renderer->Swapchain.ImageViews.Capacity; Index++)
    {
        vkDestroyFramebuffer(Renderer->Device, Renderer->Swapchain.Framebuffers.Data[Index], NULL);
        vkDestroyImageView(Renderer->Device, Renderer->Swapchain.ImageViews.Data[Index], NULL);
    }
    vkDestroySwapchainKHR(Renderer->Device, Swapchain, NULL);
}

static bool Rr_InitSwapchain(Rr_App *App, uint32_t *Width, uint32_t *Height)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VkSwapchainKHR OldSwapchain = Renderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &SurfCaps);

    if(SurfCaps.currentExtent.width == 0 || SurfCaps.currentExtent.height == 0)
    {
        return false;
    }
    if(SurfCaps.currentExtent.width == UINT32_MAX)
    {
        Renderer->Swapchain.Extent.width = *Width;
        Renderer->Swapchain.Extent.height = *Height;
    }
    else
    {
        Renderer->Swapchain.Extent.width = SurfCaps.currentExtent.width;
        Renderer->Swapchain.Extent.height = SurfCaps.currentExtent.height;
        *Width = SurfCaps.currentExtent.width;
        *Height = SurfCaps.currentExtent.height;
    }
    Renderer->Swapchain.Extent.depth = 1;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    uint32_t PresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &PresentModeCount,
        NULL);
    assert(PresentModeCount > 0);

    VkPresentModeKHR *PresentModes = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, VkPresentModeKHR, PresentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &PresentModeCount,
        PresentModes);

    VkPresentModeKHR SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for(uint32_t Index = 0; Index < PresentModeCount; Index++)
    {
        if(PresentModes[Index] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            SwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if(PresentModes[Index] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            SwapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    uint32_t DesiredNumberOfSwapchainImages = SurfCaps.minImageCount + 1;
    if((SurfCaps.maxImageCount > 0) && (DesiredNumberOfSwapchainImages > SurfCaps.maxImageCount))
    {
        DesiredNumberOfSwapchainImages = SurfCaps.maxImageCount;
    }

    VkSurfaceTransformFlagsKHR PreTransform;
    if(SurfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        PreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        PreTransform = SurfCaps.currentTransform;
    }

    uint32_t FormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &FormatCount, NULL);
    assert(FormatCount > 0);

    VkSurfaceFormatKHR *SurfaceFormats = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, VkSurfaceFormatKHR, FormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &FormatCount,
        SurfaceFormats);

    bool PreferredFormatFound = false;
    for(uint32_t Index = 0; Index < FormatCount; Index++)
    {
        VkSurfaceFormatKHR *SurfaceFormat = &SurfaceFormats[Index];

        if(SurfaceFormat->format == VK_FORMAT_B8G8R8A8_UNORM || SurfaceFormat->format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            Renderer->Swapchain.Format = SurfaceFormat->format;
            Renderer->Swapchain.ColorSpace = SurfaceFormat->colorSpace;
            PreferredFormatFound = true;
            break;
        }
    }

    if(!PreferredFormatFound)
    {
        Rr_LogAbort("No preferred surface format found!");
    }

    VkCompositeAlphaFlagBitsKHR CompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR CompositeAlphaFlags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for(uint32_t Index = 0; Index < RR_ARRAY_COUNT(CompositeAlphaFlags); Index++)
    {
        VkCompositeAlphaFlagBitsKHR CompositeAlphaFlag = CompositeAlphaFlags[Index];
        if(SurfCaps.supportedCompositeAlpha & CompositeAlphaFlag)
        {
            CompositeAlpha = CompositeAlphaFlag;
            break;
        }
    }

    VkSwapchainCreateInfoKHR SwapchainCI = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = Renderer->Surface,
        .minImageCount = DesiredNumberOfSwapchainImages,
        .imageFormat = Renderer->Swapchain.Format,
        .imageColorSpace = Renderer->Swapchain.ColorSpace,
        .imageExtent = { Renderer->Swapchain.Extent.width, Renderer->Swapchain.Extent.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = (VkSurfaceTransformFlagBitsKHR)PreTransform,
        .compositeAlpha = CompositeAlpha,
        .presentMode = SwapchainPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = OldSwapchain,
    };

    if(SurfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        SwapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if(SurfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        SwapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    vkCreateSwapchainKHR(Renderer->Device, &SwapchainCI, NULL, &Renderer->Swapchain.Handle);

    if(OldSwapchain != VK_NULL_HANDLE)
    {
        Rr_CleanupSwapchain(App, OldSwapchain);
    }

    uint32_t ImageCount = 0;
    vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, NULL);
    RR_RESERVE_SLICE(&Renderer->Swapchain.Images, ImageCount, App->PermanentArena);
    Renderer->Swapchain.Images.Count = ImageCount;
    vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, Renderer->Swapchain.Images.Data);

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

    RR_RESERVE_SLICE(&Renderer->Swapchain.ImageViews, ImageCount, App->PermanentArena);
    Renderer->Swapchain.ImageViews.Count = ImageCount;

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
        ImageViewCreateInfo.image = Renderer->Swapchain.Images.Data[Index];
        vkCreateImageView(Renderer->Device, &ImageViewCreateInfo, NULL, &Renderer->Swapchain.ImageViews.Data[Index]);

        FramebufferCreateInfo.pAttachments = &Renderer->Swapchain.ImageViews.Data[Index];
        vkCreateFramebuffer(
            Renderer->Device,
            &FramebufferCreateInfo,
            NULL,
            &Renderer->Swapchain.Framebuffers.Data[Index]);
    }

    Rr_DestroyScratch(Scratch);

    return true;
}

static void Rr_InitFrames(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;
    Rr_Frame *Frames = Renderer->Frames;

    VkFenceCreateInfo FenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VkSemaphoreCreateInfo SemaphoreCreateInfo = Rr_GetSemaphoreCreateInfo(0);

    for(size_t Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame *Frame = &Frames[Index];

        /* Commands */

        VkCommandPoolCreateInfo CommandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        };
        vkCreateCommandPool(Renderer->Device, &CommandPoolInfo, NULL, &Frame->CommandPool);
        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .commandPool = Frame->CommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 2,
        };
        vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, Frame->MainCommandBuffers);
        vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, Frame->PresentCommandBuffers);

        /* Synchronization */

        vkCreateFence(Device, &FenceCreateInfo, NULL, &Frame->RenderFence);
        vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->SwapchainSemaphore);
        vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->MainSemaphore);
        vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->PresentSemaphore);

        /* Descriptor Allocator */

        Rr_DescriptorPoolSizeRatio Ratios[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 16 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, 16 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16 },
        };
        Frame->DescriptorAllocator =
            Rr_CreateDescriptorAllocator(Renderer->Device, 1000, Ratios, RR_ARRAY_COUNT(Ratios), App->PermanentArena);

        /* Buffers */

        Frame->StagingBuffer.Buffer = Rr_CreateBuffer_Internal(
            App,
            RR_MEGABYTES(16),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO,
            true,
            false);

        Frame->Arena = Rr_CreateDefaultArena();
    }
}

static void Rr_CleanupFrames(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    for(size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame *Frame = &Renderer->Frames[Index];
        vkDestroyCommandPool(Renderer->Device, Frame->CommandPool, NULL);

        vkDestroyFence(Device, Frame->RenderFence, NULL);
        vkDestroySemaphore(Device, Frame->MainSemaphore, NULL);
        vkDestroySemaphore(Device, Frame->PresentSemaphore, NULL);
        vkDestroySemaphore(Device, Frame->SwapchainSemaphore, NULL);

        Rr_DestroyDescriptorAllocator(&Frame->DescriptorAllocator, Device);

        Rr_DestroyBuffer(App, Frame->StagingBuffer.Buffer);

        Rr_DestroyArena(Frame->Arena);
    }
}

static void Rr_InitVMA(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VmaVulkanFunctions VulkanFunctions = {
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = vkAllocateMemory,
        .vkFreeMemory = vkFreeMemory,
        .vkMapMemory = vkMapMemory,
        .vkUnmapMemory = vkUnmapMemory,
        .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = vkBindBufferMemory,
        .vkBindImageMemory = vkBindImageMemory,
        .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        .vkCreateBuffer = vkCreateBuffer,
        .vkDestroyBuffer = vkDestroyBuffer,
        .vkCreateImage = vkCreateImage,
        .vkDestroyImage = vkDestroyImage,
        .vkCmdCopyBuffer = vkCmdCopyBuffer,
        .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
        .vkBindBufferMemory2KHR = vkBindBufferMemory2,
        .vkBindImageMemory2KHR = vkBindImageMemory2,
        .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
    };
    VmaAllocatorCreateInfo AllocatorInfo = {
        .flags = 0,
        .physicalDevice = Renderer->PhysicalDevice.Handle,
        .device = Renderer->Device,
        .pVulkanFunctions = &VulkanFunctions,
        .instance = Renderer->Instance,
    };
    vmaCreateAllocator(&AllocatorInfo, &Renderer->Allocator);
}

static void Rr_InitDescriptors(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_DescriptorPoolSizeRatio Ratios[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };

    Renderer->GlobalDescriptorAllocator =
        Rr_CreateDescriptorAllocator(Renderer->Device, 10, Ratios, SDL_arraysize(Ratios), App->PermanentArena);
}

static PFN_vkVoidFunction Rr_LoadVulkanFunction(const char *FuncName, void *UserData)
{
    return (PFN_vkVoidFunction)vkGetInstanceProcAddr(volkGetLoadedInstance(), FuncName);
}

static void Rr_InitImmediateMode(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VkDevice Device = Renderer->Device;
    Rr_ImmediateMode *ImmediateMode = &Renderer->ImmediateMode;

    VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
    };
    vkCreateCommandPool(Device, &CommandPoolInfo, NULL, &ImmediateMode->CommandPool);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(ImmediateMode->CommandPool, 1);
    vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &ImmediateMode->CommandBuffer);
    VkFenceCreateInfo FenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    vkCreateFence(Device, &FenceCreateInfo, NULL, &ImmediateMode->Fence);
}

static void Rr_CleanupImmediateMode(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    vkDestroyCommandPool(Renderer->Device, Renderer->ImmediateMode.CommandPool, NULL);
    vkDestroyFence(Renderer->Device, Renderer->ImmediateMode.Fence, NULL);
}

static void Rr_InitPresent(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Renderer->PresentLayout = Rr_CreatePipelineLayout(App, NULL, 0);

    Rr_ColorTargetInfo ColorTargets[1] = { 0 };
    ColorTargets[0].Blend.ColorWriteMask = RR_COLOR_COMPONENT_ALL;
    ColorTargets[0].Format = Rr_GetSwapchainFormat(App);

    Rr_PipelineInfo PipelineInfo = { 0 };
    PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_NONE;
    PipelineInfo.Rasterizer.FrontFace = RR_FRONT_FACE_CLOCKWISE;
    PipelineInfo.Layout = Renderer->PresentLayout;
    PipelineInfo.VertexShaderSPV = Rr_LoadAsset(RR_BUILTIN_PRESENT_VERT_SPV);
    PipelineInfo.FragmentShaderSPV = Rr_LoadAsset(RR_BUILTIN_PRESENT_FRAG_SPV);
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    Renderer->PresentPipeline = Rr_CreateGraphicsPipeline(App, &PipelineInfo);

    Rr_Attachment Attachment = {
        .LoadOp = RR_LOAD_OP_DONT_CARE,
        .StoreOp = RR_STORE_OP_STORE,
    };
    Rr_RenderPassInfo RenderPassInfo = { .AttachmentCount = 1, .Attachments = &Attachment };
    Renderer->PresentRenderPass = Rr_GetRenderPass(App, &RenderPassInfo);
}

static void Rr_CleanupPresent(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_DestroyGraphicsPipeline(App, Renderer->PresentPipeline);
    Rr_DestroyPipelineLayout(App, Renderer->PresentLayout);
}

/* @TODO: Move to queue initialization? */
static void Rr_InitTransientCommandPools(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    vkCreateCommandPool(
        Renderer->Device,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        },
        NULL,
        &Renderer->GraphicsQueue.TransientCommandPool);

    vkCreateCommandPool(
        Renderer->Device,
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
    vkDestroyCommandPool(Renderer->Device, Renderer->GraphicsQueue.TransientCommandPool, NULL);
    vkDestroyCommandPool(Renderer->Device, Renderer->TransferQueue.TransientCommandPool, NULL);
}

// static void Rr_InitNullTextures(Rr_App *App)
// {
//     Rr_Renderer *Renderer = &App->Renderer;
//
//     VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);
//     Rr_WriteBuffer StagingBuffer = { .Buffer = Rr_CreateMappedBuffer(App, 256, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
//                                      .Offset = 0 };
//     Rr_UploadContext UploadContext = {
//         .StagingBuffer = &StagingBuffer,
//         .TransferCommandBuffer = CommandBuffer,
//     };
//     uint32_t WhiteData = 0xffffffff;
//     Renderer->NullTextures.White = Rr_CreateColorImageFromMemory(App, &UploadContext, (char *)&WhiteData, 1, 1,
//     false); uint32_t NormalData = 0xffff8888; Renderer->NullTextures.Normal =
//         Rr_CreateColorImageFromMemory(App, &UploadContext, (char *)&NormalData, 1, 1, false);
//     Rr_EndImmediate(Renderer);
//
//     Rr_DestroyBuffer(App, StagingBuffer.Buffer);
// }

void Rr_InitRenderer(Rr_App *App)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Renderer *Renderer = &App->Renderer;
    SDL_Window *Window = App->Window;
    // Rr_AppConfig *Config = App->Config;

    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = SDL_GetWindowTitle(Window),
        .pEngineName = "Rr_Renderer",
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0),
    };

    /* Gather required extensions. */

    const char *AppExtensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    uint32_t AppExtensionCount = RR_ARRAY_COUNT(AppExtensions);
    AppExtensionCount = 0; /* Use Vulkan Configurator! */

    uint32_t SDLExtensionCount;
    const char *const *SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&SDLExtensionCount);

    uint32_t ExtensionCount = SDLExtensionCount + AppExtensionCount;
    const char **Extensions = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, const char *, ExtensionCount);
    for(uint32_t Index = 0; Index < ExtensionCount; Index++)
    {
        Extensions[Index] = SDLExtensions[Index];
    }
    for(uint32_t Index = 0; Index < AppExtensionCount; Index++)
    {
        Extensions[Index + SDLExtensionCount] = AppExtensions[Index];
    }

    /* Create Vulkan instance. */

    VkInstanceCreateInfo InstanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &VKAppInfo,
        .enabledExtensionCount = ExtensionCount,
        .ppEnabledExtensionNames = Extensions,
    };

    vkCreateInstance(&InstanceCreateInfo, NULL, &Renderer->Instance);

    volkLoadInstance(Renderer->Instance);

    if(SDL_Vulkan_CreateSurface(Window, Renderer->Instance, NULL, &Renderer->Surface) != true)
    {
        Rr_LogAbort("Failed to create Vulkan surface: %s", SDL_GetError());
    }

    Renderer->PhysicalDevice = Rr_SelectPhysicalDevice(
        Renderer->Instance,
        Renderer->Surface,
        &Renderer->GraphicsQueue.FamilyIndex,
        &Renderer->TransferQueue.FamilyIndex,
        Scratch.Arena);
    Rr_InitDeviceAndQueues(
        Renderer->PhysicalDevice.Handle,
        Renderer->GraphicsQueue.FamilyIndex,
        Renderer->TransferQueue.FamilyIndex,
        &Renderer->Device,
        &Renderer->GraphicsQueue.Handle,
        &Renderer->TransferQueue.Handle);

    volkLoadDevice(Renderer->Device);

    Rr_InitVMA(App);
    Rr_InitTransientCommandPools(App);
    Rr_InitDescriptors(App);
    uint32_t Width, Height;
    SDL_GetWindowSizeInPixels(Window, (int32_t *)&Width, (int32_t *)&Height);
    Rr_InitSwapchain(App, &Width, &Height);
    Rr_InitPresent(App);
    Rr_InitFrames(App);
    Rr_InitImmediateMode(App);
    // Rr_InitNullTextures(App);
    // Rr_InitTextRenderer(App);

    Rr_DestroyScratch(Scratch);
}

bool Rr_NewFrame(Rr_App *App, void *Window)
{
    Rr_Renderer *Renderer = &App->Renderer;

    int32_t bResizePending = SDL_GetAtomicInt(&Renderer->Swapchain.ResizePending);
    if(bResizePending == true)
    {
        vkDeviceWaitIdle(Renderer->Device);

        int32_t Width, Height;
        SDL_GetWindowSizeInPixels(Window, &Width, &Height);

        bool Minimized = (SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED);

        if(!Minimized && Width > 0 && Height > 0 && Rr_InitSwapchain(App, (uint32_t *)&Width, (uint32_t *)&Height))
        {
            SDL_SetAtomicInt(&Renderer->Swapchain.ResizePending, 0);
            return true;
        }

        return false;
    }
    return true;
}

void Rr_CleanupRenderer(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    vkDeviceWaitIdle(Renderer->Device);

    App->Config->CleanupFunc(App, App->UserData);

    // Rr_CleanupTextRenderer(App);

    for(size_t Index = 0; Index < Renderer->RenderPasses.Count; ++Index)
    {
        vkDestroyRenderPass(Renderer->Device, Renderer->RenderPasses.Data[Index].Handle, NULL);
    }

    for(size_t Index = 0; Index < Renderer->Framebuffers.Count; ++Index)
    {
        vkDestroyFramebuffer(Renderer->Device, Renderer->Framebuffers.Data[Index].Handle, NULL);
    }

    Rr_DestroyDescriptorAllocator(&Renderer->GlobalDescriptorAllocator, Device);

    Rr_CleanupFrames(App);

    // Rr_DestroyImage(App, Renderer->NullTextures.White);
    // Rr_DestroyImage(App, Renderer->NullTextures.Normal);

    Rr_CleanupTransientCommandPools(Renderer);
    Rr_CleanupImmediateMode(App);

    Rr_CleanupPresent(App);
    Rr_CleanupSwapchain(App, Renderer->Swapchain.Handle);

    vmaDestroyAllocator(Renderer->Allocator);

    vkDestroySurfaceKHR(Renderer->Instance, Renderer->Surface, NULL);
    vkDestroyDevice(Renderer->Device, NULL);

    vkDestroyInstance(Renderer->Instance, NULL);
}

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer *Renderer)
{
    Rr_ImmediateMode *ImmediateMode = &Renderer->ImmediateMode;
    vkResetFences(Renderer->Device, 1, &ImmediateMode->Fence);
    vkResetCommandBuffer(ImmediateMode->CommandBuffer, 0);

    VkCommandBufferBeginInfo BeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };
    vkBeginCommandBuffer(ImmediateMode->CommandBuffer, &BeginInfo);

    return ImmediateMode->CommandBuffer;
}

void Rr_EndImmediate(Rr_Renderer *Renderer)
{
    Rr_ImmediateMode *ImmediateMode = &Renderer->ImmediateMode;

    vkEndCommandBuffer(ImmediateMode->CommandBuffer);

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

    vkQueueSubmit(Renderer->GraphicsQueue.Handle, 1, &SubmitInfo, ImmediateMode->Fence);
    vkWaitForFences(Renderer->Device, 1, &ImmediateMode->Fence, true, UINT64_MAX);
}

static void Rr_ProcessPendingLoads(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    if(Rr_TryLockSpinLock(&App->SyncArena.Lock))
    {
        for(size_t Index = 0; Index < Renderer->PendingLoadsSlice.Count; ++Index)
        {
            Rr_PendingLoad *PendingLoad = &Renderer->PendingLoadsSlice.Data[Index];
            PendingLoad->LoadingCallback(App, PendingLoad->Userdata);
        }
        RR_EMPTY_SLICE(&Renderer->PendingLoadsSlice);

        Rr_UnlockSpinLock(&App->SyncArena.Lock);
    }
}

void Rr_PrepareFrame(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_ResetArena(Frame->Arena);

    RR_ZERO(Frame->Graph);

    Frame->StagingBuffer.Offset = 0;

    /* We cycle between two command buffers to allow having
     * combined iterate + draw stage. */
    Frame->CurrentCommandBufferIndex = (Frame->CurrentCommandBufferIndex + 1) % 2;
    Frame->MainCommandBuffer = Frame->MainCommandBuffers[Frame->CurrentCommandBufferIndex];
    Frame->PresentCommandBuffer = Frame->PresentCommandBuffers[Frame->CurrentCommandBufferIndex];
    VkCommandBufferBeginInfo CommandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };
    vkBeginCommandBuffer(Frame->MainCommandBuffer, &CommandBufferBeginInfo);
    vkBeginCommandBuffer(Frame->PresentCommandBuffer, &CommandBufferBeginInfo);

    Rr_ProcessPendingLoads(App);
}

void Rr_Draw(Rr_App *App)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Renderer *Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;
    Rr_Swapchain *Swapchain = &Renderer->Swapchain;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    vkWaitForFences(Device, 1, &Frame->RenderFence, true, 1000000000);
    vkResetFences(Device, 1, &Frame->RenderFence);

    Rr_ResetDescriptorAllocator(&Frame->DescriptorAllocator, Renderer->Device);

    /* Acquire swapchain image. */

    uint32_t SwapchainImageIndex;
    VkResult Result = vkAcquireNextImageKHR(
        Device,
        Swapchain->Handle,
        1000000000,
        Frame->SwapchainSemaphore,
        VK_NULL_HANDLE,
        &SwapchainImageIndex);
    if(Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_SetAtomicInt(&Renderer->Swapchain.ResizePending, 1);
        return;
    }
    if(Result == VK_SUBOPTIMAL_KHR)
    {
        SDL_SetAtomicInt(&Renderer->Swapchain.ResizePending, 1);
    }
    assert(Result >= 0);

    VkImage CurrentSwapchainImage = Renderer->Swapchain.Images.Data[SwapchainImageIndex];

    /* Attempt to properly synchronize first time use of a swapchain image. */

    if(Rr_HasImageState(&Renderer->GlobalSync, CurrentSwapchainImage) != true)
    {
        Rr_SetImageState(
            &Renderer->GlobalSync,
            CurrentSwapchainImage,
            (Rr_ImageSync){
                .AccessMask = 0,
                .StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
            },
            App->PermanentArena);
    }

    /* Execute Frame Graph */

    Rr_ExecuteGraph(App, &Frame->Graph, Scratch.Arena);

    vkEndCommandBuffer(Frame->MainCommandBuffer);
    vkEndCommandBuffer(Frame->PresentCommandBuffer);

    /* Submit frame command buffer and queue present. */

    VkSubmitInfo SubmitInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &Frame->MainCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &Frame->MainSemaphore,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &Frame->PresentCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &Frame->PresentSemaphore,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &Frame->MainSemaphore,
            .pWaitDstStageMask = (VkPipelineStageFlags[]){ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT },
        },
    };

    Rr_LockSpinLock(&Renderer->GraphicsQueue.Lock);

    vkQueueSubmit(Renderer->GraphicsQueue.Handle, 2, SubmitInfos, Frame->RenderFence);

    Rr_UnlockSpinLock(&Renderer->GraphicsQueue.Lock);

    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->PresentSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    Result = vkQueuePresentKHR(Renderer->GraphicsQueue.Handle, &PresentInfo);
    if(Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_SetAtomicInt(&Renderer->Swapchain.ResizePending, 1);
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

size_t Rr_GetUniformAlignment(Rr_App *App)
{
    return App->Renderer.PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
}

size_t Rr_GetStorageAlignment(Rr_App *App)
{
    return App->Renderer.PhysicalDevice.Properties.properties.limits.minStorageBufferOffsetAlignment;
}

Rr_Arena *Rr_GetFrameArena(Rr_App *App)
{
    return Rr_GetCurrentFrame(&App->Renderer)->Arena;
}

Rr_TextureFormat Rr_GetSwapchainFormat(Rr_App *App)
{
    return Rr_GetTextureFormat(App->Renderer.Swapchain.Format);
}

static VkAttachmentLoadOp Rr_GetLoadOp(Rr_LoadOp LoadOp)
{
    switch(LoadOp)
    {
        case RR_LOAD_OP_CLEAR:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case RR_LOAD_OP_LOAD:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        default:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

static VkAttachmentStoreOp Rr_GetStoreOp(Rr_StoreOp StoreOp)
{
    switch(StoreOp)
    {
        case RR_STORE_OP_DONT_CARE:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case RR_STORE_OP_STORE:
            return VK_ATTACHMENT_STORE_OP_STORE;
        default:
            return VK_ATTACHMENT_STORE_OP_STORE;
    }
}

VkRenderPass Rr_GetRenderPass(Rr_App *App, Rr_RenderPassInfo *Info)
{
    assert(Info != NULL);

    Rr_Renderer *Renderer = &App->Renderer;

    uint32_t Hash = XXH32(Info->Attachments, sizeof(Rr_Attachment) * Info->AttachmentCount, 0);

    for(size_t Index = 0; Index < Renderer->RenderPasses.Count; ++Index)
    {
        if(Renderer->RenderPasses.Data[Index].Hash == Hash)
        {
            return Renderer->RenderPasses.Data[Index].Handle;
        }
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkAttachmentDescription *Attachments =
        RR_ALLOC_COUNT(Scratch.Arena, sizeof(VkAttachmentDescription), Info->AttachmentCount);

    size_t ColorCount = 0;
    VkAttachmentReference *ColorReferences =
        RR_ALLOC_COUNT(Scratch.Arena, sizeof(VkAttachmentDescription), Info->AttachmentCount);
    VkAttachmentReference *DepthReference = NULL;

    for(size_t Index = 0; Index < Info->AttachmentCount; ++Index)
    {
        Rr_Attachment *Attachment = &Info->Attachments[Index];
        if(Attachment->Depth)
        {
            if(DepthReference != NULL)
            {
                Rr_LogAbort("Can't have more than one depth attachment!");
            }
            DepthReference = RR_ALLOC(Scratch.Arena, sizeof(VkAttachmentDescription));
            DepthReference->attachment = Index;
            DepthReference->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            Attachments[Index] = (VkAttachmentDescription){
                .samples = 1,
                .format = VK_FORMAT_D32_SFLOAT,
                .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
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
                .format = Rr_GetVulkanTextureFormat(Rr_GetSwapchainFormat(App)),
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

    vkCreateRenderPass(Renderer->Device, &RenderPassCreateInfo, NULL, &RenderPass);

    *RR_PUSH_SLICE(&Renderer->RenderPasses, App->PermanentArena) = (Rr_CachedRenderPass){
        .Handle = RenderPass,
        .Hash = Hash,
    };

    Rr_DestroyScratch(Scratch);

    return RenderPass;
}

static VkFramebuffer Rr_GetFramebufferInternal(
    Rr_App *App,
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Renderer *Renderer = &App->Renderer;

    size_t QuerySize = sizeof(VkImageView) * ImageViewCount + sizeof(VkExtent3D) + sizeof(RenderPass); /* NOLINT */
    void *Query = RR_ALLOC(Scratch.Arena, QuerySize);
    memcpy(Query, ImageViews, sizeof(VkImageView) * ImageViewCount);
    memcpy(((char *)Query) + sizeof(VkImageView) * ImageViewCount, &Extent, sizeof(VkExtent3D));
    memcpy(
        ((char *)Query) + sizeof(VkImageView) * ImageViewCount + sizeof(Extent),
        &RenderPass,
        sizeof(RenderPass)); /* NOLINT */

    uint32_t Hash = XXH32(Query, QuerySize, 0);

    VkFramebuffer Framebuffer = VK_NULL_HANDLE;

    for(size_t Index = 0; Index < Renderer->Framebuffers.Count; ++Index)
    {
        Rr_CachedFramebuffer *CachedFramebuffer = Renderer->Framebuffers.Data + Index;

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

    vkCreateFramebuffer(Renderer->Device, &CreateInfo, NULL, &Framebuffer);

    *RR_PUSH_SLICE(&Renderer->Framebuffers, App->PermanentArena) = (Rr_CachedFramebuffer){
        .Handle = Framebuffer,
        .Hash = Hash,
    };

    Rr_DestroyScratch(Scratch);

    return Framebuffer;
}

VkFramebuffer Rr_GetFramebufferViews(
    Rr_App *App,
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent)
{
    return Rr_GetFramebufferInternal(App, RenderPass, ImageViews, ImageViewCount, Extent, NULL);
}

VkFramebuffer Rr_GetFramebuffer(
    Rr_App *App,
    VkRenderPass RenderPass,
    Rr_Image *Images,
    size_t ImageCount,
    VkExtent3D Extent)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkImageView *ImageViews = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, VkImageView, ImageCount);

    for(size_t Index = 0; Index < ImageCount; ++Index)
    {
        ImageViews[Index] = Rr_GetCurrentAllocatedImage(App, Images + Index)->View;
    }

    VkFramebuffer Framebuffer =
        Rr_GetFramebufferInternal(App, RenderPass, ImageViews, ImageCount, Extent, Scratch.Arena);

    Rr_DestroyScratch(Scratch);

    return Framebuffer;
}

Rr_TextureFormat Rr_GetTextureFormat(VkFormat TextureFormat)
{
    switch(TextureFormat)
    {
        case VK_FORMAT_R8G8B8A8_UNORM:
            return RR_TEXTURE_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return RR_TEXTURE_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_D32_SFLOAT:
            return RR_TEXTURE_FORMAT_D32_SFLOAT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return RR_TEXTURE_FORMAT_D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return RR_TEXTURE_FORMAT_D32_SFLOAT_S8_UINT;
        default:
            return RR_TEXTURE_FORMAT_UNDEFINED;
    }
}

VkFormat Rr_GetVulkanTextureFormat(Rr_TextureFormat TextureFormat)
{
    switch(TextureFormat)
    {
        case RR_TEXTURE_FORMAT_R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case RR_TEXTURE_FORMAT_B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case RR_TEXTURE_FORMAT_D32_SFLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case RR_TEXTURE_FORMAT_D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case RR_TEXTURE_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

bool Rr_HasImageState(Rr_Map **Map, VkImage Image)
{
    Rr_ImageSync **State = RR_UPSERT(Map, Image, NULL);
    return State && *State;
}

void Rr_SetImageState(Rr_Map **Map, VkImage Image, Rr_ImageSync NewState, Rr_Arena *Arena)
{
    Rr_ImageSync **State = RR_UPSERT(Map, Image, Arena);
    if(*State == NULL)
    {
        *State = RR_ALLOC(Arena, sizeof(Rr_ImageSync));
    }
    *(*State) = NewState;
}
