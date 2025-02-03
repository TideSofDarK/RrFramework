#pragma once

#include <Rr/Rr_Graph.h>

#include "Rr_BlitNode.h"
#include "Rr_BuiltinNode.h"
#include "Rr_GraphicsNode.h"
#include "Rr_PresentNode.h"
#include "Rr_TransferNode.h"

struct Rr_Frame;

typedef struct Rr_ImageSync Rr_ImageSync;
struct Rr_ImageSync
{
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout Layout;
};

typedef struct Rr_BufferSync Rr_BufferSync;
struct Rr_BufferSync
{
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkDeviceSize From;
    VkDeviceSize To;
};

typedef struct Rr_GraphNodeResource Rr_GraphNodeResource;
struct Rr_GraphNodeResource
{
    union
    {
        Rr_ImageSync Image;
        Rr_BufferSync Buffer;
    } Union;
    void *Handle;
    bool IsImage;
};

struct Rr_GraphNode
{
    union
    {
        Rr_BuiltinNode BuiltinNode;
        Rr_GraphicsNode GraphicsNode;
        Rr_PresentNode PresentNode;
        Rr_BlitNode BlitNode;
        Rr_TransferNode TransferNode;
    } Union;
    Rr_GraphNodeType Type;
    const char *Name;
    size_t OriginalIndex;
    RR_SLICE(Rr_GraphNodeResource) Reads;
    RR_SLICE(Rr_GraphNodeResource) Writes;
    Rr_Arena *Arena;
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
    RR_SLICE(Rr_GraphNode *) Nodes;
    RR_SLICE(VkImageMemoryBarrier) ImageBarriers;
    RR_SLICE(VkBuffer) Buffers;
    VkPipelineStageFlags SwapchainImageStage;
    Rr_Map *LocalSync;
    Rr_Arena *Arena;
};

struct Rr_Graph
{
    RR_SLICE(Rr_GraphNode *) Nodes;
};

extern Rr_GraphNode *Rr_AddGraphNode(struct Rr_Frame *Frame, Rr_GraphNodeType Type, const char *Name);

extern Rr_GraphNodeResource *Rr_AddGraphNodeRead(Rr_GraphNode *Node);

extern Rr_GraphNodeResource *Rr_AddGraphNodeWrite(Rr_GraphNode *Node);

extern void Rr_ExecuteGraph(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena);

extern bool Rr_BatchImagePossible(Rr_Map **Sync, VkImage Image);

extern void Rr_BatchImage(
    Rr_App *App,
    Rr_GraphBatch *Batch,
    VkImage Image,
    VkImageAspectFlags AspectMask,
    VkPipelineStageFlags StageMask,
    VkAccessFlags AccessMask,
    VkImageLayout Layout);

extern bool Rr_BatchBuffer(
    Rr_App *App,
    Rr_GraphBatch *Batch,
    VkBuffer Buffer,
    VkDeviceSize Size,
    VkDeviceSize Offset,
    VkPipelineStageFlags StageMask,
    VkAccessFlags AccessMask);
