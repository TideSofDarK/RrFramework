#pragma once

#include <SDL3/SDL.h>

#include "Rr_Framework.h"
#include "Rr_Renderer.h"

#ifdef __cplusplus
extern "C"
{
#endif

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

struct Rr_PendingLoad
{
    Rr_AcquireBarriers Barriers;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
    VkSemaphore Semaphore;
};

struct Rr_LoadingContext
{
    bool bAsync;
    Rr_LoadStatus Status;
    Rr_Renderer* Renderer;
    Rr_LoadTask* Tasks;
    SDL_Thread* Thread;
    SDL_Semaphore* Semaphore;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
};

#ifdef __cplusplus
}
#endif
