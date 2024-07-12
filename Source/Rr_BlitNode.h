#pragma once

#include "Rr/Rr_BlitNode.h"
#include "Rr_Memory.h"

#include <volk.h>

struct Rr_GraphBatch;

typedef struct Rr_BlitNode Rr_BlitNode;
struct Rr_BlitNode
{
    Rr_BlitNodeInfo Info;
    VkImageAspectFlags AspectMask;
};

extern Rr_Bool Rr_BatchBlitNode(
    Rr_App *App,
    Rr_Graph *Graph,
    struct Rr_GraphBatch *Batch,
    Rr_BlitNode *Node);

extern void Rr_ExecuteBlitNode(
    Rr_App *App,
    Rr_BlitNode *Node);
