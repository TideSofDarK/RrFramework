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

Rr_Buffer Rr_CreateBuffer(const Rr_Renderer* Renderer, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped);
Rr_Buffer Rr_CreateDeviceVertexBuffer(const Rr_Renderer* Renderer, size_t Size);
Rr_Buffer Rr_CreateDeviceUniformBuffer(const Rr_Renderer* Renderer, size_t Size);
Rr_Buffer Rr_CreateMappedBuffer(const Rr_Renderer* Renderer, size_t Size, VkBufferUsageFlags UsageFlags);
Rr_Buffer Rr_CreateMappedVertexBuffer(const Rr_Renderer* Renderer, size_t Size);
void Rr_DestroyBuffer(const Rr_Renderer* Renderer, const Rr_Buffer* Buffer);
VkDeviceAddress Rr_GetBufferAddress(const Rr_Renderer* Renderer, const Rr_Buffer* Buffer);

void Rr_UploadToDeviceBufferImmediate(
    const Rr_Renderer* Renderer,
    const Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size);
void Rr_CopyToDeviceUniformBuffer(
    const Rr_Renderer* Renderer,
    VkCommandBuffer CommandBuffer,
    Rr_StagingBuffer* StagingBuffer,
    const Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size);
void Rr_CopyToMappedUniformBuffer(
    const Rr_Renderer* Renderer,
    const Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size,
    size_t* DstOffset);
