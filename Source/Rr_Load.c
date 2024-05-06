#include "Rr_Load.h"
#include "Rr_Load_Internal.h"

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
#include "Rr_Types.h"
#include "Rr_Image_Internal.h"

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

void Rr_LoadColorImageFromPNG(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_Image** OutImage)
{
    const Rr_LoadTask Task = {
        .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
        .Asset = *Asset,
        .Out = (void**)OutImage
    };
    Rr_ArrayPush(LoadingContext->Tasks, &Task);
}

void Rr_LoadMeshFromOBJ(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_StaticMesh** OutStaticMesh)
{
    const Rr_LoadTask Task = {
        .LoadType = RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ,
        .Asset = *Asset,
        .Out = (void**)OutStaticMesh
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
        Rr_LoadTask* Task = &LoadingContext->Tasks[Index];
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                StagingBufferSize += Rr_GetImageSizePNG(&Task->Asset);
                ImageCount++;
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ:
            {
                StagingBufferSize += Rr_GetStaticMeshSizeOBJ(&Task->Asset);
                BufferCount++;
                BufferCount++;
            }
            break;
            default:
                break;
        }
    }

    /* Create appropriate upload context. */
    bool bUseAcquireBarriers = !Renderer->bUnifiedQueue && LoadingContext->bAsync;
    VkCommandBuffer TransferCommandBuffer;
    VkCommandPool CommandPool = bUseAcquireBarriers
        ? Renderer->TransferQueue.TransientCommandPool
        : Renderer->UnifiedQueue.TransientCommandPool;

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(CommandPool, 1);
    vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &TransferCommandBuffer);
    vkBeginCommandBuffer(
        TransferCommandBuffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL });

    Rr_UploadContext UploadContext = {
        .TransferCommandBuffer = TransferCommandBuffer,
        .StagingBuffer = Rr_CreateStagingBuffer(Renderer, StagingBufferSize),
    };
    if (bUseAcquireBarriers)
    {
        UploadContext.bUseAcquireBarriers = true;
        /* @TODO: Dynamic allocation. */
        Rr_ArrayInit(UploadContext.AcquireBarriers.ImageMemoryBarriersArray, VkImageMemoryBarrier, ImageCount);
        Rr_ArrayInit(UploadContext.AcquireBarriers.BufferMemoryBarriersArray, VkBufferMemoryBarrier, BufferCount);
    }

    /* Second pass: record command buffers. */
    for (size_t Index = 0; Index < TaskCount; ++Index)
    {
        Rr_LoadTask* Task = &LoadingContext->Tasks[Index];
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                *(Rr_Image**)Task->Out = Rr_CreateColorImageFromPNG(
                    Renderer,
                    &UploadContext,
                    &Task->Asset,
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                    false,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ:
            {
                *(Rr_StaticMesh**)Task->Out = Rr_CreateStaticMeshFromOBJ(
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

    vkEndCommandBuffer(TransferCommandBuffer);

    SDL_Delay(300);

    /* Done recording; finally submit it. */
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

    if (!bUseAcquireBarriers)
    {
        SDL_LockMutex(Renderer->UnifiedQueue.Mutex);
        vkQueueSubmit(
            Renderer->UnifiedQueue.Handle,
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
                .pCommandBuffers = &TransferCommandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &TransferSemaphore,
                .waitSemaphoreCount = 0,
                .pWaitSemaphores = NULL,
                .pWaitDstStageMask = 0 },
            Fence);

        SDL_LockMutex(Renderer->UnifiedQueue.Mutex);
        const Rr_PendingLoad PendingLoad = {
            .Barriers = UploadContext.AcquireBarriers, /* Transferring ownership! */
            .LoadingCallback = LoadingContext->LoadingCallback,
            .Userdata = LoadingContext->Userdata,
            .Semaphore = TransferSemaphore /* Transferring ownership! */
        };
        Rr_ArrayPush(Renderer->UnifiedQueue.PendingLoads, &PendingLoad);
        SDL_UnlockMutex(Renderer->UnifiedQueue.Mutex);
    }

    vkWaitForFences(Renderer->Device, 1, &Fence, true, UINT64_MAX);
    vkDestroyFence(Renderer->Device, Fence, NULL);

    if (!bUseAcquireBarriers && LoadingContext->LoadingCallback)
    {
        LoadingContext->LoadingCallback(Renderer, LoadingContext->Userdata);
    }

    vkFreeCommandBuffers(
        Renderer->Device,
        CommandPool,
        1,
        &TransferCommandBuffer);

    Rr_DestroyStagingBuffer(Renderer, UploadContext.StagingBuffer);

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
    LoadingContext->Semaphore = SDL_CreateSemaphore(0);
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

void Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext, u32* OutCurrent, u32* OutTotal)
{
    if (OutCurrent != NULL)
    {
        *OutCurrent = SDL_GetSemaphoreValue(LoadingContext->Semaphore);
    }
    if (OutTotal != NULL)
    {
        *OutTotal = Rr_ArrayCount(LoadingContext->Tasks);
    }
}
