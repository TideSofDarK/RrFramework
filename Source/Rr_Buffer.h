#pragma once

#include "Rr_Renderer.h"
#include "Rr_Vulkan.h"
#include "Rr_Load.h"

struct Rr_UploadContext;

typedef struct Rr_Buffer Rr_Buffer;
struct Rr_Buffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
};

extern Rr_Buffer* Rr_CreateBuffer(
    Rr_Renderer* Renderer,
    usize Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    bool bHostMapped);
extern Rr_Buffer* Rr_CreateDeviceVertexBuffer(Rr_Renderer* Renderer, usize Size);
extern Rr_Buffer* Rr_CreateDeviceUniformBuffer(Rr_Renderer* Renderer, usize Size);
extern Rr_Buffer* Rr_CreateMappedBuffer(Rr_Renderer* Renderer, usize Size, VkBufferUsageFlags UsageFlags);
extern Rr_Buffer* Rr_CreateMappedVertexBuffer(Rr_Renderer* Renderer, usize Size);
extern void Rr_DestroyBuffer(Rr_Renderer* Renderer, Rr_Buffer* Buffer);

typedef struct Rr_WriteBuffer Rr_WriteBuffer;
struct Rr_WriteBuffer
{
    Rr_Buffer* Buffer;
    VkDeviceSize Offset;
};

extern void Rr_UploadBufferAligned(
    Rr_Renderer* Renderer,
    struct Rr_UploadContext* UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    const void* Data,
    usize DataLength,
    usize Alignment);
extern void Rr_UploadBuffer(
    Rr_Renderer* Renderer,
    struct Rr_UploadContext* UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    const void* Data,
    usize DataLength);
extern void Rr_UploadToDeviceBufferImmediate(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    usize Size);
extern void Rr_UploadToUniformBuffer(
    Rr_Renderer* Renderer,
    struct Rr_UploadContext* UploadContext,
    Rr_Buffer* DstBuffer,
    const void* Data,
    usize DataLength);
extern void Rr_CopyToMappedUniformBuffer(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    usize Size,
    usize* DstOffset);
