#include "Rr_Memory.h"

#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_SYSTEM_ALIGNED_MALLOC Rr_AlignedAlloc
#define VMA_SYSTEM_ALIGNED_FREE Rr_AlignedFree
#include <vk_mem_alloc.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC Rr_Malloc
#define STBI_REALLOC Rr_Realloc
#define STBI_FREE Rr_Free
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>