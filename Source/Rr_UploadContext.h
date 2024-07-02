#pragma once

#include "Rr/Rr_Defines.h"
#include "Rr_Buffer.h"

typedef struct Rr_AcquireBarriers Rr_AcquireBarriers;
struct Rr_AcquireBarriers
{
    uintptr_t ImageMemoryBarrierCount;
    uintptr_t BufferMemoryBarrierCount;
    VkImageMemoryBarrier *ImageMemoryBarriers;
    VkBufferMemoryBarrier *BufferMemoryBarriers;
};

typedef struct Rr_UploadContext Rr_UploadContext;
struct Rr_UploadContext
{
    Rr_WriteBuffer StagingBuffer;
    VkCommandBuffer TransferCommandBuffer;
    Rr_AcquireBarriers ReleaseBarriers;
    Rr_AcquireBarriers AcquireBarriers;
    Rr_Bool bUseAcquireBarriers;
};
