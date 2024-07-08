#pragma once

#include "Rr/Rr_Graph.h"
#include "Rr/Rr_String.h"
#include "Rr_Image.h"
#include "Rr_Memory.h"

#include <volk.h>

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

typedef struct Rr_DrawTextInfo Rr_DrawTextInfo;
struct Rr_DrawTextInfo
{
    Rr_Font *Font;
    Rr_String String;
    Rr_Vec2 Position;
    float Size;
    Rr_DrawTextFlags Flags;
};

typedef RR_SLICE_TYPE(Rr_DrawTextInfo) Rr_DrawTextsSlice;
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

typedef struct Rr_BuiltinNode Rr_BuiltinNode;
struct Rr_BuiltinNode
{
    Rr_DrawTextsSlice DrawTextsSlice;
};

struct Rr_GraphicsNode
{
    Rr_GraphicsNodeInfo Info;
    Rr_DrawPrimitivesSlice DrawPrimitivesSlice;
    char GlobalsData[RR_PIPELINE_MAX_GLOBALS_SIZE];
};

typedef struct Rr_GraphNode Rr_GraphNode;
struct Rr_GraphNode
{
    union
    {
        Rr_BuiltinNode BuiltinNode;
        Rr_GraphicsNode GraphicsNode;
        Rr_PresentNode PresentNode;
    } Union;
    Rr_GraphNodeType Type;
};

typedef struct Rr_GraphEdge Rr_GraphEdge;
struct Rr_GraphEdge
{
    Rr_GraphNode *From;
    Rr_GraphNode *To;
};

struct Rr_Graph
{
    Rr_Map *ImageStateMap;
    // RR_SLICE_TYPE(Rr_GraphEdge) AdjList;
    // Rr_ImageBarrierMap ImageBarrierMap;

    RR_SLICE_TYPE(Rr_GraphNode) PassesSlice;

    Rr_BuiltinNode BuiltinPass;
};

extern void Rr_ExecuteGraph(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena);

extern void Rr_ExecutePresentNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_PresentNode *Node,
    Rr_Arena *Arena);

extern void Rr_ExecuteGraphicsNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node,
    Rr_Arena *Arena);

extern void Rr_ExecuteBuiltinNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_BuiltinNode *Node,
    Rr_Arena *Arena);
