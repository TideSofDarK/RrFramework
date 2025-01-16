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

typedef struct Rr_WriteBuffer Rr_WriteBuffer;
struct Rr_WriteBuffer
{
    Rr_Buffer *Buffer;
    VkDeviceSize Offset;
};

extern Rr_Buffer *Rr_CreateBuffer(
    Rr_App *App,
    size_t Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    bool CreateMapped);

extern Rr_Buffer *Rr_CreateDeviceVertexBuffer(Rr_App *App, size_t Size);

extern Rr_Buffer *Rr_CreateDeviceUniformBuffer(Rr_App *App, size_t Size);

extern Rr_Buffer *Rr_CreateMappedBuffer(Rr_App *App, size_t Size, VkBufferUsageFlags UsageFlags);

extern Rr_Buffer *Rr_CreateMappedVertexBuffer(Rr_App *App, size_t Size);

extern void Rr_DestroyBuffer(Rr_App *App, Rr_Buffer *Buffer);

extern void Rr_UploadBufferAligned(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_Buffer *DstBuffer,
    VkDeviceSize *DstOffset,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    Rr_Data Data,
    size_t Alignment);

extern void Rr_UploadBuffer(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    Rr_Data Data);

extern void Rr_UploadToDeviceBufferImmediate(Rr_App *App, Rr_Buffer *DstBuffer, Rr_Data Data);

extern void Rr_UploadToUniformBuffer(
    Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_Buffer *DstBuffer,
    VkDeviceSize *DstOffset,
    Rr_Data Data);

extern void Rr_CopyToMappedUniformBuffer(Rr_App *App, Rr_Buffer *DstBuffer, VkDeviceSize *DstOffset, Rr_Data Data);
