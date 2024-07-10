#pragma once

#include "Graph/Rr_BuiltinNode.h"
#include "Rr/Rr_Graph.h"
#include "Rr/Rr_String.h"
#include "Rr_Image.h"
#include "Rr_Memory.h"

#include <volk.h>

typedef struct Rr_ImageSync Rr_ImageSync;
struct Rr_ImageSync
{
    // VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout Layout;
};

struct Rr_DrawTarget
{
    Rr_Image *ColorImage;
    Rr_Image *DepthImage;
    VkFramebuffer Framebuffer;
};

typedef struct Rr_DrawPrimitiveInfo Rr_DrawPrimitiveInfo;
struct Rr_DrawPrimitiveInfo
{
    Rr_Material *Material;
    struct Rr_Primitive *Primitive;
    uint32_t PerDrawOffset;
};

typedef RR_SLICE_TYPE(Rr_DrawPrimitiveInfo) Rr_DrawPrimitivesSlice;

typedef struct Rr_GenericRenderingContext Rr_GenericRenderingContext;
struct Rr_GenericRenderingContext
{
    Rr_GenericPipeline *BasePipeline;
    Rr_GenericPipeline *OverridePipeline;
    VkDescriptorSet GlobalsDescriptorSet;
};

typedef struct Rr_TextRenderingContext Rr_TextRenderingContext;
struct Rr_TextRenderingContext
{
    VkDescriptorSet GlobalsDescriptorSet;
    VkDescriptorSet FontDescriptorSet;
};

typedef struct Rr_GraphicsNode Rr_GraphicsNode;
struct Rr_GraphicsNode
{
    Rr_GraphicsNodeInfo Info;
    Rr_DrawPrimitivesSlice DrawPrimitivesSlice;
    char GlobalsData[RR_PIPELINE_MAX_GLOBALS_SIZE];
};

typedef struct Rr_PresentNode Rr_PresentNode;
struct Rr_PresentNode
{
    Rr_PresentNodeInfo Info;
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

    RR_SLICE_TYPE(Rr_GraphNode) NodesSlice;

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

extern Rr_Bool Rr_BatchPresentNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_PresentNode *Node);

extern void Rr_ExecutePresentNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_PresentNode *Node,
    Rr_Arena *Arena);

extern Rr_Bool Rr_BatchGraphicsNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node);

extern void Rr_ExecuteGraphicsNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node,
    Rr_Arena *Arena);
