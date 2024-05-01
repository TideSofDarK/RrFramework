#include "Rr_Load.h"

#include <SDL3/SDL_thread.h>
#include <SDL_log.h>

#include "Rr_Renderer.h"
#include "Rr_Memory.h"
#include "Rr_Array.h"
#include "Rr_Image.h"
#include "Rr_Asset.h"
#include "Rr_Helpers.h"
#include "Rr_Mesh.h"

typedef enum Rr_LoadType
{
    RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
    RR_LOAD_TYPE_MESH_FROM_OBJ,
} Rr_LoadType;

typedef struct Rr_LoadTask
{
    Rr_LoadType LoadType;
    Rr_Asset Asset;
    void* Out;
} Rr_LoadTask;

typedef struct Rr_LoadingContext
{
    u32 bImmediate;
    Rr_LoadStatus Status;
    Rr_Renderer* Renderer;
    Rr_LoadTask* Tasks;
    SDL_Thread* Thread;
    SDL_Semaphore* Semaphore;
    Rr_LoadingCallback Callback;
    VkSemaphore TransferSemaphore;
    VkFence ReadyFence;
    VkCommandBuffer GraphicsCommandBuffer;
    VkCommandBuffer TransferCommandBuffer;
} Rr_LoadingContext;

Rr_LoadingContext* Rr_CreateLoadingContext(Rr_Renderer* Renderer, const size_t InitialTaskCount)
{
    Rr_LoadingContext* LoadingContext = Rr_Calloc(1, sizeof(Rr_LoadingContext));

    *LoadingContext = (Rr_LoadingContext){
        .Renderer = Renderer,
        .Status = RR_LOAD_STATUS_PENDING
    };

    Rr_ArrayInit(LoadingContext->Tasks, Rr_LoadTask, InitialTaskCount);

    return LoadingContext;
}

void Rr_LoadColorImageFromPNG(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_Image* OutImage)
{
    const Rr_LoadTask Task = {
        .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
        .Asset = *Asset,
        .Out = OutImage
    };
    Rr_ArrayPush(LoadingContext->Tasks, &Task);
}

void Rr_LoadMeshFromOBJ(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_MeshBuffers* OutMeshBuffers)
{
    const Rr_LoadTask Task = {
        .LoadType = RR_LOAD_TYPE_MESH_FROM_OBJ,
        .Asset = *Asset,
        .Out = OutMeshBuffers
    };
    Rr_ArrayPush(LoadingContext->Tasks, &Task);
}

static void BeginAsyncLoad(Rr_LoadingContext* LoadingContext)
{
    const Rr_Renderer* Renderer = LoadingContext->Renderer;

    const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Renderer->Graphics.TransientCommandPool, 1);
    vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &LoadingContext->GraphicsCommandBuffer);
    vkBeginCommandBuffer(
        LoadingContext->GraphicsCommandBuffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL });

    if (Renderer->bUnifiedQueue)
    {
        LoadingContext->TransferCommandBuffer = LoadingContext->GraphicsCommandBuffer;
    }
    else
    {
        const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Renderer->Transfer.TransientCommandPool, 1);
        vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &LoadingContext->TransferCommandBuffer);
        vkBeginCommandBuffer(
            LoadingContext->TransferCommandBuffer,
            &(VkCommandBufferBeginInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = NULL,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = NULL });
    }
}

static void EndAsyncLoad(Rr_LoadingContext* LoadingContext)
{
    Rr_Renderer* Renderer = LoadingContext->Renderer;

    vkCreateFence(
        Renderer->Device,
        &(VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &LoadingContext->ReadyFence);

    if (Renderer->bUnifiedQueue)
    {
        vkEndCommandBuffer(LoadingContext->GraphicsCommandBuffer);

        SDL_LockMutex(Renderer->Graphics.Mutex);
        vkQueueSubmit(
            Renderer->Graphics.Queue,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = NULL,
                .commandBufferCount = 1,
                .pCommandBuffers = &LoadingContext->GraphicsCommandBuffer,
                .pSignalSemaphores = NULL,
                .pWaitSemaphores = NULL,
                .signalSemaphoreCount = 0,
                .waitSemaphoreCount = 0,
                .pWaitDstStageMask = 0 },
            LoadingContext->ReadyFence);
        SDL_UnlockMutex(Renderer->Graphics.Mutex);
    }
    else
    {
        vkEndCommandBuffer(LoadingContext->TransferCommandBuffer);

        vkCreateSemaphore(
            Renderer->Device,
            &(VkSemaphoreCreateInfo){
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
            },
            NULL,
            &LoadingContext->TransferSemaphore);

        vkQueueSubmit(
            Renderer->Transfer.Queue,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = NULL,
                .commandBufferCount = 1,
                .pCommandBuffers = &LoadingContext->TransferCommandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &LoadingContext->TransferSemaphore,
                .waitSemaphoreCount = 0,
                .pWaitSemaphores = NULL,
                .pWaitDstStageMask = 0 },
            VK_NULL_HANDLE);

        vkEndCommandBuffer(LoadingContext->GraphicsCommandBuffer);

        SDL_LockMutex(Renderer->Graphics.Mutex);
        const VkPipelineStageFlagBits WaitStages[] = {
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
        };

        // VkSemaphore TransientSemaphore;
        // vkCreateSemaphore(
        //     Renderer->Device,
        //     &(VkSemaphoreCreateInfo){
        //         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        //         .pNext = NULL,
        //         .flags = 0,
        //     },
        //     NULL,
        //     &TransientSemaphore);

        vkQueueSubmit(
            Renderer->Graphics.Queue,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = NULL,
                .commandBufferCount = 1,
                .pCommandBuffers = &LoadingContext->GraphicsCommandBuffer,
                .signalSemaphoreCount = 0,
                .pSignalSemaphores = NULL,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &LoadingContext->TransferSemaphore,
                .pWaitDstStageMask = WaitStages },
            LoadingContext->ReadyFence);
        // Rr_ArrayPush(Renderer->Graphics.TransientSemaphoresArray, &TransientSemaphore);
        SDL_UnlockMutex(Renderer->Graphics.Mutex);
    }

    vkWaitForFences(Renderer->Device, 1, &LoadingContext->ReadyFence, true, UINT64_MAX);

    vkFreeCommandBuffers(Renderer->Device, Renderer->Graphics.TransientCommandPool, 1, &LoadingContext->GraphicsCommandBuffer);

    vkDestroyFence(Renderer->Device, LoadingContext->ReadyFence, NULL);

    if (!Renderer->bUnifiedQueue)
    {
        vkFreeCommandBuffers(Renderer->Device, Renderer->Transfer.TransientCommandPool, 1, &LoadingContext->TransferCommandBuffer);
        vkDestroySemaphore(Renderer->Device, LoadingContext->TransferSemaphore, NULL);
    }

    SDL_DetachThread(LoadingContext->Thread);
    SDL_DestroySemaphore(LoadingContext->Semaphore);
}

static void SDLCALL Load(Rr_LoadingContext* LoadingContext)
{
    const Rr_Renderer* Renderer = LoadingContext->Renderer;

    LoadingContext->Status = RR_LOAD_STATUS_LOADING;

    /* Select command buffers. */
    if (LoadingContext->bImmediate)
    {
        LoadingContext->GraphicsCommandBuffer = LoadingContext->TransferCommandBuffer = Rr_BeginImmediate(Renderer);
    }
    else
    {
        BeginAsyncLoad(LoadingContext);
    }

    const size_t TaskCount = Rr_ArrayCount(LoadingContext->Tasks);
    size_t StagingBufferSize = 0;

    /* First pass: calculate staging buffer size. */
    for (size_t Index = 0; Index < TaskCount; ++Index)
    {
        const Rr_LoadTask* Task = &LoadingContext->Tasks[Index];
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                StagingBufferSize += Rr_GetImageSizePNG(&Task->Asset);
            }
            break;
            case RR_LOAD_TYPE_MESH_FROM_OBJ:
            {
                StagingBufferSize += Rr_GetMeshBuffersSizeOBJ(&Task->Asset);
            }
            break;
            default:
                break;
        }
    }

    /* Create appropriate staging buffer. */
    Rr_StagingBuffer StagingBuffer = Rr_CreateStagingBuffer(Renderer, StagingBufferSize);

    /* Second pass: record command buffers. */
    for (size_t Index = 0; Index < TaskCount; ++Index)
    {
        const Rr_LoadTask* Task = &LoadingContext->Tasks[Index];
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                *(Rr_Image*)Task->Out = Rr_CreateColorImageFromPNG(
                    Renderer,
                    LoadingContext->GraphicsCommandBuffer,
                    LoadingContext->TransferCommandBuffer,
                    &StagingBuffer,
                    &Task->Asset,
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                    false,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            break;
            case RR_LOAD_TYPE_MESH_FROM_OBJ:
            {
                *(Rr_MeshBuffers*)Task->Out = Rr_CreateMeshFromOBJ(
                    Renderer,
                    LoadingContext->GraphicsCommandBuffer,
                    LoadingContext->TransferCommandBuffer,
                    &StagingBuffer,
                    &Task->Asset);
            }
            break;
            default:
                break;
        }

        if (!LoadingContext->bImmediate)
        {
            SDL_PostSemaphore(LoadingContext->Semaphore);
        }
    }

    /* Done recording; finally submit it. */
    if (LoadingContext->bImmediate)
    {
        Rr_EndImmediate(Renderer);
    }
    else
    {
        EndAsyncLoad(LoadingContext);
    }

    LoadingContext->Status = RR_LOAD_STATUS_READY;

    Rr_DestroyStagingBuffer(Renderer, &StagingBuffer);
    Rr_ArrayFree(LoadingContext->Tasks);

    if (LoadingContext->Callback != NULL)
    {
        LoadingContext->Callback(LoadingContext->Renderer, LoadingContext->Status);
    }

    Rr_Free(LoadingContext);
}

void Rr_LoadAsync(Rr_LoadingContext* LoadingContext, const Rr_LoadingCallback LoadingCallback)
{
    const size_t TaskCount = Rr_ArrayCount(LoadingContext->Tasks);
    if (TaskCount == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "Submitted Rr_LoadingContext has task count of 0!");
        return;
    }
    LoadingContext->Semaphore = SDL_CreateSemaphore(TaskCount);
    LoadingContext->Callback = LoadingCallback;
    LoadingContext->Thread = SDL_CreateThread((SDL_ThreadFunction)Load, "lt", LoadingContext);
}

Rr_LoadStatus Rr_LoadImmediate(Rr_LoadingContext* LoadingContext)
{
    const size_t TaskCount = Rr_ArrayCount(LoadingContext->Tasks);
    if (TaskCount == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "Submitted Rr_LoadingContext has task count of 0!");
        return RR_LOAD_STATUS_NO_TASKS;
    }
    LoadingContext->bImmediate = true;
    Load(LoadingContext);
    return LoadingContext->Status;
}

f32 Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext)
{
    return (f32)SDL_GetSemaphoreValue(LoadingContext->Semaphore) / (f32)Rr_ArrayCount(LoadingContext->Tasks);
}
