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
    Rr_Renderer* Renderer;
    Rr_LoadTask* Tasks;
    SDL_Thread* Thread;
    SDL_Semaphore* Semaphore;
    Rr_LoadingCallback Callback;
} Rr_LoadingContext;

Rr_LoadingContext* Rr_CreateLoadingContext(Rr_Renderer* Renderer, const size_t InitialTaskCount)
{
    Rr_LoadingContext* LoadingContext = Rr_Calloc(1, sizeof(Rr_LoadingContext));

    LoadingContext->Renderer = Renderer;

    Rr_ArrayInit(LoadingContext->Tasks, Rr_LoadTask, InitialTaskCount);

    return LoadingContext;
}

void Rr_LoadRGBA8FromPNG(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, Rr_Image* OutImage)
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

    /* Allocate command buffer. */
    VkCommandBuffer CommandBuffer;
    const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = GetCommandBufferAllocateInfo(Renderer->Transfer.CommandPool, 1);
    vkAllocateCommandBuffers(Renderer->Device, &CommandBufferAllocateInfo, &CommandBuffer);

    const size_t TaskCount = Rr_ArrayCount(LoadingContext->Tasks);
    for (size_t Index = 0; Index < TaskCount; ++Index)
    {
        Rr_LoadTask* Task = &LoadingContext->Tasks[Index];
        switch (Task->LoadType)
        {
            case RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG:
            {
                *(Rr_Image*)Task->Out = Rr_CreateImageFromPNG(
                    Renderer,
                    &Task->Asset,
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                    false,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            break;
            case RR_LOAD_TYPE_MESH_FROM_OBJ:
            {
                *(Rr_MeshBuffers*)Task->Out = Rr_CreateMeshFromOBJ(Renderer, CommandBuffer, &Task->Asset);
            }
            break;
            default:
                break;
        }
        SDL_PostSemaphore(LoadingContext->Semaphore);
    }

    VkSubmitInfo SubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &CommandBuffer,
        .pSignalSemaphores = NULL,
        .pWaitSemaphores = NULL,
        .signalSemaphoreCount = 0,
        .waitSemaphoreCount = 0,
        .pWaitDstStageMask = 0
    };

    vkQueueSubmit(Renderer->Transfer.Handle, 1, &SubmitInfo, VK_NULL_HANDLE);

    vkFreeCommandBuffers(Renderer->Device, Renderer->Transfer.CommandPool, 1, &CommandBuffer);

    Rr_ArrayFree(LoadingContext->Tasks);

    LoadingContext->Callback(LoadingContext->Renderer, 0);

    SDL_DetachThread(LoadingContext->Thread);
    SDL_DestroySemaphore(LoadingContext->Semaphore);

    Rr_Free(LoadingContext);

    // /* Marble */
    // Rr_ExternAsset(MarbleOBJ);
    // MarbleMesh = Rr_CreateMeshFromOBJ(Renderer, &MarbleOBJ);
    //
    // Rr_ExternAsset(MarbleDiffusePNG);
    // MarbleDiffuse = Rr_CreateImageFromPNG(&MarbleDiffusePNG, Renderer, VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    //
    // Rr_ExternAsset(MarbleSpecularPNG);
    // MarbleSpecular = Rr_CreateImageFromPNG(&MarbleSpecularPNG, Renderer, VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    //
    // Rr_Image* MarbleTextures[2] = { &MarbleDiffuse, &MarbleSpecular };
    // MarbleMaterial = Rr_CreateMaterial(Renderer, MarbleTextures, 2);
}

void Rr_Load(Rr_LoadingContext* LoadingContext, const Rr_LoadingCallback LoadingCallback)
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

f32 Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext)
{
    return (f32)SDL_GetSemaphoreValue(LoadingContext->Semaphore) / (f32)Rr_ArrayCount(LoadingContext->Tasks);
}
