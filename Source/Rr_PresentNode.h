#pragma once

#include <Rr/Rr_PresentNode.h>

#include "Rr_Vulkan.h"

struct Rr_GraphBatch;

typedef struct Rr_PresentNode Rr_PresentNode;
struct Rr_PresentNode
{
    Rr_PresentNodeInfo Info;
    VkPipelineStageFlags Stage;
};

extern bool Rr_BatchPresentNode(Rr_App *App, struct Rr_GraphBatch *Batch, Rr_PresentNode *Node);

extern void Rr_ExecutePresentNode(Rr_App *App, Rr_PresentNode *Node);
