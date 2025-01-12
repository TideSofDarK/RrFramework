#pragma once

#include "Rr/Rr_Graph.h"
#include "Rr/Rr_Image.h"
#include "Rr/Rr_Material.h"
#include "Rr/Rr_Mesh.h"
#include "Rr/Rr_Pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_GraphicsNodeInfo Rr_GraphicsNodeInfo;
struct Rr_GraphicsNodeInfo
{
    Rr_DrawTarget *DrawTarget;
    Rr_Image *InitialColor;
    Rr_Image *InitialDepth;
    Rr_IntVec4 Viewport;
    Rr_GenericPipeline *BasePipeline;
    Rr_GenericPipeline *OverridePipeline;
};

extern Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphicsNodeInfo *Info,
    char *GlobalsData,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

extern void Rr_DrawStaticMesh(Rr_App *App, Rr_GraphNode *Node, Rr_StaticMesh *StaticMesh, Rr_Data PerDrawData);

extern void Rr_DrawStaticMeshOverrideMaterials(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_Material **OverrideMaterials,
    size_t OverrideMaterialCount,
    Rr_StaticMesh *StaticMesh,
    Rr_Data PerDrawData);

#ifdef __cplusplus
}
#endif
