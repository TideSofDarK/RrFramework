#pragma once

#include "Rr/Rr_Graph.h"
#include "Rr/Rr_String.h"
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

/* @TODO: Separate generic and builtin stuff! */
struct Rr_GraphPass
{
    Rr_GraphPassInfo Info;
    Rr_DrawTextsSlice DrawTextsSlice;
    Rr_DrawPrimitivesSlice DrawPrimitivesSlice;
    char GlobalsData[RR_PIPELINE_MAX_GLOBALS_SIZE];
    Rr_Arena *Arena;
};

typedef struct Rr_GraphEdge Rr_GraphEdge;
struct Rr_GraphEdge
{
    Rr_GraphPass *From;
    Rr_GraphPass *To;
};

struct Rr_Graph
{
    RR_SLICE_TYPE(Rr_GraphEdge) AdjList;

    RR_SLICE_TYPE(Rr_GraphPass) PassesSlice;
};

extern void Rr_ExecuteGraph(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena);

extern void Rr_ExecuteGraphPass(
    Rr_App *App,
    Rr_GraphPass *Pass,
    Rr_Arena *Arena);
