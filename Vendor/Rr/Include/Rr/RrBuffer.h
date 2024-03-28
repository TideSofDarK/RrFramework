#pragma once

#include "RrDefines.h"
#include "RrVulkan.h"

typedef struct Rr_Buffer Rr_Buffer;

void Rr_InitBuffer(Rr_Buffer* Buffer, VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped);
void Rr_InitMappedBuffer(Rr_Buffer* Buffer, VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags);
void Rr_DestroyBuffer(Rr_Buffer* Buffer, VmaAllocator Allocator);
