/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Rr_Vulkan.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_VULKAN
#include "Rr_LogMacro.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    const char *ApplicationName,
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

    const char *InstanceExtensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    uint32_t InstanceExtensionCount = RR_ARRAY_COUNT(InstanceExtensions);

#ifndef RR_USE_GPU_DEBUG_UTILS
    InstanceExtensionCount = 0;
#endif

    uint32_t PlatformExtensionCount;
    const char *const *PlatformExtensions =
        Rr_GetVulkanExtensions(&PlatformExtensionCount);

    uint32_t ExtensionCount = PlatformExtensionCount + InstanceExtensionCount;

    const char **Extensions =
        RR_ALLOC_TYPE_COUNT(const char *, ExtensionCount + 1, Scratch.Arena);
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

    Loader->CreateInstance(&InstanceCreateInfo, NULL, &Instance->Handle);

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
    if (ExtensionCount == 0)
    {
        return false;
    }

    const char *TargetExtensions[] = {
        "VK_KHR_swapchain",
    };

    bool FoundExtensions[] = { 0 };

    VkExtensionProperties *Extensions =
        RR_ALLOC_TYPE_COUNT(VkExtensionProperties, ExtensionCount, Arena);
    Instance->EnumerateDeviceExtensionProperties(
        PhysicalDevice,
        NULL,
        &ExtensionCount,
        Extensions);

    for (uint32_t Index = 0; Index < ExtensionCount; Index++)
    {
        for (uint32_t TargetIndex = 0;
             TargetIndex < RR_ARRAY_COUNT(TargetExtensions);
             ++TargetIndex)
        {
            if (strcmp(
                    Extensions[Index].extensionName,
                    TargetExtensions[TargetIndex]) == 0)
            {
                FoundExtensions[TargetIndex] = true;
            }
        }
    }
    for (uint32_t TargetIndex = 0;
         TargetIndex < RR_ARRAY_COUNT(TargetExtensions);
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
        RR_ALLOC_TYPE_COUNT(VkQueueFamilyProperties, QueueFamilyCount, Arena);
    VkBool32 *QueuePresentSupport =
        RR_ALLOC_TYPE_COUNT(VkBool32, QueueFamilyCount, Arena);

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
        RR_ALLOC_TYPE_COUNT(VkPhysicalDevice, PhysicalDeviceCount, Arena);
    Instance->EnumeratePhysicalDevices(
        Instance->Handle,
        &PhysicalDeviceCount,
        &PhysicalDevices[0]);

    RR_LOG_TRACE("Selecting Vulkan device:");

    typedef char Rr_DeviceString[1024];
    RR_ARRAY(Rr_DeviceString) DeviceStrings = { 0 };
    uint32_t BestDeviceIndex = UINT32_MAX;
    static const uint32_t PreferredDeviceType =
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    /* static const uint32_t PreferredDeviceType = */
    /* VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU; */
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

            const char *TypeString = NULL;
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
                if (Properties.deviceType == PreferredDeviceType)
                {
                    if (BestDeviceType == PreferredDeviceType)
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

    RR_LOG_INFO(
        "Using %s transfer queue.",
        UseTransferQueue ? "dedicated" : "unified");
}

void Rr_InitSurface(Rr_Instance *Instance, VkSurfaceKHR *Surface)
{
    if (Rr_CreateVulkanSurface(Instance->Handle, (void *)Surface) != true)
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

#ifdef __APPLE__
    const char *DeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                       "VK_KHR_portability_subset" };
#else
    const char *DeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#endif
    uint32_t DeviceExtensionCount = RR_ARRAY_COUNT(DeviceExtensions);

    VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .queueCreateInfoCount = UseTransferQueue ? 2 : 1,
        .pQueueCreateInfos = QueueInfos,
        .enabledExtensionCount = DeviceExtensionCount,
        .ppEnabledExtensionNames = DeviceExtensions,
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
