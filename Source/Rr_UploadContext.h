#pragma once

#include "Rr_Buffer.h"

typedef struct Rr_AcquireBarriers Rr_AcquireBarriers;
struct Rr_AcquireBarriers
{
    size_t ImageMemoryBarrierCount;
    VkImageMemoryBarrier *ImageMemoryBarriers;
    size_t BufferMemoryBarrierCount;
    VkBufferMemoryBarrier *BufferMemoryBarriers;
};

typedef struct Rr_UploadContext Rr_UploadContext;
struct Rr_UploadContext
{
    size_t StagingBufferOffset;
    Rr_AllocatedBuffer *StagingBuffer;
    VkCommandBuffer TransferCommandBuffer;
    Rr_AcquireBarriers ReleaseBarriers;
    Rr_AcquireBarriers AcquireBarriers;
    bool UseAcquireBarriers;
};
