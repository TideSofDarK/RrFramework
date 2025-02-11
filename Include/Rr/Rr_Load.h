#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Asset.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_Mesh.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_LoadingThread Rr_LoadingThread;
typedef struct Rr_LoadingContext Rr_LoadingContext;

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

typedef struct Rr_GLTFLoader Rr_GLTFLoader;
struct Rr_GLTFLoader
{
    struct Rr_GenericPipeline *GenericPipeline;
    uint8_t BaseTexture;
    uint8_t NormalTexture;
    uint8_t SpecularTexture;
};

typedef struct Rr_MeshGLTFOptions Rr_MeshGLTFOptions;
struct Rr_MeshGLTFOptions
{
    size_t MeshIndex;
    Rr_GLTFLoader Loader;
};

typedef union
{
    Rr_MeshGLTFOptions MeshGLTF;
} Rr_LoadTaskOptions;

typedef struct Rr_LoadTask Rr_LoadTask;
struct Rr_LoadTask
{
    Rr_LoadType LoadType;
    Rr_AssetRef AssetRef;
    Rr_LoadTaskOptions Options;
    void **Out;
};

typedef void (*Rr_LoadingCallback)(Rr_App *App, void *Userdata);

extern Rr_LoadingThread *Rr_CreateLoadingThread(Rr_App *App);

extern void Rr_DestroyLoadingThread(Rr_App *App, Rr_LoadingThread *LoadingThread);

extern Rr_LoadTask Rr_LoadColorImageFromPNG(Rr_AssetRef AssetRef, Rr_Image **OutImage);

// extern Rr_LoadTask Rr_LoadStaticMeshFromOBJ(Rr_AssetRef AssetRef, Rr_StaticMesh **OutStaticMesh);

// extern Rr_LoadTask Rr_LoadStaticMeshFromGLTF(
//     Rr_AssetRef AssetRef,
//     Rr_GLTFLoader *Loader,
//     size_t MeshIndex,
//     Rr_StaticMesh **OutStaticMesh);

extern Rr_LoadingContext *Rr_LoadAsync(
    Rr_LoadingThread *LoadingThread,
    size_t TaskCount,
    Rr_LoadTask *Tasks,
    Rr_LoadingCallback LoadingCallback,
    void *Userdata);

extern Rr_LoadResult Rr_LoadImmediate(Rr_App *App, size_t TaskCount, Rr_LoadTask *Tasks);

extern void Rr_GetLoadProgress(Rr_LoadingContext *LoadingContext, uint32_t *OutCurrent, uint32_t *OutTotal);

#ifdef __cplusplus
}
#endif
