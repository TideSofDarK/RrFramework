#pragma once

#include "RrDefines.h"

typedef struct SRr SRr;
typedef struct SRrMeshBuffers SRrMeshBuffers;
typedef struct SRrVertex SRrVertex;

void Rr_UploadMesh(
    SRr* Rr,
    SRrMeshBuffers* MeshBuffers,
    MeshIndexType const* Indices,
    size_t IndexCount,
    SRrVertex const* Vertices,
    size_t VertexCount);

void Rr_CleanupMesh(SRr* Rr, SRrMeshBuffers* Mesh);
