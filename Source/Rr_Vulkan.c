#include "Rr_Vulkan.h"

#include "Rr_Log.h"
#include "Rr_Memory.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

static bool Rr_CheckPhysicalDevice(
    VkPhysicalDevice PhysicalDevice,
    VkSurfaceKHR Surface,
    uint32_t *OutGraphicsQueueFamilyIndex,
    uint32_t *OutTransferQueueFamilyIndex,
    Rr_Arena *Arena)
{
    uint32_t ExtensionCount;
    vkEnumerateDeviceExtensionProperties(
        PhysicalDevice,
        NULL,
        &ExtensionCount,
        NULL);
    if(ExtensionCount == 0)
    {
        return false;
    }

    const char *TargetExtensions[] = {
        "VK_KHR_swapchain",
    };

    bool FoundExtensions[] = { 0 };

    VkExtensionProperties *Extensions =
        RR_ALLOC_TYPE_COUNT(Arena, VkExtensionProperties, ExtensionCount);
    vkEnumerateDeviceExtensionProperties(
        PhysicalDevice,
        NULL,
        &ExtensionCount,
        Extensions);

    for(uint32_t Index = 0; Index < ExtensionCount; Index++)
    {
        for(uint32_t TargetIndex = 0;
            TargetIndex < SDL_arraysize(TargetExtensions);
            ++TargetIndex)
        {
            if(strcmp(
                   Extensions[Index].extensionName,
                   TargetExtensions[TargetIndex]) == 0)
            {
                FoundExtensions[TargetIndex] = true;
            }
        }
    }
    for(uint32_t TargetIndex = 0; TargetIndex < SDL_arraysize(TargetExtensions);
        ++TargetIndex)
    {
        if(!FoundExtensions[TargetIndex])
        {
            return false;
        }
    }

    uint32_t QueueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(
        PhysicalDevice,
        &QueueFamilyCount,
        NULL);
    if(QueueFamilyCount == 0)
    {
        return false;
    }

    VkQueueFamilyProperties *QueueFamilyProperties =
        RR_ALLOC_TYPE_COUNT(Arena, VkQueueFamilyProperties, QueueFamilyCount);
    VkBool32 *QueuePresentSupport =
        RR_ALLOC_TYPE_COUNT(Arena, VkBool32, QueueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(
        PhysicalDevice,
        &QueueFamilyCount,
        QueueFamilyProperties);

    uint32_t GraphicsQueueFamilyIndex = ~0U;
    uint32_t TransferQueueFamilyIndex = ~0U;

    for(uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(
            PhysicalDevice,
            Index,
            Surface,
            &QueuePresentSupport[Index]);
        if(QueuePresentSupport[Index] &&
           QueueFamilyProperties[Index].queueCount > 0 &&
           (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            GraphicsQueueFamilyIndex = Index;
            break;
        }
    }

    if(GraphicsQueueFamilyIndex == ~0U)
    {
        return false;
    }

    bool ForceDisableTransferQueue = RR_FORCE_DISABLE_TRANSFER_QUEUE;

    if(!ForceDisableTransferQueue)
    {
        for(uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
        {
            if(Index == GraphicsQueueFamilyIndex)
            {
                continue;
            }

            if(QueueFamilyProperties[Index].queueCount > 0 &&
               (QueueFamilyProperties[Index].queueFlags &
                VK_QUEUE_TRANSFER_BIT))
            {
                TransferQueueFamilyIndex = Index;
                break;
            }
        }
    }

    *OutGraphicsQueueFamilyIndex = GraphicsQueueFamilyIndex;
    *OutTransferQueueFamilyIndex = TransferQueueFamilyIndex == ~0U
                                       ? GraphicsQueueFamilyIndex
                                       : TransferQueueFamilyIndex;

    return true;
}

Rr_PhysicalDevice Rr_SelectPhysicalDevice(
    VkInstance Instance,
    VkSurfaceKHR Surface,
    uint32_t *OutGraphicsQueueFamilyIndex,
    uint32_t *OutTransferQueueFamilyIndex,
    Rr_Arena *Arena)
{
    uint32_t PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, NULL);
    if(PhysicalDeviceCount == 0)
    {
        RR_LOG("No device with Vulkan support found");
    }

    VkPhysicalDevice *PhysicalDevices =
        RR_ALLOC_TYPE_COUNT(Arena, VkPhysicalDevice, PhysicalDeviceCount);
    vkEnumeratePhysicalDevices(
        Instance,
        &PhysicalDeviceCount,
        &PhysicalDevices[0]);

    RR_LOG("Selecting Vulkan device:");

    typedef char Rr_DeviceString[1024];
    RR_SLICE(Rr_DeviceString) DeviceStrings = { 0 };
    uint32_t BestDeviceIndex = UINT32_MAX;
    static const int PreferredDeviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    // static const int PreferredDeviceType =
    // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    int BestDeviceType = 0;
    VkDeviceSize BestDeviceMemory = 0;
    for(uint32_t Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        VkPhysicalDevice PhysicalDeviceHandle = PhysicalDevices[Index];
        uint32_t GraphicsQueueFamilyIndex;
        uint32_t TransferQueueFamilyIndex;
        if(Rr_CheckPhysicalDevice(
               PhysicalDeviceHandle,
               Surface,
               &GraphicsQueueFamilyIndex,
               &TransferQueueFamilyIndex,
               Arena))
        {
            VkPhysicalDeviceProperties2 Properties = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            };
            vkGetPhysicalDeviceProperties2(PhysicalDeviceHandle, &Properties);

            VkPhysicalDeviceMemoryProperties MemoryProperties = { 0 };
            vkGetPhysicalDeviceMemoryProperties(
                PhysicalDeviceHandle,
                &MemoryProperties);

            VkDeviceSize Memory = 0;
            for(uint32_t MemoryHeapIndex = 0;
                MemoryHeapIndex < MemoryProperties.memoryHeapCount;
                ++MemoryHeapIndex)
            {
                Memory += MemoryProperties.memoryHeaps[MemoryHeapIndex].size;
            }

            const char *TypeString = NULL;
            switch(Properties.properties.deviceType)
            {
                case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                    TypeString = "other";
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    TypeString = "integrated";
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    TypeString = "discrete";
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                    TypeString = "virtual";
                case VK_PHYSICAL_DEVICE_TYPE_CPU:
                    TypeString = "cpu";
                default:
                    TypeString = "undefined";
            }

            char *DstString = *RR_PUSH_SLICE(&DeviceStrings, Arena);
            snprintf(
                DstString,
                sizeof(Rr_DeviceString),
                "(\\) GPU #%d: %s, type: %s, total memory: %zu",
                Index,
                Properties.properties.deviceName,
                TypeString,
                Memory);

            if(BestDeviceIndex == UINT32_MAX)
            {
            SetBestDevice:
                BestDeviceIndex = Index;
                BestDeviceType = Properties.properties.deviceType;
                BestDeviceMemory = Memory;
                *OutGraphicsQueueFamilyIndex = GraphicsQueueFamilyIndex;
                *OutTransferQueueFamilyIndex = TransferQueueFamilyIndex;
            }
            else
            {
                if(Properties.properties.deviceType == PreferredDeviceType)
                {
                    if(BestDeviceType == PreferredDeviceType)
                    {
                        if(Memory > BestDeviceMemory)
                        {
                            goto SetBestDevice;
                        }
                    }
                    else
                    {
                        goto SetBestDevice;
                    }
                }
            }
        }
    }
    if(BestDeviceIndex == UINT32_MAX)
    {
        RR_LOG(
            "Could not select physical device based on the chosen properties!");
    }

    char *Mark;
    for(size_t Index = 0; Index < DeviceStrings.Count; ++Index)
    {
        Mark = strchr(DeviceStrings.Data[Index], '\\');
        if(Index == BestDeviceIndex)
        {
            *Mark = '*';
        }
        else
        {
            *Mark = ' ';
        }
        RR_LOG("%s", DeviceStrings.Data[Index]);
    }

    bool UseTransferQueue =
        *OutGraphicsQueueFamilyIndex != *OutTransferQueueFamilyIndex;

    Rr_PhysicalDevice PhysicalDevice = {
        .SubgroupProperties =
            (VkPhysicalDeviceSubgroupProperties){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
            },
        .Properties =
            (VkPhysicalDeviceProperties2){
                .pNext = &PhysicalDevice.SubgroupProperties,
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            },
        .Handle = PhysicalDevices[BestDeviceIndex],
    };

    vkGetPhysicalDeviceFeatures(
        PhysicalDevices[BestDeviceIndex],
        &PhysicalDevice.Features);
    vkGetPhysicalDeviceMemoryProperties(
        PhysicalDevices[BestDeviceIndex],
        &PhysicalDevice.MemoryProperties);
    vkGetPhysicalDeviceProperties2(
        PhysicalDevices[BestDeviceIndex],
        &PhysicalDevice.Properties);

    RR_LOG(
        "Using %s transfer queue.",
        UseTransferQueue ? "dedicated" : "unified");

    return PhysicalDevice;
}

void Rr_InitDeviceAndQueues(
    VkPhysicalDevice PhysicalDevice,
    uint32_t GraphicsQueueFamilyIndex,
    uint32_t TransferQueueFamilyIndex,
    VkDevice *OutDevice,
    VkQueue *OutGraphicsQueue,
    VkQueue *OutTransferQueue)
{
    bool UseTransferQueue =
        GraphicsQueueFamilyIndex != TransferQueueFamilyIndex;
    float QueuePriorities[] = { 1.0f };
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

    const char *DeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .queueCreateInfoCount = UseTransferQueue ? 2 : 1,
        .pQueueCreateInfos = QueueInfos,
        .enabledExtensionCount = SDL_arraysize(DeviceExtensions),
        .ppEnabledExtensionNames = DeviceExtensions,
    };

    vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, NULL, OutDevice);

    vkGetDeviceQueue(*OutDevice, GraphicsQueueFamilyIndex, 0, OutGraphicsQueue);
    if(UseTransferQueue)
    {
        vkGetDeviceQueue(
            *OutDevice,
            TransferQueueFamilyIndex,
            0,
            OutTransferQueue);
    }
}

void Rr_BlitColorImage(
    VkCommandBuffer CommandBuffer,
    VkImage Source,
    VkImage Destination,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    VkImageAspectFlags AspectMask)
{
    VkImageBlit ImageBlit = {
        .srcSubresource = {
            .aspectMask = AspectMask,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets = {
            { SrcRect.X, SrcRect.Y, 0, },
            { SrcRect.X + SrcRect.Width, SrcRect.Y + SrcRect.Height, 1, },
        },
        .dstSubresource = {
            .aspectMask = AspectMask,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets = {
            { DstRect.X, DstRect.Y, 0, },
            { DstRect.X + DstRect.Width, DstRect.Y + DstRect.Height, 1, },
        },
    };

    vkCmdBlitImage(
        CommandBuffer,
        Source,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        Destination,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &ImageBlit,
        VK_FILTER_NEAREST);
}
