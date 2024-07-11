#pragma once

#include "Rr/Rr_Graph.h"
#include "Rr/Rr_GraphicsNode.h"
#include "Rr_Memory.h"

#include <volk.h>

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
    Rr_DrawPrimitivesSlice DrawPrimitivesSlice;
    char GlobalsData[RR_PIPELINE_MAX_GLOBALS_SIZE];
};

extern Rr_Bool Rr_BatchGraphicsNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node);

extern void Rr_ExecuteGraphicsNode(
    Rr_App *App,
    Rr_GraphicsNode *Node,
    Rr_Arena *Arena);
