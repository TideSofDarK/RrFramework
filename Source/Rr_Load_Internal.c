#include "Rr_Load_Internal.h"

#include "Rr_UploadContext.h"
#include "Rr_Mesh_Internal.h"
#include "Rr_Image_Internal.h"
#include "Rr_App_Internal.h"

#include <SDL3/SDL.h>

static Rr_LoadSize Rr_CalculateLoadSize(
    Rr_LoadTask* Tasks,
    usize TaskCount,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_LoadSize LoadSize = { 0 };

    for (usize Index = 0; Index < TaskCount; ++Index)
    {
        Rr_LoadTask* Task = &Tasks[Index];
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                Rr_GetImageSizePNG(Task->AssetRef, Scratch.Arena, &LoadSize);
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ:
            {
                Rr_GetStaticMeshSizeOBJ(Task->AssetRef, Scratch.Arena, &LoadSize);
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_GLTF:
            {
                Rr_MeshGLTFOptions Options = Task->Options.MeshGLTF;
                Rr_GetStaticMeshSizeGLTF(Task->AssetRef, &Options.Loader, Options.MeshIndex, Scratch.Arena, &LoadSize);
            }
            break;
            default:
            {
                abort();
            }
        }
    }

    Rr_DestroyArenaScratch(Scratch);

    return LoadSize;
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
                    Task->AssetRef,
                    false,
                    Scratch.Arena);
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ:
            {
                *(Rr_StaticMesh**)Task->Out = Rr_CreateStaticMeshOBJ(
                    App,
                    UploadContext,
                    Task->AssetRef,
                    Scratch.Arena);
            }
            break;
            case RR_LOAD_TYPE_STATIC_MESH_FROM_GLTF:
            {
                Rr_MeshGLTFOptions Options = Task->Options.MeshGLTF;
                *(Rr_StaticMesh**)Task->Out = Rr_CreateStaticMeshGLTF(
                    App,
                    UploadContext,
                    Task->AssetRef,
                    &Options.Loader,
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

Rr_LoadResult Rr_LoadAsync_Internal(Rr_LoadingContext* LoadingContext, Rr_LoadAsyncContext LoadAsyncContext)
{
    Rr_App* App = LoadingContext->App;
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);

    const usize TaskCount = LoadingContext->TaskCount;
    Rr_LoadTask* Tasks = LoadingContext->Tasks;

    Rr_LoadSize LoadSize = Rr_CalculateLoadSize(Tasks, TaskCount, Scratch.Arena);

    /* Create appropriate upload context. */
    bool bUseTransferQueue = LoadAsyncContext.TransferCommandPool;
    VkCommandPool CommandPool = bUseTransferQueue ? LoadAsyncContext.TransferCommandPool : LoadAsyncContext.GraphicsCommandPool;

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
            .Buffer = Rr_CreateMappedBuffer(App, LoadSize.StagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT) }
    };

    if (bUseTransferQueue)
    {
        UploadContext.bUseAcquireBarriers = true;
        UploadContext.AcquireBarriers = (Rr_AcquireBarriers){
            .BufferMemoryBarriers = Rr_ArenaAllocCount(Scratch.Arena, sizeof(VkBufferMemoryBarrier), LoadSize.BufferCount),
            .ImageMemoryBarriers = Rr_ArenaAllocCount(Scratch.Arena, sizeof(VkImageMemoryBarrier), LoadSize.ImageCount),
        };
        UploadContext.ReleaseBarriers = (Rr_AcquireBarriers){
            .BufferMemoryBarriers = Rr_ArenaAllocCount(Scratch.Arena, sizeof(VkBufferMemoryBarrier), LoadSize.BufferCount),
            .ImageMemoryBarriers = Rr_ArenaAllocCount(Scratch.Arena, sizeof(VkImageMemoryBarrier), LoadSize.ImageCount),
        };
    }

    Rr_LoadResourcesFromTasks(App, Tasks, TaskCount, &UploadContext, LoadingContext->Semaphore, Scratch.Arena);

    SDL_Delay(300);

    if (!bUseTransferQueue)
    {
        vkEndCommandBuffer(TransferCommandBuffer);

        SDL_LockMutex(Renderer->GraphicsQueueMutex);

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
                .pWaitDstStageMask = 0 },
            LoadAsyncContext.Fence);

        SDL_UnlockMutex(Renderer->GraphicsQueueMutex);
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
                .pSignalSemaphores = &LoadAsyncContext.Semaphore,
                .waitSemaphoreCount = 0,
                .pWaitSemaphores = NULL,
                .pWaitDstStageMask = 0 },
            VK_NULL_HANDLE);

        /* Push acquire barriers to graphics queue. */
        {
            VkCommandBuffer GraphicsCommandBuffer = VK_NULL_HANDLE;

            VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(LoadAsyncContext.GraphicsCommandPool, 1);
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

            SDL_LockMutex(Renderer->GraphicsQueueMutex);

            vkQueueSubmit(
                Renderer->GraphicsQueue.Handle,
                1,
                &(VkSubmitInfo){
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .pNext = NULL,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &GraphicsCommandBuffer,
                    .signalSemaphoreCount = 0,
                    .pSignalSemaphores = NULL,
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &LoadAsyncContext.Semaphore,
                    .pWaitDstStageMask = &WaitDstStageMask },
                LoadAsyncContext.Fence);

            SDL_UnlockMutex(Renderer->GraphicsQueueMutex);
        }
    }

    vkWaitForFences(Renderer->Device, 1, &LoadAsyncContext.Fence, true, UINT64_MAX);

    Rr_DestroyBuffer(App, UploadContext.StagingBuffer.Buffer);

    SDL_LockMutex(App->SyncArena.Mutex);

    Rr_PendingLoad* PendingLoad = Rr_SlicePush(&Renderer->PendingLoadsSlice, &App->SyncArena.Arena);
    *PendingLoad = (Rr_PendingLoad){
        .LoadingCallback = LoadingContext->LoadingCallback,
        .Userdata = LoadingContext->Userdata,
    };

    SDL_UnlockMutex(App->SyncArena.Mutex);

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

    Rr_LoadSize LoadSize = Rr_CalculateLoadSize(Tasks, TaskCount, Scratch.Arena);

    VkCommandBuffer TransferCommandBuffer;
    VkCommandPool CommandPool = Renderer->GraphicsQueue.TransientCommandPool;

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
            .Buffer = Rr_CreateMappedBuffer(App, LoadSize.StagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT) }
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

    SDL_LockMutex(Renderer->GraphicsQueueMutex);

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
            .pWaitDstStageMask = 0 },
        Fence);

    SDL_UnlockMutex(Renderer->GraphicsQueueMutex);

    vkWaitForFences(Renderer->Device, 1, &Fence, true, UINT64_MAX);
    vkDestroyFence(Renderer->Device, Fence, NULL);

    Rr_DestroyBuffer(App, UploadContext.StagingBuffer.Buffer);

    vkFreeCommandBuffers(
        Renderer->Device,
        CommandPool,
        1,
        &TransferCommandBuffer);

    Rr_DestroyArenaScratch(Scratch);

    return RR_LOAD_RESULT_READY;
}

static Rr_LoadAsyncContext Rr_CreateLoadAsyncContext(Rr_Renderer* Renderer)
{
    Rr_LoadAsyncContext LoadAsyncContext = { 0 };

    vkCreateCommandPool(Renderer->Device, &(VkCommandPoolCreateInfo){
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                              .pNext = VK_NULL_HANDLE,
                                              .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                              .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
                                          },
        NULL, &LoadAsyncContext.GraphicsCommandPool);
    if (!Rr_IsUsingTransferQueue(Renderer))
    {
        vkCreateCommandPool(Renderer->Device, &(VkCommandPoolCreateInfo){
                                                  .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                  .pNext = VK_NULL_HANDLE,
                                                  .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                                  .queueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                                              },
            NULL, &LoadAsyncContext.TransferCommandPool);
    }
    vkCreateFence(
        Renderer->Device,
        &(VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &LoadAsyncContext.Fence);
    vkCreateSemaphore(
        Renderer->Device,
        &(VkSemaphoreCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &LoadAsyncContext.Semaphore);

    return LoadAsyncContext;
}

static void Rr_DestroyLoadAsyncContext(Rr_Renderer* Renderer, Rr_LoadAsyncContext* LoadAsyncContext)
{
    vkDestroyCommandPool(Renderer->Device, LoadAsyncContext->GraphicsCommandPool, NULL);
    if (LoadAsyncContext->TransferCommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(Renderer->Device, LoadAsyncContext->TransferCommandPool, NULL);
    }
    vkDestroyFence(Renderer->Device, LoadAsyncContext->Fence, NULL);
    vkDestroySemaphore(Renderer->Device, LoadAsyncContext->Semaphore, NULL);
}

static int SDLCALL Rr_LoadingThreadProc(void* Data)
{
    Rr_App* App = Data;
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_LoadingThread* LoadingThread = &App->LoadingThread;

    Rr_InitThreadScratch(RR_LOADING_THREAD_SCRATCH_SIZE);

    Rr_LoadAsyncContext LoadAsyncContext = Rr_CreateLoadAsyncContext(Renderer);

    usize CurrentLoadingContextIndex = 0;

    while (true)
    {
        SDL_WaitSemaphore(LoadingThread->Semaphore);

        if (SDL_AtomicGet(&App->bExit) == true)
        {
            break;
        }

        if (LoadingThread->LoadingContextsSlice.Length == 0)
        {
            continue;
        }

        Rr_LoadAsync_Internal(&LoadingThread->LoadingContextsSlice.Data[CurrentLoadingContextIndex], LoadAsyncContext);
        CurrentLoadingContextIndex++;

        vkResetCommandPool(Renderer->Device, LoadAsyncContext.GraphicsCommandPool, 0);
        if (LoadAsyncContext.TransferCommandPool != VK_NULL_HANDLE)
        {
            vkResetCommandPool(Renderer->Device, LoadAsyncContext.TransferCommandPool, 0);
        }
        vkResetFences(Renderer->Device, 1, &LoadAsyncContext.Fence);

        SDL_LockMutex(LoadingThread->Mutex);
        if (CurrentLoadingContextIndex >= LoadingThread->LoadingContextsSlice.Length)
        {
            Rr_ResetArena(&LoadingThread->Arena);
            CurrentLoadingContextIndex = 0;
            Rr_SliceClear(&LoadingThread->LoadingContextsSlice);
        }
        SDL_UnlockMutex(LoadingThread->Mutex);
    }

    Rr_DestroyLoadAsyncContext(Renderer, &LoadAsyncContext);

    SDL_CleanupTLS();

    return 0;
}

void Rr_InitLoadingThread(Rr_App* App)
{
    SDL_assert(App->LoadingThread.Handle == NULL);

    App->LoadingThread = (Rr_LoadingThread){
        .Semaphore = SDL_CreateSemaphore(0),
        .Mutex = SDL_CreateMutex(),
        .Arena = Rr_CreateArena(RR_LOADING_THREAD_ARENA_SIZE)
    };
    App->LoadingThread.Handle = SDL_CreateThread(Rr_LoadingThreadProc, "lt", App);
}

void Rr_CleanupLoadingThread(Rr_App* App)
{
    Rr_LoadingThread* LoadingThread = &App->LoadingThread;
    SDL_PostSemaphore(LoadingThread->Semaphore);
    SDL_WaitThread(LoadingThread->Handle, NULL);
    SDL_DestroySemaphore(LoadingThread->Semaphore);
    SDL_DestroyMutex(LoadingThread->Mutex);
    Rr_DestroyArena(&LoadingThread->Arena);
}
