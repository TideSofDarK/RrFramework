#pragma once

#include "Rr_Asset.h"
#include "Rr_App.h"
#include "Rr_Math.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RR_MESH_MAX_PRIMITIVES 4

struct Rr_UploadContext;
struct Rr_Material;

typedef struct Rr_MeshBuffers Rr_MeshBuffers;
typedef struct Rr_StaticMesh Rr_StaticMesh;
typedef struct Rr_SkeletalMesh Rr_SkeletalMesh;

typedef u32 Rr_MeshIndexType;

typedef struct
{
    Rr_Vec3 Position;
    f32 TexCoordX;
    Rr_Vec3 Normal;
    f32 TexCoordY;
    Rr_Vec4 Color;
} Rr_Vertex;

typedef struct
{
    Rr_Vertex* Vertices;
    Rr_MeshIndexType* Indices;
} Rr_RawMesh;

void Rr_DestroyMeshBuffers(Rr_App* App, Rr_MeshBuffers* MeshBuffers);

Rr_StaticMesh* Rr_CreateStaticMesh(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_RawMesh* RawMeshes,
    size_t RawMeshCount,
    struct Rr_Material** Material,
    size_t MaterialCount);
Rr_StaticMesh* Rr_CreateStaticMeshGLTF(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    const Rr_Asset* Asset,
    size_t MeshIndex);
Rr_StaticMesh* Rr_CreateStaticMeshOBJ(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    const Rr_Asset* Asset);
void Rr_DestroyStaticMesh(Rr_App* App, Rr_StaticMesh* Mesh);

size_t Rr_GetStaticMeshSizeOBJ(const Rr_Asset* Asset);
size_t Rr_GetStaticMeshSizeGLTF(const Rr_Asset* Asset, size_t MeshIndex);

/* @TODO: Reuse allocated vertex/index buffers. */
Rr_RawMesh Rr_AllocateRawMesh(size_t IndexCount, size_t VertexCount);
void Rr_ParseRawMeshOBJ(const Rr_Asset* Asset, Rr_RawMesh* RawMesh);
void Rr_ParseRawMeshGLTF(const Rr_Asset* Asset, Rr_RawMesh* RawMesh);
void Rr_FreeRawMesh(Rr_RawMesh* RawMesh);

#ifdef __cplusplus
}
#endif
