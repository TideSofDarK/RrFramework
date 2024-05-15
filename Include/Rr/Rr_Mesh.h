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
typedef struct Rr_RawMesh Rr_RawMesh;

typedef u32 Rr_MeshIndexType;

typedef struct Rr_Vertex
{
    Rr_Vec3 Position;
    f32 TexCoordX;
    Rr_Vec3 Normal;
    f32 TexCoordY;
    Rr_Vec4 Color;
} Rr_Vertex;

Rr_StaticMesh* Rr_CreateStaticMeshFromOBJ(
    Rr_App* App,
    struct Rr_UploadContext* UploadContext,
    Rr_Asset* Asset);

void Rr_DestroyStaticMesh(Rr_App* App, Rr_StaticMesh* Mesh);

size_t Rr_GetStaticMeshSizeOBJ(Rr_Asset* Asset);

Rr_RawMesh* Rr_CreateRawMeshOBJ(Rr_Asset* Asset);
Rr_RawMesh* Rr_CreateRawMeshGLTF(const Rr_Asset* Asset);
void Rr_DestroyRawMesh(Rr_RawMesh* RawMesh);

#ifdef __cplusplus
}
#endif
