#pragma once

#include "Rr_Defines.h"
#include "Rr_Vulkan.h"
#include "Rr_Buffer.h"
#include "Rr_Renderer.h"

typedef struct Rr_LoadingContext Rr_LoadingContext;
typedef struct Rr_Asset Rr_Asset;
typedef struct Rr_Image Rr_Image;
typedef struct Rr_MeshBuffers Rr_MeshBuffers;
typedef struct Rr_LoadTask Rr_LoadTask;
typedef struct SDL_Thread SDL_Thread;
typedef struct SDL_Semaphore SDL_Semaphore;

typedef enum Rr_LoadStatus
{
    RR_LOAD_STATUS_READY,
    RR_LOAD_STATUS_PENDING,
    RR_LOAD_STATUS_LOADING,
    RR_LOAD_STATUS_NO_TASKS
} Rr_LoadStatus;

typedef void (*Rr_LoadingCallback)(Rr_Renderer* Renderer, const void* Userdata);

typedef struct Rr_AcquireBarriers
{
    VkImageMemoryBarrier* ImageMemoryBarriersArray;
    VkBufferMemoryBarrier* BufferMemoryBarriersArray;
} Rr_AcquireBarriers;

typedef struct Rr_UploadContext
{
    Rr_StagingBuffer StagingBuffer;
    VkCommandBuffer TransferCommandBuffer;
    Rr_AcquireBarriers AcquireBarriers;
    u32 bUnifiedQueue;
} Rr_UploadContext;

typedef struct Rr_PendingLoad
{
    Rr_AcquireBarriers Barriers;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
    VkSemaphore Semaphore;
} Rr_PendingLoad;

typedef struct Rr_LoadingContext
{
    u32 bAsync;
    Rr_LoadStatus Status;
    Rr_Renderer* Renderer;
    Rr_LoadTask* Tasks;
    SDL_Thread* Thread;
    SDL_Semaphore* Semaphore;
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
} Rr_LoadingContext;

Rr_UploadContext Rr_CreateUploadContext(
    const Rr_Renderer* Renderer,
    size_t ImageCount,
    size_t BufferCount,
    size_t StagingBufferSize,
    u32 bUnifiedQueue);
void Rr_Upload(Rr_Renderer* Renderer, Rr_UploadContext* UploadContext, Rr_LoadingCallback LoadingCallback, const void* Userdata);

Rr_LoadingContext* Rr_CreateLoadingContext(Rr_Renderer* Renderer, size_t InitialTaskCount);
void Rr_LoadColorImageFromPNG(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_Image* OutImage);
void Rr_LoadMeshFromOBJ(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_MeshBuffers* OutMeshBuffers);
void Rr_LoadAsync(Rr_LoadingContext* LoadingContext, Rr_LoadingCallback LoadingCallback, const void* Userdata);
Rr_LoadStatus Rr_LoadImmediate(Rr_LoadingContext* LoadingContext);
f32 Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext);
