#include "RrRenderer.h"
#include "RrDefines.h"

#include <math.h>
#include <string.h>
#include <stdint.h>

#include <cglm/ivec2.h>
#include <cglm/mat4.h>
#include <cglm/cam.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_vulkan.h>

#include "RrApp.h"
#include "RrTypes.h"
#include "RrVulkan.h"
#include "RrLib.h"
#include "RrDescriptor.h"
#include "RrImage.h"
#include "RrHelpers.h"
#include "RrBuffer.h"
#include "RrMesh.h"
#include "RrPipelineBuilder.h"
#include "RrPipeline.h"

static void CalculateDrawTargetResolution(Rr_DrawTarget* const DrawTarget, u32 WindowWidth, u32 WindowHeight)
{
    DrawTarget->ActiveResolution.width = DrawTarget->ReferenceResolution.width;
    DrawTarget->ActiveResolution.height = DrawTarget->ReferenceResolution.height;

    const i32 MaxAvailableScale = SDL_min(WindowWidth / DrawTarget->ReferenceResolution.width, WindowHeight / DrawTarget->ReferenceResolution.height);
    if (MaxAvailableScale >= 1)
    {
        DrawTarget->ActiveResolution.width += (WindowWidth - MaxAvailableScale * DrawTarget->ReferenceResolution.width) / MaxAvailableScale;
        DrawTarget->ActiveResolution.height += (WindowHeight - MaxAvailableScale * DrawTarget->ReferenceResolution.height) / MaxAvailableScale;

        //        DrawTarget->ActiveResolution.width++;
        //        DrawTarget->ActiveResolution.height++;
    }

    DrawTarget->Scale = MaxAvailableScale;
}

static bool Rr_CheckPhysicalDevice(Rr_Renderer* const Renderer, VkPhysicalDevice PhysicalDevice)
{
    u32 ExtensionCount;
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, NULL, &ExtensionCount, NULL);
    if (ExtensionCount == 0)
    {
        return false;
    }

    VkExtensionProperties* Extensions = SDL_stack_alloc(VkExtensionProperties, ExtensionCount);
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, NULL, &ExtensionCount, Extensions);

    bool bSwapchainFound = false;
    for (u32 Index = 0; Index < ExtensionCount; Index++)
    {
        if (strcmp(Extensions[Index].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            bSwapchainFound = true;
            break;
        }
    }
    if (!bSwapchainFound)
    {
        return false;
    }

    u32 QueueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, NULL);
    if (QueueFamilyCount == 0)
    {
        return false;
    }

    VkQueueFamilyProperties* QueueFamilyProperties = SDL_stack_alloc(VkQueueFamilyProperties, QueueFamilyCount);
    VkBool32* QueuePresentSupport = SDL_stack_alloc(VkBool32, QueueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilyProperties);

    for (u32 Index = 0; Index < QueueFamilyCount; ++Index)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, Index, Renderer->Surface, &QueuePresentSupport[Index]);

        if (!QueuePresentSupport[Index])
        {
            continue;
        }

        if ((QueueFamilyProperties[Index].queueCount > 0) && (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            Renderer->GraphicsQueue.FamilyIndex = Index;
            return true;
        }
    }

    SDL_stack_free(QueuePresentSupport);
    SDL_stack_free(QueueFamilyProperties);
    SDL_stack_free(Extensions);

    return false;
}

static void Rr_InitDevice(Rr_Renderer* const Renderer)
{
    u32 PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(Renderer->Instance, &PhysicalDeviceCount, NULL);
    if (PhysicalDeviceCount == 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No device with Vulkan support found");
        abort();
    }

    VkPhysicalDevice* PhysicalDevices = SDL_stack_alloc(VkPhysicalDevice, PhysicalDeviceCount);
    vkEnumeratePhysicalDevices(Renderer->Instance, &PhysicalDeviceCount, &PhysicalDevices[0]);

    Renderer->PhysicalDevice.SubgroupProperties = (VkPhysicalDeviceSubgroupProperties){ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES, .pNext = NULL };

    VkPhysicalDeviceProperties2 PhysicalDeviceProperties = {
        .pNext = &Renderer->PhysicalDevice.SubgroupProperties,
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };

    b32 bFoundSuitableDevice = false;
    for (u32 Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        if (Rr_CheckPhysicalDevice(Renderer, PhysicalDevices[Index]))
        {
            Renderer->PhysicalDevice.Handle = PhysicalDevices[Index];

            vkGetPhysicalDeviceFeatures(Renderer->PhysicalDevice.Handle, &Renderer->PhysicalDevice.Features);
            vkGetPhysicalDeviceMemoryProperties(Renderer->PhysicalDevice.Handle, &Renderer->PhysicalDevice.MemoryProperties);
            vkGetPhysicalDeviceProperties2(Renderer->PhysicalDevice.Handle, &PhysicalDeviceProperties);

            SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Selected GPU: %s", PhysicalDeviceProperties.properties.deviceName);

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
    VkDeviceQueueCreateInfo QueueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = QueuePriorities,
    };

    VkPhysicalDeviceVulkan13Features Features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = VK_NULL_HANDLE,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features Features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &Features13,
        .descriptorIndexing = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };

    const char* DeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &Features12,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &QueueInfo,
        .enabledExtensionCount = SDL_arraysize(DeviceExtensions),
        .ppEnabledExtensionNames = DeviceExtensions,
    };

    VK_ASSERT(vkCreateDevice(Renderer->PhysicalDevice.Handle, &DeviceCreateInfo, NULL, &Renderer->Device))

    vkGetDeviceQueue(Renderer->Device, Renderer->GraphicsQueue.FamilyIndex, 0, &Renderer->GraphicsQueue.Handle);

    SDL_stack_free(PhysicalDevices);
}

static void Rr_CreateDrawTarget(Rr_Renderer* const Renderer, u32 Width, u32 Height)
{
    Rr_Image* ColorImage = &Renderer->DrawTarget.ColorImage;
    Rr_Image* DepthImage = &Renderer->DrawTarget.DepthImage;

    ColorImage->Extent = DepthImage->Extent = (VkExtent3D){
        Width,
        Height,
        1
    };

    VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    /* Color Image */
    ColorImage->Format = RR_COLOR_FORMAT;
    VkImageUsageFlags DrawImageUsages = 0;
    DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    DrawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    DrawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkImageCreateInfo ImageCreateInfo = GetImageCreateInfo(ColorImage->Format, DrawImageUsages, ColorImage->Extent);
    VK_ASSERT(vmaCreateImage(Renderer->Allocator, &ImageCreateInfo, &AllocationCreateInfo, &ColorImage->Handle, &ColorImage->Allocation, NULL))
    VkImageViewCreateInfo ImageViewCreateInfo = GetImageViewCreateInfo(ColorImage->Format, ColorImage->Handle, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_ASSERT(vkCreateImageView(Renderer->Device, &ImageViewCreateInfo, NULL, &ColorImage->View))

    /* Depth Image */
    DepthImage->Format = RR_DEPTH_FORMAT;
    ImageCreateInfo = GetImageCreateInfo(DepthImage->Format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, DepthImage->Extent);
    VK_ASSERT(vmaCreateImage(Renderer->Allocator, &ImageCreateInfo, &AllocationCreateInfo, &DepthImage->Handle, &DepthImage->Allocation, NULL))
    ImageViewCreateInfo = GetImageViewCreateInfo(DepthImage->Format, DepthImage->Handle, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_ASSERT(vkCreateImageView(Renderer->Device, &ImageViewCreateInfo, NULL, &DepthImage->View))
}

static void Rr_CleanupDrawTarget(Rr_Renderer* const Renderer)
{
    Rr_DestroyImage(Renderer, &Renderer->DrawTarget.ColorImage);
    Rr_DestroyImage(Renderer, &Renderer->DrawTarget.DepthImage);
}

static void Rr_UpdateDrawImageDescriptors(Rr_Renderer* const Renderer, b32 bCreate, b32 bDestroy)
{
    SDescriptorAllocator* GlobalDescriptorAllocator = &Renderer->GlobalDescriptorAllocator;

    if (bDestroy)
    {
        vkDestroyDescriptorSetLayout(Renderer->Device, Renderer->DrawTarget.DescriptorSetLayout, NULL);

        Rr_ResetDescriptorAllocator(GlobalDescriptorAllocator, Renderer->Device);
    }
    if (bCreate)
    {
        Rr_DescriptorLayoutBuilder Builder = { 0 };
        Rr_AddDescriptor(&Builder, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        Renderer->DrawTarget.DescriptorSetLayout = Rr_BuildDescriptorLayout(&Builder, Renderer->Device, VK_SHADER_STAGE_COMPUTE_BIT);

        Renderer->DrawTarget.DescriptorSet = Rr_AllocateDescriptorSet(GlobalDescriptorAllocator, Renderer->Device, Renderer->DrawTarget.DescriptorSetLayout);

        SDescriptorWriter Writer = { 0 };
        Rr_InitDescriptorWriter(&Writer, 1, 0);
        Rr_WriteDescriptor_Image(&Writer, 0, Renderer->DrawTarget.ColorImage.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        Rr_UpdateDescriptorSet(&Writer, Renderer->Device, Renderer->DrawTarget.DescriptorSet);
        Rr_DestroyDescriptorWriter(&Writer);
    }
}

static void Rr_CleanupSwapchain(Rr_Renderer* const Renderer, VkSwapchainKHR Swapchain)
{
    for (u32 Index = 0; Index < Renderer->Swapchain.ImageCount; Index++)
    {
        vkDestroyImageView(Renderer->Device, Renderer->Swapchain.Images[Index].View, NULL);
    }
    vkDestroySwapchainKHR(Renderer->Device, Swapchain, NULL);
}

static b32 Rr_CreateSwapchain(Rr_Renderer* const Renderer, u32* Width, u32* Height)
{
    VkSwapchainKHR OldSwapchain = Renderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfCaps;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &SurfCaps))

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
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &PresentModeCount, NULL))
    SDL_assert(PresentModeCount > 0);

    VkPresentModeKHR* PresentModes = SDL_stack_alloc(VkPresentModeKHR, PresentModeCount);
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &PresentModeCount, PresentModes))

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
    SDL_stack_free(PresentModes);

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
    VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &FormatCount, NULL))
    SDL_assert(FormatCount > 0);

    VkSurfaceFormatKHR* SurfaceFormats = SDL_stack_alloc(VkSurfaceFormatKHR, FormatCount);
    VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &FormatCount, SurfaceFormats))

    bool bPreferredFormatFound = false;
    for (u32 Index = 0; Index < FormatCount; Index++)
    {
        VkSurfaceFormatKHR* SurfaceFormat = &SurfaceFormats[Index];

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
    VkCompositeAlphaFlagBitsKHR CompositeAlphaFlags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (u32 Index = 0; Index < SDL_arraysize(CompositeAlphaFlags); Index++)
    {
        VkCompositeAlphaFlagBitsKHR CompositeAlphaFlag = CompositeAlphaFlags[Index];
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

    VK_ASSERT(vkCreateSwapchainKHR(Renderer->Device, &SwapchainCI, NULL, &Renderer->Swapchain.Handle))

    if (OldSwapchain != VK_NULL_HANDLE)
    {
        Rr_CleanupSwapchain(Renderer, OldSwapchain);
    }

    u32 ImageCount = 0;
    VK_ASSERT(vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, NULL))
    SDL_assert(ImageCount <= MAX_SWAPCHAIN_IMAGE_COUNT);

    Renderer->Swapchain.ImageCount = ImageCount;
    VkImage* Images = SDL_stack_alloc(VkImage, ImageCount);
    VK_ASSERT(vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, Images))

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
        VK_ASSERT(vkCreateImageView(Renderer->Device, &ColorAttachmentView, NULL, &Renderer->Swapchain.Images[i].View))
    }

    CalculateDrawTargetResolution(&Renderer->DrawTarget, *Width, *Height);

    if (Renderer->DrawTarget.ActiveResolution.width > Renderer->DrawTarget.ColorImage.Extent.width
        || Renderer->DrawTarget.ActiveResolution.height > Renderer->DrawTarget.ColorImage.Extent.height)
    {
        if (Renderer->DrawTarget.ColorImage.Handle != VK_NULL_HANDLE)
        {
            Rr_CleanupDrawTarget(Renderer);
            Rr_UpdateDrawImageDescriptors(Renderer, false, true);
        }
        Rr_CreateDrawTarget(Renderer, *Width, *Height);
        Rr_UpdateDrawImageDescriptors(Renderer, true, false);
    }

    SDL_stack_free(Images);
    SDL_stack_free(SurfaceFormats);

    return true;
}

static void Rr_InitFrames(Rr_Renderer* const Renderer)
{
    VkDevice Device = Renderer->Device;
    Rr_Frame* Frames = Renderer->Frames;

    VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo SemaphoreCreateInfo = GetSemaphoreCreateInfo(0);

    for (i32 Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame* Frame = &Frames[Index];

        /* Synchronization */
        VK_ASSERT(vkCreateFence(Device, &FenceCreateInfo, NULL, &Frame->RenderFence))

        VK_ASSERT(vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->SwapchainSemaphore))
        VK_ASSERT(vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frame->RenderSemaphore))

        /* Descriptors */
        SDescriptorPoolSizeRatio Ratios[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        Frame->DescriptorAllocator = Rr_CreateDescriptorAllocator(Renderer->Device, 1000, Ratios, SDL_arraysize(Ratios));

        /* Commands */
        VkCommandPoolCreateInfo CommandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        };

        VK_ASSERT(vkCreateCommandPool(Renderer->Device, &CommandPoolInfo, NULL, &Frame->CommandPool))

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Frame->CommandPool, 1);

        VK_ASSERT(vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &Frame->MainCommandBuffer))
    }
}

static void Rr_InitAllocator(Rr_Renderer* const Renderer)
{
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
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = Renderer->PhysicalDevice.Handle,
        .device = Renderer->Device,
        .pVulkanFunctions = &VulkanFunctions,
        .instance = Renderer->Instance,
    };
    VK_ASSERT(vmaCreateAllocator(&AllocatorInfo, &Renderer->Allocator))
}

static void Rr_InitDescriptors(Rr_Renderer* const Renderer)
{
    SDescriptorPoolSizeRatio Ratios[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    Renderer->GlobalDescriptorAllocator = Rr_CreateDescriptorAllocator(Renderer->Device, 10, Ratios, SDL_arraysize(Ratios));
}

static void Rr_InitBackgroundPipelines(Rr_Renderer* const Renderer)
{
    VkPushConstantRange PushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(SComputeConstants),
    };

    VkPipelineLayoutCreateInfo ComputeLayout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = 1,
        .pSetLayouts = &Renderer->DrawTarget.DescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };

    VK_ASSERT(vkCreatePipelineLayout(Renderer->Device, &ComputeLayout, NULL, &Renderer->GradientPipelineLayout))

    VkShaderModule ComputeDrawShader;
    Rr_Asset TestCOMP;
    RrAsset_Extern(&TestCOMP, TestCOMP);
    VK_ASSERT(vkCreateShaderModule(Renderer->Device, &(VkShaderModuleCreateInfo){
                                                         .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                         .pNext = NULL,
                                                         .codeSize = TestCOMP.Length,
                                                         .pCode = (u32*)TestCOMP.Data,
                                                     },
        NULL, &ComputeDrawShader))

    VkPipelineShaderStageCreateInfo StageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = ComputeDrawShader,
        .pName = "main",
    };

    VkComputePipelineCreateInfo PipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .stage = StageCreateInfo,
        .layout = Renderer->GradientPipelineLayout,
    };

    VK_ASSERT(vkCreateComputePipelines(Renderer->Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, NULL, &Renderer->GradientPipeline))
    vkDestroyShaderModule(Renderer->Device, ComputeDrawShader, NULL);
}

static void Rr_InitPipelines(Rr_Renderer* const Renderer)
{
    Rr_InitBackgroundPipelines(Renderer);
}

static void Rr_DrawBackground(Rr_Renderer* const Renderer, VkCommandBuffer CommandBuffer)
{
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Renderer->GradientPipeline);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Renderer->GradientPipelineLayout, 0, 1, &Renderer->DrawTarget.DescriptorSet, 0, NULL);
    SComputeConstants ComputeConstants;
    glm_vec4_copy((vec4){ 1.0f, 0.0f, 0.0f, 1.0f }, ComputeConstants.Vec0);
    glm_vec4_copy((vec4){ 0.0f, 1.0f, 0.0f, 1.0f }, ComputeConstants.Vec1);
    glm_vec4_copy((vec4){ (f32)Renderer->DrawTarget.ActiveResolution.width, (f32)Renderer->DrawTarget.ActiveResolution.height, 0.0f, 1.0f }, ComputeConstants.Vec2);
    vkCmdPushConstants(CommandBuffer, Renderer->GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SComputeConstants), &ComputeConstants);
    vkCmdDispatch(CommandBuffer, ceil(Renderer->DrawTarget.ActiveResolution.width / 16.0), ceil(Renderer->DrawTarget.ActiveResolution.height / 16.0), 1);

    // float Flash = fabsf(sinf((float)Renderer->FrameNumber / 240.0f));
    // VkClearColorValue ClearValue = { { 0.0f, 0.0f, Flash, 1.0f } };
    //
    // VkImageSubresourceRange ClearRange = GetImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    //
    // vkCmdClearColorImage(CommandBuffer, Renderer->DrawImage.Handle, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &ClearRange);
}

static void Rr_DrawGeometry(Rr_Renderer* const Renderer, VkCommandBuffer CommandBuffer, VkDescriptorSet SceneDataDescriptorSet)
{
    // VkRenderingAttachmentInfo ColorAttachment = GetRenderingAttachmentInfo_Color(Renderer->DrawTarget.ColorImage.View, NULL, VK_IMAGE_LAYOUT_GENERAL);
    // VkRenderingAttachmentInfo DepthAttachment = GetRenderingAttachmentInfo_Depth(Renderer->DrawTarget.DepthImage.View, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    //
    // VkRenderingInfo renderInfo = GetRenderingInfo(Renderer->DrawTarget.ActiveResolution, &ColorAttachment, &DepthAttachment);
    // vkCmdBeginRendering(CommandBuffer, &renderInfo);
    //
    // VkViewport viewport = { 0 };
    // viewport.x = 0.0f;
    // viewport.y = 0.0f;
    // viewport.width = (float)Renderer->DrawTarget.ActiveResolution.width;
    // viewport.height = (float)Renderer->DrawTarget.ActiveResolution.height;
    // viewport.minDepth = 0.0f;
    // viewport.maxDepth = 1.0f;
    //
    // vkCmdSetViewport(CommandBuffer, 0, 1, &viewport);
    //
    // VkRect2D scissor = { 0 };
    // scissor.offset.x = 0;
    // scissor.offset.y = 0;
    // scissor.extent.width = Renderer->DrawTarget.ActiveResolution.width;
    // scissor.extent.height = Renderer->DrawTarget.ActiveResolution.height;
    //
    // vkCmdSetScissor(CommandBuffer, 0, 1, &scissor);
    //
    // vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Renderer->MeshPipeline);
    // Rr_PushConstants3D PushConstants = {
    //     .VertexBufferAddress = Renderer->Mesh.VertexBufferAddress,
    // };
    //
    // u64 Ticks = SDL_GetTicks();
    // float Time = (float)((double)Ticks / 1000.0);
    // float X = SDL_cosf(Time) * 5;
    // float Z = SDL_sinf(Time) * 5;
    // mat4 View;
    // glm_lookat((vec3){ Z, 0.2f, X }, (vec3){ 0, 0.0f, 0 }, (vec3){ 0, 1, 0 }, View);
    // mat4 Projection;
    // glm_perspective_rh_no(glm_rad(45.0f), (float)Renderer->DrawTarget.ActiveResolution.width / (float)Renderer->DrawTarget.ActiveResolution.height, 1.0f, 1000.0f, Projection);
    // Projection[1][1] *= -1.0f;
    // glm_mat4_mul(Projection, View, PushConstants.ViewProjection);
    //
    // vkCmdPushConstants(CommandBuffer, Renderer->MeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Rr_PushConstants3D), &PushConstants);
    // vkCmdBindIndexBuffer(CommandBuffer, Renderer->Mesh.IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT32);
    // vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Renderer->MeshPipelineLayout, 0, 1, &SceneDataDescriptorSet, 0, NULL);
    // vkCmdDrawIndexed(CommandBuffer, Rr_ArrayCount(Renderer->RawMesh.Indices), 1, 0, 0, 0);
    //
    // vkCmdEndRendering(CommandBuffer);
}

static PFN_vkVoidFunction Rr_ImGui_LoadFunction(const char* FuncName, void* Userdata)
{
    return (PFN_vkVoidFunction)vkGetInstanceProcAddr(volkGetLoadedInstance(), FuncName);
}

void Rr_InitImmediateMode(Rr_Renderer* const Renderer)
{
    VkDevice Device = Renderer->Device;
    Rr_ImmediateMode* ImmediateMode = &Renderer->ImmediateMode;

    VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
    };
    VK_ASSERT(vkCreateCommandPool(Device, &CommandPoolInfo, NULL, &ImmediateMode->CommandPool))
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(ImmediateMode->CommandPool, 1);
    VK_ASSERT(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &ImmediateMode->CommandBuffer))
    VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_ASSERT(vkCreateFence(Device, &FenceCreateInfo, NULL, &ImmediateMode->Fence))
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

    VkDescriptorPoolCreateInfo PoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = (u32)SDL_arraysize(PoolSizes),
        .pPoolSizes = PoolSizes,
    };

    VK_ASSERT(vkCreateDescriptorPool(Device, &PoolCreateInfo, NULL, &Renderer->ImGui.DescriptorPool))

    igCreateContext(NULL);
    ImGuiIO* IO = igGetIO();
    IO->IniFilename = NULL;

    ImGui_ImplVulkan_LoadFunctions(Rr_ImGui_LoadFunction, NULL);
    ImGui_ImplSDL3_InitForVulkan(Window);

    ImGui_ImplVulkan_InitInfo InitInfo = {
        .Instance = Renderer->Instance,
        .PhysicalDevice = Renderer->PhysicalDevice.Handle,
        .Device = Device,
        .Queue = Renderer->GraphicsQueue.Handle,
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
    f32 WindowScale = SDL_GetWindowDisplayScale(Window);
    ImGuiStyle_ScaleAllSizes(igGetStyle(), WindowScale);

    Rr_Asset MartianMonoTTF;
    RrAsset_Extern(&MartianMonoTTF, MartianMonoTTF);

    /* Don't transfer asset ownership to ImGui, it will crash otherwise! */
    ImFontConfig* FontConfig = ImFontConfig_ImFontConfig();
    FontConfig->FontDataOwnedByAtlas = false;
    ImFontAtlas_AddFontFromMemoryTTF(IO->Fonts, (void*)MartianMonoTTF.Data, (i32)MartianMonoTTF.Length, SDL_floorf(16.0f * WindowScale), FontConfig, NULL);
    igMemFree(FontConfig);

    ImGui_ImplVulkan_CreateFontsTexture();

    Renderer->ImGui.bInit = true;
}

void Rr_Init(Rr_App* App)
{
    SDL_Window* Window = App->Window;
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_AppConfig* Config = App->Config;

    Renderer->PerFrameDataSize = Config->PerFrameDataSize;
    Renderer->PerFrameDatas = SDL_calloc(RR_FRAME_OVERLAP, Config->PerFrameDataSize);

    Renderer->DrawTarget.ReferenceResolution.width = Config->ReferenceResolution[0];
    Renderer->DrawTarget.ReferenceResolution.height = Config->ReferenceResolution[1];

    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = SDL_GetWindowTitle(Window),
        .pEngineName = "Renderer",
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
    };

    /* Gather required extensions. */
    const char* AppExtensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    u32 AppExtensionCount = SDL_arraysize(AppExtensions);
    AppExtensionCount = 0; /* Use Vulkan Configurator! */

    u32 SDLExtensionCount;
    const char* const* SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&SDLExtensionCount);

    u32 ExtensionCount = SDLExtensionCount + AppExtensionCount;
    const char** Extensions = SDL_stack_alloc(const char*, ExtensionCount);
    for (u32 Index = 0; Index < ExtensionCount; Index++)
    {
        Extensions[Index] = SDLExtensions[Index];
    }
    for (u32 Index = 0; Index < AppExtensionCount; Index++)
    {
        Extensions[Index + SDLExtensionCount] = AppExtensions[Index];
    }

    /* Create Vulkan instance. */
    VkInstanceCreateInfo VKInstInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &VKAppInfo,
        .enabledExtensionCount = ExtensionCount,
        .ppEnabledExtensionNames = Extensions,
    };

    VK_ASSERT(vkCreateInstance(&VKInstInfo, NULL, &Renderer->Instance))

    volkLoadInstance(Renderer->Instance);

    if (SDL_Vulkan_CreateSurface(Window, Renderer->Instance, NULL, &Renderer->Surface) != SDL_TRUE)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create Vulkan surface: %s", SDL_GetError());
        abort();
    }

    Rr_InitDevice(Renderer);

    Rr_InitAllocator(Renderer);

    // volkLoadDevice(Renderer->Device);

    Rr_InitDescriptors(Renderer);

    u32 Width, Height;
    SDL_GetWindowSizeInPixels(Window, (i32*)&Width, (i32*)&Height);
    Rr_CreateSwapchain(Renderer, &Width, &Height);

    Rr_InitFrames(Renderer);

    // Rr_InitPipelines(Renderer);

    Rr_InitImmediateMode(Renderer);

    Rr_Asset NoisePNG;
    RrAsset_Extern(&NoisePNG, NoisePNG);
    Renderer->NoiseImage = Rr_LoadPNG(&NoisePNG, Renderer, VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    SDL_stack_free(Extensions);
}

void Rr_Cleanup(Rr_App* const App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    vkDeviceWaitIdle(Renderer->Device);

    App->Config->CleanupFunc(App);

    Rr_CleanupMesh(Renderer, &Renderer->Mesh);
    Rr_DestroyImage(Renderer, &Renderer->NoiseImage);

    if (Renderer->ImGui.bInit)
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        igDestroyContext(NULL);
        vkDestroyDescriptorPool(Device, Renderer->ImGui.DescriptorPool, NULL);
    }

    vkDestroyCommandPool(Renderer->Device, Renderer->ImmediateMode.CommandPool, NULL);
    vkDestroyFence(Device, Renderer->ImmediateMode.Fence, NULL);

    vkDestroyPipelineLayout(Device, Renderer->GradientPipelineLayout, NULL);
    vkDestroyPipeline(Device, Renderer->GradientPipeline, NULL);

    Rr_UpdateDrawImageDescriptors(Renderer, false, true);

    Rr_DestroyDescriptorAllocator(&Renderer->GlobalDescriptorAllocator, Device);

    for (u32 Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame* Frame = &Renderer->Frames[Index];
        vkDestroyCommandPool(Renderer->Device, Frame->CommandPool, NULL);

        vkDestroyFence(Device, Frame->RenderFence, NULL);
        vkDestroySemaphore(Device, Frame->RenderSemaphore, NULL);
        vkDestroySemaphore(Device, Frame->SwapchainSemaphore, NULL);

        Rr_DestroyDescriptorAllocator(&Frame->DescriptorAllocator, Device);
    }

    Rr_CleanupDrawTarget(Renderer);

    Rr_CleanupSwapchain(Renderer, Renderer->Swapchain.Handle);

    vmaDestroyAllocator(Renderer->Allocator);

    vkDestroySurfaceKHR(Renderer->Instance, Renderer->Surface, NULL);
    vkDestroyDevice(Renderer->Device, NULL);

    vkDestroyInstance(Renderer->Instance, NULL);
}

void Rr_Draw(Rr_App* const App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;
    Rr_Swapchain* Swapchain = &Renderer->Swapchain;
    Rr_Image* ColorImage = &Renderer->DrawTarget.ColorImage;
    Rr_Image* DepthImage = &Renderer->DrawTarget.DepthImage;

    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    VK_ASSERT(vkWaitForFences(Device, 1, &Frame->RenderFence, true, 1000000000))
    VK_ASSERT(vkResetFences(Device, 1, &Frame->RenderFence))

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
    VK_ASSERT(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo))

    App->Config->DrawFunc(App);

    // Rr_ImageBarrier ColorImageTransition = {
    //     .CommandBuffer = CommandBuffer,
    //     .Image = ColorImage->Handle,
    //     .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
    //     .AccessMask = VK_ACCESS_2_NONE,
    //     .StageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT
    // };
    // Rr_ChainImageBarrier(&ColorImageTransition,
    //     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    //     VK_ACCESS_2_MEMORY_WRITE_BIT,
    //     VK_IMAGE_LAYOUT_GENERAL);
    // Rr_DrawBackground(Renderer, CommandBuffer);
    // Rr_ChainImageBarrier(&ColorImageTransition,
    //     VK_PIPELINE_STAGE_2_BLIT_BIT,
    //     VK_ACCESS_2_TRANSFER_WRITE_BIT,
    //     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // // CopyImageToImage(CommandBuffer, Renderer->NoiseImage.Handle, ColorImage->Handle, GetExtent2D(Renderer->NoiseImage.Extent), Renderer->DrawTarget.ActiveResolution);
    // CopyImageToImage(CommandBuffer, Renderer->NoiseImage.Handle, ColorImage->Handle, GetExtent2D(Renderer->NoiseImage.Extent), GetExtent2D(Renderer->NoiseImage.Extent));
    // Rr_ChainImageBarrier(&ColorImageTransition,
    //     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    //     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
    //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    //
    // Rr_ImageBarrier DepthImageTransition = {
    //     .CommandBuffer = CommandBuffer,
    //     .Image = DepthImage->Handle,
    //     .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
    //     .AccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    //     .StageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
    // };
    // Rr_ChainImageBarrier(&DepthImageTransition,
    //     VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    //     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    //     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    // Rr_DrawGeometry(Renderer, CommandBuffer, SceneDataDescriptorSet);
    // Rr_ChainImageBarrier(&ColorImageTransition,
    //     VK_PIPELINE_STAGE_2_BLIT_BIT,
    //     VK_ACCESS_2_TRANSFER_READ_BIT,
    //     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    //
    Rr_ImageBarrier SwapchainImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = SwapchainImage,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_2_NONE,
        .StageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    Rr_ChainImageBarrier(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_CLEAR_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkClearColorValue ClearColorValue = {
        0
    };
    VkImageSubresourceRange ImageSubresourceRange;
    ImageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageSubresourceRange.baseMipLevel = 0;
    ImageSubresourceRange.levelCount = 1;
    ImageSubresourceRange.baseArrayLayer = 0;
    ImageSubresourceRange.layerCount = 1;
    vkCmdClearColorImage(CommandBuffer, SwapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColorValue, 1, &ImageSubresourceRange);

    // CopyImageToImage(CommandBuffer, ColorImage->Handle, SwapchainImage, Renderer->DrawTarget.ActiveResolution,
    //     (VkExtent2D){
    //         .width = (Renderer->DrawTarget.ActiveResolution.width) * Renderer->DrawTarget.Scale,
    //         .height = (Renderer->DrawTarget.ActiveResolution.height) * Renderer->DrawTarget.Scale });
    //    CopyImageToImage(CommandBuffer, ColorImage->Handle, SwapchainImage, Renderer->DrawTarget.ActiveResolution,
    //                     (VkExtent2D){
    //                         .width = Renderer->DrawTarget.ActiveResolution.width,
    //                         .height = Renderer->DrawTarget.ActiveResolution.height});
    Rr_ChainImageBarrier(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    if (Renderer->ImGui.bInit)
    {
        VkRenderingAttachmentInfo ColorAttachmentInfo = GetRenderingAttachmentInfo_Color(Swapchain->Images[SwapchainImageIndex].View, NULL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo RenderingInfo = GetRenderingInfo(Swapchain->Extent, &ColorAttachmentInfo, NULL);

        vkCmdBeginRendering(CommandBuffer, &RenderingInfo);

        ImGui_ImplVulkan_RenderDrawData(igGetDrawData(), CommandBuffer, VK_NULL_HANDLE);

        vkCmdEndRendering(CommandBuffer);
    }

    Rr_ChainImageBarrier(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(CommandBuffer);

    VkCommandBufferSubmitInfo CommandBufferSubmitInfo = GetCommandBufferSubmitInfo(CommandBuffer);

    VkSemaphoreSubmitInfo WaitSemaphoreSubmitInfo = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, Frame->SwapchainSemaphore);
    VkSemaphoreSubmitInfo SignalSemaphoreSubmitInfo = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, Frame->RenderSemaphore);

    VkSubmitInfo2 SubmitInfo = GetSubmitInfo(&CommandBufferSubmitInfo, &SignalSemaphoreSubmitInfo, &WaitSemaphoreSubmitInfo);

    vkQueueSubmit2(Renderer->GraphicsQueue.Handle, 1, &SubmitInfo, Frame->RenderFence);

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
    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 1);
    }

    Renderer->FrameNumber++;
}

b8 Rr_NewFrame(Rr_Renderer* const Renderer, SDL_Window* Window)
{
    i32 bResizePending = SDL_AtomicGet(&Renderer->Swapchain.bResizePending);
    if (bResizePending == true)
    {
        vkDeviceWaitIdle(Renderer->Device);

        i32 Width, Height;
        SDL_GetWindowSizeInPixels(Window, &Width, &Height);

        b8 bMinimized = SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED;

        if (!bMinimized && Width > 0 && Height > 0 && Rr_CreateSwapchain(Renderer, (u32*)&Width, (u32*)&Height))
        {
            SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 0);
            return true;
        }

        return false;
    }
    return true;
}

void Rr_SetMesh(Rr_Renderer* const Renderer, Rr_RawMesh* const RawMesh)
{
    Renderer->RawMesh = *RawMesh;
    Rr_UploadMesh(Renderer, &Renderer->Mesh, RawMesh->Indices, Rr_ArrayCount(RawMesh->Indices), RawMesh->Vertices, Rr_ArrayCount(RawMesh->Vertices));
}

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* const Renderer)
{
    Rr_ImmediateMode* ImmediateMode = &Renderer->ImmediateMode;
    VK_ASSERT(vkResetFences(Renderer->Device, 1, &ImmediateMode->Fence))
    VK_ASSERT(vkResetCommandBuffer(ImmediateMode->CommandBuffer, 0))

    VkCommandBufferBeginInfo BeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_ASSERT(vkBeginCommandBuffer(ImmediateMode->CommandBuffer, &BeginInfo))

    return ImmediateMode->CommandBuffer;
}

void Rr_EndImmediate(Rr_Renderer* const Renderer)
{
    Rr_ImmediateMode* ImmediateMode = &Renderer->ImmediateMode;

    VK_ASSERT(vkEndCommandBuffer(ImmediateMode->CommandBuffer))

    VkCommandBufferSubmitInfo CommandBufferSubmitInfo = GetCommandBufferSubmitInfo(ImmediateMode->CommandBuffer);
    VkSubmitInfo2 SubmitInfo = GetSubmitInfo(&CommandBufferSubmitInfo, NULL, NULL);

    VK_ASSERT(vkQueueSubmit2(Renderer->GraphicsQueue.Handle, 1, &SubmitInfo, ImmediateMode->Fence))
    VK_ASSERT(vkWaitForFences(Renderer->Device, 1, &ImmediateMode->Fence, true, UINT64_MAX))
}

Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* const Renderer)
{
    return &Renderer->Frames[Renderer->FrameNumber % RR_FRAME_OVERLAP];
}

void* Rr_GetCurrentFrameData(Rr_Renderer* Renderer)
{
    return Renderer->PerFrameDatas + (Renderer->FrameNumber % RR_FRAME_OVERLAP) * Renderer->PerFrameDataSize;
}
