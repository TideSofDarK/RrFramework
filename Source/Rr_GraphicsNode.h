#pragma once

#include "Rr_Memory.h"
#include "Rr_Vulkan.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_GraphicsNode.h>

struct Rr_GraphBatch;

typedef enum
{
    RR_GRAPHICS_NODE_FUNCTION_TYPE_NO_OP,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_DRAW_INDEXED,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE,
} Rr_GraphicsNodeFunctionType;

typedef struct Rr_GraphicsNodeFunction Rr_GraphicsNodeFunction;
struct Rr_GraphicsNodeFunction
{
    Rr_GraphicsNodeFunctionType Type;
    void *Args;
    Rr_GraphicsNodeFunction *Next;
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

typedef struct Rr_GraphicsNode Rr_GraphicsNode;
struct Rr_GraphicsNode
{
    Rr_GraphicsNodeInfo Info;
    Rr_GraphicsNodeFunction *EncodedFirst;
    Rr_GraphicsNodeFunction *Encoded;
};

extern bool Rr_BatchGraphicsNode(Rr_App *App, Rr_Graph *Graph, struct Rr_GraphBatch *Batch, Rr_GraphicsNode *Node);

extern void Rr_ExecuteGraphicsNode(Rr_App *App, Rr_GraphicsNode *Node, Rr_Arena *Arena);
