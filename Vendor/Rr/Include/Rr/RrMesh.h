#pragma once

#include "RrDefines.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_MeshBuffers Rr_MeshBuffers;
typedef struct Rr_Vertex Rr_Vertex;
typedef struct Rr_RawMesh Rr_RawMesh;
typedef struct Rr_Asset Rr_Asset;

void Rr_UploadMesh(
    Rr_Renderer* Renderer,
    Rr_MeshBuffers* MeshBuffers,
    Rr_MeshIndexType const* Indices,
    size_t IndexCount,
    Rr_Vertex const* Vertices,
    size_t VertexCount);

Rr_MeshBuffers Rr_CreateMesh_FromOBJ(Rr_Renderer* Renderer, Rr_Asset* Asset);

void Rr_DestroyMesh(Rr_Renderer* Renderer, Rr_MeshBuffers* Mesh);

void Rr_ParseOBJ(Rr_RawMesh* RawMesh, Rr_Asset* Asset);

void Rr_DestroyRawMesh(Rr_RawMesh* RawMesh);
