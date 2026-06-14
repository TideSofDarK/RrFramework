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

#include "Rr_RHI.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_RHI
#include "Rr_LogMacro.h"

#include "Rr_Allocator.h"
#include "Rr_Thread.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>

#include <assert.h>
#include <stdio.h>

Rr_RHI *gRHI;

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
    Rr_Device *Device = &gRHI->Device;

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
        gRHI->Swapchain.RecreateEventPending = true;
    }

    return Recreate;
}

static void Rr_InitFrames(void)
{
    Rr_Device *Device = &gRHI->Device;
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
    Rr_Device *Device = &gRHI->Device;

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
    Rr_Device *Device = &gRHI->Device;

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
    Rr_Device *Device = &gRHI->Device;

    Device->DestroyDescriptorPool(
        Device->Handle,
        gRHI->EmptyDescriptorPool,
        NULL);
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
        &gRHI->Device,
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
         !Rr_IsHandleSetEnd(It);)
    {
        Rr_Buffer *Buffer = (Rr_Buffer *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&Buffer->RefCount))
        {
            Rr_DestroyBuffer(Buffer);
            It = Rr_EraseFromHandleSet(It);
        }
        else
        {
            It = Rr_NextInHandleSet(It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedBuffersLock);

    Rr_LockSpinlock(&gRHI->ReleasedImagesLock);

    for (Rr_HandleSetIterator It = Rr_BeginInHandleSet(&gRHI->ReleasedImages);
         !Rr_IsHandleSetEnd(It);)
    {
        Rr_Image *Image = (Rr_Image *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&Image->RefCount))
        {
            Rr_DestroyImage(Image);
            It = Rr_EraseFromHandleSet(It);
        }
        else
        {
            It = Rr_NextInHandleSet(It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedImagesLock);

    Rr_LockSpinlock(&gRHI->ReleasedSamplersLock);

    for (Rr_HandleSetIterator It = Rr_BeginInHandleSet(&gRHI->ReleasedSamplers);
         !Rr_IsHandleSetEnd(It);)
    {
        Rr_Sampler *Sampler = (Rr_Sampler *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&Sampler->RefCount))
        {
            Rr_DestroySampler(Sampler);
            It = Rr_EraseFromHandleSet(It);
        }
        else
        {
            It = Rr_NextInHandleSet(It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedSamplersLock);

    Rr_LockSpinlock(&gRHI->ReleasedComputePipelinesLock);

    for (Rr_HandleSetIterator It =
             Rr_BeginInHandleSet(&gRHI->ReleasedComputePipelines);
         !Rr_IsHandleSetEnd(It);)
    {
        Rr_ComputePipeline *ComputePipeline =
            (Rr_ComputePipeline *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&ComputePipeline->RefCount))
        {
            Rr_DestroyComputePipeline(ComputePipeline);
            It = Rr_EraseFromHandleSet(It);
        }
        else
        {
            It = Rr_NextInHandleSet(It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedComputePipelinesLock);

    Rr_LockSpinlock(&gRHI->ReleasedGraphicsPipelinesLock);

    for (Rr_HandleSetIterator It =
             Rr_BeginInHandleSet(&gRHI->ReleasedGraphicsPipelines);
         !Rr_IsHandleSetEnd(It);)
    {
        Rr_GraphicsPipeline *GraphicsPipeline =
            (Rr_GraphicsPipeline *)It.Data->Key;
        if (!Rr_LoadAtomicIntRelaxed(&GraphicsPipeline->RefCount))
        {
            Rr_DestroyGraphicsPipeline(GraphicsPipeline);
            It = Rr_EraseFromHandleSet(It);
        }
        else
        {
            It = Rr_NextInHandleSet(It);
        }
    }

    Rr_UnlockSpinlock(&gRHI->ReleasedGraphicsPipelinesLock);
}

void Rr_CleanupRHI(void)
{
    Rr_Instance *Instance = &gRHI->Instance;
    Rr_Device *Device = &gRHI->Device;

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
             gRHI->PipelineLayoutStorage.Hive.Begin;
         It.Element != gRHI->PipelineLayoutStorage.Hive.End.Element;)
    {
        Device->DestroyPipelineLayout(Device->Handle, It.Element->Handle, NULL);
        Rr_AdvancePipelineLayoutHiveIterator(&It);
    }

    for (Rr_DescriptorSetLayoutHiveIterator It =
             gRHI->DescriptorSetLayoutStorage.Hive.Begin;
         It.Element != gRHI->DescriptorSetLayoutStorage.Hive.End.Element;)
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
             Rr_BeginInRenderPassMap(&gRHI->RenderPassMap);
         !Rr_IsRenderPassMapEnd(It);
         It = Rr_NextInRenderPassMap(It))
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

void Rr_NewFrame(void)
{
    Rr_Device *Device = &gRHI->Device;

    gRHI->FrameNumber++;
    gRHI->FrameIndex = gRHI->FrameNumber % RR_FRAME_OVERLAP;

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

    Rr_Graph *Graph = Rr_Alloc(sizeof(Rr_Graph), Frame->Arena);
    Graph->QueueType = RR_QUEUE_TYPE_MAIN;
    Graph->Primary = true;
    Graph->DescriptorPoolList = Rr_AcquireDescriptorPoolList();
    Graph->Arena = Frame->Arena;
    Rr_InitHandleSet(&Graph->Samplers, Frame->Arena);
    Rr_InitHandleSet(&Graph->ComputePipelines, Frame->Arena);
    Rr_InitHandleSet(&Graph->GraphicsPipelines, Frame->Arena);

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

    Rr_Device *Device = &gRHI->Device;
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

Rr_Frame *Rr_GetPreviousFrame(void)
{
    return &gRHI->Frames[(gRHI->FrameNumber - 1) % RR_FRAME_OVERLAP];
}

Rr_Frame *Rr_GetCurrentFrame(void)
{
    return &gRHI->Frames[gRHI->FrameIndex];
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

const char *Rr_GetPresentModeString(Rr_PresentMode PresentMode)
{
    assert((size_t)PresentMode < RR_ARRAY_COUNT(RR_PRESENT_MODES));

    return RR_PRESENT_MODES[(size_t)PresentMode];
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
    if (!Rr_IsRenderPassMapEnd(It))
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

    Rr_Device *Device = &gRHI->Device;

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
    if (!Rr_IsFramebufferMapEnd(It))
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

    Rr_Device *Device = &gRHI->Device;

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
    Rr_Device *Device = &gRHI->Device;

    Rr_LockSpinlock(&gRHI->FramebufferMapLock);

    Rr_FramebufferMapIterator It =
        Rr_BeginInFramebufferMap(&gRHI->FramebufferMap);
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
        Rr_Device *Device = &gRHI->Device;

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
        Rr_Device *Device = &gRHI->Device;

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

    Rr_Device *Device = &gRHI->Device;

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

    Rr_Device *Device = &gRHI->Device;

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
