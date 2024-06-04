#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Memory.h"
#include "Rr_Buffer.h"
#include "Rr_Load.h"

#include <SDL3/SDL_thread.h>

typedef struct Rr_PendingLoad Rr_PendingLoad;
struct Rr_PendingLoad
{
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
};

typedef struct Rr_AcquireBarriers Rr_AcquireBarriers;
struct Rr_AcquireBarriers
{
    usize ImageMemoryBarrierCount;
    usize BufferMemoryBarrierCount;
    VkImageMemoryBarrier* ImageMemoryBarriers;
    VkBufferMemoryBarrier* BufferMemoryBarriers;
};

typedef struct Rr_UploadContext Rr_UploadContext;
struct Rr_UploadContext
{
    Rr_WriteBuffer StagingBuffer;
    VkCommandBuffer TransferCommandBuffer;
    Rr_AcquireBarriers ReleaseBarriers;
    Rr_AcquireBarriers AcquireBarriers;
    bool bUseAcquireBarriers;
};

typedef struct Rr_LoadingThread Rr_LoadingThread;
struct Rr_LoadingThread
{
    Rr_Arena Arena;
    Rr_SliceType(Rr_LoadingContext) LoadingContextsSlice;
    SDL_Thread* Handle;
    SDL_Semaphore* Semaphore;
    SDL_Mutex* Mutex;
};

// typedef struct Rr_LoadingContext Rr_LoadingContext;
struct Rr_LoadingContext
{
    Rr_App* App;
    bool bAsync;
    SDL_Semaphore* Semaphore;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
    Rr_LoadTask* Tasks;
    usize TaskCount;
};

typedef struct Rr_LoadCommandPools Rr_LoadCommandPools;
struct Rr_LoadCommandPools
{
    VkCommandPool GraphicsCommandPool;
    VkCommandPool TransferCommandPool;
    VkFence Fence;
    VkSemaphore Semaphore;
};

extern Rr_LoadResult Rr_LoadAsync_Internal(Rr_LoadingContext* LoadingContext, Rr_LoadCommandPools LoadCommandPools);
extern Rr_LoadResult Rr_LoadImmediate_Internal(Rr_LoadingContext* LoadingContext);

extern void Rr_InitLoadingThread(Rr_App* App);
extern void Rr_CleanupLoadingThread(Rr_App* App);
