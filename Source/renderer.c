#include "renderer.h"

#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL_log.h>
#include <vulkan/vulkan_core.h>
#define VOLK_IMPLEMENTATION
#include <volk.h>
#include <SDL3/SDL_vulkan.h>

#include "app.h"

static const b32 bEnableValidationLayers = _DEBUG;

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

        if ((QueueFamilyProperties[Index].queueCount > 0) && (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            if (QueuePresentSupport[Index])
            {
                Renderer->GraphicsQueue.FamilyIndex = Index;
                return true;
            }
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

static void CreateSwapchain(SRenderer* Renderer, u32* Width, u32* Height, bool bVSync)
{
    VkSwapchainKHR OldSwapchain = Renderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfCaps;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &SurfCaps));

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "%d %d", SurfCaps.minImageCount, SurfCaps.maxImageCount);
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

    VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCI = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = VulkanMessage
    };

    VK_ASSERT(vkCreateDebugUtilsMessengerEXT(Renderer->Instance, &DebugUtilsMessengerCI, nullptr, &Renderer->Messenger));

    if (SDL_Vulkan_CreateSurface(App->Window, Renderer->Instance, NULL, &Renderer->Surface) != SDL_TRUE)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create Vulkan surface: %s", SDL_GetError());
        abort();
    }

    InitDevice(Renderer);

    u32 Width, Height;
    bool bVSync = {};
    CreateSwapchain(Renderer, &Width, &Height, bVSync);
}

void RendererCleanup(SApp* App)
{
    // destroy_swapchain();

    vkDeviceWaitIdle(App->Renderer.Device);

    vkDestroySurfaceKHR(App->Renderer.Instance, App->Renderer.Surface, nullptr);
    vkDestroyDevice(App->Renderer.Device, nullptr);

    vkDestroyDebugUtilsMessengerEXT(App->Renderer.Instance, App->Renderer.Messenger, nullptr);
    vkDestroyInstance(App->Renderer.Instance, nullptr);
}
