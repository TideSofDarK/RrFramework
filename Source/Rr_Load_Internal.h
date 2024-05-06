#pragma once

#include "Rr_Framework.h"
#include "Rr_Asset.h"
#include "Rr_Vulkan.h"

typedef struct Rr_AcquireBarriers
{
    VkImageMemoryBarrier* ImageMemoryBarriersArray;
    VkBufferMemoryBarrier* BufferMemoryBarriersArray;
} Rr_AcquireBarriers;

struct Rr_UploadContext
{
    Rr_StagingBuffer* StagingBuffer;
    VkCommandBuffer TransferCommandBuffer;
    Rr_AcquireBarriers AcquireBarriers;
    bool bUseAcquireBarriers;
};

typedef enum Rr_LoadType
{
    RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
    RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ,
} Rr_LoadType;

struct Rr_LoadTask
{
    Rr_LoadType LoadType;
    Rr_Asset Asset;
    void** Out;
};