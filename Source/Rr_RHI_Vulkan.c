/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "Rr_RHI_Vulkan.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_RHI
#include "Rr_LogMacro.h"

#include "Rr_App.h"
#include "Rr_Arena.h"
#include "Rr_Graph_Vulkan.h"
#include "Rr_Platform.h"
#include "Rr_SPIRV.h"
#include "Rr_Thread.h"

#include <Rr/Rr_Platform.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char const *RR_VULKAN_DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
#ifdef __APPLE__
    "VK_KHR_portability_subset",
#endif
};

void Rr_InitLoader(Rr_VulkanLoader *Loader)
{
    Loader->GetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)Rr_GetVkGetInstanceProcAddr();

    Loader->CreateInstance = (PFN_vkCreateInstance)Loader->GetInstanceProcAddr(
        NULL,
        "vkCreateInstance");
    Loader->EnumerateInstanceExtensionProperties =
        (PFN_vkEnumerateInstanceExtensionProperties)Loader->GetInstanceProcAddr(
            NULL,
            "vkEnumerateInstanceExtensionProperties");
    Loader->EnumerateInstanceLayerProperties =
        (PFN_vkEnumerateInstanceLayerProperties)Loader->GetInstanceProcAddr(
            NULL,
            "vkEnumerateInstanceLayerProperties");
}

void Rr_InitInstance(
    Rr_VulkanLoader *Loader,
    char const *ApplicationName,
    Rr_Instance *Instance)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkApplicationInfo ApplicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = ApplicationName,
        .pEngineName = "RrFramework",
        .apiVersion = RR_VULKAN_VERSION,
    };

    /* Gather required extensions. */

    char const *InstanceExtensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    uint32_t InstanceExtensionCount = RR_ARRAY_COUNT(InstanceExtensions);

#ifndef RR_USE_GPU_DEBUG_UTILS
    InstanceExtensionCount = 0;
#endif

    uint32_t PlatformExtensionCount;
    char const *const *PlatformExtensions =
        Rr_GetVulkanExtensions(&PlatformExtensionCount);

    uint32_t ExtensionCount = PlatformExtensionCount + InstanceExtensionCount;

    char const **Extensions =
        Rr_Alloc(sizeof(char const *) * (ExtensionCount + 1), Scratch.Arena);
    for (uint32_t Index = 0; Index < PlatformExtensionCount; Index++)
    {
        Extensions[Index] = PlatformExtensions[Index];
    }
    for (uint32_t Index = 0; Index < InstanceExtensionCount; Index++)
    {
        Extensions[Index + PlatformExtensionCount] = InstanceExtensions[Index];
    }

    /* Create Vulkan instance. */

    VkInstanceCreateInfo InstanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &ApplicationInfo,
        .enabledExtensionCount = ExtensionCount,
        .ppEnabledExtensionNames = Extensions,
    };

#ifdef __APPLE__
    bool PortabilityFound = false;
    for (uint32_t Index = 0; Index < ExtensionCount; ++Index)
    {
        if (strcmp(
                Extensions[Index],
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
        {
            PortabilityFound = true;
            break;
        }
    }
    if (!PortabilityFound)
    {
        Extensions[ExtensionCount] =
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        InstanceCreateInfo.enabledExtensionCount++;
    }
    InstanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkResult Result =
        Loader->CreateInstance(&InstanceCreateInfo, NULL, &Instance->Handle);
    if (Result != VK_SUCCESS)
    {
        RR_LOG_ABORT("Failed to create Vulkan instance!");
    }

    /* Vulkan 1.0 */

    Instance->CreateDevice = (PFN_vkCreateDevice)Loader->GetInstanceProcAddr(
        Instance->Handle,
        "vkCreateDevice");
    Instance->DestroyInstance =
        (PFN_vkDestroyInstance)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkDestroyInstance");
    Instance->EnumerateDeviceExtensionProperties =
        (PFN_vkEnumerateDeviceExtensionProperties)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkEnumerateDeviceExtensionProperties");
    Instance->EnumerateDeviceLayerProperties =
        (PFN_vkEnumerateDeviceLayerProperties)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkEnumerateDeviceLayerProperties");
    Instance->EnumeratePhysicalDevices =
        (PFN_vkEnumeratePhysicalDevices)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkEnumeratePhysicalDevices");
    Instance->GetDeviceProcAddr =
        (PFN_vkGetDeviceProcAddr)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetDeviceProcAddr");
    Instance->GetPhysicalDeviceFeatures =
        (PFN_vkGetPhysicalDeviceFeatures)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceFeatures");
    Instance->GetPhysicalDeviceFormatProperties =
        (PFN_vkGetPhysicalDeviceFormatProperties)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceFormatProperties");
    Instance->GetPhysicalDeviceImageFormatProperties =
        (PFN_vkGetPhysicalDeviceImageFormatProperties)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceImageFormatProperties");
    Instance->GetPhysicalDeviceMemoryProperties =
        (PFN_vkGetPhysicalDeviceMemoryProperties)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceMemoryProperties");
    Instance->GetPhysicalDeviceProperties =
        (PFN_vkGetPhysicalDeviceProperties)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceProperties");
    Instance->GetPhysicalDeviceQueueFamilyProperties =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceQueueFamilyProperties");
    Instance->GetPhysicalDeviceSparseImageFormatProperties =
        (PFN_vkGetPhysicalDeviceSparseImageFormatProperties)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceSparseImageFormatProperties");

    /* VK_KHR_surface */

    Instance->DestroySurfaceKHR =
        (PFN_vkDestroySurfaceKHR)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkDestroySurfaceKHR");
    Instance->GetPhysicalDeviceSurfaceCapabilitiesKHR =
        (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    Instance->GetPhysicalDeviceSurfaceFormatsKHR =
        (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceSurfaceFormatsKHR");
    Instance->GetPhysicalDeviceSurfacePresentModesKHR =
        (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceSurfacePresentModesKHR");
    Instance->GetPhysicalDeviceSurfaceSupportKHR =
        (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceSurfaceSupportKHR");

#ifdef RR_USE_GPU_DEBUG_UTILS
    /* VK_EXT_debug_utils */

    Instance->CmdBeginDebugUtilsLabelEXT =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkCmdBeginDebugUtilsLabelEXT");
    Instance->CmdEndDebugUtilsLabelEXT =
        (PFN_vkCmdEndDebugUtilsLabelEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkCmdEndDebugUtilsLabelEXT");
    Instance->CmdInsertDebugUtilsLabelEXT =
        (PFN_vkCmdInsertDebugUtilsLabelEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkCmdInsertDebugUtilsLabelEXT");
    Instance->CreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkCreateDebugUtilsMessengerEXT");
    Instance->DestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkDestroyDebugUtilsMessengerEXT");
    Instance->QueueBeginDebugUtilsLabelEXT =
        (PFN_vkQueueBeginDebugUtilsLabelEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkQueueBeginDebugUtilsLabelEXT");
    Instance->QueueEndDebugUtilsLabelEXT =
        (PFN_vkQueueEndDebugUtilsLabelEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkQueueEndDebugUtilsLabelEXT");
    Instance->QueueInsertDebugUtilsLabelEXT =
        (PFN_vkQueueInsertDebugUtilsLabelEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkQueueInsertDebugUtilsLabelEXT");
    Instance->SetDebugUtilsObjectNameEXT =
        (PFN_vkSetDebugUtilsObjectNameEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkSetDebugUtilsObjectNameEXT");
    Instance->SetDebugUtilsObjectTagEXT =
        (PFN_vkSetDebugUtilsObjectTagEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkSetDebugUtilsObjectTagEXT");
    Instance->SubmitDebugUtilsMessageEXT =
        (PFN_vkSubmitDebugUtilsMessageEXT)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkSubmitDebugUtilsMessageEXT");
#endif

    Rr_DestroyScratch(Scratch);
}

static bool Rr_CheckPhysicalDevice(
    Rr_Instance *Instance,
    VkPhysicalDevice PhysicalDevice,
    VkSurfaceKHR Surface,
    uint32_t *OutGraphicsQueueFamilyIndex,
    VkQueueFamilyProperties *OutGraphicsQueueFamilyProperties,
    uint32_t *OutTransferQueueFamilyIndex,
    VkQueueFamilyProperties *OutTransferQueueFamilyProperties,
    Rr_Arena *Arena)
{
    VkPhysicalDeviceFeatures Features;
    Instance->GetPhysicalDeviceFeatures(PhysicalDevice, &Features);

    if (!Features.drawIndirectFirstInstance || !Features.samplerAnisotropy ||
        !Features.independentBlend || !Features.imageCubeArray ||
        !Features.depthBiasClamp)
    {
        return false;
    }

    uint32_t ExtensionCount;
    Instance->EnumerateDeviceExtensionProperties(
        PhysicalDevice,
        NULL,
        &ExtensionCount,
        NULL);

    bool *FoundExtensions = Rr_Alloc(
        sizeof(bool) * RR_ARRAY_COUNT(RR_VULKAN_DEVICE_EXTENSIONS),
        Arena);

    VkExtensionProperties *Extensions =
        Rr_Alloc(sizeof(VkExtensionProperties) * ExtensionCount, Arena);
    Instance->EnumerateDeviceExtensionProperties(
        PhysicalDevice,
        NULL,
        &ExtensionCount,
        Extensions);

    for (uint32_t Index = 0; Index < ExtensionCount; Index++)
    {
        for (uint32_t TargetIndex = 0;
             TargetIndex < RR_ARRAY_COUNT(RR_VULKAN_DEVICE_EXTENSIONS);
             ++TargetIndex)
        {
            if (strcmp(
                    Extensions[Index].extensionName,
                    RR_VULKAN_DEVICE_EXTENSIONS[TargetIndex]) == 0)
            {
                FoundExtensions[TargetIndex] = true;
            }
        }
    }
    for (uint32_t TargetIndex = 0;
         TargetIndex < RR_ARRAY_COUNT(RR_VULKAN_DEVICE_EXTENSIONS);
         ++TargetIndex)
    {
        if (!FoundExtensions[TargetIndex])
        {
            return false;
        }
    }

    uint32_t QueueFamilyCount;
    Instance->GetPhysicalDeviceQueueFamilyProperties(
        PhysicalDevice,
        &QueueFamilyCount,
        NULL);
    if (QueueFamilyCount == 0)
    {
        return false;
    }

    VkQueueFamilyProperties *QueueFamilyProperties =
        Rr_Alloc(sizeof(VkQueueFamilyProperties) * QueueFamilyCount, Arena);
    VkBool32 *QueuePresentSupport =
        Rr_Alloc(sizeof(VkBool32) * QueueFamilyCount, Arena);

    Instance->GetPhysicalDeviceQueueFamilyProperties(
        PhysicalDevice,
        &QueueFamilyCount,
        QueueFamilyProperties);

    uint32_t GraphicsQueueFamilyIndex = ~0U;
    uint32_t TransferQueueFamilyIndex = ~0U;

    for (uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
    {
        Instance->GetPhysicalDeviceSurfaceSupportKHR(
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
        return false;
    }

#ifndef RR_DISABLE_TRANSFER_QUEUE
    for (uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
    {
        if (Index == GraphicsQueueFamilyIndex)
        {
            continue;
        }

        if (QueueFamilyProperties[Index].queueCount > 0 &&
            (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            TransferQueueFamilyIndex = Index;
            break;
        }
    }
#endif

    *OutGraphicsQueueFamilyIndex = GraphicsQueueFamilyIndex;
    *OutGraphicsQueueFamilyProperties =
        QueueFamilyProperties[GraphicsQueueFamilyIndex];
    *OutTransferQueueFamilyIndex = TransferQueueFamilyIndex == ~0U
                                       ? GraphicsQueueFamilyIndex
                                       : TransferQueueFamilyIndex;
    *OutTransferQueueFamilyProperties =
        QueueFamilyProperties[*OutTransferQueueFamilyIndex];

    return true;
}

void Rr_SelectPhysicalDevice(
    Rr_Instance *Instance,
    VkSurfaceKHR Surface,
    Rr_PhysicalDevice *PhysicalDevice,
    Rr_Queue *GraphicsQueue,
    Rr_Queue *TransferQueue,
    Rr_Arena *Arena)
{
    uint32_t PhysicalDeviceCount = 0;
    Instance->EnumeratePhysicalDevices(
        Instance->Handle,
        &PhysicalDeviceCount,
        NULL);
    if (PhysicalDeviceCount == 0)
    {
        RR_LOG_ABORT("No device with Vulkan support found");
    }

    VkPhysicalDevice *PhysicalDevices =
        Rr_Alloc(sizeof(VkPhysicalDevice) * PhysicalDeviceCount, Arena);
    Instance->EnumeratePhysicalDevices(
        Instance->Handle,
        &PhysicalDeviceCount,
        &PhysicalDevices[0]);

    RR_LOG_TRACE("Selecting Vulkan device:");

    typedef char Rr_DeviceString[1024];
    RR_ARRAY(Rr_DeviceString) DeviceStrings = { 0 };
    uint32_t BestDeviceIndex = UINT32_MAX;
    static VkPhysicalDeviceType const PREFERRED_DEVICE_TYPE =
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    uint32_t BestDeviceType = 0;
    VkDeviceSize BestDeviceMemory = 0;
    for (uint32_t Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        VkPhysicalDevice PhysicalDeviceHandle = PhysicalDevices[Index];
        uint32_t GraphicsQueueFamilyIndex;
        VkQueueFamilyProperties GraphicsQueueFamilyProperties;
        uint32_t TransferQueueFamilyIndex;
        VkQueueFamilyProperties TransferQueueFamilyProperties;
        if (Rr_CheckPhysicalDevice(
                Instance,
                PhysicalDeviceHandle,
                Surface,
                &GraphicsQueueFamilyIndex,
                &GraphicsQueueFamilyProperties,
                &TransferQueueFamilyIndex,
                &TransferQueueFamilyProperties,
                Arena))
        {
            VkPhysicalDeviceProperties Properties;
            Instance->GetPhysicalDeviceProperties(
                PhysicalDeviceHandle,
                &Properties);

            VkPhysicalDeviceMemoryProperties MemoryProperties = { 0 };
            Instance->GetPhysicalDeviceMemoryProperties(
                PhysicalDeviceHandle,
                &MemoryProperties);

            unsigned long long Memory = 0;
            for (uint32_t MemoryHeapIndex = 0;
                 MemoryHeapIndex < MemoryProperties.memoryHeapCount;
                 ++MemoryHeapIndex)
            {
                Memory += MemoryProperties.memoryHeaps[MemoryHeapIndex].size;
            }

            char const *TypeString = NULL;
            switch (Properties.deviceType)
            {
                case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                    TypeString = "other";
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    TypeString = "integrated";
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    TypeString = "discrete";
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                    TypeString = "virtual";
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_CPU:
                    TypeString = "cpu";
                    break;
                default:
                    TypeString = "undefined";
                    break;
            }

            char *DstString = *RR_PUSH_INTO_ARRAY(&DeviceStrings, Arena);
            snprintf(
                DstString,
                sizeof(Rr_DeviceString),
                "(\\) GPU #%d: %s, type: %s, total memory: %llu",
                Index,
                Properties.deviceName,
                TypeString,
                Memory);

            if (BestDeviceIndex == UINT32_MAX)
            {
            SetBestDevice:
                BestDeviceIndex = Index;
                BestDeviceType = Properties.deviceType;
                BestDeviceMemory = Memory;
                GraphicsQueue->FamilyIndex = GraphicsQueueFamilyIndex;
                GraphicsQueue->FamilyProperties = GraphicsQueueFamilyProperties;
                TransferQueue->FamilyIndex = TransferQueueFamilyIndex;
                TransferQueue->FamilyProperties = TransferQueueFamilyProperties;
            }
            else
            {
                if (Properties.deviceType == PREFERRED_DEVICE_TYPE)
                {
                    if (BestDeviceType == PREFERRED_DEVICE_TYPE)
                    {
                        if (Memory > BestDeviceMemory)
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
    if (BestDeviceIndex == UINT32_MAX)
    {
        RR_LOG_ABORT(
            "Could not select physical device based on the chosen properties!");
    }

    char *Mark;
    for (size_t Index = 0; Index < DeviceStrings.Count; ++Index)
    {
        Mark = strchr(DeviceStrings.Data[Index], '\\');
        if (Index == BestDeviceIndex)
        {
            *Mark = '*';
        }
        else
        {
            *Mark = ' ';
        }
        RR_LOG_INFO("%s", DeviceStrings.Data[Index]);
    }

    bool UseTransferQueue =
        GraphicsQueue->FamilyIndex != TransferQueue->FamilyIndex;

    PhysicalDevice->Handle = PhysicalDevices[BestDeviceIndex];

    Instance->GetPhysicalDeviceFeatures(
        PhysicalDevices[BestDeviceIndex],
        &PhysicalDevice->Features);
    Instance->GetPhysicalDeviceMemoryProperties(
        PhysicalDevices[BestDeviceIndex],
        &PhysicalDevice->MemoryProperties);
    Instance->GetPhysicalDeviceProperties(
        PhysicalDevices[BestDeviceIndex],
        &PhysicalDevice->Properties);

    /* NOTE: timestampComputeAndGraphics guarantees that compute and graphics
     * queues do support timestamps. */
    GraphicsQueue->TimestampsEnabled =
        PhysicalDevice->Properties.limits.timestampPeriod > 0.0f &&
        PhysicalDevice->Properties.limits.timestampComputeAndGraphics;

    RR_LOG_INFO("Using transfer queue: %d", UseTransferQueue);
}

void Rr_InitSurface(Rr_Instance *Instance, VkSurfaceKHR *Surface)
{
    if (!Rr_CreateVulkanSurface((uint64_t)Instance->Handle, (void *)Surface))
    {
        RR_LOG_ABORT("Failed to create Vulkan surface!");
    }
}

void Rr_InitDeviceAndQueues(
    Rr_Instance *Instance,
    VkSurfaceKHR Surface,
    Rr_PhysicalDevice *PhysicalDevice,
    Rr_Device *Device,
    Rr_Queue *GraphicsQueue,
    Rr_Queue *TransferQueue)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_SelectPhysicalDevice(
        Instance,
        Surface,
        PhysicalDevice,
        GraphicsQueue,
        TransferQueue,
        Scratch.Arena);

    bool UseTransferQueue =
        GraphicsQueue->FamilyIndex != TransferQueue->FamilyIndex;
    float QueuePriorities[] = { 1.0f };
    VkDeviceQueueCreateInfo QueueInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = GraphicsQueue->FamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = QueuePriorities,
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = TransferQueue->FamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = QueuePriorities,
        }
    };

    VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .queueCreateInfoCount = UseTransferQueue ? 2 : 1,
        .pQueueCreateInfos = QueueInfos,
        .enabledExtensionCount = RR_ARRAY_COUNT(RR_VULKAN_DEVICE_EXTENSIONS),
        .ppEnabledExtensionNames = RR_VULKAN_DEVICE_EXTENSIONS,
        .pEnabledFeatures = &PhysicalDevice->Features,
    };

    Instance->CreateDevice(
        PhysicalDevice->Handle,
        &DeviceCreateInfo,
        NULL,
        &Device->Handle);

    /* Vulkan 1.0 */

    Device->AllocateCommandBuffers =
        (PFN_vkAllocateCommandBuffers)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkAllocateCommandBuffers");
    Device->AllocateDescriptorSets =
        (PFN_vkAllocateDescriptorSets)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkAllocateDescriptorSets");
    Device->AllocateMemory = (PFN_vkAllocateMemory)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkAllocateMemory");
    Device->BeginCommandBuffer =
        (PFN_vkBeginCommandBuffer)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkBeginCommandBuffer");
    Device->BindBufferMemory =
        (PFN_vkBindBufferMemory)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkBindBufferMemory");
    Device->BindImageMemory =
        (PFN_vkBindImageMemory)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkBindImageMemory");
    Device->CmdBeginQuery = (PFN_vkCmdBeginQuery)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdBeginQuery");
    Device->CmdBeginRenderPass =
        (PFN_vkCmdBeginRenderPass)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdBeginRenderPass");
    Device->CmdBindDescriptorSets =
        (PFN_vkCmdBindDescriptorSets)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdBindDescriptorSets");
    Device->CmdBindIndexBuffer =
        (PFN_vkCmdBindIndexBuffer)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdBindIndexBuffer");
    Device->CmdBindPipeline =
        (PFN_vkCmdBindPipeline)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdBindPipeline");
    Device->CmdBindVertexBuffers =
        (PFN_vkCmdBindVertexBuffers)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdBindVertexBuffers");
    Device->CmdBlitImage = (PFN_vkCmdBlitImage)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdBlitImage");
    Device->CmdClearAttachments =
        (PFN_vkCmdClearAttachments)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdClearAttachments");
    Device->CmdClearColorImage =
        (PFN_vkCmdClearColorImage)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdClearColorImage");
    Device->CmdClearDepthStencilImage =
        (PFN_vkCmdClearDepthStencilImage)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdClearDepthStencilImage");
    Device->CmdCopyBuffer = (PFN_vkCmdCopyBuffer)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdCopyBuffer");
    Device->CmdCopyBufferToImage =
        (PFN_vkCmdCopyBufferToImage)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdCopyBufferToImage");
    Device->CmdCopyImage = (PFN_vkCmdCopyImage)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdCopyImage");
    Device->CmdCopyImageToBuffer =
        (PFN_vkCmdCopyImageToBuffer)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdCopyImageToBuffer");
    Device->CmdCopyQueryPoolResults =
        (PFN_vkCmdCopyQueryPoolResults)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdCopyQueryPoolResults");
    Device->CmdDispatch = (PFN_vkCmdDispatch)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdDispatch");
    Device->CmdDispatchIndirect =
        (PFN_vkCmdDispatchIndirect)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdDispatchIndirect");
    Device->CmdDraw =
        (PFN_vkCmdDraw)Instance->GetDeviceProcAddr(Device->Handle, "vkCmdDraw");
    Device->CmdDrawIndexed = (PFN_vkCmdDrawIndexed)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdDrawIndexed");
    Device->CmdDrawIndexedIndirect =
        (PFN_vkCmdDrawIndexedIndirect)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdDrawIndexedIndirect");
    Device->CmdDrawIndirect =
        (PFN_vkCmdDrawIndirect)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdDrawIndirect");
    Device->CmdEndQuery = (PFN_vkCmdEndQuery)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdEndQuery");
    Device->CmdEndRenderPass =
        (PFN_vkCmdEndRenderPass)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdEndRenderPass");
    Device->CmdExecuteCommands =
        (PFN_vkCmdExecuteCommands)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdExecuteCommands");
    Device->CmdFillBuffer = (PFN_vkCmdFillBuffer)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdFillBuffer");
    Device->CmdNextSubpass = (PFN_vkCmdNextSubpass)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdNextSubpass");
    Device->CmdPipelineBarrier =
        (PFN_vkCmdPipelineBarrier)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdPipelineBarrier");
    Device->CmdPushConstants =
        (PFN_vkCmdPushConstants)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdPushConstants");
    Device->CmdResetEvent = (PFN_vkCmdResetEvent)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdResetEvent");
    Device->CmdResetQueryPool =
        (PFN_vkCmdResetQueryPool)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdResetQueryPool");
    Device->CmdResolveImage =
        (PFN_vkCmdResolveImage)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdResolveImage");
    Device->CmdSetBlendConstants =
        (PFN_vkCmdSetBlendConstants)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdSetBlendConstants");
    Device->CmdSetDepthBias =
        (PFN_vkCmdSetDepthBias)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdSetDepthBias");
    Device->CmdSetDepthBounds =
        (PFN_vkCmdSetDepthBounds)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdSetDepthBounds");
    Device->CmdSetEvent = (PFN_vkCmdSetEvent)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdSetEvent");
    Device->CmdSetLineWidth =
        (PFN_vkCmdSetLineWidth)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdSetLineWidth");
    Device->CmdSetScissor = (PFN_vkCmdSetScissor)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdSetScissor");
    Device->CmdSetStencilCompareMask =
        (PFN_vkCmdSetStencilCompareMask)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdSetStencilCompareMask");
    Device->CmdSetStencilReference =
        (PFN_vkCmdSetStencilReference)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdSetStencilReference");
    Device->CmdSetStencilWriteMask =
        (PFN_vkCmdSetStencilWriteMask)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdSetStencilWriteMask");
    Device->CmdSetViewport = (PFN_vkCmdSetViewport)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdSetViewport");
    Device->CmdUpdateBuffer =
        (PFN_vkCmdUpdateBuffer)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdUpdateBuffer");
    Device->CmdWaitEvents = (PFN_vkCmdWaitEvents)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCmdWaitEvents");
    Device->CmdWriteTimestamp =
        (PFN_vkCmdWriteTimestamp)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCmdWriteTimestamp");
    Device->CreateBuffer = (PFN_vkCreateBuffer)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCreateBuffer");
    Device->CreateBufferView =
        (PFN_vkCreateBufferView)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateBufferView");
    Device->CreateCommandPool =
        (PFN_vkCreateCommandPool)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateCommandPool");
    Device->CreateComputePipelines =
        (PFN_vkCreateComputePipelines)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateComputePipelines");
    Device->CreateDescriptorPool =
        (PFN_vkCreateDescriptorPool)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateDescriptorPool");
    Device->CreateDescriptorSetLayout =
        (PFN_vkCreateDescriptorSetLayout)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateDescriptorSetLayout");
    Device->CreateEvent = (PFN_vkCreateEvent)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCreateEvent");
    Device->CreateFence = (PFN_vkCreateFence)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCreateFence");
    Device->CreateFramebuffer =
        (PFN_vkCreateFramebuffer)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateFramebuffer");
    Device->CreateGraphicsPipelines =
        (PFN_vkCreateGraphicsPipelines)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateGraphicsPipelines");
    Device->CreateImage = (PFN_vkCreateImage)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCreateImage");
    Device->CreateImageView =
        (PFN_vkCreateImageView)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateImageView");
    Device->CreatePipelineCache =
        (PFN_vkCreatePipelineCache)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreatePipelineCache");
    Device->CreatePipelineLayout =
        (PFN_vkCreatePipelineLayout)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreatePipelineLayout");
    Device->CreateQueryPool =
        (PFN_vkCreateQueryPool)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateQueryPool");
    Device->CreateRenderPass =
        (PFN_vkCreateRenderPass)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateRenderPass");
    Device->CreateSampler = (PFN_vkCreateSampler)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkCreateSampler");
    Device->CreateSemaphore =
        (PFN_vkCreateSemaphore)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateSemaphore");
    Device->CreateShaderModule =
        (PFN_vkCreateShaderModule)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateShaderModule");
    Device->DestroyBuffer = (PFN_vkDestroyBuffer)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkDestroyBuffer");
    Device->DestroyBufferView =
        (PFN_vkDestroyBufferView)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyBufferView");
    Device->DestroyCommandPool =
        (PFN_vkDestroyCommandPool)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyCommandPool");
    Device->DestroyDescriptorPool =
        (PFN_vkDestroyDescriptorPool)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyDescriptorPool");
    Device->DestroyDescriptorSetLayout =
        (PFN_vkDestroyDescriptorSetLayout)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyDescriptorSetLayout");
    Device->DestroyDevice = (PFN_vkDestroyDevice)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkDestroyDevice");
    Device->DestroyEvent = (PFN_vkDestroyEvent)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkDestroyEvent");
    Device->DestroyFence = (PFN_vkDestroyFence)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkDestroyFence");
    Device->DestroyFramebuffer =
        (PFN_vkDestroyFramebuffer)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyFramebuffer");
    Device->DestroyImage = (PFN_vkDestroyImage)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkDestroyImage");
    Device->DestroyImageView =
        (PFN_vkDestroyImageView)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyImageView");
    Device->DestroyPipeline =
        (PFN_vkDestroyPipeline)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyPipeline");
    Device->DestroyPipelineCache =
        (PFN_vkDestroyPipelineCache)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyPipelineCache");
    Device->DestroyPipelineLayout =
        (PFN_vkDestroyPipelineLayout)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyPipelineLayout");
    Device->DestroyQueryPool =
        (PFN_vkDestroyQueryPool)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyQueryPool");
    Device->DestroyRenderPass =
        (PFN_vkDestroyRenderPass)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyRenderPass");
    Device->DestroySampler = (PFN_vkDestroySampler)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkDestroySampler");
    Device->DestroySemaphore =
        (PFN_vkDestroySemaphore)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroySemaphore");
    Device->DestroyShaderModule =
        (PFN_vkDestroyShaderModule)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroyShaderModule");
    Device->DeviceWaitIdle = (PFN_vkDeviceWaitIdle)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkDeviceWaitIdle");
    Device->EndCommandBuffer =
        (PFN_vkEndCommandBuffer)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkEndCommandBuffer");
    Device->FlushMappedMemoryRanges =
        (PFN_vkFlushMappedMemoryRanges)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkFlushMappedMemoryRanges");
    Device->FreeCommandBuffers =
        (PFN_vkFreeCommandBuffers)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkFreeCommandBuffers");
    Device->FreeDescriptorSets =
        (PFN_vkFreeDescriptorSets)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkFreeDescriptorSets");
    Device->FreeMemory = (PFN_vkFreeMemory)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkFreeMemory");
    Device->GetBufferMemoryRequirements =
        (PFN_vkGetBufferMemoryRequirements)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkGetBufferMemoryRequirements");
    Device->GetDeviceMemoryCommitment =
        (PFN_vkGetDeviceMemoryCommitment)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkGetDeviceMemoryCommitment");
    Device->GetDeviceQueue = (PFN_vkGetDeviceQueue)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkGetDeviceQueue");
    Device->GetEventStatus = (PFN_vkGetEventStatus)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkGetEventStatus");
    Device->GetFenceStatus = (PFN_vkGetFenceStatus)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkGetFenceStatus");
    Device->GetImageMemoryRequirements =
        (PFN_vkGetImageMemoryRequirements)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkGetImageMemoryRequirements");
    Device->GetImageSparseMemoryRequirements =
        (PFN_vkGetImageSparseMemoryRequirements)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkGetImageSparseMemoryRequirements");
    Device->GetImageSubresourceLayout =
        (PFN_vkGetImageSubresourceLayout)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkGetImageSubresourceLayout");
    Device->GetPipelineCacheData =
        (PFN_vkGetPipelineCacheData)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkGetPipelineCacheData");
    Device->GetQueryPoolResults =
        (PFN_vkGetQueryPoolResults)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkGetQueryPoolResults");
    Device->GetRenderAreaGranularity =
        (PFN_vkGetRenderAreaGranularity)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkGetRenderAreaGranularity");
    Device->InvalidateMappedMemoryRanges =
        (PFN_vkInvalidateMappedMemoryRanges)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkInvalidateMappedMemoryRanges");
    Device->MapMemory = (PFN_vkMapMemory)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkMapMemory");
    Device->MergePipelineCaches =
        (PFN_vkMergePipelineCaches)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkMergePipelineCaches");
    Device->QueueBindSparse =
        (PFN_vkQueueBindSparse)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkQueueBindSparse");
    Device->QueueSubmit = (PFN_vkQueueSubmit)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkQueueSubmit");
    Device->QueueWaitIdle = (PFN_vkQueueWaitIdle)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkQueueWaitIdle");
    Device->ResetCommandBuffer =
        (PFN_vkResetCommandBuffer)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkResetCommandBuffer");
    Device->ResetCommandPool =
        (PFN_vkResetCommandPool)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkResetCommandPool");
    Device->ResetDescriptorPool =
        (PFN_vkResetDescriptorPool)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkResetDescriptorPool");
    Device->ResetEvent = (PFN_vkResetEvent)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkResetEvent");
    Device->ResetFences = (PFN_vkResetFences)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkResetFences");
    Device->SetEvent = (PFN_vkSetEvent)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkSetEvent");
    Device->UnmapMemory = (PFN_vkUnmapMemory)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkUnmapMemory");
    Device->UpdateDescriptorSets =
        (PFN_vkUpdateDescriptorSets)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkUpdateDescriptorSets");
    Device->WaitForFences = (PFN_vkWaitForFences)Instance->GetDeviceProcAddr(
        Device->Handle,
        "vkWaitForFences");

    /* VK_KHR_swapchain */

    Device->AcquireNextImageKHR =
        (PFN_vkAcquireNextImageKHR)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkAcquireNextImageKHR");
    Device->CreateSwapchainKHR =
        (PFN_vkCreateSwapchainKHR)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkCreateSwapchainKHR");
    Device->DestroySwapchainKHR =
        (PFN_vkDestroySwapchainKHR)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkDestroySwapchainKHR");
    Device->GetSwapchainImagesKHR =
        (PFN_vkGetSwapchainImagesKHR)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkGetSwapchainImagesKHR");
    Device->QueuePresentKHR =
        (PFN_vkQueuePresentKHR)Instance->GetDeviceProcAddr(
            Device->Handle,
            "vkQueuePresentKHR");

    Device->GetDeviceQueue(
        Device->Handle,
        GraphicsQueue->FamilyIndex,
        0,
        &GraphicsQueue->Handle);
    if (UseTransferQueue)
    {
        Device->GetDeviceQueue(
            Device->Handle,
            TransferQueue->FamilyIndex,
            0,
            &TransferQueue->Handle);
    }

    Rr_DestroyScratch(Scratch);
}

static inline void Rr_DestroySwapchainImage(Rr_SwapchainImage *SwapchainImage)
{
    Rr_AllocatedImage *AllocatedImage =
        SwapchainImage->Container.AllocatedImages;

    if (AllocatedImage->ImageViewMap)
    {
        Rr_DestroyImageViewMap(AllocatedImage->ImageViewMap, true);
    }

    if (SwapchainImage->EarlySemaphore)
    {
        Rr_ReleaseVulkanSemaphore(SwapchainImage->EarlySemaphore);
    }

    if (SwapchainImage->LateSemaphore)
    {
        Rr_ReleaseVulkanSemaphore(SwapchainImage->LateSemaphore);
    }

    RR_ZERO_PTR(SwapchainImage);
}

static bool Rr_InitSwapchain(void)
{
    Rr_WaitIdle();

    Rr_Instance *Instance = &gRHI->Instance;
    Rr_Device *Device = Rr_GetDevice();

    Rr_IntVec2 WindowSize = Rr_GetWindowSize();

    for (size_t Index = 0; Index < gRHI->SwapchainImages.Count; ++Index)
    {
        Rr_DestroySwapchainImage(gRHI->SwapchainImages.Data + Index);
    }
    RR_CLEAR_ARRAY(&gRHI->SwapchainImages);

    VkSwapchainKHR OldSwapchain = gRHI->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfaceCapabilities;
    Instance->GetPhysicalDeviceSurfaceCapabilitiesKHR(
        gRHI->PhysicalDevice.Handle,
        gRHI->Surface,
        &SurfaceCapabilities);

    if (SurfaceCapabilities.currentExtent.width == 0 ||
        SurfaceCapabilities.currentExtent.height == 0)
    {
        return false;
    }
    if (SurfaceCapabilities.currentExtent.width == UINT32_MAX)
    {
        gRHI->Swapchain.Extent.width = (uint32_t)WindowSize.Width;
        gRHI->Swapchain.Extent.height = (uint32_t)WindowSize.Height;
    }
    else
    {
        gRHI->Swapchain.Extent.width = SurfaceCapabilities.currentExtent.width;
        gRHI->Swapchain.Extent.height =
            SurfaceCapabilities.currentExtent.height;
    }
    gRHI->Swapchain.Extent.depth = 1;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    uint32_t VulkanPresentModeCount = 0;
    Instance->GetPhysicalDeviceSurfacePresentModesKHR(
        gRHI->PhysicalDevice.Handle,
        gRHI->Surface,
        &VulkanPresentModeCount,
        NULL);
    assert(VulkanPresentModeCount > 0);

    VkPresentModeKHR *VulkanPresentModes = Rr_Alloc(
        sizeof(VkPresentModeKHR) * VulkanPresentModeCount,
        Scratch.Arena);
    Instance->GetPhysicalDeviceSurfacePresentModesKHR(
        gRHI->PhysicalDevice.Handle,
        gRHI->Surface,
        &VulkanPresentModeCount,
        VulkanPresentModes);

    VkPresentModeKHR DesiredVulkanPresentMode =
        Rr_ToVulkanPresentMode(gRHI->Swapchain.PresentMode);
    bool VulkanPresentModeAvailable = false;
    gRHI->Swapchain.PresentModeCount = 0;
    for (uint32_t Index = 0; Index < VulkanPresentModeCount &&
                             gRHI->Swapchain.PresentModeCount <
                                 RR_ARRAY_COUNT(gRHI->Swapchain.PresentModes);
         Index++)
    {
        if (VulkanPresentModes[Index] <= VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        {
            gRHI->Swapchain.PresentModes[gRHI->Swapchain.PresentModeCount++] =
                Rr_ToPresentMode(VulkanPresentModes[Index]);
            if (VulkanPresentModes[Index] == DesiredVulkanPresentMode)
            {
                VulkanPresentModeAvailable = true;
            }
        }
    }
    if (VulkanPresentModeAvailable == false)
    {
        DesiredVulkanPresentMode = VulkanPresentModes[0];
        gRHI->Swapchain.PresentMode =
            Rr_ToPresentMode(DesiredVulkanPresentMode);
    }

    uint32_t DesiredNumberOfSwapchainImages =
        RR_MAX(SurfaceCapabilities.minImageCount, 3);
    if (SurfaceCapabilities.maxImageCount > 0)
    {
        DesiredNumberOfSwapchainImages = RR_MIN(
            DesiredNumberOfSwapchainImages,
            SurfaceCapabilities.maxImageCount);
    }

    VkSurfaceTransformFlagBitsKHR PreTransform;
    if (SurfaceCapabilities.supportedTransforms &
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        PreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        PreTransform = SurfaceCapabilities.currentTransform;
    }

    uint32_t FormatCount;
    Instance->GetPhysicalDeviceSurfaceFormatsKHR(
        gRHI->PhysicalDevice.Handle,
        gRHI->Surface,
        &FormatCount,
        NULL);
    assert(FormatCount > 0);

    VkSurfaceFormatKHR *SurfaceFormats =
        Rr_Alloc(sizeof(VkSurfaceFormatKHR) * FormatCount, Scratch.Arena);
    Instance->GetPhysicalDeviceSurfaceFormatsKHR(
        gRHI->PhysicalDevice.Handle,
        gRHI->Surface,
        &FormatCount,
        SurfaceFormats);

    VkSurfaceFormatKHR *PreferredFormat = NULL;
    VkSurfaceFormatKHR *FallbackFormat = SurfaceFormats;
    for (uint32_t Index = 0; Index < FormatCount; Index++)
    {
        VkSurfaceFormatKHR *SurfaceFormat = &SurfaceFormats[Index];

        if (SurfaceFormat->format == VK_FORMAT_B8G8R8A8_SRGB ||
            SurfaceFormat->format == VK_FORMAT_R8G8B8A8_SRGB ||
            SurfaceFormat->format == VK_FORMAT_A8B8G8R8_SRGB_PACK32)
        {
            PreferredFormat = SurfaceFormat;
            break;
        }
    }
    VkSurfaceFormatKHR *SelectedFormat =
        PreferredFormat ? PreferredFormat : FallbackFormat;
    if (!SelectedFormat)
    {
        RR_LOG_ABORT("No suitable surface format found!");
    }
    gRHI->Swapchain.Format = SelectedFormat->format;
    gRHI->Swapchain.ColorSpace = SelectedFormat->colorSpace;

    VkCompositeAlphaFlagBitsKHR CompositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR CompositeAlphaFlags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (uint32_t Index = 0; Index < RR_ARRAY_COUNT(CompositeAlphaFlags);
         Index++)
    {
        VkCompositeAlphaFlagBitsKHR CompositeAlphaFlag =
            CompositeAlphaFlags[Index];
        if (SurfaceCapabilities.supportedCompositeAlpha & CompositeAlphaFlag)
        {
            CompositeAlpha = CompositeAlphaFlag;
            break;
        }
    }

    VkSwapchainCreateInfoKHR SwapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = gRHI->Surface,
        .minImageCount = DesiredNumberOfSwapchainImages,
        .imageFormat = gRHI->Swapchain.Format,
        .imageColorSpace = gRHI->Swapchain.ColorSpace,
        .imageExtent = { gRHI->Swapchain.Extent.width,
                         gRHI->Swapchain.Extent.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = PreTransform,
        .compositeAlpha = CompositeAlpha,
        .presentMode = DesiredVulkanPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = OldSwapchain,
    };

    if (SurfaceCapabilities.supportedUsageFlags &
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        SwapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (SurfaceCapabilities.supportedUsageFlags &
        VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        SwapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    Device->CreateSwapchainKHR(
        gRHI->Device.Handle,
        &SwapchainCreateInfo,
        NULL,
        &gRHI->Swapchain.Handle);

    if (OldSwapchain != VK_NULL_HANDLE)
    {
        Device->DestroySwapchainKHR(Device->Handle, OldSwapchain, NULL);
    }

    /* Acquire swapchain images. */

    uint32_t ImageCount = 0;
    Device->GetSwapchainImagesKHR(
        gRHI->Device.Handle,
        gRHI->Swapchain.Handle,
        &ImageCount,
        NULL);

    VkImage *ImageHandles =
        Rr_Alloc(sizeof(VkImage) * ImageCount, Scratch.Arena);

    Device->GetSwapchainImagesKHR(
        gRHI->Device.Handle,
        gRHI->Swapchain.Handle,
        &ImageCount,
        ImageHandles);

    /* Create image views. */

    if (gRHI->SwapchainImages.Capacity < ImageCount)
    {
        RR_RESERVE_ARRAY(&gRHI->SwapchainImages, ImageCount, Rr_GetPermanent());
    }

    gRHI->SwapchainImages.Count = ImageCount;

    for (uint32_t Index = 0; Index < ImageCount; Index++)
    {
        Rr_SwapchainImage *Image = gRHI->SwapchainImages.Data + Index;

        Image->Container = (Rr_Image2D){
            .Extent =
                (VkExtent3D){
                    .width = SwapchainCreateInfo.imageExtent.width,
                    .height = SwapchainCreateInfo.imageExtent.height,
                    .depth = 1,
                },
            .AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
            .Format = SwapchainCreateInfo.imageFormat,
            .SampleCount = VK_SAMPLE_COUNT_1_BIT,
            .Flags = RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT,
            .LayerCount = 1,
            .LevelCount = 1,
            .AllocatedImageCount = 1,
            .AllocatedImages[0] =
                (Rr_AllocatedImage){
                    .Handle = ImageHandles[Index],
                    .ImageViewMap = Rr_CreateImageViewMap(),
                    .Container = &Image->Container,
                    .SyncState = RR_EMPTY_SYNC,
                },
        };

        Image->EarlySemaphore = Rr_AcquireVulkanSemaphore();
        Image->LateSemaphore = Rr_AcquireVulkanSemaphore();
    }

    Rr_SetSwapchainDirty(false);

    Rr_DestroyScratch(Scratch);

    return true;
}

static bool Rr_RecreateSwapchainIfNeeded(void)
{
    Rr_IntVec2 WindowSize = Rr_GetWindowSize();

    if (WindowSize.Width == 0 || WindowSize.Height == 0)
    {
        return false;
    }

    bool Recreate =
        gRHI->Swapchain.Extent.width != (uint32_t)WindowSize.Width ||
        gRHI->Swapchain.Extent.height != (uint32_t)WindowSize.Height ||
        gRHI->Swapchain.RecreatePending;

    if (!Recreate)
    {
        return true;
    }

    bool Recreated = Rr_InitSwapchain();

    if (Recreated)
    {
        gRHI->Swapchain.Recreated = true;
    }

    return Recreate;
}

static void Rr_InitFrames(void)
{
    Rr_Device *Device = Rr_GetDevice();
    Rr_Frame *Frames = gRHI->Frames;
    Rr_CommandPools *CommandPools = Rr_AcquireCommandPools();

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame *Frame = &Frames[Index];
        Frame->Arena = Rr_CreateDefaultArena();
        Frame->AcquireSemaphore = Rr_AcquireVulkanSemaphore();

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .commandPool = CommandPools->Graphics,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        Device->AllocateCommandBuffers(
            Device->Handle,
            &CommandBufferAllocateInfo,
            &Frame->EarlyCommandBuffer);
        Device->AllocateCommandBuffers(
            Device->Handle,
            &CommandBufferAllocateInfo,
            &Frame->LateCommandBuffer);

#ifdef RR_USE_GPU_DEBUG_UTILS
        char NameBuffer[128];
        snprintf(
            NameBuffer,
            sizeof(NameBuffer) - 1,
            "Rr.Frame#%zu.EarlyCommandBuffer",
            Index);
        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_COMMAND_BUFFER,
            (uint64_t)Frame->EarlyCommandBuffer,
            NameBuffer);
        snprintf(
            NameBuffer,
            sizeof(NameBuffer) - 1,
            "Rr.Frame#%zu.LateCommandBuffer",
            Index);
        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_COMMAND_BUFFER,
            (uint64_t)Frame->LateCommandBuffer,
            NameBuffer);
#endif

        if (gRHI->MainQueue.TimestampsEnabled)
        {
            VkQueryPoolCreateInfo QueryPoolCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                .queryType = VK_QUERY_TYPE_TIMESTAMP,
                .queryCount = 4,
            };
            Device->CreateQueryPool(
                Device->Handle,
                &QueryPoolCreateInfo,
                NULL,
                &Frame->QueryPool);
        }
    }
}

static void Rr_CleanupFrames(void)
{
    Rr_Device *Device = Rr_GetDevice();

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame *Frame = &gRHI->Frames[Index];
        Rr_ReleaseVulkanFence(Frame->SubmitFence);
        Rr_ReleaseVulkanSemaphore(Frame->AcquireSemaphore);
        if (Frame->QueryPool)
        {
            Device->DestroyQueryPool(Device->Handle, Frame->QueryPool, NULL);
        }
        Rr_DestroyArena(Frame->Arena);
    }
}

static void Rr_InitEmptyDescriptorSet(void)
{
    Rr_Device *Device = Rr_GetDevice();

    VkResult Result = Device->CreateDescriptorPool(
        Device->Handle,
        &(VkDescriptorPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
        },
        NULL,
        &gRHI->EmptyDescriptorPool);
    if (Result != VK_SUCCESS)
    {
        goto Error;
    }

    Rr_DescriptorSetLayoutKey EmptyDescriptorSetLayoutKey = { 0 };
    VkDescriptorSetLayout EmptyDescriptorSetLayout =
        Rr_GetDescriptorSetLayout(&EmptyDescriptorSetLayoutKey)->Handle;
    Result = Device->AllocateDescriptorSets(
        Device->Handle,
        &(VkDescriptorSetAllocateInfo){
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = gRHI->EmptyDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &EmptyDescriptorSetLayout,
        },
        &gRHI->EmptyDescriptorSet);
    if (Result != VK_SUCCESS)
    {
        goto Error;
    }

    return;

Error:
    RR_LOG_ABORT("Failed to initialize empty descriptor set!");
}

static void Rr_CleanupEmptyDescriptorSet(void)
{
    Rr_Device *Device = Rr_GetDevice();

    Device->DestroyDescriptorPool(
        Device->Handle,
        gRHI->EmptyDescriptorPool,
        NULL);
}

Rr_DescriptorPoolList *Rr_AcquireDescriptorPoolList(void)
{
    Rr_Device *Device = Rr_GetDevice();

    Rr_DescriptorPoolList *Result = NULL;

    Rr_LockSpinlock(&gRHI->DescriptorPoolListLock);

    if (gRHI->DescriptorPoolList)
    {
        Result = gRHI->DescriptorPoolList;
        gRHI->DescriptorPoolList = Result->Next;

        Rr_UnlockSpinlock(&gRHI->DescriptorPoolListLock);
    }
    else
    {
        Rr_UnlockSpinlock(&gRHI->DescriptorPoolListLock);

        VkDescriptorPoolSize Sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RR_DESCRIPTOR_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, RR_DESCRIPTOR_POOL_SIZE },
        };

        VkDescriptorPoolCreateInfo CreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = RR_DESCRIPTOR_POOL_SIZE,
            .poolSizeCount = RR_ARRAY_COUNT(Sizes),
            .pPoolSizes = Sizes,
        };

        VkDescriptorPool Pool;
        Device->CreateDescriptorPool(Device->Handle, &CreateInfo, NULL, &Pool);

        Result =
            Rr_AllocNoZero(sizeof(Rr_DescriptorPoolList), Rr_GetPermanent());

        Result->Handle = Pool;

        gRHI->DescriptorPoolListCount++;
    }

    Result->Next = NULL;

    return Result;
}

void Rr_ReleaseDescriptorPoolList(Rr_DescriptorPoolList *List)
{
    if (List == NULL)
    {
        return;
    }

    Rr_Device *Device = Rr_GetDevice();

    Rr_DescriptorPoolList *First = List;

    while (List->Next)
    {
        Device->ResetDescriptorPool(Device->Handle, List->Handle, 0);
        List = List->Next;
    }

    Device->ResetDescriptorPool(Device->Handle, List->Handle, 0);

    Rr_LockSpinlock(&gRHI->DescriptorPoolListLock);

    List->Next = gRHI->DescriptorPoolList;
    gRHI->DescriptorPoolList = First;

    Rr_UnlockSpinlock(&gRHI->DescriptorPoolListLock);
}

void Rr_AllocateDescriptorSets(
    Rr_DescriptorPoolList *List,
    uint32_t Count,
    VkDescriptorSetLayout *Layouts,
    VkDescriptorSet *OutSets)
{
    Rr_Device *Device = Rr_GetDevice();

    VkDescriptorSetAllocateInfo AllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = List->Handle,
        .descriptorSetCount = Count,
        .pSetLayouts = Layouts,
    };
    VkResult Result =
        Device->AllocateDescriptorSets(Device->Handle, &AllocateInfo, OutSets);

    if (Result == VK_SUCCESS)
    {
        return;
    }

    /* TODO: Consider caching descriptor sets. */
    /* TODO: Consider iterating through "failed" pools as well. */

    Rr_DescriptorPoolList *NewList = Rr_AcquireDescriptorPoolList();
    List->Handle = NewList->Handle;
    NewList->Next = List->Next;
    List->Next = NewList;
    NewList->Handle = AllocateInfo.descriptorPool;

    AllocateInfo.descriptorPool = List->Handle;
    Result =
        Device->AllocateDescriptorSets(Device->Handle, &AllocateInfo, OutSets);

    assert(
        Result == VK_SUCCESS &&
        "Failed to allocate descriptor sets, too many descriptors requested?");
}

void Rr_InvalidateDescriptorsState(
    Rr_DescriptorsState *State,
    Rr_PipelineLayout *Layout)
{
    size_t Index = 0;

    if (State->Layout != NULL)
    {
        for (; Index < RR_MAX_SETS; ++Index)
        {
            Rr_DescriptorSetLayout *OldLayout =
                State->Layout->Key.DescriptorSetLayouts[Index];
            Rr_DescriptorSetLayout *NewLayout =
                Layout->Key.DescriptorSetLayouts[Index];
            if (OldLayout != NewLayout)
            {
                break;
            }
        }
    }

    for (; Index < Layout->Key.DescriptorSetLayoutCount; ++Index)
    {
        Rr_DescriptorSetLayout *NewLayout =
            Layout->Key.DescriptorSetLayouts[Index];
        State->Sets[Index] =
            NewLayout->Key.BindingCount ? NULL : State->EmptyDescriptorSet;
    }

    State->Layout = Layout;
}

static inline void Rr_CopyDescriptorSet(
    VkDescriptorSet Dst,
    VkDescriptorSet Src,
    Rr_Device *Device,
    Rr_DescriptorSetLayout *Layout)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkCopyDescriptorSet *Copies = Rr_AllocNoZero(
        sizeof(VkCopyDescriptorSet) * Layout->Key.BindingCount,
        Scratch.Arena);

    uint32_t CurrentCopyIndex = 0;

    for (uint32_t Index = 0; Index < Layout->Key.BindingCount; ++Index)
    {
        Rr_VulkanBinding *Binding = &Layout->Key.Bindings[Index];

        if (Binding->Count > 0)
        {
            Copies[CurrentCopyIndex++] = (VkCopyDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
                .srcSet = Src,
                .srcBinding = Binding->Index,
                .srcArrayElement = 0,
                .dstSet = Dst,
                .dstBinding = Binding->Index,
                .dstArrayElement = 0,
                .descriptorCount = Binding->Count,
            };
        }
    }

    Device->UpdateDescriptorSets(
        Device->Handle,
        0,
        NULL,
        CurrentCopyIndex,
        Copies);

    Rr_DestroyScratch(Scratch);
}

static inline VkDescriptorSet Rr_GetDescriptorSet(
    Rr_DescriptorsState *State,
    uint32_t SetIndex)
{
    if (State->Sets[SetIndex] == VK_NULL_HANDLE || !State->Dirty[SetIndex])
    {
        VkDescriptorSet OldSet = State->Sets[SetIndex];

        Rr_AllocateDescriptorSets(
            State->DescriptorPoolList,
            1,
            &State->Layout->Key.DescriptorSetLayouts[SetIndex]->Handle,
            &State->Sets[SetIndex]);

        if (OldSet)
        {
            Rr_CopyDescriptorSet(
                State->Sets[SetIndex],
                OldSet,
                State->Device,
                State->Layout->Key.DescriptorSetLayouts[SetIndex]);
        }

        State->Dirty[SetIndex] = true;
    }

    return State->Sets[SetIndex];
}

#define RR_RETURN_IF_NO_LAYOUT(State)                                        \
    {                                                                        \
        if (!State->Layout)                                                  \
        {                                                                    \
            RR_LOG_ERROR(                                                    \
                "Attempting to bind a resource but current layout is NULL, " \
                "forgot to bind a pipeline?");                               \
            return;                                                          \
        }                                                                    \
    }

void Rr_WriteImageDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkImageView View,
    VkImageLayout Layout,
    VkSampler Sampler)
{
    RR_RETURN_IF_NO_LAYOUT(State);

    Rr_Device *Device = State->Device;

    VkDescriptorImageInfo ImageInfo = {
        .sampler = Sampler,
        .imageView = View,
        .imageLayout = Layout,
    };

    VkWriteDescriptorSet Write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Rr_GetDescriptorSet(State, Set),
        .dstBinding = Binding,
        .dstArrayElement = ArrayIndex,
        .descriptorCount = 1,
        .descriptorType = Type,
        .pImageInfo = &ImageInfo,
    };

    Device->UpdateDescriptorSets(Device->Handle, 1, &Write, 0, NULL);
}

void Rr_WriteBufferDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkBuffer Handle,
    uint64_t Size,
    uint64_t Offset)
{
    RR_RETURN_IF_NO_LAYOUT(State);

    Rr_Device *Device = State->Device;

    VkDescriptorBufferInfo BufferInfo = {
        .buffer = Handle,
        .offset = Offset,
        .range = Size,
    };

    VkWriteDescriptorSet Write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Rr_GetDescriptorSet(State, Set),
        .dstBinding = Binding,
        .dstArrayElement = ArrayIndex,
        .descriptorCount = 1,
        .descriptorType = Type,
        .pBufferInfo = &BufferInfo,
    };

    Device->UpdateDescriptorSets(Device->Handle, 1, &Write, 0, NULL);
}

void Rr_WriteSamplerDescriptor(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkSampler Sampler)
{
    RR_RETURN_IF_NO_LAYOUT(State);

    Rr_Device *Device = State->Device;

    VkDescriptorImageInfo ImageInfo = {
        .sampler = Sampler,
    };

    VkWriteDescriptorSet Write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Rr_GetDescriptorSet(State, Set),
        .dstBinding = Binding,
        .dstArrayElement = ArrayIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &ImageInfo,
    };

    Device->UpdateDescriptorSets(Device->Handle, 1, &Write, 0, NULL);
}

void Rr_ApplyDescriptorsState(
    Rr_DescriptorsState *State,
    VkPipelineBindPoint BindPoint)
{
    RR_RETURN_IF_NO_LAYOUT(State);

    Rr_Device *Device = State->Device;

    for (uint32_t Index = 0; Index < RR_MAX_SETS; ++Index)
    {
        if (State->Dirty[Index])
        {
            Device->CmdBindDescriptorSets(
                State->CommandBuffer,
                BindPoint,
                State->Layout->Handle,
                Index,
                1,
                &State->Sets[Index],
                0,
                NULL);
            State->Dirty[Index] = false;
        }
    }
}

Rr_Buffer *Rr_CreateBuffer(uint64_t Size, Rr_BufferFlags Flags)
{
    if (Size == 0)
    {
        RR_LOG_ERROR("Buffer size can't be zero!");

        return NULL;
    }

    Rr_LockSpinlock(&gRHI->BuffersLock);

    Rr_Buffer *Buffer =
        Rr_PushBufferIntoHive(&gRHI->Buffers, Rr_GetPermanent()).Element;

    Rr_UnlockSpinlock(&gRHI->BuffersLock);

    *Buffer = (Rr_Buffer){
        .Flags = Flags,
        .Size = (VkDeviceSize)Size,
    };

    Rr_ConsumeNextObjectName(Buffer->Name);

    Buffer->Usage = 0;
    if (Flags & RR_BUFFER_FLAGS_UNIFORM_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (Flags & RR_BUFFER_FLAGS_STORAGE_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (Flags & RR_BUFFER_FLAGS_VERTEX_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (Flags & RR_BUFFER_FLAGS_INDEX_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (Flags & RR_BUFFER_FLAGS_INDIRECT_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    Buffer->Usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    Buffer->Usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    Buffer->AllocatedBufferCount = 1;
    if (Flags & RR_BUFFER_FLAGS_PER_FRAME_BIT)
    {
        Buffer->AllocatedBufferCount = RR_FRAME_OVERLAP;
    }

    VkBufferCreateInfo BufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = Size,
        .usage = Buffer->Usage,
    };

    Rr_Device *Device = Rr_GetDevice();

    for (uint32_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        AllocatedBuffer->SyncState = RR_EMPTY_SYNC;

        if (Device->CreateBuffer(
                Device->Handle,
                &BufferCreateInfo,
                NULL,
                &AllocatedBuffer->Handle) != VK_SUCCESS)
        {
            RR_LOG_ERROR("Failed to create buffer!");

            Rr_DestroyBuffer(Buffer);

            return NULL;
        }

#ifdef RR_USE_GPU_DEBUG_UTILS
        char ObjectName[RR_MAX_OBJECT_NAME_LENGTH];
        if (snprintf(
                ObjectName,
                sizeof(ObjectName) - 1,
                "%s#%d",
                Buffer->Name,
                Index))
        {
        }
        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_BUFFER,
            (uint64_t)AllocatedBuffer->Handle,
            ObjectName);
#endif
    }

    if (!Rr_AllocBufferMemory(&gRHI->Allocator, Buffer))
    {
        Rr_DestroyBuffer(Buffer);

        return NULL;
    }

    return Buffer;
}

size_t Rr_GetBufferSize(Rr_Buffer *Buffer)
{
    return (size_t)Buffer->Size;
}

void Rr_ReleaseBuffer(Rr_Buffer *Buffer)
{
    if (Buffer == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->ReleasedBuffersLock);

    Rr_InsertIntoHandleSet(
        &gRHI->ReleasedBuffers,
        (Rr_Handle const *)&Buffer,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->ReleasedBuffersLock);
}

void Rr_DestroyBuffer(Rr_Buffer *Buffer)
{
    assert(Buffer);

    Rr_Device *Device = Rr_GetDevice();

    for (uint32_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];

        if (AllocatedBuffer->Handle != VK_NULL_HANDLE)
        {
            Device->DestroyBuffer(
                Device->Handle,
                AllocatedBuffer->Handle,
                NULL);
        }
    }

    Rr_FreeBufferMemory(&gRHI->Allocator, Buffer);

    Rr_LockSpinlock(&gRHI->BuffersLock);

    Rr_BufferHiveIterator It = Rr_GetBufferHiveIterator(&gRHI->Buffers, Buffer);
    Rr_EraseFromBufferHive(&gRHI->Buffers, &It);

    Rr_UnlockSpinlock(&gRHI->BuffersLock);
}

void *Rr_GetMappedBufferData(Rr_Buffer *Buffer)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);

    return AllocatedBuffer->MappedData;
}

void *Rr_MapBuffer(Rr_Buffer *Buffer)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);

    if (Buffer->Flags & RR_BUFFER_FLAGS_MAPPED_BIT)
    {
        return AllocatedBuffer->MappedData;
    }

    return Rr_MapAllocatedBufferMemory(&gRHI->Allocator, AllocatedBuffer);
}

void Rr_UnmapBuffer(Rr_Buffer *Buffer)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);

    if (Buffer->Flags & RR_BUFFER_FLAGS_MAPPED_BIT)
    {
        return;
    }

    Rr_UnmapAllocatedBufferMemory(&gRHI->Allocator, AllocatedBuffer);
}

void Rr_FlushBufferRange(Rr_Buffer *Buffer, uint64_t Offset, uint64_t Size)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);

    Rr_FlushAllocatedBufferMemory(
        &gRHI->Allocator,
        AllocatedBuffer,
        Offset,
        Size);
}

Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(Rr_Buffer *Buffer)
{
    return &Buffer->AllocatedBuffers
                [gRHI->FrameIndex % Buffer->AllocatedBufferCount];
}

Rr_Sampler *Rr_CreateSampler(Rr_SamplerInfo const *Info)
{
    Rr_Device *Device = Rr_GetDevice();

    VkSamplerCreateInfo SamplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = Rr_ToVulkanFilter(Info->MagFilter),
        .minFilter = Rr_ToVulkanFilter(Info->MinFilter),
        .mipmapMode = Rr_ToVulkanSamplerMipmapMode(Info->MipmapMode),
        .addressModeU = Rr_ToVulkanSamplerAddressMode(Info->AddressModeU),
        .addressModeV = Rr_ToVulkanSamplerAddressMode(Info->AddressModeV),
        .addressModeW = Rr_ToVulkanSamplerAddressMode(Info->AddressModeW),
        .mipLodBias = Info->MipLodBias,
        .anisotropyEnable = Info->AnisotropyEnable,
        .maxAnisotropy = Info->MaxAnisotropy,
        .compareEnable = Info->CompareEnable,
        .compareOp = Rr_ToVulkanCompareOp(Info->CompareOp),
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
        .borderColor = Rr_ToVulkanBorderColor(Info->BorderColor),
        .unnormalizedCoordinates = Info->UnnormalizedCoordinates,
    };

    VkSampler Handle;
    if (Device->CreateSampler(Device->Handle, &SamplerInfo, NULL, &Handle) !=
        VK_SUCCESS)
    {
        RR_LOG_ERROR("Failed to create sampler!");

        return NULL;
    }

    Rr_LockSpinlock(&gRHI->SamplersLock);

    Rr_Sampler *Sampler =
        Rr_PushSamplerIntoHive(&gRHI->Samplers, Rr_GetPermanent()).Element;

    Rr_UnlockSpinlock(&gRHI->SamplersLock);

    *Sampler = (Rr_Sampler){
        .Handle = Handle,
    };

    Rr_ConsumeNextObjectName(Sampler->Name);

    Rr_SetVulkanObjectName(
        VK_OBJECT_TYPE_SAMPLER,
        (uint64_t)Sampler->Handle,
        Sampler->Name);

    return Sampler;
}

void Rr_ReleaseSampler(Rr_Sampler *Sampler)
{
    if (Sampler == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->ReleasedSamplersLock);

    Rr_InsertIntoHandleSet(
        &gRHI->ReleasedSamplers,
        (Rr_Handle const *)&Sampler,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->ReleasedSamplersLock);
}

void Rr_DestroySampler(Rr_Sampler *Sampler)
{
    assert(Sampler != NULL && Sampler->Handle != VK_NULL_HANDLE);

    Rr_Device *Device = Rr_GetDevice();

    Device->DestroySampler(Device->Handle, Sampler->Handle, NULL);

    Rr_LockSpinlock(&gRHI->SamplersLock);

    Rr_SamplerHiveIterator It =
        Rr_GetSamplerHiveIterator(&gRHI->Samplers, Sampler);
    Rr_EraseFromSamplerHive(&gRHI->Samplers, &It);

    Rr_UnlockSpinlock(&gRHI->SamplersLock);
}

Rr_ImageViewMap *Rr_CreateImageViewMap(void)
{
    Rr_Arena *Arena = Rr_GetPermanent();

    Rr_LockSpinlock(&gRHI->ImageViewMapsLock);

    Rr_ImageViewMap *ImageViewMap =
        RR_GET_FREE_LIST_ITEM(&gRHI->ImageViewMaps, Arena);
    if (!ImageViewMap->Capacity)
    {
        Rr_InitImageViewMap(ImageViewMap, Arena);
    }

    Rr_UnlockSpinlock(&gRHI->ImageViewMapsLock);

    return ImageViewMap;
}

void Rr_DestroyImageViewMap(
    Rr_ImageViewMap *ImageViewMap,
    bool DestroyFramebuffers)
{
    Rr_Device *Device = Rr_GetDevice();

    Rr_ImageViewMapIterator It = Rr_BeginInImageViewMap(ImageViewMap);
    while (!Rr_IsImageViewMapEnd(&It))
    {
        VkImageView Handle = It.Data->Value;
        if (DestroyFramebuffers)
        {
            Rr_DestroyFramebuffers(Handle);
        }
        Device->DestroyImageView(Device->Handle, Handle, NULL);
        Rr_EraseFromImageViewMap(&It);
    }

    Rr_LockSpinlock(&gRHI->ImageViewMapsLock);

    RR_RETURN_FREE_LIST_ITEM(&gRHI->ImageViewMaps, ImageViewMap);

    Rr_UnlockSpinlock(&gRHI->ImageViewMapsLock);
}

VkImageView Rr_GetVulkanImageView(
    Rr_AllocatedImage *AllocatedImage,
    Rr_ImageViewKey const *Key)
{
    Rr_LockSpinlock(&AllocatedImage->ImageViewMapLock);

    Rr_ImageViewMapIterator It =
        Rr_FindInImageViewMap(AllocatedImage->ImageViewMap, Key);
    if (!Rr_IsImageViewMapEnd(&It))
    {
        Rr_UnlockSpinlock(&AllocatedImage->ImageViewMapLock);

        return It.Data->Value;
    }

    Rr_UnlockSpinlock(&AllocatedImage->ImageViewMapLock);

    Rr_Device *Device = Rr_GetDevice();

    VkImageViewCreateInfo ImageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = AllocatedImage->Handle,
        .viewType = Key->Type,
        .format = Key->Format != VK_FORMAT_UNDEFINED
                      ? Key->Format
                      : AllocatedImage->Container->Format,
        .subresourceRange = Key->SubresourceRange,
    };

    VkImageView Handle = VK_NULL_HANDLE;
    VkResult Result = Device->CreateImageView(
        Device->Handle,
        &ImageViewCreateInfo,
        NULL,
        &Handle);
    assert(Result == VK_SUCCESS);

#ifdef RR_DEBUG
    if (AllocatedImage->Container->Name[0] != '\0')
    {
        char NameBuffer[128];
        uint32_t LayerCount =
            Key->SubresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS
                ? AllocatedImage->Container->LayerCount
                : Key->SubresourceRange.layerCount;
        uint32_t LevelCount =
            Key->SubresourceRange.levelCount == VK_REMAINING_MIP_LEVELS
                ? AllocatedImage->Container->LevelCount
                : Key->SubresourceRange.levelCount;
        snprintf(
            NameBuffer,
            sizeof(NameBuffer) - 1,
            "Rr.Image.%s.View.%d-%d/%d-%d",
            AllocatedImage->Container->Name,
            Key->SubresourceRange.baseArrayLayer,
            Key->SubresourceRange.baseArrayLayer + LayerCount,
            Key->SubresourceRange.baseMipLevel,
            Key->SubresourceRange.baseMipLevel + LevelCount);

        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_IMAGE_VIEW,
            (uint64_t)Handle,
            NameBuffer);
    }
#endif

    Rr_LockSpinlock(&AllocatedImage->ImageViewMapLock);

    Rr_InsertIntoImageViewMap(
        AllocatedImage->ImageViewMap,
        Key,
        &Handle,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&AllocatedImage->ImageViewMapLock);

    return Handle;
}

static inline VkSampleCountFlagBits Rr_ToVulkanSampleCountFlagBits(
    Rr_ImageFlags ImageFlags)
{
    /* TODO: Hardcoded offset. */
    return ImageFlags >> 9;
}

static Rr_Image *Rr_CreateImage(
    Rr_IntVec3 Extent,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags,
    uint32_t LayerCount,
    VkImageType ImageType,
    VkImageCreateFlags AdditionalFlags)
{
    Rr_LockSpinlock(&gRHI->ImagesLock);

    Rr_Image *Image =
        Rr_PushImageIntoHive(&gRHI->Images, Rr_GetPermanent()).Element;

    Rr_UnlockSpinlock(&gRHI->ImagesLock);

    uint32_t LevelCount = 1;
    if (Flags & RR_IMAGE_FLAGS_MIP_MAPPED_BIT)
    {
        int32_t Max = RR_MAX(RR_MAX(Extent.Width, Extent.Height), Extent.Depth);
        LevelCount = (uint32_t)floorf(log2f((float)Max)) + 1;
    }

    *Image = (Rr_Image){
        .Flags = Flags,
        .Format = Rr_ToVulkanImageFormat(Format),
        .Extent.width = (uint32_t)Extent.Width,
        .Extent.height = (uint32_t)Extent.Height,
        .Extent.depth = (uint32_t)Extent.Depth,
        .LayerCount = LayerCount,
        .LevelCount = LevelCount,
        .AllocatedImageCount = 1,
    };

    Rr_ConsumeNextObjectName(Image->Name);

    VkImageUsageFlags UsageFlags = 0;
    if (Flags & RR_IMAGE_FLAGS_STORAGE_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (Flags & RR_IMAGE_FLAGS_SAMPLED_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (Flags & RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (Flags & RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if (Flags & RR_IMAGE_FLAGS_TRANSFER_BIT)
    {
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        UsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (Flags & RR_IMAGE_FLAGS_MUTABLE_FORMAT_BIT)
    {
        AdditionalFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }

    /* TODO: Some kind of real usage must be enforced aside from TRANSFER_*. */

    Image->SampleCount = Rr_ToVulkanSampleCountFlagBits(Flags);
    if (Image->SampleCount == 0)
    {
        Image->SampleCount = VK_SAMPLE_COUNT_1_BIT;
    }
    VkImageCreateInfo ImageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = AdditionalFlags,
        .imageType = ImageType,
        .format = Image->Format,
        .extent = Image->Extent,
        .mipLevels = LevelCount,
        .arrayLayers = LayerCount,
        .samples = Image->SampleCount,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = UsageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    Image->AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if (Image->Format == VK_FORMAT_D16_UNORM_S8_UINT ||
        Image->Format == VK_FORMAT_D24_UNORM_S8_UINT ||
        Image->Format == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        Image->AspectFlags =
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else if (
        Image->Format == VK_FORMAT_D16_UNORM ||
        Image->Format == VK_FORMAT_D32_SFLOAT ||
        Image->Format == VK_FORMAT_X8_D24_UNORM_PACK32)
    {
        Image->AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    Rr_Device *Device = Rr_GetDevice();

    for (uint32_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Rr_AllocatedImage *AllocatedImage = Image->AllocatedImages + Index;
        AllocatedImage->SyncState = RR_EMPTY_SYNC;
        AllocatedImage->Container = Image;

        if (Device->CreateImage(
                Device->Handle,
                &ImageCreateInfo,
                NULL,
                &AllocatedImage->Handle) != VK_SUCCESS)
        {
            RR_LOG_ERROR("Failed to create image!");

            Rr_DestroyImage(Image);

            return NULL;
        }

        AllocatedImage->ImageViewMap = Rr_CreateImageViewMap();

#ifdef RR_USE_GPU_DEBUG_UTILS
        char ObjectName[RR_MAX_OBJECT_NAME_LENGTH];
        if (snprintf(
                ObjectName,
                sizeof(ObjectName) - 1,
                "%s#%d",
                Image->Name,
                Index))
        {
        }
        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_IMAGE,
            (uint64_t)AllocatedImage->Handle,
            ObjectName);
#endif
    }

    if (!Rr_AllocImageMemory(&gRHI->Allocator, Image))
    {
        Rr_DestroyImage(Image);

        return NULL;
    }

    return Image;
}

void Rr_ReleaseImage(Rr_Image *Image)
{
    if (Image == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->ReleasedImagesLock);

    Rr_InsertIntoHandleSet(
        &gRHI->ReleasedImages,
        (Rr_Handle const *)&Image,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->ReleasedImagesLock);
}

void Rr_DestroyImage(Rr_Image *Image)
{
    assert(Image);

    Rr_Device *Device = Rr_GetDevice();

    bool DestroyFramebuffers =
        (Image->Flags & RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT) ||
        (Image->Flags & RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);

    for (uint32_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Rr_AllocatedImage *AllocatedImage = &Image->AllocatedImages[Index];

        Rr_DestroyImageViewMap(
            AllocatedImage->ImageViewMap,
            DestroyFramebuffers);

        if (AllocatedImage->Handle)
        {
            Device->DestroyImage(Device->Handle, AllocatedImage->Handle, NULL);
        }
    }

    Rr_FreeImageMemory(&gRHI->Allocator, Image);

    Rr_LockSpinlock(&gRHI->ImagesLock);

    Rr_ImageHiveIterator It = Rr_GetImageHiveIterator(&gRHI->Images, Image);
    Rr_EraseFromImageHive(&gRHI->Images, &It);

    Rr_UnlockSpinlock(&gRHI->ImagesLock);
}

Rr_Image2D *Rr_CreateImage2D(
    Rr_IntVec2 Extent,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);

    return (Rr_Image2D *)Rr_CreateImage(
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        Format,
        Flags,
        1,
        VK_IMAGE_TYPE_2D,
        0);
}

Rr_Image2DArray *Rr_CreateImage2DArray(
    Rr_IntVec2 Extent,
    uint32_t ArrayCount,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);
    assert(ArrayCount >= 1);

    return (Rr_Image2DArray *)Rr_CreateImage(
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        Format,
        Flags,
        ArrayCount,
        VK_IMAGE_TYPE_2D,
        0);
}

Rr_Image3D *Rr_CreateImage3D(
    Rr_IntVec3 Extent,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);
    assert(Extent.Depth >= 1);

    return (Rr_Image3D *)
        Rr_CreateImage(Extent, Format, Flags, 1, VK_IMAGE_TYPE_3D, 0);
}

Rr_ImageCube *Rr_CreateImageCube(
    Rr_IntVec2 Extent,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags)
{
    assert(Extent.Width >= 1);
    assert(Extent.Height >= 1);

    return (Rr_ImageCube *)Rr_CreateImage(
        (Rr_IntVec3){ Extent.Width, Extent.Height, 1 },
        Format,
        Flags,
        6,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
}

Rr_ImageFormat Rr_GetImageFormat(struct Rr_Image *Image)
{
    return Rr_ToImageFormat(Image->Format);
}

Rr_IntVec2 Rr_GetImage2DExtent(Rr_Image2D *Image2D)
{
    return (Rr_IntVec2){
        .Width = (int32_t)Image2D->Extent.width,
        .Height = (int32_t)Image2D->Extent.height,
    };
}

float Rr_GetImage2DAspect(Rr_Image2D *Image)
{
    return (float)Image->Extent.width / (float)Image->Extent.height;
}

Rr_IntVec3 Rr_GetImageExtent(Rr_Image *Image)
{
    return (Rr_IntVec3){
        .Width = (int32_t)Image->Extent.width,
        .Height = (int32_t)Image->Extent.height,
        .Depth = (int32_t)Image->Extent.depth,
    };
}

Rr_AllocatedImage *Rr_GetCurrentAllocatedImage(Rr_Image *Image)
{
    return &Image->AllocatedImages
                [gRHI->FrameIndex % Image->AllocatedImageCount];
}

static VkRenderPass Rr_GetCompatibleRenderPass(
    uint32_t ColorTargetCount,
    Rr_ColorTargetInfo const *ColorTargets,
    Rr_DepthStencil const *DepthStencil,
    uint32_t SampleCount)
{
    Rr_RenderPassKey Key = {
        .ColorAttachmentCount = (uint8_t)ColorTargetCount,
        .DepthStencil = DepthStencil->EnableDepthTest ||
                        DepthStencil->EnableStencilTest ||
                        DepthStencil->EnableDepthWrite,
    };

    uint32_t ResolveAttachmentIndex = ColorTargetCount;

    for (uint32_t Index = 0; Index < ColorTargetCount; ++Index)
    {
        Rr_ColorTargetInfo const *Info = &ColorTargets[Index];

        Key.Attachments[Index].Format = Rr_ToVulkanImageFormat(Info->Format);
        Key.Attachments[Index].Samples = SampleCount;
        Key.Attachments[Index].LoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        Key.Attachments[Index].StoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        if (!Info->Resolve)
        {
            continue;
        }

        Key.ResolveMask |= (uint8_t)(1 << Index);
        Key.ResolveAttachmentCount++;

        Key.Attachments[ResolveAttachmentIndex].Format =
            Rr_ToVulkanImageFormat(Info->Format);
        Key.Attachments[ResolveAttachmentIndex].Samples = 1;
        Key.Attachments[ResolveAttachmentIndex].LoadOp =
            VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        Key.Attachments[ResolveAttachmentIndex].StoreOp =
            VK_ATTACHMENT_STORE_OP_DONT_CARE;

        ResolveAttachmentIndex++;
    }

    if (Key.DepthStencil)
    {
        Key.Attachments[ResolveAttachmentIndex].Format =
            Rr_ToVulkanImageFormat(DepthStencil->Format);
        Key.Attachments[ResolveAttachmentIndex].Samples = SampleCount;
        Key.Attachments[ResolveAttachmentIndex].LoadOp =
            VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        Key.Attachments[ResolveAttachmentIndex].StoreOp =
            VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    return Rr_GetRenderPass(&Key);
}

static inline int Rr_BindingSort(void const *A, void const *B)
{
    Rr_Binding const *BindingA = A;
    Rr_Binding const *BindingB = B;

    return (int)((int64_t)BindingA->Index - (int64_t)BindingB->Index);
}

Rr_PipelineLayout *Rr_GetPipelineLayout(
    size_t BindingSetCount,
    Rr_BindingSet const *BindingSets)
{
    assert(BindingSetCount <= RR_MAX_SETS);

    Rr_PipelineLayoutKey PipelineLayoutKey = {
        .DescriptorSetLayoutCount = (uint32_t)BindingSetCount,
    };
    for (size_t SetIndex = 0; SetIndex < BindingSetCount; ++SetIndex)
    {
        Rr_Binding *Bindings = BindingSets[SetIndex].Bindings;
        size_t BindingCount = BindingSets[SetIndex].BindingCount;

        qsort(Bindings, BindingCount, sizeof(Rr_Binding), Rr_BindingSort);

        Rr_DescriptorSetLayoutKey Key = { 0 };
        Key.BindingCount = (uint32_t)BindingCount;
        Rr_ToVulkanBindings(BindingCount, Bindings, Key.Bindings);

        PipelineLayoutKey.DescriptorSetLayouts[SetIndex] =
            Rr_GetDescriptorSetLayout(&Key);
    }

    size_t HashSize = sizeof(Rr_PipelineLayoutKey);
    Rr_PipelineLayout **MapRef = &gRHI->PipelineLayoutStorage.Map;
    Rr_PipelineLayout *PipelineLayout = NULL;

    Rr_LockSpinlock(&gRHI->PipelineLayoutStorageLock);

    for (uint64_t Hash = Rr_Hash64(HashSize, &PipelineLayoutKey); *MapRef;
         Hash <<= 2)
    {
        if ((*MapRef)->Handle == VK_NULL_HANDLE)
        {
            (*MapRef)->Key = PipelineLayoutKey;
            PipelineLayout = *MapRef;

            goto FoundEmpty;
        }
        if (memcmp(&PipelineLayoutKey, &(*MapRef)->Key, HashSize) == 0)
        {
            Rr_UnlockSpinlock(&gRHI->PipelineLayoutStorageLock);

            return *MapRef;
        }
        MapRef = &(*MapRef)->Children[Hash >> 62];
    }
    *MapRef = Rr_PushPipelineLayoutIntoHive(
                  &gRHI->PipelineLayoutStorage.Hive,
                  Rr_GetPermanent())
                  .Element;
    PipelineLayout = *MapRef;
    RR_ZERO_PTR(PipelineLayout);

FoundEmpty:

    PipelineLayout->Key = PipelineLayoutKey;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = Rr_GetDevice();

    VkDescriptorSetLayout VulkanDescriptorSetLayouts[RR_MAX_SETS];
    for (size_t Index = 0; Index < PipelineLayoutKey.DescriptorSetLayoutCount;
         ++Index)
    {
        VulkanDescriptorSetLayouts[Index] =
            PipelineLayoutKey.DescriptorSetLayouts[Index]->Handle;
    }

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = PipelineLayoutKey.DescriptorSetLayoutCount,
        .pSetLayouts = VulkanDescriptorSetLayouts,
    };

    VkResult Result = Device->CreatePipelineLayout(
        Device->Handle,
        &PipelineLayoutCreateInfo,
        NULL,
        &PipelineLayout->Handle);
    assert(Result == VK_SUCCESS);

    Rr_UnlockSpinlock(&gRHI->PipelineLayoutStorageLock);

    Rr_DestroyScratch(Scratch);

    return PipelineLayout;
}

static VkSpecializationInfo *Rr_GetVulkanSpecializationInfo(
    size_t SpecializationCount,
    Rr_PipelineSpecialization const *Specializations,
    Rr_Arena *Arena)
{
    VkSpecializationInfo *SpecializationInfo =
        Rr_Alloc(sizeof(VkSpecializationInfo), Arena);
    SpecializationInfo->mapEntryCount = (uint32_t)SpecializationCount;
    VkSpecializationMapEntry *Entries = Rr_AllocNoZero(
        sizeof(VkSpecializationMapEntry) * SpecializationCount,
        Arena);
    uintptr_t ArenaPosition = Arena->Position;
    char *DataStart = NULL;
    for (size_t Index = 0; Index < SpecializationCount; ++Index)
    {
        Rr_PipelineSpecialization const *Specialization =
            Specializations + Index;
        char *SpecializationData = Rr_AllocNoZero(Specialization->Size, Arena);
        if (DataStart == NULL)
        {
            DataStart = SpecializationData;
        }
        memcpy(SpecializationData, Specialization->Data, Specialization->Size);
        Entries[Index] = (VkSpecializationMapEntry){
            .constantID = Specialization->ConstantID,
            .size = Specialization->Size,
            .offset = (uint32_t)(SpecializationData - DataStart),
        };
    }
    SpecializationInfo->pMapEntries = Entries;
    SpecializationInfo->pData = DataStart;
    SpecializationInfo->dataSize = Arena->Position - ArenaPosition;

    return SpecializationInfo;
}

Rr_ComputePipeline *Rr_CreateComputePipeline(Rr_ShaderInfo const *ShaderInfo)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_BindingArray BindingArrays[RR_MAX_SETS] = { 0 };
    size_t BindingSetCount = Rr_GetBindingsFromSPIRV(
        ShaderInfo,
        RR_SHADER_STAGE_COMPUTE_BIT,
        BindingArrays,
        Scratch.Arena);

    Rr_BindingSet BindingSets[RR_MAX_SETS] = { 0 };
    for (size_t Index = 0; Index < BindingSetCount; ++Index)
    {
        BindingSets[Index].BindingCount = BindingArrays[Index].Count;
        BindingSets[Index].Bindings = BindingArrays[Index].Data;
    }

    Rr_PipelineLayout *PipelineLayout =
        Rr_GetPipelineLayout(BindingSetCount, BindingSets);

    Rr_DestroyScratch(Scratch);

    return Rr_CreateComputePipelineWithLayout(ShaderInfo, PipelineLayout);
}

Rr_ComputePipeline *Rr_CreateComputePipelineWithLayout(
    Rr_ShaderInfo const *ShaderInfo,
    Rr_PipelineLayout *PipelineLayout)
{
    Rr_Device *Device = Rr_GetDevice();

    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = ShaderInfo->SPVSize,
        .pCode = (uint32_t const *)ShaderInfo->SPVData,
    };
    VkShaderModule ShaderModule = VK_NULL_HANDLE;
    if (Device->CreateShaderModule(
            Device->Handle,
            &ShaderModuleCreateInfo,
            NULL,
            &ShaderModule) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Failed to create compute shader module!");

        return NULL;
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkComputePipelineCreateInfo PipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = PipelineLayout->Handle,
        .stage =
            (VkPipelineShaderStageCreateInfo){

                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = ShaderModule,
                .pName =
                    ShaderInfo->EntryPoint ? ShaderInfo->EntryPoint : "main",
            },
    };
    if (ShaderInfo->SpecializationCount)
    {
        PipelineCreateInfo.stage.pSpecializationInfo =
            Rr_GetVulkanSpecializationInfo(
                ShaderInfo->SpecializationCount,
                ShaderInfo->Specializations,
                Scratch.Arena);
    }
    VkPipeline Handle;
    if (Device->CreateComputePipelines(
            Device->Handle,
            VK_NULL_HANDLE,
            1,
            &PipelineCreateInfo,
            NULL,
            &Handle) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Failed to create compute pipeline!");

        Device->DestroyShaderModule(Device->Handle, ShaderModule, NULL);

        Rr_DestroyScratch(Scratch);

        return NULL;
    }

    Device->DestroyShaderModule(Device->Handle, ShaderModule, NULL);

    Rr_LockSpinlock(&gRHI->ComputePipelinesLock);

    Rr_ComputePipeline *ComputePipeline = Rr_PushComputePipelineIntoHive(
                                              &gRHI->ComputePipelines,
                                              Rr_GetPermanent())
                                              .Element;

    Rr_UnlockSpinlock(&gRHI->ComputePipelinesLock);

    *ComputePipeline = (Rr_ComputePipeline){
        .Layout = PipelineLayout,
        .Handle = Handle,
    };

    Rr_ConsumeNextObjectName(ComputePipeline->Name);

    Rr_SetVulkanObjectName(
        VK_OBJECT_TYPE_PIPELINE,
        (uint64_t)ComputePipeline->Handle,
        ComputePipeline->Name);

    Rr_DestroyScratch(Scratch);

    return ComputePipeline;
}

void Rr_ReleaseComputePipeline(Rr_ComputePipeline *ComputePipeline)
{
    if (ComputePipeline == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->ReleasedComputePipelinesLock);

    Rr_InsertIntoHandleSet(
        &gRHI->ReleasedComputePipelines,
        (Rr_Handle const *)&ComputePipeline,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->ReleasedComputePipelinesLock);
}

void Rr_DestroyComputePipeline(Rr_ComputePipeline *ComputePipeline)
{
    assert(ComputePipeline && ComputePipeline->Handle != VK_NULL_HANDLE);

    Rr_Device *Device = Rr_GetDevice();

    Device->DestroyPipeline(Device->Handle, ComputePipeline->Handle, NULL);

    Rr_LockSpinlock(&gRHI->ComputePipelinesLock);

    Rr_ComputePipelineHiveIterator It = Rr_GetComputePipelineHiveIterator(
        &gRHI->ComputePipelines,
        ComputePipeline);
    Rr_EraseFromComputePipelineHive(&gRHI->ComputePipelines, &It);

    Rr_UnlockSpinlock(&gRHI->ComputePipelinesLock);
}

Rr_ColorTargetBlend Rr_AlphaBlend(void)
{
    Rr_ColorTargetBlend Blend;
    Blend.BlendEnable = true;
    Blend.SrcColorBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
    Blend.DstColorBlendFactor = RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    Blend.ColorBlendOp = RR_BLEND_OP_ADD;
    Blend.SrcAlphaBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
    Blend.DstAlphaBlendFactor = RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    Blend.AlphaBlendOp = RR_BLEND_OP_ADD;
    Blend.ColorWriteMask = RR_COLOR_COMPONENT_DEFAULT;

    return Blend;
}

Rr_GraphicsPipeline *Rr_CreateGraphicsPipeline(
    Rr_GraphicsPipelineCreateInfo const *CreateInfo)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_BindingArray BindingArrays[RR_MAX_SETS] = { 0 };
    size_t VertexBindingSetCount = Rr_GetBindingsFromSPIRV(
        CreateInfo->VertexShaderInfo,
        RR_SHADER_STAGE_VERTEX_BIT,
        BindingArrays,
        Scratch.Arena);
    size_t FragmentBindingSetCount = Rr_GetBindingsFromSPIRV(
        CreateInfo->FragmentShaderInfo,
        RR_SHADER_STAGE_FRAGMENT_BIT,
        BindingArrays,
        Scratch.Arena);
    size_t BindingSetCount =
        RR_MAX(VertexBindingSetCount, FragmentBindingSetCount);

    Rr_BindingSet BindingSets[RR_MAX_SETS] = { 0 };
    for (size_t Index = 0; Index < BindingSetCount; ++Index)
    {
        BindingSets[Index].BindingCount = BindingArrays[Index].Count;
        BindingSets[Index].Bindings = BindingArrays[Index].Data;
    }

    Rr_PipelineLayout *PipelineLayout =
        Rr_GetPipelineLayout(BindingSetCount, BindingSets);

    Rr_DestroyScratch(Scratch);

    return Rr_CreateGraphicsPipelineWithLayout(CreateInfo, PipelineLayout);
}

Rr_GraphicsPipeline *Rr_CreateGraphicsPipelineWithLayout(
    Rr_GraphicsPipelineCreateInfo const *CreateInfo,
    Rr_PipelineLayout *PipelineLayout)
{
    assert(CreateInfo);

    bool HasDepthStencil = CreateInfo->DepthStencil.EnableDepthTest ||
                           CreateInfo->DepthStencil.EnableStencilTest ||
                           CreateInfo->DepthStencil.EnableDepthWrite;
    if (HasDepthStencil)
    {
        assert(CreateInfo->DepthStencil.Format != RR_IMAGE_FORMAT_UNDEFINED);
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = Rr_GetDevice();

    VkResult Result;

    RR_ARRAY(VkPipelineShaderStageCreateInfo) ShaderStages = { 0 };

    VkShaderModule VertModule = VK_NULL_HANDLE;
    if (CreateInfo->VertexShaderInfo)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = CreateInfo->VertexShaderInfo->SPVSize,
            .pCode = (uint32_t const *)CreateInfo->VertexShaderInfo->SPVData,
        };
        Result = Device->CreateShaderModule(
            Device->Handle,
            &ShaderModuleCreateInfo,
            NULL,
            &VertModule);
        assert(Result == VK_SUCCESS);

        VkPipelineShaderStageCreateInfo *PipelineShaderStageCreateInfo =
            RR_PUSH_INTO_ARRAY(&ShaderStages, Scratch.Arena);
        *PipelineShaderStageCreateInfo = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = CreateInfo->VertexShaderInfo->EntryPoint
                         ? CreateInfo->VertexShaderInfo->EntryPoint
                         : "main",
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = VertModule,
        };
        if (CreateInfo->VertexShaderInfo->SpecializationCount)
        {
            PipelineShaderStageCreateInfo->pSpecializationInfo =
                Rr_GetVulkanSpecializationInfo(
                    CreateInfo->VertexShaderInfo->SpecializationCount,
                    CreateInfo->VertexShaderInfo->Specializations,
                    Scratch.Arena);
        }
    }

    VkShaderModule FragModule = VK_NULL_HANDLE;
    if (CreateInfo->FragmentShaderInfo)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .codeSize = CreateInfo->FragmentShaderInfo->SPVSize,
            .pCode = (uint32_t const *)CreateInfo->FragmentShaderInfo->SPVData,
        };
        Result = Device->CreateShaderModule(
            Device->Handle,
            &ShaderModuleCreateInfo,
            NULL,
            &FragModule);
        assert(Result == VK_SUCCESS);

        VkPipelineShaderStageCreateInfo *PipelineShaderStageCreateInfo =
            RR_PUSH_INTO_ARRAY(&ShaderStages, Scratch.Arena);
        *PipelineShaderStageCreateInfo = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .pName = CreateInfo->FragmentShaderInfo->EntryPoint
                         ? CreateInfo->FragmentShaderInfo->EntryPoint
                         : "main",
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = FragModule,
        };
        if (CreateInfo->FragmentShaderInfo->SpecializationCount)
        {
            PipelineShaderStageCreateInfo->pSpecializationInfo =
                Rr_GetVulkanSpecializationInfo(
                    CreateInfo->FragmentShaderInfo->SpecializationCount,
                    CreateInfo->FragmentShaderInfo->Specializations,
                    Scratch.Arena);
        }
    }

    RR_ARRAY(VkVertexInputBindingDescription) BindingDescriptions = { 0 };
    RR_RESERVE_ARRAY(
        &BindingDescriptions,
        CreateInfo->VertexInputBindingCount,
        Scratch.Arena);
    RR_ARRAY(VkVertexInputAttributeDescription) AttributeDescriptions = { 0 };
    for (uint32_t BindingIndex = 0;
         BindingIndex < CreateInfo->VertexInputBindingCount;
         ++BindingIndex)
    {
        Rr_VertexInputBinding const *VertexInputBinding =
            CreateInfo->VertexInputBindings + BindingIndex;

        RR_RESERVE_ARRAY(
            &AttributeDescriptions,
            AttributeDescriptions.Count + VertexInputBinding->AttributeCount,
            Scratch.Arena);

        for (size_t AttributeIndex = 0;
             AttributeIndex < VertexInputBinding->AttributeCount;
             ++AttributeIndex)
        {
            Rr_VertexInputAttribute const *Attribute =
                VertexInputBinding->Attributes + AttributeIndex;

            VkVertexInputAttributeDescription *AttributeDescription =
                RR_PUSH_INTO_ARRAY(&AttributeDescriptions, Scratch.Arena);
            *AttributeDescription = (VkVertexInputAttributeDescription){
                .location = Attribute->Location,
                .format = Rr_ToVulkanFormat(Attribute->Format),
                .binding = BindingIndex,
            };

            VkVertexInputBindingDescription *BindingDescription = NULL;
            for (size_t Index = 0; Index < BindingDescriptions.Count; ++Index)
            {
                if (BindingDescriptions.Data[Index].binding == BindingIndex)
                {
                    BindingDescription = BindingDescriptions.Data + Index;

                    break;
                }
            }
            if (BindingDescription == NULL)
            {
                BindingDescription =
                    RR_PUSH_INTO_ARRAY(&BindingDescriptions, Scratch.Arena);
                *BindingDescription = (VkVertexInputBindingDescription){
                    .stride = 0,
                    .binding = BindingIndex,
                    .inputRate = VertexInputBinding->Rate ==
                                         RR_VERTEX_INPUT_RATE_INSTANCE
                                     ? VK_VERTEX_INPUT_RATE_INSTANCE
                                     : VK_VERTEX_INPUT_RATE_VERTEX,
                };
            }
            size_t Size = Rr_GetFormatSize(Attribute->Format);
            AttributeDescription->offset = BindingDescription->stride;
            BindingDescription->stride += (uint32_t)Size;
        }
    }

    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexAttributeDescriptionCount =
            (uint32_t)AttributeDescriptions.Count,
        .pVertexAttributeDescriptions = AttributeDescriptions.Data,
        .vertexBindingDescriptionCount = (uint32_t)BindingDescriptions.Count,
        .pVertexBindingDescriptions = BindingDescriptions.Data,
    };

    VkPipelineInputAssemblyStateCreateInfo InputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = Rr_ToVulkanPrimitiveTopology(CreateInfo->Topology),
    };

    VkPipelineViewportStateCreateInfo ViewportInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo Rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode =
            Rr_ToVulkanPolygonMode(CreateInfo->Rasterizer.PolygonMode),
        .cullMode = Rr_ToVulkanCullMode(CreateInfo->Rasterizer.CullMode),
        .frontFace = Rr_ToVulkanFrontFace(CreateInfo->Rasterizer.FrontFace),
        .depthBiasEnable = CreateInfo->Rasterizer.EnableDepthBias,
        .depthBiasConstantFactor =
            CreateInfo->Rasterizer.DepthBiasConstantFactor,
        .depthBiasClamp = CreateInfo->Rasterizer.DepthBiasClamp,
        .depthBiasSlopeFactor = CreateInfo->Rasterizer.DepthBiasSlopeFactor,
        .lineWidth = 1.0f,
    };

    VkDynamicState DynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT,
                                       VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pDynamicStates = DynamicStates,
        .dynamicStateCount = RR_ARRAY_COUNT(DynamicStates),
    };

    VkPipelineMultisampleStateCreateInfo Multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    if (RR_IS_POW2(CreateInfo->Multisampling.SampleCount) &&
        CreateInfo->Multisampling.SampleCount != 0)
    {
        Multisampling.rasterizationSamples =
            CreateInfo->Multisampling.SampleCount;
    }
    else
    {
        Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    }

    RR_ARRAY(VkPipelineColorBlendAttachmentState) ColorAttachments = { 0 };
    RR_RESERVE_ARRAY(
        &ColorAttachments,
        CreateInfo->ColorTargetCount,
        Scratch.Arena);
    for (size_t Index = 0; Index < CreateInfo->ColorTargetCount; ++Index)
    {
        VkPipelineColorBlendAttachmentState *Attachment =
            RR_PUSH_INTO_ARRAY(&ColorAttachments, Scratch.Arena);
        Rr_ColorTargetInfo const *ColorTargetInfo =
            CreateInfo->ColorTargets + Index;
        Rr_ColorTargetBlend const *Blend = &ColorTargetInfo->Blend;

        VkColorComponentFlags ColorWriteMask = Blend->ColorWriteMask;
        if (Blend->ColorWriteMask == RR_COLOR_COMPONENT_DEFAULT)
        {
            ColorWriteMask = RR_COLOR_COMPONENT_ALL;
        }
        Attachment->blendEnable = Blend->BlendEnable;
        Attachment->srcColorBlendFactor =
            Rr_ToVulkanBlendFactor(Blend->SrcColorBlendFactor);
        Attachment->dstColorBlendFactor =
            Rr_ToVulkanBlendFactor(Blend->DstColorBlendFactor);
        Attachment->colorBlendOp = Rr_ToVulkanBlendOp(Blend->ColorBlendOp);
        Attachment->srcAlphaBlendFactor =
            Rr_ToVulkanBlendFactor(Blend->SrcAlphaBlendFactor);
        Attachment->dstAlphaBlendFactor =
            Rr_ToVulkanBlendFactor(Blend->DstAlphaBlendFactor);
        Attachment->alphaBlendOp = Rr_ToVulkanBlendOp(Blend->AlphaBlendOp);
        Attachment->colorWriteMask = ColorWriteMask;
    }

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = (uint32_t)ColorAttachments.Count,
        .pAttachments = ColorAttachments.Data,
    };

    VkPipelineDepthStencilStateCreateInfo DepthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = CreateInfo->DepthStencil.EnableDepthTest,
        .depthWriteEnable = CreateInfo->DepthStencil.EnableDepthWrite,
        .depthCompareOp =
            Rr_ToVulkanCompareOp(CreateInfo->DepthStencil.CompareOp),
        .stencilTestEnable = CreateInfo->DepthStencil.EnableStencilTest,
        .front = Rr_ToVulkanStencilOpState(
            &CreateInfo->DepthStencil.FrontStencilState,
            &CreateInfo->DepthStencil),
        .back = Rr_ToVulkanStencilOpState(
            &CreateInfo->DepthStencil.BackStencilState,
            &CreateInfo->DepthStencil),
    };

    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = (uint32_t)ShaderStages.Count,
        .pStages = ShaderStages.Data,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &Rasterizer,
        .pMultisampleState = &Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &DepthStencil,
        .layout = PipelineLayout->Handle,
        .pDynamicState = &DynamicStateInfo,
        .renderPass = Rr_GetCompatibleRenderPass(
            (uint32_t)CreateInfo->ColorTargetCount,
            CreateInfo->ColorTargets,
            &CreateInfo->DepthStencil,
            Multisampling.rasterizationSamples),
    };

    VkPipeline Handle = VK_NULL_HANDLE;
    Result = Device->CreateGraphicsPipelines(
        Device->Handle,
        VK_NULL_HANDLE,
        1,
        &PipelineInfo,
        NULL,
        &Handle);

    Rr_GraphicsPipeline *GraphicsPipeline = NULL;

    if (Result == VK_SUCCESS)
    {
        Rr_LockSpinlock(&gRHI->GraphicsPipelinesLock);

        GraphicsPipeline = Rr_PushGraphicsPipelineIntoHive(
                               &gRHI->GraphicsPipelines,
                               Rr_GetPermanent())
                               .Element;

        Rr_UnlockSpinlock(&gRHI->GraphicsPipelinesLock);

        *GraphicsPipeline = (Rr_GraphicsPipeline){
            .Layout = PipelineLayout,
            .HasDepthStencil = HasDepthStencil,
            .Handle = Handle,
            .ColorAttachmentCount = (uint32_t)CreateInfo->ColorTargetCount,
        };

        Rr_ConsumeNextObjectName(GraphicsPipeline->Name);

        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_PIPELINE,
            (uint64_t)GraphicsPipeline->Handle,
            GraphicsPipeline->Name);
    }
    else
    {
        /* TODO: Set error etc... */
    }

    if (VertModule != VK_NULL_HANDLE)
    {
        Device->DestroyShaderModule(Device->Handle, VertModule, NULL);
    }

    if (FragModule != VK_NULL_HANDLE)
    {
        Device->DestroyShaderModule(Device->Handle, FragModule, NULL);
    }

    Rr_DestroyScratch(Scratch);

    return GraphicsPipeline;
}

void Rr_ReleaseGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipeline)
{
    if (GraphicsPipeline == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->ReleasedGraphicsPipelinesLock);

    Rr_InsertIntoHandleSet(
        &gRHI->ReleasedGraphicsPipelines,
        (Rr_Handle const *)&GraphicsPipeline,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->ReleasedGraphicsPipelinesLock);
}

void Rr_DestroyGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipeline)
{
    assert(GraphicsPipeline && GraphicsPipeline->Handle != VK_NULL_HANDLE);

    Rr_Device *Device = Rr_GetDevice();

    Device->DestroyPipeline(Device->Handle, GraphicsPipeline->Handle, NULL);

    Rr_LockSpinlock(&gRHI->GraphicsPipelinesLock);

    Rr_GraphicsPipelineHiveIterator It = Rr_GetGraphicsPipelineHiveIterator(
        &gRHI->GraphicsPipelines,
        GraphicsPipeline);
    Rr_EraseFromGraphicsPipelineHive(&gRHI->GraphicsPipelines, &It);

    Rr_UnlockSpinlock(&gRHI->GraphicsPipelinesLock);
}

Rr_DescriptorSetLayout *Rr_GetDescriptorSetLayout(
    Rr_DescriptorSetLayoutKey const *Key)
{
    Rr_LockSpinlock(&gRHI->DescriptorSetLayoutStorageLock);

    size_t HashSize = sizeof(Rr_DescriptorSetLayoutKey);
    Rr_DescriptorSetLayout **MapRef = &gRHI->DescriptorSetLayoutStorage.Map;
    Rr_DescriptorSetLayout *DescriptorSetLayout = NULL;

    for (uint64_t Hash = Rr_Hash64(HashSize, Key); *MapRef; Hash <<= 2)
    {
        if ((*MapRef)->Handle == VK_NULL_HANDLE)
        {
            DescriptorSetLayout = *MapRef;

            goto FoundEmpty;
        }
        if (memcmp(Key, &(*MapRef)->Key, HashSize) == 0)
        {
            Rr_UnlockSpinlock(&gRHI->DescriptorSetLayoutStorageLock);

            return *MapRef;
        }
        MapRef = &(*MapRef)->Children[Hash >> 62];
    }
    *MapRef = Rr_PushDescriptorSetLayoutIntoHive(
                  &gRHI->DescriptorSetLayoutStorage.Hive,
                  Rr_GetPermanent())
                  .Element;
    DescriptorSetLayout = *MapRef;
    RR_ZERO_PTR(DescriptorSetLayout);

FoundEmpty:

    DescriptorSetLayout->Key = *Key;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = Rr_GetDevice();

    RR_ARRAY(VkDescriptorSetLayoutBinding) VkBindings = { 0 };

    for (uint32_t BindingIndex = 0; BindingIndex < RR_MAX_BINDINGS;
         ++BindingIndex)
    {
        Rr_VulkanBinding const *Binding = Key->Bindings + BindingIndex;

        if (Binding->Count == 0)
        {
            continue;
        }

        *RR_PUSH_INTO_ARRAY(&VkBindings, Scratch.Arena) =
            (VkDescriptorSetLayoutBinding){
                .binding = Binding->Index,
                .descriptorType = Binding->Type,
                .descriptorCount = Binding->Count,
                .stageFlags = Binding->Stages,
            };
    }

    VkDescriptorSetLayoutCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (uint32_t)VkBindings.Count,
        .pBindings = VkBindings.Data,
    };

    Device->CreateDescriptorSetLayout(
        Device->Handle,
        &CreateInfo,
        NULL,
        &DescriptorSetLayout->Handle);

    Rr_UnlockSpinlock(&gRHI->DescriptorSetLayoutStorageLock);

    Rr_DestroyScratch(Scratch);

    return *MapRef;
}

#define RR_MIN_LEFTOVERS_SIZE 1024

/* TODO: Per memory type locking? */

void Rr_InitAllocator(
    Rr_Allocator *Allocator,
    Rr_PhysicalDevice *PhysicalDevice)
{
    uint32_t MemoryTypeCount = PhysicalDevice->MemoryProperties.memoryTypeCount;
    VkMemoryType *MemoryTypes = PhysicalDevice->MemoryProperties.memoryTypes;
    VkMemoryHeap *MemoryHeaps = PhysicalDevice->MemoryProperties.memoryHeaps;
    VkPhysicalDeviceLimits *Limits = &PhysicalDevice->Properties.limits;

    Rr_Arena *Arena = Rr_GetPermanent();

    Allocator->BufferImageGranularity = Limits->bufferImageGranularity;
    Allocator->NonCoherentAtomSize = Limits->nonCoherentAtomSize;
    Allocator->BigChunkSize = RR_BIG_CHUNK_SIZE;
    Allocator->SmallChunkSize = RR_SMALL_CHUNK_SIZE;

    Allocator->MemoryTypeCount = MemoryTypeCount;
    Allocator->MemoryTypes =
        Rr_Alloc(sizeof(Rr_MemoryType) * MemoryTypeCount, Arena);

    for (uint32_t Index = 0; Index < MemoryTypeCount; ++Index)
    {
        VkMemoryType *MemoryType = &MemoryTypes[Index];
        VkMemoryHeap *MemoryHeap = &MemoryHeaps[MemoryType->heapIndex];

        Allocator->MemoryTypes[Index].HeapSize = MemoryHeap->size;
        Allocator->MemoryTypes[Index].DeviceLocalHeap =
            MemoryHeap->flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
        if (MemoryHeap->size > Allocator->BigChunkSize)
        {
            Allocator->MemoryTypes[Index].ChunkSize = Allocator->BigChunkSize;
        }
        else if (MemoryHeap->size > Allocator->SmallChunkSize)
        {
            Allocator->MemoryTypes[Index].ChunkSize = Allocator->SmallChunkSize;
        }
        else
        {
            Allocator->MemoryTypes[Index].ChunkSize = MemoryHeap->size / 2;
        }
        Allocator->MemoryTypes[Index].PropertyFlags = MemoryType->propertyFlags;
    }
}

void Rr_CleanupAllocator(Rr_Allocator *Allocator)
{
    Rr_Device *Device = Rr_GetDevice();

    Rr_LockSpinlock(&Allocator->Lock);

    uint32_t LeakedMappings = 0;
    size_t DeviceLocalMemoryFreed = 0;
    for (size_t Index = 0; Index < Allocator->MemoryTypeCount; ++Index)
    {
        Rr_MemoryType *MemoryType = &Allocator->MemoryTypes[Index];

        Rr_ChunkHiveIterator It = Rr_BeginInChunkHive(&MemoryType->ChunkHive);
        while (!Rr_IsChunkHiveEnd(&MemoryType->ChunkHive, &It))
        {
            Rr_Chunk *Chunk = It.Element;

            if (Chunk->MappedData)
            {
                Device->UnmapMemory(Device->Handle, Chunk->Memory);
            }

            Device->FreeMemory(Device->Handle, Chunk->Memory, NULL);

            LeakedMappings += Chunk->MappingCount;

            if (MemoryType->DeviceLocalHeap)
            {
                DeviceLocalMemoryFreed += Chunk->Size;
            }

            Allocator->HardAllocationCount--;

            Rr_NextInChunkHive(&It);
        }

        Rr_ClearChunkHive(&MemoryType->ChunkHive);
    }

    Rr_ClearRangeHive(&Allocator->RangeHive);

    if (Allocator->SoftAllocationCount)
    {
        RR_LOG_WARNING(
            "Leaked %u soft allocations",
            Allocator->SoftAllocationCount);
    }
    if (Allocator->HardAllocationCount)
    {
        RR_LOG_WARNING(
            "Leaked %u hard allocations",
            Allocator->HardAllocationCount);
    }
    if (LeakedMappings)
    {
        RR_LOG_WARNING("Leaked %u memory mappings", LeakedMappings);
    }
    if (DeviceLocalMemoryFreed)
    {
        RR_LOG_INFO(
            "Freed %.2f mebibytes of pooled device local memory",
            (double)DeviceLocalMemoryFreed / (double)RR_MEBIBYTES(1));
    }

    Rr_UnlockSpinlock(&Allocator->Lock);
}

static inline Rr_Range *Rr_GetRange(Rr_Allocator *Allocator, Rr_Arena *Arena)
{
    return memset(
        Rr_PushRangeIntoHive(&Allocator->RangeHive, Arena).Element,
        0,
        sizeof(Rr_Range));
}

static inline void Rr_ReturnRange(Rr_Allocator *Allocator, Rr_Range *Range)
{
    Rr_RangeHiveIterator It =
        Rr_GetRangeHiveIterator(&Allocator->RangeHive, Range);
    Rr_EraseFromRangeHive(&Allocator->RangeHive, &It);
}

static inline uint32_t Rr_FindMemoryType(
    Rr_Allocator *Allocator,
    uint32_t Filter,
    VkMemoryPropertyFlags RequiredFlags,
    VkMemoryPropertyFlags PreferredFlags)
{
    VkPhysicalDeviceMemoryProperties *MemoryProperties =
        &gRHI->PhysicalDevice.MemoryProperties;

    /* First pass: required and preferred. */

    for (uint32_t Index = 0; Index < MemoryProperties->memoryTypeCount; ++Index)
    {
        bool PassesFilter = Filter & (1 << Index);
        if (!PassesFilter)
        {
            continue;
        }

        VkMemoryType *MemoryType = &MemoryProperties->memoryTypes[Index];

        if ((MemoryType->propertyFlags & RequiredFlags) == RequiredFlags &&
            (MemoryType->propertyFlags & PreferredFlags) == PreferredFlags)
        {
            return Index;
        }
    }

    /* Second pass: required only. */

    for (uint32_t Index = 0; Index < MemoryProperties->memoryTypeCount; ++Index)
    {
        bool PassesFilter = Filter & (1 << Index);
        if (!PassesFilter)
        {
            continue;
        }

        VkMemoryType *MemoryType = &MemoryProperties->memoryTypes[Index];

        if ((MemoryType->propertyFlags & RequiredFlags) == RequiredFlags)
        {
            return Index;
        }
    }

    return VK_MAX_MEMORY_TYPES;
}

static inline Rr_Chunk *Rr_AllocateChunk(
    Rr_Allocator *Allocator,
    uint32_t MemoryTypeIndex,
    VkDeviceSize Size)
{
    Rr_Device *Device = Rr_GetDevice();

    VkMemoryAllocateInfo MemoryAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = Size,
        .memoryTypeIndex = MemoryTypeIndex,
    };
    VkDeviceMemory DeviceMemory;
    if (Device->AllocateMemory(
            Device->Handle,
            &MemoryAllocateInfo,
            NULL,
            &DeviceMemory) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Couldn't not allocate a chunk of memory!");

        return NULL;
    }
    Allocator->HardAllocationCount++;

    Rr_MemoryType *MemoryType = &Allocator->MemoryTypes[MemoryTypeIndex];
    Rr_Arena *Arena = Rr_GetPermanent();

    Rr_Range *Range = Rr_GetRange(Allocator, Arena);
    *Range = (Rr_Range){
        .Size = Size,
        .Free = true,
    };

    Rr_Chunk *Chunk =
        Rr_PushChunkIntoHive(&MemoryType->ChunkHive, Arena).Element;
    *Chunk = (Rr_Chunk){
        .Size = Size,
        .Memory = DeviceMemory,
        .MemoryTypeIndex = MemoryTypeIndex,
        .FirstRange = Range,
        .FirstFreeRange = Range,
    };

    return Chunk;
}

static inline VkDeviceSize Rr_AlignVulkanOffset(
    VkDeviceSize Offset,
    VkDeviceSize Alignment)
{
    if (Offset % Alignment == 0)
    {
        return Offset;
    }

    return (Offset / Alignment) * Alignment + Alignment;
}

static inline bool Rr_FindChunkAndRange(
    Rr_Allocator *Allocator,
    uint32_t MemoryTypeIndex,
    VkDeviceSize Size,
    VkDeviceSize Alignment,
    Rr_Chunk **OutChunk,
    Rr_Range **OutRange)
{
    Rr_MemoryType *MemoryType = &Allocator->MemoryTypes[MemoryTypeIndex];

    Rr_LockSpinlock(&Allocator->Lock);

    if (Size > MemoryType->ChunkSize)
    {
        /* NOTE: Dedicated allocation. */

        Rr_Chunk *Chunk = Rr_AllocateChunk(Allocator, MemoryTypeIndex, Size);

        if (!Chunk)
        {
            Rr_UnlockSpinlock(&Allocator->Lock);

            return false;
        }

        Chunk->Dedicated = true;
        Chunk->FirstFreeRange = NULL;
        Chunk->FirstRange->Free = false;

        *OutChunk = Chunk;
        *OutRange = Chunk->FirstRange;

        Rr_UnlockSpinlock(&Allocator->Lock);

        return true;
    }

    Rr_ChunkHiveIterator It = Rr_BeginInChunkHive(&MemoryType->ChunkHive);
    while (true)
    {
        Rr_Chunk *Chunk = It.Element;

        if (Rr_IsChunkHiveEnd(&MemoryType->ChunkHive, &It))
        {
            Chunk = Rr_AllocateChunk(
                Allocator,
                MemoryTypeIndex,
                MemoryType->ChunkSize);

            if (!Chunk)
            {
                Rr_UnlockSpinlock(&Allocator->Lock);

                return false;
            }

            It = Rr_GetChunkHiveIterator(&MemoryType->ChunkHive, Chunk);
        }
        else if (Chunk->Dedicated)
        {
            Rr_NextInChunkHive(&It);

            continue;
        }

        Rr_Range *PreviousRange = NULL;
        Rr_Range **RangeRef = &Chunk->FirstFreeRange;
        Rr_Range *Range = Chunk->FirstFreeRange;
        while (Range)
        {
            VkDeviceSize AlignedOffset =
                Rr_AlignVulkanOffset(Range->Offset, Alignment);
            VkDeviceSize AlignedDelta = AlignedOffset - Range->Offset;
            VkDeviceSize AlignedAvailableSize = Range->Size - AlignedDelta;
            if (Size <= AlignedAvailableSize)
            {
                /* Claim this range; put leftovers (if any) into a new one. */

                VkDeviceSize Leftovers = AlignedAvailableSize - Size;
                Rr_Range *NewRange = NULL;
                if (Leftovers >= RR_MIN_LEFTOVERS_SIZE)
                {
                    Range->Size -= Leftovers;

                    NewRange = Rr_GetRange(Allocator, Rr_GetPermanent());
                    NewRange->Offset = Range->Offset + Range->Size;
                    NewRange->Size = Leftovers;
                    NewRange->Free = true;

                    NewRange->Previous = Range;
                    NewRange->Next = Range->Next;

                    NewRange->PreviousFree = PreviousRange;
                    NewRange->NextFree = Range->NextFree;

                    Range->Next = NewRange;
                }

                *RangeRef = NewRange ? NewRange : Range->NextFree;

                Range->AlignedOffset = AlignedOffset;
                Range->Free = false;
                Range->PreviousFree = NULL;
                Range->NextFree = NULL;

                Rr_UnlockSpinlock(&Allocator->Lock);

                *OutChunk = Chunk;
                *OutRange = Range;

                return true;
            }

            PreviousRange = Range;
            RangeRef = &Range->NextFree;
            Range = *RangeRef;
        }

        Rr_NextInChunkHive(&It);
    }

    Rr_UnlockSpinlock(&Allocator->Lock);

    RR_LOG_ERROR("Failed to find appropriate sub allocation!");

    return false;
}

static inline void Rr_FreeChunkAndRange(
    Rr_Allocator *Allocator,
    Rr_Chunk *Chunk,
    Rr_Range *Range)
{
    Rr_LockSpinlock(&Allocator->Lock);

    if (Chunk->Dedicated)
    {
        Rr_Device *Device = Rr_GetDevice();

        Device->FreeMemory(Device->Handle, Chunk->Memory, NULL);

        Rr_ReturnRange(Allocator, Range);

        Rr_MemoryType *MemoryType =
            &Allocator->MemoryTypes[Chunk->MemoryTypeIndex];
        Rr_ChunkHiveIterator It =
            Rr_GetChunkHiveIterator(&MemoryType->ChunkHive, Chunk);
        Rr_EraseFromChunkHive(&MemoryType->ChunkHive, &It);

        Allocator->HardAllocationCount--;

        Rr_UnlockSpinlock(&Allocator->Lock);

        return;
    }

    Range->Free = true;

    /* Coalesce in both directions. */

    Rr_Range *RangeToTheLeft = Range->Previous;
    if (RangeToTheLeft && RangeToTheLeft->Free)
    {
        Range->Offset = RangeToTheLeft->Offset;
        Range->Size += RangeToTheLeft->Size;

        Range->Previous = RangeToTheLeft->Previous;
        Range->PreviousFree = RangeToTheLeft->PreviousFree;

        if (Chunk->FirstFreeRange == RangeToTheLeft)
        {
            Chunk->FirstFreeRange = Range;
        }

        if (Chunk->FirstRange == RangeToTheLeft)
        {
            Chunk->FirstRange = Range;
        }

        Rr_ReturnRange(Allocator, RangeToTheLeft);
    }

    Rr_Range *RangeToTheRight = Range->Next;
    if (RangeToTheRight && RangeToTheRight->Free)
    {
        Range->Size += RangeToTheRight->Size;

        Range->Next = RangeToTheRight->Next;
        Range->NextFree = RangeToTheRight->NextFree;

        if (Chunk->FirstFreeRange == RangeToTheRight)
        {
            Chunk->FirstFreeRange = Range;
        }

        Rr_ReturnRange(Allocator, RangeToTheRight);
    }

    if (Range->Next)
    {
        Range->Next->Previous = Range;
    }
    if (Range->Previous)
    {
        Range->Previous->Next = Range;
    }
    if (Range->NextFree)
    {
        Range->NextFree->PreviousFree = Range;
    }
    if (Range->PreviousFree)
    {
        Range->PreviousFree->NextFree = Range;
    }

    Chunk->SoftAllocationCount--;
    Allocator->SoftAllocationCount--;

    Rr_UnlockSpinlock(&Allocator->Lock);
}

static inline bool Rr_BindAllocatedBuffer(
    Rr_Allocator *Allocator,
    Rr_AllocatedBuffer *AllocatedBuffer,
    VkDeviceSize Size,
    VkDeviceSize Alignment,
    uint32_t MemoryTypeIndex,
    bool Mapped)
{
    if (AllocatedBuffer->Chunk || AllocatedBuffer->Range)
    {
        RR_LOG_ERROR("Buffer memory is already bound!");

        return false;
    }

    Rr_Device *Device = Rr_GetDevice();

    if (!Rr_FindChunkAndRange(
            Allocator,
            MemoryTypeIndex,
            Size,
            Alignment,
            &AllocatedBuffer->Chunk,
            &AllocatedBuffer->Range))
    {
        return false;
    }

    if (Mapped && !Rr_MapAllocatedBufferMemory(Allocator, AllocatedBuffer))
    {
        return false;
    }

    if (Device->BindBufferMemory(
            Device->Handle,
            AllocatedBuffer->Handle,
            AllocatedBuffer->Chunk->Memory,
            AllocatedBuffer->Range->AlignedOffset) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Failed to bind buffer memory!");

        return false;
    }

    if (!AllocatedBuffer->Chunk->Dedicated)
    {
        Rr_LockSpinlock(&Allocator->Lock);

        AllocatedBuffer->Chunk->SoftAllocationCount++;
        Allocator->SoftAllocationCount++;

        Rr_UnlockSpinlock(&Allocator->Lock);
    }

    return true;
}

bool Rr_AllocBufferMemory(Rr_Allocator *Allocator, Rr_Buffer *Buffer)
{
    Rr_Device *Device = Rr_GetDevice();

    VkMemoryRequirements MemoryRequirements;
    Device->GetBufferMemoryRequirements(
        Device->Handle,
        Buffer->AllocatedBuffers[0].Handle,
        &MemoryRequirements);
    if (MemoryRequirements.size == 0)
    {
        RR_LOG_ERROR("Invalid memory requirements for buffer!");

        return false;
    }

    VkMemoryPropertyFlags RequiredFlags = 0;
    VkMemoryPropertyFlags PreferredFlags = 0;

    if (Buffer->Flags & RR_BUFFER_FLAGS_READBACK_BIT)
    {
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        PreferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    if (Buffer->Flags & RR_BUFFER_FLAGS_STAGING_BIT)
    {
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    if (Buffer->Flags & RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT)
    {
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    if (Buffer->Flags & RR_BUFFER_FLAGS_UNIFORM_BIT ||
        Buffer->Flags & RR_BUFFER_FLAGS_STORAGE_BIT ||
        Buffer->Flags & RR_BUFFER_FLAGS_VERTEX_BIT ||
        Buffer->Flags & RR_BUFFER_FLAGS_INDEX_BIT ||
        Buffer->Flags & RR_BUFFER_FLAGS_INDIRECT_BIT)
    {
        PreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    size_t AllocatedIndex = 0;
    uint32_t MemoryTypeFilter = MemoryRequirements.memoryTypeBits;
    while (true)
    {
        /* NOTE: This loop allows to allocate from different memory types. */

        uint32_t MemoryTypeIndex = Rr_FindMemoryType(
            Allocator,
            MemoryTypeFilter,
            RequiredFlags,
            PreferredFlags);
        if (MemoryTypeIndex == VK_MAX_MEMORY_TYPES)
        {
            RR_LOG_ERROR("Failed to find appropriate memory type!");

            break;
        }

        for (; AllocatedIndex < Buffer->AllocatedBufferCount; ++AllocatedIndex)
        {
            Rr_AllocatedBuffer *AllocatedBuffer =
                &Buffer->AllocatedBuffers[AllocatedIndex];
            if (!Rr_BindAllocatedBuffer(
                    Allocator,
                    AllocatedBuffer,
                    MemoryRequirements.size,
                    MemoryRequirements.alignment,
                    MemoryTypeIndex,
                    Buffer->Flags & RR_BUFFER_FLAGS_MAPPED_BIT))
            {
                break;
            }
        }

        if (AllocatedIndex == Buffer->AllocatedBufferCount)
        {
            return true;
        }

        MemoryTypeFilter &= ~(1U << MemoryTypeIndex);
    }

    Rr_FreeBufferMemory(Allocator, Buffer);

    return false;
}

void Rr_FreeBufferMemory(Rr_Allocator *Allocator, Rr_Buffer *Buffer)
{
    for (size_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        Rr_Chunk *Chunk = AllocatedBuffer->Chunk;
        Rr_Range *Range = AllocatedBuffer->Range;
        if (!Chunk || !Range)
        {
            continue;
        }

        if (Buffer->Flags & RR_BUFFER_FLAGS_MAPPED_BIT)
        {
            Rr_UnmapAllocatedBufferMemory(Allocator, AllocatedBuffer);
        }

        Rr_FreeChunkAndRange(Allocator, Chunk, Range);
    }
}

void *Rr_MapAllocatedBufferMemory(
    Rr_Allocator *Allocator,
    Rr_AllocatedBuffer *AllocatedBuffer)
{
    if (AllocatedBuffer->MappedData)
    {
        return AllocatedBuffer->MappedData;
    }

    Rr_Device *Device = Rr_GetDevice();

    Rr_Chunk *Chunk = AllocatedBuffer->Chunk;
    Rr_Range *Range = AllocatedBuffer->Range;

    Rr_LockSpinlock(&Allocator->Lock);

    if (!Chunk->MappedData)
    {
        if (Device->MapMemory(
                Device->Handle,
                Chunk->Memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &Chunk->MappedData) != VK_SUCCESS)
        {
            Rr_UnlockSpinlock(&Allocator->Lock);

            RR_LOG_ERROR("Failed to map a chunk of memory!");

            return NULL;
        }
    }

    AllocatedBuffer->MappedData =
        (uint8_t *)Chunk->MappedData + Range->AlignedOffset;

    Chunk->MappingCount++;

    Rr_UnlockSpinlock(&Allocator->Lock);

    return (uint8_t *)Chunk->MappedData + Range->AlignedOffset;
}

void Rr_UnmapAllocatedBufferMemory(
    Rr_Allocator *Allocator,
    Rr_AllocatedBuffer *AllocatedBuffer)
{
    if (!AllocatedBuffer->MappedData)
    {
        return;
    }

    Rr_Chunk *Chunk = AllocatedBuffer->Chunk;

    Rr_LockSpinlock(&Allocator->Lock);

    if (Chunk->MappedData)
    {
        AllocatedBuffer->MappedData = NULL;

        if (--Chunk->MappingCount == 0)
        {
            Rr_Device *Device = Rr_GetDevice();

            Device->UnmapMemory(Device->Handle, Chunk->Memory);

            Chunk->MappedData = NULL;
        }
    }

    Rr_UnlockSpinlock(&Allocator->Lock);
}

void Rr_FlushAllocatedBufferMemory(
    Rr_Allocator *Allocator,
    Rr_AllocatedBuffer *AllocatedBuffer,
    size_t Offset,
    size_t Size)
{
    Rr_Device *Device = Rr_GetDevice();

    if (Device->FlushMappedMemoryRanges(
            Device->Handle,
            1,
            &(VkMappedMemoryRange){
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = AllocatedBuffer->Chunk->Memory,
                .offset = AllocatedBuffer->Range->AlignedOffset,
                .size = Size,
            }) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Failed to flush buffer memory!");
    }
}

static inline bool Rr_BindAllocatedImage(
    Rr_Allocator *Allocator,
    Rr_AllocatedImage *AllocatedImage,
    VkDeviceSize Size,
    VkDeviceSize Alignment,
    uint32_t MemoryTypeIndex)
{
    if (AllocatedImage->Chunk || AllocatedImage->Range)
    {
        RR_LOG_ERROR("Image memory is already bound!");

        return false;
    }

    Rr_Device *Device = Rr_GetDevice();

    if (!Rr_FindChunkAndRange(
            Allocator,
            MemoryTypeIndex,
            Size,
            Alignment,
            &AllocatedImage->Chunk,
            &AllocatedImage->Range))
    {
        return false;
    }

    if (Device->BindImageMemory(
            Device->Handle,
            AllocatedImage->Handle,
            AllocatedImage->Chunk->Memory,
            AllocatedImage->Range->AlignedOffset) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Failed to bind image memory!");

        return false;
    }

    if (!AllocatedImage->Chunk->Dedicated)
    {
        Rr_LockSpinlock(&Allocator->Lock);

        AllocatedImage->Chunk->SoftAllocationCount++;
        Allocator->SoftAllocationCount++;

        Rr_UnlockSpinlock(&Allocator->Lock);
    }

    return true;
}

bool Rr_AllocImageMemory(Rr_Allocator *Allocator, Rr_Image *Image)
{
    Rr_Device *Device = Rr_GetDevice();

    VkMemoryRequirements MemoryRequirements;
    Device->GetImageMemoryRequirements(
        Device->Handle,
        Image->AllocatedImages[0].Handle,
        &MemoryRequirements);
    if (MemoryRequirements.size == 0)
    {
        RR_LOG_ERROR("Invalid memory requirements for image!");

        return false;
    }

    VkMemoryPropertyFlags RequiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    size_t AllocatedIndex = 0;
    uint32_t MemoryTypeFilter = MemoryRequirements.memoryTypeBits;
    while (true)
    {
        /* NOTE: This loop allows to allocate from different memory types. */

        uint32_t MemoryTypeIndex =
            Rr_FindMemoryType(Allocator, MemoryTypeFilter, RequiredFlags, 0);
        if (MemoryTypeIndex == VK_MAX_MEMORY_TYPES)
        {
            RR_LOG_ERROR("Failed to find appropriate memory type!");

            break;
        }

        for (; AllocatedIndex < Image->AllocatedImageCount; ++AllocatedIndex)
        {
            Rr_AllocatedImage *AllocatedImage =
                &Image->AllocatedImages[AllocatedIndex];
            if (!Rr_BindAllocatedImage(
                    Allocator,
                    AllocatedImage,
                    RR_ALIGN_POW2(
                        MemoryRequirements.size,
                        Allocator->BufferImageGranularity),
                    RR_MAX(
                        MemoryRequirements.alignment,
                        Allocator->BufferImageGranularity),
                    MemoryTypeIndex))
            {
                break;
            }
        }

        if (AllocatedIndex == Image->AllocatedImageCount)
        {
            return true;
        }

        MemoryTypeFilter &= ~(1U << MemoryTypeIndex);
    }

    Rr_FreeImageMemory(Allocator, Image);

    return false;
}

void Rr_FreeImageMemory(Rr_Allocator *Allocator, Rr_Image *Image)
{
    for (size_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Rr_AllocatedImage *AllocatedImage = &Image->AllocatedImages[Index];
        Rr_Chunk *Chunk = AllocatedImage->Chunk;
        Rr_Range *Range = AllocatedImage->Range;
        if (!Chunk || !Range)
        {
            continue;
        }

        Rr_FreeChunkAndRange(Allocator, Chunk, Range);
    }
}

void Rr_InitRHI(const char *Title)
{
    Rr_Arena *Arena = Rr_GetPermanent();

    gRHI = Rr_Alloc(sizeof(Rr_RHI), Arena);

    Rr_InitLoader(&gRHI->Loader);
    Rr_InitInstance(&gRHI->Loader, Title, &gRHI->Instance);
    Rr_InitSurface(&gRHI->Instance, &gRHI->Surface);
    Rr_InitDeviceAndQueues(
        &gRHI->Instance,
        gRHI->Surface,
        &gRHI->PhysicalDevice,
        Rr_GetDevice(),
        &gRHI->MainQueue,
        &gRHI->DedicatedTransferQueue);

    Rr_InitAllocator(&gRHI->Allocator, &gRHI->PhysicalDevice);
    Rr_InitFrames();
    Rr_InitSwapchain();
    Rr_InitEmptyDescriptorSet();

    Rr_InitFramebufferMap(&gRHI->FramebufferMap, Arena);
    Rr_InitRenderPassMap(&gRHI->RenderPassMap, Arena);

    Rr_InitHandleSet(&gRHI->ReleasedBuffers, Arena);
    Rr_InitHandleSet(&gRHI->ReleasedImages, Arena);
    Rr_InitHandleSet(&gRHI->ReleasedSamplers, Arena);
    Rr_InitHandleSet(&gRHI->ReleasedComputePipelines, Arena);
    Rr_InitHandleSet(&gRHI->ReleasedGraphicsPipelines, Arena);
}

static inline void Rr_DestroyReleasedObjects(void)
{
    Rr_LockSpinlock(&gRHI->ReleasedBuffersLock);

    for (Rr_HandleSetIterator It = Rr_BeginInHandleSet(&gRHI->ReleasedBuffers);
         !Rr_IsHandleSetEnd(&It);)
    {
        Rr_Buffer *Buffer = (Rr_Buffer *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&Buffer->RefCount))
        {
            Rr_DestroyBuffer(Buffer);
            Rr_EraseFromHandleSet(&It);
        }
        else
        {
            Rr_NextInHandleSet(&It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedBuffersLock);

    Rr_LockSpinlock(&gRHI->ReleasedImagesLock);

    for (Rr_HandleSetIterator It = Rr_BeginInHandleSet(&gRHI->ReleasedImages);
         !Rr_IsHandleSetEnd(&It);)
    {
        Rr_Image *Image = (Rr_Image *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&Image->RefCount))
        {
            Rr_DestroyImage(Image);
            Rr_EraseFromHandleSet(&It);
        }
        else
        {
            Rr_NextInHandleSet(&It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedImagesLock);

    Rr_LockSpinlock(&gRHI->ReleasedSamplersLock);

    for (Rr_HandleSetIterator It = Rr_BeginInHandleSet(&gRHI->ReleasedSamplers);
         !Rr_IsHandleSetEnd(&It);)
    {
        Rr_Sampler *Sampler = (Rr_Sampler *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&Sampler->RefCount))
        {
            Rr_DestroySampler(Sampler);
            Rr_EraseFromHandleSet(&It);
        }
        else
        {
            Rr_NextInHandleSet(&It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedSamplersLock);

    Rr_LockSpinlock(&gRHI->ReleasedComputePipelinesLock);

    for (Rr_HandleSetIterator It =
             Rr_BeginInHandleSet(&gRHI->ReleasedComputePipelines);
         !Rr_IsHandleSetEnd(&It);)
    {
        Rr_ComputePipeline *ComputePipeline =
            (Rr_ComputePipeline *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&ComputePipeline->RefCount))
        {
            Rr_DestroyComputePipeline(ComputePipeline);
            Rr_EraseFromHandleSet(&It);
        }
        else
        {
            Rr_NextInHandleSet(&It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedComputePipelinesLock);

    Rr_LockSpinlock(&gRHI->ReleasedGraphicsPipelinesLock);

    for (Rr_HandleSetIterator It =
             Rr_BeginInHandleSet(&gRHI->ReleasedGraphicsPipelines);
         !Rr_IsHandleSetEnd(&It);)
    {
        Rr_GraphicsPipeline *GraphicsPipeline =
            (Rr_GraphicsPipeline *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&GraphicsPipeline->RefCount))
        {
            Rr_DestroyGraphicsPipeline(GraphicsPipeline);
            Rr_EraseFromHandleSet(&It);
        }
        else
        {
            Rr_NextInHandleSet(&It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedGraphicsPipelinesLock);
}

void Rr_CleanupRHI(void)
{
    Rr_Instance *Instance = &gRHI->Instance;
    Rr_Device *Device = Rr_GetDevice();

    Rr_WaitIdle();

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Graph *Graph = gRHI->Frames[Index].Graph;

        if (Graph)
        {
            Rr_ReleaseGraphResources(Graph);
        }
    }

    Rr_DestroyReleasedObjects();

    for (Rr_PipelineLayoutHiveIterator It =
             Rr_BeginInPipelineLayoutHive(&gRHI->PipelineLayoutStorage.Hive);
         !Rr_IsPipelineLayoutHiveEnd(&gRHI->PipelineLayoutStorage.Hive, &It);
         Rr_NextInPipelineLayoutHive(&It))
    {
        Device->DestroyPipelineLayout(Device->Handle, It.Element->Handle, NULL);
    }

    for (Rr_DescriptorSetLayoutHiveIterator It =
             Rr_BeginInDescriptorSetLayoutHive(
                 &gRHI->DescriptorSetLayoutStorage.Hive);
         !Rr_IsDescriptorSetLayoutHiveEnd(
             &gRHI->DescriptorSetLayoutStorage.Hive,
             &It);
         Rr_NextInDescriptorSetLayoutHive(&It))
    {
        Device->DestroyDescriptorSetLayout(
            Device->Handle,
            It.Element->Handle,
            NULL);
    }

    Rr_CleanupEmptyDescriptorSet();

    /* NOTE: VkFramebuffers are destroyed along with VkImageViews.
     * For now, we don't care for destroying render passes unless it's
     * application shutdown. */

    for (Rr_RenderPassMapIterator It =
             Rr_BeginInRenderPassMap(&gRHI->RenderPassMap);
         !Rr_IsRenderPassMapEnd(&It);
         Rr_NextInRenderPassMap(&It))
    {
        Device->DestroyRenderPass(Device->Handle, It.Data->Value, NULL);
    }

    for (Rr_DescriptorPoolList *List = gRHI->DescriptorPoolList; List;
         List = List->Next)
    {
        Device->DestroyDescriptorPool(Device->Handle, List->Handle, NULL);
    }

    Rr_CleanupFrames();

    for (size_t Index = 0; Index < gRHI->SwapchainImages.Count; ++Index)
    {
        Rr_DestroySwapchainImage(&gRHI->SwapchainImages.Data[Index]);
    }

    if (gRHI->Swapchain.Handle != VK_NULL_HANDLE)
    {
        Device->DestroySwapchainKHR(
            gRHI->Device.Handle,
            gRHI->Swapchain.Handle,
            NULL);
    }

    Rr_ReleaseCommandPools();

    for (Rr_CommandPools *CommandPools = gRHI->FreeCommandPools; CommandPools;
         CommandPools = CommandPools->Next)
    {
        Device->DestroyCommandPool(
            Device->Handle,
            CommandPools->Graphics,
            NULL);
        Device->DestroyCommandPool(
            Device->Handle,
            CommandPools->Transfer,
            NULL);
        // Device->DestroyCommandPool(Device->Handle, CommandPools->Compute,
        // NULL);
    }

    for (size_t Index = 0; Index < gRHI->Semaphores.Count; ++Index)
    {
        Device->DestroySemaphore(
            Device->Handle,
            gRHI->Semaphores.Data[Index],
            NULL);
    }

    for (size_t Index = 0; Index < gRHI->Fences.Count; ++Index)
    {
        Device->DestroyFence(Device->Handle, gRHI->Fences.Data[Index], NULL);
    }

    Rr_CleanupAllocator(&gRHI->Allocator);

    Instance->DestroySurfaceKHR(Instance->Handle, gRHI->Surface, NULL);
    Device->DestroyDevice(Device->Handle, NULL);
    Instance->DestroyInstance(Instance->Handle, NULL);

    gRHI = NULL;
}

Rr_Device *Rr_GetDevice(void)
{
    return &gRHI->Device;
}

void Rr_WaitIdle(void)
{
    Rr_Device *Device = Rr_GetDevice();

    Device->DeviceWaitIdle(Device->Handle);
}

void Rr_SetSwapchainDirty(bool Dirty)
{
    gRHI->Swapchain.RecreatePending = Dirty;
}

bool Rr_HandleSwapchainRecreated(void)
{
    if (gRHI->Swapchain.Recreated)
    {
        gRHI->Swapchain.Recreated = false;

        return true;
    }

    return false;
}

void Rr_NewFrame(void)
{
    Rr_Device *Device = Rr_GetDevice();

    gRHI->FrameNumber++;
    gRHI->FrameIndex = gRHI->FrameNumber % RR_FRAME_OVERLAP;

    Rr_Frame *Frame = Rr_GetCurrentFrame();
    Rr_Arena *Arena = Frame->Arena;

    VkResult Result;

    /* Wait for previous work associated with given frame index. */

    if (Frame->SubmitFence != VK_NULL_HANDLE)
    {
        Result = Device->WaitForFences(
            Device->Handle,
            1,
            &Frame->SubmitFence,
            true,
            1000000000);
        assert(Result != VK_TIMEOUT && "Submit fence timeout!");

        if (gRHI->MainQueue.TimestampsEnabled)
        {
            uint64_t Timestamps[2];
            Device->GetQueryPoolResults(
                Device->Handle,
                Frame->QueryPool,
                0,
                2,
                sizeof(Timestamps),
                Timestamps,
                sizeof(uint64_t),
                VK_QUERY_RESULT_64_BIT);
            double Period =
                (double)gRHI->PhysicalDevice.Properties.limits.timestampPeriod;
            double DeltaNS = (double)(Timestamps[1] - Timestamps[0]);
            gRHI->LastFrameMS = Period * DeltaNS / 1000000.0;
        }

        Rr_ReleaseVulkanFence(Frame->SubmitFence);
        Frame->SubmitFence = VK_NULL_HANDLE;
    }

    size_t NodeCount = 0;
    size_t BufferCount = 0;
    size_t ImageCount = 0;
    size_t SamplerCount = 0;
    size_t ComputePipelineCount = 0;
    size_t GraphicsPipelineCount = 0;

    if (Frame->Graph)
    {
        NodeCount = Frame->Graph->Nodes.Count;
        BufferCount = Frame->Graph->BufferResources.Count;
        ImageCount = Frame->Graph->ImageResources.Count;
        SamplerCount = Frame->Graph->Samplers.Count;
        ComputePipelineCount = Frame->Graph->ComputePipelines.Count;
        GraphicsPipelineCount = Frame->Graph->GraphicsPipelines.Count;

        Rr_ReleaseGraphResources(Frame->Graph);
    }

    Rr_DestroyReleasedObjects();

    /* NOTE: Resets everything allocated last time! */

    Rr_ResetArena(Arena);

    Frame->Profiler = Rr_CreateProfiler(Arena);

    /* Acquire swapchain image. */

    uint32_t SwapchainImageIndex = UINT32_MAX;
    while (Rr_RecreateSwapchainIfNeeded())
    {
        Result = Device->AcquireNextImageKHR(
            Device->Handle,
            gRHI->Swapchain.Handle,
            1000000000,
            Frame->AcquireSemaphore,
            NULL,
            &SwapchainImageIndex);
        if (Result == VK_TIMEOUT)
        {
            RR_LOG_WARNING("Timeout acquiring swapchain image!");

            break;
        }
        if (Result == VK_SUCCESS)
        {
            break;
        }
        if (Result == VK_SUBOPTIMAL_KHR)
        {
            Rr_SetSwapchainDirty(true);
#ifdef __APPLE__
            /* https://github.com/KhronosGroup/MoltenVK/issues/2542 */
            Device->DestroySemaphore(
                Device->Handle,
                Frame->AcquireSemaphore,
                NULL);
            Frame->AcquireSemaphore = Rr_AcquireVulkanSemaphore();
            continue;
#else
            break;
#endif
        }
        Rr_SetSwapchainDirty(true);
    }

    if (SwapchainImageIndex != UINT32_MAX)
    {
        Frame->SwapchainImage =
            &gRHI->SwapchainImages.Data[SwapchainImageIndex];

        gRHI->Swapchain.Unavailable = false;
    }
    else
    {
        /* HACK: Use whatever swapchain image if for whatever reason the
         * swapchain is not available. We will ultimately skip issuing this
         * frame to the GPU but user might want to know its format/extent/etc.
         */

        Frame->SwapchainImage = &gRHI->SwapchainImages.Data[0];

        gRHI->Swapchain.Unavailable = true;
    }

    Rr_Graph *Graph = Rr_Alloc(sizeof(Rr_Graph), Arena);
    Graph->QueueType = RR_QUEUE_TYPE_MAIN;
    Graph->Primary = true;
    Graph->DescriptorPoolList = Rr_AcquireDescriptorPoolList();
    Graph->Arena = Arena;
    Rr_InitHandleSet(&Graph->Samplers, Arena);
    Rr_InitHandleSet(&Graph->ComputePipelines, Arena);
    Rr_InitHandleSet(&Graph->GraphicsPipelines, Arena);

    RR_RESERVE_ARRAY(&Graph->Nodes, NodeCount, Arena);
    RR_RESERVE_ARRAY(&Graph->BufferResources, BufferCount, Arena);
    RR_RESERVE_ARRAY(&Graph->ImageResources, ImageCount, Arena);
    Rr_ReserveHandleSet(&Graph->Samplers, SamplerCount, Arena);
    Rr_ReserveHandleSet(&Graph->ComputePipelines, ComputePipelineCount, Arena);
    Rr_ReserveHandleSet(
        &Graph->GraphicsPipelines,
        GraphicsPipelineCount,
        Arena);

    Frame->Graph = Graph;

    Graph->SwapchainImageHandle =
        Rr_GetGraphImageHandle(Frame->Graph, &Frame->SwapchainImage->Container);
}

void Rr_DrawFrame(void)
{
    if (gRHI->Swapchain.Unavailable)
    {
        return;
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = Rr_GetDevice();
    Rr_Swapchain *Swapchain = &gRHI->Swapchain;
    Rr_Frame *Frame = Rr_GetCurrentFrame();

    Frame->SubmitFence = Rr_AcquireVulkanFence();

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    /* Execute Frame Graph */

    Device->BeginCommandBuffer(
        Frame->EarlyCommandBuffer,
        &CommandBufferBeginInfo);
    Device->BeginCommandBuffer(
        Frame->LateCommandBuffer,
        &CommandBufferBeginInfo);

    if (gRHI->MainQueue.TimestampsEnabled)
    {
        Device->CmdResetQueryPool(
            Frame->EarlyCommandBuffer,
            Frame->QueryPool,
            0,
            4);
        Device->CmdWriteTimestamp(
            Frame->EarlyCommandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            Frame->QueryPool,
            0);
    }

    Rr_BeginFrameSection("Rr.FrameGraph");

    Rr_ExecuteGraph(
        Frame->Graph,
        gRHI->MainQueue.FamilyIndex,
        Frame->EarlyCommandBuffer,
        Frame->LateCommandBuffer);

    Rr_EndFrameSection("Rr.FrameGraph");

    Device->EndCommandBuffer(Frame->EarlyCommandBuffer);

    /* Always transition swapchain image to present layout. */

    Rr_AllocatedImage *AllocatedSwapchainImage =
        &Frame->SwapchainImage->Container.AllocatedImages[0];

    Device->CmdPipelineBarrier(
        Frame->LateCommandBuffer,
        AllocatedSwapchainImage->SyncState.StageMask,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &(VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image = AllocatedSwapchainImage->Handle,
            .oldLayout = AllocatedSwapchainImage->SyncState.Layout,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcAccessMask = AllocatedSwapchainImage->SyncState.AccessMask,
            .dstAccessMask = 0,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        });

    AllocatedSwapchainImage->SyncState = (Rr_SyncState){
        .AccessMask = 0,
        .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        .Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .QueueFamilyIndex = gRHI->MainQueue.FamilyIndex,
    };

    if (gRHI->MainQueue.TimestampsEnabled)
    {
        Device->CmdWriteTimestamp(
            Frame->LateCommandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            Frame->QueryPool,
            1);
    }

    Device->EndCommandBuffer(Frame->LateCommandBuffer);

    /* Submit frame command buffer and queue present. */

    VkSubmitInfo SubmitInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &Frame->EarlyCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &Frame->SwapchainImage->EarlySemaphore,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &Frame->LateCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &Frame->SwapchainImage->LateSemaphore,
            .waitSemaphoreCount = 2,
            .pWaitSemaphores =
                (VkSemaphore[]){
                    Frame->AcquireSemaphore,
                    Frame->SwapchainImage->EarlySemaphore,
                },
            .pWaitDstStageMask =
                (VkPipelineStageFlags[]){
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                },
        },
    };

    Rr_LockSpinlock(&gRHI->MainQueue.Lock);

    Device->QueueSubmit(
        gRHI->MainQueue.Handle,
        2,
        SubmitInfos,
        Frame->SubmitFence);

    uint32_t SwapchainImageIndex =
        (uint32_t)(Frame->SwapchainImage - gRHI->SwapchainImages.Data);
    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->SwapchainImage->LateSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    VkResult Result =
        Device->QueuePresentKHR(gRHI->MainQueue.Handle, &PresentInfo);

    Rr_UnlockSpinlock(&gRHI->MainQueue.Lock);

    if (Result == VK_SUBOPTIMAL_KHR || Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Rr_RecreateSwapchainIfNeeded();
    }

    Rr_DestroyScratch(Scratch);
}

bool Rr_HasQueue(Rr_QueueType QueueType)
{
    switch (QueueType)
    {
        case RR_QUEUE_TYPE_MAIN:
            return gRHI->MainQueue.Handle != VK_NULL_HANDLE;
        case RR_QUEUE_TYPE_DEDICATED_TRANSFER:
            return gRHI->DedicatedTransferQueue.Handle != VK_NULL_HANDLE;
        case RR_QUEUE_TYPE_ASYNC_COMPUTE:
            return gRHI->AsyncComputeQueue.Handle != VK_NULL_HANDLE;
        default:
            RR_LOG_ABORT("Invalid queue type!");
    }
}

Rr_Queue *Rr_GetQueue(Rr_QueueType QueueType)
{
    assert(Rr_HasQueue(QueueType));
    switch (QueueType)
    {
        case RR_QUEUE_TYPE_MAIN:
            return &gRHI->MainQueue;
        case RR_QUEUE_TYPE_DEDICATED_TRANSFER:
            return &gRHI->DedicatedTransferQueue;
        case RR_QUEUE_TYPE_ASYNC_COMPUTE:
            return &gRHI->AsyncComputeQueue;
        default:
            RR_LOG_ABORT("Invalid queue type!");
    }
}

static inline Rr_Frame *Rr_GetPreviousFrame(void)
{
    return &gRHI->Frames[(gRHI->FrameNumber - 1) % RR_FRAME_OVERLAP];
}

Rr_Frame *Rr_GetCurrentFrame(void)
{
    return &gRHI->Frames[gRHI->FrameIndex];
}

void Rr_BeginFrameSection(char const *Name)
{
    Rr_BeginSection(Rr_GetCurrentFrame()->Profiler, Name);
}

void Rr_EndFrameSection(char const *Name)
{
    Rr_EndSection(Rr_GetCurrentFrame()->Profiler, Name);
}

Rr_Profiler *Rr_GetFrameProfiler(void)
{
    return Rr_GetPreviousFrame()->Profiler;
}

bool Rr_IsUsingTransferQueue(void)
{
    return gRHI->DedicatedTransferQueue.Handle != VK_NULL_HANDLE;
}

bool Rr_IsIntegratedGPU(void)
{
    return gRHI->PhysicalDevice.Properties.deviceType ==
           VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}

size_t Rr_GetMaxUniformRange(void)
{
    return gRHI->PhysicalDevice.Properties.limits.maxUniformBufferRange;
}

size_t Rr_GetUniformAlignment(void)
{
    return gRHI->PhysicalDevice.Properties.limits
        .minUniformBufferOffsetAlignment;
}

size_t Rr_GetStorageAlignment(void)
{
    return gRHI->PhysicalDevice.Properties.limits
        .minStorageBufferOffsetAlignment;
}

size_t Rr_GetMaxComputeSharedMemorySize(void)
{
    return gRHI->PhysicalDevice.Properties.limits.maxComputeSharedMemorySize;
}

size_t Rr_GetMaxComputeWorkgroupInvocations(void)
{
    return gRHI->PhysicalDevice.Properties.limits
        .maxComputeWorkGroupInvocations;
}

Rr_ImageFormat Rr_GetSwapchainFormat(void)
{
    return Rr_ToImageFormat(gRHI->Swapchain.Format);
}

Rr_IntVec2 Rr_GetSwapchainSize(void)
{
    return (Rr_IntVec2){
        (int32_t)gRHI->Swapchain.Extent.width,
        (int32_t)gRHI->Swapchain.Extent.height,
    };
}

Rr_Image2D *Rr_GetSwapchainImage(void)
{
    return &Rr_GetCurrentFrame()->SwapchainImage->Container;
}

Rr_PresentMode *Rr_GetAvailablePresentModes(uint32_t *Count)
{
    if (Count)
    {
        *Count = gRHI->Swapchain.PresentModeCount;
    }
    return gRHI->Swapchain.PresentModes;
}

Rr_PresentMode Rr_GetPresentMode(void)
{
    return gRHI->Swapchain.PresentMode;
}

char const *Rr_GetPresentModeString(Rr_PresentMode PresentMode)
{
    return Rr_GetPresentModeStrings()[(size_t)PresentMode];
}

char const *const *RR_CC Rr_GetPresentModeStrings(void)
{
    static char const *PRESENT_MODES[] = {
        "FIFO",
        "FIFO_RELAXED",
        "IMMEDIATE",
        "MAILBOX",
    };

    return PRESENT_MODES;
}

bool Rr_SetPresentMode(Rr_PresentMode PresentMode)
{
    gRHI->Swapchain.PresentMode = PresentMode;
    Rr_SetSwapchainDirty(true);

    return true;
}

VkRenderPass Rr_GetRenderPass(Rr_RenderPassKey const *Key)
{
    Rr_LockSpinlock(&gRHI->RenderPassMapLock);

    Rr_RenderPassMapIterator It =
        Rr_FindInRenderPassMap(&gRHI->RenderPassMap, Key);
    if (!Rr_IsRenderPassMapEnd(&It))
    {
        Rr_UnlockSpinlock(&gRHI->RenderPassMapLock);

        return It.Data->Value;
    }

    Rr_UnlockSpinlock(&gRHI->RenderPassMapLock);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkAttachmentReference *ColorReferences = NULL;
    VkAttachmentReference *ResolveReferences = NULL;
    VkAttachmentReference *DepthReference = NULL;

    uint32_t AttachmentCount =
        (uint32_t)(Key->ColorAttachmentCount + Key->ResolveAttachmentCount +
                   Key->DepthStencil);

    VkAttachmentDescription *Descriptions = Rr_Alloc(
        sizeof(VkAttachmentDescription) * AttachmentCount,
        Scratch.Arena);

    uint32_t ResolveDescriptionIndex = Key->ColorAttachmentCount;

    if (Key->ColorAttachmentCount > 0)
    {
        ColorReferences = Rr_Alloc(
            sizeof(VkAttachmentReference) * Key->ColorAttachmentCount,
            Scratch.Arena);

        ResolveReferences = Rr_Alloc(
            sizeof(VkAttachmentReference) * Key->ColorAttachmentCount,
            Scratch.Arena);

        for (uint32_t Index = 0; Index < Key->ColorAttachmentCount; ++Index)
        {
            Descriptions[Index] = (VkAttachmentDescription){
                .samples = Key->Attachments[Index].Samples,
                .format = Key->Attachments[Index].Format,
                .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = Key->Attachments[Index].LoadOp,
                .storeOp = Key->Attachments[Index].StoreOp,
            };

            ColorReferences[Index] = (VkAttachmentReference){
                .attachment = Index,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };

            if (Key->ResolveMask & (1 << Index))
            {
                Descriptions[ResolveDescriptionIndex] =
                    (VkAttachmentDescription){
                        .samples =
                            Key->Attachments[ResolveDescriptionIndex].Samples,
                        .format =
                            Key->Attachments[ResolveDescriptionIndex].Format,
                        .initialLayout =
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .loadOp =
                            Key->Attachments[ResolveDescriptionIndex].LoadOp,
                        .storeOp =
                            Key->Attachments[ResolveDescriptionIndex].StoreOp,
                    };

                ResolveReferences[Index] = (VkAttachmentReference){
                    .attachment = ResolveDescriptionIndex,
                    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                };

                ResolveDescriptionIndex++;
            }
            else
            {
                ResolveReferences[Index].attachment = VK_ATTACHMENT_UNUSED;
            }
        }
    }

    if (Key->DepthStencil)
    {
        Descriptions[ResolveDescriptionIndex] = (VkAttachmentDescription){
            .samples = Key->Attachments[ResolveDescriptionIndex].Samples,
            .format = Key->Attachments[ResolveDescriptionIndex].Format,
            .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .loadOp = Key->Attachments[ResolveDescriptionIndex].LoadOp,
            .storeOp = Key->Attachments[ResolveDescriptionIndex].StoreOp,
        };
        DepthReference =
            Rr_AllocNoZero(sizeof(VkAttachmentReference), Scratch.Arena);
        *DepthReference = (VkAttachmentReference){
            .attachment = ResolveDescriptionIndex,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
    }

    VkSubpassDescription SubpassDescription = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = Key->ColorAttachmentCount,
        .pColorAttachments = ColorReferences,
        .pResolveAttachments = ResolveReferences,
        .pDepthStencilAttachment = DepthReference,
    };

    VkRenderPassCreateInfo RenderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = AttachmentCount,
        .pAttachments = Descriptions,
        .subpassCount = 1,
        .pSubpasses = &SubpassDescription,
    };

    Rr_Device *Device = Rr_GetDevice();

    VkRenderPass Handle = VK_NULL_HANDLE;
    Device->CreateRenderPass(
        Device->Handle,
        &RenderPassCreateInfo,
        NULL,
        &Handle);

    Rr_LockSpinlock(&gRHI->RenderPassMapLock);

    Rr_InsertIntoRenderPassMap(
        &gRHI->RenderPassMap,
        Key,
        &Handle,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->RenderPassMapLock);

    Rr_DestroyScratch(Scratch);

    return Handle;
}

VkFramebuffer Rr_GetFramebuffer(Rr_FramebufferKey *Key)
{
    Rr_LockSpinlock(&gRHI->FramebufferMapLock);

    Rr_FramebufferMapIterator It =
        Rr_FindInFramebufferMap(&gRHI->FramebufferMap, Key);
    if (!Rr_IsFramebufferMapEnd(&It))
    {
        Rr_UnlockSpinlock(&gRHI->FramebufferMapLock);

        return It.Data->Value;
    }

    Rr_UnlockSpinlock(&gRHI->FramebufferMapLock);

    uint32_t AttachmentCount =
        (uint32_t)(Key->ColorAttachmentCount + Key->ResolveAttachmentCount +
                   Key->DepthStencil);

    VkFramebufferCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = Key->RenderPass,
        .width = Key->Extent.width,
        .height = Key->Extent.height,
        .layers = Key->Extent.depth,
        .attachmentCount = AttachmentCount,
        .pAttachments = Key->ImageViews,
    };

    Rr_Device *Device = Rr_GetDevice();

    VkFramebuffer Handle = VK_NULL_HANDLE;
    Device->CreateFramebuffer(Device->Handle, &CreateInfo, NULL, &Handle);

    Rr_LockSpinlock(&gRHI->FramebufferMapLock);

    Rr_InsertIntoFramebufferMap(
        &gRHI->FramebufferMap,
        Key,
        &Handle,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->FramebufferMapLock);

    return Handle;
}

void Rr_DestroyFramebuffers(VkImageView ImageView)
{
    Rr_Device *Device = Rr_GetDevice();

    Rr_LockSpinlock(&gRHI->FramebufferMapLock);

    Rr_FramebufferMapIterator It =
        Rr_BeginInFramebufferMap(&gRHI->FramebufferMap);
    while (!Rr_IsFramebufferMapEnd(&It))
    {
        Rr_FramebufferKey *Key = &It.Data->Key;
        bool Destroy = false;
        size_t Boundary = Key->ColorAttachmentCount +
                          Key->ResolveAttachmentCount +
                          (size_t)Key->DepthStencil;
        for (size_t Index = 0; Index < Boundary; ++Index)
        {
            if (Key->ImageViews[Index] == ImageView)
            {
                Destroy = true;
                break;
            }
        }

        if (Destroy)
        {
            Device->DestroyFramebuffer(Device->Handle, It.Data->Value, NULL);
            Rr_EraseFromFramebufferMap(&It);
        }
        else
        {
            Rr_NextInFramebufferMap(&It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->FramebufferMapLock);
}

VkSemaphore Rr_AcquireVulkanSemaphore(void)
{
    VkSemaphore Semaphore;

    bool Locked = Rr_TryLockSpinlock(&gRHI->SemaphoresLock);

    if (Locked && gRHI->Semaphores.Count > 0)
    {
        Semaphore = RR_POP_FROM_ARRAY(&gRHI->Semaphores);
    }
    else
    {
        Rr_Device *Device = Rr_GetDevice();

        Device->CreateSemaphore(
            Device->Handle,
            &(VkSemaphoreCreateInfo){
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            },
            NULL,
            &Semaphore);
    }

    if (Locked)
    {
        Rr_UnlockSpinlock(&gRHI->SemaphoresLock);
    }

    return Semaphore;
}

void Rr_ReleaseVulkanSemaphore(VkSemaphore Semaphore)
{
    if (Semaphore == VK_NULL_HANDLE)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->SemaphoresLock);

    *RR_PUSH_INTO_ARRAY(&gRHI->Semaphores, Rr_GetPermanent()) = Semaphore;

    Rr_UnlockSpinlock(&gRHI->SemaphoresLock);
}

VkFence Rr_AcquireVulkanFence(void)
{
    VkFence Fence;

    bool Locked = Rr_TryLockSpinlock(&gRHI->FencesLock);

    if (Locked && gRHI->Fences.Count > 0)
    {
        Fence = RR_POP_FROM_ARRAY(&gRHI->Fences);
    }
    else
    {
        Rr_Device *Device = Rr_GetDevice();

        Device->CreateFence(
            Device->Handle,
            &(VkFenceCreateInfo){
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            },
            NULL,
            &Fence);
    }

    if (Locked)
    {
        Rr_UnlockSpinlock(&gRHI->FencesLock);
    }

    return Fence;
}

void Rr_ReleaseVulkanFence(VkFence Fence)
{
    if (Fence == VK_NULL_HANDLE)
    {
        return;
    }

    Rr_Device *Device = Rr_GetDevice();

    Rr_LockSpinlock(&gRHI->FencesLock);

    *RR_PUSH_INTO_ARRAY(&gRHI->Fences, Rr_GetPermanent()) = Fence;

    Rr_UnlockSpinlock(&gRHI->FencesLock);

    Device->ResetFences(Device->Handle, 1, &Fence);
}

Rr_CommandPools *Rr_AcquireCommandPools(void)
{
    Rr_ThreadContext *ThreadContext = Rr_GetThreadContext();

    if (ThreadContext->CommandPools)
    {
        return ThreadContext->CommandPools;
    }

    Rr_Device *Device = Rr_GetDevice();

    Rr_LockSpinlock(&gRHI->CommandPoolsLock);

    if (gRHI->FreeCommandPools)
    {
        ThreadContext->CommandPools = gRHI->FreeCommandPools;
        gRHI->FreeCommandPools = ThreadContext->CommandPools->Next;

        Rr_UnlockSpinlock(&gRHI->CommandPoolsLock);
    }
    else
    {
        Rr_UnlockSpinlock(&gRHI->CommandPoolsLock);

        ThreadContext->CommandPools =
            Rr_AllocNoZero(sizeof(Rr_CommandPools), Rr_GetPermanent());

        Device->CreateCommandPool(
            Device->Handle,
            &(VkCommandPoolCreateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = gRHI->MainQueue.FamilyIndex,
            },
            NULL,
            &ThreadContext->CommandPools->Graphics);

        Device->CreateCommandPool(
            Device->Handle,
            &(VkCommandPoolCreateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = gRHI->DedicatedTransferQueue.FamilyIndex,
            },
            NULL,
            &ThreadContext->CommandPools->Transfer);

        ThreadContext->CommandPools->Compute = NULL;

        // Device->CreateCommandPool(
        //     Device->Handle,
        //     &(VkCommandPoolCreateInfo){
        //         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        //         .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        //         .queueFamilyIndex = gRHI->ComputeQueue.FamilyIndex,
        //     },
        //     NULL,
        //     &CommandPools->Compute);
    }

    ThreadContext->CommandPools->Next = NULL;

    return ThreadContext->CommandPools;
}

void Rr_ReleaseCommandPools(void)
{
    Rr_ThreadContext *ThreadContext = Rr_GetThreadContext();

    if (!ThreadContext->CommandPools)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->CommandPoolsLock);

    ThreadContext->CommandPools->Next = gRHI->FreeCommandPools;
    gRHI->FreeCommandPools = ThreadContext->CommandPools;

    Rr_UnlockSpinlock(&gRHI->CommandPoolsLock);

    ThreadContext->CommandPools = NULL;
}

bool Rr_IsSRGBFormat(Rr_ImageFormat Format)
{
    return Format == RR_IMAGE_FORMAT_R8G8_SRGB ||
           Format == RR_IMAGE_FORMAT_R8G8B8_SRGB ||
           Format == RR_IMAGE_FORMAT_B8G8R8_SRGB ||
           Format == RR_IMAGE_FORMAT_R8G8B8A8_SRGB ||
           Format == RR_IMAGE_FORMAT_B8G8R8A8_SRGB ||
           Format == RR_IMAGE_FORMAT_A8B8G8R8_SRGB_PACK32;
}

void Rr_SetVulkanObjectName(
    VkObjectType ObjectType,
    uint64_t Handle,
    const char *Name)
{
#ifdef RR_USE_GPU_DEBUG_UTILS
    VkDebugUtilsObjectNameInfoEXT ObjectNameInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = ObjectType,
        .objectHandle = Handle,
        .pObjectName = Name,
    };
    gRHI->Instance.SetDebugUtilsObjectNameEXT(
        gRHI->Device.Handle,
        &ObjectNameInfo);
#endif
}

void Rr_BeginVulkanCommandBufferLabel(
    VkCommandBuffer CommandBuffer,
    const char *Name)
{
#ifdef RR_USE_GPU_DEBUG_UTILS
    Rr_Instance *Instance = &gRHI->Instance;
    VkDebugUtilsLabelEXT Label = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = Name,
    };
    Instance->CmdBeginDebugUtilsLabelEXT(CommandBuffer, &Label);
#endif
}

void Rr_EndVulkanCommandBufferLabel(VkCommandBuffer CommandBuffer)
{
#ifdef RR_USE_GPU_DEBUG_UTILS
    Rr_Instance *Instance = &gRHI->Instance;
    Instance->CmdEndDebugUtilsLabelEXT(CommandBuffer);
#endif
}
