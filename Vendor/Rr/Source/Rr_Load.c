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
    u32 bAsync;
    Rr_LoadStatus Status;
    Rr_Renderer* Renderer;
    Rr_LoadTask* Tasks;
    SDL_Thread* Thread;
    SDL_Semaphore* Semaphore;
    Rr_LoadingCallback Callback;
} Rr_LoadingContext;

static Rr_UploadContext CreateUploadContext(
    const Rr_Renderer* Renderer,
    const VkCommandBuffer TransferCommandBuffer,
    const VkCommandBuffer GraphicsCommandBuffer,
    const u32 bUnifiedQueue,
    const size_t StagingBufferSize)
{
    return (Rr_UploadContext){
        .bUnifiedQueue = bUnifiedQueue,
        .StagingBuffer = Rr_CreateStagingBuffer(Renderer, StagingBufferSize),
        .GraphicsCommandBuffer = GraphicsCommandBuffer,
        .TransferCommandBuffer = TransferCommandBuffer,
    };
}

Rr_UploadContext Rr_CreateUploadContext(const Rr_Renderer* Renderer, size_t StagingBufferSize, u32 bUnifiedQueue)
{
    VkCommandBuffer GraphicsCommandBuffer;
    VkCommandBuffer TransferCommandBuffer;

    const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Renderer->Graphics.TransientCommandPool, 1);
    vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &GraphicsCommandBuffer);
    vkBeginCommandBuffer(
        GraphicsCommandBuffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL });

    if (bUnifiedQueue)
    {
        TransferCommandBuffer = GraphicsCommandBuffer;
    }
    else
    {
        const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Renderer->Transfer.TransientCommandPool, 1);
        vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &TransferCommandBuffer);
        vkBeginCommandBuffer(
            TransferCommandBuffer,
            &(VkCommandBufferBeginInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = NULL,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = NULL });
    }

    return CreateUploadContext(
        Renderer,
        TransferCommandBuffer,
        GraphicsCommandBuffer,
        bUnifiedQueue,
        StagingBufferSize);
}

void Rr_Upload(const Rr_Renderer* Renderer, Rr_UploadContext* UploadContext)
{
    VkFence Fence;
    VkSemaphore TransferSemaphore;

    vkCreateFence(
        Renderer->Device,
        &(VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &Fence);

    vkEndCommandBuffer(UploadContext->GraphicsCommandBuffer);

    if (UploadContext->bUnifiedQueue)
    {
        SDL_LockMutex(Renderer->Graphics.Mutex);
        vkQueueSubmit(
            Renderer->Graphics.Queue,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = NULL,
                .commandBufferCount = 1,
                .pCommandBuffers = &UploadContext->GraphicsCommandBuffer,
                .pSignalSemaphores = NULL,
                .pWaitSemaphores = NULL,
                .signalSemaphoreCount = 0,
                .waitSemaphoreCount = 0,
                .pWaitDstStageMask = 0 },
            Fence);
        SDL_UnlockMutex(Renderer->Graphics.Mutex);
    }
    else
    {
        vkEndCommandBuffer(UploadContext->TransferCommandBuffer);

        vkCreateSemaphore(
            Renderer->Device,
            &(VkSemaphoreCreateInfo){
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
            },
            NULL,
            &TransferSemaphore);

        vkQueueSubmit(
            Renderer->Transfer.Queue,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = NULL,
                .commandBufferCount = 1,
                .pCommandBuffers = &UploadContext->TransferCommandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &TransferSemaphore,
                .waitSemaphoreCount = 0,
                .pWaitSemaphores = NULL,
                .pWaitDstStageMask = 0 },
            VK_NULL_HANDLE);

        SDL_LockMutex(Renderer->Graphics.Mutex);
        const VkPipelineStageFlags WaitStages[] = {
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
        };

        vkQueueSubmit(
            Renderer->Graphics.Queue,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = NULL,
                .commandBufferCount = 1,
                .pCommandBuffers = &UploadContext->GraphicsCommandBuffer,
                .signalSemaphoreCount = 0,
                .pSignalSemaphores = NULL,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &TransferSemaphore,
                .pWaitDstStageMask = WaitStages },
            Fence);
        SDL_UnlockMutex(Renderer->Graphics.Mutex);
    }

    vkWaitForFences(Renderer->Device, 1, &Fence, true, UINT64_MAX);
    vkDestroyFence(Renderer->Device, Fence, NULL);
    vkFreeCommandBuffers(
        Renderer->Device,
        Renderer->Graphics.TransientCommandPool,
        1,
        &UploadContext->GraphicsCommandBuffer);
    if (!UploadContext->bUnifiedQueue)
    {
        vkFreeCommandBuffers(
            Renderer->Device,
            Renderer->Transfer.TransientCommandPool,
            1,
            &UploadContext->TransferCommandBuffer);
        vkDestroySemaphore(Renderer->Device, TransferSemaphore, NULL);
    }

    Rr_DestroyStagingBuffer(Renderer, &UploadContext->StagingBuffer);
}

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

static void SDLCALL Load(Rr_LoadingContext* LoadingContext)
{
    const Rr_Renderer* Renderer = LoadingContext->Renderer;

    LoadingContext->Status = RR_LOAD_STATUS_LOADING;

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

    /* Create appropriate upload context. */
    Rr_UploadContext UploadContext = Rr_CreateUploadContext(
        Renderer,
        StagingBufferSize,
        Renderer->bUnifiedQueue || !LoadingContext->bAsync);

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
                    &UploadContext,
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
                    &UploadContext,
                    &Task->Asset);
            }
            break;
            default:
                break;
        }

        if (LoadingContext->Semaphore)
        {
            SDL_PostSemaphore(LoadingContext->Semaphore);
        }
    }

    /* Done recording; finally submit it. */
    Rr_Upload(Renderer, &UploadContext);

    if (LoadingContext->Semaphore)
    {
        SDL_DestroySemaphore(LoadingContext->Semaphore);
    }

    LoadingContext->Status = RR_LOAD_STATUS_READY;

    if (LoadingContext->Callback != NULL)
    {
        LoadingContext->Callback(LoadingContext->Renderer, LoadingContext->Status);
    }

    Rr_ArrayFree(LoadingContext->Tasks);
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
    LoadingContext->bAsync = true;
    LoadingContext->Thread = SDL_CreateThread((SDL_ThreadFunction)Load, "lt", LoadingContext);
    SDL_DetachThread(LoadingContext->Thread);
}

Rr_LoadStatus Rr_LoadImmediate(Rr_LoadingContext* LoadingContext)
{
    const size_t TaskCount = Rr_ArrayCount(LoadingContext->Tasks);
    if (TaskCount == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "Submitted Rr_LoadingContext has task count of 0!");
        return RR_LOAD_STATUS_NO_TASKS;
    }
    Load(LoadingContext);
    return LoadingContext->Status;
}

f32 Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext)
{
    return (f32)SDL_GetSemaphoreValue(LoadingContext->Semaphore) / (f32)Rr_ArrayCount(LoadingContext->Tasks);
}
