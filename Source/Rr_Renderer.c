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

#include "Rr_Renderer.h"

#include "Rr_App.h"
#include "Rr_BuiltinAssets.inc"
#include "Rr_Log.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>

#include <xxHash/xxhash.h>

#include <assert.h>

Rr_Renderer *gRenderer;

void Rr_MarkBufferUsed(Rr_Frame *Frame, Rr_Buffer *Buffer)
{
    atomic_fetch_add_explicit(&Buffer->RefCount, 1, memory_order_relaxed);
    *RR_PUSH_INTO_ARRAY(&Frame->UsedObjects.Buffers, Frame->Arena) = Buffer;
}

void Rr_MarkImageUsed(Rr_Frame *Frame, Rr_Image *Image)
{
    atomic_fetch_add_explicit(&Image->RefCount, 1, memory_order_relaxed);
    *RR_PUSH_INTO_ARRAY(&Frame->UsedObjects.Images, Frame->Arena) = Image;
}

void Rr_MarkSamplerUsed(Rr_Frame *Frame, Rr_Sampler *Sampler)
{
    atomic_fetch_add_explicit(&Sampler->RefCount, 1, memory_order_relaxed);
    *RR_PUSH_INTO_ARRAY(&Frame->UsedObjects.Samplers, Frame->Arena) = Sampler;
}

void Rr_MarkComputePipelineUsed(
    Rr_Frame *Frame,
    Rr_ComputePipeline *ComputePipeline)
{
    atomic_fetch_add_explicit(
        &ComputePipeline->RefCount,
        1,
        memory_order_relaxed);
    *RR_PUSH_INTO_ARRAY(&Frame->UsedObjects.ComputePipelines, Frame->Arena) =
        ComputePipeline;
}

void Rr_MarkGraphicsPipelineUsed(
    Rr_Frame *Frame,
    Rr_GraphicsPipeline *GraphicsPipeline)
{
    atomic_fetch_add_explicit(
        &GraphicsPipeline->RefCount,
        1,
        memory_order_relaxed);
    *RR_PUSH_INTO_ARRAY(&Frame->UsedObjects.GraphicsPipelines, Frame->Arena) =
        GraphicsPipeline;
}

static inline void Rr_DestroySwapchainImage(Rr_SwapchainImage *SwapchainImage)
{
    Rr_Device *Device = &gRenderer->Device;

    if (SwapchainImage->ViewStorage)
    {
        Rr_DestroyImageViewStorage(SwapchainImage->ViewStorage);
    }

    if (SwapchainImage->Handle)
    {
        Rr_ReturnSyncState((Rr_MapKey)SwapchainImage->Handle);
    }

    if (SwapchainImage->EarlySemaphore)
    {
        Rr_ReturnVulkanSemaphore(SwapchainImage->EarlySemaphore);
    }

    if (SwapchainImage->LateSemaphore)
    {
        Rr_ReturnVulkanSemaphore(SwapchainImage->LateSemaphore);
    }

    RR_ZERO_PTR(SwapchainImage);
}

void Rr_SetSwapchainDirty(bool Dirty)
{
    atomic_store_explicit(
        &gRenderer->Swapchain.RecreatePending,
        Dirty,
        memory_order_relaxed);
}

static bool Rr_InitSwapchain(void)
{
    Rr_Instance *Instance = &gRenderer->Instance;
    Rr_Device *Device = &gRenderer->Device;

    Rr_IntVec2 WindowSize = Rr_GetWindowSize();

    if (WindowSize.Width == 0 || WindowSize.Height == 0)
    {
        return false;
    }

    bool Recreate =
        gRenderer->Swapchain.Extent.width != (uint32_t)WindowSize.Width ||
        gRenderer->Swapchain.Extent.height != (uint32_t)WindowSize.Height ||
        atomic_load_explicit(
            &gRenderer->Swapchain.RecreatePending,
            memory_order_relaxed);

    if (!Recreate)
    {
        return true;
    }

    Rr_WaitIdle();

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
        gRenderer->Swapchain.Extent.width = WindowSize.Width;
        gRenderer->Swapchain.Extent.height = WindowSize.Height;
    }
    else
    {
        gRenderer->Swapchain.Extent.width =
            SurfaceCapabilities.currentExtent.width;
        gRenderer->Swapchain.Extent.height =
            SurfaceCapabilities.currentExtent.height;
    }

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

    bool PreferredFormatFound = false;
    for (uint32_t Index = 0; Index < FormatCount; Index++)
    {
        VkSurfaceFormatKHR *SurfaceFormat = &SurfaceFormats[Index];

        if (SurfaceFormat->format == VK_FORMAT_B8G8R8A8_UNORM ||
            SurfaceFormat->format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            gRenderer->Swapchain.Format = SurfaceFormat->format;
            gRenderer->Swapchain.ColorSpace = SurfaceFormat->colorSpace;
            PreferredFormatFound = true;
            break;
        }
    }

    if (!PreferredFormatFound)
    {
        RR_ABORT("No preferred surface format found!");
    }

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

        Rr_SyncState *SyncState = Rr_GetSyncState((Rr_MapKey)Image->Handle);
        SyncState->StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        Image->EarlySemaphore = Rr_GetVulkanSemaphore();
        Image->LateSemaphore = Rr_GetVulkanSemaphore();
    }

    Rr_SetSwapchainDirty(false);

    Rr_DestroyScratch(Scratch);

    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_SWAPCHAIN_CREATED;

    return true;
}

static void Rr_InitFrames(void)
{
    Rr_Device *Device = &gRenderer->Device;
    Rr_Frame *Frames = gRenderer->Frames;

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame *Frame = &Frames[Index];

        Frame->Arena = Rr_CreateDefaultArena();

        VkCommandPoolCreateInfo CommandPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = gRenderer->GraphicsQueue.FamilyIndex,
        };
        Device->CreateCommandPool(
            Device->Handle,
            &CommandPoolCreateInfo,
            NULL,
            &Frame->CommandPool);

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .commandPool = Frame->CommandPool,
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

        Rr_DescriptorPoolSizeRatio Ratios[] = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 32 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 32 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, 32 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32 },
        };
        Frame->DescriptorAllocator = Rr_CreateDescriptorAllocator(
            Device,
            1024,
            Ratios,
            RR_ARRAY_COUNT(Ratios));

        Frame->AcquireSemaphore = Rr_GetVulkanSemaphore();
    }
}

static void Rr_CleanupFrames(void)
{
    Rr_Device *Device = &gRenderer->Device;

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame *Frame = &gRenderer->Frames[Index];

        Device->DestroyCommandPool(Device->Handle, Frame->CommandPool, NULL);

        Rr_DestroyDescriptorAllocator(Frame->DescriptorAllocator, Device);

        Rr_ReturnVulkanFence(Frame->SubmitFence);
        Rr_ReturnVulkanSemaphore(Frame->AcquireSemaphore);

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
        .vkGetPhysicalDeviceMemoryProperties2KHR =
            Instance->GetPhysicalDeviceMemoryProperties2,
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

static void Rr_InitImmediateMode(void)
{
    Rr_Device *Device = &gRenderer->Device;
    Rr_ImmediateMode *ImmediateMode = &gRenderer->ImmediateMode;

    VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = gRenderer->GraphicsQueue.FamilyIndex,
    };
    Device->CreateCommandPool(
        Device->Handle,
        &CommandPoolInfo,
        NULL,
        &ImmediateMode->CommandPool);

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .commandPool = ImmediateMode->CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    Device->AllocateCommandBuffers(
        Device->Handle,
        &CommandBufferAllocateInfo,
        &ImmediateMode->CommandBuffer);
}

static void Rr_CleanupImmediateMode(void)
{
    Rr_Device *Device = &gRenderer->Device;

    Device->DestroyCommandPool(
        Device->Handle,
        gRenderer->ImmediateMode.CommandPool,
        NULL);
}

/* TODO: Move to queue initialization? */
static void Rr_InitTransientCommandPools(void)
{
    Rr_Device *Device = &gRenderer->Device;

    Device->CreateCommandPool(
        Device->Handle,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = gRenderer->GraphicsQueue.FamilyIndex,
        },
        NULL,
        &gRenderer->GraphicsQueue.TransientCommandPool);

    Device->CreateCommandPool(
        Device->Handle,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = gRenderer->TransferQueue.FamilyIndex,
        },
        NULL,
        &gRenderer->TransferQueue.TransientCommandPool);
}

static void Rr_CleanupTransientCommandPools(void)
{
    Rr_Device *Device = &gRenderer->Device;
    Device->DestroyCommandPool(
        Device->Handle,
        gRenderer->GraphicsQueue.TransientCommandPool,
        NULL);
    Device->DestroyCommandPool(
        Device->Handle,
        gRenderer->TransferQueue.TransientCommandPool,
        NULL);
}

void Rr_InitRenderer(void)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gRenderer = RR_ALLOC_TYPE(Arena, Rr_Renderer);
    gRenderer->Arena = Arena;

    Rr_InitLoader(&gRenderer->Loader);
    Rr_InitInstance(
        &gRenderer->Loader,
        gApp->Config->Title,
        &gRenderer->Instance);
    Rr_InitSurface(&gRenderer->Instance, &gRenderer->Surface);
    Rr_InitDeviceAndQueues(
        &gRenderer->Instance,
        gRenderer->Surface,
        &gRenderer->PhysicalDevice,
        &gRenderer->Device,
        &gRenderer->GraphicsQueue,
        &gRenderer->TransferQueue);

    Rr_InitVMA();
    Rr_InitTransientCommandPools();
    Rr_InitFrames();
    Rr_InitImmediateMode();
    Rr_InitSwapchain();

    Rr_DestroyScratch(Scratch);
}

void Rr_WaitIdle(void)
{
    Rr_Device *Device = &gRenderer->Device;
    Device->DeviceWaitIdle(Device->Handle);
}

static void Rr_DecrementRefCounts(Rr_Frame *Frame)
{
    for (size_t Index = 0; Index < Frame->UsedObjects.Buffers.Count; ++Index)
    {
        atomic_fetch_sub_explicit(
            &Frame->UsedObjects.Buffers.Data[Index]->RefCount,
            1,
            memory_order_relaxed);
    }

    for (size_t Index = 0; Index < Frame->UsedObjects.Images.Count; ++Index)
    {
        atomic_fetch_sub_explicit(
            &Frame->UsedObjects.Images.Data[Index]->RefCount,
            1,
            memory_order_relaxed);
    }

    for (size_t Index = 0; Index < Frame->UsedObjects.Samplers.Count; ++Index)
    {
        atomic_fetch_sub_explicit(
            &Frame->UsedObjects.Samplers.Data[Index]->RefCount,
            1,
            memory_order_relaxed);
    }

    for (size_t Index = 0; Index < Frame->UsedObjects.ComputePipelines.Count;
         ++Index)
    {
        atomic_fetch_sub_explicit(
            &Frame->UsedObjects.ComputePipelines.Data[Index]->RefCount,
            1,
            memory_order_relaxed);
    }

    for (size_t Index = 0; Index < Frame->UsedObjects.GraphicsPipelines.Count;
         ++Index)
    {
        atomic_fetch_sub_explicit(
            &Frame->UsedObjects.GraphicsPipelines.Data[Index]->RefCount,
            1,
            memory_order_relaxed);
    }
}

static inline void Rr_ProcessReleasedObjects(void)
{
    static const char *RENDERER_OBJECT_NAMES[] = {
        "buffer",
        "image",
        "pipeline layout",
        "compute pipeline",
        "graphics pipeline",
        "sampler",
    };

    for (Rr_RendererObjectHiveIterator It = gRenderer->ReleasedObjects.Begin;
         It.Element != gRenderer->ReleasedObjects.End.Element;)
    {
        Rr_RendererObject Object = *It.Element;
        switch (Object.Type)
        {
            case RR_RENDERER_OBJECT_TYPE_BUFFER:
            {
                Rr_Buffer *Buffer = Object.Ptr;

                if (!atomic_load_explicit(
                        &Buffer->RefCount,
                        memory_order_relaxed))
                {
                    Rr_DestroyBuffer(Buffer);
                    goto ObjectDeleted;
                }
            }
            break;
            case RR_RENDERER_OBJECT_TYPE_IMAGE:
            {
                Rr_Image *Image = Object.Ptr;
                if (!atomic_load_explicit(
                        &Image->RefCount,
                        memory_order_relaxed))
                {
                    Rr_DestroyImage(Image);
                    goto ObjectDeleted;
                }
            }
            break;
            case RR_RENDERER_OBJECT_TYPE_SAMPLER:
            {
                Rr_Sampler *Sampler = Object.Ptr;
                if (!atomic_load_explicit(
                        &Sampler->RefCount,
                        memory_order_relaxed))
                {
                    Rr_DestroySampler(Sampler);
                    goto ObjectDeleted;
                }
            }
            break;
            case RR_RENDERER_OBJECT_TYPE_PIPELINE_LAYOUT:
            {
                Rr_PipelineLayout *PipelineLayout = Object.Ptr;
                if (!atomic_load_explicit(
                        &PipelineLayout->RefCount,
                        memory_order_relaxed))
                {
                    Rr_DestroyPipelineLayout(PipelineLayout);
                    goto ObjectDeleted;
                }
            }
            break;
            case RR_RENDERER_OBJECT_TYPE_COMPUTE_PIPELINE:
            {
                Rr_ComputePipeline *ComputePipeline = Object.Ptr;
                if (!atomic_load_explicit(
                        &ComputePipeline->RefCount,
                        memory_order_relaxed))
                {
                    Rr_DestroyComputePipeline(ComputePipeline);
                    goto ObjectDeleted;
                }
            }
            break;
            case RR_RENDERER_OBJECT_TYPE_GRAPHICS_PIPELINE:
            {
                Rr_GraphicsPipeline *GraphicsPipeline = Object.Ptr;
                if (!atomic_load_explicit(
                        &GraphicsPipeline->RefCount,
                        memory_order_relaxed))
                {
                    Rr_DestroyGraphicsPipeline(GraphicsPipeline);
                    goto ObjectDeleted;
                }
            }
            break;
            default:
                RR_ABORT("Invalid renderer object type in deletion queue!");
        }
        Rr_AdvanceRendererObjectHiveIterator(&It);
        continue;
    ObjectDeleted:
        RR_LOG(
            "Destroying %s with address %p",
            RENDERER_OBJECT_NAMES[Object.Type],
            Object.Ptr);
        Rr_RemoveFromRendererObjectHive(&gRenderer->ReleasedObjects, &It);
    }
}

static void Rr_ProcessPendingLoads(void)
{
    for (size_t Index = 0; Index < gRenderer->PendingLoads.Count; ++Index)
    {
        Rr_PendingLoad *PendingLoad = &gRenderer->PendingLoads.Data[Index];
        PendingLoad->LoadingCallback(PendingLoad->UserData);
    }
    RR_CLEAR_ARRAY(&gRenderer->PendingLoads);
}

void Rr_CleanupRenderer(void)
{
    Rr_Instance *Instance = &gRenderer->Instance;
    Rr_Device *Device = &gRenderer->Device;

    Rr_WaitIdle();

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_DecrementRefCounts(&gRenderer->Frames[Index]);
    }

    while (gRenderer->ReleasedObjects.Count > 0)
    {
        Rr_ProcessReleasedObjects();
    }

    for (size_t Index = 0; Index < gRenderer->RenderPasses.Count; ++Index)
    {
        Device->DestroyRenderPass(
            Device->Handle,
            gRenderer->RenderPasses.Data[Index].Handle,
            NULL);
    }

    /* for (size_t Index = 0; Index < gRenderer->Framebuffers.Count; ++Index) */
    /* { */
    /*     Device->DestroyFramebuffer( */
    /*         Device->Handle, */
    /*         gRenderer->Framebuffers.Data[Index].Handle, */
    /*         NULL); */
    /* } */

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

    Rr_CleanupTransientCommandPools();
    Rr_CleanupImmediateMode();

    for (size_t Index = 0; Index < gRenderer->DescriptorSetLayouts.Count;
         ++Index)
    {
        Rr_DescriptorSetLayout *DescriptorSetLayout =
            gRenderer->DescriptorSetLayouts.Data + Index;
        Device->DestroyDescriptorSetLayout(
            Device->Handle,
            DescriptorSetLayout->Handle,
            NULL);
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

VkCommandBuffer Rr_BeginImmediate(void)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_ImmediateMode *ImmediateMode = &gRenderer->ImmediateMode;
    Device->ResetCommandBuffer(ImmediateMode->CommandBuffer, 0);

    VkCommandBufferBeginInfo BeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    Device->BeginCommandBuffer(ImmediateMode->CommandBuffer, &BeginInfo);

    return ImmediateMode->CommandBuffer;
}

void Rr_EndImmediate(void)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_ImmediateMode *ImmediateMode = &gRenderer->ImmediateMode;

    Device->EndCommandBuffer(ImmediateMode->CommandBuffer);

    VkFence Fence = Rr_GetVulkanFence();

    VkSubmitInfo SubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ImmediateMode->CommandBuffer,
    };

    Device->QueueSubmit(gRenderer->GraphicsQueue.Handle, 1, &SubmitInfo, Fence);
    Device->WaitForFences(Device->Handle, 1, &Fence, true, UINT64_MAX);

    Rr_ReturnVulkanFence(Fence);
}

void Rr_NewFrame(void)
{
    Rr_Device *Device = &gRenderer->Device;

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

        Rr_ReturnVulkanFence(Frame->SubmitFence);
        Frame->SubmitFence = VK_NULL_HANDLE;

        Rr_ResetDescriptorAllocator(Frame->DescriptorAllocator, Device);
    }

    Rr_DecrementRefCounts(Frame);

    /* TODO: Probably should use mutex instead. */

    if (Rr_TryLockSpinlock(&gRenderer->Lock))
    {
        Rr_ProcessPendingLoads();
        Rr_ProcessReleasedObjects();

        Rr_UnlockSpinlock(&gRenderer->Lock);
    }

    /* Reset everything allocated last time. */

    Rr_ResetArena(Frame->Arena);

    RR_RESET_ARRAY(&Frame->UsedObjects.Buffers, Frame->Arena);
    RR_RESET_ARRAY(&Frame->UsedObjects.Images, Frame->Arena);
    RR_RESET_ARRAY(&Frame->UsedObjects.Samplers, Frame->Arena);
    RR_RESET_ARRAY(&Frame->UsedObjects.ComputePipelines, Frame->Arena);
    RR_RESET_ARRAY(&Frame->UsedObjects.GraphicsPipelines, Frame->Arena);

    Frame->VirtualSwapchainImage = RR_ALLOC_TYPE(Frame->Arena, Rr_Image2D);

    /* These are applied again just before graph execution. */

    Frame->VirtualSwapchainImage->Extent = gRenderer->Swapchain.Extent;
    Frame->VirtualSwapchainImage->Format = gRenderer->Swapchain.Format;
    Frame->VirtualSwapchainImage->AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    Frame->Graph = RR_ALLOC_TYPE(Frame->Arena, Rr_Graph);
    Frame->Graph->Frame = Frame;
    Frame->Graph->SwapchainImageResourceIndex =
        Rr_GetGraphImageHandle(
            Frame->Graph,
            (Rr_Image *)Frame->VirtualSwapchainImage)
            ->Values.Index;
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
        if (Rr_InitSwapchain() == false)
        {
            return;
        }
    }

    Frame->SubmitFence = Rr_GetVulkanFence();

    Rr_SwapchainImage *SwapchainImage =
        &gRenderer->SwapchainImages.Data[SwapchainImageIndex];
    VkImage SwapchainImageHandle = SwapchainImage->Handle;

    /* Now that we acquired swapchain image index we can
     * put real handles to virtual swapchain image which
     * will be used by the graph. */

    *Frame->VirtualSwapchainImage = (Rr_Image2D){
        .Extent = gRenderer->Swapchain.Extent,
        .Format = gRenderer->Swapchain.Format,
        .AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
        .AllocatedImageCount = 1,
        .AllocatedImages[0] = {
            .ViewStorage = gRenderer->SwapchainImages.Data[SwapchainImageIndex].ViewStorage,
            .Handle = SwapchainImageHandle,
            .Container = (Rr_Image *)Frame->VirtualSwapchainImage,
        },
    };

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

    Rr_ExecuteGraph(Frame->Graph, Scratch.Arena);

    Device->EndCommandBuffer(Frame->EarlyCommandBuffer);

    /* Always transition swapchain image to present layout. */

    Rr_SyncState *SwapchainImageSyncState =
        Rr_GetSyncState((Rr_MapKey)SwapchainImageHandle);
    Device->CmdPipelineBarrier(
        Frame->LateCommandBuffer,
        SwapchainImageSyncState->StageMask,
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
            .oldLayout = SwapchainImageSyncState->Layout,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcAccessMask = SwapchainImageSyncState->AccessMask,
            .dstAccessMask = 0,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        });
    SwapchainImageSyncState->AccessMask = 0;
    SwapchainImageSyncState->StageMask =
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; /* Doesn't look right. */
    SwapchainImageSyncState->Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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

    Rr_InitSwapchain();

    gRenderer->FrameNumber++;
    gRenderer->FrameIndex = gRenderer->FrameNumber % RR_FRAME_OVERLAP;

    Rr_DestroyScratch(Scratch);
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
    return gRenderer->PhysicalDevice.Properties.properties.limits
        .minUniformBufferOffsetAlignment;
}

size_t Rr_GetStorageAlignment(void)
{
    return gRenderer->PhysicalDevice.Properties.properties.limits
        .minStorageBufferOffsetAlignment;
}

size_t Rr_GetMaxComputeSharedMemorySize(void)
{
    return gRenderer->PhysicalDevice.Properties.properties.limits
        .maxComputeSharedMemorySize;
}

size_t Rr_GetMaxComputeWorkgroupInvocations(void)
{
    return gRenderer->PhysicalDevice.Properties.properties.limits
        .maxComputeWorkGroupInvocations;
}

Rr_Graph *Rr_GetGraph(void)
{
    return Rr_GetCurrentFrame()->Graph;
}

Rr_Arena *Rr_GetFrameArena(void)
{
    return Rr_GetCurrentFrame()->Arena;
}

Rr_TextureFormat Rr_GetSwapchainFormat(void)
{
    return Rr_ToTextureFormat(gRenderer->Swapchain.Format);
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
    return Rr_GetCurrentFrame()->VirtualSwapchainImage;
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

VkRenderPass Rr_GetVulkanRenderPass(Rr_RenderPassInfo *Info)
{
    assert(Info != NULL);

    uint32_t Hash = XXH32(
        Info->Attachments,
        sizeof(Rr_RenderPassAttachment) * Info->AttachmentCount,
        0);

    for (size_t Index = 0; Index < gRenderer->RenderPasses.Count; ++Index)
    {
        if (gRenderer->RenderPasses.Data[Index].Hash == Hash)
        {
            return gRenderer->RenderPasses.Data[Index].Handle;
        }
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkAttachmentDescription *Attachments = RR_ALLOC_TYPE_COUNT(
        Scratch.Arena,
        VkAttachmentDescription,
        Info->AttachmentCount);

    size_t ColorCount = 0;
    VkAttachmentReference *ColorReferences = RR_ALLOC_TYPE_COUNT(
        Scratch.Arena,
        VkAttachmentReference,
        Info->AttachmentCount);
    VkAttachmentReference *DepthReference = NULL;

    for (uint32_t Index = 0; Index < Info->AttachmentCount; ++Index)
    {
        Rr_RenderPassAttachment *Attachment = &Info->Attachments[Index];
        if (Rr_IsVulkanDepthFormat(Attachment->Format))
        {
            if (DepthReference != NULL)
            {
                RR_ABORT("Can't have more than one depth attachment!");
            }
            DepthReference =
                RR_ALLOC(Scratch.Arena, sizeof(VkAttachmentDescription));
            DepthReference->attachment = Index;
            DepthReference->layout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            Attachments[Index] = (VkAttachmentDescription){
                .samples = 1,
                .format = Attachment->Format,
                .initialLayout =
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .flags = 0,
                .loadOp = Rr_ToVulkanLoadOp(Attachment->LoadOp),
                .storeOp = Rr_ToVulkanStoreOp(Attachment->StoreOp),
            };
        }
        else
        {
            ColorCount++;
            ColorReferences[Index] = (VkAttachmentReference){
                .attachment = Index,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            Attachments[Index] = (VkAttachmentDescription){
                .samples = 1,
                .format = Attachment->Format,
                .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .flags = 0,
                .loadOp = Rr_ToVulkanLoadOp(Attachment->LoadOp),
                .storeOp = Rr_ToVulkanStoreOp(Attachment->StoreOp),
            };
        }
    }

    VkSubpassDescription SubpassDescription = {
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = (uint32_t)ColorCount,
        .pColorAttachments = ColorReferences,
        .pDepthStencilAttachment = DepthReference,
        .pResolveAttachments = NULL,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    VkRenderPassCreateInfo RenderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .attachmentCount = (uint32_t)Info->AttachmentCount,
        .pAttachments = Attachments,
        .subpassCount = 1,
        .pSubpasses = &SubpassDescription,
        .dependencyCount = 0,
        .pDependencies = NULL,
    };

    VkRenderPass RenderPass = VK_NULL_HANDLE;

    Rr_Device *Device = &gRenderer->Device;

    Device->CreateRenderPass(
        Device->Handle,
        &RenderPassCreateInfo,
        NULL,
        &RenderPass);

    Rr_LockSpinlock(&gRenderer->Lock);

    *RR_PUSH_INTO_ARRAY(&gRenderer->RenderPasses, gRenderer->Arena) =
        (Rr_RenderPass){
            .Handle = RenderPass,
            .Hash = Hash,
        };

    Rr_UnlockSpinlock(&gRenderer->Lock);

    Rr_DestroyScratch(Scratch);

    return RenderPass;
}

void Rr_DestroyVulkanFramebuffers(VkImageView ImageView)
{
    Rr_Device *Device = &gRenderer->Device;

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
}

VkFramebuffer Rr_GetVulkanFramebuffer(
    VkRenderPass RenderPass,
    Rr_FramebufferMapKey *Key)
{
    VkFramebuffer *FramebufferRef = NULL;

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_FramebufferMap **Map = &gRenderer->FramebufferStorage.Map;
    for (uint64_t Hash = XXH64(Key, sizeof(*Key), 0); *Map; Hash <<= 2)
    {
        if ((*Map)->Value == VK_NULL_HANDLE)
        {
            (*Map)->Key = *Key;
            FramebufferRef = &(*Map)->Value;

            goto Found;
        }
        else if (memcmp(Key, &(*Map)->Key, sizeof(*Key)) == 0)
        {
            FramebufferRef = &(*Map)->Value;

            goto Found;
        }
        Map = &(*Map)->Children[Hash >> 62];
    }
    *Map = Rr_PushFramebufferMapIntoHive(
               &gRenderer->FramebufferStorage.Hive,
               gRenderer->Arena)
               .Element;
    (*Map)->Key = *Key;
    (*Map)->Value = VK_NULL_HANDLE;
    RR_ZERO_PTR((*Map)->Children);
    FramebufferRef = &(*Map)->Value;

Found:

    Rr_UnlockSpinlock(&gRenderer->Lock);

    if (*FramebufferRef != VK_NULL_HANDLE)
    {
        return *FramebufferRef;
    }

    uint32_t AttachmentCount = Key->ColorAttachmentCount +
                               Key->ResolveAttachmentCount +
                               (uint32_t)Key->DepthStencil;

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

Rr_SyncState *Rr_GetSyncState(Rr_MapKey Key)
{
    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_SyncState **SyncStateRef =
        RR_GET_MAP_VALUE(&gRenderer->GlobalSync, Key, gRenderer->Arena);

    if (*SyncStateRef != NULL)
    {
        Rr_UnlockSpinlock(&gRenderer->Lock);

        return *SyncStateRef;
    }

    *SyncStateRef =
        RR_GET_FREE_LIST_ITEM(&gRenderer->SyncStates, gRenderer->Arena);
    Rr_SyncState *SyncState = *SyncStateRef;
    RR_ZERO_PTR(SyncState);

    Rr_UnlockSpinlock(&gRenderer->Lock);

    return SyncState;
}

void Rr_ReturnSyncState(Rr_MapKey Key)
{
    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_SyncState **SyncStateRef =
        RR_GET_MAP_VALUE(&gRenderer->GlobalSync, Key, gRenderer->Arena);

    if (*SyncStateRef != NULL)
    {
        RR_RETURN_FREE_LIST_ITEM(&gRenderer->SyncStates, *SyncStateRef);
    }

    Rr_UnlockSpinlock(&gRenderer->Lock);

    *SyncStateRef = NULL;
}

VkSemaphore Rr_GetVulkanSemaphore(void)
{
    VkSemaphore Semaphore;

    bool Locked = Rr_TryLockSpinlock(&gRenderer->Lock);

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
        Rr_UnlockSpinlock(&gRenderer->Lock);
    }

    return Semaphore;
}

void Rr_ReturnVulkanSemaphore(VkSemaphore Semaphore)
{
    Rr_LockSpinlock(&gRenderer->Lock);

    *RR_PUSH_INTO_ARRAY(&gRenderer->Semaphores, gRenderer->Arena) = Semaphore;

    Rr_UnlockSpinlock(&gRenderer->Lock);
}

VkFence Rr_GetVulkanFence(void)
{
    VkFence Fence;

    bool Locked = Rr_TryLockSpinlock(&gRenderer->Lock);

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
        Rr_UnlockSpinlock(&gRenderer->Lock);
    }

    return Fence;
}

void Rr_ReturnVulkanFence(VkFence Fence)
{
    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_Device *Device = &gRenderer->Device;
    *RR_PUSH_INTO_ARRAY(&gRenderer->Fences, gRenderer->Arena) = Fence;

    Rr_UnlockSpinlock(&gRenderer->Lock);

    Device->ResetFences(Device->Handle, 1, &Fence);
}
