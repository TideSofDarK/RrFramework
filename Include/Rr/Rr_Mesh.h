#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RR_MESH_MAX_PRIMITIVES 4

typedef struct Rr_StaticMesh Rr_StaticMesh;
typedef struct Rr_SkeletalMesh Rr_SkeletalMesh;

typedef uint32_t Rr_MeshIndexType;

typedef struct Rr_Vertex Rr_Vertex;
struct Rr_Vertex
{
    Rr_Vec3 Position;
    float TexCoordX;
    Rr_Vec3 Normal;
    float TexCoordY;
    Rr_Vec3 Tangent;
    float Reserved;
};

extern void Rr_DestroyStaticMesh(Rr_App *App, Rr_StaticMesh *Mesh);

#ifdef __cplusplus
}
#endif
