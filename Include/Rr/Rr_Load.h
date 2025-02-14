#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Asset.h>
#include <Rr/Rr_Image.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GLTFContext;
struct GLTFAsset;

typedef struct Rr_LoadThread Rr_LoadThread;
typedef struct Rr_LoadContext Rr_LoadContext;

typedef enum Rr_LoadResult
{
    RR_LOAD_RESULT_READY,
    RR_LOAD_RESULT_WRONG_LOAD_TYPE,
    RR_LOAD_RESULT_NO_TASKS
} Rr_LoadResult;

typedef enum Rr_LoadType
{
    RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
    RR_LOAD_TYPE_GLTF_ASSET,
    RR_LOAD_TYPE_CUSTOM,
} Rr_LoadType;

typedef struct Rr_LoadGLTFOptions Rr_LoadGLTFOptions;
struct Rr_LoadGLTFOptions
{
    struct Rr_GLTFContext *GLTFContext;
};

typedef struct Rr_LoadTask Rr_LoadTask;
struct Rr_LoadTask
{
    Rr_LoadType LoadType;
    Rr_AssetRef AssetRef;
    union
    {
        Rr_LoadGLTFOptions GLTF;
    } Options;
    union
    {
        void **Any;
        Rr_Image **Image;
        struct Rr_GLTFAsset **GLTFAsset;
    } Out;
};

typedef void (*Rr_LoadCallback)(Rr_App *App, void *Userdata);

extern Rr_LoadThread *Rr_CreateLoadThread(Rr_App *App);

extern void Rr_DestroyLoadThread(Rr_App *App, Rr_LoadThread *LoadThread);

extern Rr_LoadContext *Rr_LoadAsync(
    Rr_LoadThread *LoadThread,
    size_t TaskCount,
    Rr_LoadTask *Tasks,
    Rr_LoadCallback LoadCallback,
    void *Userdata);

extern Rr_LoadResult Rr_LoadImmediate(
    Rr_App *App,
    size_t TaskCount,
    Rr_LoadTask *Tasks);

extern void Rr_GetLoadProgress(
    Rr_LoadContext *LoadContext,
    uint32_t *OutCurrent,
    uint32_t *OutTotal);

#ifdef __cplusplus
}
#endif
