#pragma once

#include "Rr_Vulkan.h"

typedef struct Rr_UploadContext Rr_UploadContext;
struct Rr_UploadContext
{
    VkCommandBuffer CommandBuffer;
    RR_ARRAY(struct Rr_Buffer *) StagingBuffers;
    RR_ARRAY(VkImageMemoryBarrier) ReleaseImageMemoryBarriers;
    RR_ARRAY(VkImageMemoryBarrier) AcquireImageMemoryBarriers;
    RR_ARRAY(VkBufferMemoryBarrier) ReleaseBufferMemoryBarriers;
    RR_ARRAY(VkBufferMemoryBarrier) AcquireBufferMemoryBarriers;
    bool UseAcquireBarriers;
    Rr_Arena *Arena;
};
