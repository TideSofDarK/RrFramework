#define VK_NO_PROTOTYPES

#include <SDL3/SDL_stdinc.h>

#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_SYSTEM_ALIGNED_MALLOC SDL_aligned_alloc
#define VMA_SYSTEM_ALIGNED_FREE SDL_aligned_free
#include <vk_mem_alloc.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>
