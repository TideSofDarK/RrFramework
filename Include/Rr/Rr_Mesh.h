#pragma once

#include "Rr_Array.h"
#include "Rr_Defines.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Asset Rr_Asset;
typedef struct Rr_UploadContext Rr_UploadContext;
typedef struct Rr_StaticMesh Rr_StaticMesh;
typedef struct Rr_RawMesh Rr_RawMesh;

Rr_StaticMesh* Rr_CreateStaticMeshFromOBJ(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    Rr_Asset* Asset);
void Rr_DestroyStaticMesh(Rr_Renderer* Renderer, Rr_StaticMesh* Mesh);

size_t Rr_GetStaticMeshSizeOBJ(Rr_Asset* Asset);

Rr_RawMesh* Rr_CreateRawMeshOBJ(Rr_Asset* Asset);
void Rr_DestroyRawMesh(Rr_RawMesh* RawMesh);

#ifdef __cplusplus
}
#endif
