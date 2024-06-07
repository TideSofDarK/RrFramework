#include "Rr_Load.h"

#include "Rr_Load_Internal.h"
#include "Rr_Types.h"

#include <SDL3/SDL.h>

Rr_LoadTask Rr_LoadColorImageFromPNG(Rr_AssetRef AssetRef, Rr_Image** OutImage)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
        .AssetRef = AssetRef,
        .Out = (void**)OutImage
    };
}

Rr_LoadTask Rr_LoadStaticMeshFromOBJ(Rr_AssetRef AssetRef, struct Rr_StaticMesh** OutStaticMesh)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ,
        .AssetRef = AssetRef,
        .Out = (void**)OutStaticMesh
    };
}

Rr_LoadTask Rr_LoadStaticMeshFromGLTF(
    Rr_AssetRef AssetRef,
    Rr_GLTFLoader* Loader,
    usize MeshIndex,
    struct Rr_StaticMesh** OutStaticMesh)
{
    return (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_STATIC_MESH_FROM_GLTF,
        .AssetRef = AssetRef,
        .Out = (void**)OutStaticMesh,
        .Options = (Rr_MeshGLTFOptions){
            .MeshIndex = MeshIndex,
            .Loader = *Loader }
    };
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
