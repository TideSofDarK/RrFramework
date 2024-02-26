#define VOLK_IMPLEMENTATION

#include "Rr.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#include <cglm/cglm.h>

#include <vulkan/vk_enum_string_helper.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <SDL3/SDL_vulkan.h>

#include "RrLib.h"

#ifdef RR_DEBUG
static const b32 bEnableValidationLayers = true;
#else
static const b32 bEnableValidationLayers = false;
#endif

static bool Rr_CheckPhysicalDevice(SRr* Rr, VkPhysicalDevice PhysicalDevice)
{
    u32 ExtensionCount;
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, NULL, &ExtensionCount, NULL);
    if (ExtensionCount == 0)
    {
        return false;
    }

    VkExtensionProperties* Extensions = StackAlloc(VkExtensionProperties, ExtensionCount);
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

    VkQueueFamilyProperties* QueueFamilyProperties = StackAlloc(VkQueueFamilyProperties, QueueFamilyCount);
    VkBool32* QueuePresentSupport = StackAlloc(VkBool32, QueueFamilyCount);

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

    return false;
}

static void Rr_InitDevice(SRr* Rr)
{
    u32 PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(Rr->Instance, &PhysicalDeviceCount, NULL);
    if (PhysicalDeviceCount == 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No device with Vulkan support found");
        abort();
    }

    VkPhysicalDevice* PhysicalDevices = StackAlloc(VkPhysicalDevice, PhysicalDeviceCount);
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

    // VkPhysicalDeviceFeatures2 Features2 = {
    //     .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    //     .pNext = &float16_int8
    // };
    // vkGetPhysicalDeviceFeatures2(Renderer->PhysicalDevice.Handle, &Features2);

    const char* SwapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &Features12,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &QueueInfo,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &SwapchainExtension,
    };

    VK_ASSERT(vkCreateDevice(Rr->PhysicalDevice.Handle, &DeviceCreateInfo, NULL, &Rr->Device))

    vkGetDeviceQueue(Rr->Device, Rr->GraphicsQueue.FamilyIndex, 0, &Rr->GraphicsQueue.Handle);
}

static void Rr_CreateDrawImage(SRr* Rr, u32 Width, u32 Height)
{
    SAllocatedImage* DrawImage = &Rr->DrawImage;

    DrawImage->Extent = (VkExtent3D){
        Width,
        Height,
        1
    };
    DrawImage->Format = VK_FORMAT_R16G16B16A16_SFLOAT;

    VkImageUsageFlags DrawImageUsages = 0;
    DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    DrawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    DrawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo ImageCreateInfo = GetImageCreateInfo(DrawImage->Format, DrawImageUsages, DrawImage->Extent);

    VmaAllocationCreateInfo AllocationCreateInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VK_ASSERT(vmaCreateImage(Rr->Allocator, &ImageCreateInfo, &AllocationCreateInfo, &DrawImage->Handle, &DrawImage->Allocation, NULL))

    VkImageViewCreateInfo ImageViewCreateInfo = GetImageViewCreateInfo(DrawImage->Format, DrawImage->Handle, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_ASSERT(vkCreateImageView(Rr->Device, &ImageViewCreateInfo, NULL, &DrawImage->View))
}

static void Rr_CleanupSwapchain(SRr* Rr, VkSwapchainKHR Swapchain)
{
    for (u32 Index = 0; Index < Rr->Swapchain.ImageCount; Index++)
    {
        vkDestroyImageView(Rr->Device, Rr->Swapchain.Images[Index].View, NULL);
    }
    vkDestroySwapchainKHR(Rr->Device, Swapchain, NULL);

    Rr_DestroyAllocatedImage(Rr, &Rr->DrawImage);
}

static void Rr_CreateSwapchain(SRr* Rr, u32* Width, u32* Height, bool bVSync)
{
    VkSwapchainKHR OldSwapchain = Rr->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfCaps;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Rr->PhysicalDevice.Handle, Rr->Surface, &SurfCaps))

    u32 PresentModeCount;
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Rr->PhysicalDevice.Handle, Rr->Surface, &PresentModeCount, NULL))
    assert(PresentModeCount > 0);

    VkPresentModeKHR* PresentModes = StackAlloc(VkPresentModeKHR, PresentModeCount);
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Rr->PhysicalDevice.Handle, Rr->Surface, &PresentModeCount, PresentModes))

    VkPresentModeKHR SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    if (!bVSync)
    {
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
    }

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

    VkExtent2D SwapchainExtent;
    if (SurfCaps.currentExtent.width == UINT32_MAX)
    {
        SwapchainExtent.width = *Width;
        SwapchainExtent.height = *Height;
    }
    else
    {
        SwapchainExtent = SurfCaps.currentExtent;
        *Width = SurfCaps.currentExtent.width;
        *Height = SurfCaps.currentExtent.height;
    }

    Rr->Swapchain.Extent = SwapchainExtent;

    u32 FormatCount;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(Rr->PhysicalDevice.Handle, Rr->Surface, &FormatCount, NULL))
    assert(FormatCount > 0);

    VkSurfaceFormatKHR* SurfaceFormats = StackAlloc(VkSurfaceFormatKHR, FormatCount);
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
    for (u32 Index = 0; Index < ArraySize(CompositeAlphaFlags); Index++)
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
        .imageExtent = { SwapchainExtent.width, SwapchainExtent.height },
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
    assert(ImageCount <= MAX_SWAPCHAIN_IMAGE_COUNT);

    Rr->Swapchain.ImageCount = ImageCount;
    VkImage* Images = StackAlloc(VkImage, ImageCount);
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

    Rr_CreateDrawImage(Rr, *Width, *Height);
}

static VkBool32 VKAPI_PTR DebugMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT MessageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* Userdata)
{
    if (pCallbackData != NULL)
    {
        const char* format = "{0x%x}% s\n %s ";
        SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, format,
            pCallbackData->messageIdNumber,
            pCallbackData->pMessageIdName,
            pCallbackData->pMessage);
    }
    return VK_FALSE;
}

static void VKAPI_CALL Rr_InitDebugMessenger(SRr* Rr)
{
    VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCI = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = DebugMessage
    };

    VK_ASSERT(vkCreateDebugUtilsMessengerEXT(Rr->Instance, &DebugUtilsMessengerCI, NULL, &Rr->Messenger))
}

static void Rr_InitCommands(SRr* Rr)
{
    VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = Rr->GraphicsQueue.FamilyIndex,
    };

    for (u32 Index = 0; Index < FRAME_OVERLAP; ++Index)
    {
        SFrameData* Frame = &Rr->Frames[Index];

        VK_ASSERT(vkCreateCommandPool(Rr->Device, &CommandPoolInfo, NULL, &Frame->CommandPool))

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Frame->CommandPool, 1);

        VK_ASSERT(vkAllocateCommandBuffers(Rr->Device, &CommandBufferAllocateInfo, &Frame->MainCommandBuffer))
    }
}

static void Rr_InitSyncStructures(SRr* Rr)
{
    VkDevice Device = Rr->Device;
    SFrameData* Frames = Rr->Frames;

    VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo SemaphoreCreateInfo = GetSemaphoreCreateInfo(0);

    for (i32 Index = 0; Index < FRAME_OVERLAP; Index++)
    {
        VK_ASSERT(vkCreateFence(Device, &FenceCreateInfo, NULL, &Frames[Index].RenderFence))

        VK_ASSERT(vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frames[Index].SwapchainSemaphore))
        VK_ASSERT(vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &Frames[Index].RenderSemaphore))
    }
}

static void Rr_InitAllocator(SRr* Rr)
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

static void Rr_UpdateDrawImageDescriptors(SRr* Rr, b32 bCreate, b32 bDestroy)
{
    SDescriptorAllocator* GlobalDescriptorAllocator = &Rr->GlobalDescriptorAllocator;

    if (bDestroy)
    {
        vkDestroyDescriptorSetLayout(Rr->Device, Rr->DrawImageDescriptorLayout, NULL);

        DescriptorAllocator_ClearDescriptors(GlobalDescriptorAllocator, Rr->Device);
    }
    if (bCreate)
    {
        SDescriptorLayoutBuilder Builder = { 0 };
        DescriptorLayoutBuilder_Add(&Builder, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        Rr->DrawImageDescriptorLayout = DescriptorLayoutBuilder_Build(&Builder, Rr->Device, VK_SHADER_STAGE_COMPUTE_BIT);

        Rr->DrawImageDescriptors = DescriptorAllocator_Allocate(GlobalDescriptorAllocator, Rr->Device, Rr->DrawImageDescriptorLayout);

        VkDescriptorImageInfo DescriptorImageInfo = {
            .imageView = Rr->DrawImage.View,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkWriteDescriptorSet WriteDescriptorSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = Rr->DrawImageDescriptors,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &DescriptorImageInfo,
        };

        vkUpdateDescriptorSets(Rr->Device, 1, &WriteDescriptorSet, 0, NULL);
    }
}

static void Rr_InitDescriptors(SRr* Renderer)
{
    SDescriptorAllocator* GlobalDescriptorAllocator = &Renderer->GlobalDescriptorAllocator;

    SDescriptorPoolSizeRatio sizes[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    DescriptorAllocator_Init(GlobalDescriptorAllocator, Renderer->Device, 10, sizes, 1);

    /* */

    Rr_UpdateDrawImageDescriptors(Renderer, true, false);
}

static void Rr_InitBackgroundPipelines(SRr* Rr)
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
        .pSetLayouts = &Rr->DrawImageDescriptorLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };

    VK_ASSERT(vkCreatePipelineLayout(Rr->Device, &ComputeLayout, NULL, &Rr->GradientPipelineLayout))

    VkShaderModule ComputeDrawShader;
    LoadShaderModule("test.comp.spv", Rr->Device, &ComputeDrawShader);

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

static void Rr_InitTrianglePipeline(SRr* Rr)
{
    VkShaderModule VertModule;
    LoadShaderModule("triangle.vert.spv", Rr->Device, &VertModule);

    VkShaderModule FragModule;
    LoadShaderModule("triangle.frag.spv", Rr->Device, &FragModule);

    VkPipelineLayoutCreateInfo LayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        // .setLayoutCount = 1,
        // .pSetLayouts = &Rr->DrawImageDescriptorLayout,
        // .pushConstantRangeCount = 1,
        // .pPushConstantRanges = &PushConstantRange,
    };
    vkCreatePipelineLayout(Rr->Device, &LayoutInfo, NULL, &Rr->TrianglePipelineLayout);

    SPipelineBuilder Builder = GetPipelineBuilder();
    PipelineBuilder_Default(&Builder, VertModule, FragModule, Rr->DrawImage.Format, VK_FORMAT_UNDEFINED, Rr->TrianglePipelineLayout);
    Rr->TrianglePipeline = Rr_BuildPipeline(Rr, &Builder);

    vkDestroyShaderModule(Rr->Device, VertModule, NULL);
    vkDestroyShaderModule(Rr->Device, FragModule, NULL);
}

static void Rr_InitPipelines(SRr* Rr)
{
    Rr_InitBackgroundPipelines(Rr);
    Rr_InitTrianglePipeline(Rr);
}

static void Rr_DrawBackground(SRr* Rr, VkCommandBuffer CommandBuffer)
{
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Rr->GradientPipeline);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Rr->GradientPipelineLayout, 0, 1, &Rr->DrawImageDescriptors, 0, NULL);
    SComputeConstants ComputeConstants;
    glm_vec4_copy((vec4){ 1.0f, 0.0f, 0.0f, 1.0f }, ComputeConstants.Vec0);
    glm_vec4_copy((vec4){ 0.0f, 1.0f, 0.0f, 1.0f }, ComputeConstants.Vec1);
    vkCmdPushConstants(CommandBuffer, Rr->GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SComputeConstants), &ComputeConstants);
    vkCmdDispatch(CommandBuffer, ceil(Rr->DrawExtent.width / 16.0), ceil(Rr->DrawExtent.height / 16.0), 1);

    // float Flash = fabsf(sinf((float)Renderer->FrameNumber / 240.0f));
    // VkClearColorValue ClearValue = { { 0.0f, 0.0f, Flash, 1.0f } };
    //
    // VkImageSubresourceRange ClearRange = GetImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    //
    // vkCmdClearColorImage(CommandBuffer, Renderer->DrawImage.Handle, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &ClearRange);
}

static void Rr_DrawGeometry(SRr* Rr, VkCommandBuffer CommandBuffer)
{
    VkRenderingAttachmentInfo colorAttachment = GetRenderingAttachmentInfo(Rr->DrawImage.View, NULL, VK_IMAGE_LAYOUT_GENERAL);

    VkRenderingInfo renderInfo = GetRenderingInfo(Rr->DrawExtent, &colorAttachment, NULL);
    vkCmdBeginRendering(CommandBuffer, &renderInfo);

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Rr->TrianglePipeline);

    VkViewport viewport = { 0 };
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)Rr->DrawExtent.width;
    viewport.height = (float)Rr->DrawExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(CommandBuffer, 0, 1, &viewport);

    VkRect2D scissor = { 0 };
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = Rr->DrawExtent.width;
    scissor.extent.height = Rr->DrawExtent.height;

    vkCmdSetScissor(CommandBuffer, 0, 1, &scissor);

    vkCmdDraw(CommandBuffer, 3, 1, 0, 0);

    vkCmdEndRendering(CommandBuffer);
}

void Rr_Init(SRr* Rr, struct SDL_Window* Window)
{
    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = AppTitle,
        .pEngineName = AppTitle,
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
    };

    u32 ExtensionCount;
    const char* const* SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&ExtensionCount);

    const char** Extensions = StackAlloc(const char*, ExtensionCount + 1);
    for (u32 Index = 0; Index < ExtensionCount; Index++)
    {
        Extensions[Index] = SDLExtensions[Index];
    }

    if (bEnableValidationLayers)
    {
        Extensions[ExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        ExtensionCount++;
    }

    VkInstanceCreateInfo VKInstInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &VKAppInfo,
        .enabledExtensionCount = ExtensionCount,
        .ppEnabledExtensionNames = Extensions,
    };

    if (bEnableValidationLayers)
    {
        const char* ValidationLayerName = "VK_LAYER_KHRONOS_validation";

        u32 InstanceLayerCount;
        vkEnumerateInstanceLayerProperties(&InstanceLayerCount, NULL);
        VkLayerProperties* InstanceLayerProperties = StackAlloc(VkLayerProperties, InstanceLayerCount);
        vkEnumerateInstanceLayerProperties(&InstanceLayerCount, InstanceLayerProperties);

        bool bValidationLayerPresent = false;
        for (u32 Index = 0; Index < InstanceLayerCount; Index++)
        {
            VkLayerProperties* Layer = &InstanceLayerProperties[Index];
            if (strcmp(Layer->layerName, ValidationLayerName) == 0)
            {
                bValidationLayerPresent = true;
                break;
            }
        }
        if (!bValidationLayerPresent)
        {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No %s present!", ValidationLayerName);
            abort();
        }

        VKInstInfo.enabledLayerCount = 1;
        VKInstInfo.ppEnabledLayerNames = &ValidationLayerName;

        VkValidationFeatureEnableEXT EnabledValidationFeatures[] = { VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT };

        VkValidationFeaturesEXT ValidationFeatures = {
            .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            .enabledValidationFeatureCount = 1,
            .pEnabledValidationFeatures = EnabledValidationFeatures,
        };

        VKInstInfo.pNext = &ValidationFeatures;
    }

    VK_ASSERT(vkCreateInstance(&VKInstInfo, NULL, &Rr->Instance))

    volkLoadInstance(Rr->Instance);

    if (bEnableValidationLayers)
    {
        Rr_InitDebugMessenger(Rr);
    }

    if (SDL_Vulkan_CreateSurface(Window, Rr->Instance, NULL, &Rr->Surface) != SDL_TRUE)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create Vulkan surface: %s", SDL_GetError());
        abort();
    }

    Rr_InitDevice(Rr);

    Rr_InitAllocator(Rr);

    // volkLoadDevice(Renderer->Device);

    u32 Width, Height;
    SDL_GetWindowSizeInPixels(Window, (i32*)&Width, (i32*)&Height);
    bool bVSync = true;
    Rr_CreateSwapchain(Rr, &Width, &Height, bVSync);

    Rr_InitCommands(Rr);

    Rr_InitSyncStructures(Rr);

    Rr_InitDescriptors(Rr);

    Rr_InitPipelines(Rr);
}

static PFN_vkVoidFunction Rr_ImGui_LoadFunction(const char* FuncName, void* Userdata)
{
    return (PFN_vkVoidFunction)vkGetInstanceProcAddr(volkGetLoadedInstance(), FuncName);
}

void Rr_InitImGui(SRr* Rr, struct SDL_Window* Window)
{
    VkDevice Device = Rr->Device;

    VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = Rr->GraphicsQueue.FamilyIndex,
    };
    VK_ASSERT(vkCreateCommandPool(Rr->Device, &CommandPoolInfo, NULL, &Rr->ImmediateMode.CommandPool))
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Rr->ImmediateMode.CommandPool, 1);
    VK_ASSERT(vkAllocateCommandBuffers(Rr->Device, &CommandBufferAllocateInfo, &Rr->ImmediateMode.CommandBuffer))
    VkFenceCreateInfo FenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_ASSERT(vkCreateFence(Device, &FenceCreateInfo, NULL, &Rr->ImmediateMode.Fence))

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
        .poolSizeCount = (u32)ArraySize(PoolSizes),
        .pPoolSizes = PoolSizes,
    };

    VK_ASSERT(vkCreateDescriptorPool(Device, &PoolCreateInfo, NULL, &Rr->ImmediateMode.DescriptorPool))

    igCreateContext(NULL);

    ImGui_ImplVulkan_LoadFunctions(Rr_ImGui_LoadFunction, NULL);
    ImGui_ImplSDL3_InitForVulkan(Window);

    ImGui_ImplVulkan_InitInfo InitInfo = {
        .Instance = Rr->Instance,
        .PhysicalDevice = Rr->PhysicalDevice.Handle,
        .Device = Device,
        .Queue = Rr->GraphicsQueue.Handle,
        .DescriptorPool = Rr->ImmediateMode.DescriptorPool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .PipelineRenderingCreateInfo.colorAttachmentCount = 1,
        .PipelineRenderingCreateInfo.pColorAttachmentFormats = &Rr->Swapchain.Format,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
    };

    ImGui_ImplVulkan_Init(&InitInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    Rr->ImmediateMode.bInit = true;
}

void Rr_Cleanup(SRr* Rr)
{
    VkDevice Device = Rr->Device;

    vkDeviceWaitIdle(Rr->Device);

    if (Rr->ImmediateMode.bInit)
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        vkDestroyCommandPool(Rr->Device, Rr->ImmediateMode.CommandPool, NULL);
        vkDestroyDescriptorPool(Device, Rr->ImmediateMode.DescriptorPool, NULL);
    }

    vkDestroyPipelineLayout(Device, Rr->TrianglePipelineLayout, NULL);
    vkDestroyPipeline(Device, Rr->TrianglePipeline, NULL);

    vkDestroyPipelineLayout(Device, Rr->GradientPipelineLayout, NULL);
    vkDestroyPipeline(Device, Rr->GradientPipeline, NULL);

    Rr_UpdateDrawImageDescriptors(Rr, false, true);

    DescriptorAllocator_DestroyPool(&Rr->GlobalDescriptorAllocator, Device);

    vkDestroyFence(Device, Rr->ImmediateMode.Fence, NULL);

    for (u32 Index = 0; Index < FRAME_OVERLAP; ++Index)
    {
        SFrameData* Frame = &Rr->Frames[Index];
        vkDestroyCommandPool(Rr->Device, Frame->CommandPool, NULL);

        vkDestroyFence(Device, Frame->RenderFence, NULL);
        vkDestroySemaphore(Device, Frame->RenderSemaphore, NULL);
        vkDestroySemaphore(Device, Frame->SwapchainSemaphore, NULL);
    }

    Rr_CleanupSwapchain(Rr, Rr->Swapchain.Handle);

    vmaDestroyAllocator(Rr->Allocator);

    vkDestroySurfaceKHR(Rr->Instance, Rr->Surface, NULL);
    vkDestroyDevice(Rr->Device, NULL);

    if (bEnableValidationLayers)
    {
        vkDestroyDebugUtilsMessengerEXT(Rr->Instance, Rr->Messenger, NULL);
    }
    vkDestroyInstance(Rr->Instance, NULL);
}

void Rr_Draw(SRr* Rr)
{
    VkDevice Device = Rr->Device;
    SFrameData* CurrentFrame = Rr_GetCurrentFrame(Rr);
    SAllocatedImage* DrawImage = &Rr->DrawImage;
    VkCommandBuffer CommandBuffer = CurrentFrame->MainCommandBuffer;

    VK_ASSERT(vkWaitForFences(Device, 1, &CurrentFrame->RenderFence, true, 1000000000))
    VK_ASSERT(vkResetFences(Device, 1, &CurrentFrame->RenderFence))

    u32 SwapchainImageIndex;
    VK_ASSERT(vkAcquireNextImageKHR(Device, Rr->Swapchain.Handle, 1000000000, CurrentFrame->SwapchainSemaphore, VK_NULL_HANDLE, &SwapchainImageIndex))

    VkImage SwapchainImage = Rr->Swapchain.Images[SwapchainImageIndex].Handle;

    Rr->DrawExtent.width = DrawImage->Extent.width;
    Rr->DrawExtent.height = DrawImage->Extent.height;

    VkCommandBufferBeginInfo CommandBufferBeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_ASSERT(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo))

    STransitionImage DrawImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = DrawImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_2_NONE,
        .StageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT
    };
    TransitionImage_To(&DrawImageTransition,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL);
    Rr_DrawBackground(Rr, CommandBuffer);
    TransitionImage_To(&DrawImageTransition,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    Rr_DrawGeometry(Rr, CommandBuffer);
    TransitionImage_To(&DrawImageTransition,
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

    CopyImageToImage(CommandBuffer, DrawImage->Handle, SwapchainImage, Rr->DrawExtent, Rr->Swapchain.Extent);
    TransitionImage_To(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    if (Rr->ImmediateMode.bInit)
    {
        VkRenderingAttachmentInfo ColorAttachment = GetRenderingAttachmentInfo(Rr->Swapchain.Images[SwapchainImageIndex].View, NULL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderInfo = GetRenderingInfo(Rr->Swapchain.Extent, &ColorAttachment, NULL);

        vkCmdBeginRendering(CommandBuffer, &renderInfo);

        ImGui_ImplVulkan_RenderDrawData(igGetDrawData(), CommandBuffer, VK_NULL_HANDLE);

        vkCmdEndRendering(CommandBuffer);
    }

    TransitionImage_To(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_ASSERT(vkEndCommandBuffer(CommandBuffer))

    VkCommandBufferSubmitInfo CommandBufferSubmitInfo = GetCommandBufferSubmitInfo(CommandBuffer);

    VkSemaphoreSubmitInfo WaitSemaphoreSubmitInfo = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, CurrentFrame->SwapchainSemaphore);
    VkSemaphoreSubmitInfo SignalSemaphoreSubmitInfo = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, CurrentFrame->RenderSemaphore);

    VkSubmitInfo2 SubmitInfo = GetSubmitInfo(&CommandBufferSubmitInfo, &SignalSemaphoreSubmitInfo, &WaitSemaphoreSubmitInfo);

    VK_ASSERT(vkQueueSubmit2(Rr->GraphicsQueue.Handle, 1, &SubmitInfo, CurrentFrame->RenderFence))

    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &CurrentFrame->RenderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Rr->Swapchain.Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    VK_ASSERT(vkQueuePresentKHR(Rr->GraphicsQueue.Handle, &PresentInfo))

    Rr->FrameNumber++;
}

void Rr_Resize(SRr* Rr, u32 Width, u32 Height)
{
    vkDeviceWaitIdle(Rr->Device);

    Rr_CreateSwapchain(Rr, &Width, &Height, true);

    Rr_UpdateDrawImageDescriptors(Rr, true, true);
}
