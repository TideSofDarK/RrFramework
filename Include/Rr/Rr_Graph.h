#pragma once

#include <Rr/Rr_Pipeline.h>
#include <Rr/Rr_Renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Rr_Sampler;
struct Rr_Buffer;
struct Rr_Image;

typedef struct Rr_Graph Rr_Graph;
typedef struct Rr_GraphNode Rr_GraphNode;

typedef enum Rr_GraphNodeType
{
    RR_GRAPH_NODE_TYPE_BUILTIN,
    RR_GRAPH_NODE_TYPE_GRAPHICS,
    RR_GRAPH_NODE_TYPE_BLIT,
    RR_GRAPH_NODE_TYPE_PRESENT,
    RR_GRAPH_NODE_TYPE_TRANSFER,
} Rr_GraphNodeType;

typedef union Rr_GraphResourceHandle Rr_GraphBufferHandle;
typedef union Rr_GraphResourceHandle Rr_GraphImageHandle;
typedef union Rr_GraphResourceHandle Rr_GraphResourceHandle;
union Rr_GraphResourceHandle
{
    struct
    {
        RR_HALF_UINTPTR Index;
        RR_HALF_UINTPTR Generation;
    } Values;
    uintptr_t Hash;
};

typedef enum Rr_BlitMode
{
    RR_BLIT_MODE_COLOR,
    RR_BLIT_MODE_DEPTH,
} Rr_BlitMode;

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
    Rr_GraphBufferHandle *ImageHandle;
    uint32_t Slot;
    Rr_LoadOp LoadOp;
    Rr_StoreOp StoreOp;
    Rr_ColorClear ColorClear;
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
    Rr_GraphBufferHandle *ImageHandle;
    uint32_t Slot;
    Rr_LoadOp LoadOp;
    Rr_StoreOp StoreOp;
    Rr_DepthClear Clear;
};

typedef enum Rr_PresentMode
{
    RR_PRESENT_MODE_NORMAL,
    RR_PRESENT_MODE_STRETCH,
    RR_PRESENT_MODE_FIT,
} Rr_PresentMode;

extern Rr_GraphBufferHandle Rr_RegisterGraphBuffer(Rr_App *App, struct Rr_Buffer *Buffer);

extern Rr_GraphImageHandle Rr_RegisterGraphImage(Rr_App *App, struct Rr_Image *Image);

extern Rr_GraphNode *Rr_AddPresentNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphImageHandle *ImageHandle,
    struct Rr_Sampler *Sampler,
    Rr_PresentMode Mode);

extern Rr_GraphNode *Rr_AddTransferNode(Rr_App *App, const char *Name, Rr_GraphBufferHandle *DstBufferHandle);

extern void Rr_TransferBufferData(Rr_App *App, Rr_GraphNode *Node, Rr_Data Data, size_t DstOffset);

extern Rr_GraphNode *Rr_AddBlitNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphImageHandle *SrcImage,
    Rr_GraphImageHandle *DstImage,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    Rr_BlitMode Mode);

extern Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_App *App,
    const char *Name,
    Rr_ColorTarget *ColorTargets,
    size_t ColorTargetCount,
    Rr_DepthTarget *DepthTarget);

extern void Rr_DrawIndexed(
    Rr_GraphNode *Node,
    uint32_t IndexCount,
    uint32_t InstanceCount,
    uint32_t FirstIndex,
    int32_t VertexOffset,
    uint32_t FirstInstance);

extern void Rr_BindVertexBuffer(Rr_GraphNode *Node, Rr_GraphBufferHandle *BufferHandle, uint32_t Slot, uint32_t Offset);

extern void Rr_BindIndexBuffer(
    Rr_GraphNode *Node,
    Rr_GraphBufferHandle *BufferHandle,
    uint32_t Slot,
    uint32_t Offset,
    Rr_IndexType Type);

extern void Rr_BindGraphicsPipeline(Rr_GraphNode *Node, Rr_GraphicsPipeline *GraphicsPipeline);

extern void Rr_SetViewport(Rr_GraphNode *Node, Rr_Vec4 Rect);

extern void Rr_SetScissor(Rr_GraphNode *Node, Rr_IntVec4 Rect);

extern void Rr_BindGraphicsUniformBuffer(
    Rr_GraphNode *Node,
    Rr_GraphBufferHandle *BufferHandle,
    uint32_t Set,
    uint32_t Binding,
    uint32_t Offset,
    uint32_t Size);

extern void Rr_BindCombinedImageSampler(
    Rr_GraphNode *Node,
    Rr_GraphImageHandle *ImageHandle,
    struct Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

#ifdef __cplusplus
}
#endif
