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

#include "Rr_Renderer.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_RENDERER
#include "Rr_App.h"
#include "Rr_LogMacro.h"
#include "Rr_System.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>

#include <assert.h>
#include <stdio.h>

Rr_Renderer *gRenderer;

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

    VkPresentModeKHR *VulkanPresentModes = Rr_Alloc(
        sizeof(VkPresentModeKHR) * VulkanPresentModeCount,
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
        Rr_Alloc(sizeof(VkSurfaceFormatKHR) * FormatCount, Scratch.Arena);
    Instance->GetPhysicalDeviceSurfaceFormatsKHR(
        gRenderer->PhysicalDevice.Handle,
        gRenderer->Surface,
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
        Rr_Alloc(sizeof(VkImage) * ImageCount, Scratch.Arena);

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

static void Rr_InitEmptyDescriptorSet(void)
{
    Rr_Device *Device = &gRenderer->Device;

    VkResult Result;

    Result = Device->CreateDescriptorPool(
        Device->Handle,
        &(VkDescriptorPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
        },
        NULL,
        &gRenderer->EmptyDescriptorPool);
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
            .descriptorPool = gRenderer->EmptyDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &EmptyDescriptorSetLayout,
        },
        &gRenderer->EmptyDescriptorSet);
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
    Rr_Device *Device = &gRenderer->Device;

    Device->DestroyDescriptorPool(
        Device->Handle,
        gRenderer->EmptyDescriptorPool,
        NULL);
}

void Rr_InitRenderer(const char *Title)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gRenderer = Rr_Alloc(sizeof(Rr_Renderer), Arena);
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
    Rr_InitEmptyDescriptorSet();

    Rr_InitFramebufferMap(&gRenderer->FramebufferMap, Arena);
    Rr_InitRenderPassMap(&gRenderer->RenderPassMap, Arena);

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
        if (!Rr_LoadAtomicIntRelaxed(&Buffer->RefCount))
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
        if (!Rr_LoadAtomicIntRelaxed(&Image->RefCount))
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
        if (!Rr_LoadAtomicIntRelaxed(&Sampler->RefCount))
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
        if (!Rr_LoadAtomicIntRelaxed(&ComputePipeline->RefCount))
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
        if (!Rr_LoadAtomicIntRelaxed(&GraphicsPipeline->RefCount))
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
}

void Rr_CleanupRenderer(void)
{
    Rr_Instance *Instance = &gRenderer->Instance;
    Rr_Device *Device = &gRenderer->Device;

    Rr_WaitIdle();

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Graph *Graph = gRenderer->Frames[Index].Graph;

        if (Graph)
        {
            Rr_ReleaseGraphResources(Graph);
        }
    }

    Rr_DestroyReleasedObjects();

    for (Rr_PipelineLayoutHiveIterator It =
             gRenderer->PipelineLayoutStorage.Hive.Begin;
         It.Element != gRenderer->PipelineLayoutStorage.Hive.End.Element;)
    {
        Device->DestroyPipelineLayout(Device->Handle, It.Element->Handle, NULL);
        Rr_AdvancePipelineLayoutHiveIterator(&It);
    }

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

    Rr_CleanupEmptyDescriptorSet();

    /* NOTE: VkFramebuffers are destroyed along with VkImageViews.
     * For now, we don't care for destroying render passes unless it's
     * application shutdown. */

    for (Rr_RenderPassMapIterator It =
             Rr_BeginInRenderPassMap(&gRenderer->RenderPassMap);
         !Rr_IsRenderPassMapEnd(It);
         It = Rr_NextInRenderPassMap(It))
    {
        Device->DestroyRenderPass(Device->Handle, It.Data->Value, NULL);
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
    }

    if (Frame->Graph)
    {
        Rr_ReleaseGraphResources(Frame->Graph);
    }

    Rr_DestroyReleasedObjects();

    /* NOTE: Resets everything allocated last time! */

    Rr_ResetArena(Frame->Arena);

    Frame->Profiler = Rr_CreateProfiler(Frame->Arena);

    /* Acquire swapchain image. */

    uint32_t SwapchainImageIndex = UINT32_MAX;
    for (;;)
    {
        if (!Rr_RecreateSwapchainIfNeeded())
        {
            break;
        }
        Result = Device->AcquireNextImageKHR(
            Device->Handle,
            gRenderer->Swapchain.Handle,
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
            &gRenderer->SwapchainImages.Data[SwapchainImageIndex];

        gRenderer->Swapchain.Unavailable = false;
    }
    else
    {
        /* HACK: Use whatever swapchain image if for whatever reason the
         * swapchain is not available. We will ultimately skip issuing this
         * frame to the GPU but user might want to know its format/extent/etc.
         */

        Frame->SwapchainImage = &gRenderer->SwapchainImages.Data[0];

        gRenderer->Swapchain.Unavailable = true;
    }

    Frame->Graph = Rr_Alloc(sizeof(Rr_Graph), Frame->Arena);
    Frame->Graph->QueueType = RR_QUEUE_TYPE_MAIN;
    Frame->Graph->Primary = true;
    Frame->Graph->DescriptorPoolList = Rr_AcquireDescriptorPoolList();
    Frame->Graph->Arena = Frame->Arena;
    Frame->Graph->SwapchainImageHandle =
        Rr_GetGraphImageHandle(Frame->Graph, &Frame->SwapchainImage->Container);
}

void Rr_DrawFrame(void)
{
    if (gRenderer->Swapchain.Unavailable)
    {
        return;
    }

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

    Rr_BeginFrameSection("Rr.FrameGraph");

    Rr_ExecuteGraph(
        Frame->Graph,
        gRenderer->MainQueue.FamilyIndex,
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

    Rr_UnlockSpinlock(&gRenderer->MainQueue.Lock);

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

VkRenderPass Rr_GetRenderPass(Rr_RenderPassKey *Key)
{
    Rr_LockSpinlock(&gRenderer->RenderPassMapLock);

    Rr_RenderPassMapIterator It =
        Rr_FindInRenderPassMap(&gRenderer->RenderPassMap, Key);
    if (!Rr_IsRenderPassMapEnd(It))
    {
        Rr_UnlockSpinlock(&gRenderer->RenderPassMapLock);

        return It.Data->Value;
    }

    Rr_UnlockSpinlock(&gRenderer->RenderPassMapLock);

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

    Rr_Device *Device = &gRenderer->Device;

    VkRenderPass Handle = VK_NULL_HANDLE;
    Device->CreateRenderPass(
        Device->Handle,
        &RenderPassCreateInfo,
        NULL,
        &Handle);

    Rr_LockSpinlock(&gRenderer->RenderPassMapLock);
    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_InsertIntoRenderPassMap(
        &gRenderer->RenderPassMap,
        Key,
        &Handle,
        gRenderer->Arena);

    Rr_UnlockSpinlock(&gRenderer->Lock);
    Rr_UnlockSpinlock(&gRenderer->RenderPassMapLock);

    Rr_DestroyScratch(Scratch);

    return Handle;
}

VkFramebuffer Rr_GetFramebuffer(Rr_FramebufferKey *Key)
{
    Rr_LockSpinlock(&gRenderer->FramebufferMapLock);

    Rr_FramebufferMapIterator It =
        Rr_FindInFramebufferMap(&gRenderer->FramebufferMap, Key);
    if (!Rr_IsFramebufferMapEnd(It))
    {
        Rr_UnlockSpinlock(&gRenderer->FramebufferMapLock);

        return It.Data->Value;
    }

    Rr_UnlockSpinlock(&gRenderer->FramebufferMapLock);

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

    Rr_Device *Device = &gRenderer->Device;

    VkFramebuffer Handle = VK_NULL_HANDLE;
    Device->CreateFramebuffer(Device->Handle, &CreateInfo, NULL, &Handle);

    Rr_LockSpinlock(&gRenderer->FramebufferMapLock);
    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_InsertIntoFramebufferMap(
        &gRenderer->FramebufferMap,
        Key,
        &Handle,
        gRenderer->Arena);

    Rr_UnlockSpinlock(&gRenderer->Lock);
    Rr_UnlockSpinlock(&gRenderer->FramebufferMapLock);

    return Handle;
}

void Rr_DestroyFramebuffers(VkImageView ImageView)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->FramebufferMapLock);

    Rr_FramebufferMapIterator It =
        Rr_BeginInFramebufferMap(&gRenderer->FramebufferMap);
    while (!Rr_IsFramebufferMapEnd(It))
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
            It = Rr_EraseFromFramebufferMap(It);
        }
        else
        {
            It = Rr_NextInFramebufferMap(It);
        }
    }

    Rr_UnlockSpinlock(&gRenderer->FramebufferMapLock);
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
    if (Semaphore == VK_NULL_HANDLE)
    {
        return;
    }

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
    if (Fence == VK_NULL_HANDLE)
    {
        return;
    }

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
            Rr_AllocNoZero(sizeof(Rr_CommandPools), gRenderer->Arena);
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
    if (!Name)
    {
        return;
    }

    size_t Length = strlen(Name);
    if (!Length)
    {
        return;
    }

    if (Length > RR_MAX_OBJECT_NAME_LENGTH - 1)
    {
        memcpy(NextObjectName, Name, RR_MAX_OBJECT_NAME_LENGTH - 1);
        NextObjectName[RR_MAX_OBJECT_NAME_LENGTH - 1] = '\0';

        return;
    }

    memcpy(NextObjectName, Name, Length);
    NextObjectName[Length] = '\0';
}

void
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    Rr_SetNextObjectNameF(const char *Format, ...)
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
