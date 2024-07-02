#pragma once

#include "Rr_Load.h"
#include "Rr_Vulkan.h"

struct Rr_UploadContext;

typedef struct Rr_Buffer Rr_Buffer;
struct Rr_Buffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
};

extern Rr_Buffer *Rr_CreateBuffer(
    Rr_App *App,
    uintptr_t Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    Rr_Bool bHostMapped);

extern Rr_Buffer *Rr_CreateDeviceVertexBuffer(Rr_App *App, uintptr_t Size);

extern Rr_Buffer *Rr_CreateDeviceUniformBuffer(Rr_App *App, uintptr_t Size);

extern Rr_Buffer *Rr_CreateMappedBuffer(
    Rr_App *App,
    uintptr_t Size,
    VkBufferUsageFlags UsageFlags);

extern Rr_Buffer *Rr_CreateMappedVertexBuffer(Rr_App *App, uintptr_t Size);

extern void Rr_DestroyBuffer(Rr_App *App, Rr_Buffer *Buffer);

typedef struct Rr_WriteBuffer Rr_WriteBuffer;
struct Rr_WriteBuffer
{
    Rr_Buffer *Buffer;
    VkDeviceSize Offset;
};

extern void Rr_UploadBufferAligned(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    void *Data,
    uintptr_t DataLength,
    uintptr_t Alignment);

extern void Rr_UploadBuffer(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    void *Data,
    uintptr_t DataLength);

extern void Rr_UploadToDeviceBufferImmediate(
    Rr_App *App,
    Rr_Buffer *DstBuffer,
    void *Data,
    uintptr_t Size);

extern void Rr_UploadToUniformBuffer(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_Buffer *DstBuffer,
    void *Data,
    uintptr_t DataLength);

extern void Rr_CopyToMappedUniformBuffer(
    Rr_App *App,
    Rr_Buffer *DstBuffer,
    void *Data,
    uintptr_t Size,
    VkDeviceSize *DstOffset);
