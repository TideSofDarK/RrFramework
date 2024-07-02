#include "Rr_Vulkan.h"

#include "Rr_Log.h"
#include "Rr_Memory.h"

#include <SDL3/SDL.h>

#include <stdlib.h>

static Rr_Bool Rr_CheckPhysicalDevice(
    VkPhysicalDevice PhysicalDevice,
    VkSurfaceKHR Surface,
    uint32_t *OutGraphicsQueueFamilyIndex,
    uint32_t *OutTransferQueueFamilyIndex)
{
    uint32_t ExtensionCount;
    vkEnumerateDeviceExtensionProperties(
        PhysicalDevice,
        NULL,
        &ExtensionCount,
        NULL);
    if (ExtensionCount == 0)
    {
        return RR_FALSE;
    }

    char *TargetExtensions[] = {
        "VK_KHR_swapchain",
    };

    Rr_Bool FoundExtensions[] = { 0 };

    VkExtensionProperties *Extensions =
        Rr_StackAlloc(VkExtensionProperties, ExtensionCount);
    vkEnumerateDeviceExtensionProperties(
        PhysicalDevice,
        NULL,
        &ExtensionCount,
        Extensions);

    for (uint32_t Index = 0; Index < ExtensionCount; Index++)
    {
        for (uint32_t TargetIndex = 0;
             TargetIndex < SDL_arraysize(TargetExtensions);
             ++TargetIndex)
        {
            if (strcmp(
                    Extensions[Index].extensionName,
                    TargetExtensions[TargetIndex]) == 0)
            {
                FoundExtensions[TargetIndex] = RR_TRUE;
            }
        }
    }
    for (uint32_t TargetIndex = 0;
         TargetIndex < SDL_arraysize(TargetExtensions);
         ++TargetIndex)
    {
        if (!FoundExtensions[TargetIndex])
        {
            return RR_FALSE;
        }
    }

    uint32_t QueueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(
        PhysicalDevice,
        &QueueFamilyCount,
        NULL);
    if (QueueFamilyCount == 0)
    {
        return RR_FALSE;
    }

    VkQueueFamilyProperties *QueueFamilyProperties =
        Rr_StackAlloc(VkQueueFamilyProperties, QueueFamilyCount);
    VkBool32 *QueuePresentSupport = Rr_StackAlloc(VkBool32, QueueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(
        PhysicalDevice,
        &QueueFamilyCount,
        QueueFamilyProperties);

    uint32_t GraphicsQueueFamilyIndex = ~0U;
    uint32_t TransferQueueFamilyIndex = ~0U;

    for (uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(
            PhysicalDevice,
            Index,
            Surface,
            &QueuePresentSupport[Index]);
        if (QueuePresentSupport[Index] &&
            QueueFamilyProperties[Index].queueCount > 0 &&
            (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            GraphicsQueueFamilyIndex = Index;
            break;
        }
    }

    if (GraphicsQueueFamilyIndex == ~0U)
    {
        return RR_FALSE;
    }

    Rr_Bool bForceDisableTransferQueue = RR_FORCE_DISABLE_TRANSFER_QUEUE;

    if (!bForceDisableTransferQueue)
    {
        for (uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
        {
            if (Index == GraphicsQueueFamilyIndex)
            {
                continue;
            }

            if (QueueFamilyProperties[Index].queueCount > 0 &&
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

    Rr_StackFree(QueuePresentSupport);
    Rr_StackFree(QueueFamilyProperties);
    Rr_StackFree(Extensions);

    return RR_TRUE;
}

Rr_PhysicalDevice Rr_CreatePhysicalDevice(
    VkInstance Instance,
    VkSurfaceKHR Surface,
    uint32_t *OutGraphicsQueueFamilyIndex,
    uint32_t *OutTransferQueueFamilyIndex)
{
    Rr_PhysicalDevice PhysicalDevice = { 0 };

    uint32_t PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, NULL);
    if (PhysicalDeviceCount == 0)
    {
        Rr_LogAbort("No device with Vulkan support found");
    }

    VkPhysicalDevice *PhysicalDevices =
        Rr_StackAlloc(VkPhysicalDevice, PhysicalDeviceCount);
    vkEnumeratePhysicalDevices(
        Instance,
        &PhysicalDeviceCount,
        &PhysicalDevices[0]);

    PhysicalDevice.SubgroupProperties = (VkPhysicalDeviceSubgroupProperties){
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
        .pNext = NULL,
    };

    PhysicalDevice.Properties = (VkPhysicalDeviceProperties2){
        .pNext = &PhysicalDevice.SubgroupProperties,
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };

    Rr_Bool bUseTransferQueue = RR_FALSE;
    Rr_Bool bFoundSuitableDevice = RR_FALSE;
    for (uint32_t Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        VkPhysicalDevice PhysicalDeviceHandle = PhysicalDevices[Index];
        if (Rr_CheckPhysicalDevice(
                PhysicalDeviceHandle,
                Surface,
                OutGraphicsQueueFamilyIndex,
                OutTransferQueueFamilyIndex))
        {
            bUseTransferQueue =
                *OutGraphicsQueueFamilyIndex != *OutTransferQueueFamilyIndex;

            vkGetPhysicalDeviceFeatures(
                PhysicalDeviceHandle,
                &PhysicalDevice.Features);
            vkGetPhysicalDeviceMemoryProperties(
                PhysicalDeviceHandle,
                &PhysicalDevice.MemoryProperties);
            vkGetPhysicalDeviceProperties2(
                PhysicalDeviceHandle,
                &PhysicalDevice.Properties);

            Rr_LogVulkan(
                "Selected GPU: %s",
                PhysicalDevice.Properties.properties.deviceName);

            PhysicalDevice.Handle = PhysicalDeviceHandle;
            bFoundSuitableDevice = RR_TRUE;
            break;
        }
    }
    if (!bFoundSuitableDevice)
    {
        Rr_LogAbort(
            "Could not select physical device based on the chosen properties!");
    }

    Rr_LogVulkan(
        "Unified Queue Mode: %s",
        !bUseTransferQueue ? "RR_TRUE" : "RR_FALSE");

    Rr_StackFree(PhysicalDevices);

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
    Rr_Bool bUseTransferQueue =
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
        .queueCreateInfoCount = bUseTransferQueue ? 2 : 1,
        .pQueueCreateInfos = QueueInfos,
        .enabledExtensionCount = SDL_arraysize(DeviceExtensions),
        .ppEnabledExtensionNames = DeviceExtensions,
    };

    vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, NULL, OutDevice);

    vkGetDeviceQueue(*OutDevice, GraphicsQueueFamilyIndex, 0, OutGraphicsQueue);
    if (bUseTransferQueue)
    {
        vkGetDeviceQueue(
            *OutDevice,
            TransferQueueFamilyIndex,
            0,
            OutTransferQueue);
    }
}

void Rr_BlitPrerenderedDepth(
    VkCommandBuffer CommandBuffer,
    VkImage Source,
    VkImage Destination,
    VkExtent2D SrcSize,
    VkExtent2D DstSize)
{
    // VkImageBlit2 BlitRegion = {
    //     .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
    //     .pNext = NULL,
    //     .srcSubresource = {
    //         .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
    //         .mipLevel = 0,
    //         .baseArrayLayer = 0,
    //         .layerCount = 1,
    //     },
    //     .srcOffsets = { { 0 }, { (i32)SrcSize.width, (i32)SrcSize.height, 1 }
    //     }, .dstSubresource = {
    //         .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
    //         .mipLevel = 0,
    //         .baseArrayLayer = 0,
    //         .layerCount = 1,
    //     },
    //     .dstOffsets = { { 0 }, { (i32)DstSize.width, (i32)DstSize.height, 1 }
    //     },
    // };
    //
    //  VkBlitImageInfo2 BlitInfo = {
    //     .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
    //     .pNext = NULL,
    //     .srcImage = Source,
    //     .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    //     .dstImage = Destination,
    //     .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //     .regionCount = 1,
    //     .pRegions = &BlitRegion,
    //     .filter = VK_FILTER_NEAREST,
    // };
    //
    // vkCmdBlitImage2(CommandBuffer, &BlitInfo);

    abort();
}

void Rr_BlitColorImage(
    VkCommandBuffer CommandBuffer,
    VkImage Source,
    VkImage Destination,
    VkExtent2D SrcSize,
    VkExtent2D DstSize)
{
    VkImageBlit ImageBlit = {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets = {
            { 0 },
            { (int32_t)SrcSize.width, (int32_t)SrcSize.height, 1, },
        },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets = {
            { 0 },
            { (int32_t)DstSize.width, (int32_t)DstSize.height, 1, },
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
