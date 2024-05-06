#pragma once

#include "Rr_Array.h"
#include "Rr_Framework.h"

typedef u32 Rr_MeshIndexType;

typedef struct Rr_Vertex
{
    vec3 Position;
    f32 TexCoordX;
    vec3 Normal;
    f32 TexCoordY;
    vec4 Color;
} Rr_Vertex;

struct Rr_RawMesh
{
    Rr_Array Vertices;
    Rr_Array Indices;
};

struct Rr_StaticMesh
{
    size_t IndexCount;
    Rr_Buffer* IndexBuffer;
    Rr_Buffer* VertexBuffer;
};

Rr_StaticMesh* Rr_CreateStaticMeshFromOBJ(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    Rr_Asset* Asset);

size_t Rr_GetStaticMeshSizeOBJ(Rr_Asset* Asset);

Rr_RawMesh* Rr_CreateRawMeshOBJ(Rr_Asset* Asset);
void Rr_DestroyRawMesh(Rr_RawMesh* RawMesh);
