#pragma once

#include "Rr_Defines.h"
#include "Rr_Asset.h"
#include "Rr_Vulkan.h"

typedef struct Rr_StagingBuffer Rr_StagingBuffer;
typedef struct Rr_Renderer Rr_Renderer;

typedef struct Rr_AcquireBarriers
{
    VkImageMemoryBarrier* ImageMemoryBarriersArray;
    VkBufferMemoryBarrier* BufferMemoryBarriersArray;
} Rr_AcquireBarriers;

typedef struct Rr_UploadContext
{
    Rr_StagingBuffer* StagingBuffer;
    VkCommandBuffer TransferCommandBuffer;
    Rr_AcquireBarriers AcquireBarriers;
    bool bUseAcquireBarriers;
} Rr_UploadContext;

typedef enum Rr_LoadType
{
    RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
    RR_LOAD_TYPE_MESH_FROM_OBJ,
} Rr_LoadType;

typedef struct Rr_LoadTask
{
    Rr_LoadType LoadType;
    Rr_Asset Asset;
    void** Out;
} Rr_LoadTask;