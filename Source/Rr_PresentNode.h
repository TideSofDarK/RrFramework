#pragma once

#include "Rr/Rr_PresentNode.h"
#include "Rr_Memory.h"

typedef struct Rr_PresentNode Rr_PresentNode;
struct Rr_PresentNode
{
    Rr_PresentNodeInfo Info;
};

extern Rr_Bool Rr_BatchPresentNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_PresentNode *Node);

extern void Rr_ExecutePresentNode(
    Rr_App *App,
    Rr_PresentNode *Node);
