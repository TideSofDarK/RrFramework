#pragma once

#include "Rr/Rr_Load.h"
#include "Rr_Memory.h"

#include <SDL3/SDL_thread.h>

#include <volk.h>

typedef struct Rr_LoadSize Rr_LoadSize;
struct Rr_LoadSize
{
    size_t StagingBufferSize;
    size_t BufferCount;
    size_t ImageCount;
};

typedef struct Rr_PendingLoad Rr_PendingLoad;
struct Rr_PendingLoad
{
    Rr_LoadingCallback LoadingCallback;
    void *Userdata;
};

typedef struct Rr_LoadingThread Rr_LoadingThread;
struct Rr_LoadingThread
{
    Rr_Arena Arena;
    Rr_SliceType(Rr_LoadingContext) LoadingContextsSlice;
    SDL_Thread *Handle;
    SDL_Semaphore *Semaphore;
    SDL_Mutex *Mutex;
};

// typedef struct Rr_LoadingContext Rr_LoadingContext;
struct Rr_LoadingContext
{
    struct Rr_App *App;
    SDL_Semaphore *Semaphore;
    Rr_LoadingCallback LoadingCallback;
    void *Userdata;
    Rr_LoadTask *Tasks;
    size_t TaskCount;
};

typedef struct Rr_LoadAsyncContext Rr_LoadAsyncContext;
struct Rr_LoadAsyncContext
{
    VkCommandPool GraphicsCommandPool;
    VkCommandPool TransferCommandPool;
    VkFence Fence;
    VkSemaphore Semaphore;
};

extern Rr_LoadResult Rr_LoadAsync_Internal(
    Rr_LoadingContext *LoadingContext,
    Rr_LoadAsyncContext LoadAsyncContext);

extern Rr_LoadResult Rr_LoadImmediate_Internal(
    Rr_LoadingContext *LoadingContext);

extern void Rr_InitLoadingThread(Rr_App *App);

extern void Rr_CleanupLoadingThread(Rr_App *App);
