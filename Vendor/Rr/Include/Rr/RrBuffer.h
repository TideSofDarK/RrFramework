#pragma once

#include "RrDefines.h"
#include "RrVulkan.h"

typedef struct Rr_Buffer Rr_Buffer;

void AllocatedBuffer_Init(Rr_Buffer* Buffer, VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped);
void AllocatedBuffer_Cleanup(Rr_Buffer* Buffer, VmaAllocator Allocator);
