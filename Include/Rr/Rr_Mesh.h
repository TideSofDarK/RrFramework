#pragma once

#include "Rr_Array.h"
#include "Rr_Defines.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Vertex Rr_Vertex;
typedef struct Rr_Asset Rr_Asset;
typedef struct Rr_Buffer Rr_Buffer;
typedef struct Rr_UploadContext Rr_UploadContext;

typedef struct Rr_RawMesh
{
    Rr_Array Vertices;
    Rr_Array Indices;
} Rr_RawMesh;

typedef struct Rr_MeshBuffers
{
    size_t IndexCount;
    Rr_Buffer* IndexBuffer;
    Rr_Buffer* VertexBuffer;
} Rr_MeshBuffers;

Rr_MeshBuffers Rr_CreateMeshFromOBJ(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    Rr_Asset* Asset);

void Rr_DestroyMesh(Rr_Renderer* Renderer, Rr_MeshBuffers* Mesh);

size_t Rr_GetMeshBuffersSizeOBJ(Rr_Asset* Asset);

void Rr_ParseOBJ(Rr_RawMesh* RawMesh, Rr_Asset* Asset);

void Rr_DestroyRawMesh(Rr_RawMesh* RawMesh);

#ifdef __cplusplus
}
#endif
