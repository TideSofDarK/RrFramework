#include "Rr_Renderer.h"

#include "Rr/Rr_Graph.h"
#include "Rr/Rr_Material.h"
#include "Rr_App.h"
#include "Rr_Buffer.h"
#include "Rr_BuiltinAssets.inc"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include <SDL3/SDL_video.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_vulkan.h>

// static void Rr_CalculateDrawTargetResolution(Rr_Renderer*  Renderer,  u32
// WindowWidth,  u32 WindowHeight)
// {
//     Renderer->ActiveResolution.width = Renderer->ReferenceResolution.width;
//     Renderer->ActiveResolution.height = Renderer->ReferenceResolution.height;
//
//      i32 MaxAvailableScale = SDL_min(WindowWidth /
//      Renderer->ReferenceResolution.width, WindowHeight /
//      Renderer->ReferenceResolution.height);
//     if (MaxAvailableScale >= 1)
//     {
//         Renderer->ActiveResolution.width += (WindowWidth - MaxAvailableScale
//         * Renderer->ReferenceResolution.width) / MaxAvailableScale;
//         Renderer->ActiveResolution.height += (WindowHeight -
//         MaxAvailableScale * Renderer->ReferenceResolution.height) /
//         MaxAvailableScale;
//     }
//
//     Renderer->Scale = MaxAvailableScale;
// }

static void Rr_CleanupSwapchain(Rr_App *App, VkSwapchainKHR Swapchain)
{
    Rr_Renderer *Renderer = &App->Renderer;

    for(uint32_t Index = 0; Index < Renderer->Swapchain.ImageCount; Index++)
    {
        vkDestroyImageView(Renderer->Device, Renderer->Swapchain.Images[Index].View, NULL);
    }
    vkDestroySwapchainKHR(Renderer->Device, Swapchain, NULL);
}

static Rr_Bool Rr_InitSwapchain(Rr_App *App, uint32_t *Width, uint32_t *Height)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VkSwapchainKHR OldSwapchain = Renderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &SurfCaps);

    if(SurfCaps.currentExtent.width == 0 || SurfCaps.currentExtent.height == 0)
    {
        return RR_FALSE;
    }
    if(SurfCaps.currentExtent.width == UINT32_MAX)
    {
        Renderer->Swapchain.Extent.width = *Width;
        Renderer->Swapchain.Extent.height = *Height;
    }
    else
    {
        Renderer->Swapchain.Extent = SurfCaps.currentExtent;
        *Width = SurfCaps.currentExtent.width;
        *Height = SurfCaps.currentExtent.height;
    }

    uint32_t PresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &PresentModeCount,
        NULL);
    SDL_assert(PresentModeCount > 0);

    VkPresentModeKHR *PresentModes = Rr_StackAlloc(VkPresentModeKHR, PresentModeCount);
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
    Rr_StackFree(PresentModes);

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
    SDL_assert(FormatCount > 0);

    VkSurfaceFormatKHR *SurfaceFormats = Rr_StackAlloc(VkSurfaceFormatKHR, FormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &FormatCount,
        SurfaceFormats);

    Rr_Bool PreferredFormatFound = RR_FALSE;
    for(uint32_t Index = 0; Index < FormatCount; Index++)
    {
        VkSurfaceFormatKHR *SurfaceFormat = &SurfaceFormats[Index];

        if(SurfaceFormat->format == VK_FORMAT_B8G8R8A8_UNORM || SurfaceFormat->format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            Renderer->Swapchain.Format = SurfaceFormat->format;
            Renderer->Swapchain.ColorSpace = SurfaceFormat->colorSpace;
            PreferredFormatFound = RR_TRUE;
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
    for(uint32_t Index = 0; Index < SDL_arraysize(CompositeAlphaFlags); Index++)
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
    SDL_assert(ImageCount <= RR_MAX_SWAPCHAIN_IMAGE_COUNT);

    Renderer->Swapchain.ImageCount = ImageCount;
    VkImage *Images = Rr_StackAlloc(VkImage, ImageCount);
    vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, Images);

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

    for(uint32_t i = 0; i < ImageCount; i++)
    {
        Renderer->Swapchain.Images[i].Handle = Images[i];
        ImageViewCreateInfo.image = Images[i];
        vkCreateImageView(Renderer->Device, &ImageViewCreateInfo, NULL, &Renderer->Swapchain.Images[i].View);
    }

    Rr_Bool DrawTargetDirty = RR_TRUE;
    if(Renderer->DrawTarget != NULL)
    {
        VkExtent3D Extent = Renderer->DrawTarget->Frames[0].ColorImage->Extent;
        if(*Width <= Extent.width || *Height <= Extent.height)
        {
            DrawTargetDirty = RR_FALSE;
        }
        else
        {
            Rr_DestroyDrawTarget(App, Renderer->DrawTarget);
        }
    }
    if(DrawTargetDirty)
    {
        SDL_DisplayID DisplayID = SDL_GetPrimaryDisplay();
        SDL_Rect DisplayBounds;
        SDL_GetDisplayBounds(DisplayID, &DisplayBounds);

        uint32_t DrawTargetWidth = SDL_max(DisplayBounds.w, *Width);
        uint32_t DrawTargetHeight = SDL_max(DisplayBounds.h, *Height);
        Renderer->DrawTarget = Rr_CreateDrawTarget(App, DrawTargetWidth, DrawTargetHeight);

        Rr_LogVulkan("Creating primary draw target with size %dx%d", DrawTargetWidth, DrawTargetHeight);
    }

    Renderer->SwapchainSize = (VkExtent2D){ .width = *Width, .height = *Height };

    Rr_StackFree(Images);
    Rr_StackFree(SurfaceFormats);

    return RR_TRUE;
}

static void Rr_InitFrames(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;
    Rr_Frame *Frames = Renderer->Frames;

    VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo SemaphoreCreateInfo = GetSemaphoreCreateInfo(0);

    for(size_t Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame *Frame = &Frames[Index];
        RR_ZERO_PTR(Frame);

        /* Commands */

        VkCommandPoolCreateInfo CommandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        };
        vkCreateCommandPool(Renderer->Device, &CommandPoolInfo, NULL, &Frame->CommandPool);
        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Frame->CommandPool, 1);
        vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &Frame->MainCommandBuffer);

        /* Synchronization */

        vkCreateFence(Device, &FenceCreateInfo, NULL, &Frame->RenderFence);
        vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->SwapchainSemaphore);
        vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->RenderSemaphore);

        /* Descriptor Allocator */

        Rr_DescriptorPoolSizeRatio Ratios[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },          { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },         { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
        };
        Frame->DescriptorAllocator =
            Rr_CreateDescriptorAllocator(Renderer->Device, 1000, Ratios, SDL_arraysize(Ratios), &App->PermanentArena);

        /* Buffers */

        Frame->StagingBuffer.Buffer =
            Rr_CreateMappedBuffer(App, RR_STAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        Frame->CommonBuffer.Buffer = Rr_CreateDeviceUniformBuffer(App, 66560);
        Frame->PerDrawBuffer.Buffer =
            Rr_CreateMappedBuffer(App, 66560, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        Frame->Arena = Rr_CreateArena(RR_PER_FRAME_ARENA_SIZE);
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
        vkDestroySemaphore(Device, Frame->RenderSemaphore, NULL);
        vkDestroySemaphore(Device, Frame->SwapchainSemaphore, NULL);

        Rr_DestroyDescriptorAllocator(&Frame->DescriptorAllocator, Device);

        Rr_DestroyBuffer(App, Frame->StagingBuffer.Buffer);
        Rr_DestroyBuffer(App, Frame->PerDrawBuffer.Buffer);
        Rr_DestroyBuffer(App, Frame->CommonBuffer.Buffer);

        Rr_DestroyArena(&Frame->Arena);
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
        Rr_CreateDescriptorAllocator(Renderer->Device, 10, Ratios, SDL_arraysize(Ratios), &App->PermanentArena);
}

static PFN_vkVoidFunction Rr_LoadVulkanFunction(const char *FuncName, void *UserData)
{
    return (PFN_vkVoidFunction)vkGetInstanceProcAddr(volkGetLoadedInstance(), FuncName);
}

void Rr_InitImGui(Rr_App *App)
{
    SDL_Window *Window = App->Window;
    Rr_Renderer *Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    VkDescriptorPoolSize PoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
    };

    VkDescriptorPoolCreateInfo PoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = (uint32_t)SDL_arraysize(PoolSizes),
        .pPoolSizes = PoolSizes,
    };

    vkCreateDescriptorPool(Device, &PoolCreateInfo, NULL, &Renderer->ImGui.DescriptorPool);

    igCreateContext(NULL);
    ImGuiIO *IO = igGetIO();
    IO->IniFilename = NULL;

    ImGui_ImplVulkan_LoadFunctions(Rr_LoadVulkanFunction, NULL);
    ImGui_ImplSDL3_InitForVulkan(Window);

    ImGui_ImplVulkan_InitInfo InitInfo = { .Instance = Renderer->Instance,
                                           .PhysicalDevice = Renderer->PhysicalDevice.Handle,
                                           .Device = Device,
                                           .QueueFamily = Renderer->GraphicsQueue.FamilyIndex,
                                           .Queue = Renderer->GraphicsQueue.Handle,
                                           .DescriptorPool = Renderer->ImGui.DescriptorPool,
                                           .MinImageCount = 3,
                                           .ImageCount = 3,
                                           .UseDynamicRendering = RR_FALSE,
                                           .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                                           .RenderPass = Renderer->RenderPasses.ColorDepthLoad };

    ImGui_ImplVulkan_Init(&InitInfo);

    float WindowScale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(Window));
    ImGuiStyle_ScaleAllSizes(igGetStyle(), WindowScale);

    /* Init default font. */
    Rr_Asset MartianMonoTTF = Rr_LoadAsset(RR_BUILTIN_MARTIANMONO_TTF);
    ImFontConfig *FontConfig = ImFontConfig_ImFontConfig();
    FontConfig->FontDataOwnedByAtlas = RR_FALSE; /* Don't transfer asset ownership to
                                                    ImGui, it will crash otherwise! */
    ImFontAtlas_AddFontFromMemoryTTF(
        IO->Fonts,
        (void *)MartianMonoTTF.Data,
        (int32_t)MartianMonoTTF.Length,
        SDL_floorf(14.0f * WindowScale),
        FontConfig,
        NULL);
    igMemFree(FontConfig);

    ImGui_ImplVulkan_CreateFontsTexture();

    Renderer->ImGui.IsInitialized = RR_TRUE;
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
    VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    vkCreateFence(Device, &FenceCreateInfo, NULL, &ImmediateMode->Fence);
}

static void Rr_CleanupImmediateMode(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    vkDestroyCommandPool(Renderer->Device, Renderer->ImmediateMode.CommandPool, NULL);
    vkDestroyFence(Renderer->Device, Renderer->ImmediateMode.Fence, NULL);
}

static void Rr_InitGenericPipelineLayout(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    /* Descriptor Set Layouts */
    Rr_DescriptorLayoutBuilder DescriptorLayoutBuilder = { 0 };
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_AddDescriptorArray(
        &DescriptorLayoutBuilder,
        1,
        RR_MAX_TEXTURES_PER_MATERIAL,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS] = Rr_BuildDescriptorLayout(
        &DescriptorLayoutBuilder,
        Device,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    Rr_ClearDescriptors(&DescriptorLayoutBuilder);
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_AddDescriptorArray(
        &DescriptorLayoutBuilder,
        1,
        RR_MAX_TEXTURES_PER_MATERIAL,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL] = Rr_BuildDescriptorLayout(
        &DescriptorLayoutBuilder,
        Device,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    Rr_ClearDescriptors(&DescriptorLayoutBuilder);
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
    Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW] = Rr_BuildDescriptorLayout(
        &DescriptorLayoutBuilder,
        Device,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    /* Pipeline Layout */
    VkPushConstantRange PushConstantRange = { .offset = 0,
                                              .size = 128,
                                              .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };

    VkPipelineLayoutCreateInfo LayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT,
        .pSetLayouts = Renderer->GenericDescriptorSetLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };
    vkCreatePipelineLayout(Device, &LayoutInfo, NULL, &Renderer->GenericPipelineLayout);
}

static void Rr_InitSamplers(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VkSamplerCreateInfo SamplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    SamplerInfo.magFilter = VK_FILTER_NEAREST;
    SamplerInfo.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(Renderer->Device, &SamplerInfo, NULL, &Renderer->NearestSampler);

    SamplerInfo.magFilter = VK_FILTER_LINEAR;
    SamplerInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(Renderer->Device, &SamplerInfo, NULL, &Renderer->LinearSampler);
}

static void Rr_CleanupSamplers(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    vkDestroySampler(Renderer->Device, Renderer->NearestSampler, NULL);
    vkDestroySampler(Renderer->Device, Renderer->LinearSampler, NULL);
}

static VkRenderPass Rr_CreateRenderPassColorDepth(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VkAttachmentDescription Attachments[2] = {
        {
            .samples = 1,
            .format = RR_COLOR_FORMAT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .flags = 0,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        },
        {
            .samples = 1,
            .format = RR_DEPTH_FORMAT,
            .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .flags = 0,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        },
    };

    VkAttachmentReference ColorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference DepthAttachmentRef = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription SubpassDescription = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ColorAttachmentRef,
        .pDepthStencilAttachment = &DepthAttachmentRef,
        .pResolveAttachments = NULL,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    VkRenderPassCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .attachmentCount = SDL_arraysize(Attachments),
        .pAttachments = Attachments,
        .subpassCount = 1,
        .pSubpasses = &SubpassDescription,
        .dependencyCount = 0,
        .pDependencies = NULL,
    };

    VkRenderPass RenderPass;
    vkCreateRenderPass(Renderer->Device, &Info, NULL, &RenderPass);
    return RenderPass;
}

static VkRenderPass Rr_CreateRenderPassColorDepthLoad(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VkAttachmentDescription Attachments[2] = { {
                                                   .samples = 1,
                                                   .format = RR_COLOR_FORMAT,
                                                   .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                   .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                   .flags = 0,
                                                   .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                                                   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                               },
                                               {
                                                   .samples = 1,
                                                   .format = RR_DEPTH_FORMAT,
                                                   .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                   .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                   .flags = 0,
                                                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                   .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                               } };

    VkAttachmentReference ColorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference DepthAttachmentRef = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription SubpassDescription = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ColorAttachmentRef,
        .pDepthStencilAttachment = &DepthAttachmentRef,
        .pResolveAttachments = NULL,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    VkRenderPassCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .attachmentCount = SDL_arraysize(Attachments),
        .pAttachments = Attachments,
        .subpassCount = 1,
        .pSubpasses = &SubpassDescription,
        .dependencyCount = 0,
        .pDependencies = NULL,
    };

    VkRenderPass RenderPass;
    vkCreateRenderPass(Renderer->Device, &Info, NULL, &RenderPass);
    return RenderPass;
}

static VkRenderPass Rr_CreateRenderPassDepth(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VkAttachmentDescription Attachments[] = {
        {
            .samples = 1,
            .format = RR_DEPTH_FORMAT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .flags = 0,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        },
    };

    VkAttachmentReference DepthAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription SubpassDescription = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 0,
        .pColorAttachments = NULL,
        .pDepthStencilAttachment = &DepthAttachmentRef,
        .pResolveAttachments = NULL,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    VkSubpassDependency Dependencies[] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .dependencyFlags = 0,
        },
        // {
        //     .srcSubpass = 0,
        //     .dstSubpass = VK_SUBPASS_EXTERNAL,
        //     .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        //     .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        //     .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        //     .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        //     .dependencyFlags = 0,
        // },
    };

    VkRenderPassCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .attachmentCount = SDL_arraysize(Attachments),
        .pAttachments = Attachments,
        .subpassCount = 1,
        .pSubpasses = &SubpassDescription,
        .dependencyCount = SDL_arraysize(Dependencies),
        .pDependencies = Dependencies,
    };

    VkRenderPass RenderPass;
    vkCreateRenderPass(Renderer->Device, &Info, NULL, &RenderPass);
    return RenderPass;
}

static void Rr_InitRenderPasses(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Renderer->RenderPasses.ColorDepth = Rr_CreateRenderPassColorDepth(App);
    Renderer->RenderPasses.ColorDepthLoad = Rr_CreateRenderPassColorDepthLoad(App);
    Renderer->RenderPasses.Depth = Rr_CreateRenderPassDepth(App);
}

static void Rr_CleanupRenderPasses(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    vkDestroyRenderPass(Renderer->Device, Renderer->RenderPasses.ColorDepth, NULL);
    vkDestroyRenderPass(Renderer->Device, Renderer->RenderPasses.ColorDepthLoad, NULL);
    vkDestroyRenderPass(Renderer->Device, Renderer->RenderPasses.Depth, NULL);
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

static void Rr_InitNullTextures(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);
    Rr_WriteBuffer StagingBuffer = { .Buffer = Rr_CreateMappedBuffer(App, 256, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                                     .Offset = 0 };
    Rr_UploadContext UploadContext = {
        .StagingBuffer = &StagingBuffer,
        .TransferCommandBuffer = CommandBuffer,
    };
    uint32_t WhiteData = 0xffffffff;
    Renderer->NullTextures.White =
        Rr_CreateColorImageFromMemory(App, &UploadContext, (char *)&WhiteData, 1, 1, RR_FALSE);
    uint32_t NormalData = 0xffff8888;
    Renderer->NullTextures.Normal =
        Rr_CreateColorImageFromMemory(App, &UploadContext, (char *)&NormalData, 1, 1, RR_FALSE);
    Rr_EndImmediate(Renderer);

    Rr_DestroyBuffer(App, StagingBuffer.Buffer);
}

void Rr_InitRenderer(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    SDL_Window *Window = App->Window;
    Rr_AppConfig *Config = App->Config;

    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = SDL_GetWindowTitle(Window),
        .pEngineName = "Rr_Renderer",
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0),
    };

    /* Gather required extensions. */
    const char *AppExtensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    uint32_t AppExtensionCount = SDL_arraysize(AppExtensions);
    AppExtensionCount = 0; /* Use Vulkan Configurator! */

    uint32_t SDLExtensionCount;
    const char *const *SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&SDLExtensionCount);

    uint32_t ExtensionCount = SDLExtensionCount + AppExtensionCount;
    const char **Extensions = Rr_StackAlloc(const char *, ExtensionCount);
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
        &Renderer->TransferQueue.FamilyIndex);
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
    Rr_InitSamplers(App);
    Rr_InitRenderPasses(App);
    Rr_InitDescriptors(App);

    uint32_t Width, Height;
    SDL_GetWindowSizeInPixels(Window, (int32_t *)&Width, (int32_t *)&Height);
    Rr_InitSwapchain(App, &Width, &Height);

    Rr_InitFrames(App);
    Rr_InitImmediateMode(App);
    Rr_InitGenericPipelineLayout(App);
    Rr_InitNullTextures(App);
    Rr_InitTextRenderer(App);

    Rr_StackFree(Extensions);
}

Rr_Bool Rr_NewFrame(Rr_App *App, void *Window)
{
    Rr_Renderer *Renderer = &App->Renderer;

    int32_t bResizePending = SDL_GetAtomicInt(&Renderer->Swapchain.bResizePending);
    if(bResizePending == RR_TRUE)
    {
        vkDeviceWaitIdle(Renderer->Device);

        int32_t Width, Height;
        SDL_GetWindowSizeInPixels(Window, &Width, &Height);

        Rr_Bool Minimized = (SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED);

        if(!Minimized && Width > 0 && Height > 0 && Rr_InitSwapchain(App, (uint32_t *)&Width, (uint32_t *)&Height))
        {
            SDL_SetAtomicInt(&Renderer->Swapchain.bResizePending, 0);
            return RR_TRUE;
        }

        return RR_FALSE;
    }
    return RR_TRUE;
}

void Rr_CleanupRenderer(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    vkDeviceWaitIdle(Renderer->Device);

    App->Config->CleanupFunc(App, App->UserData);

    if(Renderer->ImGui.IsInitialized)
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        igDestroyContext(NULL);
        vkDestroyDescriptorPool(Device, Renderer->ImGui.DescriptorPool, NULL);
    }

    Rr_CleanupTextRenderer(App);

    /* Generic Pipeline Layout */
    vkDestroyPipelineLayout(Device, Renderer->GenericPipelineLayout, NULL);
    for(size_t Index = 0; Index < RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT; ++Index)
    {
        vkDestroyDescriptorSetLayout(Device, Renderer->GenericDescriptorSetLayouts[Index], NULL);
    }

    Rr_DestroyDescriptorAllocator(&Renderer->GlobalDescriptorAllocator, Device);

    Rr_CleanupFrames(App);

    Rr_DestroyImage(App, Renderer->NullTextures.White);
    Rr_DestroyImage(App, Renderer->NullTextures.Normal);

    Rr_DestroyDrawTarget(App, Renderer->DrawTarget);

    Rr_CleanupTransientCommandPools(Renderer);
    Rr_CleanupImmediateMode(App);
    Rr_CleanupRenderPasses(App);
    Rr_CleanupSamplers(App);
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

    VkCommandBufferBeginInfo BeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

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
    vkWaitForFences(Renderer->Device, 1, &ImmediateMode->Fence, RR_TRUE, UINT64_MAX);
}

static void Rr_ResetFrameResources(Rr_Frame *Frame)
{
    Frame->StagingBuffer.Offset = 0;
    Frame->CommonBuffer.Offset = 0;
    Frame->PerDrawBuffer.Offset = 0;

    RR_ZERO(Frame->Graph);

    Rr_ResetArena(&Frame->Arena);
}

static void Rr_ProcessPendingLoads(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;

    if(SDL_TryLockSpinlock(&App->SyncArena.Lock))
    {
        for(size_t Index = 0; Index < Renderer->PendingLoadsSlice.Count; ++Index)
        {
            Rr_PendingLoad *PendingLoad = &Renderer->PendingLoadsSlice.Data[Index];
            PendingLoad->LoadingCallback(App, PendingLoad->Userdata);
        }
        RR_SLICE_EMPTY(&Renderer->PendingLoadsSlice);

        SDL_UnlockSpinlock(&App->SyncArena.Lock);
    }
}

void Rr_PrepareFrame(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    /* @TODO: Ugly. */
    Frame->Graph.Arena = &Frame->Arena;

    Rr_ProcessPendingLoads(App);
}

void Rr_Draw(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;
    Rr_Swapchain *Swapchain = &Renderer->Swapchain;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);

    vkWaitForFences(Device, 1, &Frame->RenderFence, RR_TRUE, 1000000000);
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
        SDL_SetAtomicInt(&Renderer->Swapchain.bResizePending, 1);
        return;
    }
    if(Result == VK_SUBOPTIMAL_KHR)
    {
        SDL_SetAtomicInt(&Renderer->Swapchain.bResizePending, 1);
    }
    SDL_assert(Result >= 0);

    Frame->CurrentSwapchainImage = Swapchain->Images[SwapchainImageIndex].Handle;

    /* Begin main command buffer. */

    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;
    VkCommandBufferBeginInfo CommandBufferBeginInfo =
        GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

    /* Execute Frame Graph */

    Rr_ExecuteGraph(App, &Frame->Graph, Scratch.Arena);

    vkEndCommandBuffer(CommandBuffer);

    /* Submit frame command buffer and queue present. */

    VkSemaphore *WaitSemaphores = RR_ARENA_ALLOC_ONE(Scratch.Arena, sizeof(VkSemaphore));
    VkPipelineStageFlags *WaitDstStages = RR_ARENA_ALLOC_ONE(Scratch.Arena, sizeof(VkPipelineStageFlags));
    size_t WaitSemaphoreIndex = 1;
    WaitSemaphores[0] = Frame->SwapchainSemaphore;
    WaitDstStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo SubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &CommandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &Frame->RenderSemaphore,
        .waitSemaphoreCount = WaitSemaphoreIndex,
        .pWaitSemaphores = WaitSemaphores,
        .pWaitDstStageMask = WaitDstStages,
    };

    SDL_LockSpinlock(&Renderer->GraphicsQueue.Lock);

    vkQueueSubmit(Renderer->GraphicsQueue.Handle, 1, &SubmitInfo, Frame->RenderFence);

    SDL_UnlockSpinlock(&Renderer->GraphicsQueue.Lock);

    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->RenderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    Result = vkQueuePresentKHR(Renderer->GraphicsQueue.Handle, &PresentInfo);
    if(Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_SetAtomicInt(&Renderer->Swapchain.bResizePending, 1);
    }

    Renderer->FrameNumber++;
    Renderer->CurrentFrameIndex = Renderer->FrameNumber % RR_FRAME_OVERLAP;

    /* Cleanup resources. */

    Rr_ResetFrameResources(Frame);
    Rr_DestroyArenaScratch(Scratch);
}

Rr_Frame *Rr_GetCurrentFrame(Rr_Renderer *Renderer)
{
    return &Renderer->Frames[Renderer->CurrentFrameIndex];
}

Rr_Bool Rr_IsUsingTransferQueue(Rr_Renderer *Renderer)
{
    return Renderer->TransferQueue.Handle != VK_NULL_HANDLE;
}

VkDeviceSize Rr_GetUniformAlignment(Rr_Renderer *Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
}
