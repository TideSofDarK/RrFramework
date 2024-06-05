#pragma once

#include "Rr_Mesh.h"
#include "Rr_Memory.h"

struct Rr_UploadContext;
struct Rr_Material;
struct Rr_Buffer;

typedef struct Rr_Primitive Rr_Primitive;
struct Rr_Primitive
{
    struct Rr_Buffer* IndexBuffer;
    struct Rr_Buffer* VertexBuffer;
    u32 IndexCount;
};

typedef struct
{
    Rr_SliceType(Rr_Vertex) VerticesSlice;
    Rr_SliceType(Rr_MeshIndexType) IndicesSlice;
} Rr_RawMesh;

struct Rr_StaticMesh
{
    Rr_Primitive* Primitives[RR_MESH_MAX_PRIMITIVES];
    struct Rr_Material* Materials[RR_MESH_MAX_PRIMITIVES];
    u8 PrimitiveCount;
    u8 MaterialCount;
};

struct Rr_SkeletalMesh
{
    Rr_Primitive* Primitives[RR_MESH_MAX_PRIMITIVES];
    struct Rr_Material* Materials[RR_MESH_MAX_PRIMITIVES];
    u8 PrimitiveCount;
    u8 MaterialCount;
};

extern Rr_Primitive* Rr_CreatePrimitive(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_RawMesh* RawMesh);
extern void Rr_DestroyPrimitive(Rr_App* App, Rr_Primitive* Primitive);

extern Rr_StaticMesh* Rr_CreateStaticMesh(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_RawMesh* RawMeshes,
    size_t RawMeshCount,
    struct Rr_Material** Materials,
    size_t MaterialCount);
extern Rr_StaticMesh* Rr_CreateStaticMeshGLTF(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    const Rr_Asset* Asset,
    size_t MeshIndex,
    Rr_Arena* Arena);
extern Rr_StaticMesh* Rr_CreateStaticMeshOBJ(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    const Rr_Asset* Asset,
    Rr_Arena* Arena);

extern size_t Rr_GetStaticMeshSizeOBJ(const Rr_Asset* Asset, Rr_Arena* Arena);
extern size_t Rr_GetStaticMeshSizeGLTF(const Rr_Asset* Asset, size_t MeshIndex, Rr_Arena* Arena);
