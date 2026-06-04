/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef RR_GRAPH_H
#define RR_GRAPH_H

#include <Rr/Rr_Buffer.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_Pipeline.h>
#include <Rr/Rr_Renderer.h>

typedef struct Rr_Graph Rr_Graph;
typedef struct Rr_GraphNode Rr_GraphNode;
typedef struct Rr_TransferNode Rr_TransferNode;

#ifdef __cplusplus
extern "C" {
#endif

extern Rr_Graph *RR_CC Rr_GetGraph(void);

extern Rr_Graph *RR_CC Rr_BeginGraph(Rr_QueueType QueueType);

extern void RR_CC Rr_EndGraph(Rr_Graph *Graph);

extern void RR_CC Rr_SetNextNodeName(Rr_Graph *Graph, const char *Name);

extern void RR_CC Rr_BeginGraphLabel(Rr_Graph *Graph, const char *Name);

extern void RR_CC Rr_EndGraphLabel(Rr_Graph *Graph, const char *Name);

extern void RR_CC Rr_TransferBufferToQueue(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    Rr_QueueType QueueType);

extern void RR_CC Rr_TransferImage2DToQueue(
    Rr_Graph *Graph,
    Rr_Image2D *Image,
    Rr_QueueType QueueType);

extern void RR_CC Rr_TransferImage2DArrayToQueue(
    Rr_Graph *Graph,
    Rr_Image2DArray *Image,
    Rr_QueueType QueueType);

extern void RR_CC Rr_TransferImage3DToQueue(
    Rr_Graph *Graph,
    Rr_Image3D *Image,
    Rr_QueueType QueueType);

extern void RR_CC Rr_TransferImageCubeToQueue(
    Rr_Graph *Graph,
    Rr_ImageCube *Image,
    Rr_QueueType QueueType);

/* Allows multiple writes to the same buffer. */
extern Rr_TransferNode *RR_CC Rr_AddTransferNode(Rr_Graph *Graph);

extern void RR_CC Rr_TransferBufferData(
    Rr_TransferNode *Node,
    uint64_t Size,
    Rr_Buffer *SrcBuffer,
    uint64_t SrcOffset,
    Rr_Buffer *DstBuffer,
    uint64_t DstOffset);

extern void RR_CC Rr_CopyBufferToImage2D(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_Image2D *Image,
    uint32_t MipLevel);

extern void RR_CC Rr_CopyBufferToImage2DArray(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_Image2DArray *Image2DArray,
    uint32_t ArrayIndex,
    uint32_t MipLevel);

extern void RR_CC Rr_CopyBufferToImage3D(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec3 Extent,
    Rr_Image3D *Image3D,
    uint32_t MipLevel);

extern void RR_CC Rr_CopyBufferToImageCube(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_ImageCube *ImageCube,
    Rr_ImageCubeFace Face,
    uint32_t MipLevel);

extern void RR_CC Rr_CopyBufferToImageCubeEx(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset,
    Rr_IntVec2 Extent,
    Rr_ImageCube *ImageCube,
    Rr_ImageCubeFace FirstFace,
    Rr_ImageCubeFace LastFace,
    uint32_t MipLevel);

extern void RR_CC Rr_CopyImage2DToBuffer(
    Rr_Graph *Graph,
    Rr_Image2D *Image,
    Rr_IntVec2 ImageOffset,
    Rr_IntVec2 ImageExtent,
    Rr_ImageAspect ImageAspect,
    uint32_t ImageMipLevel,
    Rr_Buffer *Buffer,
    uint64_t BufferOffset);

extern void RR_CC Rr_CopyImage2D(
    Rr_Graph *Graph,
    Rr_Image2D *SrcImage,
    Rr_IntVec2 SrcOffset,
    Rr_Image2D *DstImage,
    Rr_IntVec2 DstOffset,
    Rr_IntVec2 Extent,
    uint32_t MipLevel);

extern void RR_CC Rr_CopyImageCube(
    Rr_Graph *Graph,
    Rr_ImageCube *SrcImage,
    Rr_ImageCube *DstImage,
    uint32_t MipLevel);

extern void RR_CC Rr_BlitImage2D(
    Rr_Graph *Graph,
    Rr_Image2D *SrcImage,
    Rr_Image2D *DstImage,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    Rr_ImageAspect ImageAspect);

extern void RR_CC Rr_GenerateMipmaps(Rr_Graph *Graph, struct Rr_Image *Image);

extern void RR_CC Rr_ClearColorImage2D(
    Rr_Graph *Graph,
    Rr_ColorClear ColorClear,
    Rr_Image2D *Image);

extern void RR_CC Rr_ResolveImage2D(
    Rr_Graph *Graph,
    Rr_Image2D *SrcImage,
    uint32_t SrcImageLayerIndex,
    Rr_Image2D *DstImage,
    uint32_t DstImageLayerIndex,
    Rr_ImageAspect Aspect);

extern Rr_GraphNode *RR_CC Rr_AddComputeNode(Rr_Graph *Graph);

extern void RR_CC
Rr_BindComputePipeline(Rr_GraphNode *Node, Rr_ComputePipeline *ComputePipeline);

extern void RR_CC Rr_Dispatch(
    Rr_GraphNode *Node,
    uint32_t GroupCountX,
    uint32_t GroupCountY,
    uint32_t GroupCountZ);

extern void RR_CC Rr_ComputeBarrier(Rr_GraphNode *Node);

extern Rr_GraphNode *RR_CC Rr_AddGraphicsNode(
    Rr_Graph *Graph,
    size_t ColorTargetCount,
    Rr_ColorTarget *ColorTargets,
    Rr_DepthTarget *DepthTarget);

extern void RR_CC Rr_Draw(
    Rr_GraphNode *Node,
    uint32_t VertexCount,
    uint32_t InstanceCount,
    uint32_t FirstVertex,
    uint32_t FirstInstance);

extern void RR_CC Rr_DrawIndirect(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint64_t Offset,
    uint32_t Count,
    uint32_t Stride);

extern void RR_CC Rr_DrawIndexed(
    Rr_GraphNode *Node,
    uint32_t IndexCount,
    uint32_t InstanceCount,
    uint32_t FirstIndex,
    int32_t VertexOffset,
    uint32_t FirstInstance);

extern void RR_CC Rr_DrawIndexedIndirect(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint64_t Offset,
    uint32_t Count,
    uint32_t Stride);

extern void RR_CC Rr_BindVertexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Slot,
    uint64_t Offset);

extern void RR_CC Rr_BindIndexBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Slot,
    uint64_t Offset,
    Rr_IndexType Type);

extern void RR_CC Rr_BindGraphicsPipeline(
    Rr_GraphNode *Node,
    Rr_GraphicsPipeline *GraphicsPipeline);

extern void RR_CC Rr_SetViewport(Rr_GraphNode *Node, Rr_Rect *Rect);

extern void RR_CC Rr_SetScissor(Rr_GraphNode *Node, Rr_IntRect *Rect);

extern void RR_CC Rr_BindSampler(
    Rr_GraphNode *Node,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindSamplerAt(
    Rr_GraphNode *Node,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindSampledImage2D(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindSampledImage2DAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindSampledImage2DArray(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindSampledImage2DArrayAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindSampledImage3D(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindSampledImage3DAt(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindSampledImageCube(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindSampledImageCubeAt(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindCombinedImage2DSampler(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindCombinedImage2DSamplerAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindCombinedImage2DArraySampler(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindCombinedImage2DArraySamplerAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindCombinedImage3DSampler(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindCombinedImage3DSamplerAt(
    Rr_GraphNode *Node,
    Rr_Image3D *Image3D,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindCombinedImageCubeSampler(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindCombinedImageCubeSamplerAt(
    Rr_GraphNode *Node,
    Rr_ImageCube *ImageCube,
    Rr_Sampler *Sampler,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindUniformBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint64_t Offset,
    uint64_t Size);

extern void RR_CC Rr_BindUniformBufferAt(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    uint64_t Offset,
    uint64_t Size);

extern void RR_CC Rr_BindStorageBuffer(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint64_t Offset,
    uint64_t Size);

extern void RR_CC Rr_BindStorageBufferAt(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    uint64_t Offset,
    uint64_t Size);

extern void RR_CC Rr_BindStorageBufferRW(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint64_t Offset,
    uint64_t Size);

extern void RR_CC Rr_BindStorageBufferRWAt(
    Rr_GraphNode *Node,
    Rr_Buffer *Buffer,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    uint64_t Offset,
    uint64_t Size);

extern void RR_CC Rr_BindStorageImage2D(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindStorageImage2DAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindStorageImage2DRW(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindStorageImage2DRWAt(
    Rr_GraphNode *Node,
    Rr_Image2D *Image2D,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindStorageImage2DArray(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindStorageImage2DArrayAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BindStorageImage2DArrayRW(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding);

extern void RR_CC Rr_BindStorageImage2DArrayRWAt(
    Rr_GraphNode *Node,
    Rr_Image2DArray *Image2DArray,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex);

extern void RR_CC Rr_BeginNodeLabel(Rr_GraphNode *Node, const char *Name);

extern void RR_CC Rr_EndNodeLabel(Rr_GraphNode *Node);

#ifdef __cplusplus
}
#endif

#endif
