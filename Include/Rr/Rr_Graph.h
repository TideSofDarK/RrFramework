#pragma once

#include "Rr/Rr_App.h"
#include "Rr/Rr_Image.h"
#include "Rr/Rr_Material.h"
#include "Rr/Rr_Math.h"
#include "Rr/Rr_Mesh.h"
#include "Rr/Rr_Pipeline.h"
#include "Rr/Rr_String.h"
#include "Rr/Rr_Text.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_DrawTarget Rr_DrawTarget;
typedef struct Rr_Graph Rr_Graph;
typedef struct Rr_GraphNode Rr_GraphNode;

typedef enum Rr_GraphNodeType
{
    RR_GRAPH_NODE_TYPE_BUILTIN,
    RR_GRAPH_NODE_TYPE_GRAPHICS,
    RR_GRAPH_NODE_TYPE_BLIT,
    RR_GRAPH_NODE_TYPE_PRESENT,
} Rr_GraphNodeType;

/*
 * Graphics Node
 */

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

/*
 * Present Node
 */

typedef enum Rr_PresentMode
{
    RR_PRESENT_MODE_NORMAL,
    RR_PRESENT_MODE_STRETCH,
    RR_PRESENT_MODE_FIT,
} Rr_PresentMode;

typedef struct Rr_PresentNodeInfo Rr_PresentNodeInfo;
struct Rr_PresentNodeInfo
{
    Rr_PresentMode Mode;
    Rr_DrawTarget *DrawTarget;
};

extern Rr_GraphNode *Rr_AddPresentNode(
    Rr_App *App,
    const char *Name,
    Rr_PresentNodeInfo *Info,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

/*
 * Builtin Node
 */

typedef enum Rr_DrawTextFlags
{
    RR_DRAW_TEXT_FLAGS_NONE_BIT = 0,
    RR_DRAW_TEXT_FLAGS_ANIMATION_BIT = 1
} Rr_DrawTextFlags;

extern Rr_GraphNode *Rr_AddBuiltinNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

extern void Rr_DrawCustomText(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_Font *Font,
    Rr_String *String,
    Rr_Vec2 Position,
    float Size,
    Rr_DrawTextFlags Flags);

extern void Rr_DrawDefaultText(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_String *String,
    Rr_Vec2 Position);

/*
 * Draw Commands
 */

extern void Rr_DrawStaticMesh(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_StaticMesh *StaticMesh,
    Rr_Data PerDrawData);

extern void Rr_DrawStaticMeshOverrideMaterials(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_Material **OverrideMaterials,
    size_t OverrideMaterialCount,
    Rr_StaticMesh *StaticMesh,
    Rr_Data PerDrawData);

/*
 * Draw Target
 */

extern Rr_DrawTarget *Rr_CreateDrawTarget(
    Rr_App *App,
    uint32_t Width,
    uint32_t Height);

extern Rr_DrawTarget *Rr_CreateDrawTargetDepthOnly(
    Rr_App *App,
    uint32_t Width,
    uint32_t Height);

extern void Rr_DestroyDrawTarget(Rr_App *App, Rr_DrawTarget *DrawTarget);

extern Rr_DrawTarget *Rr_GetMainDrawTarget(Rr_App *App);

#ifdef __cplusplus
}
#endif
