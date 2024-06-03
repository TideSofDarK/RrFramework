#include "Rr_Vulkan.h"

#include "Rr_Defines.h"
#include "Rr_Memory.h"

#include <SDL3/SDL.h>

static bool CheckPhysicalDevice(
    VkPhysicalDevice PhysicalDevice,
    VkSurfaceKHR Surface,
    u32* OutGraphicsQueueFamilyIndex,
    u32* OutTransferQueueFamilyIndex)
{
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
        vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, Index, Surface, &QueuePresentSupport[Index]);
        if (QueuePresentSupport[Index]
            && QueueFamilyProperties[Index].queueCount > 0
            && (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            GraphicsQueueFamilyIndex = Index;
            break;
        }
    }

    if (GraphicsQueueFamilyIndex == ~0U)
    {
        return false;
    }

    const u32 bForceDisableTransferQueue = RR_FORCE_DISABLE_TRANSFER_QUEUE;

    if (!bForceDisableTransferQueue)
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

    *OutGraphicsQueueFamilyIndex = GraphicsQueueFamilyIndex;
    *OutTransferQueueFamilyIndex = TransferQueueFamilyIndex == ~0U ? GraphicsQueueFamilyIndex : TransferQueueFamilyIndex;

    Rr_StackFree(QueuePresentSupport);
    Rr_StackFree(QueueFamilyProperties);
    Rr_StackFree(Extensions);

    return true;
}

Rr_PhysicalDevice Rr_CreatePhysicalDevice(
    VkInstance Instance,
    VkSurfaceKHR Surface,
    u32* OutGraphicsQueueFamilyIndex,
    u32* OutTransferQueueFamilyIndex)
{
    Rr_PhysicalDevice PhysicalDevice = { 0 };

    u32 PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, NULL);
    if (PhysicalDeviceCount == 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No device with Vulkan support found");
        abort();
    }

    VkPhysicalDevice* PhysicalDevices = Rr_StackAlloc(VkPhysicalDevice, PhysicalDeviceCount);
    vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, &PhysicalDevices[0]);

    PhysicalDevice.SubgroupProperties = (VkPhysicalDeviceSubgroupProperties){ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES, .pNext = NULL };

    PhysicalDevice.Properties = (VkPhysicalDeviceProperties2){
        .pNext = &PhysicalDevice.SubgroupProperties,
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };

    bool bUseTransferQueue = false;
    bool bFoundSuitableDevice = false;
    for (u32 Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        VkPhysicalDevice PhysicalDeviceHandle = PhysicalDevices[Index];
        if (CheckPhysicalDevice(
                PhysicalDeviceHandle,
                Surface,
                OutGraphicsQueueFamilyIndex,
                OutTransferQueueFamilyIndex))
        {
            bUseTransferQueue = *OutGraphicsQueueFamilyIndex != *OutTransferQueueFamilyIndex;

            vkGetPhysicalDeviceFeatures(PhysicalDeviceHandle, &PhysicalDevice.Features);
            vkGetPhysicalDeviceMemoryProperties(PhysicalDeviceHandle, &PhysicalDevice.MemoryProperties);
            vkGetPhysicalDeviceProperties2(PhysicalDeviceHandle, &PhysicalDevice.Properties);

            SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Selected GPU: %s", PhysicalDevice.Properties.properties.deviceName);

            PhysicalDevice.Handle = PhysicalDeviceHandle;
            bFoundSuitableDevice = true;
            break;
        }
    }
    if (!bFoundSuitableDevice)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not select physical device based on the chosen properties!");
        abort();
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Unified Queue Mode: %s", !bUseTransferQueue ? "true" : "false");

    Rr_StackFree(PhysicalDevices);

    return PhysicalDevice;
}

void Rr_InitDeviceAndQueues(
    VkPhysicalDevice PhysicalDevice,
    u32 GraphicsQueueFamilyIndex,
    u32 TransferQueueFamilyIndex,
    VkDevice* OutDevice,
    VkQueue* OutGraphicsQueue,
    VkQueue* OutTransferQueue)
{
    bool bUseTransferQueue = GraphicsQueueFamilyIndex != TransferQueueFamilyIndex;
    const float QueuePriorities[] = { 1.0f };
    VkDeviceQueueCreateInfo QueueInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = GraphicsQueueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = QueuePriorities,
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = TransferQueueFamilyIndex,
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
    };

    const char* DeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    const VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &Features12,
        .queueCreateInfoCount = bUseTransferQueue ? 2 : 1,
        .pQueueCreateInfos = QueueInfos,
        .enabledExtensionCount = SDL_arraysize(DeviceExtensions),
        .ppEnabledExtensionNames = DeviceExtensions,
    };

    vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, NULL, OutDevice);

    vkGetDeviceQueue(*OutDevice, GraphicsQueueFamilyIndex, 0, OutGraphicsQueue);
    if (bUseTransferQueue)
    {
        vkGetDeviceQueue(*OutDevice, TransferQueueFamilyIndex, 0, OutTransferQueue);
    }
}
