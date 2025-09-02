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

typedef enum
{
    RR_GRAPH_FLAGS_GRAPHICS_BIT = (1 << 0),
    RR_GRAPH_FLAGS_TRANSFER_BIT = (1 << 1),
    RR_GRAPH_FLAGS_COMPUTE_BIT = (1 << 2),
} Rr_GraphFlagsBits;

typedef uint32_t Rr_GraphFlags;

typedef struct Rr_Graph Rr_Graph;

extern struct Rr_Graph *Rr_GetGraph(void);

extern struct Rr_Graph *Rr_GetSubGraph(Rr_GraphFlags Flags);

extern void Rr_SubmitSubGraph(struct Rr_Graph *Graph);

/* TODO: Move this enum. */

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
    struct Rr_Image *Image;
    uint32_t ImageLayerIndex;
    struct Rr_Image *ResolveImage;
    uint32_t ResolveImageLayerIndex;
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
    struct Rr_Image *Image;
    uint32_t ImageLayerIndex;
};

typedef struct Rr_DrawIndirectCommand Rr_DrawIndirectCommand;
struct Rr_DrawIndirectCommand
{
    uint32_t VertexCount;
    uint32_t InstanceCount;
    uint32_t FirstVertex;
    uint32_t FirstInstance;
};

typedef struct Rr_GraphNode Rr_GraphNode;
typedef struct Rr_TransferNode Rr_TransferNode;

extern void Rr_SetNextNodeName(Rr_Graph *Graph, const char *Name);

extern Rr_TransferNode *Rr_AddTransferNode(Rr_Graph *Graph);

extern void Rr_TransferBufferData(
    Rr_TransferNode *Node,
    uint64_t Size,
    Rr_Buffer *SrcBuffer,
    uint64_t SrcOffset,
    Rr_Buffer *DstBuffer,
    uint64_t DstOffset);

extern void Rr_CopyBufferToImage2D(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_Image2D *Image,
    uint32_t MipLevel);

extern void Rr_CopyBufferToImage2DArray(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_Image2DArray *Image2DArray,
    uint32_t ArrayIndex,
    uint32_t MipLevel);

extern void Rr_CopyBufferToImage3D(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec3 Extent,
    Rr_Image3D *Image3D,
    uint32_t MipLevel);

extern void Rr_CopyBufferToImageCube(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_ImageCube *ImageCube,
    Rr_ImageCubeFace Face,
    uint32_t MipLevel);

extern void Rr_CopyBufferToImageCubeEx(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_ImageCube *ImageCube,
    Rr_ImageCubeFace FirstFace,
    Rr_ImageCubeFace LastFace,
    uint32_t MipLevel);

extern void Rr_CopyImage2D(
    Rr_Graph *Graph,
    Rr_Image2D *SrcImage,
    Rr_IntVec2 SrcOffset,
    Rr_Image2D *DstImage,
    Rr_IntVec2 DstOffset,
    Rr_IntVec2 Extent,
    uint32_t MipLevel);

extern void Rr_BlitImage2D(
    Rr_Graph *Graph,
    Rr_Image2D *SrcImage,
    Rr_Image2D *DstImage,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    Rr_ImageAspect ImageAspect);

extern void Rr_ClearColorImage2D(
    Rr_Graph *Graph,
    Rr_ColorClear *ColorClear,
    Rr_Image2D *Image);

extern void Rr_ResolveImage2D(
    Rr_Graph *Graph,
    Rr_Image2D *SrcImage,
    uint32_t SrcImageLayerIndex,
    Rr_Image2D *DstImage,
    uint32_t DstImageLayerIndex,
    Rr_ImageAspect Aspect);

extern Rr_GraphNode *Rr_AddComputeNode(Rr_Graph *Graph);

extern void Rr_BindComputePipeline(
    Rr_GraphNode *Node,
    Rr_ComputePipeline *ComputePipeline);

extern void Rr_Dispatch(
    Rr_GraphNode *Node,
    uint32_t GroupCountX,
    uint32_t GroupCountY,
    uint32_t GroupCountZ);

extern void Rr_ComputeBarrier(Rr_GraphNode *Node);

extern Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_Graph *Graph,
    size_t ColorTargetCount,
    Rr_ColorTarget *ColorTargets,
    Rr_DepthTarget *DepthTarget);

extern void Rr_Draw(
    Rr_GraphNode *Node,
    uint32_t VertexCount,
    uint32_t InstanceCount,
    uint32_t FirstVertex,
    uint32_t FirstInstance);

extern void Rr_DrawIndirect(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint64_t Offset,
    uint32_t Count,
    uint32_t Stride);

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
    uint64_t Offset);

extern void Rr_BindIndexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Slot,
    uint64_t Offset,
    Rr_IndexType Type);

extern void Rr_BindGraphicsPipeline(
    Rr_GraphNode *Node,
    Rr_GraphicsPipeline *GraphicsPipeline);

extern void Rr_SetViewport(Rr_GraphNode *Node, Rr_Rect *Rect);

extern void Rr_SetScissor(Rr_GraphNode *Node, Rr_IntRect *Rect);

extern void Rr_BindSampler(
    Rr_GraphNode *Node,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindSamplerAt(
    Rr_GraphNode *Node,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindSampledImage2D(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindSampledImage2DAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindSampledImage2DArray(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindSampledImage2DArrayAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindSampledImage3D(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindSampledImage3DAt(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindSampledImageCube(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindSampledImageCubeAt(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindCombinedImage2DSampler(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindCombinedImage2DSamplerAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindCombinedImage2DArraySampler(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindCombinedImage2DArraySamplerAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindCombinedImage3DSampler(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindCombinedImage3DSamplerAt(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindCombinedImageCubeSampler(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindCombinedImageCubeSamplerAt(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindUniformBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint64_t Offset,
    uint64_t Size);

extern void Rr_BindUniformBufferAt(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    uint64_t Offset,
    uint64_t Size);

extern void Rr_BindStorageBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint64_t Offset,
    uint64_t Size);

extern void Rr_BindStorageBufferAt(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    uint64_t Offset,
    uint64_t Size);

extern void Rr_BindStorageBufferRW(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint64_t Offset,
    uint64_t Size);

extern void Rr_BindStorageBufferRWAt(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    uint64_t Offset,
    uint64_t Size);

extern void Rr_BindStorageImage2D(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindStorageImage2DAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void Rr_BindStorageImage2DRW(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding);

extern void Rr_BindStorageImage2DRWAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

#ifdef __cplusplus
}
#endif
