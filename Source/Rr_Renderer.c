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

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Rr_Renderer.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_RENDERER
#include "Rr_App.h"
#include "Rr_LogMacro.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>

#if defined(__x86_64__) && !defined(__APPLE__)
#include <xxHash/xxh_x86dispatch.h>
#else
#include <xxHash/xxhash.h>
#endif

#include <assert.h>
#include <stdio.h>

Rr_Renderer *gRenderer;

static inline void Rr_DestroySwapchainImage(Rr_SwapchainImage *SwapchainImage)
{
    Rr_AllocatedImage *AllocatedImage =
        SwapchainImage->Container.AllocatedImages;

    if (AllocatedImage->ViewStorage)
    {
        Rr_DestroyImageViewStorage(AllocatedImage->ViewStorage, true);
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

void Rr_SetSwapchainDirty(bool Dirty)
{
    gRenderer->Swapchain.RecreatePending = Dirty;
}

static bool Rr_InitSwapchain(void)
{
    Rr_WaitIdle();

    Rr_Instance *Instance = &gRenderer->Instance;
    Rr_Device *Device = &gRenderer->Device;

    Rr_IntVec2 WindowSize = Rr_GetWindowSize();

    for (size_t Index = 0; Index < gRenderer->SwapchainImages.Count; ++Index)
    {
        Rr_DestroySwapchainImage(gRenderer->SwapchainImages.Data + Index);
    }
    RR_CLEAR_ARRAY(&gRenderer->SwapchainImages);

    VkSwapchainKHR OldSwapchain = gRenderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfaceCapabilities;
    Instance->GetPhysicalDeviceSurfaceCapabilitiesKHR(
        gRenderer->PhysicalDevice.Handle,
        gRenderer->Surface,
        &SurfaceCapabilities);

    if (SurfaceCapabilities.currentExtent.width == 0 ||
        SurfaceCapabilities.currentExtent.height == 0)
    {
        return false;
    }
    if (SurfaceCapabilities.currentExtent.width == UINT32_MAX)
    {
        gRenderer->Swapchain.Extent.width = (uint32_t)WindowSize.Width;
        gRenderer->Swapchain.Extent.height = (uint32_t)WindowSize.Height;
    }
    else
    {
        gRenderer->Swapchain.Extent.width =
            SurfaceCapabilities.currentExtent.width;
        gRenderer->Swapchain.Extent.height =
            SurfaceCapabilities.currentExtent.height;
    }
    gRenderer->Swapchain.Extent.depth = 1;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    uint32_t VulkanPresentModeCount = 0;
    Instance->GetPhysicalDeviceSurfacePresentModesKHR(
        gRenderer->PhysicalDevice.Handle,
        gRenderer->Surface,
        &VulkanPresentModeCount,
        NULL);
    assert(VulkanPresentModeCount > 0);

    VkPresentModeKHR *VulkanPresentModes = RR_ALLOC_TYPE_COUNT(
        VkPresentModeKHR,
        VulkanPresentModeCount,
        Scratch.Arena);
    Instance->GetPhysicalDeviceSurfacePresentModesKHR(
        gRenderer->PhysicalDevice.Handle,
        gRenderer->Surface,
        &VulkanPresentModeCount,
        VulkanPresentModes);

    VkPresentModeKHR DesiredVulkanPresentMode =
        Rr_ToVulkanPresentMode(gRenderer->Swapchain.PresentMode);
    bool VulkanPresentModeAvailable = false;
    gRenderer->Swapchain.PresentModeCount = 0;
    for (uint32_t Index = 0;
         Index < VulkanPresentModeCount &&
         gRenderer->Swapchain.PresentModeCount <
             RR_ARRAY_COUNT(gRenderer->Swapchain.PresentModes);
         Index++)
    {
        if (VulkanPresentModes[Index] <= VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        {
            gRenderer->Swapchain
                .PresentModes[gRenderer->Swapchain.PresentModeCount++] =
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
        gRenderer->Swapchain.PresentMode =
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
        gRenderer->PhysicalDevice.Handle,
        gRenderer->Surface,
        &FormatCount,
        NULL);
    assert(FormatCount > 0);

    VkSurfaceFormatKHR *SurfaceFormats =
        RR_ALLOC_TYPE_COUNT(VkSurfaceFormatKHR, FormatCount, Scratch.Arena);
    Instance->GetPhysicalDeviceSurfaceFormatsKHR(
        gRenderer->PhysicalDevice.Handle,
        gRenderer->Surface,
        &FormatCount,
        SurfaceFormats);

    VkSurfaceFormatKHR *PrefferedFormat = NULL;
    VkSurfaceFormatKHR *FallbackFormat = SurfaceFormats;
    for (uint32_t Index = 0; Index < FormatCount; Index++)
    {
        VkSurfaceFormatKHR *SurfaceFormat = &SurfaceFormats[Index];

        if (SurfaceFormat->format == VK_FORMAT_B8G8R8A8_SRGB ||
            SurfaceFormat->format == VK_FORMAT_R8G8B8A8_SRGB ||
            SurfaceFormat->format == VK_FORMAT_A8B8G8R8_SRGB_PACK32)
        {
            PrefferedFormat = SurfaceFormat;
            break;
        }
    }
    VkSurfaceFormatKHR *SelectedFormat =
        PrefferedFormat ? PrefferedFormat : FallbackFormat;
    if (!SelectedFormat)
    {
        RR_LOG_ABORT("No suitable surface format found!");
    }
    gRenderer->Swapchain.Format = SelectedFormat->format;
    gRenderer->Swapchain.ColorSpace = SelectedFormat->colorSpace;

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
        .surface = gRenderer->Surface,
        .minImageCount = DesiredNumberOfSwapchainImages,
        .imageFormat = gRenderer->Swapchain.Format,
        .imageColorSpace = gRenderer->Swapchain.ColorSpace,
        .imageExtent = { gRenderer->Swapchain.Extent.width,
                         gRenderer->Swapchain.Extent.height },
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
        gRenderer->Device.Handle,
        &SwapchainCreateInfo,
        NULL,
        &gRenderer->Swapchain.Handle);

    if (OldSwapchain != VK_NULL_HANDLE)
    {
        Device->DestroySwapchainKHR(Device->Handle, OldSwapchain, NULL);
    }

    /* Acquire swapchain images. */

    uint32_t ImageCount = 0;
    Device->GetSwapchainImagesKHR(
        gRenderer->Device.Handle,
        gRenderer->Swapchain.Handle,
        &ImageCount,
        NULL);

    VkImage *ImageHandles =
        RR_ALLOC_TYPE_COUNT(VkImage, ImageCount, Scratch.Arena);

    Device->GetSwapchainImagesKHR(
        gRenderer->Device.Handle,
        gRenderer->Swapchain.Handle,
        &ImageCount,
        ImageHandles);

    /* Create image views. */

    if (gRenderer->SwapchainImages.Capacity < ImageCount)
    {
        Rr_LockSpinlock(&gRenderer->Lock);

        RR_RESERVE_ARRAY(
            &gRenderer->SwapchainImages,
            ImageCount,
            gRenderer->Arena);

        Rr_UnlockSpinlock(&gRenderer->Lock);
    }

    gRenderer->SwapchainImages.Count = ImageCount;

    for (uint32_t Index = 0; Index < ImageCount; Index++)
    {
        Rr_SwapchainImage *Image = gRenderer->SwapchainImages.Data + Index;

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
                    .ViewStorage = Rr_CreateImageViewStorage(),
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
        gRenderer->Swapchain.Extent.width != (uint32_t)WindowSize.Width ||
        gRenderer->Swapchain.Extent.height != (uint32_t)WindowSize.Height ||
        gRenderer->Swapchain.RecreatePending;

    if (!Recreate)
    {
        return true;
    }

    bool Recreated = Rr_InitSwapchain();

    if (Recreated)
    {
        gRenderer->Swapchain.RecreateEventPending = true;
    }

    return Recreate;
}

static void Rr_InitFrames(void)
{
    Rr_Device *Device = &gRenderer->Device;
    Rr_Frame *Frames = gRenderer->Frames;
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

        if (gRenderer->MainQueue.TimestampsEnabled)
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
    Rr_Device *Device = &gRenderer->Device;

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame *Frame = &gRenderer->Frames[Index];
        Rr_ReleaseVulkanFence(Frame->SubmitFence);
        Rr_ReleaseVulkanSemaphore(Frame->AcquireSemaphore);
        if (Frame->QueryPool)
        {
            Device->DestroyQueryPool(Device->Handle, Frame->QueryPool, NULL);
        }
        Rr_DestroyArena(Frame->Arena);
    }
}

static void Rr_InitVMA(void)
{
    Rr_Instance *Instance = &gRenderer->Instance;
    Rr_Device *Device = &gRenderer->Device;

    VmaVulkanFunctions VulkanFunctions = {
        .vkGetPhysicalDeviceProperties = Instance->GetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties =
            Instance->GetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = Device->AllocateMemory,
        .vkFreeMemory = Device->FreeMemory,
        .vkMapMemory = Device->MapMemory,
        .vkUnmapMemory = Device->UnmapMemory,
        .vkFlushMappedMemoryRanges = Device->FlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = Device->InvalidateMappedMemoryRanges,
        .vkBindBufferMemory = Device->BindBufferMemory,
        .vkBindImageMemory = Device->BindImageMemory,
        .vkGetBufferMemoryRequirements = Device->GetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = Device->GetImageMemoryRequirements,
        .vkCreateBuffer = Device->CreateBuffer,
        .vkDestroyBuffer = Device->DestroyBuffer,
        .vkCreateImage = Device->CreateImage,
        .vkDestroyImage = Device->DestroyImage,
        .vkCmdCopyBuffer = Device->CmdCopyBuffer,
    };
    VmaAllocatorCreateInfo AllocatorInfo = {
        .flags = 0,
        .physicalDevice = gRenderer->PhysicalDevice.Handle,
        .device = gRenderer->Device.Handle,
        .pVulkanFunctions = &VulkanFunctions,
        .instance = gRenderer->Instance.Handle,
    };
    vmaCreateAllocator(&AllocatorInfo, &gRenderer->Allocator);
}

void Rr_InitRenderer(const char *Title)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gRenderer = RR_ALLOC_TYPE(Rr_Renderer, Arena);
    gRenderer->Arena = Arena;

    Rr_InitLoader(&gRenderer->Loader);
    Rr_InitInstance(&gRenderer->Loader, Title, &gRenderer->Instance);
    Rr_InitSurface(&gRenderer->Instance, &gRenderer->Surface);
    Rr_InitDeviceAndQueues(
        &gRenderer->Instance,
        gRenderer->Surface,
        &gRenderer->PhysicalDevice,
        &gRenderer->Device,
        &gRenderer->MainQueue,
        &gRenderer->DedicatedTransferQueue);

    Rr_InitVMA();
    Rr_InitFrames();
    Rr_InitSwapchain();

    Rr_DestroyScratch(Scratch);
}

void Rr_WaitIdle(void)
{
    Rr_Device *Device = &gRenderer->Device;
    Device->DeviceWaitIdle(Device->Handle);
}

static inline void Rr_DestroyReleasedObjects(void)
{
    Rr_LockSpinlock(&gRenderer->ReleasedBuffersLock);
    for (Rr_HandleHiveIterator It = gRenderer->ReleasedBuffers.Begin;
         It.Element != gRenderer->ReleasedBuffers.End.Element;)
    {
        Rr_Buffer *Buffer = *(Rr_Buffer **)It.Element;
        if (!Rr_LoadAtomicRelaxed(&Buffer->RefCount))
        {
            Rr_DestroyBuffer(Buffer);
            Rr_RemoveFromHandleHive(&gRenderer->ReleasedBuffers, &It);
        }
        else
        {
            Rr_AdvanceHandleHiveIterator(&It);
        }
    }
    Rr_UnlockSpinlock(&gRenderer->ReleasedBuffersLock);

    Rr_LockSpinlock(&gRenderer->ReleasedImagesLock);
    for (Rr_HandleHiveIterator It = gRenderer->ReleasedImages.Begin;
         It.Element != gRenderer->ReleasedImages.End.Element;)
    {
        Rr_Image *Image = *(Rr_Image **)It.Element;
        if (!Rr_LoadAtomicRelaxed(&Image->RefCount))
        {
            Rr_DestroyImage(Image);
            Rr_RemoveFromHandleHive(&gRenderer->ReleasedImages, &It);
        }
        else
        {
            Rr_AdvanceHandleHiveIterator(&It);
        }
    }
    Rr_UnlockSpinlock(&gRenderer->ReleasedImagesLock);

    Rr_LockSpinlock(&gRenderer->ReleasedSamplersLock);
    for (Rr_HandleHiveIterator It = gRenderer->ReleasedSamplers.Begin;
         It.Element != gRenderer->ReleasedSamplers.End.Element;)
    {
        Rr_Sampler *Sampler = *(Rr_Sampler **)It.Element;
        if (!Rr_LoadAtomicRelaxed(&Sampler->RefCount))
        {
            Rr_DestroySampler(Sampler);
            Rr_RemoveFromHandleHive(&gRenderer->ReleasedSamplers, &It);
        }
        else
        {
            Rr_AdvanceHandleHiveIterator(&It);
        }
    }
    Rr_UnlockSpinlock(&gRenderer->ReleasedSamplersLock);

    Rr_LockSpinlock(&gRenderer->ReleasedComputePipelinesLock);
    for (Rr_HandleHiveIterator It = gRenderer->ReleasedComputePipelines.Begin;
         It.Element != gRenderer->ReleasedComputePipelines.End.Element;)
    {
        Rr_ComputePipeline *ComputePipeline =
            *(Rr_ComputePipeline **)It.Element;
        if (!Rr_LoadAtomicRelaxed(&ComputePipeline->RefCount))
        {
            Rr_DestroyComputePipeline(ComputePipeline);
            Rr_RemoveFromHandleHive(&gRenderer->ReleasedComputePipelines, &It);
        }
        else
        {
            Rr_AdvanceHandleHiveIterator(&It);
        }
    }
    Rr_UnlockSpinlock(&gRenderer->ReleasedComputePipelinesLock);

    Rr_LockSpinlock(&gRenderer->ReleasedGraphicsPipelinesLock);
    for (Rr_HandleHiveIterator It = gRenderer->ReleasedGraphicsPipelines.Begin;
         It.Element != gRenderer->ReleasedGraphicsPipelines.End.Element;)
    {
        Rr_GraphicsPipeline *GraphicsPipeline =
            *(Rr_GraphicsPipeline **)It.Element;
        if (!Rr_LoadAtomicRelaxed(&GraphicsPipeline->RefCount))
        {
            Rr_DestroyGraphicsPipeline(GraphicsPipeline);
            Rr_RemoveFromHandleHive(&gRenderer->ReleasedGraphicsPipelines, &It);
        }
        else
        {
            Rr_AdvanceHandleHiveIterator(&It);
        }
    }
    Rr_UnlockSpinlock(&gRenderer->ReleasedGraphicsPipelinesLock);

    Rr_LockSpinlock(&gRenderer->ReleasedPipelineLayoutsLock);
    for (Rr_HandleHiveIterator It = gRenderer->ReleasedPipelineLayouts.Begin;
         It.Element != gRenderer->ReleasedPipelineLayouts.End.Element;)
    {
        Rr_PipelineLayout *PipelineLayout = *(Rr_PipelineLayout **)It.Element;
        if (!Rr_LoadAtomicRelaxed(&PipelineLayout->RefCount))
        {
            Rr_DestroyPipelineLayout(PipelineLayout);
            Rr_RemoveFromHandleHive(&gRenderer->ReleasedPipelineLayouts, &It);
        }
        else
        {
            Rr_AdvanceHandleHiveIterator(&It);
        }
    }

    Rr_UnlockSpinlock(&gRenderer->ReleasedPipelineLayoutsLock);
}

void Rr_CleanupRenderer(void)
{
    Rr_Instance *Instance = &gRenderer->Instance;
    Rr_Device *Device = &gRenderer->Device;

    Rr_WaitIdle();

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_FinalizeGraph(gRenderer->Frames[Index].Graph);
    }

    Rr_DestroyReleasedObjects();

    /* NOTE: VkFramebuffers are destroyed along with VkImageViews.
     * For now, we don't care for destroying render passes unless it's
     * application shutdown. */

    for (Rr_DescriptorSetLayoutHiveIterator It =
             gRenderer->DescriptorSetLayoutStorage.Hive.Begin;
         It.Element != gRenderer->DescriptorSetLayoutStorage.Hive.End.Element;)
    {
        Device->DestroyDescriptorSetLayout(
            Device->Handle,
            It.Element->Handle,
            NULL);
        Rr_AdvanceDescriptorSetLayoutHiveIterator(&It);
    }

    for (Rr_RenderPassMapHiveIterator It =
             gRenderer->RenderPassStorage.Hive.Begin;
         It.Element != gRenderer->RenderPassStorage.Hive.End.Element;)
    {
        Device->DestroyRenderPass(Device->Handle, It.Element->Value, NULL);
        Rr_AdvanceRenderPassMapHiveIterator(&It);
    }

    for (Rr_DescriptorPoolList *List = gRenderer->DescriptorPoolList; List;
         List = List->Next)
    {
        Device->DestroyDescriptorPool(Device->Handle, List->Handle, NULL);
    }

    Rr_CleanupFrames();

    for (size_t Index = 0; Index < gRenderer->SwapchainImages.Count; ++Index)
    {
        Rr_DestroySwapchainImage(&gRenderer->SwapchainImages.Data[Index]);
    }

    if (gRenderer->Swapchain.Handle != VK_NULL_HANDLE)
    {
        Device->DestroySwapchainKHR(
            gRenderer->Device.Handle,
            gRenderer->Swapchain.Handle,
            NULL);
    }

    Rr_ReleaseCommandPools();

    for (Rr_CommandPools *CommandPools = gRenderer->FreeCommandPools;
         CommandPools;
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

    for (size_t Index = 0; Index < gRenderer->Semaphores.Count; ++Index)
    {
        Device->DestroySemaphore(
            Device->Handle,
            gRenderer->Semaphores.Data[Index],
            NULL);
    }

    for (size_t Index = 0; Index < gRenderer->Fences.Count; ++Index)
    {
        Device->DestroyFence(
            Device->Handle,
            gRenderer->Fences.Data[Index],
            NULL);
    }

    vmaDestroyAllocator(gRenderer->Allocator);

    Instance->DestroySurfaceKHR(Instance->Handle, gRenderer->Surface, NULL);
    Device->DestroyDevice(Device->Handle, NULL);
    Instance->DestroyInstance(Instance->Handle, NULL);

    Rr_DestroyArena(gRenderer->Arena);

    gRenderer = NULL;
}

void Rr_NewFrame(void)
{
    Rr_Device *Device = &gRenderer->Device;

    gRenderer->FrameNumber++;
    gRenderer->FrameIndex = gRenderer->FrameNumber % RR_FRAME_OVERLAP;

    Rr_Frame *Frame = Rr_GetCurrentFrame();

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

        if (gRenderer->MainQueue.TimestampsEnabled)
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
                (double)
                    gRenderer->PhysicalDevice.Properties.limits.timestampPeriod;
            double DeltaNS = (double)(Timestamps[1] - Timestamps[0]);
            gRenderer->LastFrameMS = Period * DeltaNS / 1000000.0;
        }

        Rr_ReleaseVulkanFence(Frame->SubmitFence);
        Frame->SubmitFence = VK_NULL_HANDLE;

        Rr_FinalizeGraph(Frame->Graph);
    }

    Rr_DestroyReleasedObjects();

    /* NOTE: Resets everything allocated last time! */

    Rr_ResetArena(Frame->Arena);

    Frame->Profiler = Rr_CreateProfiler(Frame->Arena);

    /* Acquire swapchain image. */

    uint32_t SwapchainImageIndex;
    while (true)
    {
        if (!Rr_RecreateSwapchainIfNeeded())
        {
            RR_LOG_ABORT("Couldn't recreate swapchain!");
            return;
        }
        Result = Device->AcquireNextImageKHR(
            Device->Handle,
            gRenderer->Swapchain.Handle,
            1000000000,
            Frame->AcquireSemaphore,
            NULL,
            &SwapchainImageIndex);
        assert(Result != VK_TIMEOUT && "Swapchain image timeout!");
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
        if (Result == VK_SUCCESS)
        {
            break;
        }
        Rr_SetSwapchainDirty(true);
    }

    Frame->SwapchainImage =
        &gRenderer->SwapchainImages.Data[SwapchainImageIndex];

    Frame->Graph = RR_ALLOC_TYPE(Rr_Graph, Frame->Arena);
    Frame->Graph->QueueType = RR_QUEUE_TYPE_MAIN;
    Frame->Graph->Arena = Frame->Arena;
    Frame->Graph->DescriptorPoolList = Rr_AcquireDescriptorPoolList();
    Frame->Graph->SwapchainImageHandle =
        Rr_GetGraphImageHandle(Frame->Graph, &Frame->SwapchainImage->Container);
}

void Rr_DrawFrame(void)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;
    Rr_Swapchain *Swapchain = &gRenderer->Swapchain;
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

    if (gRenderer->MainQueue.TimestampsEnabled)
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

    RR_BEGIN_FRAME_SECTION("Rr.FrameGraph");

    Rr_ExecuteGraph(
        Frame->Graph,
        gRenderer->MainQueue.FamilyIndex,
        Frame->EarlyCommandBuffer,
        Frame->LateCommandBuffer);

    RR_END_FRAME_SECTION("Rr.FrameGraph");

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
        .QueueFamilyIndex = gRenderer->MainQueue.FamilyIndex,
    };

    if (gRenderer->MainQueue.TimestampsEnabled)
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

    Rr_LockSpinlock(&gRenderer->MainQueue.Lock);

    Device->QueueSubmit(
        gRenderer->MainQueue.Handle,
        2,
        SubmitInfos,
        Frame->SubmitFence);

    uint32_t SwapchainImageIndex =
        (uint32_t)(Frame->SwapchainImage - gRenderer->SwapchainImages.Data);
    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->SwapchainImage->LateSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    VkResult Result =
        Device->QueuePresentKHR(gRenderer->MainQueue.Handle, &PresentInfo);
    assert(Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR);

    Rr_UnlockSpinlock(&gRenderer->MainQueue.Lock);

    Rr_DestroyScratch(Scratch);
}

bool Rr_HasQueue(Rr_QueueType QueueType)
{
    switch (QueueType)
    {
        case RR_QUEUE_TYPE_MAIN:
            return gRenderer->MainQueue.Handle != VK_NULL_HANDLE;
        case RR_QUEUE_TYPE_DEDICATED_TRANSFER:
            return gRenderer->DedicatedTransferQueue.Handle != VK_NULL_HANDLE;
        case RR_QUEUE_TYPE_ASYNC_COMPUTE:
            return gRenderer->AsyncComputeQueue.Handle != VK_NULL_HANDLE;
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
            return &gRenderer->MainQueue;
        case RR_QUEUE_TYPE_DEDICATED_TRANSFER:
            return &gRenderer->DedicatedTransferQueue;
        case RR_QUEUE_TYPE_ASYNC_COMPUTE:
            return &gRenderer->AsyncComputeQueue;
        default:
            RR_LOG_ABORT("Invalid queue type!");
    }
}

Rr_Frame *Rr_GetPreviousFrame(void)
{
    return &gRenderer->Frames[(gRenderer->FrameNumber - 1) % RR_FRAME_OVERLAP];
}

Rr_Frame *Rr_GetCurrentFrame(void)
{
    return &gRenderer->Frames[gRenderer->FrameIndex];
}

bool Rr_IsUsingTransferQueue(void)
{
    return gRenderer->DedicatedTransferQueue.Handle != VK_NULL_HANDLE;
}

bool Rr_IsIntegratedGPU(void)
{
    return gRenderer->PhysicalDevice.Properties.deviceType ==
           VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}

size_t Rr_GetMaxUniformRange(void)
{
    return gRenderer->PhysicalDevice.Properties.limits.maxUniformBufferRange;
}

size_t Rr_GetUniformAlignment(void)
{
    return gRenderer->PhysicalDevice.Properties.limits
        .minUniformBufferOffsetAlignment;
}

size_t Rr_GetStorageAlignment(void)
{
    return gRenderer->PhysicalDevice.Properties.limits
        .minStorageBufferOffsetAlignment;
}

size_t Rr_GetMaxComputeSharedMemorySize(void)
{
    return gRenderer->PhysicalDevice.Properties.limits
        .maxComputeSharedMemorySize;
}

size_t Rr_GetMaxComputeWorkgroupInvocations(void)
{
    return gRenderer->PhysicalDevice.Properties.limits
        .maxComputeWorkGroupInvocations;
}

Rr_ImageFormat Rr_GetSwapchainFormat(void)
{
    return Rr_ToImageFormat(gRenderer->Swapchain.Format);
}

Rr_IntVec2 Rr_GetSwapchainSize(void)
{
    return (Rr_IntVec2){
        (int32_t)gRenderer->Swapchain.Extent.width,
        (int32_t)gRenderer->Swapchain.Extent.height,
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
        *Count = gRenderer->Swapchain.PresentModeCount;
    }
    return gRenderer->Swapchain.PresentModes;
}

Rr_PresentMode Rr_GetPresentMode(void)
{
    return gRenderer->Swapchain.PresentMode;
}

const char *Rr_GetPresentModeString(Rr_PresentMode PresentMode)
{
    assert((size_t)PresentMode < RR_ARRAY_COUNT(RR_PRESENT_MODES));

    return RR_PRESENT_MODES[(size_t)PresentMode];
}

bool Rr_SetPresentMode(Rr_PresentMode PresentMode)
{
    gRenderer->Swapchain.PresentMode = PresentMode;
    Rr_SetSwapchainDirty(true);

    return true;
}

VkRenderPass Rr_GetVulkanRenderPass(Rr_RenderPassMapKey *Key)
{
    VkRenderPass *RenderPassRef = NULL;

    Rr_LockSpinlock(&gRenderer->RenderPassStorageLock);

    uint32_t AttachmentCount =
        (uint32_t)(Key->ColorAttachmentCount + Key->ResolveAttachmentCount +
                   Key->DepthStencil);

    size_t HashSize = offsetof(Rr_RenderPassMapKey, Attachments) +
                      AttachmentCount * sizeof(Rr_RenderPassAttachment);

    Rr_RenderPassMap **MapRef = &gRenderer->RenderPassStorage.Map;
    for (uint64_t Hash = XXH64(Key, HashSize, 0); *MapRef; Hash <<= 2)
    {
        if (memcmp(Key, &(*MapRef)->Key, HashSize) == 0)
        {
            RenderPassRef = &(*MapRef)->Value;

            goto Found;
        }
        MapRef = &(*MapRef)->Children[Hash >> 62];
    }
    *MapRef = Rr_PushRenderPassMapIntoHiveLocked(
                  &gRenderer->RenderPassStorage.Hive,
                  gRenderer->Arena,
                  &gRenderer->Lock)
                  .Element;
    (*MapRef)->Key = *Key;
    (*MapRef)->Value = VK_NULL_HANDLE;
    RR_ZERO((*MapRef)->Children);
    RenderPassRef = &(*MapRef)->Value;

Found:

    Rr_UnlockSpinlock(&gRenderer->RenderPassStorageLock);

    if (*RenderPassRef != VK_NULL_HANDLE)
    {
        return *RenderPassRef;
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkAttachmentReference *ColorReferences = NULL;
    VkAttachmentReference *ResolveReferences = NULL;
    VkAttachmentReference *DepthReference = NULL;

    VkAttachmentDescription *Descriptions = RR_ALLOC_TYPE_COUNT(
        VkAttachmentDescription,
        AttachmentCount,
        Scratch.Arena);

    uint32_t ResolveDescriptionIndex = Key->ColorAttachmentCount;

    if (Key->ColorAttachmentCount > 0)
    {
        ColorReferences = RR_ALLOC_TYPE_COUNT(
            VkAttachmentReference,
            Key->ColorAttachmentCount,
            Scratch.Arena);

        ResolveReferences = RR_ALLOC_TYPE_COUNT(
            VkAttachmentReference,
            Key->ColorAttachmentCount,
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

            if (RR_HAS_BIT(Key->ResolveMask, 1 << Index))
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
            RR_ALLOC_NO_ZERO(sizeof(VkAttachmentReference), Scratch.Arena);
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

    Rr_Device *Device = &gRenderer->Device;

    Device->CreateRenderPass(
        Device->Handle,
        &RenderPassCreateInfo,
        NULL,
        RenderPassRef);

    Rr_DestroyScratch(Scratch);

    return *RenderPassRef;
}

VkFramebuffer Rr_GetVulkanFramebuffer(
    VkRenderPass RenderPass,
    Rr_FramebufferMapKey *Key)
{
    VkFramebuffer *FramebufferRef = NULL;

    Rr_LockSpinlock(&gRenderer->FramebufferStorageLock);

    uint32_t AttachmentCount =
        (uint32_t)(Key->ColorAttachmentCount + Key->ResolveAttachmentCount +
                   Key->DepthStencil);

    size_t HashSize = offsetof(Rr_FramebufferMapKey, ImageViews) +
                      AttachmentCount * sizeof(VkImageView);

    Rr_FramebufferMap **MapRef = &gRenderer->FramebufferStorage.Map;
    for (uint64_t Hash = XXH64(Key, HashSize, 0); *MapRef; Hash <<= 2)
    {
        if ((*MapRef)->Value == VK_NULL_HANDLE)
        {
            (*MapRef)->Key = *Key;
            FramebufferRef = &(*MapRef)->Value;

            goto Found;
        }
        else if (memcmp(Key, &(*MapRef)->Key, HashSize) == 0)
        {
            FramebufferRef = &(*MapRef)->Value;

            goto Found;
        }
        MapRef = &(*MapRef)->Children[Hash >> 62];
    }
    *MapRef = Rr_PushFramebufferMapIntoHiveLocked(
                  &gRenderer->FramebufferStorage.Hive,
                  gRenderer->Arena,
                  &gRenderer->Lock)
                  .Element;
    (*MapRef)->Key = *Key;
    (*MapRef)->Value = VK_NULL_HANDLE;
    RR_ZERO((*MapRef)->Children);
    FramebufferRef = &(*MapRef)->Value;

Found:

    Rr_UnlockSpinlock(&gRenderer->FramebufferStorageLock);

    if (*FramebufferRef != VK_NULL_HANDLE)
    {
        return *FramebufferRef;
    }

    VkFramebufferCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = RenderPass,
        .width = Key->Extent.width,
        .height = Key->Extent.height,
        .layers = Key->Extent.depth,
        .attachmentCount = AttachmentCount,
        .pAttachments = Key->ImageViews,
    };

    Rr_Device *Device = &gRenderer->Device;

    Device
        ->CreateFramebuffer(Device->Handle, &CreateInfo, NULL, FramebufferRef);

    return *FramebufferRef;
}

void Rr_DestroyVulkanFramebuffers(VkImageView ImageView)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->FramebufferStorageLock);

    for (Rr_FramebufferMapHiveIterator It =
             gRenderer->FramebufferStorage.Hive.Begin;
         It.Element != gRenderer->FramebufferStorage.Hive.End.Element;)
    {
        Rr_FramebufferMap *Map = It.Element;
        if (Map->Value != VK_NULL_HANDLE)
        {
            bool Destroy = false;
            size_t Boundary = Map->Key.ColorAttachmentCount +
                              Map->Key.ResolveAttachmentCount +
                              (size_t)Map->Key.DepthStencil;
            for (size_t Index = 0; Index < Boundary; ++Index)
            {
                if (Map->Key.ImageViews[Index] == ImageView)
                {
                    Destroy = true;
                    break;
                }
            }

            if (Destroy)
            {
                Device->DestroyFramebuffer(Device->Handle, Map->Value, NULL);
                Map->Value = VK_NULL_HANDLE;
            }
        }

        Rr_AdvanceFramebufferMapHiveIterator(&It);
    }

    Rr_UnlockSpinlock(&gRenderer->FramebufferStorageLock);
}

VkSemaphore Rr_AcquireVulkanSemaphore(void)
{
    VkSemaphore Semaphore;

    bool Locked = Rr_TryLockSpinlock(&gRenderer->SemaphoresLock);

    if (Locked && gRenderer->Semaphores.Count > 0)
    {
        Semaphore = RR_POP_FROM_ARRAY(&gRenderer->Semaphores);
    }
    else
    {
        Rr_Device *Device = &gRenderer->Device;

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
        Rr_UnlockSpinlock(&gRenderer->SemaphoresLock);
    }

    return Semaphore;
}

void Rr_ReleaseVulkanSemaphore(VkSemaphore Semaphore)
{
    Rr_LockSpinlock(&gRenderer->SemaphoresLock);
    Rr_LockSpinlock(&gRenderer->Lock);

    *RR_PUSH_INTO_ARRAY(&gRenderer->Semaphores, gRenderer->Arena) = Semaphore;

    Rr_UnlockSpinlock(&gRenderer->Lock);
    Rr_UnlockSpinlock(&gRenderer->SemaphoresLock);
}

VkFence Rr_AcquireVulkanFence(void)
{
    VkFence Fence;

    bool Locked = Rr_TryLockSpinlock(&gRenderer->FencesLock);

    if (Locked && gRenderer->Fences.Count > 0)
    {
        Fence = RR_POP_FROM_ARRAY(&gRenderer->Fences);
    }
    else
    {
        Rr_Device *Device = &gRenderer->Device;

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
        Rr_UnlockSpinlock(&gRenderer->FencesLock);
    }

    return Fence;
}

void Rr_ReleaseVulkanFence(VkFence Fence)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->FencesLock);
    Rr_LockSpinlock(&gRenderer->Lock);

    *RR_PUSH_INTO_ARRAY(&gRenderer->Fences, gRenderer->Arena) = Fence;

    Rr_UnlockSpinlock(&gRenderer->Lock);
    Rr_UnlockSpinlock(&gRenderer->FencesLock);

    Device->ResetFences(Device->Handle, 1, &Fence);
}

Rr_CommandPools *Rr_AcquireCommandPools(void)
{
    Rr_ThreadContext *ThreadContext = Rr_GetThreadContext();

    if (ThreadContext->CommandPools)
    {
        return ThreadContext->CommandPools;
    }

    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->CommandPoolsLock);

    if (gRenderer->FreeCommandPools)
    {
        ThreadContext->CommandPools = gRenderer->FreeCommandPools;
        gRenderer->FreeCommandPools = ThreadContext->CommandPools->Next;

        Rr_UnlockSpinlock(&gRenderer->CommandPoolsLock);
    }
    else
    {
        Rr_UnlockSpinlock(&gRenderer->CommandPoolsLock);

        Rr_LockSpinlock(&gRenderer->Lock);
        ThreadContext->CommandPools =
            RR_ALLOC_NO_ZERO(sizeof(Rr_CommandPools), gRenderer->Arena);
        Rr_UnlockSpinlock(&gRenderer->Lock);

        Device->CreateCommandPool(
            Device->Handle,
            &(VkCommandPoolCreateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = gRenderer->MainQueue.FamilyIndex,
            },
            NULL,
            &ThreadContext->CommandPools->Graphics);

        Device->CreateCommandPool(
            Device->Handle,
            &(VkCommandPoolCreateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex =
                    gRenderer->DedicatedTransferQueue.FamilyIndex,
            },
            NULL,
            &ThreadContext->CommandPools->Transfer);

        ThreadContext->CommandPools->Compute = NULL;

        // Device->CreateCommandPool(
        //     Device->Handle,
        //     &(VkCommandPoolCreateInfo){
        //         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        //         .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        //         .queueFamilyIndex = gRenderer->ComputeQueue.FamilyIndex,
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

    Rr_LockSpinlock(&gRenderer->CommandPoolsLock);

    ThreadContext->CommandPools->Next = gRenderer->FreeCommandPools;
    gRenderer->FreeCommandPools = ThreadContext->CommandPools;

    Rr_UnlockSpinlock(&gRenderer->CommandPoolsLock);

    ThreadContext->CommandPools = NULL;
}

bool Rr_IsSRGBFormat(Rr_ImageFormat Format)
{
    return Format == RR_IMAGE_FORMAT_R8G8B8A8_SRGB ||
           Format == RR_IMAGE_FORMAT_B8G8R8A8_SRGB ||
           Format == RR_IMAGE_FORMAT_A8B8G8R8_SRGB_PACK32;
}

static RR_THREAD_LOCAL char NextObjectName[RR_MAX_OBJECT_NAME_LENGTH] = { 0 };

void Rr_SetNextObjectName(const char *Name)
{
    strncpy(NextObjectName, Name, sizeof(NextObjectName) - 1);
}

void Rr_SetNextObjectNameF(const char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vsnprintf(NextObjectName, sizeof(NextObjectName), Format, Args);
    va_end(Args);
}

void Rr_ConsumeNextObjectName(char Dst[RR_MAX_OBJECT_NAME_LENGTH])
{
    if (NextObjectName[0] != '\0')
    {
        for (uint32_t Index = 0; Index < RR_MAX_OBJECT_NAME_LENGTH; ++Index)
        {
            Dst[Index] = NextObjectName[Index];
        }
        NextObjectName[0] = '\0';
    }
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
    gRenderer->Instance.SetDebugUtilsObjectNameEXT(
        gRenderer->Device.Handle,
        &ObjectNameInfo);
#endif
}

void Rr_BeginVulkanCommandBufferLabel(
    VkCommandBuffer CommandBuffer,
    const char *Name)
{
#ifdef RR_USE_GPU_DEBUG_UTILS
    Rr_Instance *Instance = &gRenderer->Instance;
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
    Rr_Instance *Instance = &gRenderer->Instance;
    Instance->CmdEndDebugUtilsLabelEXT(CommandBuffer);
#endif
}

void Rr_PrintDestroyMessage(const char *Type, const char *Name, void *Address)
{
    RR_LOG_INFO(
        "Destroying %s: { name: \"%s\", address = %p }",
        Type,
        Name[0] != '\0' ? Name : "UNNAMED",
        (void *)Address);
}
