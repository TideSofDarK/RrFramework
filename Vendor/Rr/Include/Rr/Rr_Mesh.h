#pragma once

#include "Rr_Array.h"
#include "Rr_Defines.h"
#include "Rr_Buffer.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Vertex Rr_Vertex;
typedef struct Rr_Asset Rr_Asset;

typedef struct Rr_RawMesh
{
    Rr_Array Vertices;
    Rr_Array Indices;
} Rr_RawMesh;

typedef struct Rr_MeshBuffers
{
    size_t IndexCount;
    Rr_Buffer IndexBuffer;
    Rr_Buffer VertexBuffer;
    VkDeviceAddress VertexBufferAddress;
} Rr_MeshBuffers;

Rr_MeshBuffers Rr_CreateMeshFromOBJ(
    const Rr_Renderer* const Renderer,
    const VkCommandBuffer GraphicsCommandBuffer,
    const VkCommandBuffer TransferCommandBuffer,
    Rr_StagingBuffer* StagingBuffer,
    const Rr_Asset* Asset);

void Rr_DestroyMesh(const Rr_Renderer* Renderer, const Rr_MeshBuffers* Mesh);

size_t Rr_GetMeshBuffersSizeOBJ(const Rr_Asset* Asset);

void Rr_ParseOBJ(Rr_RawMesh* RawMesh, const Rr_Asset* Asset);

void Rr_DestroyRawMesh(Rr_RawMesh* RawMesh);
