#pragma once

#include "Rr_Defines.h"
#include "Rr_Vulkan.h"

typedef struct Rr_Renderer Rr_Renderer;

typedef struct Rr_Buffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
} Rr_Buffer;

typedef struct Rr_StagingBuffer
{
    Rr_Buffer Buffer;
    size_t CurrentOffset;
} Rr_StagingBuffer;

Rr_Buffer Rr_CreateBuffer(VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped);
Rr_Buffer Rr_CreateMappedBuffer(VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags);
/* @TODO: pass Rr_Renderer* instead! */
void Rr_DestroyBuffer(Rr_Buffer* Buffer, VmaAllocator Allocator);
VkDeviceAddress Rr_GetBufferAddress(Rr_Renderer* Renderer, Rr_Buffer* Buffer);

void Rr_UploadToDeviceBufferImmediate(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size);
void Rr_UploadToDeviceBuffer(
    Rr_Renderer* Renderer,
    VkCommandBuffer CommandBuffer,
    Rr_StagingBuffer* StagingBuffer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size);
void Rr_CopyToMappedBuffer(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size,
    size_t* DstOffset);
