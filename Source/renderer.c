#include "renderer.h"

#include <stdio.h>
#include <SDL_log.h>
#define VOLK_IMPLEMENTATION
#include <vulkan/vulkan.h>
#include <volk.h>
#include <SDL3/SDL_vulkan.h>

#include "app.h"
#include "core.h"

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

void CreateRenderer(SApp* App)
{
    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = AppTitle,
        .pEngineName = AppTitle,
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
    };

    u32 SDLExtensionCount;
    const char* const* SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&SDLExtensionCount);

    const char** Extensions = (const char**)malloc(sizeof(const char*) * (SDLExtensionCount + 1));
    for (int Index = 0; Index < SDLExtensionCount; Index++)
    {
        Extensions[Index] = SDLExtensions[Index];
    }

    if (true)
    {
        Extensions[SDLExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    VkInstanceCreateInfo VKInstInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = &VKAppInfo,
        .enabledExtensionCount = SDLExtensionCount + 1,
        .ppEnabledExtensionNames = Extensions
    };

    /* @TODO: Validation. */
    if (true)
    {
        const char* ValidationLayerName = "VK_LAYER_KHRONOS_validation";

        u32 InstanceLayerCount;
        vkEnumerateInstanceLayerProperties(&InstanceLayerCount, nullptr);
        VkLayerProperties InstanceLayerProperties[InstanceLayerCount];
        vkEnumerateInstanceLayerProperties(&InstanceLayerCount, InstanceLayerProperties);
        bool bValidationLayerPresent = false;

        for (int Index = 0; Index < InstanceLayerCount; Index++)
        {
            VkLayerProperties* Layer = &InstanceLayerProperties[Index];
            if (strcmp(Layer->layerName, ValidationLayerName) == 0)
            {
                bValidationLayerPresent = true;
                break;
            }
        }

        VKInstInfo.enabledLayerCount = 1;
        VKInstInfo.ppEnabledLayerNames = &ValidationLayerName;
    }

    VK_ASSERT(vkCreateInstance(&VKInstInfo, NULL, &App->Renderer.Instance));

    volkLoadInstance(App->Renderer.Instance);

    VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCI = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = VulkanMessage
    };

    VK_ASSERT(vkCreateDebugUtilsMessengerEXT(App->Renderer.Instance, &DebugUtilsMessengerCI, nullptr, &App->Renderer.Messenger));

    assert(SDL_Vulkan_CreateSurface(App->Window, App->Renderer.Instance, NULL, &App->Renderer.Surface) == SDL_TRUE);

    u32 PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(App->Renderer.Instance, &PhysicalDeviceCount, nullptr);
    if (PhysicalDeviceCount == 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No device with Vulkan support found\n");
    }

    VkPhysicalDevice PhysicalDevices[PhysicalDeviceCount] = {};
    vkEnumeratePhysicalDevices(App->Renderer.Instance, &PhysicalDeviceCount, &PhysicalDevices[0]);

    for (i32 Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        VkPhysicalDeviceProperties PhysicalDeviceProperties = {};
        vkGetPhysicalDeviceProperties(PhysicalDevices[Index], &PhysicalDeviceProperties);
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "%s", PhysicalDeviceProperties.deviceName);
    }

    // SDL_LogError(SDL_LOG_CATEGORY_ERROR, "No device with Vulkan support found\n");
}
