#pragma once

#include "RrDefines.h"
#include "RrVulkan.h"

typedef struct SAllocatedBuffer SAllocatedBuffer;

void AllocatedBuffer_Init(SAllocatedBuffer* Buffer, VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped);
void AllocatedBuffer_Cleanup(SAllocatedBuffer* Buffer, VmaAllocator Allocator);
