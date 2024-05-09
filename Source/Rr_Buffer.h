#pragma once

#include "Rr_Renderer.h"
#include "Rr_Vulkan.h"
#include "Rr_Load.h"

typedef struct Rr_Buffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
} Rr_Buffer;

typedef struct Rr_StagingBuffer
{
    Rr_Buffer* Buffer;
    size_t CurrentOffset;
} Rr_StagingBuffer;

void Rr_UploadBufferAligned(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    const void* Data,
    size_t DataLength,
    size_t Alignment);
void Rr_UploadBuffer(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    const void* Data,
    size_t DataLength);
void Rr_UploadToDeviceBufferImmediate(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size);
void Rr_UploadToUniformBuffer(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t DataLength);
void Rr_CopyToMappedUniformBuffer(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size,
    size_t* DstOffset);

Rr_Buffer* Rr_CreateBuffer(
    Rr_Renderer* Renderer,
    size_t Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    bool bHostMapped);
Rr_Buffer* Rr_CreateDeviceVertexBuffer(Rr_Renderer* Renderer, size_t Size);
Rr_Buffer* Rr_CreateDeviceUniformBuffer(Rr_Renderer* Renderer, size_t Size);
Rr_Buffer* Rr_CreateMappedBuffer(Rr_Renderer* Renderer, size_t Size, VkBufferUsageFlags UsageFlags);
Rr_Buffer* Rr_CreateMappedVertexBuffer(Rr_Renderer* Renderer, size_t Size);
void Rr_DestroyBuffer(Rr_Renderer* Renderer, Rr_Buffer* Buffer);

Rr_StagingBuffer* Rr_CreateStagingBuffer(Rr_Renderer* Renderer, size_t Size);
void Rr_DestroyStagingBuffer(Rr_Renderer* Renderer, Rr_StagingBuffer* StagingBuffer);
