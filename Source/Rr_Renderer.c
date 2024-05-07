#include "Rr_Renderer.h"

#include "Rr_Text.h"
#include "Rr_Defines.h"
#include "Rr_App.h"
#include "Rr_Array.h"
#include "Rr_Vulkan.h"
#include "Rr_Descriptor.h"
#include "Rr_Buffer.h"
#include "Rr_Memory.h"
#include "Rr_Util.h"
#include "Rr_Types.h"
#include "Rr_Barrier.h"

#include <cglm/ivec2.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_stdinc.h>

#include <string.h>

static void Rr_BlitPrerenderedDepth(
    VkCommandBuffer CommandBuffer,
    VkImage Source,
    VkImage Destination,
    VkExtent2D SrcSize,
    VkExtent2D DstSize)
{
    VkImageBlit2 BlitRegion = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = NULL,
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets = { { 0 }, { (i32)SrcSize.width, (i32)SrcSize.height, 1 } },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets = { { 0 }, { (i32)DstSize.width, (i32)DstSize.height, 1 } },
    };

    const VkBlitImageInfo2 BlitInfo = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = NULL,
        .srcImage = Source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = Destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &BlitRegion,
        .filter = VK_FILTER_NEAREST,
    };

    vkCmdBlitImage2(CommandBuffer, &BlitInfo);
}

static void Rr_BlitColorImage(VkCommandBuffer CommandBuffer, VkImage Source, VkImage Destination, VkExtent2D SrcSize, VkExtent2D DstSize)
{
    VkImageBlit2 BlitRegion = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = NULL,
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets = { { 0 }, { (i32)SrcSize.width, (i32)SrcSize.height, 1 } },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets = { { 0 }, { (i32)DstSize.width, (i32)DstSize.height, 1 } },
    };

    const VkBlitImageInfo2 BlitInfo = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = NULL,
        .srcImage = Source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = Destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &BlitRegion,
        .filter = VK_FILTER_NEAREST,
    };

    vkCmdBlitImage2(CommandBuffer, &BlitInfo);
}

static void CalculateDrawTargetResolution(Rr_Renderer* const Renderer, const u32 WindowWidth, const u32 WindowHeight)
{
    Renderer->ActiveResolution.width = Renderer->ReferenceResolution.width;
    Renderer->ActiveResolution.height = Renderer->ReferenceResolution.height;

    const i32 MaxAvailableScale = SDL_min(WindowWidth / Renderer->ReferenceResolution.width, WindowHeight / Renderer->ReferenceResolution.height);
    if (MaxAvailableScale >= 1)
    {
        Renderer->ActiveResolution.width += (WindowWidth - MaxAvailableScale * Renderer->ReferenceResolution.width) / MaxAvailableScale;
        Renderer->ActiveResolution.height += (WindowHeight - MaxAvailableScale * Renderer->ReferenceResolution.height) / MaxAvailableScale;
    }

    Renderer->Scale = MaxAvailableScale;
}

static bool CheckPhysicalDevice(Rr_App* App, VkPhysicalDevice PhysicalDevice)
{
    Rr_Renderer* Renderer = &App->Renderer;

    u32 ExtensionCount;
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, NULL, &ExtensionCount, NULL);
    if (ExtensionCount == 0)
    {
        return false;
    }

    const char* TargetExtensions[] = {
        "VK_EXT_descriptor_indexing",
        "VK_KHR_swapchain"
    };

    bool FoundExtensions[] = { 0, 0 };

    VkExtensionProperties* Extensions = Rr_StackAlloc(VkExtensionProperties, ExtensionCount);
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, NULL, &ExtensionCount, Extensions);

    for (u32 Index = 0; Index < ExtensionCount; Index++)
    {
        for (u32 TargetIndex = 0; TargetIndex < SDL_arraysize(TargetExtensions); ++TargetIndex)
        {
            if (strcmp(Extensions[Index].extensionName, TargetExtensions[TargetIndex]) == 0)
            {
                FoundExtensions[TargetIndex] = true;
            }
        }
    }
    for (u32 TargetIndex = 0; TargetIndex < SDL_arraysize(TargetExtensions); ++TargetIndex)
    {
        if (!FoundExtensions[TargetIndex])
        {
            return false;
        }
    }

    u32 QueueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, NULL);
    if (QueueFamilyCount == 0)
    {
        return false;
    }

    VkQueueFamilyProperties* QueueFamilyProperties = Rr_StackAlloc(VkQueueFamilyProperties, QueueFamilyCount);
    VkBool32* QueuePresentSupport = Rr_StackAlloc(VkBool32, QueueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilyProperties);

    u32 GraphicsQueueFamilyIndex = ~0U;
    u32 TransferQueueFamilyIndex = ~0U;

    for (u32 Index = 0; Index < QueueFamilyCount; ++Index)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, Index, Renderer->Surface, &QueuePresentSupport[Index]);
        if (QueuePresentSupport[Index]
            && QueueFamilyProperties[Index].queueCount > 0
            && (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            GraphicsQueueFamilyIndex = Index;
            break;
        }
    }

    const u32 bForceUnifiedQueue = RR_FORCE_UNIFIED_QUEUE;

    if (!bForceUnifiedQueue)
    {
        for (u32 Index = 0; Index < QueueFamilyCount; ++Index)
        {
            if (Index == GraphicsQueueFamilyIndex)
            {
                continue;
            }

            if (QueueFamilyProperties[Index].queueCount > 0
                && (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_TRANSFER_BIT))
            {
                TransferQueueFamilyIndex = Index;
                break;
            }
        }
    }

    if (TransferQueueFamilyIndex == ~0U)
    {
        Renderer->bUnifiedQueue = true;
        Renderer->TransferQueue.FamilyIndex = GraphicsQueueFamilyIndex;
    }
    else
    {
        Renderer->bUnifiedQueue = false;
        Renderer->TransferQueue.FamilyIndex = TransferQueueFamilyIndex;
    }

    Renderer->UnifiedQueue.FamilyIndex = GraphicsQueueFamilyIndex;

    Rr_StackFree(QueuePresentSupport);
    Rr_StackFree(QueueFamilyProperties);
    Rr_StackFree(Extensions);

    return GraphicsQueueFamilyIndex != ~0U;
}

static void InitDevice(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    u32 PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(Renderer->Instance, &PhysicalDeviceCount, NULL);
    if (PhysicalDeviceCount == 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No device with Vulkan support found");
        abort();
    }

    VkPhysicalDevice* PhysicalDevices = Rr_StackAlloc(VkPhysicalDevice, PhysicalDeviceCount);
    vkEnumeratePhysicalDevices(Renderer->Instance, &PhysicalDeviceCount, &PhysicalDevices[0]);

    Renderer->PhysicalDevice.SubgroupProperties = (VkPhysicalDeviceSubgroupProperties){ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES, .pNext = NULL };

    Renderer->PhysicalDevice.Properties = (VkPhysicalDeviceProperties2){
        .pNext = &Renderer->PhysicalDevice.SubgroupProperties,
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };

    bool bFoundSuitableDevice = false;
    for (u32 Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        if (CheckPhysicalDevice(App, PhysicalDevices[Index]))
        {
            Renderer->PhysicalDevice.Handle = PhysicalDevices[Index];

            vkGetPhysicalDeviceFeatures(Renderer->PhysicalDevice.Handle, &Renderer->PhysicalDevice.Features);
            if (!Renderer->PhysicalDevice.Features.shaderSampledImageArrayDynamicIndexing)
            {
                continue;
            }
            vkGetPhysicalDeviceMemoryProperties(Renderer->PhysicalDevice.Handle, &Renderer->PhysicalDevice.MemoryProperties);
            vkGetPhysicalDeviceProperties2(Renderer->PhysicalDevice.Handle, &Renderer->PhysicalDevice.Properties);

            SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Selected GPU: %s", Renderer->PhysicalDevice.Properties.properties.deviceName);

            bFoundSuitableDevice = true;
            break;
        }
    }
    if (!bFoundSuitableDevice)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not select physical device based on the chosen properties!");
        abort();
    }

    const float QueuePriorities[] = { 1.0f };
    VkDeviceQueueCreateInfo QueueInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = Renderer->UnifiedQueue.FamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = QueuePriorities,
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = QueuePriorities,
        }
    };

    VkPhysicalDeviceVulkan13Features Features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = VK_NULL_HANDLE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features Features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &Features13,
        /* Descriptor Indexing */
        .descriptorIndexing = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
    };

    const char* DeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    const VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &Features12,
        .queueCreateInfoCount = Renderer->bUnifiedQueue ? 1 : 2,
        .pQueueCreateInfos = QueueInfos,
        .enabledExtensionCount = SDL_arraysize(DeviceExtensions),
        .ppEnabledExtensionNames = DeviceExtensions,
    };

    vkCreateDevice(Renderer->PhysicalDevice.Handle, &DeviceCreateInfo, NULL, &Renderer->Device);

    vkGetDeviceQueue(Renderer->Device, Renderer->UnifiedQueue.FamilyIndex, 0, &Renderer->UnifiedQueue.Handle);
    if (!Renderer->bUnifiedQueue)
    {
        vkGetDeviceQueue(Renderer->Device, Renderer->TransferQueue.FamilyIndex, 0, &Renderer->TransferQueue.Handle);
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Unified Queue Mode: %s", Renderer->bUnifiedQueue ? "true" : "false");

    Renderer->UnifiedQueue.Mutex = SDL_CreateMutex();

    Rr_ArrayInit(Renderer->UnifiedQueue.PendingLoads, Rr_PendingLoad, 1);

    Rr_StackFree(PhysicalDevices);
}

static void InitDrawTarget(Rr_App* App, Rr_DrawTarget* DrawTarget, const u32 Width, const u32 Height)
{
    Rr_Renderer* Renderer = &App->Renderer;

    DrawTarget->ColorImage = Rr_CreateColorAttachmentImage(App, Width, Height);
    DrawTarget->DepthImage = Rr_CreateDepthAttachmentImage(App, Width, Height);

    VkImageView Attachments[2] = { DrawTarget->ColorImage->View, DrawTarget->DepthImage->View };

    const VkFramebufferCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = Renderer->RenderPass,
        .height = Height,
        .width = Width,
        .layers = 1,
        .attachmentCount = 2,
        .pAttachments = Attachments
    };
    vkCreateFramebuffer(Renderer->Device, &Info, NULL, &DrawTarget->Framebuffer);
}

static void CleanupDrawTarget(Rr_App* App, Rr_DrawTarget* DrawTarget)
{
    Rr_Renderer* Renderer = &App->Renderer;
    vkDestroyFramebuffer(Renderer->Device, DrawTarget->Framebuffer, NULL);
    Rr_DestroyImage(App, DrawTarget->ColorImage);
    Rr_DestroyImage(App, DrawTarget->DepthImage);
}

static void CleanupSwapchain(Rr_App* App, VkSwapchainKHR Swapchain)
{
    Rr_Renderer* Renderer = &App->Renderer;

    for (u32 Index = 0; Index < Renderer->Swapchain.ImageCount; Index++)
    {
        vkDestroyImageView(Renderer->Device, Renderer->Swapchain.Images[Index].View, NULL);
    }
    vkDestroySwapchainKHR(Renderer->Device, Swapchain, NULL);
}

static bool InitSwapchain(Rr_App* App, u32* Width, u32* Height)
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
        CleanupSwapchain(App, OldSwapchain);
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

    CalculateDrawTargetResolution(Renderer, *Width, *Height);
    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_DrawTarget* DrawTarget = &Renderer->Frames[Index].DrawTarget;

        if (DrawTarget->ColorImage != NULL)
        {
            CleanupDrawTarget(App, DrawTarget);
        }
        InitDrawTarget(App, DrawTarget, *Width, *Height);

        // if (Renderer->DrawTarget.ActiveResolution.width > Renderer->DrawTarget.ColorImage.Extent.width
        //     || Renderer->DrawTarget.ActiveResolution.height > Renderer->DrawTarget.ColorImage.Extent.height)
        // {
        //     if (Renderer->DrawTarget.ColorImage.Handle != VK_NULL_HANDLE)
        //     {
        //         CleanupDrawTarget(Renderer);
        //     }
        //     InitDrawTarget(Renderer, *Width, *Height);
        // }
    }

    Rr_StackFree(Images);
    Rr_StackFree(SurfaceFormats);

    return true;
}

static void InitFrames(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;
    Rr_Frame* Frames = Renderer->Frames;

    const VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo SemaphoreCreateInfo = GetSemaphoreCreateInfo(0);

    for (i32 Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame* Frame = &Frames[Index];

        Frame->StagingBuffer = Rr_CreateStagingBuffer(Renderer, RR_STAGING_BUFFER_SIZE);

        /* Synchronization */
        vkCreateFence(Device, &FenceCreateInfo, NULL, &Frame->RenderFence);

        vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->SwapchainSemaphore);
        vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->RenderSemaphore);

        Rr_ArrayInit(Frame->RetiredSemaphoresArray, VkSemaphore, 1);

        /* Descriptors */
        Rr_DescriptorPoolSizeRatio Ratios[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
        };

        Frame->DescriptorAllocator = Rr_CreateDescriptorAllocator(Renderer->Device, 1000, Ratios, SDL_arraysize(Ratios));

        /* Commands */
        VkCommandPoolCreateInfo CommandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = Renderer->UnifiedQueue.FamilyIndex,
        };

        vkCreateCommandPool(Renderer->Device, &CommandPoolInfo, NULL, &Frame->CommandPool);

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Frame->CommandPool, 1);

        vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &Frame->MainCommandBuffer);
    }
}

static void CleanupFrames(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    for (u32 Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame* Frame = &Renderer->Frames[Index];
        vkDestroyCommandPool(Renderer->Device, Frame->CommandPool, NULL);

        vkDestroyFence(Device, Frame->RenderFence, NULL);
        vkDestroySemaphore(Device, Frame->RenderSemaphore, NULL);
        vkDestroySemaphore(Device, Frame->SwapchainSemaphore, NULL);

        Rr_DestroyStagingBuffer(Renderer, Frame->StagingBuffer);
        Rr_DestroyDescriptorAllocator(&Frame->DescriptorAllocator, Device);

        CleanupDrawTarget(App, &Frame->DrawTarget);

        Rr_ArrayFree(Frame->RetiredSemaphoresArray);
    }
}

static void InitAllocator(Rr_App* App)
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

static void InitDescriptors(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_DescriptorPoolSizeRatio Ratios[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    Renderer->GlobalDescriptorAllocator = Rr_CreateDescriptorAllocator(Renderer->Device, 10, Ratios, SDL_arraysize(Ratios));
}

static PFN_vkVoidFunction LoadVulkanFunction(const char* FuncName, void* Userdata)
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

    ImGui_ImplVulkan_LoadFunctions(LoadVulkanFunction, NULL);
    ImGui_ImplSDL3_InitForVulkan(Window);

    ImGui_ImplVulkan_InitInfo InitInfo = {
        .Instance = Renderer->Instance,
        .PhysicalDevice = Renderer->PhysicalDevice.Handle,
        .Device = Device,
        .Queue = Renderer->UnifiedQueue.Handle,
        .DescriptorPool = Renderer->ImGui.DescriptorPool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .PipelineRenderingCreateInfo.colorAttachmentCount = 1,
        .PipelineRenderingCreateInfo.pColorAttachmentFormats = &Renderer->Swapchain.Format,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
    };

    ImGui_ImplVulkan_Init(&InitInfo);

    /* Init default font. */
    const f32 WindowScale = SDL_GetWindowDisplayScale(Window);
    ImGuiStyle_ScaleAllSizes(igGetStyle(), WindowScale);

    Rr_ExternAsset(MartianMonoTTF);

    /* Don't transfer asset ownership to ImGui, it will crash otherwise! */
    ImFontConfig* FontConfig = ImFontConfig_ImFontConfig();
    FontConfig->FontDataOwnedByAtlas = false;
    ImFontAtlas_AddFontFromMemoryTTF(IO->Fonts, (void*)MartianMonoTTF.Data, (i32)MartianMonoTTF.Length, SDL_floorf(16.0f * WindowScale), FontConfig, NULL);
    igMemFree(FontConfig);

    ImGui_ImplVulkan_CreateFontsTexture();

    Renderer->ImGui.bInitiated = true;
}

static void InitImmediateMode(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    VkDevice Device = Renderer->Device;
    Rr_ImmediateMode* ImmediateMode = &Renderer->ImmediateMode;

    const VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = Renderer->UnifiedQueue.FamilyIndex,
    };
    vkCreateCommandPool(Device, &CommandPoolInfo, NULL, &ImmediateMode->CommandPool);
    const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(ImmediateMode->CommandPool, 1);
    vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &ImmediateMode->CommandBuffer);
    const VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    vkCreateFence(Device, &FenceCreateInfo, NULL, &ImmediateMode->Fence);
}

static void CleanupImmediateMode(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    vkDestroyCommandPool(Renderer->Device, Renderer->ImmediateMode.CommandPool, NULL);
    vkDestroyFence(Renderer->Device, Renderer->ImmediateMode.Fence, NULL);
}

static void InitGenericPipelineLayout(Rr_App* App)
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

static void InitSamplers(Rr_App* App)
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

static void CleanupSamplers(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    vkDestroySampler(Renderer->Device, Renderer->NearestSampler, NULL);
    vkDestroySampler(Renderer->Device, Renderer->LinearSampler, NULL);
}

static void InitRenderPass(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    const VkAttachmentDescription ColorAttachment = {
        .samples = 1,
        .format = RR_COLOR_FORMAT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .flags = 0,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_NONE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE
    };

    const VkAttachmentDescription DepthAttachment = {
        .samples = 1,
        .format = RR_DEPTH_FORMAT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .flags = 0,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_NONE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE
    };

    VkAttachmentDescription Attachments[2] = { ColorAttachment, DepthAttachment };

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

    const VkRenderPassCreateInfo Info = {
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
}

static void CleanupRenderPass(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    vkDestroyRenderPass(Renderer->Device, Renderer->RenderPass, NULL);
}

static void InitTransientCommandPools(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;

    vkCreateCommandPool(
        Renderer->Device,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = Renderer->UnifiedQueue.FamilyIndex,
        },
        NULL,
        &Renderer->UnifiedQueue.TransientCommandPool);

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

static void CleanupTransientCommandPools(Rr_Renderer* Renderer)
{
    vkDestroyCommandPool(Renderer->Device, Renderer->UnifiedQueue.TransientCommandPool, NULL);
    vkDestroyCommandPool(Renderer->Device, Renderer->TransferQueue.TransientCommandPool, NULL);
}

void Rr_InitRenderer(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    SDL_Window* Window = App->Window;
    const Rr_AppConfig* Config = App->Config;

    Renderer->ReferenceResolution.width = Config->ReferenceResolution[0];
    Renderer->ReferenceResolution.height = Config->ReferenceResolution[1];

    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = SDL_GetWindowTitle(Window),
        .pEngineName = "Rr_Renderer",
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
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

    InitDevice(App);
    InitAllocator(App);

    // volkLoadDevice(Renderer->Device);
    //

    InitTransientCommandPools(App);
    InitSamplers(App);
    InitRenderPass(App);
    InitDescriptors(App);

    u32 Width, Height;
    SDL_GetWindowSizeInPixels(Window, (i32*)&Width, (i32*)&Height);
    InitSwapchain(App, &Width, &Height);

    InitFrames(App);
    InitImmediateMode(App);
    InitGenericPipelineLayout(App);

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

        if (!bMinimized && Width > 0 && Height > 0 && InitSwapchain(App, (u32*)&Width, (u32*)&Height))
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

    CleanupFrames(App);

    for (size_t Index = 0; Index < Rr_ArrayCount(Renderer->UnifiedQueue.PendingLoads); ++Index)
    {
        Rr_PendingLoad* PendingLoad = &Renderer->UnifiedQueue.PendingLoads[Index];

        vkDestroySemaphore(Device, PendingLoad->Semaphore, NULL);
    }
    Rr_ArrayFree(Renderer->UnifiedQueue.PendingLoads);

    SDL_DestroyMutex(Renderer->UnifiedQueue.Mutex);

    CleanupTransientCommandPools(Renderer);
    CleanupImmediateMode(App);
    CleanupRenderPass(App);
    CleanupSamplers(App);
    CleanupSwapchain(App, Renderer->Swapchain.Handle);

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

    vkQueueSubmit(Renderer->UnifiedQueue.Handle, 1, &SubmitInfo, ImmediateMode->Fence);
    vkWaitForFences(Renderer->Device, 1, &ImmediateMode->Fence, true, UINT64_MAX);
}

void Rr_Draw(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;
    Rr_Swapchain* Swapchain = &Renderer->Swapchain;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    Rr_Image* ColorImage = Frame->DrawTarget.ColorImage;
    Rr_Image* DepthImage = Frame->DrawTarget.DepthImage;

    vkWaitForFences(Device, 1, &Frame->RenderFence, true, 1000000000);
    vkResetFences(Device, 1, &Frame->RenderFence);

    Frame->StagingBuffer->CurrentOffset = 0;

    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    Rr_ResetDescriptorAllocator(&Frame->DescriptorAllocator, Device);

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

    VkCommandBufferBeginInfo CommandBufferBeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

    /* Process pending loads to acquire images/buffers.
     * Generate wait semaphore input. */
    SDL_LockMutex(Renderer->UnifiedQueue.Mutex);
    const size_t PendingLoadsCount = Rr_ArrayCount(Renderer->UnifiedQueue.PendingLoads);
    VkSemaphore WaitSemaphores[PendingLoadsCount + 1];
    VkPipelineStageFlags WaitDstStages[PendingLoadsCount + 1];
    WaitSemaphores[0] = Frame->SwapchainSemaphore;
    WaitDstStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    size_t WaitSemaphoreIndex = 1;

    for (size_t Index = 0; Index < Rr_ArrayCount(Frame->RetiredSemaphoresArray); ++Index)
    {
        vkDestroySemaphore(Device, Frame->RetiredSemaphoresArray[Index], NULL);
    }
    Rr_ArrayEmpty(Frame->RetiredSemaphoresArray);

    for (size_t Index = 0; Index < PendingLoadsCount; ++Index)
    {
        Rr_PendingLoad* PendingLoad = &Renderer->UnifiedQueue.PendingLoads[Index];

        PendingLoad->LoadingCallback(App, PendingLoad->Userdata);

        vkCmdPipelineBarrier(
            CommandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0,
            NULL,
            Rr_ArrayCount(PendingLoad->Barriers.BufferMemoryBarriersArray),
            PendingLoad->Barriers.BufferMemoryBarriersArray,
            Rr_ArrayCount(PendingLoad->Barriers.ImageMemoryBarriersArray),
            PendingLoad->Barriers.ImageMemoryBarriersArray);

        Rr_ArrayFree(PendingLoad->Barriers.BufferMemoryBarriersArray);
        Rr_ArrayFree(PendingLoad->Barriers.ImageMemoryBarriersArray);

        WaitSemaphores[WaitSemaphoreIndex] = PendingLoad->Semaphore;
        WaitDstStages[WaitSemaphoreIndex] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        WaitSemaphoreIndex++;

        Rr_ArrayPush(Frame->RetiredSemaphoresArray, &PendingLoad->Semaphore);
    }
    Rr_ArrayEmpty(Renderer->UnifiedQueue.PendingLoads);
    SDL_UnlockMutex(Renderer->UnifiedQueue.Mutex);

    /* Rendering */
    Rr_ImageBarrier ColorImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = ColorImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    Rr_ChainImageBarrier(&ColorImageTransition,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    Rr_ImageBarrier DepthImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = DepthImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .StageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
    };
    Rr_ChainImageBarrier(&DepthImageTransition,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    App->Config->DrawFunc(App);

    // Rr_ImageBarrier ColorImageTransition = {
    //     .CommandBuffer = CommandBuffer,
    //     .Image = ColorImage->Handle,
    //     .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //     .AccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    //     .StageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    // };
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
    //
    // Rr_ChainImageBarrier(&SwapchainImageTransition,
    //     VK_PIPELINE_STAGE_2_CLEAR_BIT,
    //     VK_ACCESS_2_TRANSFER_WRITE_BIT,
    //     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // vkCmdClearColorImage(
    //     CommandBuffer,
    //     SwapchainImage,
    //     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //     &(VkClearColorValue){},
    //     1,
    //     &(VkImageSubresourceRange){
    //         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    //         .baseMipLevel = 0,
    //         .levelCount = 1,
    //         .baseArrayLayer = 0,
    //         .layerCount = 1,
    //     });
    Rr_ChainImageBarrier(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    //
    Rr_BlitColorImage(CommandBuffer, ColorImage->Handle, SwapchainImage, Renderer->ActiveResolution,
        (VkExtent2D){
            .width = (Renderer->ActiveResolution.width) * Renderer->Scale,
            .height = (Renderer->ActiveResolution.height) * Renderer->Scale });

    if (Renderer->ImGui.bInitiated)
    {
        Rr_ChainImageBarrier(&SwapchainImageTransition,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo ColorAttachmentInfo = GetRenderingAttachmentInfo_Color(Swapchain->Images[SwapchainImageIndex].View, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, NULL);
        VkRenderingInfo RenderingInfo = GetRenderingInfo(Swapchain->Extent, &ColorAttachmentInfo, NULL);

        vkCmdBeginRendering(CommandBuffer, &RenderingInfo);

        ImGui_ImplVulkan_RenderDrawData(igGetDrawData(), CommandBuffer, VK_NULL_HANDLE);

        vkCmdEndRendering(CommandBuffer);
    }

    Rr_ChainImageBarrier(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Rr_ChainImageBarrier(&ColorImageTransition,
    //     VK_PIPELINE_STAGE_2_BLIT_BIT,
    //     VK_ACCESS_2_TRANSFER_READ_BIT,
    //     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vkEndCommandBuffer(CommandBuffer);

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

    SDL_LockMutex(Renderer->UnifiedQueue.Mutex);
    vkQueueSubmit(Renderer->UnifiedQueue.Handle, 1, &SubmitInfo, Frame->RenderFence);

    const VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->RenderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    Result = vkQueuePresentKHR(Renderer->UnifiedQueue.Handle, &PresentInfo);
    SDL_UnlockMutex(Renderer->UnifiedQueue.Mutex);
    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 1);
    }

    Renderer->FrameNumber++;
    Renderer->CurrentFrameIndex = Renderer->FrameNumber % RR_FRAME_OVERLAP;
}

Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer)
{
    return &Renderer->Frames[Renderer->CurrentFrameIndex];
}


