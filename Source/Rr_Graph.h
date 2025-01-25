#pragma once

#include "Rr_BlitNode.h"
#include "Rr_BuiltinNode.h"
#include "Rr_GraphicsNode.h"
#include "Rr_Image.h"
#include "Rr_PresentNode.h"

#include <Rr/Rr_Graph.h>

struct Rr_Frame;

typedef struct Rr_ImageSync Rr_ImageSync;
struct Rr_ImageSync
{
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout Layout;
};

struct Rr_GraphNode
{
    union
    {
        Rr_BuiltinNode BuiltinNode;
        Rr_GraphicsNode GraphicsNode;
        Rr_PresentNode PresentNode;
        Rr_BlitNode BlitNode;
    } Union;
    Rr_Arena *Arena;
    Rr_GraphNodeType Type;
    const char *Name;
    RR_SLICE_TYPE(Rr_GraphNode *) Dependencies;
    bool Executed;
};

typedef struct Rr_GraphEdge Rr_GraphEdge;
struct Rr_GraphEdge
{
    Rr_GraphNode *From;
    Rr_GraphNode *To;
};

typedef struct Rr_GraphBatch Rr_GraphBatch;
struct Rr_GraphBatch
{
    RR_SLICE_TYPE(Rr_GraphNode *) Nodes;
    RR_SLICE_TYPE(VkImageMemoryBarrier) ImageBarriers;
    RR_SLICE_TYPE(VkBufferMemoryBarrier) BufferBarriers;
    VkPipelineStageFlags StageMask;
    VkPipelineStageFlags SwapchainImageStage;
    Rr_Map *LocalSync;
    Rr_Arena *Arena;
};

struct Rr_Graph
{
    RR_SLICE_TYPE(Rr_GraphNode *) Nodes;
};

extern bool Rr_BatchImagePossible(Rr_Map **Sync, VkImage Image);

extern void Rr_BatchImage(
    Rr_App *App,
    Rr_GraphBatch *GraphBatch,
    VkImage Image,
    VkImageAspectFlags AspectMask,
    VkPipelineStageFlags StageMask,
    VkAccessFlags AccessMask,
    VkImageLayout Layout);

extern Rr_GraphNode *Rr_AddGraphNode(
    struct Rr_Frame *Frame,
    Rr_GraphNodeType Type,
    const char *Name,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

extern void Rr_ExecuteGraph(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena);
