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

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Rr_Renderer.h"

#include "Rr_App.h"
#include "Rr_Log.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>

#include <xxHash/xxhash.h>

#include <assert.h>

Rr_Renderer *gRenderer;

static inline void Rr_DestroySwapchainImage(Rr_SwapchainImage *SwapchainImage)
{
    Rr_Device *Device = &gRenderer->Device;

    if (SwapchainImage->ViewStorage)
    {
        Rr_DestroyImageViewStorage(SwapchainImage->ViewStorage, true);
    }

    if (SwapchainImage->Handle)
    {
        Rr_LockSpinlock(&gRenderer->Lock);

        Rr_EraseSyncState(
            &gRenderer->SyncStateStorage,
            (uint64_t)SwapchainImage->Handle);

        Rr_UnlockSpinlock(&gRenderer->Lock);
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
        Scratch.Arena,
        VkPresentModeKHR,
        VulkanPresentModeCount);
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
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkSurfaceFormatKHR, FormatCount);
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
        RR_ABORT("No suitable surface format found!");
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

    VkImage *Images = RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkImage, ImageCount);

    Device->GetSwapchainImagesKHR(
        gRenderer->Device.Handle,
        gRenderer->Swapchain.Handle,
        &ImageCount,
        Images);

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

        Image->Handle = Images[Index];

        Image->ViewStorage = Rr_CreateImageViewStorage();
        Image->EarlySemaphore = Rr_AcquireVulkanSemaphore();
        Image->LateSemaphore = Rr_AcquireVulkanSemaphore();

        Rr_LockSpinlock(&gRenderer->Lock);

        Rr_FindSyncState(
            &gRenderer->SyncStateStorage,
            (uint64_t)Image->Handle,
            gRenderer->Arena)
            ->StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        Rr_UnlockSpinlock(&gRenderer->Lock);
    }

    Rr_SetSwapchainDirty(false);

    Rr_DestroyScratch(Scratch);

    return true;
}

static bool Rr_RecreateSwapchain(void)
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
    Rr_ThreadContext *ThreadContext = Rr_GetThreadContext();

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame *Frame = &Frames[Index];
        Frame->Arena = Rr_CreateDefaultArena();
        Frame->AcquireSemaphore = Rr_AcquireVulkanSemaphore();

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .commandPool = ThreadContext->CommandPools->Graphics,
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

        if (gRenderer->GraphicsQueue.TimestampsEnabled)
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

        Frame->VirtualSwapchainImage.SampleCount = VK_SAMPLE_COUNT_1_BIT;
        Frame->VirtualSwapchainImage.AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        Frame->VirtualSwapchainImage.AllocatedImageCount = 1;
        Frame->VirtualSwapchainImage.AllocatedImages[0] = (Rr_AllocatedImage){
            .Container = &Frame->VirtualSwapchainImage,
        };
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

    gRenderer = RR_ALLOC_TYPE(Arena, Rr_Renderer);
    gRenderer->Arena = Arena;

    Rr_InitLoader(&gRenderer->Loader);
    Rr_InitInstance(&gRenderer->Loader, Title, &gRenderer->Instance);
    Rr_InitSurface(&gRenderer->Instance, &gRenderer->Surface);
    Rr_InitDeviceAndQueues(
        &gRenderer->Instance,
        gRenderer->Surface,
        &gRenderer->PhysicalDevice,
        &gRenderer->Device,
        &gRenderer->GraphicsQueue,
        &gRenderer->TransferQueue);

    Rr_InitVMA();
    Rr_InitThreadContext();
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

    Rr_CleanupThreadContext();
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

    for (Rr_CommandPools *CommandPools = gRenderer->CommandPools; CommandPools;
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

    /* Wait for previous work associated with given frame index. */

    if (Frame->SubmitFence != VK_NULL_HANDLE)
    {
        VkResult Result = Device->WaitForFences(
            Device->Handle,
            1,
            &Frame->SubmitFence,
            true,
            1000000000);
        assert(Result != VK_TIMEOUT && "Submit fence timeout!");

        if (gRenderer->GraphicsQueue.TimestampsEnabled)
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

    /* Swapchain might have been recreated so fill in updated settings. */

    Frame->VirtualSwapchainImage.Extent = gRenderer->Swapchain.Extent;
    Frame->VirtualSwapchainImage.Format = gRenderer->Swapchain.Format;

    Frame->Graph = RR_ALLOC_TYPE(Frame->Arena, Rr_Graph);
    Frame->Graph->Flags = RR_GRAPH_FLAGS_GRAPHICS_BIT |
                          RR_GRAPH_FLAGS_COMPUTE_BIT |
                          RR_GRAPH_FLAGS_TRANSFER_BIT;
    Frame->Graph->Arena = Frame->Arena;
    Frame->Graph->DescriptorPoolList = Rr_AcquireDescriptorPoolList();
    Frame->Graph->SwapchainImageHandle =
        Rr_GetGraphImageHandle(Frame->Graph, &Frame->VirtualSwapchainImage);
}

void Rr_DrawFrame(void)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;
    Rr_Swapchain *Swapchain = &gRenderer->Swapchain;
    Rr_Frame *Frame = Rr_GetCurrentFrame();

    VkResult Result;

    /* Acquire swapchain image. */

    uint32_t SwapchainImageIndex;
    while (true)
    {
        Result = Device->AcquireNextImageKHR(
            Device->Handle,
            Swapchain->Handle,
            1000000000,
            Frame->AcquireSemaphore,
            NULL,
            &SwapchainImageIndex);
        assert(Result != VK_TIMEOUT && "Swapchain image timeout!");
        if (Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR)
        {
            break;
        }
        Rr_SetSwapchainDirty(true);
        if (Rr_RecreateSwapchain() == false)
        {
            return;
        }
    }

    Frame->SubmitFence = Rr_AcquireVulkanFence();

    Rr_SwapchainImage *SwapchainImage =
        &gRenderer->SwapchainImages.Data[SwapchainImageIndex];
    VkImage SwapchainImageHandle = SwapchainImage->Handle;

    /* Now that we acquired swapchain image index we can
     * put real handles to virtual swapchain image which
     * will be used by the graph. */

    Frame->VirtualSwapchainImage.Extent = gRenderer->Swapchain.Extent;
    Frame->VirtualSwapchainImage.Format = gRenderer->Swapchain.Format;
    Frame->VirtualSwapchainImage.AllocatedImages[0].ViewStorage =
        gRenderer->SwapchainImages.Data[SwapchainImageIndex].ViewStorage;
    Frame->VirtualSwapchainImage.AllocatedImages[0].Handle =
        SwapchainImageHandle;

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

    if (gRenderer->GraphicsQueue.TimestampsEnabled)
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
        &gRenderer->SyncStateStorage,
        &gRenderer->Lock,
        gRenderer->Arena,
        gRenderer->GraphicsQueue.FamilyIndex,
        Frame->EarlyCommandBuffer,
        Frame->LateCommandBuffer);

    RR_END_FRAME_SECTION("Rr.FrameGraph");

    Device->EndCommandBuffer(Frame->EarlyCommandBuffer);

    /* Always transition swapchain image to present layout. */

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_SyncState SwapchainImageSyncState = Rr_GetSyncState(
        &gRenderer->SyncStateStorage,
        (uint64_t)SwapchainImageHandle);

    Rr_UnlockSpinlock(&gRenderer->Lock);

    Device->CmdPipelineBarrier(
        Frame->LateCommandBuffer,
        SwapchainImageSyncState.StageMask,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &(VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image = SwapchainImageHandle,
            .oldLayout = SwapchainImageSyncState.Layout,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcAccessMask = SwapchainImageSyncState.AccessMask,
            .dstAccessMask = 0,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        });

    Rr_LockSpinlock(&gRenderer->Lock);

    *Rr_FindSyncState(
        &gRenderer->SyncStateStorage,
        (uint64_t)SwapchainImageHandle,
        gRenderer->Arena) = (Rr_SyncState){
        .AccessMask = 0,
        .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        .Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .QueueFamilyIndex = gRenderer->GraphicsQueue.FamilyIndex,
    };

    Rr_UnlockSpinlock(&gRenderer->Lock);

    if (gRenderer->GraphicsQueue.TimestampsEnabled)
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
            .pSignalSemaphores = &SwapchainImage->EarlySemaphore,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &Frame->LateCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &SwapchainImage->LateSemaphore,
            .waitSemaphoreCount = 2,
            .pWaitSemaphores =
                (VkSemaphore[]){
                    Frame->AcquireSemaphore,
                    SwapchainImage->EarlySemaphore,
                },
            .pWaitDstStageMask =
                (VkPipelineStageFlags[]){
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                },
        },
    };

    Rr_LockSpinlock(&gRenderer->GraphicsQueue.Lock);

    Device->QueueSubmit(
        gRenderer->GraphicsQueue.Handle,
        2,
        SubmitInfos,
        Frame->SubmitFence);

    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &SwapchainImage->LateSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    Result =
        Device->QueuePresentKHR(gRenderer->GraphicsQueue.Handle, &PresentInfo);

    Rr_UnlockSpinlock(&gRenderer->GraphicsQueue.Lock);

    Rr_RecreateSwapchain();

    Rr_DestroyScratch(Scratch);
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
    return gRenderer->TransferQueue.Handle != VK_NULL_HANDLE;
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
    return &Rr_GetCurrentFrame()->VirtualSwapchainImage;
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
        Scratch.Arena,
        VkAttachmentDescription,
        AttachmentCount);

    uint32_t ResolveDescriptionIndex = Key->ColorAttachmentCount;

    if (Key->ColorAttachmentCount > 0)
    {
        ColorReferences = RR_ALLOC_TYPE_COUNT(
            Scratch.Arena,
            VkAttachmentReference,
            Key->ColorAttachmentCount);

        ResolveReferences = RR_ALLOC_TYPE_COUNT(
            Scratch.Arena,
            VkAttachmentReference,
            Key->ColorAttachmentCount);

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
            RR_ALLOC_NO_ZERO(Scratch.Arena, sizeof(VkAttachmentReference));
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

static const Rr_SyncState RR_EMPTY_SYNC = {
    .StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    .QueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
};

Rr_SyncState *Rr_FindSyncState(
    Rr_SyncStateStorage *Storage,
    uint64_t VulkanHandle,
    Rr_Arena *Arena)
{
    Rr_SyncStateMap **MapRef = &Storage->Map;
    uint64_t Hash = VulkanHandle;
    for (; *MapRef; Hash <<= 2)
    {
        uint64_t Key = (*MapRef)->Key;
        if (VulkanHandle == Key || Key == (uint64_t)VK_NULL_HANDLE)
        {
            (*MapRef)->Key = VulkanHandle;
            return &(*MapRef)->Value;
        }
        MapRef = &(*MapRef)->Children[Hash >> 62];
    }
    assert(Arena);
    *MapRef = Rr_PushSyncStateMapIntoHive(&Storage->Hive, Arena).Element;
    RR_ZERO((*MapRef)->Children);
    (*MapRef)->Value = RR_EMPTY_SYNC;
    (*MapRef)->Key = (uint64_t)VulkanHandle;
    return &(*MapRef)->Value;
}

Rr_SyncState Rr_GetSyncState(
    Rr_SyncStateStorage *Storage,
    uint64_t VulkanHandle)
{
    Rr_SyncStateMap **MapRef = &Storage->Map;
    uint64_t Hash = VulkanHandle;
    for (; *MapRef; Hash <<= 2)
    {
        if (VulkanHandle == (*MapRef)->Key)
        {
            return (*MapRef)->Value;
        }
        MapRef = &(*MapRef)->Children[Hash >> 62];
    }
    return RR_EMPTY_SYNC;
}

void Rr_EraseSyncState(Rr_SyncStateStorage *Storage, uint64_t VulkanHandle)
{
    Rr_SyncStateMap **MapRef = &Storage->Map;
    uint64_t Hash = VulkanHandle;
    for (; *MapRef; Hash <<= 2)
    {
        if (VulkanHandle == (*MapRef)->Key)
        {
            (*MapRef)->Key = (uint64_t)VK_NULL_HANDLE;
            (*MapRef)->Value = RR_EMPTY_SYNC;
        }
        MapRef = &(*MapRef)->Children[Hash >> 62];
    }
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
    Rr_Device *Device = &gRenderer->Device;

    Rr_CommandPools *CommandPools = NULL;

    Rr_LockSpinlock(&gRenderer->CommandPoolsLock);

    if (gRenderer->CommandPools)
    {
        CommandPools = gRenderer->CommandPools;
        gRenderer->CommandPools = CommandPools->Next;

        Rr_UnlockSpinlock(&gRenderer->CommandPoolsLock);
    }
    else
    {
        Rr_UnlockSpinlock(&gRenderer->CommandPoolsLock);

        Rr_LockSpinlock(&gRenderer->Lock);
        CommandPools =
            RR_ALLOC_NO_ZERO(gRenderer->Arena, sizeof(Rr_CommandPools));
        Rr_UnlockSpinlock(&gRenderer->Lock);

        Device->CreateCommandPool(
            Device->Handle,
            &(VkCommandPoolCreateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = gRenderer->GraphicsQueue.FamilyIndex,
            },
            NULL,
            &CommandPools->Graphics);

        Device->CreateCommandPool(
            Device->Handle,
            &(VkCommandPoolCreateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = gRenderer->TransferQueue.FamilyIndex,
            },
            NULL,
            &CommandPools->Transfer);

        CommandPools->Compute = NULL;

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

    CommandPools->Next = NULL;

    return CommandPools;
}

void Rr_ReleaseCommandPools(Rr_CommandPools *CommandPools)
{
    Rr_LockSpinlock(&gRenderer->CommandPoolsLock);

    CommandPools->Next = gRenderer->CommandPools;
    gRenderer->CommandPools = CommandPools;

    Rr_UnlockSpinlock(&gRenderer->CommandPoolsLock);
}

static RR_THREAD_LOCAL char NextObjectName[32] = { 0 };

void Rr_SetNextObjectName(const char *Name)
{
    strncpy(NextObjectName, Name, sizeof(NextObjectName) - 1);
}

void Rr_ConsumeNextObjectName(char Dst[32])
{
    if (NextObjectName[0] != '\0')
    {
        for (uint32_t Index = 0; Index < 32; ++Index)
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
    RR_LOG(
        "Destroying %s: { name: \"%s\", address = %p }",
        Type,
        Name[0] != '\0' ? Name : "UNNAMED",
        (void *)Address);
}
