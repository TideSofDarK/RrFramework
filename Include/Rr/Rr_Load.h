#pragma once

#include "Rr_Defines.h"
#include "Rr_App.h"
#include "Rr_Asset.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct Rr_Image;
struct Rr_StaticMesh;

typedef struct Rr_LoadingContext Rr_LoadingContext;
typedef struct Rr_UploadContext Rr_UploadContext;

typedef enum Rr_LoadResult
{
    RR_LOAD_RESULT_READY,
    RR_LOAD_RESULT_WRONG_LOAD_TYPE,
    RR_LOAD_RESULT_NO_TASKS
} Rr_LoadResult;

typedef enum Rr_LoadType
{
    RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
    RR_LOAD_TYPE_STATIC_MESH_FROM_OBJ,
    RR_LOAD_TYPE_STATIC_MESH_FROM_GLTF,
} Rr_LoadType;

typedef struct
{
    usize MeshIndex;
} Rr_MeshGLTFOptions;

typedef union
{
    Rr_MeshGLTFOptions MeshGLTF;
} Rr_LoadTaskOptions;

typedef struct Rr_LoadTask
{
    Rr_LoadType LoadType;
    Rr_Asset Asset;
    Rr_LoadTaskOptions Options;
    void** Out;
} Rr_LoadTask;

typedef void (*Rr_LoadingCallback)(Rr_App* App, const void* Userdata);

extern Rr_LoadTask Rr_LoadColorImageFromPNG(
    const Rr_Asset* Asset,
    struct Rr_Image** OutImage);
extern Rr_LoadTask Rr_LoadStaticMeshFromOBJ(
    const Rr_Asset* Asset,
    struct Rr_StaticMesh** OutStaticMesh);
extern Rr_LoadTask Rr_LoadStaticMeshFromGLTF(
    const Rr_Asset* Asset,
    size_t MeshIndex,
    struct Rr_StaticMesh** OutStaticMesh);

extern Rr_LoadResult Rr_Load(Rr_LoadingContext* LoadingContext);
extern Rr_LoadingContext* Rr_LoadAsync(
    Rr_App* App,
    Rr_LoadTask* LoadTasks,
    usize LoadTasksCount,
    Rr_LoadingCallback LoadingCallback,
    const void* Userdata);
extern Rr_LoadResult Rr_LoadImmediate(
    Rr_App* App,
    Rr_LoadTask* LoadTasks,
    usize LoadTasksCount);
extern void Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext, u32* OutCurrent, u32* OutTotal);

#ifdef __cplusplus
}
#endif
