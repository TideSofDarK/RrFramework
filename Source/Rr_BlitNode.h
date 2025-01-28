#pragma once

#include <Rr/Rr_BlitNode.h>

#include "Rr_Image.h"
#include "Rr_Vulkan.h"

struct Rr_GraphBatch;

typedef struct Rr_BlitNode Rr_BlitNode;
struct Rr_BlitNode
{
    Rr_AllocatedImage *SrcImage;
    Rr_AllocatedImage *DstImage;
    Rr_IntVec4 SrcRect;
    Rr_IntVec4 DstRect;
    Rr_BlitMode Mode;
    VkImageAspectFlags AspectMask;
};

extern bool Rr_BatchBlitNode(Rr_App *App, Rr_Graph *Graph, struct Rr_GraphBatch *Batch, Rr_BlitNode *Node);

extern void Rr_ExecuteBlitNode(Rr_App *App, Rr_BlitNode *Node);
