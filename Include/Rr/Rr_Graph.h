/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
    RR_GRAPH_NODE_TYPE_CLEAR_COLOR_IMAGE,
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

typedef struct Rr_DrawIndirectCommand Rr_DrawIndirectCommand;
struct Rr_DrawIndirectCommand
{
    uint32_t VertexCount;
    uint32_t InstanceCount;
    uint32_t FirstVertex;
    uint32_t FirstInstance;
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
    Rr_Image2D *SrcImage,
    Rr_Image2D *DstImage,
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
    Rr_Image2D **ColorImages,
    Rr_DepthTarget *DepthTarget,
    Rr_Image2D *DepthImage);

extern Rr_GraphNode *Rr_AddClearColorImageNode(
    Rr_Renderer *Renderer,
    const char *Name,
    Rr_ColorClear *ColorClear,
    Rr_Image2D *Image);

extern void Rr_Draw(
    Rr_GraphNode *Node,
    size_t VertexCount,
    size_t InstanceCount,
    size_t FirstVertex,
    size_t FirstInstance);

extern void Rr_DrawIndirect(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    size_t Offset,
    size_t Count,
    size_t Stride);

extern void Rr_DrawIndexed(
    Rr_GraphNode *Node,
    size_t IndexCount,
    size_t InstanceCount,
    size_t FirstIndex,
    int32_t VertexOffset,
    size_t FirstInstance);

extern void Rr_BindVertexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    size_t Slot,
    size_t Offset);

extern void Rr_BindIndexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    size_t Slot,
    size_t Offset,
    Rr_IndexType Type);

extern void Rr_BindGraphicsPipeline(
    Rr_GraphNode *Node,
    Rr_GraphicsPipeline *GraphicsPipeline);

extern void Rr_SetViewport(Rr_GraphNode *Node, Rr_Rect *Rect);

extern void Rr_SetScissor(Rr_GraphNode *Node, Rr_IntRect *Rect);

extern void Rr_BindSampler(
    Rr_GraphNode *Node,
    Rr_Sampler *Sampler,
    size_t Set,
    size_t Binding);

extern void Rr_BindSampledImage(
    Rr_GraphNode *Node,
    Rr_Image2D *Image,
    size_t Set,
    size_t Binding);

extern void Rr_BindCombinedImageSampler(
    Rr_GraphNode *Node,
    Rr_Image2D *Image,
    Rr_Sampler *Sampler,
    size_t Set,
    size_t Binding);

extern void Rr_BindUniformBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    size_t Set,
    size_t Binding,
    size_t Offset,
    size_t Size);

extern void Rr_BindStorageBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    size_t Set,
    size_t Binding,
    size_t Offset,
    size_t Size);

extern void Rr_BindStorageImage(
    Rr_GraphNode *Node,
    Rr_Image2D *Image,
    size_t Set,
    size_t Binding);

#ifdef __cplusplus
}
#endif
