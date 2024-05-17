#pragma once

#include "Rr_Asset.h"
#include "Rr_App.h"
#include "Rr_Math.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct Rr_UploadContext;

typedef struct Rr_StaticMesh Rr_StaticMesh;

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

Rr_StaticMesh* Rr_CreateStaticMeshFromOBJ(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_Asset* Asset);
Rr_StaticMesh* Rr_CreateStaticMeshFromGLTF(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_Asset* Asset);

void Rr_DestroyStaticMesh(Rr_App* App, Rr_StaticMesh* Mesh);

size_t Rr_GetStaticMeshSizeOBJ(const Rr_Asset* Asset);
size_t Rr_GetStaticMeshSizeGLTF(const Rr_Asset* Asset);

void Rr_ParseRawMeshOBJ(Rr_RawMesh* RawMesh, const Rr_Asset* Asset);
void Rr_ParseRawMeshGLTF(Rr_RawMesh* RawMesh, const Rr_Asset* Asset);
void Rr_FreeRawMesh(Rr_RawMesh* RawMesh);

#ifdef __cplusplus
}
#endif
