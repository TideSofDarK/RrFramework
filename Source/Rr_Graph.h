#pragma once

#include "Rr/Rr_Graph.h"
#include "Rr_BuiltinNode.h"
#include "Rr_GraphicsNode.h"
#include "Rr_PresentNode.h"
#include "Rr_Image.h"

typedef struct Rr_ImageSync Rr_ImageSync;
struct Rr_ImageSync
{
    // VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout Layout;
};

struct Rr_DrawTarget
{
    struct
    {
        Rr_Image *ColorImage;
        Rr_Image *DepthImage;
        VkFramebuffer Framebuffer;
    } Frames[RR_FRAME_OVERLAP];
};

struct Rr_GraphNode
{
    union
    {
        Rr_BuiltinNode BuiltinNode;
        Rr_GraphicsNode GraphicsNode;
        Rr_PresentNode PresentNode;
    } Union;
    Rr_GraphNodeType Type;
    const char *Name;
    RR_SLICE_TYPE(Rr_GraphNode *) Dependencies;
    Rr_Bool Executed;
};

typedef struct Rr_GraphEdge Rr_GraphEdge;
struct Rr_GraphEdge
{
    Rr_GraphNode *From;
    Rr_GraphNode *To;
};

struct Rr_Graph
{
    /* Nodes */

    RR_SLICE_TYPE(Rr_GraphNode *) NodesSlice;

    /* Global State */

    VkPipelineStageFlags StageMask;
    Rr_Map *GlobalSyncMap;

    /* Batch State */

    struct Rr_GraphBatch
    {
        RR_SLICE_TYPE(Rr_GraphNode *) NodesSlice;
        RR_SLICE_TYPE(VkImageMemoryBarrier) ImageBarriersSlice;
        RR_SLICE_TYPE(VkBufferMemoryBarrier) BufferBarriersSlice;
        VkPipelineStageFlags StageMask;
        Rr_Map *SyncMap;
        Rr_Bool Final;
        Rr_Arena *Arena;
    } Batch;

    /* Resources */

    VkImage SwapchainImage;
};

extern Rr_Bool Rr_SyncImage(
    Rr_App *App,
    Rr_Graph *Graph,
    VkImage Image,
    VkImageAspectFlags AspectMask,
    VkPipelineStageFlags StageMask,
    VkAccessFlags AccessMask,
    VkImageLayout Layout);

extern void Rr_ExecuteGraph(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena);
