/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Rr_Load.h"

#include "Rr_App.h"
#include "Rr_GLTF.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_Renderer.h"
#include "Rr_UploadContext.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_atomic.h>

#include <assert.h>

static void Rr_LoadResourcesFromTasks(
    Rr_LoadTask *Tasks,
    size_t TaskCount,
    Rr_UploadContext *UploadContext,
    SDL_Semaphore *Semaphore,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    for (size_t Index = 0; Index < TaskCount; ++Index)
    {
        Rr_LoadTask *Task = &Tasks[Index];
        void *Result = NULL;
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                Rr_Asset Asset = Rr_LoadAsset(Task->AssetRef);
                Result = Rr_CreateImage2DRGBA8FromPNG(
                    UploadContext,
                    Asset.Size,
                    Asset.Pointer);
            }
            break;
            case RR_LOAD_TYPE_GLTF_ASSET:
            {
                Rr_LoadGLTFOptions *Options = &Task->Options.GLTF;
                Result = Rr_CreateGLTFAsset(
                    Options->GLTFContext,
                    UploadContext,
                    Task->AssetRef,
                    Scratch.Arena);
            }
            break;
            default:
            {
                RR_ABORT("Unsupported load type!");
            }
        }

        if (Task->Out.Any != NULL)
        {
            *Task->Out.Any = Result;
        }
        else
        {
            RR_LOG("Loaded asset leaked, provide correct \"Out\" pointer!");
        }

        if (Semaphore)
        {
            SDL_SignalSemaphore(Semaphore);
        }
    }

    Rr_DestroyScratch(Scratch);
}

Rr_LoadTask Rr_LoadImageRGBA8FromPNG(
    Rr_AssetRef AssetRef,
    Rr_Image2D **OutImage)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
        .AssetRef = AssetRef,
        .Out = (void **)OutImage,
    };
}

static Rr_LoadResult Rr_ProcessLoadContext(
    Rr_LoadContext *LoadContext,
    Rr_LoadAsyncContext LoadAsyncContext)
{
    Rr_Device *Device = &gRenderer->Device;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    size_t TaskCount = LoadContext->TaskCount;
    Rr_LoadTask *Tasks = LoadContext->Tasks;

    /* Create appropriate upload context. */

    bool UseTransferQueue = Rr_IsUsingTransferQueue();
    VkCommandPool CommandPool = UseTransferQueue
                                    ? LoadAsyncContext.TransferCommandPool
                                    : LoadAsyncContext.GraphicsCommandPool;

    VkCommandBuffer TransferCommandBuffer;
    {
        Device->AllocateCommandBuffers(
            Device->Handle,
            &(VkCommandBufferAllocateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = CommandPool,
                .commandBufferCount = 1,
            },
            &TransferCommandBuffer);
    }

    Device->BeginCommandBuffer(
        TransferCommandBuffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        });

    Rr_UploadContext UploadContext = {
        .CommandBuffer = TransferCommandBuffer,
        .Arena = Scratch.Arena,
        .UseAcquireBarriers = UseTransferQueue,
    };

    Rr_LoadResourcesFromTasks(
        Tasks,
        TaskCount,
        &UploadContext,
        LoadContext->Semaphore,
        Scratch.Arena);

    SDL_Delay(300);

    if (!UseTransferQueue)
    {
        Device->EndCommandBuffer(TransferCommandBuffer);

        Rr_LockSpinlock(&gRenderer->GraphicsQueue.Lock);

        Device->QueueSubmit(
            gRenderer->GraphicsQueue.Handle,
            1,
            &(VkSubmitInfo){
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &TransferCommandBuffer,
            },
            LoadAsyncContext.Fence);

        Rr_UnlockSpinlock(&gRenderer->GraphicsQueue.Lock);
    }
    else
    {
        Device->CmdPipelineBarrier(
            TransferCommandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            NULL,
            (uint32_t)UploadContext.ReleaseBufferMemoryBarriers.Count,
            UploadContext.ReleaseBufferMemoryBarriers.Data,
            (uint32_t)UploadContext.ReleaseImageMemoryBarriers.Count,
            UploadContext.ReleaseImageMemoryBarriers.Data);

        Device->EndCommandBuffer(TransferCommandBuffer);

        Device->QueueSubmit(
            gRenderer->TransferQueue.Handle,
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

            Device->AllocateCommandBuffers(
                Device->Handle,
                &(VkCommandBufferAllocateInfo){
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .commandPool = LoadAsyncContext.GraphicsCommandPool,
                    .commandBufferCount = 1,
                },
                &GraphicsCommandBuffer);

            Device->BeginCommandBuffer(
                GraphicsCommandBuffer,
                &(VkCommandBufferBeginInfo){
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                });

            Device->CmdPipelineBarrier(
                GraphicsCommandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                NULL,
                (uint32_t)UploadContext.AcquireBufferMemoryBarriers.Count,
                UploadContext.AcquireBufferMemoryBarriers.Data,
                (uint32_t)UploadContext.AcquireImageMemoryBarriers.Count,
                UploadContext.AcquireImageMemoryBarriers.Data);

            Device->EndCommandBuffer(GraphicsCommandBuffer);

            VkPipelineStageFlags WaitDstStageMask =
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            Rr_LockSpinlock(&gRenderer->GraphicsQueue.Lock);

            Device->QueueSubmit(
                gRenderer->GraphicsQueue.Handle,
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

            Rr_UnlockSpinlock(&gRenderer->GraphicsQueue.Lock);
        }
    }

    Device->WaitForFences(
        Device->Handle,
        1,
        &LoadAsyncContext.Fence,
        true,
        UINT64_MAX);

    for (size_t Index = 0; Index < UploadContext.StagingBuffers.Count; ++Index)
    {
        Rr_DestroyBuffer(UploadContext.StagingBuffers.Data[Index]);
    }

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_PendingLoad *PendingLoad =
        RR_PUSH_INTO_ARRAY(&gRenderer->PendingLoadsArray, gRenderer->tArena);
    *PendingLoad = (Rr_PendingLoad){
        .LoadingCallback = LoadContext->LoadingCallback,
        .UserData = LoadContext->UserData,
    };

    Rr_UnlockSpinlock(&gRenderer->Lock);

    if (LoadContext->Semaphore)
    {
        SDL_DestroySemaphore(LoadContext->Semaphore);
        LoadContext->Semaphore = NULL;
    }

    Rr_DestroyScratch(Scratch);

    return RR_LOAD_RESULT_READY;
}

static void Rr_InitLoadAsyncContext(
    Rr_Renderer *Renderer,
    Rr_LoadAsyncContext *LoadAsyncContext)
{
    Rr_Device *Device = &Renderer->Device;

    Device->CreateCommandPool(
        Device->Handle,
        &(VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
        },
        NULL,
        &LoadAsyncContext->GraphicsCommandPool);
    Device->CreateFence(
        Device->Handle,
        &(VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &LoadAsyncContext->Fence);
    if (Rr_IsUsingTransferQueue())
    {
        Device->CreateCommandPool(
            Device->Handle,
            &(VkCommandPoolCreateInfo){
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext = VK_NULL_HANDLE,
                .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                .queueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
            },
            NULL,
            &LoadAsyncContext->TransferCommandPool);
        Device->CreateSemaphore(
            Device->Handle,
            &(VkSemaphoreCreateInfo){
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
            },
            NULL,
            &LoadAsyncContext->Semaphore);
    }
}

static void Rr_CleanupLoadAsyncContext(
    Rr_Renderer *Renderer,
    Rr_LoadAsyncContext *LoadAsyncContext)
{
    Rr_Device *Device = &Renderer->Device;

    Device->DestroyCommandPool(
        Device->Handle,
        LoadAsyncContext->GraphicsCommandPool,
        NULL);
    Device->DestroyFence(Device->Handle, LoadAsyncContext->Fence, NULL);
    if (LoadAsyncContext->TransferCommandPool != VK_NULL_HANDLE)
    {
        Device->DestroyCommandPool(
            Device->Handle,
            LoadAsyncContext->TransferCommandPool,
            NULL);
        Device->DestroySemaphore(
            Device->Handle,
            LoadAsyncContext->Semaphore,
            NULL);
    }
    RR_ZERO_PTR(LoadAsyncContext);
}

static int SDLCALL Rr_LoadThreadProc(void *UserData)
{
    Rr_LoadThread *LoadThread = UserData;

    Rr_Device *Device = &gRenderer->Device;

    Rr_InitScratch(RR_LOADING_THREAD_SCRATCH_SIZE);

    Rr_LoadAsyncContext LoadAsyncContext = { 0 };
    Rr_InitLoadAsyncContext(gRenderer, &LoadAsyncContext);

    size_t CurrentLoadingUIContextIndex = 0;

    while (true)
    {
        SDL_WaitSemaphore(LoadThread->Semaphore);

        if (Rr_GetAtomicInt(&gApp->QuitRequested) == true)
        {
            break;
        }

        if (LoadThread->LoadContexts.Count == 0)
        {
            continue;
        }

        Rr_ProcessLoadContext(
            &LoadThread->LoadContexts.Data[CurrentLoadingUIContextIndex],
            LoadAsyncContext);
        CurrentLoadingUIContextIndex++;

        Device->ResetCommandPool(
            Device->Handle,
            LoadAsyncContext.GraphicsCommandPool,
            0);
        if (LoadAsyncContext.TransferCommandPool != VK_NULL_HANDLE)
        {
            Device->ResetCommandPool(
                Device->Handle,
                LoadAsyncContext.TransferCommandPool,
                0);
        }
        Device->ResetFences(Device->Handle, 1, &LoadAsyncContext.Fence);

        SDL_LockMutex(LoadThread->Mutex);
        if (CurrentLoadingUIContextIndex >= LoadThread->LoadContexts.Count)
        {
            Rr_ResetArena(LoadThread->Arena);
            CurrentLoadingUIContextIndex = 0;
            RR_ZERO(LoadThread->LoadContexts);
        }
        SDL_UnlockMutex(LoadThread->Mutex);
    }

    Rr_CleanupLoadAsyncContext(gRenderer, &LoadAsyncContext);

    SDL_CleanupTLS();

    return 0;
}

Rr_LoadThread *Rr_CreateLoadThread(void)
{
    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_LoadThread *LoadThread = RR_ALLOC_TYPE(Arena, Rr_LoadThread);
    LoadThread->Mutex = SDL_CreateMutex();
    LoadThread->Semaphore = SDL_CreateSemaphore(0);
    LoadThread->Arena = Arena;
    LoadThread->Handle = SDL_CreateThread(Rr_LoadThreadProc, "lt", LoadThread);

    return LoadThread;
}

void Rr_DestroyLoadThread(Rr_LoadThread *LoadThread)
{
    SDL_SignalSemaphore(LoadThread->Semaphore);
    SDL_WaitThread(LoadThread->Handle, NULL);
    SDL_DestroySemaphore(LoadThread->Semaphore);
    SDL_DestroyMutex(LoadThread->Mutex);
    Rr_DestroyArena(LoadThread->Arena);
}

Rr_LoadTask Rr_LoadGLTFAssetTask(
    Rr_AssetRef AssetRef,
    Rr_GLTFContext *Context,
    Rr_GLTFAsset **Out)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_GLTF_ASSET,
        .AssetRef = AssetRef,
        .Options = {
            .GLTF = { .GLTFContext = Context, },
        },
        .Out = { .GLTFAsset = Out },
    };
}

Rr_LoadTask Rr_LoadImageRGBA8FromPNGTask(Rr_AssetRef AssetRef, Rr_Image2D **Out)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
        .AssetRef = AssetRef,
        .Out = { .Image = Out },
    };
}

Rr_LoadContext *Rr_LoadAsync(
    Rr_LoadThread *LoadThread,
    size_t TaskCount,
    Rr_LoadTask *Tasks,
    Rr_LoadCallback LoadCallback,
    void *UserData)
{
    if (TaskCount == 0)
    {
        RR_ABORT("Submitted zero tasks to load procedure!");
    }

    SDL_LockMutex(LoadThread->Mutex);
    Rr_LoadTask *NewTasks =
        RR_ALLOC_TYPE_COUNT(LoadThread->Arena, Rr_LoadTask, TaskCount);
    memcpy(NewTasks, Tasks, sizeof(Rr_LoadTask) * TaskCount);
    Rr_LoadContext *LoadingUIContext =
        RR_PUSH_INTO_ARRAY(&LoadThread->LoadContexts, LoadThread->Arena);
    *LoadingUIContext = (Rr_LoadContext){
        .Semaphore = SDL_CreateSemaphore(0),
        .LoadingCallback = LoadCallback,
        .UserData = UserData,
        .Tasks = NewTasks,
        .TaskCount = TaskCount,
    };
    SDL_SignalSemaphore(LoadThread->Semaphore);
    SDL_UnlockMutex(LoadThread->Mutex);

    return LoadingUIContext;
}

Rr_LoadResult Rr_LoadImmediate(size_t TaskCount, Rr_LoadTask *Tasks)
{
    assert(TaskCount > 0 && Tasks != NULL);

    Rr_Device *Device = &gRenderer->Device;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkCommandBuffer TransferCommandBuffer;
    VkCommandPool CommandPool = gRenderer->GraphicsQueue.TransientCommandPool;
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .commandPool = CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    Device->AllocateCommandBuffers(
        Device->Handle,
        &CommandBufferAllocateInfo,
        &TransferCommandBuffer);
    Device->BeginCommandBuffer(
        TransferCommandBuffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL,
        });

    Rr_UploadContext UploadContext = {
        .CommandBuffer = TransferCommandBuffer,
        .Arena = Scratch.Arena,
    };

    Rr_LoadResourcesFromTasks(
        Tasks,
        TaskCount,
        &UploadContext,
        NULL,
        Scratch.Arena);

    VkFence Fence;
    Device->CreateFence(
        Device->Handle,
        &(VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &Fence);

    Device->EndCommandBuffer(TransferCommandBuffer);

    Rr_LockSpinlock(&gRenderer->GraphicsQueue.Lock);

    Device->QueueSubmit(
        gRenderer->GraphicsQueue.Handle,
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

    Rr_UnlockSpinlock(&gRenderer->GraphicsQueue.Lock);

    Device->WaitForFences(Device->Handle, 1, &Fence, true, UINT64_MAX);
    Device->DestroyFence(Device->Handle, Fence, NULL);

    for (size_t Index = 0; Index < UploadContext.StagingBuffers.Count; ++Index)
    {
        Rr_DestroyBuffer(UploadContext.StagingBuffers.Data[Index]);
    }

    Device->FreeCommandBuffers(
        Device->Handle,
        CommandPool,
        1,
        &TransferCommandBuffer);

    Rr_DestroyScratch(Scratch);

    return RR_LOAD_RESULT_READY;
}

void Rr_GetLoadProgress(
    Rr_LoadContext *LoadContext,
    size_t *OutCurrent,
    size_t *OutTotal)
{
    if (OutCurrent != NULL)
    {
        if (LoadContext->Semaphore != NULL)
        {
            *OutCurrent = (size_t)SDL_GetSemaphoreValue(LoadContext->Semaphore);
        }
        *OutCurrent = LoadContext->TaskCount;
    }
    if (OutTotal != NULL)
    {
        *OutTotal = LoadContext->TaskCount;
    }
}
