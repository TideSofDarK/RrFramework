#pragma once

#define VK_NO_PROTOTYPES

#include <assert.h>
#include <vk_mem_alloc.h>

#define VK_ASSERT(expr)             \
    {                               \
        assert(expr == VK_SUCCESS); \
    }

typedef struct SApp SApp;

typedef struct SRenderer
{
    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkQueue Queue;
    VmaAllocator Allocator;
    VkSurfaceKHR Surface;
    VkDebugUtilsMessengerEXT Messenger;
} SRenderer;

void CreateRenderer(SApp* App);
