#include "Rr_Renderer.h"

#include "Rr_Barrier.h"
#include "Rr_Rendering.h"
#include "Rr_Image.h"
#include "Rr_App.h"
#include "Rr_BuiltinAssets.inc"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// static void Rr_CalculateDrawTargetResolution(Rr_Renderer* const Renderer, const u32 WindowWidth, const u32 WindowHeight)
// {
//     Renderer->ActiveResolution.width = Renderer->ReferenceResolution.width;
//     Renderer->ActiveResolution.height = Renderer->ReferenceResolution.height;
//
//     const i32 MaxAvailableScale = SDL_min(WindowWidth / Renderer->ReferenceResolution.width, WindowHeight / Renderer->ReferenceResolution.height);
//     if (MaxAvailableScale >= 1)
//     {
//         Renderer->ActiveResolution.width += (WindowWidth - MaxAvailableScale * Renderer->ReferenceResolution.width) / MaxAvailableScale;
//         Renderer->ActiveResolution.height += (WindowHeight - MaxAvailableScale * Renderer->ReferenceResolution.height) / MaxAvailableScale;
//     }
//
//     Renderer->Scale = MaxAvailableScale;
// }

static void Rr_CleanupSwapchain(Rr_App* App, VkSwapchainKHR Swapchain)
{
    Rr_Renderer* Renderer = &App->Renderer;

    for (u32 Index = 0; Index < Renderer->Swapchain.ImageCount; Index++)
    {
        vkDestroyImageView(Renderer->Device, Renderer->Swapchain.Images[Index].View, NULL);
    }
    vkDestroySwapchainKHR(Renderer->Device, Swapchain, NULL);
}

static bool Rr_InitSwapchain(Rr_App* App, u32* Width, u32* Height)
{
    Rr_Renderer* Renderer = &App->Renderer;

    VkSwapchainKHR OldSwapchain = Renderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &SurfCaps);

    if (SurfCaps.currentExtent.width == 0 || SurfCaps.currentExtent.height == 0)
    {
        return false;
    }
    if (SurfCaps.currentExtent.width == UINT32_MAX)
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

    u32 PresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &PresentModeCount, NULL);
    SDL_assert(PresentModeCount > 0);

    VkPresentModeKHR* PresentModes = Rr_StackAlloc(VkPresentModeKHR, PresentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &PresentModeCount, PresentModes);

    VkPresentModeKHR SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 Index = 0; Index < PresentModeCount; Index++)
    {
        if (PresentModes[Index] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            SwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if (PresentModes[Index] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            SwapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }
    Rr_StackFree(PresentModes);

    u32 DesiredNumberOfSwapchainImages = SurfCaps.minImageCount + 1;
    if ((SurfCaps.maxImageCount > 0) && (DesiredNumberOfSwapchainImages > SurfCaps.maxImageCount))
    {
        DesiredNumberOfSwapchainImages = SurfCaps.maxImageCount;
    }

    VkSurfaceTransformFlagsKHR PreTransform;
    if (SurfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        PreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        PreTransform = SurfCaps.currentTransform;
    }

    u32 FormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &FormatCount, NULL);
    SDL_assert(FormatCount > 0);

    VkSurfaceFormatKHR* SurfaceFormats = Rr_StackAlloc(VkSurfaceFormatKHR, FormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &FormatCount, SurfaceFormats);

    bool bPreferredFormatFound = false;
    for (u32 Index = 0; Index < FormatCount; Index++)
    {
        const VkSurfaceFormatKHR* SurfaceFormat = &SurfaceFormats[Index];

        if (SurfaceFormat->format == VK_FORMAT_B8G8R8A8_UNORM || SurfaceFormat->format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            Renderer->Swapchain.Format = SurfaceFormat->format;
            Renderer->Swapchain.ColorSpace = SurfaceFormat->colorSpace;
            bPreferredFormatFound = true;
            break;
        }
    }

    if (!bPreferredFormatFound)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No preferred surface format found!");
        abort();
    }

    VkCompositeAlphaFlagBitsKHR CompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    const VkCompositeAlphaFlagBitsKHR CompositeAlphaFlags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (u32 Index = 0; Index < SDL_arraysize(CompositeAlphaFlags); Index++)
    {
        const VkCompositeAlphaFlagBitsKHR CompositeAlphaFlag = CompositeAlphaFlags[Index];
        if (SurfCaps.supportedCompositeAlpha & CompositeAlphaFlag)
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

    if (SurfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        SwapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (SurfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        SwapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    vkCreateSwapchainKHR(Renderer->Device, &SwapchainCI, NULL, &Renderer->Swapchain.Handle);

    if (OldSwapchain != VK_NULL_HANDLE)
    {
        Rr_CleanupSwapchain(App, OldSwapchain);
    }

    u32 ImageCount = 0;
    vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, NULL);
    SDL_assert(ImageCount <= RR_MAX_SWAPCHAIN_IMAGE_COUNT);

    Renderer->Swapchain.ImageCount = ImageCount;
    VkImage* Images = Rr_StackAlloc(VkImage, ImageCount);
    vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, Images);

    VkImageViewCreateInfo ColorAttachmentView = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = Renderer->Swapchain.Format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    for (u32 i = 0; i < ImageCount; i++)
    {
        Renderer->Swapchain.Images[i].Handle = Images[i];
        ColorAttachmentView.image = Images[i];
        vkCreateImageView(Renderer->Device, &ColorAttachmentView, NULL, &Renderer->Swapchain.Images[i].View);
    }

    bool bDrawTargetDirty = true;
    if (Renderer->DrawTarget != NULL)
    {
        VkExtent3D Extent = Renderer->DrawTarget->ColorImage->Extent;
        if (*Width <= Extent.width || *Height <= Extent.height)
        {
            bDrawTargetDirty = false;
        }
        else
        {
            Rr_DestroyDrawTarget(App, Renderer->DrawTarget);
        }
    }
    if (bDrawTargetDirty)
    {
        SDL_DisplayID DisplayID = SDL_GetPrimaryDisplay();
        SDL_Rect DisplayBounds;
        SDL_GetDisplayBounds(DisplayID, &DisplayBounds);

        u32 DrawTargetWidth = SDL_max(DisplayBounds.w, *Width);
        u32 DrawTargetHeight = SDL_max(DisplayBounds.h, *Height);
        Renderer->DrawTarget = Rr_CreateDrawTarget(App, DrawTargetWidth, DrawTargetHeight);

        SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Creating primary draw target with size %dx%d", DrawTargetWidth, DrawTargetHeight);
    }

    Renderer->SwapchainSize = (VkExtent2D){ .width = *Width, .height = *Height };

    Rr_StackFree(Images);
    Rr_StackFree(SurfaceFormats);

    return true;
}

static void Rr_InitFrames(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;
    Rr_Frame* Frames = Renderer->Frames;

    const VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo SemaphoreCreateInfo = GetSemaphoreCreateInfo(0);

    for (usize Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame* Frame = &Frames[Index];
        SDL_zerop(Frame);

        vkCreateFence(Device, &FenceCreateInfo, NULL, &Frame->RenderFence);
        vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->SwapchainSemaphore);
        vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->RenderSemaphore);

        Rr_DescriptorPoolSizeRatio Ratios[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
        };
        Frame->DescriptorAllocator = Rr_CreateDescriptorAllocator(Renderer->Device, 1000, Ratios, SDL_arraysize(Ratios), &App->PermanentArena);

        VkCommandPoolCreateInfo CommandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        };
        vkCreateCommandPool(Renderer->Device, &CommandPoolInfo, NULL, &Frame->CommandPool);
        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Frame->CommandPool, 1);
        vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &Frame->MainCommandBuffer);

        Frame->StagingBuffer.Buffer = Rr_CreateMappedBuffer(App, RR_STAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        Frame->DrawBuffer.Buffer = Rr_CreateMappedBuffer(App, 66560, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        Frame->CommonBuffer.Buffer = Rr_CreateDeviceUniformBuffer(App, 66560);

        Frame->Arena = Rr_CreateArena(RR_PER_FRAME_ARENA_SIZE);
    }
}

static void Rr_CleanupFrames(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    for (usize Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame* Frame = &Renderer->Frames[Index];
        vkDestroyCommandPool(Renderer->Device, Frame->CommandPool, NULL);

        vkDestroyFence(Device, Frame->RenderFence, NULL);
        vkDestroySemaphore(Device, Frame->RenderSemaphore, NULL);
        vkDestroySemaphore(Device, Frame->SwapchainSemaphore, NULL);

        Rr_DestroyDescriptorAllocator(&Frame->DescriptorAllocator, Device);

        Rr_DestroyBuffer(App, Frame->StagingBuffer.Buffer);
        Rr_DestroyBuffer(App, Frame->DrawBuffer.Buffer);
        Rr_DestroyBuffer(App, Frame->CommonBuffer.Buffer);

        Rr_DestroyArena(&Frame->Arena);
    }
}

static void Rr_InitVMA(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

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
    const VmaAllocatorCreateInfo AllocatorInfo = {
        .flags = 0,
        .physicalDevice = Renderer->PhysicalDevice.Handle,
        .device = Renderer->Device,
        .pVulkanFunctions = &VulkanFunctions,
        .instance = Renderer->Instance,
    };
    vmaCreateAllocator(&AllocatorInfo, &Renderer->Allocator);
}

static void Rr_InitDescriptors(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_DescriptorPoolSizeRatio Ratios[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    Renderer->GlobalDescriptorAllocator = Rr_CreateDescriptorAllocator(Renderer->Device, 10, Ratios, SDL_arraysize(Ratios), &App->PermanentArena);
}

static PFN_vkVoidFunction Rr_LoadVulkanFunction(const char* FuncName, void* Userdata)
{
    return (PFN_vkVoidFunction)vkGetInstanceProcAddr(volkGetLoadedInstance(), FuncName);
}

void Rr_InitImGui(Rr_App* App)
{
    SDL_Window* Window = App->Window;
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    VkDescriptorPoolSize PoolSizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    const VkDescriptorPoolCreateInfo PoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = (u32)SDL_arraysize(PoolSizes),
        .pPoolSizes = PoolSizes,
    };

    vkCreateDescriptorPool(Device, &PoolCreateInfo, NULL, &Renderer->ImGui.DescriptorPool);

    igCreateContext(NULL);
    ImGuiIO* IO = igGetIO();
    IO->IniFilename = NULL;

    ImGui_ImplVulkan_LoadFunctions(Rr_LoadVulkanFunction, NULL);
    ImGui_ImplSDL3_InitForVulkan(Window);

    ImGui_ImplVulkan_InitInfo InitInfo = {
        .Instance = Renderer->Instance,
        .PhysicalDevice = Renderer->PhysicalDevice.Handle,
        .Device = Device,
        .QueueFamily = Renderer->GraphicsQueue.FamilyIndex,
        .Queue = Renderer->GraphicsQueue.Handle,
        .DescriptorPool = Renderer->ImGui.DescriptorPool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .UseDynamicRendering = false,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .RenderPass = Renderer->RenderPassNoClear
    };

    ImGui_ImplVulkan_Init(&InitInfo);

    const f32 WindowScale = SDL_GetWindowDisplayScale(Window);
    ImGuiStyle_ScaleAllSizes(igGetStyle(), WindowScale);

    /* Init default font. */
    Rr_Asset MartianMonoTTF = Rr_LoadAsset(RR_BUILTIN_MARTIANMONO_TTF);
    ImFontConfig* FontConfig = ImFontConfig_ImFontConfig();
    FontConfig->FontDataOwnedByAtlas = false; /* Don't transfer asset ownership to ImGui, it will crash otherwise! */
    ImFontAtlas_AddFontFromMemoryTTF(IO->Fonts, (void*)MartianMonoTTF.Data, (i32)MartianMonoTTF.Length, SDL_floorf(16.0f * WindowScale), FontConfig, NULL);
    igMemFree(FontConfig);

    ImGui_ImplVulkan_CreateFontsTexture();

    Renderer->ImGui.bInitiated = true;
}

static void Rr_InitImmediateMode(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    VkDevice Device = Renderer->Device;
    Rr_ImmediateMode* ImmediateMode = &Renderer->ImmediateMode;

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

static void Rr_CleanupImmediateMode(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    vkDestroyCommandPool(Renderer->Device, Renderer->ImmediateMode.CommandPool, NULL);
    vkDestroyFence(Renderer->Device, Renderer->ImmediateMode.Fence, NULL);
}

static void Rr_InitGenericPipelineLayout(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    /* Descriptor Set Layouts */
    Rr_DescriptorLayoutBuilder DescriptorLayoutBuilder = { 0 };
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_AddDescriptorArray(&DescriptorLayoutBuilder, 1, RR_MAX_TEXTURES_PER_MATERIAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS] =
        Rr_BuildDescriptorLayout(
            &DescriptorLayoutBuilder,
            Device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    Rr_ClearDescriptors(&DescriptorLayoutBuilder);
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_AddDescriptorArray(&DescriptorLayoutBuilder, 1, RR_MAX_TEXTURES_PER_MATERIAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL] =
        Rr_BuildDescriptorLayout(
            &DescriptorLayoutBuilder,
            Device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    Rr_ClearDescriptors(&DescriptorLayoutBuilder);
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
    Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW] =
        Rr_BuildDescriptorLayout(
            &DescriptorLayoutBuilder,
            Device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    /* Pipeline Layout */
    VkPushConstantRange PushConstantRange = {
        .offset = 0,
        .size = 128,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };

    const VkPipelineLayoutCreateInfo LayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT,
        .pSetLayouts = Renderer->GenericDescriptorSetLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };
    vkCreatePipelineLayout(Device, &LayoutInfo, NULL, &Renderer->GenericPipelineLayout);
}

static void Rr_InitSamplers(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    VkSamplerCreateInfo SamplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    SamplerInfo.magFilter = VK_FILTER_NEAREST;
    SamplerInfo.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(Renderer->Device, &SamplerInfo, NULL, &Renderer->NearestSampler);

    SamplerInfo.magFilter = VK_FILTER_LINEAR;
    SamplerInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(Renderer->Device, &SamplerInfo, NULL, &Renderer->LinearSampler);
}

static void Rr_CleanupSamplers(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    vkDestroySampler(Renderer->Device, Renderer->NearestSampler, NULL);
    vkDestroySampler(Renderer->Device, Renderer->LinearSampler, NULL);
}

static void Rr_InitRenderPass(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    VkAttachmentDescription Attachments[2] = {
        (VkAttachmentDescription){
            .samples = 1,
            .format = RR_COLOR_FORMAT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .flags = 0,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE },
        (VkAttachmentDescription){
            .samples = 1,
            .format = RR_DEPTH_FORMAT,
            .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .flags = 0,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE }
    };

    VkAttachmentReference ColorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference DepthAttachmentRef = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
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
        .pPreserveAttachments = NULL
    };

    VkSubpassDependency Dependencies[] = {
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
        },
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
        }
    };

    VkRenderPassCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .attachmentCount = 2,
        .pAttachments = Attachments,
        .subpassCount = 1,
        .pSubpasses = &SubpassDescription,
        .dependencyCount = 0,
        .pDependencies = Dependencies
    };

    vkCreateRenderPass(Renderer->Device, &Info, NULL, &Renderer->RenderPass);

    Attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    Attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    Attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    vkCreateRenderPass(Renderer->Device, &Info, NULL, &Renderer->RenderPassNoClear);
}

static void Rr_CleanupRenderPass(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    vkDestroyRenderPass(Renderer->Device, Renderer->RenderPass, NULL);
    vkDestroyRenderPass(Renderer->Device, Renderer->RenderPassNoClear, NULL);
}

static void Rr_InitTransientCommandPools(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

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

static void Rr_CleanupTransientCommandPools(Rr_Renderer* Renderer)
{
    vkDestroyCommandPool(Renderer->Device, Renderer->GraphicsQueue.TransientCommandPool, NULL);
    vkDestroyCommandPool(Renderer->Device, Renderer->TransferQueue.TransientCommandPool, NULL);
}

static void Rr_InitNullTexture(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    Renderer->NullTexture = Rr_CreateImage(App, (VkExtent3D){ .width = 1, .height = 1, .depth = 1 }, RR_COLOR_FORMAT, VK_IMAGE_USAGE_SAMPLED_BIT, false);

    VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);
    Rr_ImageBarrier Barrier = {
        .CommandBuffer = CommandBuffer,
        .Image = Renderer->NullTexture->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = 0,
        .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
    };
    Rr_ChainImageBarrier(&Barrier,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Rr_EndImmediate(Renderer);
}

void Rr_InitRenderer(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    SDL_Window* Window = App->Window;
    const Rr_AppConfig* Config = App->Config;

    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = SDL_GetWindowTitle(Window),
        .pEngineName = "Rr_Renderer",
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0),
    };

    /* Gather required extensions. */
    const char* AppExtensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    u32 AppExtensionCount = SDL_arraysize(AppExtensions);
    AppExtensionCount = 0; /* Use Vulkan Configurator! */

    u32 SDLExtensionCount;
    const char* const* SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&SDLExtensionCount);

    const u32 ExtensionCount = SDLExtensionCount + AppExtensionCount;
    const char** Extensions = Rr_StackAlloc(const char*, ExtensionCount);
    for (u32 Index = 0; Index < ExtensionCount; Index++)
    {
        Extensions[Index] = SDLExtensions[Index];
    }
    for (u32 Index = 0; Index < AppExtensionCount; Index++)
    {
        Extensions[Index + SDLExtensionCount] = AppExtensions[Index];
    }

    /* Create Vulkan instance. */
    const VkInstanceCreateInfo InstanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &VKAppInfo,
        .enabledExtensionCount = ExtensionCount,
        .ppEnabledExtensionNames = Extensions,
    };

    vkCreateInstance(&InstanceCreateInfo, NULL, &Renderer->Instance);

    volkLoadInstance(Renderer->Instance);

    if (SDL_Vulkan_CreateSurface(Window, Renderer->Instance, NULL, &Renderer->Surface) != SDL_TRUE)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create Vulkan surface: %s", SDL_GetError());
        abort();
    }

    Renderer->PhysicalDevice = Rr_CreatePhysicalDevice(
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
    Renderer->GraphicsQueueMutex = SDL_CreateMutex();

    Renderer->ObjectStorage = Rr_CreateObjectStorage();

    volkLoadDevice(Renderer->Device);

    Rr_InitVMA(App);
    Rr_InitTransientCommandPools(App);
    Rr_InitSamplers(App);
    Rr_InitRenderPass(App);
    Rr_InitDescriptors(App);

    u32 Width, Height;
    SDL_GetWindowSizeInPixels(Window, (i32*)&Width, (i32*)&Height);
    Rr_InitSwapchain(App, &Width, &Height);

    Rr_InitFrames(App);
    Rr_InitImmediateMode(App);
    Rr_InitGenericPipelineLayout(App);
    Rr_InitNullTexture(App);
    Rr_InitTextRenderer(App);

    Rr_StackFree(Extensions);
}

bool Rr_NewFrame(Rr_App* App, void* Window)
{
    Rr_Renderer* Renderer = &App->Renderer;

    const i32 bResizePending = SDL_AtomicGet(&Renderer->Swapchain.bResizePending);
    if (bResizePending == true)
    {
        vkDeviceWaitIdle(Renderer->Device);

        i32 Width, Height;
        SDL_GetWindowSizeInPixels(Window, &Width, &Height);

        const bool bMinimized = SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED;

        if (!bMinimized && Width > 0 && Height > 0 && Rr_InitSwapchain(App, (u32*)&Width, (u32*)&Height))
        {
            SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 0);
            return true;
        }

        return false;
    }
    return true;
}

void Rr_CleanupRenderer(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    vkDeviceWaitIdle(Renderer->Device);

    App->Config->CleanupFunc(App);

    if (Renderer->ImGui.bInitiated)
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        igDestroyContext(NULL);
        vkDestroyDescriptorPool(Device, Renderer->ImGui.DescriptorPool, NULL);
    }

    Rr_CleanupTextRenderer(App);

    /* Generic Pipeline Layout */
    vkDestroyPipelineLayout(Device, Renderer->GenericPipelineLayout, NULL);
    for (int Index = 0; Index < RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT; ++Index)
    {
        vkDestroyDescriptorSetLayout(Device, Renderer->GenericDescriptorSetLayouts[Index], NULL);
    }

    Rr_DestroyDescriptorAllocator(&Renderer->GlobalDescriptorAllocator, Device);

    Rr_CleanupFrames(App);

    Rr_DestroyImage(App, Renderer->NullTexture);

    Rr_DestroyDrawTarget(App, Renderer->DrawTarget);

    SDL_DestroyMutex(Renderer->GraphicsQueueMutex);

    Rr_CleanupTransientCommandPools(Renderer);
    Rr_CleanupImmediateMode(App);
    Rr_CleanupRenderPass(App);
    Rr_CleanupSamplers(App);
    Rr_CleanupSwapchain(App, Renderer->Swapchain.Handle);

    Rr_DestroyObjectStorage(&Renderer->ObjectStorage);

    vmaDestroyAllocator(Renderer->Allocator);

    vkDestroySurfaceKHR(Renderer->Instance, Renderer->Surface, NULL);
    vkDestroyDevice(Renderer->Device, NULL);

    vkDestroyInstance(Renderer->Instance, NULL);
}

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer)
{
    Rr_ImmediateMode* ImmediateMode = &Renderer->ImmediateMode;
    vkResetFences(Renderer->Device, 1, &ImmediateMode->Fence);
    vkResetCommandBuffer(ImmediateMode->CommandBuffer, 0);

    VkCommandBufferBeginInfo BeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vkBeginCommandBuffer(ImmediateMode->CommandBuffer, &BeginInfo);

    return ImmediateMode->CommandBuffer;
}

void Rr_EndImmediate(Rr_Renderer* Renderer)
{
    Rr_ImmediateMode* ImmediateMode = &Renderer->ImmediateMode;

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
        .pWaitDstStageMask = NULL
    };

    vkQueueSubmit(Renderer->GraphicsQueue.Handle, 1, &SubmitInfo, ImmediateMode->Fence);
    vkWaitForFences(Renderer->Device, 1, &ImmediateMode->Fence, true, UINT64_MAX);
}

static void Rr_ResetFrameResources(Rr_Frame* Frame)
{
    Frame->StagingBuffer.Offset = 0;
    Frame->DrawBuffer.Offset = 0;
    Frame->CommonBuffer.Offset = 0;

    Rr_ResetArena(&Frame->Arena);
}

void Rr_ProcessPendingLoads(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    if (SDL_TryLockMutex(App->SyncArena.Mutex) == 0)
    {
        for (usize Index = 0; Index < Rr_SliceLength(&Renderer->PendingLoadsSlice); ++Index)
        {
            Rr_PendingLoad* PendingLoad = &Renderer->PendingLoadsSlice.Data[Index];
            PendingLoad->LoadingCallback(App, PendingLoad->Userdata);
        }
        Rr_SliceEmpty(&Renderer->PendingLoadsSlice);

        SDL_UnlockMutex(App->SyncArena.Mutex);
    }
}

void Rr_Draw(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;
    Rr_Swapchain* Swapchain = &Renderer->Swapchain;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);

    vkWaitForFences(Device, 1, &Frame->RenderFence, true, 1000000000);
    vkResetFences(Device, 1, &Frame->RenderFence);

    Rr_ResetDescriptorAllocator(&Frame->DescriptorAllocator, Renderer->Device);

    /* Acquire swapchain image. */
    u32 SwapchainImageIndex;
    VkResult Result = vkAcquireNextImageKHR(Device, Swapchain->Handle, 1000000000, Frame->SwapchainSemaphore, VK_NULL_HANDLE, &SwapchainImageIndex);
    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 1);
        return;
    }
    if (Result == VK_SUBOPTIMAL_KHR)
    {
        SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 1);
    }
    SDL_assert(Result >= 0);

    VkImage SwapchainImage = Swapchain->Images[SwapchainImageIndex].Handle;

    /* Begin main command buffer. */
    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;
    VkCommandBufferBeginInfo CommandBufferBeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

    /* Rendering */
    Rr_Image* ColorImage = Renderer->DrawTarget->ColorImage;
    Rr_Image* DepthImage = Renderer->DrawTarget->DepthImage;

    Rr_ImageBarrier ColorImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = ColorImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    Rr_ImageBarrier DepthImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = DepthImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .StageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
    };

    Rr_ChainImageBarrier(&ColorImageTransition,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    Rr_ChainImageBarrier(&DepthImageTransition,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    /* Flush Rendering Contexts */
    for (usize Index = 0; Index < Frame->DrawContextsSlice.Length; ++Index)
    {
        Rr_FlushRenderingContext(&Frame->DrawContextsSlice.Data[Index], Scratch.Arena);
    }
    Rr_SliceClear(&Frame->DrawContextsSlice);

    /* Render Dear ImGui if needed. */
    Rr_ImGui* ImGui = &Renderer->ImGui;
    if (ImGui->bInitiated)
    {
        VkRenderPassBeginInfo rp_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = Renderer->RenderPassNoClear,
            .framebuffer = Renderer->DrawTarget->Framebuffer,
            .renderArea.extent.width = Renderer->DrawTarget->ColorImage->Extent.width,
            .renderArea.extent.height = Renderer->DrawTarget->ColorImage->Extent.height,
            .clearValueCount = 0,
            .pClearValues = NULL,
        };
        vkCmdBeginRenderPass(CommandBuffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(igGetDrawData(), CommandBuffer, VK_NULL_HANDLE);

        vkCmdEndRenderPass(CommandBuffer);
    }

    /* Blit primary draw target to swapchain image. */
    Rr_ChainImageBarrier(&ColorImageTransition,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    Rr_ImageBarrier SwapchainImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = SwapchainImage,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_NONE,
        .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    Rr_ChainImageBarrier(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    Rr_BlitColorImage(
        CommandBuffer,
        ColorImage->Handle,
        SwapchainImage,
        Renderer->SwapchainSize,
        Renderer->SwapchainSize);

    Rr_ChainImageBarrier(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(CommandBuffer);

    /* Submit frame command buffer and queue present. */
    VkSemaphore* WaitSemaphores = Rr_ArenaAllocOne(Scratch.Arena, sizeof(VkSemaphore));
    VkPipelineStageFlags* WaitDstStages = Rr_ArenaAllocOne(Scratch.Arena, sizeof(VkPipelineStageFlags));
    usize WaitSemaphoreIndex = 1;
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
        .pWaitDstStageMask = WaitDstStages
    };

    SDL_LockMutex(Renderer->GraphicsQueueMutex);

    vkQueueSubmit(Renderer->GraphicsQueue.Handle, 1, &SubmitInfo, Frame->RenderFence);

    const VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->RenderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    Result = vkQueuePresentKHR(Renderer->GraphicsQueue.Handle, &PresentInfo);
    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 1);
    }

    Renderer->FrameNumber++;
    Renderer->CurrentFrameIndex = Renderer->FrameNumber % RR_FRAME_OVERLAP;

    SDL_UnlockMutex(Renderer->GraphicsQueueMutex);

    /* Cleanup resources. */
    Rr_ResetFrameResources(Frame);
    Rr_DestroyArenaScratch(Scratch);
}

Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer)
{
    return &Renderer->Frames[Renderer->CurrentFrameIndex];
}

bool Rr_IsUsingTransferQueue(Rr_Renderer* Renderer)
{
    return Renderer->TransferQueue.FamilyIndex == Renderer->GraphicsQueue.FamilyIndex;
}

VkDeviceSize Rr_GetUniformAlignment(Rr_Renderer* Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
}
