#include "Rr_Load.h"

#include "Rr_Renderer.h"
#include "Rr_Memory.h"
#include "Rr_Asset.h"
#include "Rr_MeshTools.h"
#include "Rr_ImageTools.h"
#include "Rr_Buffer.h"
#include "Rr_Types.h"

#include <SDL3/SDL.h>

Rr_LoadTask Rr_LoadColorImageFromPNG(const Rr_Asset* Asset, Rr_Image** OutImage)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
        .Asset = *Asset,
        .Out = (void**)OutImage
    };
}

Rr_LoadTask Rr_LoadStaticMeshFromOBJ(const Rr_Asset* Asset, struct Rr_StaticMesh** OutStaticMesh)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ,
        .Asset = *Asset,
        .Out = (void**)OutStaticMesh
    };
}

Rr_LoadTask Rr_LoadStaticMeshFromGLTF(const Rr_Asset* Asset, usize MeshIndex, struct Rr_StaticMesh** OutStaticMesh)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_STATIC_MESH_FROM_GLTF,
        .Asset = *Asset,
        .Out = (void**)OutStaticMesh,
        .Options = (Rr_MeshGLTFOptions){
            .MeshIndex = MeshIndex }
    };
}

static usize Rr_CalculateLoadSize(
    Rr_LoadTask* Tasks,
    usize TaskCount,
    usize* OutImageCount,
    usize* OutBufferCount,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    usize StagingBufferSize = 0;
    for (usize Index = 0; Index < TaskCount; ++Index)
    {
        Rr_LoadTask* Task = &Tasks[Index];
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                StagingBufferSize += Rr_GetImageSizePNG(&Task->Asset, Scratch.Arena);
                *OutImageCount += 1;
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ:
            {
                StagingBufferSize += Rr_GetStaticMeshSizeOBJ(&Task->Asset, Scratch.Arena);
                *OutBufferCount += 2;
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_GLTF:
            {
                Rr_MeshGLTFOptions Options = Task->Options.MeshGLTF;
                StagingBufferSize += Rr_GetStaticMeshSizeGLTF(&Task->Asset, Options.MeshIndex, Scratch.Arena);
                *OutBufferCount += 2;
            }
            break;
            default:
            {
                abort();
            }
        }
    }

    Rr_DestroyArenaScratch(Scratch);

    return StagingBufferSize;
}

static void Rr_LoadResourcesFromTasks(
    Rr_App* App,
    Rr_LoadTask* Tasks,
    usize TaskCount,
    Rr_UploadContext* UploadContext,
    SDL_Semaphore* Semaphore,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    for (usize Index = 0; Index < TaskCount; ++Index)
    {
        Rr_LoadTask* Task = &Tasks[Index];
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                *(Rr_Image**)Task->Out = Rr_CreateColorImageFromPNG(
                    App,
                    UploadContext,
                    &Task->Asset,
                    false,
                    Scratch.Arena);
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ:
            {
                *(Rr_StaticMesh**)Task->Out = Rr_CreateStaticMeshOBJ(
                    App,
                    UploadContext,
                    &Task->Asset,
                    Scratch.Arena);
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_GLTF:
            {
                Rr_MeshGLTFOptions Options = Task->Options.MeshGLTF;
                *(Rr_StaticMesh**)Task->Out = Rr_CreateStaticMeshGLTF(
                    App,
                    UploadContext,
                    &Task->Asset,
                    Options.MeshIndex,
                    Scratch.Arena);
            }
            break;
            default:
            {
                abort();
            }
        }

        if (Semaphore)
        {
            SDL_PostSemaphore(Semaphore);
        }
    }

    Rr_DestroyArenaScratch(Scratch);
}

Rr_LoadResult Rr_LoadAsync_Internal(Rr_LoadingContext* LoadingContext, Rr_LoadCommandPools LoadCommandPools)
{
    Rr_App* App = LoadingContext->App;
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);

    const usize TaskCount = LoadingContext->TaskCount;
    Rr_LoadTask* Tasks = LoadingContext->Tasks;

    usize ImageCount = 0;
    usize BufferCount = 0;
    usize StagingBufferSize = Rr_CalculateLoadSize(Tasks, TaskCount, &ImageCount, &BufferCount, Scratch.Arena);

    /* Create appropriate upload context. */
    bool bUseTransferQueue = LoadCommandPools.TransferCommandPool;
    VkCommandPool CommandPool = bUseTransferQueue ? LoadCommandPools.TransferCommandPool : LoadCommandPools.GraphicsCommandPool;

    VkCommandBuffer TransferCommandBuffer;
    {
        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(CommandPool, 1);
        vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &TransferCommandBuffer);
    }

    vkBeginCommandBuffer(
        TransferCommandBuffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL });

    Rr_UploadContext UploadContext = {
        .TransferCommandBuffer = TransferCommandBuffer,
        .StagingBuffer = {
            .Buffer = Rr_CreateMappedBuffer(Renderer, StagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT) }
    };

    if (bUseTransferQueue)
    {
        UploadContext.bUseAcquireBarriers = true;
        UploadContext.AcquireBarriers = (Rr_AcquireBarriers){
            .BufferMemoryBarriers = Rr_ArenaAllocCount(Scratch.Arena, sizeof(VkBufferMemoryBarrier), BufferCount),
            .ImageMemoryBarriers = Rr_ArenaAllocCount(Scratch.Arena, sizeof(VkImageMemoryBarrier), ImageCount),
        };
        UploadContext.ReleaseBarriers = (Rr_AcquireBarriers){
            .BufferMemoryBarriers = Rr_ArenaAllocCount(Scratch.Arena, sizeof(VkBufferMemoryBarrier), BufferCount),
            .ImageMemoryBarriers = Rr_ArenaAllocCount(Scratch.Arena, sizeof(VkImageMemoryBarrier), ImageCount),
        };
    }

    Rr_LoadResourcesFromTasks(App, Tasks, TaskCount, &UploadContext, LoadingContext->Semaphore, Scratch.Arena);

    if (LoadingContext->bAsync)
    {
        SDL_Delay(300);
    }

    if (!bUseTransferQueue)
    {
        vkEndCommandBuffer(TransferCommandBuffer);

        SDL_LockMutex(Renderer->UnifiedQueueMutex);

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
            LoadCommandPools.Fence);

        SDL_UnlockMutex(Renderer->UnifiedQueueMutex);
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
                .pNext = NULL,
                .commandBufferCount = 1,
                .pCommandBuffers = &TransferCommandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &LoadCommandPools.Semaphore,
                .waitSemaphoreCount = 0,
                .pWaitSemaphores = NULL,
                .pWaitDstStageMask = 0 },
            VK_NULL_HANDLE);

        /* Push acquire barriers to graphics queue. */
        {
            VkCommandBuffer GraphicsCommandBuffer = VK_NULL_HANDLE;

            VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(LoadCommandPools.GraphicsCommandPool, 1);
            vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &GraphicsCommandBuffer);

            vkBeginCommandBuffer(
                GraphicsCommandBuffer,
                &(VkCommandBufferBeginInfo){
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .pNext = NULL,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                    .pInheritanceInfo = NULL });

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

            SDL_LockMutex(Renderer->UnifiedQueueMutex);

            vkQueueSubmit(
                Renderer->UnifiedQueue.Handle,
                1,
                &(VkSubmitInfo){
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .pNext = NULL,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &GraphicsCommandBuffer,
                    .signalSemaphoreCount = 0,
                    .pSignalSemaphores = NULL,
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &LoadCommandPools.Semaphore,
                    .pWaitDstStageMask = &WaitDstStageMask },
                LoadCommandPools.Fence);

            SDL_UnlockMutex(Renderer->UnifiedQueueMutex);
        }
    }

    vkWaitForFences(Renderer->Device, 1, &LoadCommandPools.Fence, true, UINT64_MAX);

    Rr_DestroyBuffer(Renderer, UploadContext.StagingBuffer.Buffer);

    if (LoadingContext->bAsync)
    {
        SDL_LockMutex(App->SyncArena.Mutex);

        Rr_PendingLoad* PendingLoad = Rr_SlicePush(&Renderer->PendingLoadsSlice, &App->SyncArena.Arena);
        *PendingLoad = (Rr_PendingLoad){
            .LoadingCallback = LoadingContext->LoadingCallback,
            .Userdata = LoadingContext->Userdata,
        };

        SDL_UnlockMutex(App->SyncArena.Mutex);
    }
    else
    {
        LoadingContext->LoadingCallback(App, LoadingContext->Userdata);
    }

    if (LoadingContext->Semaphore)
    {
        SDL_DestroySemaphore(LoadingContext->Semaphore);
    }

    Rr_DestroyArenaScratch(Scratch);

    return RR_LOAD_RESULT_READY;
}

Rr_LoadResult Rr_LoadImmediate_Internal(
    Rr_LoadingContext* LoadingContext)
{
    Rr_App* App = LoadingContext->App;
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);

    const usize TaskCount = LoadingContext->TaskCount;
    Rr_LoadTask* Tasks = LoadingContext->Tasks;

    usize ImageCount = 0;
    usize BufferCount = 0;
    usize StagingBufferSize = Rr_CalculateLoadSize(Tasks, TaskCount, &ImageCount, &BufferCount, Scratch.Arena);

    VkCommandBuffer TransferCommandBuffer;
    VkCommandPool CommandPool = Renderer->UnifiedQueue.TransientCommandPool;

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
        .StagingBuffer = {
            .Buffer = Rr_CreateMappedBuffer(Renderer, StagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT) }
    };

    Rr_LoadResourcesFromTasks(App, Tasks, TaskCount, &UploadContext, LoadingContext->Semaphore, Scratch.Arena);

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

    SDL_LockMutex(Renderer->UnifiedQueueMutex);

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

    SDL_UnlockMutex(Renderer->UnifiedQueueMutex);

    vkWaitForFences(Renderer->Device, 1, &Fence, true, UINT64_MAX);
    vkDestroyFence(Renderer->Device, Fence, NULL);

    Rr_DestroyBuffer(Renderer, UploadContext.StagingBuffer.Buffer);

    vkFreeCommandBuffers(
        Renderer->Device,
        CommandPool,
        1,
        &TransferCommandBuffer);

    Rr_DestroyArenaScratch(Scratch);

    return RR_LOAD_RESULT_READY;
}

Rr_LoadingContext* Rr_LoadAsync(
    Rr_App* App,
    Rr_LoadTask* Tasks,
    usize TaskCount,
    Rr_LoadingCallback LoadingCallback,
    const void* Userdata)
{
    if (TaskCount == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "Submitted zero tasks to load procedure!");
        return NULL;
    }

    usize AllocationSize = sizeof(Rr_LoadTask) * TaskCount + sizeof(Rr_LoadingContext);
    AllocationSize = Rr_Align(AllocationSize, RR_SAFE_ALIGNMENT);

    Rr_LoadingThread* LoadingThread = &App->LoadingThread;
    SDL_LockMutex(LoadingThread->Mutex);
    Rr_LoadTask* NewTasks = Rr_ArenaAllocCount(&LoadingThread->Arena, sizeof(Rr_LoadTask), TaskCount);
    SDL_memcpy(NewTasks, Tasks, sizeof(Rr_LoadTask) * TaskCount);
    Rr_LoadingContext* LoadingContext = Rr_SlicePush(&LoadingThread->LoadingContextsSlice, &LoadingThread->Arena);
    *LoadingContext = (Rr_LoadingContext){
        .Semaphore = SDL_CreateSemaphore(0),
        .LoadingCallback = LoadingCallback,
        .Userdata = Userdata,
        .bAsync = true,
        .App = App,
        .Tasks = NewTasks,
        .TaskCount = TaskCount
    };
    SDL_PostSemaphore(LoadingThread->Semaphore);
    SDL_UnlockMutex(LoadingThread->Mutex);

    /* @TODO: Create better handle! */
    return LoadingContext;
}

Rr_LoadResult Rr_LoadImmediate(
    Rr_App* App,
    Rr_LoadTask* Tasks,
    usize TaskCount)
{
    if (Tasks == 0)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "Submitted zero tasks to load procedure!");
        return RR_LOAD_RESULT_NO_TASKS;
    }
    Rr_LoadingContext LoadingContext = {
        .App = App,
        .Tasks = Tasks,
        .TaskCount = TaskCount,
    };
    return Rr_LoadImmediate_Internal(&LoadingContext);
}

void Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext, u32* OutCurrent, u32* OutTotal)
{
    if (OutCurrent != NULL)
    {
        *OutCurrent = SDL_GetSemaphoreValue(LoadingContext->Semaphore);
    }
    if (OutTotal != NULL)
    {
        *OutTotal = LoadingContext->TaskCount;
    }
}
