#include "Rr_Vulkan.h"

#include "Rr_Log.h"
#include "Rr_Memory.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <stdio.h>
#include <stdlib.h>

void Rr_InitLoader(Rr_VulkanLoader *Loader)
{
    Loader->GetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();

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

    const char *AppExtensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    uint32_t AppExtensionCount = RR_ARRAY_COUNT(AppExtensions);
    AppExtensionCount = 0; /* Use Vulkan Configurator! */

    uint32_t SDLExtensionCount;
    const char *const *SDLExtensions =
        SDL_Vulkan_GetInstanceExtensions(&SDLExtensionCount);

    uint32_t ExtensionCount = SDLExtensionCount + AppExtensionCount;
    const char **Extensions =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, const char *, ExtensionCount);
    for(uint32_t Index = 0; Index < ExtensionCount; Index++)
    {
        Extensions[Index] = SDLExtensions[Index];
    }
    for(uint32_t Index = 0; Index < AppExtensionCount; Index++)
    {
        Extensions[Index + SDLExtensionCount] = AppExtensions[Index];
    }

    /* Create Vulkan instance. */

    VkInstanceCreateInfo InstanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pApplicationInfo = &ApplicationInfo,
        .enabledExtensionCount = ExtensionCount,
        .ppEnabledExtensionNames = Extensions,
    };

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

    /* Vulkan 1.1 */

    Instance->EnumeratePhysicalDeviceGroups =
        (PFN_vkEnumeratePhysicalDeviceGroups)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkEnumeratePhysicalDeviceGroups");
    Instance->GetPhysicalDeviceExternalBufferProperties =
        (PFN_vkGetPhysicalDeviceExternalBufferProperties)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceExternalBufferProperties");
    Instance->GetPhysicalDeviceExternalFenceProperties =
        (PFN_vkGetPhysicalDeviceExternalFenceProperties)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceExternalFenceProperties");
    Instance->GetPhysicalDeviceExternalSemaphoreProperties =
        (PFN_vkGetPhysicalDeviceExternalSemaphoreProperties)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceExternalSemaphoreProperties");
    Instance->GetPhysicalDeviceFeatures2 =
        (PFN_vkGetPhysicalDeviceFeatures2)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceFeatures2");
    Instance->GetPhysicalDeviceFormatProperties2 =
        (PFN_vkGetPhysicalDeviceFormatProperties2)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceFormatProperties2");
    Instance->GetPhysicalDeviceImageFormatProperties2 =
        (PFN_vkGetPhysicalDeviceImageFormatProperties2)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceImageFormatProperties2");
    Instance->GetPhysicalDeviceMemoryProperties2 =
        (PFN_vkGetPhysicalDeviceMemoryProperties2)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceMemoryProperties2");
    Instance->GetPhysicalDeviceProperties2 =
        (PFN_vkGetPhysicalDeviceProperties2)Loader->GetInstanceProcAddr(
            Instance->Handle,
            "vkGetPhysicalDeviceProperties2");
    Instance->GetPhysicalDeviceQueueFamilyProperties2 =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties2)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceQueueFamilyProperties2");
    Instance->GetPhysicalDeviceSparseImageFormatProperties2 =
        (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2)
            Loader->GetInstanceProcAddr(
                Instance->Handle,
                "vkGetPhysicalDeviceSparseImageFormatProperties2");

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

    Rr_DestroyScratch(Scratch);
}

static bool Rr_CheckPhysicalDevice(
    Rr_Instance *Instance,
    VkPhysicalDevice PhysicalDevice,
    VkSurfaceKHR Surface,
    uint32_t *OutGraphicsQueueFamilyIndex,
    uint32_t *OutTransferQueueFamilyIndex,
    Rr_Arena *Arena)
{
    uint32_t ExtensionCount;
    Instance->EnumerateDeviceExtensionProperties(
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
    Instance->EnumerateDeviceExtensionProperties(
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
    Instance->GetPhysicalDeviceQueueFamilyProperties(
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

    Instance->GetPhysicalDeviceQueueFamilyProperties(
        PhysicalDevice,
        &QueueFamilyCount,
        QueueFamilyProperties);

    uint32_t GraphicsQueueFamilyIndex = ~0U;
    uint32_t TransferQueueFamilyIndex = ~0U;

    for(uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
    {
        Instance->GetPhysicalDeviceSurfaceSupportKHR(
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

void Rr_SelectPhysicalDevice(
    Rr_Instance *Instance,
    VkSurfaceKHR Surface,
    Rr_PhysicalDevice *PhysicalDevice,
    uint32_t *OutGraphicsQueueFamilyIndex,
    uint32_t *OutTransferQueueFamilyIndex,
    Rr_Arena *Arena)
{
    uint32_t PhysicalDeviceCount = 0;
    Instance->EnumeratePhysicalDevices(
        Instance->Handle,
        &PhysicalDeviceCount,
        NULL);
    if(PhysicalDeviceCount == 0)
    {
        RR_LOG("No device with Vulkan support found");
    }

    VkPhysicalDevice *PhysicalDevices =
        RR_ALLOC_TYPE_COUNT(Arena, VkPhysicalDevice, PhysicalDeviceCount);
    Instance->EnumeratePhysicalDevices(
        Instance->Handle,
        &PhysicalDeviceCount,
        &PhysicalDevices[0]);

    RR_LOG("Selecting Vulkan device:");

    typedef char Rr_DeviceString[1024];
    RR_SLICE(Rr_DeviceString) DeviceStrings = { 0 };
    uint32_t BestDeviceIndex = UINT32_MAX;
    static const uint32_t PreferredDeviceType =
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    // static const uint32_t PreferredDeviceType =
    // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    uint32_t BestDeviceType = 0;
    VkDeviceSize BestDeviceMemory = 0;
    for(uint32_t Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        VkPhysicalDevice PhysicalDeviceHandle = PhysicalDevices[Index];
        uint32_t GraphicsQueueFamilyIndex;
        uint32_t TransferQueueFamilyIndex;
        if(Rr_CheckPhysicalDevice(
               Instance,
               PhysicalDeviceHandle,
               Surface,
               &GraphicsQueueFamilyIndex,
               &TransferQueueFamilyIndex,
               Arena))
        {
            VkPhysicalDeviceProperties2 Properties = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            };
            Instance->GetPhysicalDeviceProperties2(
                PhysicalDeviceHandle,
                &Properties);

            VkPhysicalDeviceMemoryProperties MemoryProperties = { 0 };
            Instance->GetPhysicalDeviceMemoryProperties(
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

    *PhysicalDevice = (Rr_PhysicalDevice){
        .SubgroupProperties =
            (VkPhysicalDeviceSubgroupProperties){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,
            },
        .Properties =
            (VkPhysicalDeviceProperties2){
                .pNext = &PhysicalDevice->SubgroupProperties,
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            },
        .Handle = PhysicalDevices[BestDeviceIndex],
    };

    Instance->GetPhysicalDeviceFeatures(
        PhysicalDevices[BestDeviceIndex],
        &PhysicalDevice->Features);
    Instance->GetPhysicalDeviceMemoryProperties(
        PhysicalDevices[BestDeviceIndex],
        &PhysicalDevice->MemoryProperties);
    Instance->GetPhysicalDeviceProperties2(
        PhysicalDevices[BestDeviceIndex],
        &PhysicalDevice->Properties);

    RR_LOG(
        "Using %s transfer queue.",
        UseTransferQueue ? "dedicated" : "unified");
}

void Rr_InitSurface(void *Window, Rr_Instance *Instance, VkSurfaceKHR *Surface)
{
    if(SDL_Vulkan_CreateSurface(Window, Instance->Handle, NULL, Surface) !=
       true)
    {
        RR_ABORT("Failed to create Vulkan surface: %s", SDL_GetError());
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
        &GraphicsQueue->FamilyIndex,
        &TransferQueue->FamilyIndex,
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

    const char *DeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .queueCreateInfoCount = UseTransferQueue ? 2 : 1,
        .pQueueCreateInfos = QueueInfos,
        .enabledExtensionCount = SDL_arraysize(DeviceExtensions),
        .ppEnabledExtensionNames = DeviceExtensions,
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
    if(UseTransferQueue)
    {
        Device->GetDeviceQueue(
            Device->Handle,
            TransferQueue->FamilyIndex,
            0,
            &TransferQueue->Handle);
    }

    Rr_DestroyScratch(Scratch);
}

void Rr_BlitColorImage(
    Rr_Device *Device,
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

    Device->CmdBlitImage(
        CommandBuffer,
        Source,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        Destination,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &ImageBlit,
        VK_FILTER_NEAREST);
}
