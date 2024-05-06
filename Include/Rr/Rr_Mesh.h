#pragma once

#include "Rr_Array.h"
#include "Rr_Framework.h"

#ifdef __cplusplus
extern "C"
{
#endif

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
