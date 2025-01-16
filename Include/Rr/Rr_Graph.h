#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_String.h>

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
 * Draw Target
 */

extern Rr_DrawTarget *Rr_CreateDrawTarget(Rr_App *App, uint32_t Width, uint32_t Height);

extern Rr_DrawTarget *Rr_CreateDrawTargetDepthOnly(Rr_App *App, uint32_t Width, uint32_t Height);

extern void Rr_DestroyDrawTarget(Rr_App *App, Rr_DrawTarget *DrawTarget);

extern struct Rr_Image *Rr_GetDrawTargetColorImage(Rr_App *App, Rr_DrawTarget *DrawTarget);

extern struct Rr_Image *Rr_GetDrawTargetDepthImage(Rr_App *App, Rr_DrawTarget *DrawTarget);

extern Rr_DrawTarget *Rr_GetMainDrawTarget(Rr_App *App);

#ifdef __cplusplus
}
#endif
