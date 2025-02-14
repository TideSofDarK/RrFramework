#pragma once

#include "Rr_Vulkan.h"

typedef struct Rr_UploadContext Rr_UploadContext;
struct Rr_UploadContext
{
    VkCommandBuffer CommandBuffer;
    RR_SLICE(Rr_Buffer *) StagingBuffers;
    RR_SLICE(VkImageMemoryBarrier) ReleaseImageMemoryBarriers;
    RR_SLICE(VkImageMemoryBarrier) AcquireImageMemoryBarriers;
    RR_SLICE(VkBufferMemoryBarrier) ReleaseBufferMemoryBarriers;
    RR_SLICE(VkBufferMemoryBarrier) AcquireBufferMemoryBarriers;
    bool UseAcquireBarriers;
    Rr_Arena *Arena;
};
