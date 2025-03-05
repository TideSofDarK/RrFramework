#pragma once

#include <Rr/Rr_Buffer.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_Pipeline.h>
#include <Rr/Rr_Renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Graph Rr_Graph;
typedef struct Rr_GraphNode Rr_GraphNode;

typedef enum Rr_GraphNodeType
{
    RR_GRAPH_NODE_TYPE_COMPUTE,
    RR_GRAPH_NODE_TYPE_GRAPHICS,
    RR_GRAPH_NODE_TYPE_BLIT,
    RR_GRAPH_NODE_TYPE_TRANSFER,
} Rr_GraphNodeType;

typedef union Rr_GraphHandle Rr_GraphBuffer;
typedef union Rr_GraphHandle Rr_GraphImage;
typedef union Rr_GraphHandle Rr_GraphHandle;
union Rr_GraphHandle
{
    struct
    {
        uint32_t Index;
        uint32_t Generation;
    } Values;
    Rr_MapKey Hash;
};

typedef enum Rr_BlitMode
{
    RR_BLIT_MODE_COLOR,
    RR_BLIT_MODE_DEPTH,
} Rr_BlitMode;

typedef union Rr_ColorClear Rr_ColorClear;
union Rr_ColorClear
{
    Rr_Vec4 Vec4;
    Rr_IntVec4 IntVec4;
};

typedef struct Rr_ColorTarget Rr_ColorTarget;
struct Rr_ColorTarget
{
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
    Rr_LoadOp LoadOp;
    Rr_StoreOp StoreOp;
    Rr_DepthClear Clear;
};

extern Rr_GraphNode *Rr_AddTransferNode(
    Rr_Renderer *Renderer,
    const char *Name);

extern void Rr_TransferBufferData(
    Rr_GraphNode *Node,
    size_t Size,
    Rr_Buffer *SrcBuffer,
    size_t SrcOffset,
    Rr_Buffer *DstBuffer,
    size_t DstOffset);

extern Rr_GraphNode *Rr_AddBlitNode(
    Rr_Renderer *Renderer,
    const char *Name,
    Rr_Image *SrcImage,
    Rr_Image *DstImage,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    Rr_ImageAspect ImageAspect);

extern Rr_GraphNode *Rr_AddComputeNode(Rr_Renderer *Renderer, const char *Name);

extern void Rr_BindComputePipeline(
    Rr_GraphNode *Node,
    Rr_ComputePipeline *ComputePipeline);

extern void Rr_Dispatch(
    Rr_GraphNode *Node,
    uint32_t GroupCountX,
    uint32_t GroupCountY,
    uint32_t GroupCountZ);

extern Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_Renderer *Renderer,
    const char *Name,
    size_t ColorTargetCount,
    Rr_ColorTarget *ColorTargets,
    Rr_Image **ColorImages,
    Rr_DepthTarget *DepthTarget,
    Rr_Image *DepthImage);

extern void Rr_Draw(
    Rr_GraphNode *Node,
    uint32_t VertexCount,
    uint32_t InstanceCount,
    uint32_t FirstVertex,
    uint32_t FirstInstance);

extern void Rr_DrawIndexed(
    Rr_GraphNode *Node,
    uint32_t IndexCount,
    uint32_t InstanceCount,
    uint32_t FirstIndex,
    int32_t VertexOffset,
    uint32_t FirstInstance);

extern void Rr_BindVertexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Slot,
    uint32_t Offset);

extern void Rr_BindIndexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Slot,
    uint32_t Offset,
    Rr_IndexType Type);

extern void Rr_BindGraphicsPipeline(
    Rr_GraphNode *Node,
    Rr_GraphicsPipeline *GraphicsPipeline);

extern void Rr_SetViewport(Rr_GraphNode *Node, Rr_Vec4 Rect);

extern void Rr_SetScissor(Rr_GraphNode *Node, Rr_IntVec4 Rect);

extern void Rr_BindSampler(
    Rr_GraphNode *Node,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindSampledImage(
    Rr_GraphNode *Node,
    Rr_Image *Image,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindCombinedImageSampler(
    Rr_GraphNode *Node,
    Rr_Image *Image,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindUniformBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t Offset,
    uint32_t Size);

extern void Rr_BindStorageBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t Offset,
    uint32_t Size);

extern void Rr_BindStorageImage(
    Rr_GraphNode *Node,
    Rr_Image *Image,
    uint32_t Set,
    uint32_t Binding);

#ifdef __cplusplus
}
#endif
