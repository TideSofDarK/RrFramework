#include "Rr_BlitNode.h"

#include "Rr_App.h"
#include "Rr_Graph.h"
#include "Rr_Memory.h"
#include "Rr_Renderer.h"

Rr_GraphNode *Rr_AddBlitNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(
        Frame,
        RR_GRAPH_NODE_TYPE_BLIT,
        Name,
        Dependencies,
        DependencyCount);

    Rr_BlitNode *BlitNode = &GraphNode->Union.BlitNode;
    RR_ZERO_PTR(BlitNode);

    return GraphNode;
}
