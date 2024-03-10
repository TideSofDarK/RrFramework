#include "Rr.h"
#include "RrTypes.h"

#include <math.h>
#include <string.h>
#include <stdint.h>

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

#include "RrVulkan.h"
#include "RrLib.h"
#include "RrDescriptor.h"
#include "RrImage.h"
#include "RrHelpers.h"
#include "RrBuffer.h"
#include "RrMesh.h"
#include "RrPipelineBuilder.h"

static bool Rr_CheckPhysicalDevice(SRr* const Rr, VkPhysicalDevice PhysicalDevice)
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
        vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, Index, Rr->Surface, &QueuePresentSupport[Index]);

        if (!QueuePresentSupport[Index])
        {
            continue;
        }

        if ((QueueFamilyProperties[Index].queueCount > 0) && (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            Rr->GraphicsQueue.FamilyIndex = Index;
            return true;
        }
    }

    SDL_stack_free(QueuePresentSupport);
    SDL_stack_free(QueueFamilyProperties);
    SDL_stack_free(Extensions);

    return false;
}

static void Rr_InitDevice(SRr* const Rr)
{
    u32 PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(Rr->Instance, &PhysicalDeviceCount, NULL);
    if (PhysicalDeviceCount == 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No device with Vulkan support found");
        abort();
    }

    VkPhysicalDevice* PhysicalDevices = SDL_stack_alloc(VkPhysicalDevice, PhysicalDeviceCount);
    vkEnumeratePhysicalDevices(Rr->Instance, &PhysicalDeviceCount, &PhysicalDevices[0]);

    Rr->PhysicalDevice.SubgroupProperties = (VkPhysicalDeviceSubgroupProperties){ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES, .pNext = NULL };

    VkPhysicalDeviceProperties2 PhysicalDeviceProperties = {
        .pNext = &Rr->PhysicalDevice.SubgroupProperties,
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };

    b32 bFoundSuitableDevice = false;
    for (u32 Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        if (Rr_CheckPhysicalDevice(Rr, PhysicalDevices[Index]))
        {
            Rr->PhysicalDevice.Handle = PhysicalDevices[Index];

            vkGetPhysicalDeviceFeatures(Rr->PhysicalDevice.Handle, &Rr->PhysicalDevice.Features);
            vkGetPhysicalDeviceMemoryProperties(Rr->PhysicalDevice.Handle, &Rr->PhysicalDevice.MemoryProperties);
            vkGetPhysicalDeviceProperties2(Rr->PhysicalDevice.Handle, &PhysicalDeviceProperties);

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
        .queueFamilyIndex = Rr->GraphicsQueue.FamilyIndex,
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

    VK_ASSERT(vkCreateDevice(Rr->PhysicalDevice.Handle, &DeviceCreateInfo, NULL, &Rr->Device))

    vkGetDeviceQueue(Rr->Device, Rr->GraphicsQueue.FamilyIndex, 0, &Rr->GraphicsQueue.Handle);

    SDL_stack_free(PhysicalDevices);
}

static void Rr_CreateDrawTarget(SRr* const Rr, u32 Width, u32 Height)
{
    SAllocatedImage* ColorImage = &Rr->DrawTarget.ColorImage;
    SAllocatedImage* DepthImage = &Rr->DrawTarget.DepthImage;

    ColorImage->Extent = DepthImage->Extent = (VkExtent3D){
        Width,
        Height,
        1
    };

    Rr->DrawTarget.ActiveExtent.width = ColorImage->Extent.width;
    Rr->DrawTarget.ActiveExtent.height = ColorImage->Extent.height;

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
    VK_ASSERT(vmaCreateImage(Rr->Allocator, &ImageCreateInfo, &AllocationCreateInfo, &ColorImage->Handle, &ColorImage->Allocation, NULL))
    VkImageViewCreateInfo ImageViewCreateInfo = GetImageViewCreateInfo(ColorImage->Format, ColorImage->Handle, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_ASSERT(vkCreateImageView(Rr->Device, &ImageViewCreateInfo, NULL, &ColorImage->View))

    /* Depth Image */
    DepthImage->Format = RR_DEPTH_FORMAT;
    ImageCreateInfo = GetImageCreateInfo(DepthImage->Format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, DepthImage->Extent);
    VK_ASSERT(vmaCreateImage(Rr->Allocator, &ImageCreateInfo, &AllocationCreateInfo, &DepthImage->Handle, &DepthImage->Allocation, NULL))
    ImageViewCreateInfo = GetImageViewCreateInfo(DepthImage->Format, DepthImage->Handle, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_ASSERT(vkCreateImageView(Rr->Device, &ImageViewCreateInfo, NULL, &DepthImage->View))
}

static void Rr_CleanupDrawTarget(SRr* const Rr)
{
    Rr_DestroyImage(Rr, &Rr->DrawTarget.ColorImage);
    Rr_DestroyImage(Rr, &Rr->DrawTarget.DepthImage);
}

static void Rr_UpdateDrawImageDescriptors(SRr* const Rr, b32 bCreate, b32 bDestroy)
{
    SDescriptorAllocator* GlobalDescriptorAllocator = &Rr->GlobalDescriptorAllocator;

    if (bDestroy)
    {
        vkDestroyDescriptorSetLayout(Rr->Device, Rr->DrawTarget.DescriptorSetLayout, NULL);

        DescriptorAllocator_ClearPools(GlobalDescriptorAllocator, Rr->Device);
    }
    if (bCreate)
    {
        SDescriptorLayoutBuilder Builder = { 0 };
        DescriptorLayoutBuilder_Add(&Builder, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        Rr->DrawTarget.DescriptorSetLayout = DescriptorLayoutBuilder_Build(&Builder, Rr->Device, VK_SHADER_STAGE_COMPUTE_BIT);

        Rr->DrawTarget.DescriptorSet = DescriptorAllocator_Allocate(GlobalDescriptorAllocator, Rr->Device, Rr->DrawTarget.DescriptorSetLayout);

        SDescriptorWriter Writer = { 0 };
        DescriptorWriter_Init(&Writer, 1, 0);
        DescriptorWriter_WriteImage(&Writer, 0, Rr->DrawTarget.ColorImage.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        DescriptorWriter_Update(&Writer, Rr->Device, Rr->DrawTarget.DescriptorSet);
        DescriptorWriter_Cleanup(&Writer);
    }
}

static void Rr_CleanupSwapchain(SRr* const Rr, VkSwapchainKHR Swapchain)
{
    for (u32 Index = 0; Index < Rr->Swapchain.ImageCount; Index++)
    {
        vkDestroyImageView(Rr->Device, Rr->Swapchain.Images[Index].View, NULL);
    }
    vkDestroySwapchainKHR(Rr->Device, Swapchain, NULL);
}

static b32 Rr_CreateSwapchain(SRr* const Rr, u32* Width, u32* Height)
{
    VkSwapchainKHR OldSwapchain = Rr->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfCaps;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Rr->PhysicalDevice.Handle, Rr->Surface, &SurfCaps))

    if (SurfCaps.currentExtent.width == 0 || SurfCaps.currentExtent.height == 0)
    {
        return false;
    }
    if (SurfCaps.currentExtent.width == UINT32_MAX)
    {
        Rr->Swapchain.Extent.width = *Width;
        Rr->Swapchain.Extent.height = *Height;
    }
    else
    {
        Rr->Swapchain.Extent = SurfCaps.currentExtent;
        *Width = SurfCaps.currentExtent.width;
        *Height = SurfCaps.currentExtent.height;
    }

    u32 PresentModeCount;
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Rr->PhysicalDevice.Handle, Rr->Surface, &PresentModeCount, NULL))
    SDL_assert(PresentModeCount > 0);

    VkPresentModeKHR* PresentModes = SDL_stack_alloc(VkPresentModeKHR, PresentModeCount);
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Rr->PhysicalDevice.Handle, Rr->Surface, &PresentModeCount, PresentModes))

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
    VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(Rr->PhysicalDevice.Handle, Rr->Surface, &FormatCount, NULL))
    SDL_assert(FormatCount > 0);

    VkSurfaceFormatKHR* SurfaceFormats = SDL_stack_alloc(VkSurfaceFormatKHR, FormatCount);
    VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(Rr->PhysicalDevice.Handle, Rr->Surface, &FormatCount, SurfaceFormats))

    bool bPreferredFormatFound = false;
    for (u32 Index = 0; Index < FormatCount; Index++)
    {
        VkSurfaceFormatKHR* SurfaceFormat = &SurfaceFormats[Index];

        if (SurfaceFormat->format == VK_FORMAT_B8G8R8A8_UNORM || SurfaceFormat->format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            Rr->Swapchain.Format = SurfaceFormat->format;
            Rr->Swapchain.ColorSpace = SurfaceFormat->colorSpace;
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
        .surface = Rr->Surface,
        .minImageCount = DesiredNumberOfSwapchainImages,
        .imageFormat = Rr->Swapchain.Format,
        .imageColorSpace = Rr->Swapchain.ColorSpace,
        .imageExtent = { Rr->Swapchain.Extent.width, Rr->Swapchain.Extent.height },
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

    VK_ASSERT(vkCreateSwapchainKHR(Rr->Device, &SwapchainCI, NULL, &Rr->Swapchain.Handle))

    if (OldSwapchain != VK_NULL_HANDLE)
    {
        Rr_CleanupSwapchain(Rr, OldSwapchain);
    }

    u32 ImageCount = 0;
    VK_ASSERT(vkGetSwapchainImagesKHR(Rr->Device, Rr->Swapchain.Handle, &ImageCount, NULL))
    SDL_assert(ImageCount <= MAX_SWAPCHAIN_IMAGE_COUNT);

    Rr->Swapchain.ImageCount = ImageCount;
    VkImage* Images = SDL_stack_alloc(VkImage, ImageCount);
    VK_ASSERT(vkGetSwapchainImagesKHR(Rr->Device, Rr->Swapchain.Handle, &ImageCount, Images))

    VkImageViewCreateInfo ColorAttachmentView = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = Rr->Swapchain.Format,
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
        Rr->Swapchain.Images[i].Handle = Images[i];
        ColorAttachmentView.image = Images[i];
        VK_ASSERT(vkCreateImageView(Rr->Device, &ColorAttachmentView, NULL, &Rr->Swapchain.Images[i].View))
    }

    if (*Width > Rr->DrawTarget.ColorImage.Extent.width || *Height > Rr->DrawTarget.ColorImage.Extent.height)
    {
        if (Rr->DrawTarget.ColorImage.Handle != VK_NULL_HANDLE)
        {
            Rr_CleanupDrawTarget(Rr);
            Rr_UpdateDrawImageDescriptors(Rr, false, true);
        }
        Rr_CreateDrawTarget(Rr, *Width, *Height);
        Rr_UpdateDrawImageDescriptors(Rr, true, false);
    }

    SDL_stack_free(Images);
    SDL_stack_free(SurfaceFormats);

    return true;
}

static void Rr_InitFrames(SRr* const Rr)
{
    VkDevice Device = Rr->Device;
    SRrFrame* Frames = Rr->Frames;

    VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo SemaphoreCreateInfo = GetSemaphoreCreateInfo(0);

    for (i32 Index = 0; Index < FRAME_OVERLAP; Index++)
    {
        SRrFrame* Frame = &Frames[Index];

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

        DescriptorAllocator_Init(&Frame->DescriptorAllocator, Rr->Device, 1000, Ratios, SDL_arraysize(Ratios));

        /* Commands */
        VkCommandPoolCreateInfo CommandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = Rr->GraphicsQueue.FamilyIndex,
        };

        VK_ASSERT(vkCreateCommandPool(Rr->Device, &CommandPoolInfo, NULL, &Frame->CommandPool))

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Frame->CommandPool, 1);

        VK_ASSERT(vkAllocateCommandBuffers(Rr->Device, &CommandBufferAllocateInfo, &Frame->MainCommandBuffer))
    }
}

static void Rr_InitAllocator(SRr* const Rr)
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
        .physicalDevice = Rr->PhysicalDevice.Handle,
        .device = Rr->Device,
        .pVulkanFunctions = &VulkanFunctions,
        .instance = Rr->Instance,
    };
    VK_ASSERT(vmaCreateAllocator(&AllocatorInfo, &Rr->Allocator))
}

static void Rr_InitDescriptors(SRr* Renderer)
{
    SDescriptorAllocator* GlobalDescriptorAllocator = &Renderer->GlobalDescriptorAllocator;

    SDescriptorPoolSizeRatio Ratios[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    DescriptorAllocator_Init(GlobalDescriptorAllocator, Renderer->Device, 10, Ratios, SDL_arraysize(Ratios));

    SDescriptorLayoutBuilder Builder = { 0 };
    DescriptorLayoutBuilder_Add(&Builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Renderer->SceneDataLayout = DescriptorLayoutBuilder_Build(&Builder, Renderer->Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}

static void Rr_InitBackgroundPipelines(SRr* const Rr)
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
        .pSetLayouts = &Rr->DrawTarget.DescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };

    VK_ASSERT(vkCreatePipelineLayout(Rr->Device, &ComputeLayout, NULL, &Rr->GradientPipelineLayout))

    VkShaderModule ComputeDrawShader;
    SRrAsset TestCOMP;
    RrAsset_Extern(&TestCOMP, TestCOMP);
    VK_ASSERT(vkCreateShaderModule(Rr->Device, &(VkShaderModuleCreateInfo){
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
        .layout = Rr->GradientPipelineLayout,
    };

    VK_ASSERT(vkCreateComputePipelines(Rr->Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, NULL, &Rr->GradientPipeline))
    vkDestroyShaderModule(Rr->Device, ComputeDrawShader, NULL);
}

static void Rr_InitMeshPipeline(SRr* const Rr)
{
    VkShaderModule VertModule;
    SRrAsset TriangleVERT;
    RrAsset_Extern(&TriangleVERT, TriangleVERT);
    VK_ASSERT(vkCreateShaderModule(Rr->Device, &(VkShaderModuleCreateInfo){
                                                   .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                   .pNext = NULL,
                                                   .codeSize = TriangleVERT.Length,
                                                   .pCode = (u32*)TriangleVERT.Data,
                                               },
        NULL, &VertModule))

    VkShaderModule FragModule;
    SRrAsset TriangleFRAG;
    RrAsset_Extern(&TriangleFRAG, TriangleFRAG);
    VK_ASSERT(vkCreateShaderModule(Rr->Device, &(VkShaderModuleCreateInfo){
                                                   .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                   .pNext = NULL,
                                                   .codeSize = TriangleFRAG.Length,
                                                   .pCode = (u32*)TriangleFRAG.Data,
                                               },
        NULL, &FragModule))

    VkPushConstantRange PushConstantRange = {
        .offset = 0,
        .size = sizeof(SRrPushConstants3D),
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    VkPipelineLayoutCreateInfo LayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = 1,
        .pSetLayouts = &Rr->SceneDataLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };
    vkCreatePipelineLayout(Rr->Device, &LayoutInfo, NULL, &Rr->MeshPipelineLayout);

    SPipelineBuilder Builder;
    PipelineBuilder_Default(&Builder, VertModule, FragModule, Rr->DrawTarget.ColorImage.Format, Rr->DrawTarget.DepthImage.Format, Rr->MeshPipelineLayout);
    // PipelineBuilder_AlphaBlend(&Builder);
    PipelineBuilder_Depth(&Builder);
    Rr->MeshPipeline = Rr_BuildPipeline(Rr, &Builder);

    vkDestroyShaderModule(Rr->Device, VertModule, NULL);
    vkDestroyShaderModule(Rr->Device, FragModule, NULL);
}

static void Rr_InitPipelines(SRr* const Rr)
{
    Rr_InitBackgroundPipelines(Rr);
    Rr_InitMeshPipeline(Rr);
}

static void Rr_DrawBackground(SRr* const Rr, VkCommandBuffer CommandBuffer)
{
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Rr->GradientPipeline);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Rr->GradientPipelineLayout, 0, 1, &Rr->DrawTarget.DescriptorSet, 0, NULL);
    SComputeConstants ComputeConstants;
    glm_vec4_copy((vec4){ 1.0f, 0.0f, 0.0f, 1.0f }, ComputeConstants.Vec0);
    glm_vec4_copy((vec4){ 0.0f, 1.0f, 0.0f, 1.0f }, ComputeConstants.Vec1);
    vkCmdPushConstants(CommandBuffer, Rr->GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SComputeConstants), &ComputeConstants);
    vkCmdDispatch(CommandBuffer, ceil(Rr->DrawTarget.ActiveExtent.width / 16.0), ceil(Rr->DrawTarget.ActiveExtent.height / 16.0), 1);

    // float Flash = fabsf(sinf((float)Renderer->FrameNumber / 240.0f));
    // VkClearColorValue ClearValue = { { 0.0f, 0.0f, Flash, 1.0f } };
    //
    // VkImageSubresourceRange ClearRange = GetImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    //
    // vkCmdClearColorImage(CommandBuffer, Renderer->DrawImage.Handle, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &ClearRange);
}

static void Rr_DrawGeometry(SRr* const Rr, VkCommandBuffer CommandBuffer, VkDescriptorSet SceneDataDescriptorSet)
{
    VkRenderingAttachmentInfo ColorAttachment = GetRenderingAttachmentInfo_Color(Rr->DrawTarget.ColorImage.View, NULL, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo DepthAttachment = GetRenderingAttachmentInfo_Depth(Rr->DrawTarget.DepthImage.View, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = GetRenderingInfo(Rr->DrawTarget.ActiveExtent, &ColorAttachment, &DepthAttachment);
    vkCmdBeginRendering(CommandBuffer, &renderInfo);

    VkViewport viewport = { 0 };
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)Rr->DrawTarget.ActiveExtent.width;
    viewport.height = (float)Rr->DrawTarget.ActiveExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(CommandBuffer, 0, 1, &viewport);

    VkRect2D scissor = { 0 };
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = Rr->DrawTarget.ActiveExtent.width;
    scissor.extent.height = Rr->DrawTarget.ActiveExtent.height;

    vkCmdSetScissor(CommandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Rr->MeshPipeline);
    SRrPushConstants3D PushConstants = {
        .VertexBufferAddress = Rr->Mesh.VertexBufferAddress,
    };

    u64 Ticks = SDL_GetTicks();
    float Time = (float)((double)Ticks / 1000.0);
    float X = SDL_cosf(Time) * 5;
    float Z = SDL_sinf(Time) * 5;
    mat4 View;
    glm_lookat((vec3){ Z, 0.2f, X }, (vec3){ 0, 0.0f, 0 }, (vec3){ 0, 1, 0 }, View);
    mat4 Projection;
    glm_perspective_rh_no(glm_rad(45.0f), (float)Rr->DrawTarget.ActiveExtent.width / (float)Rr->DrawTarget.ActiveExtent.height, 1.0f, 1000.0f, Projection);
    Projection[1][1] *= -1.0f;
    glm_mat4_mul(Projection, View, PushConstants.ViewProjection);

    vkCmdPushConstants(CommandBuffer, Rr->MeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SRrPushConstants3D), &PushConstants);
    vkCmdBindIndexBuffer(CommandBuffer, Rr->Mesh.IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Rr->MeshPipelineLayout, 0, 1, &SceneDataDescriptorSet, 0, NULL);
    vkCmdDrawIndexed(CommandBuffer, RrArray_Count(Rr->RawMesh.Indices), 1, 0, 0, 0);

    vkCmdEndRendering(CommandBuffer);
}

static PFN_vkVoidFunction Rr_ImGui_LoadFunction(const char* FuncName, void* Userdata)
{
    return (PFN_vkVoidFunction)vkGetInstanceProcAddr(volkGetLoadedInstance(), FuncName);
}

void Rr_InitImmediateMode(SRr* const Rr)
{
    VkDevice Device = Rr->Device;
    SImmediateMode* ImmediateMode = &Rr->ImmediateMode;

    VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = Rr->GraphicsQueue.FamilyIndex,
    };
    VK_ASSERT(vkCreateCommandPool(Device, &CommandPoolInfo, NULL, &ImmediateMode->CommandPool))
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(ImmediateMode->CommandPool, 1);
    VK_ASSERT(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &ImmediateMode->CommandBuffer))
    VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_ASSERT(vkCreateFence(Device, &FenceCreateInfo, NULL, &ImmediateMode->Fence))
}

void Rr_InitImGui(SRr* const Rr, struct SDL_Window* Window)
{
    VkDevice Device = Rr->Device;

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

    VK_ASSERT(vkCreateDescriptorPool(Device, &PoolCreateInfo, NULL, &Rr->ImGui.DescriptorPool))

    igCreateContext(NULL);
    ImGuiIO* IO = igGetIO();
    IO->IniFilename = NULL;

    ImGui_ImplVulkan_LoadFunctions(Rr_ImGui_LoadFunction, NULL);
    ImGui_ImplSDL3_InitForVulkan(Window);

    ImGui_ImplVulkan_InitInfo InitInfo = {
        .Instance = Rr->Instance,
        .PhysicalDevice = Rr->PhysicalDevice.Handle,
        .Device = Device,
        .Queue = Rr->GraphicsQueue.Handle,
        .DescriptorPool = Rr->ImGui.DescriptorPool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .PipelineRenderingCreateInfo.colorAttachmentCount = 1,
        .PipelineRenderingCreateInfo.pColorAttachmentFormats = &Rr->Swapchain.Format,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
    };

    ImGui_ImplVulkan_Init(&InitInfo);

    /* Init default font. */
    f32 WindowScale = SDL_GetWindowDisplayScale(Window);
    ImGuiStyle_ScaleAllSizes(igGetStyle(), WindowScale);

    SRrAsset MartianMonoTTF;
    RrAsset_Extern(&MartianMonoTTF, MartianMonoTTF);

    /* Don't transfer asset ownership to ImGui, it will crash otherwise! */
    ImFontConfig* FontConfig = ImFontConfig_ImFontConfig();
    FontConfig->FontDataOwnedByAtlas = false;
    ImFontAtlas_AddFontFromMemoryTTF(IO->Fonts, (void*)MartianMonoTTF.Data, (i32)MartianMonoTTF.Length, SDL_floorf(16.0f * WindowScale), FontConfig, NULL);
    igMemFree(FontConfig);

    ImGui_ImplVulkan_CreateFontsTexture();

    Rr->ImGui.bInit = true;
}

void Rr_Init(SRr* const Rr, struct SDL_Window* Window)
{
    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = SDL_GetWindowTitle(Window),
        .pEngineName = "Rr",
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

    VK_ASSERT(vkCreateInstance(&VKInstInfo, NULL, &Rr->Instance))

    volkLoadInstance(Rr->Instance);

    if (SDL_Vulkan_CreateSurface(Window, Rr->Instance, NULL, &Rr->Surface) != SDL_TRUE)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create Vulkan surface: %s", SDL_GetError());
        abort();
    }

    Rr_InitDevice(Rr);

    Rr_InitAllocator(Rr);

    // volkLoadDevice(Renderer->Device);

    Rr_InitDescriptors(Rr);

    u32 Width, Height;
    SDL_GetWindowSizeInPixels(Window, (i32*)&Width, (i32*)&Height);
    Rr_CreateSwapchain(Rr, &Width, &Height);

    Rr_InitFrames(Rr);

    Rr_InitPipelines(Rr);

    Rr_InitImmediateMode(Rr);

    SRrAsset NoisePNG;
    RrAsset_Extern(&NoisePNG, NoisePNG);
    Rr->NoiseImage = Rr_LoadImageRGBA8(&NoisePNG, Rr, VK_IMAGE_USAGE_SAMPLED_BIT, false);

    SDL_stack_free(Extensions);
}

void Rr_Cleanup(SRr* const Rr)
{
    VkDevice Device = Rr->Device;

    vkDeviceWaitIdle(Rr->Device);

    Rr_CleanupMesh(Rr, &Rr->Mesh);
    Rr_DestroyImage(Rr, &Rr->NoiseImage);

    if (Rr->ImGui.bInit)
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        igDestroyContext(NULL);
        vkDestroyDescriptorPool(Device, Rr->ImGui.DescriptorPool, NULL);
    }

    vkDestroyCommandPool(Rr->Device, Rr->ImmediateMode.CommandPool, NULL);
    vkDestroyFence(Device, Rr->ImmediateMode.Fence, NULL);

    vkDestroyPipelineLayout(Device, Rr->MeshPipelineLayout, NULL);
    vkDestroyPipeline(Device, Rr->MeshPipeline, NULL);

    vkDestroyPipelineLayout(Device, Rr->GradientPipelineLayout, NULL);
    vkDestroyPipeline(Device, Rr->GradientPipeline, NULL);

    vkDestroyDescriptorSetLayout(Rr->Device, Rr->SceneDataLayout, NULL);

    Rr_UpdateDrawImageDescriptors(Rr, false, true);

    DescriptorAllocator_Cleanup(&Rr->GlobalDescriptorAllocator, Device);

    for (u32 Index = 0; Index < FRAME_OVERLAP; ++Index)
    {
        SRrFrame* Frame = &Rr->Frames[Index];
        vkDestroyCommandPool(Rr->Device, Frame->CommandPool, NULL);

        vkDestroyFence(Device, Frame->RenderFence, NULL);
        vkDestroySemaphore(Device, Frame->RenderSemaphore, NULL);
        vkDestroySemaphore(Device, Frame->SwapchainSemaphore, NULL);

        DescriptorAllocator_Cleanup(&Frame->DescriptorAllocator, Device);
        AllocatedBuffer_Cleanup(&Frame->SceneDataBuffer, Rr->Allocator);
    }

    Rr_CleanupDrawTarget(Rr);

    Rr_CleanupSwapchain(Rr, Rr->Swapchain.Handle);

    vmaDestroyAllocator(Rr->Allocator);

    vkDestroySurfaceKHR(Rr->Instance, Rr->Surface, NULL);
    vkDestroyDevice(Rr->Device, NULL);

    vkDestroyInstance(Rr->Instance, NULL);
}

void Rr_Draw(SRr* const Rr)
{
    VkDevice Device = Rr->Device;
    SSwapchain* Swapchain = &Rr->Swapchain;
    SAllocatedImage* ColorImage = &Rr->DrawTarget.ColorImage;
    SAllocatedImage* DepthImage = &Rr->DrawTarget.DepthImage;

    SRrFrame* Frame = Rr_GetCurrentFrame(Rr);
    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    VK_ASSERT(vkWaitForFences(Device, 1, &Frame->RenderFence, true, 1000000000))
    VK_ASSERT(vkResetFences(Device, 1, &Frame->RenderFence))

    DescriptorAllocator_ClearPools(&Frame->DescriptorAllocator, Device);
    AllocatedBuffer_Cleanup(&Frame->SceneDataBuffer, Rr->Allocator);

    Rr->DrawTarget.ActiveExtent.width = Swapchain->Extent.width;
    Rr->DrawTarget.ActiveExtent.height = Swapchain->Extent.height;

    u32 SwapchainImageIndex;
    VkResult Result = vkAcquireNextImageKHR(Device, Swapchain->Handle, 1000000000, Frame->SwapchainSemaphore, VK_NULL_HANDLE, &SwapchainImageIndex);
    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_AtomicSet(&Rr->Swapchain.bResizePending, 1);
        return;
    }
    if (Result == VK_SUBOPTIMAL_KHR)
    {
        SDL_AtomicSet(&Rr->Swapchain.bResizePending, 1);
    }
    SDL_assert(Result >= 0);

    AllocatedBuffer_Init(&Frame->SceneDataBuffer, Rr->Allocator, sizeof(SRrSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, true);
    SRrSceneData* SceneData = (SRrSceneData*)Frame->SceneDataBuffer.AllocationInfo.pMappedData;
    glm_vec4_copy((vec4){ 1.0f, 1.0f, 1.0f, 0.5f }, Rr->SceneData.AmbientColor);
    *SceneData = Rr->SceneData;
    VkDescriptorSet SceneDataDescriptorSet = DescriptorAllocator_Allocate(&Frame->DescriptorAllocator, Rr->Device, Rr->SceneDataLayout);
    SDescriptorWriter Writer = { 0 };
    DescriptorWriter_Init(&Writer, 0, 1);
    DescriptorWriter_WriteBuffer(&Writer, 0, Frame->SceneDataBuffer.Handle, sizeof(SRrSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    DescriptorWriter_Update(&Writer, Device, SceneDataDescriptorSet);
    DescriptorWriter_Cleanup(&Writer);

    VkImage SwapchainImage = Swapchain->Images[SwapchainImageIndex].Handle;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_ASSERT(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo))

    STransitionImage ColorImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = ColorImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_2_NONE,
        .StageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT
    };
    TransitionImage_To(&ColorImageTransition,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
    Rr_DrawBackground(Rr, CommandBuffer);
    TransitionImage_To(&ColorImageTransition,
                       VK_PIPELINE_STAGE_2_BLIT_BIT,
                       VK_ACCESS_2_TRANSFER_WRITE_BIT,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyImageToImage(CommandBuffer, Rr->NoiseImage.Handle, ColorImage->Handle, GetExtent2D(Rr->NoiseImage.Extent), GetExtent2D(ColorImage->Extent));
    TransitionImage_To(&ColorImageTransition,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    STransitionImage DepthImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = DepthImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .StageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
    };
    TransitionImage_To(&DepthImageTransition,
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    Rr_DrawGeometry(Rr, CommandBuffer, SceneDataDescriptorSet);
    TransitionImage_To(&ColorImageTransition,
        VK_PIPELINE_STAGE_2_BLIT_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    STransitionImage SwapchainImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = SwapchainImage,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_2_NONE,
        .StageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    TransitionImage_To(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_BLIT_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    CopyImageToImage(CommandBuffer, ColorImage->Handle, SwapchainImage, Rr->DrawTarget.ActiveExtent, Swapchain->Extent);
    TransitionImage_To(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    if (Rr->ImGui.bInit)
    {
        VkRenderingAttachmentInfo ColorAttachment = GetRenderingAttachmentInfo_Color(Swapchain->Images[SwapchainImageIndex].View, NULL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderInfo = GetRenderingInfo(Swapchain->Extent, &ColorAttachment, NULL);

        vkCmdBeginRendering(CommandBuffer, &renderInfo);

        ImGui_ImplVulkan_RenderDrawData(igGetDrawData(), CommandBuffer, VK_NULL_HANDLE);

        vkCmdEndRendering(CommandBuffer);
    }

    TransitionImage_To(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_ASSERT(vkEndCommandBuffer(CommandBuffer))

    VkCommandBufferSubmitInfo CommandBufferSubmitInfo = GetCommandBufferSubmitInfo(CommandBuffer);

    VkSemaphoreSubmitInfo WaitSemaphoreSubmitInfo = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, Frame->SwapchainSemaphore);
    VkSemaphoreSubmitInfo SignalSemaphoreSubmitInfo = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, Frame->RenderSemaphore);

    VkSubmitInfo2 SubmitInfo = GetSubmitInfo(&CommandBufferSubmitInfo, &SignalSemaphoreSubmitInfo, &WaitSemaphoreSubmitInfo);

    VK_ASSERT(vkQueueSubmit2(Rr->GraphicsQueue.Handle, 1, &SubmitInfo, Frame->RenderFence))

    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->RenderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    Result = vkQueuePresentKHR(Rr->GraphicsQueue.Handle, &PresentInfo);
    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_AtomicSet(&Rr->Swapchain.bResizePending, 1);
    }

    Rr->FrameNumber++;
}

b8 Rr_NewFrame(SRr* const Rr, SDL_Window* Window)
{
    i32 bResizePending = SDL_AtomicGet(&Rr->Swapchain.bResizePending);
    if (bResizePending == true)
    {
        vkDeviceWaitIdle(Rr->Device);

        i32 Width, Height;
        SDL_GetWindowSizeInPixels(Window, &Width, &Height);

        b8 bMinimized = SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED;

        if (!bMinimized && Width > 0 && Height > 0 && Rr_CreateSwapchain(Rr, (u32*)&Width, (u32*)&Height))
        {
            SDL_AtomicSet(&Rr->Swapchain.bResizePending, 0);
            return true;
        }

        return false;
    }
    return true;
}

void Rr_SetMesh(SRr* const Rr, SRrRawMesh* const RawMesh)
{
    Rr->RawMesh = *RawMesh;
    Rr_UploadMesh(Rr, &Rr->Mesh, RawMesh->Indices, RrArray_Count(RawMesh->Indices), RawMesh->Vertices, RrArray_Count(RawMesh->Vertices));
}

VkCommandBuffer Rr_BeginImmediate(SRr* const Rr)
{
    SImmediateMode* ImmediateMode = &Rr->ImmediateMode;
    VK_ASSERT(vkResetFences(Rr->Device, 1, &ImmediateMode->Fence))
    VK_ASSERT(vkResetCommandBuffer(ImmediateMode->CommandBuffer, 0))

    VkCommandBufferBeginInfo BeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_ASSERT(vkBeginCommandBuffer(ImmediateMode->CommandBuffer, &BeginInfo))

    return ImmediateMode->CommandBuffer;
}

void Rr_EndImmediate(SRr* const Rr)
{
    SImmediateMode* ImmediateMode = &Rr->ImmediateMode;

    VK_ASSERT(vkEndCommandBuffer(ImmediateMode->CommandBuffer))

    VkCommandBufferSubmitInfo CommandBufferSubmitInfo = GetCommandBufferSubmitInfo(ImmediateMode->CommandBuffer);
    VkSubmitInfo2 SubmitInfo = GetSubmitInfo(&CommandBufferSubmitInfo, NULL, NULL);

    VK_ASSERT(vkQueueSubmit2(Rr->GraphicsQueue.Handle, 1, &SubmitInfo, ImmediateMode->Fence))
    VK_ASSERT(vkWaitForFences(Rr->Device, 1, &ImmediateMode->Fence, true, UINT64_MAX))
}

SRrFrame* Rr_GetCurrentFrame(SRr* const Rr)
{
    return &Rr->Frames[Rr->FrameNumber % FRAME_OVERLAP];
}

VkPipeline Rr_BuildPipeline(SRr* const Rr, SPipelineBuilder const* const PipelineBuilder)
{
    VkPipelineViewportStateCreateInfo ViewportInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &PipelineBuilder->ColorBlendAttachment,
    };

    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
    };

    VkDynamicState DynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .pDynamicStates = DynamicState,
        .dynamicStateCount = SDL_arraysize(DynamicState)
    };

    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &PipelineBuilder->RenderInfo,
        .stageCount = PipelineBuilder->ShaderStageCount,
        .pStages = PipelineBuilder->ShaderStages,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &PipelineBuilder->InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &PipelineBuilder->Rasterizer,
        .pMultisampleState = &PipelineBuilder->Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &PipelineBuilder->DepthStencil,
        .layout = PipelineBuilder->PipelineLayout,
        .pDynamicState = &DynamicStateInfo
    };

    VkPipeline Pipeline;
    VK_ASSERT(vkCreateGraphicsPipelines(Rr->Device, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &Pipeline))

    return Pipeline;
}
