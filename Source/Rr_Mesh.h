#pragma once

#include "Rr_Memory.h"
#include <Rr/Rr_Asset.h>
#include <Rr/Rr_Mesh.h>

struct Rr_LoadSize;
struct Rr_UploadContext;
struct Rr_Material;
struct Rr_Buffer;
struct Rr_GLTFLoader;

typedef struct Rr_Primitive Rr_Primitive;
struct Rr_Primitive
{
    struct Rr_Buffer *IndexBuffer;
    struct Rr_Buffer *VertexBuffer;
    uint32_t IndexCount;
};

typedef struct Rr_RawMesh Rr_RawMesh;
struct Rr_RawMesh
{
    RR_SLICE_TYPE(Rr_Vertex) VerticesSlice;
    RR_SLICE_TYPE(Rr_MeshIndexType) IndicesSlice;
};

struct Rr_StaticMesh
{
    Rr_Primitive *Primitives[RR_MESH_MAX_PRIMITIVES];
    struct Rr_Material *Materials[RR_MESH_MAX_PRIMITIVES];
    uint8_t PrimitiveCount : 4;
    uint8_t MaterialCount : 4;
};

struct Rr_SkeletalMesh
{
    Rr_Primitive *Primitives[RR_MESH_MAX_PRIMITIVES];
    struct Rr_Material *Materials[RR_MESH_MAX_PRIMITIVES];
    uint8_t PrimitiveCount : 4;
    uint8_t MaterialCount : 4;
};

extern Rr_Primitive *Rr_CreatePrimitive(
    struct Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_RawMesh *RawMesh);

extern void Rr_DestroyPrimitive(struct Rr_App *App, Rr_Primitive *Primitive);

extern Rr_StaticMesh *Rr_CreateStaticMesh(
    struct Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_RawMesh *RawMeshes,
    size_t RawMeshCount,
    struct Rr_Material **Materials,
    size_t MaterialCount);

extern Rr_StaticMesh *Rr_CreateStaticMeshGLTF(
    struct Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    struct Rr_GLTFLoader *Loader,
    size_t MeshIndex,
    Rr_Arena *Arena);

extern Rr_StaticMesh *Rr_CreateStaticMeshOBJ(
    struct Rr_App *App,
    struct Rr_UploadContext *UploadContext,
    Rr_AssetRef AssetRef,
    Rr_Arena *Arena);

extern void Rr_GetStaticMeshSizeOBJ(Rr_AssetRef AssetRef, Rr_Arena *Arena, struct Rr_LoadSize *OutLoadSize);

extern void Rr_GetStaticMeshSizeGLTF(
    Rr_AssetRef AssetRef,
    struct Rr_GLTFLoader *Loader,
    size_t MeshIndex,
    Rr_Arena *Arena,
    struct Rr_LoadSize *OutLoadSize);
