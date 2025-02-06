#pragma once

#include <Rr/Rr_Buffer.h>

#include "Rr_Load.h"
#include "Rr_Vulkan.h"

struct Rr_UploadContext;

typedef struct Rr_AllocatedBuffer Rr_AllocatedBuffer;
struct Rr_AllocatedBuffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
};

struct Rr_Buffer
{
    VkBufferUsageFlags Usage;
    size_t AllocatedBufferCount;
    Rr_AllocatedBuffer AllocatedBuffers[RR_MAX_FRAME_OVERLAP];
};

typedef struct Rr_WriteBuffer Rr_WriteBuffer;
struct Rr_WriteBuffer
{
    Rr_Buffer *Buffer;
    VkDeviceSize Offset;
};

extern Rr_Buffer *Rr_CreateBuffer_Internal(
    Rr_App *App,
    size_t Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    bool CreateMapped,
    bool Buffered);

extern Rr_Buffer *Rr_CreateDeviceUniformBuffer(Rr_App *App, size_t Size);

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

// extern void Rr_UploadToUniformBuffer(
//     Rr_App *App,
//     struct Rr_UploadContext *UploadContext,
//     Rr_Buffer *DstBuffer,
//     VkDeviceSize *DstOffset,
//     Rr_Data Data);

// extern void Rr_CopyToMappedUniformBuffer(Rr_App *App, Rr_Buffer *DstBuffer, VkDeviceSize *DstOffset, Rr_Data Data);

extern Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(Rr_App *App, Rr_Buffer *Buffer);
