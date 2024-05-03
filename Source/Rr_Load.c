#include "Rr_Load.h"

#include <SDL3/SDL_thread.h>
#include <SDL_log.h>
#include <SDL3/SDL.h>

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

static Rr_UploadContext CreateUploadContext(
    const Rr_Renderer* Renderer,
    const VkCommandBuffer TransferCommandBuffer,
    const u32 bUnifiedQueue,
    const size_t StagingBufferSize)
{
    return (Rr_UploadContext){
        .bUnifiedQueue = bUnifiedQueue,
        .StagingBuffer = Rr_CreateStagingBuffer(Renderer, StagingBufferSize),
        .TransferCommandBuffer = TransferCommandBuffer,
    };
}

Rr_UploadContext Rr_CreateUploadContext(
    const Rr_Renderer* Renderer,
    const size_t ImageCount,
    const size_t BufferCount,
    const size_t StagingBufferSize,
    const u32 bUnifiedQueue)
{
    VkCommandBuffer TransferCommandBuffer;
    const VkCommandPool CommandPool = bUnifiedQueue ? Renderer->UnifiedQueue.TransientCommandPool : Renderer->TransferQueue.TransientCommandPool;

    const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(CommandPool, 1);
    vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &TransferCommandBuffer);
    vkBeginCommandBuffer(
        TransferCommandBuffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL });

    Rr_UploadContext UploadContext = CreateUploadContext(
        Renderer,
        TransferCommandBuffer,
        bUnifiedQueue,
        StagingBufferSize);

    if (!bUnifiedQueue)
    {
        Rr_ArrayInit(UploadContext.AcquireBarriers.ImageMemoryBarriersArray, VkImageMemoryBarrier, ImageCount);
        Rr_ArrayInit(UploadContext.AcquireBarriers.BufferMemoryBarriersArray, VkBufferMemoryBarrier, BufferCount);
    }

    return UploadContext;
}

void Rr_Upload(Rr_Renderer* Renderer, Rr_UploadContext* UploadContext, Rr_LoadingCallback LoadingCallback, const void* Userdata)
{
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

    vkEndCommandBuffer(UploadContext->TransferCommandBuffer);

    if (UploadContext->bUnifiedQueue)
    {
        SDL_LockMutex(Renderer->UnifiedQueue.Mutex);
        vkQueueSubmit(
            Renderer->UnifiedQueue.Handle,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = NULL,
                .commandBufferCount = 1,
                .pCommandBuffers = &UploadContext->TransferCommandBuffer,
                .pSignalSemaphores = NULL,
                .pWaitSemaphores = NULL,
                .signalSemaphoreCount = 0,
                .waitSemaphoreCount = 0,
                .pWaitDstStageMask = 0 },
            Fence);
        SDL_UnlockMutex(Renderer->UnifiedQueue.Mutex);
    }
    else
    {
        VkSemaphore TransferSemaphore;
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
            Renderer->TransferQueue.Handle,
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
            Fence);

        SDL_LockMutex(Renderer->UnifiedQueue.Mutex);
        const Rr_PendingLoad PendingLoad = {
            .Barriers = UploadContext->AcquireBarriers, /* Transfering ownership! */
            .LoadingCallback = LoadingCallback,
            .Userdata = Userdata,
            .Semaphore = TransferSemaphore /* Transfering ownership! */
        };
        Rr_ArrayPush(Renderer->UnifiedQueue.PendingLoads, &PendingLoad);
        SDL_UnlockMutex(Renderer->UnifiedQueue.Mutex);
    }

    vkWaitForFences(Renderer->Device, 1, &Fence, true, UINT64_MAX);
    vkDestroyFence(Renderer->Device, Fence, NULL);

    if (UploadContext->bUnifiedQueue && LoadingCallback)
    {
        LoadingCallback(Renderer, Userdata);
    }

    const VkCommandPool CommandPool = UploadContext->bUnifiedQueue ? Renderer->UnifiedQueue.TransientCommandPool : Renderer->TransferQueue.TransientCommandPool;
    vkFreeCommandBuffers(
        Renderer->Device,
        CommandPool,
        1,
        &UploadContext->TransferCommandBuffer);

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

static int SDLCALL Load(void* Data)
{
    Rr_LoadingContext* LoadingContext = Data;
    Rr_Renderer* Renderer = LoadingContext->Renderer;

    LoadingContext->Status = RR_LOAD_STATUS_LOADING;

    const size_t TaskCount = Rr_ArrayCount(LoadingContext->Tasks);
    size_t StagingBufferSize = 0;

    size_t ImageCount = 0;
    size_t BufferCount = 0;

    /* First pass: calculate staging buffer size. */
    for (size_t Index = 0; Index < TaskCount; ++Index)
    {
        const Rr_LoadTask* Task = &LoadingContext->Tasks[Index];
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                StagingBufferSize += Rr_GetImageSizePNG(&Task->Asset);
                ImageCount++;
            }
            break;
            case RR_LOAD_TYPE_MESH_FROM_OBJ:
            {
                StagingBufferSize += Rr_GetMeshBuffersSizeOBJ(&Task->Asset);
                BufferCount++;
                BufferCount++;
            }
            break;
            default:
                break;
        }
    }

    /* Create appropriate upload context. */
    Rr_UploadContext UploadContext = Rr_CreateUploadContext(
        Renderer,
        ImageCount,
        BufferCount,
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
    Rr_Upload(Renderer, &UploadContext, LoadingContext->LoadingCallback, LoadingContext->Userdata);

    if (LoadingContext->Semaphore)
    {
        SDL_DestroySemaphore(LoadingContext->Semaphore);
    }

    LoadingContext->Status = RR_LOAD_STATUS_READY;

    Rr_ArrayFree(LoadingContext->Tasks);
    Rr_Free(LoadingContext);

    return 0;
}

void Rr_LoadAsync(Rr_LoadingContext* LoadingContext, const Rr_LoadingCallback LoadingCallback, const void* Userdata)
{
    const size_t TaskCount = Rr_ArrayCount(LoadingContext->Tasks);
    if (TaskCount == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "Submitted Rr_LoadingContext has task count of 0!");
        return;
    }
    LoadingContext->Semaphore = SDL_CreateSemaphore(TaskCount);
    LoadingContext->LoadingCallback = LoadingCallback;
    LoadingContext->Userdata = Userdata;
    LoadingContext->bAsync = true;
    LoadingContext->Thread = SDL_CreateThread(Load, "lt", LoadingContext);
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
    return RR_LOAD_STATUS_READY;
}

f32 Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext)
{
    return (f32)SDL_GetSemaphoreValue(LoadingContext->Semaphore) / (f32)Rr_ArrayCount(LoadingContext->Tasks);
}
