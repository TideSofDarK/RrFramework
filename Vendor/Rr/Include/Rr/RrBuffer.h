#pragma once

#include "RrDefines.h"
#include "RrVulkan.h"

typedef struct Rr_Buffer Rr_Buffer;

Rr_Buffer Rr_CreateBuffer(VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped);
Rr_Buffer Rr_CreateMappedBuffer(VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags);
void Rr_DestroyBuffer(Rr_Buffer* Buffer, VmaAllocator Allocator);
