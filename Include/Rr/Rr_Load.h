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

typedef enum Rr_LoadStatus
{
    RR_LOAD_STATUS_READY,
    RR_LOAD_STATUS_PENDING,
    RR_LOAD_STATUS_LOADING,
    RR_LOAD_STATUS_NO_TASKS
} Rr_LoadStatus;

typedef void (*Rr_LoadingCallback)(Rr_App* App, const void* Userdata);

Rr_LoadingContext* Rr_CreateLoadingContext(Rr_App* App, size_t InitialTaskCount);
void Rr_LoadColorImageFromPNG(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, struct Rr_Image** OutImage);
void Rr_LoadMeshFromOBJ(Rr_LoadingContext* LoadingContext, const Rr_Asset* Asset, struct Rr_StaticMesh** OutStaticMesh);
void Rr_LoadAsync(Rr_LoadingContext* LoadingContext, Rr_LoadingCallback LoadingCallback, const void* Userdata);
Rr_LoadStatus Rr_LoadImmediate(Rr_LoadingContext* LoadingContext);
void Rr_GetLoadProgress(const Rr_LoadingContext* LoadingContext, u32* OutCurrent, u32* OutTotal);

#ifdef __cplusplus
}
#endif
