#include "Renderer.h"

#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <SDL_log.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#define VOLK_IMPLEMENTATION
#include <volk.h>
#include <SDL3/SDL_vulkan.h>

#include "App.h"

static const b32 bEnableValidationLayers = true;

static inline SRendererSwapchain EmptySwapchain()
{
    return (SRendererSwapchain){
        .Handle = VK_NULL_HANDLE,
        // .VkFormat = ,
        // .VkColorSpaceKHR ColorSpace,
        // .SVulkanImage * Images,
        // .VkExtent2D Extent,
    };
}

static inline SRendererQueue EmptyQueue()
{
    return (SRendererQueue){
        .Handle = VK_NULL_HANDLE,
        .FamilyIndex = UINT32_MAX
    };
}

static inline SPhysicalDevice EmptyPhysicalDevice()
{
    return (SPhysicalDevice){
        .Handle = VK_NULL_HANDLE,
        .Features = {},
        .MemoryProperties = {}
    };
}

static VkBool32 VulkanMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
    VkDebugUtilsMessageTypeFlagsEXT Types,
    const VkDebugUtilsMessengerCallbackDataEXT* CallbackData,
    void* Userdata)
{
    if (CallbackData != nullptr)
    {
        fprintf(stderr, "%s\n", CallbackData->pMessage);
    }
    return true;
}

static bool CheckPhysicalDevice(SRenderer* Renderer, VkPhysicalDevice PhysicalDevice)
{
    u32 ExtensionCount;
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);
    if (ExtensionCount == 0)
    {
        return false;
    }

    VkExtensionProperties Extensions[ExtensionCount] = {};
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, Extensions);

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
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);
    if (QueueFamilyCount == 0)
    {
        return false;
    }

    VkQueueFamilyProperties QueueFamilyProperties[QueueFamilyCount];
    VkBool32 QueuePresentSupport[QueueFamilyCount];

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

    return false;
}

static void InitDevice(SRenderer* Renderer)
{
    u32 PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(Renderer->Instance, &PhysicalDeviceCount, nullptr);
    if (PhysicalDeviceCount == 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No device with Vulkan support found");
        abort();
    }

    VkPhysicalDevice PhysicalDevices[PhysicalDeviceCount] = {};
    vkEnumeratePhysicalDevices(Renderer->Instance, &PhysicalDeviceCount, &PhysicalDevices[0]);

    VkPhysicalDeviceProperties PhysicalDeviceProperties = {};
    for (u32 Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        if (CheckPhysicalDevice(Renderer, PhysicalDevices[Index]))
        {
            Renderer->PhysicalDevice.Handle = PhysicalDevices[Index];
            vkGetPhysicalDeviceProperties(PhysicalDevices[Index], &PhysicalDeviceProperties);
            SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Selected GPU: %s", PhysicalDeviceProperties.deviceName);
            break;
        }
    }

    if (Renderer->PhysicalDevice.Handle == VK_NULL_HANDLE || Renderer->GraphicsQueue.FamilyIndex == UINT32_MAX)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not select physical device based on the chosen properties!");
        abort();
    }
    else
    {
        vkGetPhysicalDeviceFeatures(Renderer->PhysicalDevice.Handle, &Renderer->PhysicalDevice.Features);
        vkGetPhysicalDeviceMemoryProperties(Renderer->PhysicalDevice.Handle, &Renderer->PhysicalDevice.MemoryProperties);
    }

    const float QueuePriorities[] = { 1.0f };
    VkDeviceQueueCreateInfo QueueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = QueuePriorities,
    };

    const char* SwapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &QueueInfo,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &SwapchainExtension,
    };

    VK_ASSERT(vkCreateDevice(Renderer->PhysicalDevice.Handle, &DeviceCreateInfo, nullptr, &Renderer->Device));

    vkGetDeviceQueue(Renderer->Device, Renderer->GraphicsQueue.FamilyIndex, 0, &Renderer->GraphicsQueue.Handle);
}

static void CleanupSwapchain(SRenderer* Renderer, VkSwapchainKHR Swapchain)
{
    for (u32 Index = 0; Index < Renderer->Swapchain.ImageCount; Index++)
    {
        vkDestroyImageView(Renderer->Device, Renderer->Swapchain.Images[Index].View, nullptr);
    }
    memset(Renderer->Swapchain.Images, 0, sizeof(Renderer->Swapchain.Images));
    Renderer->Swapchain.ImageCount = 0;
    vkDestroySwapchainKHR(Renderer->Device, Swapchain, nullptr);
}

static void CreateSwapchain(SRenderer* Renderer, u32* Width, u32* Height, bool bVSync)
{
    VkSwapchainKHR OldSwapchain = Renderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfCaps;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &SurfCaps));

    u32 PresentModeCount;
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &PresentModeCount, nullptr));
    assert(PresentModeCount > 0);

    VkPresentModeKHR PresentModes[PresentModeCount] = {};
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &PresentModeCount, PresentModes));

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

    u32 DesiredNumberOfSwapchainImages = Renderer->CommandBufferCount = SurfCaps.minImageCount + 1;
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

    VkExtent2D SwapchainExtent = {};
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

    Renderer->Swapchain.Extent = SwapchainExtent;

    u32 FormatCount;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &FormatCount, NULL));
    assert(FormatCount > 0);

    VkSurfaceFormatKHR SurfaceFormats[FormatCount] = {};
    VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &FormatCount, SurfaceFormats));

    bool bPreferredFormatFound = false;
    for (u32 Index = 0; Index < PresentModeCount; Index++)
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
    for (u32 Index = 0; Index < sizeof(CompositeAlphaFlags) / sizeof(*CompositeAlphaFlags); Index++)
    {
        if (SurfCaps.supportedCompositeAlpha & CompositeAlphaFlags[Index])
        {
            CompositeAlpha = CompositeAlphaFlags[Index];
            break;
        }
    }

    VkSwapchainCreateInfoKHR SwapchainCI = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = Renderer->Surface,
        .minImageCount = DesiredNumberOfSwapchainImages,
        .imageFormat = Renderer->Swapchain.Format,
        .imageColorSpace = Renderer->Swapchain.ColorSpace,
        .imageExtent = { SwapchainExtent.width, SwapchainExtent.height },
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = (VkSurfaceTransformFlagBitsKHR)PreTransform,
        .imageArrayLayers = 1,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .presentMode = SwapchainPresentMode,
        .oldSwapchain = OldSwapchain,
        .clipped = VK_TRUE,
        .compositeAlpha = CompositeAlpha,
    };

    if (SurfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        SwapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (SurfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        SwapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VK_ASSERT(vkCreateSwapchainKHR(Renderer->Device, &SwapchainCI, nullptr, &Renderer->Swapchain.Handle));

    if (OldSwapchain != VK_NULL_HANDLE)
    {
        CleanupSwapchain(Renderer, OldSwapchain);
    }

    u32 ImageCount = 0;
    VK_ASSERT(vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, NULL));
    assert(ImageCount <= MAX_SWAPCHAIN_IMAGE_COUNT);

    Renderer->Swapchain.ImageCount = ImageCount;
    VkImage Images[ImageCount];
    VK_ASSERT(vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, Images));

    VkImageViewCreateInfo ColorAttachmentView = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .format = Renderer->Swapchain.Format,
        .components = (VkComponentMapping){
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A },
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
    };

    for (u32 i = 0; i < ImageCount; i++)
    {
        Renderer->Swapchain.Images[i].Handle = Images[i];
        ColorAttachmentView.image = Images[i];
        VK_ASSERT(vkCreateImageView(Renderer->Device, &ColorAttachmentView, nullptr, &Renderer->Swapchain.Images[i].View));
    }
}

static void InitDebugMessenger(SRenderer* Renderer)
{
    VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCI = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = VulkanMessage
    };

    VK_ASSERT(vkCreateDebugUtilsMessengerEXT(Renderer->Instance, &DebugUtilsMessengerCI, nullptr, &Renderer->Messenger));
}

void RendererInit(SApp* App)
{
    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    SRenderer* Renderer = &App->Renderer;
    *Renderer = (SRenderer){
        .Instance = VK_NULL_HANDLE,
        .PhysicalDevice = EmptyPhysicalDevice(),
        .Device = VK_NULL_HANDLE,
        .GraphicsQueue = EmptyQueue(),
        .TransferQueue = EmptyQueue(),
        .ComputeQueue = EmptyQueue(),
        .Allocator = VK_NULL_HANDLE,
        .Surface = VK_NULL_HANDLE,
        .Swapchain = EmptySwapchain(),
        .Messenger = VK_NULL_HANDLE,
    };

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = AppTitle,
        .pEngineName = AppTitle,
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
    };

    u32 ExtensionCount;
    const char* const* SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&ExtensionCount);

    const char* Extensions[ExtensionCount + 1] = {};
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
        .pNext = nullptr,
        .pApplicationInfo = &VKAppInfo,
        .enabledExtensionCount = ExtensionCount,
        .ppEnabledExtensionNames = Extensions
    };

    if (bEnableValidationLayers)
    {
        const char* ValidationLayerName = "VK_LAYER_KHRONOS_validation";

        u32 InstanceLayerCount;
        vkEnumerateInstanceLayerProperties(&InstanceLayerCount, nullptr);
        VkLayerProperties InstanceLayerProperties[InstanceLayerCount];
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
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No %s present!\n", ValidationLayerName);
            abort();
        }

        VKInstInfo.enabledLayerCount = 1;
        VKInstInfo.ppEnabledLayerNames = &ValidationLayerName;
    }

    VK_ASSERT(vkCreateInstance(&VKInstInfo, NULL, &Renderer->Instance));

    volkLoadInstance(Renderer->Instance);

    InitDebugMessenger(Renderer);

    if (SDL_Vulkan_CreateSurface(App->Window, Renderer->Instance, NULL, &Renderer->Surface) != SDL_TRUE)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create Vulkan surface: %s", SDL_GetError());
        abort();
    }

    InitDevice(Renderer);

    u32 Width, Height;
    bool bVSync = true;
    CreateSwapchain(Renderer, &Width, &Height, bVSync);
}

void RendererCleanup(SRenderer* Renderer)
{
    vkDeviceWaitIdle(Renderer->Device);

    CleanupSwapchain(Renderer, Renderer->Swapchain.Handle);

    vkDestroySurfaceKHR(Renderer->Instance, Renderer->Surface, nullptr);
    vkDestroyDevice(Renderer->Device, nullptr);

    vkDestroyDebugUtilsMessengerEXT(Renderer->Instance, Renderer->Messenger, nullptr);
    vkDestroyInstance(Renderer->Instance, nullptr);
}
