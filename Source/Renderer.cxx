#define VOLK_IMPLEMENTATION

#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

#include "Renderer.hxx"

#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <vulkan/vk_enum_string_helper.h>

#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <SDL_log.h>
#include <SDL3/SDL_vulkan.h>

#include "App.hxx"
#include "RendererHelpers.hxx"

#define VK_ASSERT(Expr)                                                                                                              \
    {                                                                                                                                \
        VkResult Result = Expr;                                                                                                      \
        if (Result != VK_SUCCESS)                                                                                                    \
        {                                                                                                                            \
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Assertion " #Expr " == VK_SUCCESS failed! Result is %s", string_VkResult(Result)); \
            abort();                                                                                                                 \
        }                                                                                                                            \
    }

#ifdef DEBUG
static const b32 bEnableValidationLayers = true;
#else
static const b32 bEnableValidationLayers = false;
#endif

static SFrameData* GetCurrentFrame(SRenderer* Renderer)
{
    return &Renderer->Frames[Renderer->FrameNumber % FRAME_OVERLAP];
}

static bool CheckPhysicalDevice(SRenderer* Renderer, VkPhysicalDevice PhysicalDevice)
{
    u32 ExtensionCount;
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);
    if (ExtensionCount == 0)
    {
        return false;
    }

    auto* Extensions = SDL_stack_alloc(VkExtensionProperties, ExtensionCount);
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, Extensions);

    bool bSwapchainFound = false;
    for (u32 Index = 0; Index < ExtensionCount; Index++)
    {
        if (strcmp(Extensions[Index].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            bSwapchainFound = true;
            break;
        }
    }
    if (!bSwapchainFound)
    {
        return false;
    }

    u32 QueueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);
    if (QueueFamilyCount == 0)
    {
        return false;
    }

    auto* QueueFamilyProperties = SDL_stack_alloc(VkQueueFamilyProperties, QueueFamilyCount);
    auto* QueuePresentSupport = SDL_stack_alloc(VkBool32, QueueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilyProperties);

    for (u32 Index = 0; Index < QueueFamilyCount; ++Index)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, Index, Renderer->Surface, &QueuePresentSupport[Index]);

        if (!QueuePresentSupport[Index])
        {
            continue;
        }

        if ((QueueFamilyProperties[Index].queueCount > 0) && (QueueFamilyProperties[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            Renderer->GraphicsQueue.FamilyIndex = Index;
            return true;
        }
    }

    return false;
}

static void InitDevice(SRenderer* Renderer)
{
    u32 PhysicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(Renderer->Instance, &PhysicalDeviceCount, nullptr);
    if (PhysicalDeviceCount == 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No device with Vulkan support found");
        abort();
    }

    auto* PhysicalDevices = SDL_stack_alloc(VkPhysicalDevice, PhysicalDeviceCount);
    vkEnumeratePhysicalDevices(Renderer->Instance, &PhysicalDeviceCount, &PhysicalDevices[0]);

    VkPhysicalDeviceProperties PhysicalDeviceProperties = {};
    for (u32 Index = 0; Index < PhysicalDeviceCount; Index++)
    {
        if (CheckPhysicalDevice(Renderer, PhysicalDevices[Index]))
        {
            Renderer->PhysicalDevice.Handle = PhysicalDevices[Index];
            vkGetPhysicalDeviceProperties(PhysicalDevices[Index], &PhysicalDeviceProperties);
            SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "Selected GPU: %s", PhysicalDeviceProperties.deviceName);
            break;
        }
    }

    if (Renderer->PhysicalDevice.Handle == VK_NULL_HANDLE || Renderer->GraphicsQueue.FamilyIndex == UINT32_MAX)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Could not select physical device based on the chosen properties!");
        abort();
    }
    else
    {
        vkGetPhysicalDeviceFeatures(Renderer->PhysicalDevice.Handle, &Renderer->PhysicalDevice.Features);
        vkGetPhysicalDeviceMemoryProperties(Renderer->PhysicalDevice.Handle, &Renderer->PhysicalDevice.MemoryProperties);
    }

    const float QueuePriorities[] = { 1.0f };
    VkDeviceQueueCreateInfo QueueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = QueuePriorities,
    };

    VkPhysicalDeviceVulkan13Features Features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = VK_NULL_HANDLE,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features Features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &Features13,
        .descriptorIndexing = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };

    // VkPhysicalDeviceFeatures2 Features2 = {
    //     .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    //     .pNext = &float16_int8
    // };
    // vkGetPhysicalDeviceFeatures2(Renderer->PhysicalDevice.Handle, &Features2);

    const char* SwapchainExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    VkDeviceCreateInfo DeviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &Features12,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &QueueInfo,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &SwapchainExtension,
    };

    VK_ASSERT(vkCreateDevice(Renderer->PhysicalDevice.Handle, &DeviceCreateInfo, nullptr, &Renderer->Device));

    vkGetDeviceQueue(Renderer->Device, Renderer->GraphicsQueue.FamilyIndex, 0, &Renderer->GraphicsQueue.Handle);
}

static void CleanupSwapchain(SRenderer* Renderer, VkSwapchainKHR Swapchain)
{
    for (u32 Index = 0; Index < Renderer->Swapchain.ImageCount; Index++)
    {
        vkDestroyImageView(Renderer->Device, Renderer->Swapchain.Images[Index].View, nullptr);
    }
    memset(Renderer->Swapchain.Images, 0, sizeof(Renderer->Swapchain.Images));
    Renderer->Swapchain.ImageCount = 0;
    vkDestroySwapchainKHR(Renderer->Device, Swapchain, nullptr);
}

static void CreateSwapchain(SRenderer* Renderer, u32* Width, u32* Height, bool bVSync)
{
    VkSwapchainKHR OldSwapchain = Renderer->Swapchain.Handle;

    VkSurfaceCapabilitiesKHR SurfCaps;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &SurfCaps));

    u32 PresentModeCount;
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &PresentModeCount, nullptr));
    assert(PresentModeCount > 0);

    auto PresentModes = SDL_stack_alloc(VkPresentModeKHR, PresentModeCount);
    VK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &PresentModeCount, PresentModes));

    VkPresentModeKHR SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    if (!bVSync)
    {
        for (u32 Index = 0; Index < PresentModeCount; Index++)
        {
            if (PresentModes[Index] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                SwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if (PresentModes[Index] == VK_PRESENT_MODE_IMMEDIATE_KHR)
            {
                SwapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    u32 DesiredNumberOfSwapchainImages = Renderer->CommandBufferCount = SurfCaps.minImageCount + 1;
    if ((SurfCaps.maxImageCount > 0) && (DesiredNumberOfSwapchainImages > SurfCaps.maxImageCount))
    {
        DesiredNumberOfSwapchainImages = SurfCaps.maxImageCount;
    }

    VkSurfaceTransformFlagsKHR PreTransform;
    if (SurfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        PreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        PreTransform = SurfCaps.currentTransform;
    }

    VkExtent2D SwapchainExtent = {};
    if (SurfCaps.currentExtent.width == UINT32_MAX)
    {
        SwapchainExtent.width = *Width;
        SwapchainExtent.height = *Height;
    }
    else
    {
        SwapchainExtent = SurfCaps.currentExtent;
        *Width = SurfCaps.currentExtent.width;
        *Height = SurfCaps.currentExtent.height;
    }

    Renderer->Swapchain.Extent = SwapchainExtent;

    u32 FormatCount;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &FormatCount, nullptr));
    assert(FormatCount > 0);

    auto* SurfaceFormats = SDL_stack_alloc(VkSurfaceFormatKHR, FormatCount);
    VK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(Renderer->PhysicalDevice.Handle, Renderer->Surface, &FormatCount, SurfaceFormats));

    bool bPreferredFormatFound = false;
    for (u32 Index = 0; Index < PresentModeCount; Index++)
    {
        VkSurfaceFormatKHR* SurfaceFormat = &SurfaceFormats[Index];

        if (SurfaceFormat->format == VK_FORMAT_B8G8R8A8_UNORM || SurfaceFormat->format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            Renderer->Swapchain.Format = SurfaceFormat->format;
            Renderer->Swapchain.ColorSpace = SurfaceFormat->colorSpace;
            bPreferredFormatFound = true;
            break;
        }
    }

    if (!bPreferredFormatFound)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No preferred surface format found!");
        abort();
    }

    VkCompositeAlphaFlagBitsKHR CompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR CompositeAlphaFlags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (auto& CompositeAlphaFlag : CompositeAlphaFlags)
    {
        if (SurfCaps.supportedCompositeAlpha & CompositeAlphaFlag)
        {
            CompositeAlpha = CompositeAlphaFlag;
            break;
        }
    }

    VkSwapchainCreateInfoKHR SwapchainCI = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = Renderer->Surface,
        .minImageCount = DesiredNumberOfSwapchainImages,
        .imageFormat = Renderer->Swapchain.Format,
        .imageColorSpace = Renderer->Swapchain.ColorSpace,
        .imageExtent = { SwapchainExtent.width, SwapchainExtent.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = (VkSurfaceTransformFlagBitsKHR)PreTransform,
        .compositeAlpha = CompositeAlpha,
        .presentMode = SwapchainPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = OldSwapchain,
    };

    if (SurfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        SwapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (SurfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        SwapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VK_ASSERT(vkCreateSwapchainKHR(Renderer->Device, &SwapchainCI, nullptr, &Renderer->Swapchain.Handle));

    if (OldSwapchain != VK_NULL_HANDLE)
    {
        CleanupSwapchain(Renderer, OldSwapchain);
    }

    u32 ImageCount = 0;
    VK_ASSERT(vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, nullptr));
    assert(ImageCount <= MAX_SWAPCHAIN_IMAGE_COUNT);

    Renderer->Swapchain.ImageCount = ImageCount;
    auto Images = SDL_stack_alloc(VkImage, ImageCount);
    VK_ASSERT(vkGetSwapchainImagesKHR(Renderer->Device, Renderer->Swapchain.Handle, &ImageCount, Images));

    VkImageViewCreateInfo ColorAttachmentView = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = Renderer->Swapchain.Format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    for (u32 i = 0; i < ImageCount; i++)
    {
        Renderer->Swapchain.Images[i].Handle = Images[i];
        ColorAttachmentView.image = Images[i];
        VK_ASSERT(vkCreateImageView(Renderer->Device, &ColorAttachmentView, nullptr, &Renderer->Swapchain.Images[i].View));
    }
}

static VkBool32 VKAPI_PTR DebugMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if (pCallbackData != nullptr)
    {
        const char* format = "{0x%x}% s\n %s ";
        SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, format,
            pCallbackData->messageIdNumber,
            pCallbackData->pMessageIdName,
            pCallbackData->pMessage);
    }
    return VK_FALSE;
}

static void VKAPI_CALL InitDebugMessenger(SRenderer* Renderer)
{
    VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCI = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = static_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(DebugMessage)
    };

    VK_ASSERT(vkCreateDebugUtilsMessengerEXT(Renderer->Instance, &DebugUtilsMessengerCI, nullptr, &Renderer->Messenger));
}

static void InitCommands(SRenderer* Renderer)
{
    VkCommandPoolCreateInfo CommandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
    };

    for (auto& Frame : Renderer->Frames)
    {
        VK_ASSERT(vkCreateCommandPool(Renderer->Device, &CommandPoolInfo, nullptr, &Frame.CommandPool));

        VkCommandBufferAllocateInfo cmdAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .commandPool = Frame.CommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VK_ASSERT(vkAllocateCommandBuffers(Renderer->Device, &cmdAllocInfo, &Frame.MainCommandBuffer));
    }
}

static void InitSyncStructures(SRenderer* Renderer)
{
    VkDevice Device = Renderer->Device;
    SFrameData* Frames = Renderer->Frames;

    VkFenceCreateInfo fenceCreateInfo = GetFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = GetSemaphoreCreateInfo(0);

    for (i32 Index = 0; Index < FRAME_OVERLAP; Index++)
    {
        VK_ASSERT(vkCreateFence(Device, &fenceCreateInfo, nullptr, &Frames[Index].RenderFence));

        VK_ASSERT(vkCreateSemaphore(Device, &semaphoreCreateInfo, nullptr, &Frames[Index].SwapchainSemaphore));
        VK_ASSERT(vkCreateSemaphore(Device, &semaphoreCreateInfo, nullptr, &Frames[Index].RenderSemaphore));
    }
}

void RendererInit(SApp* App)
{
    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    SRenderer* Renderer = &App->Renderer;
    *Renderer = {};

    VkApplicationInfo VKAppInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = AppTitle,
        .pEngineName = AppTitle,
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0)
    };

    u32 ExtensionCount;
    const char* const* SDLExtensions = SDL_Vulkan_GetInstanceExtensions(&ExtensionCount);

    auto Extensions = SDL_stack_alloc(const char*, ExtensionCount + 1);
    for (u32 Index = 0; Index < ExtensionCount; Index++)
    {
        Extensions[Index] = SDLExtensions[Index];
    }

    if (bEnableValidationLayers)
    {
        Extensions[ExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        ExtensionCount++;
    }

    VkInstanceCreateInfo VKInstInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = &VKAppInfo,
        .enabledExtensionCount = ExtensionCount,
        .ppEnabledExtensionNames = Extensions,
    };

    if (bEnableValidationLayers)
    {
        const char* ValidationLayerName = "VK_LAYER_KHRONOS_validation";

        u32 InstanceLayerCount;
        vkEnumerateInstanceLayerProperties(&InstanceLayerCount, nullptr);
        auto InstanceLayerProperties = SDL_stack_alloc(VkLayerProperties, InstanceLayerCount);
        vkEnumerateInstanceLayerProperties(&InstanceLayerCount, InstanceLayerProperties);

        bool bValidationLayerPresent = false;
        for (u32 Index = 0; Index < InstanceLayerCount; Index++)
        {
            VkLayerProperties* Layer = &InstanceLayerProperties[Index];
            if (strcmp(Layer->layerName, ValidationLayerName) == 0)
            {
                bValidationLayerPresent = true;
                break;
            }
        }
        if (!bValidationLayerPresent)
        {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "No %s present!\n", ValidationLayerName);
            abort();
        }

        VKInstInfo.enabledLayerCount = 1;
        VKInstInfo.ppEnabledLayerNames = &ValidationLayerName;
    }

    VK_ASSERT(vkCreateInstance(&VKInstInfo, nullptr, &Renderer->Instance));

    volkLoadInstance(Renderer->Instance);

    if (bEnableValidationLayers)
    {
        InitDebugMessenger(Renderer);
    }

    if (SDL_Vulkan_CreateSurface(App->Window, Renderer->Instance, nullptr, &Renderer->Surface) != SDL_TRUE)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create Vulkan surface: %s", SDL_GetError());
        abort();
    }

    InitDevice(Renderer);

    u32 Width, Height;
    bool bVSync = true;
    CreateSwapchain(Renderer, &Width, &Height, bVSync);

    InitCommands(Renderer);

    InitSyncStructures(Renderer);
}

void RendererCleanup(SRenderer* Renderer)
{
    VkDevice Device = Renderer->Device;

    vkDeviceWaitIdle(Renderer->Device);

    for (auto& Index : Renderer->Frames)
    {
        SFrameData* Frame = &Index;
        vkDestroyCommandPool(Renderer->Device, Frame->CommandPool, nullptr);

        vkDestroyFence(Device, Frame->RenderFence, nullptr);
        vkDestroySemaphore(Device, Frame->RenderSemaphore, nullptr);
        vkDestroySemaphore(Device, Frame->SwapchainSemaphore, nullptr);
    }

    CleanupSwapchain(Renderer, Renderer->Swapchain.Handle);

    vkDestroySurfaceKHR(Renderer->Instance, Renderer->Surface, nullptr);
    vkDestroyDevice(Renderer->Device, nullptr);

    if (bEnableValidationLayers)
    {
        vkDestroyDebugUtilsMessengerEXT(Renderer->Instance, Renderer->Messenger, nullptr);
    }
    vkDestroyInstance(Renderer->Instance, nullptr);
}

void RendererDraw(SRenderer* Renderer)
{
    VkDevice Device = Renderer->Device;
    SFrameData* CurrentFrame = GetCurrentFrame(Renderer);
    VK_ASSERT(vkWaitForFences(Device, 1, &CurrentFrame->RenderFence, true, 1000000000));
    VK_ASSERT(vkResetFences(Device, 1, &CurrentFrame->RenderFence));

    u32 SwapchainImageIndex;
    VK_ASSERT(vkAcquireNextImageKHR(Device, Renderer->Swapchain.Handle, 1000000000, CurrentFrame->SwapchainSemaphore, VK_NULL_HANDLE, &SwapchainImageIndex));

    VkCommandBuffer CommandBuffer = CurrentFrame->MainCommandBuffer;
    VK_ASSERT(vkResetCommandBuffer(CommandBuffer, 0));
    VkCommandBufferBeginInfo cmdBeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_ASSERT(vkBeginCommandBuffer(CommandBuffer, &cmdBeginInfo));

    TransitionImage(CommandBuffer, Renderer->Swapchain.Images[SwapchainImageIndex].Handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    float Flash = fabsf(sinf((float)Renderer->FrameNumber / 240.0f));
    VkClearColorValue ClearValue = { { 0.0f, 0.0f, Flash, 1.0f } };

    VkImageSubresourceRange ClearRange = GetImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdClearColorImage(CommandBuffer, Renderer->Swapchain.Images[SwapchainImageIndex].Handle, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &ClearRange);

    TransitionImage(CommandBuffer, Renderer->Swapchain.Images[SwapchainImageIndex].Handle, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_ASSERT(vkEndCommandBuffer(CommandBuffer));

    VkCommandBufferSubmitInfo CommandBufferSubmitInfo = GetCommandBufferSubmitInfo(CommandBuffer);

    VkSemaphoreSubmitInfo WaitSemaphoreSubmitInfo = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, CurrentFrame->SwapchainSemaphore);
    VkSemaphoreSubmitInfo SignalSemaphoreSubmitInfo = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, CurrentFrame->RenderSemaphore);

    VkSubmitInfo2 SubmitInfo = GetSubmitInfo(&CommandBufferSubmitInfo, &SignalSemaphoreSubmitInfo, &WaitSemaphoreSubmitInfo);

    VK_ASSERT(vkQueueSubmit2(Renderer->GraphicsQueue.Handle, 1, &SubmitInfo, CurrentFrame->RenderFence));

    VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &CurrentFrame->RenderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Renderer->Swapchain.Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    VK_ASSERT(vkQueuePresentKHR(Renderer->GraphicsQueue.Handle, &PresentInfo));

    Renderer->FrameNumber++;
}
