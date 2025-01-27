#pragma once

#include <Rr/Rr_Buffer.h>
#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_Mesh.h>
#include <Rr/Rr_Pipeline.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    RR_LOAD_OP_LOAD,
    RR_LOAD_OP_CLEAR,
    RR_LOAD_OP_DONT_CARE,
} Rr_LoadOp;

typedef enum
{
    RR_STORE_OP_STORE,
    RR_STORE_OP_DONT_CARE,
    RR_STORE_OP_RESOLVE,
    RR_STORE_OP_RESOLVE_AND_STORE,
} Rr_StoreOp;

typedef union Rr_ColorClear Rr_ColorClear;
union Rr_ColorClear
{
    float Float32[4];
    int32_t Int32[4];
    uint32_t Uint32[4];
};

typedef struct Rr_ColorTarget Rr_ColorTarget;
struct Rr_ColorTarget
{
    Rr_Image *Image;
    uint32_t Slot;
    Rr_LoadOp LoadOp;
    Rr_StoreOp StoreOp;
    Rr_ColorClear Clear;
};

typedef struct Rr_DepthClear Rr_DepthClear;
struct Rr_DepthClear
{
    float Depth;
    uint32_t Stencil;
};

typedef struct Rr_DepthTarget Rr_DepthTarget;
struct Rr_DepthTarget
{
    Rr_Image *Image;
    uint32_t Slot;
    Rr_LoadOp LoadOp;
    Rr_StoreOp StoreOp;
    Rr_DepthClear Clear;
};

extern Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_App *App,
    const char *Name,
    Rr_ColorTarget *ColorTargets,
    size_t ColorTargetCount,
    Rr_DepthTarget *DepthTarget,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

extern void Rr_DrawIndexed(
    Rr_GraphNode *Node,
    uint32_t IndexCount,
    uint32_t InstanceCount,
    uint32_t FirstIndex,
    int32_t VertexOffset,
    uint32_t FirstInstance);

extern void Rr_BindVertexBuffer(Rr_GraphNode *Node, Rr_Buffer *Buffer, uint32_t Slot, uint32_t Offset);

extern void Rr_BindIndexBuffer(Rr_GraphNode *Node, Rr_Buffer *Buffer, uint32_t Slot, uint32_t Offset);

extern void Rr_BindGraphicsPipeline(Rr_GraphNode *Node, Rr_GraphicsPipeline *GraphicsPipeline);

#ifdef __cplusplus
}
#endif
