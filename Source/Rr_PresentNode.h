#pragma once

#include "Rr/Rr_PresentNode.h"
#include "Rr_Memory.h"

struct Rr_GraphBatch;

typedef struct Rr_PresentNode Rr_PresentNode;
struct Rr_PresentNode
{
    Rr_PresentNodeInfo Info;
};

extern Rr_Bool Rr_BatchPresentNode(
    Rr_App *App,
    Rr_Graph *Graph,
    struct Rr_GraphBatch *Batch,
    Rr_PresentNode *Node);

extern void Rr_ExecutePresentNode(
    Rr_App *App,
    Rr_PresentNode *Node);
