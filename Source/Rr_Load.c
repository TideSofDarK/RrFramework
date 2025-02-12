#include "Rr_Load.h"

#include "Rr_App.h"
#include "Rr_GLTF.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_UploadContext.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_atomic.h>

#include <assert.h>

static Rr_LoadSize Rr_CalculateLoadSize(Rr_LoadTask *Tasks, size_t TaskCount, Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    Rr_LoadSize LoadSize = { 0 };

    for(size_t Index = 0; Index < TaskCount; ++Index)
    {
        Rr_LoadTask *Task = &Tasks[Index];
        switch(Task->LoadType)
        {
            case RR_LOAD_TYPE_PNG_RGBA8:
            {
                Rr_GetImageSizePNG(Task->AssetRef, Scratch.Arena, &LoadSize);
            }
            break;
            // case RR_LOAD_TYPE_OBJ_MESH:
            // {
            //     Rr_GetStaticMeshSizeOBJ(Task->AssetRef, Scratch.Arena, &LoadSize);
            // }
            // break;
            case RR_LOAD_TYPE_GLTF_SCENE:
            {
                Rr_LoadGLTFOptions *Options = &Task->Options.GLTF;
                GetGLTFSceneLoadSize(Task->AssetRef, Options->SceneIndex, &LoadSize, Scratch.Arena);
            }
            break;
            default:
            {
                RR_ABORT("Unsupported load type!");
            }
        }
    }

    Rr_DestroyScratch(Scratch);

    return LoadSize;
}

static void Rr_LoadResourcesFromTasks(
    Rr_App *App,
    Rr_LoadTask *Tasks,
    size_t TaskCount,
    Rr_UploadContext *UploadContext,
    SDL_Semaphore *Semaphore,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    for(size_t Index = 0; Index < TaskCount; ++Index)
    {
        Rr_LoadTask *Task = &Tasks[Index];
        void *Result = NULL;
        switch(Task->LoadType)
        {
            case RR_LOAD_TYPE_PNG_RGBA8:
            {
                Rr_Asset Asset = Rr_LoadAsset(Task->AssetRef);
                Result = Rr_CreateColorImageFromPNGMemory(App, UploadContext, Asset.Size, Asset.Pointer, false);
            }
            break;
            // case RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ:
            // {
            //     Result =
            //         Rr_CreateStaticMeshOBJ(App, UploadContext, Task->AssetRef, Scratch.Arena);
            // }
            // break;
            case RR_LOAD_TYPE_GLTF_SCENE:
            {
                Rr_LoadGLTFOptions *Options = &Task->Options.GLTF;
                // Result = Rr_CreateStaticMeshGLTF(
                //     App,
                //     UploadContext,
                //     Task->AssetRef,
                //     &Options.Loader,
                //     Options.MeshIndex,
                //     Scratch.Arena);
            }
            break;
            default:
            {
                RR_ABORT("Unsupported load type!");
            }
        }

        if(Task->Out != NULL)
        {
            *Task->Out = Result;
        }
        else
        {
            RR_LOG("Loaded asset leaked, provide correct \"Out\" argument!");
        }

        if(Semaphore)
        {
            SDL_SignalSemaphore(Semaphore);
        }
    }

    Rr_DestroyScratch(Scratch);
}

Rr_LoadTask Rr_LoadColorImageFromPNG(Rr_AssetRef AssetRef, Rr_Image **OutImage)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
        .AssetRef = AssetRef,
        .Out = (void **)OutImage,
    };
}

// Rr_LoadTask Rr_LoadStaticMeshFromOBJ(Rr_AssetRef AssetRef, Rr_StaticMesh **OutStaticMesh)
// {
//     return (Rr_LoadTask){
//         .LoadType = RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ,
//         .AssetRef = AssetRef,
//         .Out = (void **)OutStaticMesh,
//     };
// }

// Rr_LoadTask Rr_LoadStaticMeshFromGLTF(
//     Rr_AssetRef AssetRef,
//     Rr_GLTFLoader *Loader,
//     size_t MeshIndex,
//     Rr_StaticMesh **OutStaticMesh)
// {
//     return (Rr_LoadTask){
//         .LoadType = RR_LOAD_TYPE_STATIC_MESH_FROM_GLTF,
//         .AssetRef = AssetRef,
//         .Out = (void **)OutStaticMesh,
//         .Options = (Rr_MeshGLTFOptions){ .MeshIndex = MeshIndex, .Loader = *Loader },
//     };
// }

static Rr_LoadResult Rr_ProcessLoadContext(Rr_LoadContext *LoadContext, Rr_LoadAsyncContext LoadAsyncContext)
{
    Rr_App *App = LoadContext->App;
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    size_t TaskCount = LoadContext->TaskCount;
    Rr_LoadTask *Tasks = LoadContext->Tasks;

    Rr_LoadSize LoadSize = Rr_CalculateLoadSize(Tasks, TaskCount, Scratch.Arena);

    /* Create appropriate upload context. */

    bool UseTransferQueue = Rr_IsUsingTransferQueue(Renderer);
    /* @TODO: Simplify checks */
    VkCommandPool CommandPool =
        UseTransferQueue ? LoadAsyncContext.TransferCommandPool : LoadAsyncContext.GraphicsCommandPool;

    VkCommandBuffer TransferCommandBuffer;
    {
        vkAllocateCommandBuffers(
            Renderer->Device,
            &(VkCommandBufferAllocateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = CommandPool,
                .commandBufferCount = 1,
            },
            &TransferCommandBuffer);
    }

    vkBeginCommandBuffer(
        TransferCommandBuffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        });

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer_Internal(
        App,
        LoadSize.StagingBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO,
        true,
        false);

    Rr_UploadContext UploadContext = {
        .StagingBuffer = StagingBuffer->AllocatedBuffers,
        .TransferCommandBuffer = TransferCommandBuffer,
    };

    if(UseTransferQueue)
    {
        UploadContext.UseAcquireBarriers = true;
        UploadContext.AcquireBarriers = (Rr_AcquireBarriers){
            .BufferMemoryBarriers = RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkBufferMemoryBarrier, LoadSize.BufferCount),
            .ImageMemoryBarriers = RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkImageMemoryBarrier, LoadSize.ImageCount),
        };
        UploadContext.ReleaseBarriers = (Rr_AcquireBarriers){
            .BufferMemoryBarriers = RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkBufferMemoryBarrier, LoadSize.BufferCount),
            .ImageMemoryBarriers = RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkImageMemoryBarrier, LoadSize.ImageCount),
        };
    }

    Rr_LoadResourcesFromTasks(App, Tasks, TaskCount, &UploadContext, LoadContext->Semaphore, Scratch.Arena);

    SDL_Delay(300);

    if(!UseTransferQueue)
    {
        vkEndCommandBuffer(TransferCommandBuffer);

        Rr_LockSpinLock(&Renderer->GraphicsQueue.Lock);

        vkQueueSubmit(
            Renderer->GraphicsQueue.Handle,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &TransferCommandBuffer,
            },
            LoadAsyncContext.Fence);

        Rr_UnlockSpinLock(&Renderer->GraphicsQueue.Lock);
    }
    else
    {
        vkCmdPipelineBarrier(
            TransferCommandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            NULL,
            UploadContext.ReleaseBarriers.BufferMemoryBarrierCount,
            UploadContext.ReleaseBarriers.BufferMemoryBarriers,
            UploadContext.ReleaseBarriers.ImageMemoryBarrierCount,
            UploadContext.ReleaseBarriers.ImageMemoryBarriers);

        vkEndCommandBuffer(TransferCommandBuffer);

        vkQueueSubmit(
            Renderer->TransferQueue.Handle,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &TransferCommandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &LoadAsyncContext.Semaphore,
            },
            VK_NULL_HANDLE);

        /* Push acquire barriers to graphics queue. */
        {
            VkCommandBuffer GraphicsCommandBuffer = VK_NULL_HANDLE;

            vkAllocateCommandBuffers(
                Renderer->Device,
                &(VkCommandBufferAllocateInfo){
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .commandPool = LoadAsyncContext.GraphicsCommandPool,
                    .commandBufferCount = 1,
                },
                &GraphicsCommandBuffer);

            vkBeginCommandBuffer(
                GraphicsCommandBuffer,
                &(VkCommandBufferBeginInfo){
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                });

            vkCmdPipelineBarrier(
                GraphicsCommandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                NULL,
                UploadContext.AcquireBarriers.BufferMemoryBarrierCount,
                UploadContext.AcquireBarriers.BufferMemoryBarriers,
                UploadContext.AcquireBarriers.ImageMemoryBarrierCount,
                UploadContext.AcquireBarriers.ImageMemoryBarriers);

            vkEndCommandBuffer(GraphicsCommandBuffer);

            VkPipelineStageFlags WaitDstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            Rr_LockSpinLock(&Renderer->GraphicsQueue.Lock);

            vkQueueSubmit(
                Renderer->GraphicsQueue.Handle,
                1,
                &(VkSubmitInfo){
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &GraphicsCommandBuffer,
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &LoadAsyncContext.Semaphore,
                    .pWaitDstStageMask = &WaitDstStageMask,
                },
                LoadAsyncContext.Fence);

            Rr_UnlockSpinLock(&Renderer->GraphicsQueue.Lock);
        }
    }

    vkWaitForFences(Renderer->Device, 1, &LoadAsyncContext.Fence, true, UINT64_MAX);

    Rr_DestroyBuffer(App, StagingBuffer);

    Rr_LockSpinLock(&App->SyncArena.Lock);

    Rr_PendingLoad *PendingLoad = RR_PUSH_SLICE(&Renderer->PendingLoadsSlice, App->SyncArena.Arena);
    *PendingLoad = (Rr_PendingLoad){
        .LoadingCallback = LoadContext->LoadingCallback,
        .UserData = LoadContext->UserData,
    };

    Rr_UnlockSpinLock(&App->SyncArena.Lock);

    if(LoadContext->Semaphore)
    {
        SDL_DestroySemaphore(LoadContext->Semaphore);
        LoadContext->Semaphore = NULL;
    }

    Rr_DestroyScratch(Scratch);

    return RR_LOAD_RESULT_READY;
}

static void Rr_InitLoadAsyncContext(Rr_Renderer *Renderer, Rr_LoadAsyncContext *LoadAsyncContext)
{
    vkCreateCommandPool(
        Renderer->Device,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        },
        NULL,
        &LoadAsyncContext->GraphicsCommandPool);
    if(Rr_IsUsingTransferQueue(Renderer))
    {
        vkCreateCommandPool(
            Renderer->Device,
            &(VkCommandPoolCreateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = VK_NULL_HANDLE,
                .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                .queueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
            },
            NULL,
            &LoadAsyncContext->TransferCommandPool);
    }
    vkCreateFence(
        Renderer->Device,
        &(VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &LoadAsyncContext->Fence);
    vkCreateSemaphore(
        Renderer->Device,
        &(VkSemaphoreCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &LoadAsyncContext->Semaphore);
}

static void Rr_CleanupLoadAsyncContext(Rr_Renderer *Renderer, Rr_LoadAsyncContext *LoadAsyncContext)
{
    vkDestroyCommandPool(Renderer->Device, LoadAsyncContext->GraphicsCommandPool, NULL);
    if(LoadAsyncContext->TransferCommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(Renderer->Device, LoadAsyncContext->TransferCommandPool, NULL);
    }
    vkDestroyFence(Renderer->Device, LoadAsyncContext->Fence, NULL);
    vkDestroySemaphore(Renderer->Device, LoadAsyncContext->Semaphore, NULL);
    RR_ZERO_PTR(LoadAsyncContext);
}

static int SDLCALL Rr_LoadThreadProc(void *UserData)
{
    Rr_LoadThread *LoadThread = UserData;
    Rr_App *App = LoadThread->App;
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_InitScratch(RR_LOADING_THREAD_SCRATCH_SIZE);

    Rr_LoadAsyncContext LoadAsyncContext = { 0 };
    Rr_InitLoadAsyncContext(Renderer, &LoadAsyncContext);

    size_t CurrentLoadingContextIndex = 0;

    while(true)
    {
        SDL_WaitSemaphore(LoadThread->Semaphore);

        if(SDL_GetAtomicInt(&App->bExit) == true)
        {
            break;
        }

        if(LoadThread->LoadContexts.Count == 0)
        {
            continue;
        }

        Rr_ProcessLoadContext(&LoadThread->LoadContexts.Data[CurrentLoadingContextIndex], LoadAsyncContext);
        CurrentLoadingContextIndex++;

        vkResetCommandPool(Renderer->Device, LoadAsyncContext.GraphicsCommandPool, 0);
        if(LoadAsyncContext.TransferCommandPool != VK_NULL_HANDLE)
        {
            vkResetCommandPool(Renderer->Device, LoadAsyncContext.TransferCommandPool, 0);
        }
        vkResetFences(Renderer->Device, 1, &LoadAsyncContext.Fence);

        SDL_LockMutex(LoadThread->Mutex);
        if(CurrentLoadingContextIndex >= LoadThread->LoadContexts.Count)
        {
            Rr_ResetArena(LoadThread->Arena);
            CurrentLoadingContextIndex = 0;
            RR_ZERO(LoadThread->LoadContexts);
        }
        SDL_UnlockMutex(LoadThread->Mutex);
    }

    Rr_CleanupLoadAsyncContext(Renderer, &LoadAsyncContext);

    SDL_CleanupTLS();

    return 0;
}

Rr_LoadThread *Rr_CreateLoadThread(Rr_App *App)
{
    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_LoadThread *LoadThread = RR_ALLOC_TYPE(Arena, Rr_LoadThread);
    LoadThread->Mutex = SDL_CreateMutex();
    LoadThread->Semaphore = SDL_CreateSemaphore(0);
    LoadThread->Arena = Arena;
    LoadThread->App = App;
    LoadThread->Handle = SDL_CreateThread(Rr_LoadThreadProc, "lt", LoadThread);

    return LoadThread;
}

void Rr_DestroyLoadThread(Rr_App *App, Rr_LoadThread *LoadThread)
{
    SDL_SignalSemaphore(LoadThread->Semaphore);
    SDL_WaitThread(LoadThread->Handle, NULL);
    SDL_DestroySemaphore(LoadThread->Semaphore);
    SDL_DestroyMutex(LoadThread->Mutex);
    Rr_DestroyArena(LoadThread->Arena);
}

Rr_LoadContext *Rr_LoadAsync(
    Rr_LoadThread *LoadThread,
    size_t TaskCount,
    Rr_LoadTask *Tasks,
    Rr_LoadCallback LoadCallback,
    void *Userdata)
{
    if(TaskCount == 0)
    {
        RR_ABORT("Submitted zero tasks to load procedure!");
    }

    SDL_LockMutex(LoadThread->Mutex);
    Rr_LoadTask *NewTasks = RR_ALLOC_TYPE_COUNT(LoadThread->Arena, Rr_LoadTask, TaskCount);
    memcpy(NewTasks, Tasks, sizeof(Rr_LoadTask) * TaskCount);
    Rr_LoadContext *LoadingContext = RR_PUSH_SLICE(&LoadThread->LoadContexts, LoadThread->Arena);
    *LoadingContext = (Rr_LoadContext){
        .Semaphore = SDL_CreateSemaphore(0),
        .LoadingCallback = LoadCallback,
        .UserData = Userdata,
        .App = LoadThread->App,
        .Tasks = NewTasks,
        .TaskCount = TaskCount,
    };
    SDL_SignalSemaphore(LoadThread->Semaphore);
    SDL_UnlockMutex(LoadThread->Mutex);

    return LoadingContext;
}

Rr_LoadResult Rr_LoadImmediate(Rr_App *App, size_t TaskCount, Rr_LoadTask *Tasks)
{
    assert(TaskCount > 0 && Tasks != NULL);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_LoadSize LoadSize = Rr_CalculateLoadSize(Tasks, TaskCount, Scratch.Arena);

    VkCommandBuffer TransferCommandBuffer;
    VkCommandPool CommandPool = Renderer->GraphicsQueue.TransientCommandPool;
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .commandPool = CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &TransferCommandBuffer);
    vkBeginCommandBuffer(
        TransferCommandBuffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL,
        });

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer_Internal(
        App,
        LoadSize.StagingBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO,
        true,
        false);

    Rr_UploadContext UploadContext = {
        .StagingBuffer = StagingBuffer->AllocatedBuffers,
        .TransferCommandBuffer = TransferCommandBuffer,
    };

    Rr_LoadResourcesFromTasks(App, Tasks, TaskCount, &UploadContext, NULL, Scratch.Arena);

    VkFence Fence;
    vkCreateFence(
        Renderer->Device,
        &(VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &Fence);

    vkEndCommandBuffer(TransferCommandBuffer);

    Rr_LockSpinLock(&Renderer->GraphicsQueue.Lock);

    vkQueueSubmit(
        Renderer->GraphicsQueue.Handle,
        1,
        &(VkSubmitInfo){
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .commandBufferCount = 1,
            .pCommandBuffers = &TransferCommandBuffer,
            .pSignalSemaphores = NULL,
            .pWaitSemaphores = NULL,
            .signalSemaphoreCount = 0,
            .waitSemaphoreCount = 0,
            .pWaitDstStageMask = 0,
        },
        Fence);

    Rr_UnlockSpinLock(&Renderer->GraphicsQueue.Lock);

    vkWaitForFences(Renderer->Device, 1, &Fence, true, UINT64_MAX);
    vkDestroyFence(Renderer->Device, Fence, NULL);

    Rr_DestroyBuffer(App, StagingBuffer);

    vkFreeCommandBuffers(Renderer->Device, CommandPool, 1, &TransferCommandBuffer);

    Rr_DestroyScratch(Scratch);

    return RR_LOAD_RESULT_READY;
}

void Rr_GetLoadProgress(Rr_LoadContext *LoadContext, uint32_t *OutCurrent, uint32_t *OutTotal)
{
    if(OutCurrent != NULL)
    {
        if(LoadContext->Semaphore != NULL)
        {
            *OutCurrent = SDL_GetSemaphoreValue(LoadContext->Semaphore);
        }
        *OutCurrent = LoadContext->TaskCount;
    }
    if(OutTotal != NULL)
    {
        *OutTotal = LoadContext->TaskCount;
    }
}
