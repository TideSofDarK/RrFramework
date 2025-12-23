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

#include <Rr/Rr_Defines.h>

extern "C" {
extern void *Rr_AlignedAlloc(size_t Alignment, size_t Bytes);
extern void Rr_AlignedFree(void *Ptr);
}

#define VK_NO_PROTOTYPES

#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION           1001000
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_SYSTEM_ALIGNED_MALLOC    Rr_AlignedAlloc
#define VMA_SYSTEM_ALIGNED_FREE      Rr_AlignedFree
#include <vma/vk_mem_alloc.h>
