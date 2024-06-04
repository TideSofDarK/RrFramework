#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Memory.h"
#include "Rr_Load.h"

#include <SDL3/SDL_thread.h>

struct Rr_UploadContext;

typedef struct Rr_PendingLoad Rr_PendingLoad;
struct Rr_PendingLoad
{
    Rr_LoadingCallback LoadingCallback;
    const void* Userdata;
};

typedef struct Rr_LoadingThread Rr_LoadingThread;
struct Rr_LoadingThread
{
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

typedef struct Rr_LoadAsyncContext Rr_LoadAsyncContext;
struct Rr_LoadAsyncContext
{
    VkCommandPool GraphicsCommandPool;
    VkCommandPool TransferCommandPool;
    VkFence Fence;
    VkSemaphore Semaphore;
};

extern Rr_LoadResult Rr_LoadAsync_Internal(Rr_LoadingContext* LoadingContext, Rr_LoadAsyncContext LoadAsyncContext);
extern Rr_LoadResult Rr_LoadImmediate_Internal(Rr_LoadingContext* LoadingContext);

extern void Rr_InitLoadingThread(Rr_App* App);
extern void Rr_CleanupLoadingThread(Rr_App* App);
