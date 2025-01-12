#pragma once

#include "Rr/Rr_BuiltinNode.h"
#include "Rr/Rr_Graph.h"
#include "Rr_Memory.h"

#include <volk.h>

struct Rr_GraphBatch;

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

typedef struct Rr_BuiltinNode Rr_BuiltinNode;
struct Rr_BuiltinNode
{
    Rr_DrawTextsSlice DrawTextsSlice;
};

typedef struct Rr_TextRenderingContext Rr_TextRenderingContext;
struct Rr_TextRenderingContext
{
    VkDescriptorSet GlobalsDescriptorSet;
    VkDescriptorSet FontDescriptorSet;
};

extern Rr_Bool Rr_BatchBuiltinNode(Rr_App *App, Rr_Graph *Graph, struct Rr_GraphBatch *Batch, Rr_BuiltinNode *Node);

extern void Rr_ExecuteBuiltinNode(Rr_App *App, Rr_BuiltinNode *Node, Rr_Arena *Arena);
