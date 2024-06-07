#pragma once

#include "Rr_Asset.h"
#include "Rr_App.h"
#include "Rr_Math.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RR_MESH_MAX_PRIMITIVES 4

typedef struct Rr_StaticMesh Rr_StaticMesh;
typedef struct Rr_SkeletalMesh Rr_SkeletalMesh;

typedef u32 Rr_MeshIndexType;

typedef struct Rr_Vertex Rr_Vertex;
struct Rr_Vertex
{
    Rr_Vec3 Position;
    f32 TexCoordX;
    Rr_Vec3 Normal;
    f32 TexCoordY;
    Rr_Vec4 Color;
};

extern void Rr_DestroyStaticMesh(Rr_App* App, Rr_StaticMesh* Mesh);

#ifdef __cplusplus
}
#endif
