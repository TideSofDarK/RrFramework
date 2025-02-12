#pragma once

#include <Rr/Rr_Load.h>

#include "Rr_Memory.h"
#include "Rr_Vulkan.h"

#include <SDL3/SDL_mutex.h>

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
    Rr_LoadCallback LoadingCallback;
    void *UserData;
};

struct Rr_LoadThread
{
    RR_SLICE(Rr_LoadContext) LoadContexts;

    SDL_Thread *Handle;
    SDL_Semaphore *Semaphore;
    SDL_Mutex *Mutex;

    Rr_App *App;

    Rr_Arena *Arena;
};

struct Rr_LoadContext
{
    struct Rr_App *App;
    SDL_Semaphore *Semaphore;
    Rr_LoadCallback LoadingCallback;
    void *UserData;
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
