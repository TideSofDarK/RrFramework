#include "Rr_Renderer.h"

#include "Rr_App.h"
#include "Rr_BuiltinAssets.inc"
#include "Rr_Image.h"
#include "Rr_Log.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <xxHash/xxhash.h>

#include <assert.h>

static inline void Rr_DestroySwapchainImage(
    Rr_Renderer *Renderer,
    Rr_SwapchainImage *SwapchainImage)
{
    Rr_Device *Device = &Renderer->Device;

    if(SwapchainImage->Framebuffer)
    {
        Device->DestroyFramebuffer(
            Device->Handle,
            SwapchainImage->Framebuffer,
            NULL);
    }

    if(SwapchainImage->View)
    {
        Device->DestroyImageView(Device->Handle, SwapchainImage->View, NULL);
    }

    if(SwapchainImage->Handle)
    {
        Rr_ReturnSyncState(Renderer, (Rr_MapKey)SwapchainImage->Handle);
    }

    RR_ZERO_PTR(SwapchainImage);
}

static void Rr_CleanupSwapchainData(
    Rr_Renderer *Renderer,
    Rr_SwapchainCleanupData *SwapchainCleanupData)
{
    Rr_Device *Device = &Renderer->Device;

    if(SwapchainCleanupData->Handle != VK_NULL_HANDLE)
    {
        Device->DestroySwapchainKHR(
            Renderer->Device.Handle,
            SwapchainCleanupData->Handle,
            NULL);
    }

    for(size_t Index = 0; Index < SwapchainCleanupData->Semaphores.Count;
        ++Index)
    {
        Rr_ReturnVulkanSemaphore(
            Renderer,
            SwapchainCleanupData->Semaphores.Data[Index]);
    }

    RR_ZERO_PTR(SwapchainCleanupData);
}

static bool Rr_CheckSwapchainDirty(Rr_App *App)
{
    Rr_Renderer *Renderer = App->Renderer;
    bool StillDirty = Rr_GetAtomicInt(&Renderer->Swapchain.RecreatePending);
    if(StillDirty)
    {
        StillDirty = !Rr_RecreateSwapchain(App);
        Rr_SetSwapchainDirty(Renderer, StillDirty);
    }
    return StillDirty;
}

void Rr_SetSwapchainDirty(Rr_Renderer *Renderer, bool Dirty)
{
    Rr_SetAtomicInt(&Renderer->Swapchain.RecreatePending, Dirty);
}

void Rr_ScheduleOldSwapchainForDestruction(
    Rr_Renderer *Renderer,
    VkSwapchainKHR Handle)
{
    Rr_Device *Device = &Renderer->Device;

    if(Renderer->PresentHistory.Count > 0 &&
       Renderer->PresentHistory.Data[Renderer->PresentHistory.Count - 1]
               .ImageIndex == UINT32_MAX)
    {
        Device->DestroySwapchainKHR(Renderer->Device.Handle, Handle, NULL);
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_SwapchainCleanupData Cleanup = { 0 };
    Cleanup.Handle = Handle;

    RR_SLICE(Rr_PresentInfo) HistoryToKeep = { 0 };
    while(Renderer->PresentHistory.Count)
    {
        Rr_PresentInfo *PresentInfo = Renderer->PresentHistory.Data;

        if(PresentInfo->ImageIndex == UINT32_MAX)
        {
            assert(PresentInfo->CleanupFence != VK_NULL_HANDLE);
            break;
        }

        PresentInfo->ImageIndex = UINT32_MAX;

        if(PresentInfo->CleanupFence != VK_NULL_HANDLE)
        {
            *RR_PUSH_SLICE(&HistoryToKeep, Scratch.Arena) = *PresentInfo;
        }
        else
        {
            assert(PresentInfo->EarlySemaphore != VK_NULL_HANDLE);
            assert(PresentInfo->LateSemaphore != VK_NULL_HANDLE);

            *RR_PUSH_SLICE(&Cleanup.Semaphores, Scratch.Arena) =
                PresentInfo->EarlySemaphore;
            *RR_PUSH_SLICE(&Cleanup.Semaphores, Scratch.Arena) =
                PresentInfo->LateSemaphore;

            for(size_t Index = 0; Index < PresentInfo->OldSwapchains.Count;
                ++Index)
            {
                *RR_PUSH_SLICE(&Renderer->OldSwapchains, Renderer->Arena) =
                    PresentInfo->OldSwapchains.Data[Index];
            }
            RR_EMPTY_SLICE(&PresentInfo->OldSwapchains);
        }

        if(Renderer->PresentHistory.Count > 1)
        {
            memmove(
                Renderer->PresentHistory.Data,
                Renderer->PresentHistory.Data + 1,
                sizeof(Rr_PresentInfo) * (Renderer->PresentHistory.Count - 1));
        }
        Renderer->PresentHistory.Count--;
    }

    for(size_t Index = 0; Index < HistoryToKeep.Count; ++Index)
    {
        *RR_PUSH_SLICE(&Renderer->PresentHistory, Renderer->Arena) =
            HistoryToKeep.Data[Index];
    }

    if(Cleanup.Handle != VK_NULL_HANDLE || Cleanup.Semaphores.Count > 0)
    {
        *RR_PUSH_SLICE(&Renderer->OldSwapchains, Renderer->Arena) = Cleanup;
    }

    Rr_DestroyScratch(Scratch);
}

static bool Rr_InitSwapchain(
    Rr_Renderer *Renderer,
    uint32_t Width,
    uint32_t Height)
{
    Rr_Instance *Instance = &Renderer->Instance;
    Rr_Device *Device = &Renderer->Device;

    VkResult Result;
    (void)Result;

    VkSwapchainKHR OldSwapchain = Renderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfaceCapabilities;
    Instance->GetPhysicalDeviceSurfaceCapabilitiesKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &SurfaceCapabilities);

    if(SurfaceCapabilities.currentExtent.width == 0 ||
       SurfaceCapabilities.currentExtent.height == 0)
    {
        return false;
    }
    if(SurfaceCapabilities.currentExtent.width == UINT32_MAX)
    {
        Renderer->Swapchain.Extent.width = Width;
        Renderer->Swapchain.Extent.height = Height;
    }
    else
    {
        Renderer->Swapchain.Extent.width =
            SurfaceCapabilities.currentExtent.width;
        Renderer->Swapchain.Extent.height =
            SurfaceCapabilities.currentExtent.height;
        Width = SurfaceCapabilities.currentExtent.width;
        Height = SurfaceCapabilities.currentExtent.height;
    }
    Renderer->Swapchain.Extent.depth = 1;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    uint32_t PresentModeCount;
    Instance->GetPhysicalDeviceSurfacePresentModesKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &PresentModeCount,
        NULL);
    assert(PresentModeCount > 0);

    VkPresentModeKHR *PresentModes =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkPresentModeKHR, PresentModeCount);
    Instance->GetPhysicalDeviceSurfacePresentModesKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &PresentModeCount,
        PresentModes);

    VkPresentModeKHR SwapchainPresentMode;
    switch(Renderer->Swapchain.PresentMode)
    {
        case RR_PRESENT_MODE_FIFO_RELAXED:
            SwapchainPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            break;
        case RR_PRESENT_MODE_IMMEDIATE:
            SwapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        case RR_PRESENT_MODE_MAILBOX:
            SwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        default:
            SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
            break;
    }
    bool PresentModeAvailable = false;
    for(uint32_t Index = 0; Index < PresentModeCount; Index++)
    {
        if(PresentModes[Index] == SwapchainPresentMode)
        {
            PresentModeAvailable = true;
            break;
        }
    }
    if(PresentModeAvailable == false)
    {
        SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        Renderer->Swapchain.PresentMode = RR_PRESENT_MODE_FIFO;
    }

    uint32_t DesiredNumberOfSwapchainImages =
        RR_MAX(SurfaceCapabilities.minImageCount, 3);
    if(SurfaceCapabilities.maxImageCount > 0)
    {
        DesiredNumberOfSwapchainImages = RR_MIN(
            DesiredNumberOfSwapchainImages,
            SurfaceCapabilities.maxImageCount);
    }

    VkSurfaceTransformFlagBitsKHR PreTransform;
    if(SurfaceCapabilities.supportedTransforms &
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
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &FormatCount,
        NULL);
    assert(FormatCount > 0);

    VkSurfaceFormatKHR *SurfaceFormats =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkSurfaceFormatKHR, FormatCount);
    Instance->GetPhysicalDeviceSurfaceFormatsKHR(
        Renderer->PhysicalDevice.Handle,
        Renderer->Surface,
        &FormatCount,
        SurfaceFormats);

    bool PreferredFormatFound = false;
    for(uint32_t Index = 0; Index < FormatCount; Index++)
    {
        VkSurfaceFormatKHR *SurfaceFormat = &SurfaceFormats[Index];

        if(SurfaceFormat->format == VK_FORMAT_B8G8R8A8_UNORM ||
           SurfaceFormat->format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            Renderer->Swapchain.Format = SurfaceFormat->format;
            Renderer->Swapchain.ColorSpace = SurfaceFormat->colorSpace;
            PreferredFormatFound = true;
            break;
        }
    }

    if(!PreferredFormatFound)
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
    for(uint32_t Index = 0; Index < RR_ARRAY_COUNT(CompositeAlphaFlags);
        Index++)
    {
        VkCompositeAlphaFlagBitsKHR CompositeAlphaFlag =
            CompositeAlphaFlags[Index];
        if(SurfaceCapabilities.supportedCompositeAlpha & CompositeAlphaFlag)
        {
            CompositeAlpha = CompositeAlphaFlag;
            break;
        }
    }

    VkSwapchainCreateInfoKHR SwapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = Renderer->Surface,
        .minImageCount = DesiredNumberOfSwapchainImages,
        .imageFormat = Renderer->Swapchain.Format,
        .imageColorSpace = Renderer->Swapchain.ColorSpace,
        .imageExtent = { Renderer->Swapchain.Extent.width,
                         Renderer->Swapchain.Extent.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = PreTransform,
        .compositeAlpha = CompositeAlpha,
        .presentMode = SwapchainPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = OldSwapchain,
    };

    if(SurfaceCapabilities.supportedUsageFlags &
       VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        SwapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if(SurfaceCapabilities.supportedUsageFlags &
       VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        SwapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    Result = Device->CreateSwapchainKHR(
        Renderer->Device.Handle,
        &SwapchainCreateInfo,
        NULL,
        &Renderer->Swapchain.Handle);

    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    if(Renderer->SwapchainImages.Count > 0)
    {
        for(size_t Index = 0; Index < Renderer->SwapchainImages.Count; ++Index)
        {
            *RR_PUSH_SLICE(&Frame->SwapchainGarbage, Renderer->Arena) =
                Renderer->SwapchainImages.Data[Index];
        }
        RR_EMPTY_SLICE(&Renderer->SwapchainImages);
    }
    if(OldSwapchain != VK_NULL_HANDLE)
    {
        Rr_ScheduleOldSwapchainForDestruction(Renderer, OldSwapchain);
    }

    /* Acquire swapchain images. */

    uint32_t ImageCount = 0;
    Device->GetSwapchainImagesKHR(
        Renderer->Device.Handle,
        Renderer->Swapchain.Handle,
        &ImageCount,
        NULL);

    VkImage *Images = RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkImage, ImageCount);

    Device->GetSwapchainImagesKHR(
        Renderer->Device.Handle,
        Renderer->Swapchain.Handle,
        &ImageCount,
        Images);

    /* Initialize present pipeline if needed. */

    Rr_RenderPassAttachment Attachment = {
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Format = Renderer->Swapchain.Format,
    };
    Rr_RenderPassInfo RenderPassInfo = {
        .AttachmentCount = 1,
        .Attachments = &Attachment,
    };
    VkRenderPass RenderPass = Rr_GetVulkanRenderPass(Renderer, &RenderPassInfo);

    /* Create framebuffers and image views. */

    RR_RESERVE_SLICE(&Renderer->SwapchainImages, ImageCount, Renderer->Arena);
    Renderer->SwapchainImages.Count = ImageCount;

    VkImageViewCreateInfo ImageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = Renderer->Swapchain.Format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkFramebufferCreateInfo FramebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .attachmentCount = 1,
        .width = Width,
        .height = Height,
        .layers = 1,
        .renderPass = RenderPass,
    };

    for(uint32_t Index = 0; Index < ImageCount; Index++)
    {
        Rr_SwapchainImage *Image = Renderer->SwapchainImages.Data + Index;

        Image->Handle = Images[Index];

        ImageViewCreateInfo.image = Image->Handle;
        Device->CreateImageView(
            Renderer->Device.Handle,
            &ImageViewCreateInfo,
            NULL,
            &Image->View);

        FramebufferCreateInfo.pAttachments = &Image->View;
        Device->CreateFramebuffer(
            Renderer->Device.Handle,
            &FramebufferCreateInfo,
            NULL,
            &Image->Framebuffer);

        Rr_SyncState *SyncState =
            Rr_GetSyncState(Renderer, (Rr_MapKey)Image->Handle);
        SyncState->StageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    Rr_DestroyScratch(Scratch);

    return true;
}

bool Rr_RecreateSwapchain(Rr_App *App)
{
    Rr_Renderer *Renderer = App->Renderer;

    int32_t Width, Height;
    SDL_GetWindowSizeInPixels(App->Window, &Width, &Height);

    return Rr_InitSwapchain(Renderer, Width, Height);
}

static void Rr_AssociateFenceWithPresentHistory(
    Rr_Renderer *Renderer,
    uint32_t SwapchainImageIndex,
    VkFence AcquireFence)
{
    for(size_t Index = 0; Index < Renderer->PresentHistory.Count; ++Index)
    {
        size_t Reverse = Renderer->PresentHistory.Count - (1 + Index);
        Rr_PresentInfo *PresentInfo = Renderer->PresentHistory.Data + Reverse;

        if(PresentInfo->ImageIndex == UINT32_MAX)
        {
            break;
        }

        if(PresentInfo->ImageIndex == SwapchainImageIndex)
        {
            assert(PresentInfo->CleanupFence == VK_NULL_HANDLE);
            PresentInfo->CleanupFence = AcquireFence;
            return;
        }
    }

    *RR_PUSH_SLICE(&Renderer->PresentHistory, Renderer->Arena) =
        (Rr_PresentInfo){
            .CleanupFence = AcquireFence,
            .ImageIndex = SwapchainImageIndex,
        };
}

static VkResult Rr_AcquireNextImage(
    Rr_Renderer *Renderer,
    uint32_t *SwapchainImageIndex)
{
    Rr_Device *Device = &Renderer->Device;
    Rr_Swapchain *Swapchain = &Renderer->Swapchain;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    VkFence AcquireFence = Rr_GetVulkanFence(Renderer);

    VkResult Result = Device->AcquireNextImageKHR(
        Device->Handle,
        Swapchain->Handle,
        1000000000,
        Frame->AcquireSemaphore,
        AcquireFence,
        SwapchainImageIndex);

    assert(Result != VK_TIMEOUT && "Swapchain image timeout!");

    if(Result != VK_SUCCESS && Result != VK_SUBOPTIMAL_KHR)
    {
        Rr_ReturnVulkanFence(Renderer, AcquireFence);

        return Result;
    }

    Rr_AssociateFenceWithPresentHistory(
        Renderer,
        *SwapchainImageIndex,
        AcquireFence);

    return Result;
}

static void Rr_CleanupPresentInfo(
    Rr_Renderer *Renderer,
    Rr_PresentInfo *PresentInfo)
{
    if(PresentInfo->CleanupFence != VK_NULL_HANDLE)
    {
        Rr_ReturnVulkanFence(Renderer, PresentInfo->CleanupFence);
    }
    if(PresentInfo->EarlySemaphore != VK_NULL_HANDLE)
    {
        Rr_ReturnVulkanSemaphore(Renderer, PresentInfo->EarlySemaphore);
    }
    if(PresentInfo->LateSemaphore != VK_NULL_HANDLE)
    {
        Rr_ReturnVulkanSemaphore(Renderer, PresentInfo->LateSemaphore);
    }
    for(size_t Index = 0; Index < PresentInfo->OldSwapchains.Count; ++Index)
    {
        Rr_SwapchainCleanupData *OldSwapchain =
            PresentInfo->OldSwapchains.Data + Index;
    }
    RR_ZERO_PTR(PresentInfo);
}

static void Rr_CleanupPresentHistory(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    while(Renderer->PresentHistory.Count)
    {
        Rr_PresentInfo *PresentInfo = Renderer->PresentHistory.Data;

        if(PresentInfo->CleanupFence == VK_NULL_HANDLE)
        {
            assert(PresentInfo->ImageIndex != UINT32_MAX);
            break;
        }

        VkResult Result =
            Device->GetFenceStatus(Device->Handle, PresentInfo->CleanupFence);

        if(Result == VK_NOT_READY)
        {
            break;
        }

        Rr_CleanupPresentInfo(Renderer, PresentInfo);

        if(Renderer->PresentHistory.Count > 1)
        {
            memmove(
                Renderer->PresentHistory.Data,
                Renderer->PresentHistory.Data + 1,
                sizeof(Rr_PresentInfo) * (Renderer->PresentHistory.Count - 1));
        }
        Renderer->PresentHistory.Count--;
    }

    if(Renderer->PresentHistory.Count > Renderer->SwapchainImages.Count * 2 &&
       Renderer->PresentHistory.Data->CleanupFence == VK_NULL_HANDLE)
    {
        Rr_PresentInfo PresentInfo = Renderer->PresentHistory.Data[0];

        if(Renderer->PresentHistory.Count > 1)
        {
            memmove(
                Renderer->PresentHistory.Data,
                Renderer->PresentHistory.Data + 1,
                sizeof(Rr_PresentInfo) * (Renderer->PresentHistory.Count - 1));
        }
        Renderer->PresentHistory.Count--;

        assert(PresentInfo.ImageIndex != UINT32_MAX);

        for(size_t Index = 0; Index < Renderer->PresentHistory.Count; ++Index)
        {
            assert(
                Renderer->PresentHistory.Data[Index].OldSwapchains.Count == 0);
        }
        Renderer->PresentHistory.Data[0].OldSwapchains =
            PresentInfo.OldSwapchains;
        RR_ZERO(PresentInfo.OldSwapchains);

        *RR_PUSH_SLICE(&Renderer->PresentHistory, Renderer->Arena) =
            PresentInfo;
    }
}

static void Rr_AddPresentToHistory(
    Rr_Renderer *Renderer,
    uint32_t SwapchainImageIndex)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_PresentInfo *PresentInfo =
        RR_PUSH_SLICE(&Renderer->PresentHistory, Renderer->Arena);
    *PresentInfo = (Rr_PresentInfo){
        .EarlySemaphore = Frame->EarlySemaphore,
        .LateSemaphore = Frame->LateSemaphore,
        .ImageIndex = SwapchainImageIndex,
    };

    memcpy(
        &PresentInfo->OldSwapchains,
        &Renderer->OldSwapchains,
        sizeof(RR_SLICE(Rr_SwapchainCleanupData)));
    RR_ZERO(Renderer->OldSwapchains);

    Frame->EarlySemaphore = VK_NULL_HANDLE;
    Frame->LateSemaphore = VK_NULL_HANDLE;
}

static void Rr_InitFrames(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;
    Rr_Frame *Frames = Renderer->Frames;

    for(size_t Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_Frame *Frame = &Frames[Index];

        /* Command Pool */

        VkCommandPoolCreateInfo CommandPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        };
        Device->CreateCommandPool(
            Device->Handle,
            &CommandPoolCreateInfo,
            NULL,
            &Frame->CommandPool);

        /* Command Buffers */

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

        /* Descriptor Allocator */

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
            RR_ARRAY_COUNT(Ratios),
            Renderer->Arena);

        Frame->Arena = Rr_CreateDefaultArena();
    }
}

static void Rr_CleanupFrames(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    for(size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame *Frame = &Renderer->Frames[Index];

        Device->DestroyCommandPool(Device->Handle, Frame->CommandPool, NULL);

        Rr_DestroyDescriptorAllocator(&Frame->DescriptorAllocator, Device);

        Rr_ReturnVulkanFence(Renderer, Frame->SubmitFence);
        Rr_ReturnVulkanSemaphore(Renderer, Frame->AcquireSemaphore);
        for(size_t Index = 0; Index < Frame->SwapchainGarbage.Count; ++Index)
        {
            Rr_DestroySwapchainImage(
                Renderer,
                Frame->SwapchainGarbage.Data + Index);
        }
        RR_EMPTY_SLICE(&Frame->SwapchainGarbage);
        assert(Frame->EarlySemaphore == VK_NULL_HANDLE);
        assert(Frame->LateSemaphore == VK_NULL_HANDLE);

        Rr_DestroyArena(Frame->Arena);
    }
}

static void Rr_InitVMA(Rr_Renderer *Renderer)
{
    Rr_Instance *Instance = &Renderer->Instance;
    Rr_Device *Device = &Renderer->Device;

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
        // .vkGetBufferMemoryRequirements2KHR =
        // Device->GetBufferMemoryRequirements2,
        // .vkGetImageMemoryRequirements2KHR =
        // Device->GetImageMemoryRequirements2, .vkBindBufferMemory2KHR =
        // Device->BindBufferMemory2, .vkBindImageMemory2KHR =
        // Device->BindImageMemory2,
    };
    VmaAllocatorCreateInfo AllocatorInfo = {
        .flags = 0,
        .physicalDevice = Renderer->PhysicalDevice.Handle,
        .device = Renderer->Device.Handle,
        .pVulkanFunctions = &VulkanFunctions,
        .instance = Renderer->Instance.Handle,
    };
    vmaCreateAllocator(&AllocatorInfo, &Renderer->Allocator);
}

static void Rr_InitImmediateMode(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;
    Rr_ImmediateMode *ImmediateMode = &Renderer->ImmediateMode;

    VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
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

static void Rr_CleanupImmediateMode(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    Device->DestroyCommandPool(
        Device->Handle,
        Renderer->ImmediateMode.CommandPool,
        NULL);
}

/* TODO: Move to queue initialization? */
static void Rr_InitTransientCommandPools(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    Device->CreateCommandPool(
        Device->Handle,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        },
        NULL,
        &Renderer->GraphicsQueue.TransientCommandPool);

    Device->CreateCommandPool(
        Device->Handle,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
        },
        NULL,
        &Renderer->TransferQueue.TransientCommandPool);
}

static void Rr_CleanupTransientCommandPools(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;
    Device->DestroyCommandPool(
        Device->Handle,
        Renderer->GraphicsQueue.TransientCommandPool,
        NULL);
    Device->DestroyCommandPool(
        Device->Handle,
        Renderer->TransferQueue.TransientCommandPool,
        NULL);
}

Rr_Renderer *Rr_CreateRenderer(Rr_App *App)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_Renderer *Renderer = RR_ALLOC_TYPE(Arena, Rr_Renderer);
    Renderer->Arena = Arena;

    SDL_Window *Window = App->Window;
    // Rr_AppConfig *Config = App->Config;

    Rr_InitLoader(&Renderer->Loader);
    Rr_InitInstance(
        &Renderer->Loader,
        SDL_GetWindowTitle(App->Window),
        &Renderer->Instance);
    Rr_InitSurface(Window, &Renderer->Instance, &Renderer->Surface);
    Rr_InitDeviceAndQueues(
        &Renderer->Instance,
        Renderer->Surface,
        &Renderer->PhysicalDevice,
        &Renderer->Device,
        &Renderer->GraphicsQueue,
        &Renderer->TransferQueue);

    Rr_InitVMA(Renderer);
    Rr_InitTransientCommandPools(Renderer);
    Rr_InitFrames(Renderer);
    Rr_InitImmediateMode(Renderer);

    Rr_DestroyScratch(Scratch);

    return Renderer;
}

void Rr_WaitIdle(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;
    Device->DeviceWaitIdle(Device->Handle);
}

void Rr_DestroyRenderer(Rr_Renderer *Renderer)
{
    Rr_Instance *Instance = &Renderer->Instance;
    Rr_Device *Device = &Renderer->Device;

    Rr_WaitIdle(Renderer);

    for(size_t Index = 0; Index < Renderer->RenderPasses.Count; ++Index)
    {
        Device->DestroyRenderPass(
            Device->Handle,
            Renderer->RenderPasses.Data[Index].Handle,
            NULL);
    }

    for(size_t Index = 0; Index < Renderer->Framebuffers.Count; ++Index)
    {
        Device->DestroyFramebuffer(
            Device->Handle,
            Renderer->Framebuffers.Data[Index].Handle,
            NULL);
    }

    Rr_CleanupFrames(Renderer);

    for(size_t Index = 0; Index < Renderer->PresentHistory.Count; ++Index)
    {
        Rr_PresentInfo *PresentInfo = Renderer->PresentHistory.Data + Index;
        if(PresentInfo->CleanupFence != VK_NULL_HANDLE)
        {
            Device->WaitForFences(
                Device->Handle,
                1,
                &PresentInfo->CleanupFence,
                true,
                UINT64_MAX);
        }
        Rr_CleanupPresentInfo(Renderer, PresentInfo);
    }

    for(size_t Index = 0; Index < Renderer->OldSwapchains.Count; ++Index)
    {
        Rr_CleanupSwapchainData(Renderer, &Renderer->OldSwapchains.Data[Index]);
    }

    for(size_t Index = 0; Index < Renderer->SwapchainImages.Count; ++Index)
    {
        Rr_DestroySwapchainImage(
            Renderer,
            &Renderer->SwapchainImages.Data[Index]);
    }

    if(Renderer->Swapchain.Handle != VK_NULL_HANDLE)
    {
        Device->DestroySwapchainKHR(
            Renderer->Device.Handle,
            Renderer->Swapchain.Handle,
            NULL);
    }

    Rr_CleanupTransientCommandPools(Renderer);
    Rr_CleanupImmediateMode(Renderer);

    for(size_t Index = 0; Index < Renderer->DescriptorSetLayouts.Count; ++Index)
    {
        Rr_DescriptorSetLayout *DescriptorSetLayout =
            Renderer->DescriptorSetLayouts.Data + Index;
        Device->DestroyDescriptorSetLayout(
            Device->Handle,
            DescriptorSetLayout->Handle,
            NULL);
    }

    for(size_t Index = 0; Index < Renderer->Semaphores.Count; ++Index)
    {
        Device->DestroySemaphore(
            Device->Handle,
            Renderer->Semaphores.Data[Index],
            NULL);
    }

    for(size_t Index = 0; Index < Renderer->Fences.Count; ++Index)
    {
        Device->DestroyFence(
            Device->Handle,
            Renderer->Fences.Data[Index],
            NULL);
    }

    vmaDestroyAllocator(Renderer->Allocator);

    Instance->DestroySurfaceKHR(Instance->Handle, Renderer->Surface, NULL);
    Device->DestroyDevice(Device->Handle, NULL);

    Instance->DestroyInstance(Instance->Handle, NULL);

    Rr_DestroyArena(Renderer->Arena);
}

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    Rr_ImmediateMode *ImmediateMode = &Renderer->ImmediateMode;
    Device->ResetCommandBuffer(ImmediateMode->CommandBuffer, 0);

    VkCommandBufferBeginInfo BeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    Device->BeginCommandBuffer(ImmediateMode->CommandBuffer, &BeginInfo);

    return ImmediateMode->CommandBuffer;
}

void Rr_EndImmediate(Rr_Renderer *Renderer)
{
    Rr_Device *Device = &Renderer->Device;

    Rr_ImmediateMode *ImmediateMode = &Renderer->ImmediateMode;

    Device->EndCommandBuffer(ImmediateMode->CommandBuffer);

    VkFence Fence = Rr_GetVulkanFence(Renderer);

    VkSubmitInfo SubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ImmediateMode->CommandBuffer,
    };

    Device->QueueSubmit(Renderer->GraphicsQueue.Handle, 1, &SubmitInfo, Fence);
    Device->WaitForFences(Device->Handle, 1, &Fence, true, UINT64_MAX);

    Rr_ReturnVulkanFence(Renderer, Fence);
}

static void Rr_ProcessPendingLoads(Rr_App *App)
{
    Rr_Renderer *Renderer = App->Renderer;

    if(Rr_TryLockSpinlock(&App->SyncArena.Lock))
    {
        for(size_t Index = 0; Index < Renderer->PendingLoadsSlice.Count;
            ++Index)
        {
            Rr_PendingLoad *PendingLoad =
                &Renderer->PendingLoadsSlice.Data[Index];
            PendingLoad->LoadingCallback(App, PendingLoad->UserData);
        }
        RR_EMPTY_SLICE(&Renderer->PendingLoadsSlice);

        Rr_UnlockSpinlock(&App->SyncArena.Lock);
    }
}

void Rr_PrepareFrame(Rr_App *App)
{
    Rr_Renderer *Renderer = App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_ResetArena(Frame->Arena);

    Frame->VirtualSwapchainImage = RR_ALLOC_TYPE(Frame->Arena, Rr_Image);

    /* These are applied again just before graph execution. */

    Frame->VirtualSwapchainImage->Extent = Renderer->Swapchain.Extent;
    Frame->VirtualSwapchainImage->Format = Renderer->Swapchain.Format;
    Frame->VirtualSwapchainImage->AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    Frame->Graph = RR_ALLOC_TYPE(Frame->Arena, Rr_Graph);
    Frame->Graph->Arena = Frame->Arena;
    Frame->Graph->SwapchainImageResourceIndex =
        Rr_GetGraphImageHandle(Frame->Graph, Frame->VirtualSwapchainImage)
            ->Values.Index;

    Rr_ProcessPendingLoads(App);
}

void Rr_DrawFrame(Rr_App *App)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Renderer *Renderer = App->Renderer;
    Rr_Device *Device = &Renderer->Device;
    Rr_Swapchain *Swapchain = &Renderer->Swapchain;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    VkResult Result;

    if(Frame->SubmitFence != VK_NULL_HANDLE)
    {
        Result = Device->WaitForFences(
            Device->Handle,
            1,
            &Frame->SubmitFence,
            true,
            1000000000);
        assert(Result != VK_TIMEOUT && "Render fence timeout!");

        Device->ResetFences(Device->Handle, 1, &Frame->SubmitFence);
        Rr_ReturnVulkanFence(Renderer, Frame->SubmitFence);
        Rr_ReturnVulkanSemaphore(Renderer, Frame->AcquireSemaphore);

        Rr_ResetDescriptorAllocator(&Frame->DescriptorAllocator, Device);

        for(size_t Index = 0; Index < Frame->SwapchainGarbage.Count; ++Index)
        {
            Rr_SwapchainImage *Garbage = Frame->SwapchainGarbage.Data + Index;
            Rr_DestroySwapchainImage(Renderer, Garbage);
        }
        RR_EMPTY_SLICE(&Frame->SwapchainGarbage);

        assert(Frame->EarlySemaphore == VK_NULL_HANDLE);
        assert(Frame->LateSemaphore == VK_NULL_HANDLE);
    }

    Frame->SubmitFence = Rr_GetVulkanFence(Renderer);
    Frame->AcquireSemaphore = Rr_GetVulkanSemaphore(Renderer);
    Frame->EarlySemaphore = Rr_GetVulkanSemaphore(Renderer);
    Frame->LateSemaphore = Rr_GetVulkanSemaphore(Renderer);

    Rr_CheckSwapchainDirty(App);

    /* Acquire swapchain image. */

    uint32_t SwapchainImageIndex;
    Result = Rr_AcquireNextImage(Renderer, &SwapchainImageIndex);
    if(Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
    {
        Rr_RecreateSwapchain(App);
        Result = Rr_AcquireNextImage(Renderer, &SwapchainImageIndex);
    }
    if(Result != VK_SUBOPTIMAL_KHR)
    {
        assert(Result >= 0);
    }

    VkImage SwapchainImage =
        Renderer->SwapchainImages.Data[SwapchainImageIndex].Handle;

    /* Now that we acquired swapchain image index we can
     * put real handles to virtual swapchain image which
     * will be used by the graph. */

    *Frame->VirtualSwapchainImage = (Rr_Image){
        .AllocatedImageCount = 1,
        .Extent = Renderer->Swapchain.Extent,
        .Format = Renderer->Swapchain.Format,
        .AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
        .AllocatedImages[0] = {
            .View = Renderer->SwapchainImages.Data[SwapchainImageIndex].View,
            .Handle = SwapchainImage,
            .Container = Frame->VirtualSwapchainImage,
        },
    };

    VkCommandBufferBeginInfo CommandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };

    /* Execute Frame Graph */

    Device->BeginCommandBuffer(
        Frame->EarlyCommandBuffer,
        &CommandBufferBeginInfo);
    Device->BeginCommandBuffer(
        Frame->LateCommandBuffer,
        &CommandBufferBeginInfo);

    Rr_ExecuteGraph(Renderer, Frame->Graph, Scratch.Arena);

    Device->EndCommandBuffer(Frame->EarlyCommandBuffer);

    /* Always transition swapchain image to present layout. */

    Rr_SyncState *SwapchainImageSyncState =
        Rr_GetSyncState(Renderer, (Rr_MapKey)SwapchainImage);
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
            .image = SwapchainImage,
            .oldLayout = SwapchainImageSyncState->Specific.Layout,
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
    SwapchainImageSyncState->Specific.Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    Device->EndCommandBuffer(Frame->LateCommandBuffer);

    /* Submit frame command buffer and queue present. */

    VkSubmitInfo SubmitInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &Frame->EarlyCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &Frame->EarlySemaphore,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &Frame->LateCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &Frame->LateSemaphore,
            .waitSemaphoreCount = 2,
            .pWaitSemaphores =
                (VkSemaphore[]){
                    Frame->EarlySemaphore,
                    Frame->AcquireSemaphore,
                },
            .pWaitDstStageMask =
                (VkPipelineStageFlags[]){
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                },
        },
    };

    Rr_LockSpinlock(&Renderer->GraphicsQueue.Lock);

    Device->QueueSubmit(
        Renderer->GraphicsQueue.Handle,
        2,
        SubmitInfos,
        Frame->SubmitFence);

    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->LateSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    Result =
        Device->QueuePresentKHR(Renderer->GraphicsQueue.Handle, &PresentInfo);

    Rr_UnlockSpinlock(&Renderer->GraphicsQueue.Lock);

    Rr_AddPresentToHistory(Renderer, SwapchainImageIndex);
    Rr_CleanupPresentHistory(Renderer);

    if(Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
    {
        Rr_RecreateSwapchain(App);
    }

    Renderer->FrameNumber++;
    Renderer->CurrentFrameIndex = Renderer->FrameNumber % RR_FRAME_OVERLAP;

    Rr_DestroyScratch(Scratch);
}

Rr_Frame *Rr_GetCurrentFrame(Rr_Renderer *Renderer)
{
    return &Renderer->Frames[Renderer->CurrentFrameIndex];
}

bool Rr_IsUsingTransferQueue(Rr_Renderer *Renderer)
{
    return Renderer->TransferQueue.Handle != VK_NULL_HANDLE;
}

size_t Rr_GetUniformAlignment(Rr_Renderer *Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits
        .minUniformBufferOffsetAlignment;
}

size_t Rr_GetStorageAlignment(Rr_Renderer *Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits
        .minStorageBufferOffsetAlignment;
}

size_t Rr_GetMaxComputeSharedMemorySize(Rr_Renderer *Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits
        .maxComputeSharedMemorySize;
}

size_t Rr_GetMaxComputeWorkgroupInvocations(Rr_Renderer *Renderer)
{
    return Renderer->PhysicalDevice.Properties.properties.limits
        .maxComputeWorkGroupInvocations;
}

Rr_Graph *Rr_GetGraph(Rr_Renderer *Renderer)
{
    return Rr_GetCurrentFrame(Renderer)->Graph;
}

Rr_Arena *Rr_GetFrameArena(Rr_Renderer *Renderer)
{
    return Rr_GetCurrentFrame(Renderer)->Arena;
}

Rr_TextureFormat Rr_GetSwapchainFormat(Rr_Renderer *Renderer)
{
    return Rr_GetTextureFormat(Renderer->Swapchain.Format);
}

Rr_IntVec2 Rr_GetSwapchainSize(Rr_Renderer *Renderer)
{
    return (Rr_IntVec2){
        (int32_t)Renderer->Swapchain.Extent.width,
        (int32_t)Renderer->Swapchain.Extent.height,
    };
}

Rr_Image *Rr_GetSwapchainImage(Rr_Renderer *Renderer)
{
    return Rr_GetCurrentFrame(Renderer)->VirtualSwapchainImage;
}

bool Rr_SetSwapchainPresentMode(
    Rr_Renderer *Renderer,
    Rr_PresentMode PresentMode)
{
    Renderer->Swapchain.PresentMode = PresentMode;
    Rr_SetSwapchainDirty(Renderer, true);

    return true;
}

VkRenderPass Rr_GetVulkanRenderPass(
    Rr_Renderer *Renderer,
    Rr_RenderPassInfo *Info)
{
    assert(Info != NULL);

    uint32_t Hash = XXH32(
        Info->Attachments,
        sizeof(Rr_RenderPassAttachment) * Info->AttachmentCount,
        0);

    for(size_t Index = 0; Index < Renderer->RenderPasses.Count; ++Index)
    {
        if(Renderer->RenderPasses.Data[Index].Hash == Hash)
        {
            return Renderer->RenderPasses.Data[Index].Handle;
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

    for(uint32_t Index = 0; Index < Info->AttachmentCount; ++Index)
    {
        Rr_RenderPassAttachment *Attachment = &Info->Attachments[Index];
        if(Rr_IsVulkanDepthFormat(Attachment->Format))
        {
            if(DepthReference != NULL)
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
                .loadOp = Rr_GetLoadOp(Attachment->LoadOp),
                .storeOp = Rr_GetStoreOp(Attachment->StoreOp),
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
                .loadOp = Rr_GetLoadOp(Attachment->LoadOp),
                .storeOp = Rr_GetStoreOp(Attachment->StoreOp),
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

    Rr_Device *Device = &Renderer->Device;

    Device->CreateRenderPass(
        Device->Handle,
        &RenderPassCreateInfo,
        NULL,
        &RenderPass);

    *RR_PUSH_SLICE(&Renderer->RenderPasses, Renderer->Arena) = (Rr_RenderPass){
        .Handle = RenderPass,
        .Hash = Hash,
    };

    Rr_DestroyScratch(Scratch);

    return RenderPass;
}

static VkFramebuffer Rr_GetFramebufferInternal(
    Rr_Renderer *Renderer,
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    size_t QuerySize = sizeof(VkImageView) * ImageViewCount +
                       sizeof(VkExtent3D) + sizeof(VkRenderPass);
    void *Query = RR_ALLOC(Scratch.Arena, QuerySize);
    memcpy(Query, ImageViews, sizeof(VkImageView) * ImageViewCount);
    memcpy(
        ((char *)Query) + sizeof(VkImageView) * ImageViewCount,
        &Extent,
        sizeof(VkExtent3D));
    memcpy(
        ((char *)Query) + sizeof(VkImageView) * ImageViewCount + sizeof(Extent),
        &RenderPass,
        sizeof(VkRenderPass));

    uint32_t Hash = XXH32(Query, QuerySize, 0);

    VkFramebuffer Framebuffer = VK_NULL_HANDLE;

    for(size_t Index = 0; Index < Renderer->Framebuffers.Count; ++Index)
    {
        Rr_Framebuffer *CachedFramebuffer = Renderer->Framebuffers.Data + Index;

        if(CachedFramebuffer->Hash == Hash)
        {
            return CachedFramebuffer->Handle;
        }
    }

    VkFramebufferCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = RenderPass,
        .height = Extent.height,
        .width = Extent.width,
        .layers = Extent.depth,
        .attachmentCount = (uint32_t)ImageViewCount,
        .pAttachments = ImageViews,
    };

    Rr_Device *Device = &Renderer->Device;

    Device->CreateFramebuffer(Device->Handle, &CreateInfo, NULL, &Framebuffer);

    *RR_PUSH_SLICE(&Renderer->Framebuffers, Renderer->Arena) = (Rr_Framebuffer){
        .Handle = Framebuffer,
        .Hash = Hash,
    };

    Rr_DestroyScratch(Scratch);

    return Framebuffer;
}

VkFramebuffer Rr_GetVulkanFramebufferFromViews(
    Rr_Renderer *Renderer,
    VkRenderPass RenderPass,
    VkImageView *ImageViews,
    size_t ImageViewCount,
    VkExtent3D Extent)
{
    return Rr_GetFramebufferInternal(
        Renderer,
        RenderPass,
        ImageViews,
        ImageViewCount,
        Extent,
        NULL);
}

VkFramebuffer Rr_GetVulkanFramebuffer(
    Rr_Renderer *Renderer,
    VkRenderPass RenderPass,
    Rr_Image *Images,
    size_t ImageCount,
    VkExtent3D Extent)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkImageView *ImageViews =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkImageView, ImageCount);

    for(size_t Index = 0; Index < ImageCount; ++Index)
    {
        ImageViews[Index] =
            Rr_GetCurrentAllocatedImage(Renderer, Images + Index)->View;
    }

    VkFramebuffer Framebuffer = Rr_GetFramebufferInternal(
        Renderer,
        RenderPass,
        ImageViews,
        ImageCount,
        Extent,
        Scratch.Arena);

    Rr_DestroyScratch(Scratch);

    return Framebuffer;
}

Rr_SyncState *Rr_GetSyncState(Rr_Renderer *Renderer, Rr_MapKey Key)
{
    Rr_SyncState **SyncStateRef =
        RR_UPSERT(&Renderer->GlobalSync, Key, Renderer->Arena);
    if(*SyncStateRef != NULL)
    {
        return *SyncStateRef;
    }
    *SyncStateRef =
        RR_GET_FREE_LIST_ITEM(&Renderer->SyncStates, Renderer->Arena);
    Rr_SyncState *SyncState = *SyncStateRef;
    RR_ZERO_PTR(SyncState);
    return SyncState;
}

void Rr_ReturnSyncState(Rr_Renderer *Renderer, Rr_MapKey Key)
{
    Rr_SyncState **SyncStateRef =
        RR_UPSERT(&Renderer->GlobalSync, Key, Renderer->Arena);
    if(*SyncStateRef != NULL)
    {
        RR_RETURN_FREE_LIST_ITEM(&Renderer->SyncStates, *SyncStateRef);
    }
    *SyncStateRef = NULL;
}

VkSemaphore Rr_GetVulkanSemaphore(Rr_Renderer *Renderer)
{
    if(Renderer->Semaphores.Count > 0)
    {
        return RR_POP_SLICE(&Renderer->Semaphores);
    }

    Rr_Device *Device = &Renderer->Device;

    VkSemaphore Semaphore;

    Device->CreateSemaphore(
        Device->Handle,
        &(VkSemaphoreCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        },
        NULL,
        &Semaphore);

    return Semaphore;
}

void Rr_ReturnVulkanSemaphore(Rr_Renderer *Renderer, VkSemaphore Semaphore)
{
    *RR_PUSH_SLICE(&Renderer->Semaphores, Renderer->Arena) = Semaphore;
}

VkFence Rr_GetVulkanFence(Rr_Renderer *Renderer)
{
    if(Renderer->Fences.Count > 0)
    {
        return RR_POP_SLICE(&Renderer->Fences);
    }

    Rr_Device *Device = &Renderer->Device;

    VkFence Fence;

    Device->CreateFence(
        Device->Handle,
        &(VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        },
        NULL,
        &Fence);

    return Fence;
}

void Rr_ReturnVulkanFence(Rr_Renderer *Renderer, VkFence Fence)
{
    Rr_Device *Device = &Renderer->Device;
    *RR_PUSH_SLICE(&Renderer->Fences, Renderer->Arena) = Fence;
    Device->ResetFences(Device->Handle, 1, &Fence);
}
