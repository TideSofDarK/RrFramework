#pragma once

#include "Rr/Rr_App.h"
#include "Rr/Rr_Asset.h"
#include "Rr/Rr_Image.h"
#include "Rr/Rr_Mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    Rr_U8 BaseTexture;
    Rr_U8 NormalTexture;
    Rr_U8 SpecularTexture;
};

typedef struct Rr_MeshGLTFOptions Rr_MeshGLTFOptions;
struct Rr_MeshGLTFOptions
{
    Rr_USize MeshIndex;
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

extern Rr_LoadTask
Rr_LoadColorImageFromPNG(Rr_AssetRef AssetRef, Rr_Image **OutImage);

extern Rr_LoadTask
Rr_LoadStaticMeshFromOBJ(Rr_AssetRef AssetRef, Rr_StaticMesh **OutStaticMesh);

extern Rr_LoadTask Rr_LoadStaticMeshFromGLTF(
    Rr_AssetRef AssetRef,
    Rr_GLTFLoader *Loader,
    size_t MeshIndex,
    Rr_StaticMesh **OutStaticMesh);

extern Rr_LoadingContext *Rr_LoadAsync(
    Rr_App *App,
    Rr_LoadTask *Tasks,
    Rr_USize TaskCount,
    Rr_LoadingCallback LoadingCallback,
    void *Userdata);

extern Rr_LoadResult
Rr_LoadImmediate(Rr_App *App, Rr_LoadTask *Tasks, Rr_USize TaskCount);

extern void Rr_GetLoadProgress(
    Rr_LoadingContext *LoadingContext,
    Rr_U32 *OutCurrent,
    Rr_U32 *OutTotal);

#ifdef __cplusplus
}
#endif
