#pragma once

#include <Rr/Rr_Buffer.h>

#include "Rr_UploadContext.h"
#include "Rr_Vulkan.h"

typedef struct Rr_AllocatedBuffer Rr_AllocatedBuffer;
struct Rr_AllocatedBuffer
{
    VkBuffer Handle;
    VmaAllocationInfo AllocationInfo;
    VmaAllocation Allocation;
};

struct Rr_Buffer
{
    Rr_BufferFlags Flags;
    VkBufferUsageFlags Usage;
    size_t AllocatedBufferCount;
    Rr_AllocatedBuffer AllocatedBuffers[RR_MAX_FRAME_OVERLAP];
};

extern void Rr_UploadStagingBuffer(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Buffer *StagingBuffer,
    size_t StagingOffset,
    size_t StagingSize);

extern void Rr_UploadBuffer(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Data Data);

extern Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(
    Rr_Renderer *Renderer,
    Rr_Buffer *Buffer);
