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

#include <Rr/Rr_Graph.h>

#include "Rr_Image.h"
#include "Rr_Vulkan.h"

struct Rr_Frame;

typedef RR_ARRAY(size_t) Rr_IndexArray;
typedef RR_ARRAY(Rr_GraphNode *) Rr_NodeArray;

typedef enum Rr_GraphNodeType
{
    RR_GRAPH_NODE_TYPE_COMPUTE,
    RR_GRAPH_NODE_TYPE_GRAPHICS,
    RR_GRAPH_NODE_TYPE_CLEAR_COLOR_IMAGE,
    RR_GRAPH_NODE_TYPE_BLIT,
    RR_GRAPH_NODE_TYPE_TRANSFER,
    RR_GRAPH_NODE_TYPE_COPY_BUFFER_TO_IMAGE,
    RR_GRAPH_NODE_TYPE_COPY_IMAGE,
} Rr_GraphNodeType;

typedef enum
{
    RR_NODE_FUNCTION_TYPE_NO_OP,
    RR_NODE_FUNCTION_TYPE_BIND_COMPUTE_PIPELINE,
    RR_NODE_FUNCTION_TYPE_DISPATCH,
    RR_NODE_FUNCTION_TYPE_DRAW,
    RR_NODE_FUNCTION_TYPE_DRAW_INDIRECT,
    RR_NODE_FUNCTION_TYPE_DRAW_INDEXED,
    RR_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER,
    RR_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER,
    RR_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE,
    RR_NODE_FUNCTION_TYPE_SET_VIEWPORT,
    RR_NODE_FUNCTION_TYPE_SET_SCISSOR,
    RR_NODE_FUNCTION_TYPE_BIND_SAMPLER,
    RR_NODE_FUNCTION_TYPE_BIND_SAMPLED_IMAGE,
    RR_NODE_FUNCTION_TYPE_BIND_COMBINED_IMAGE_SAMPLER,
    RR_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER,
    RR_NODE_FUNCTION_TYPE_BIND_STORAGE_BUFFER,
    RR_NODE_FUNCTION_TYPE_BIND_STORAGE_IMAGE,
} Rr_NodeFunctionType;

typedef struct Rr_NodeFunction Rr_NodeFunction;
struct Rr_NodeFunction
{
    Rr_NodeFunctionType Type;
    void *Args;
    Rr_NodeFunction *Next;
};

typedef struct Rr_Encoded Rr_Encoded;
struct Rr_Encoded
{
    Rr_NodeFunction *EncodedFirst;
    Rr_NodeFunction *Encoded;
};

typedef struct Rr_ComputeNode Rr_ComputeNode;
struct Rr_ComputeNode
{
    Rr_Encoded Encoded;
};

typedef struct Rr_GraphicsNode Rr_GraphicsNode;
struct Rr_GraphicsNode
{
    Rr_Encoded Encoded;
    size_t ColorTargetCount;
    Rr_ColorTarget *ColorTargets;
    Rr_GraphImage *ColorImages;
    Rr_DepthTarget *DepthTarget;
    Rr_GraphImage DepthImage;
};

typedef struct Rr_ClearColorImageNode Rr_ClearColorImageNode;
struct Rr_ClearColorImageNode
{
    Rr_ColorClear ColorClear;
    Rr_GraphImage ColorImage;
};

typedef struct Rr_CopyBufferToImageNode Rr_CopyBufferToImageNode;
struct Rr_CopyBufferToImageNode
{
    Rr_GraphBuffer Buffer;
    size_t BufferOffset;
    Rr_GraphImage Image;
    uint32_t BaseLayer;
    uint32_t LayerCount;
    uint32_t MipLevel;
};

typedef struct Rr_CopyImageNode Rr_CopyImageNode;
struct Rr_CopyImageNode
{
    Rr_GraphImage SrcImage;
    Rr_IntVec3 SrcOffset;
    Rr_GraphImage DstImage;
    Rr_IntVec3 DstOffset;
    Rr_IntVec3 Extent;
    uint32_t BaseLayer;
    uint32_t LayerCount;
    uint32_t MipLevel;
};

typedef struct Rr_BindIndexBufferArgs Rr_BindIndexBufferArgs;
struct Rr_BindIndexBufferArgs
{
    Rr_GraphBuffer BufferHandle;
    uint32_t Slot;
    uint32_t Offset;
    VkIndexType Type;
};

typedef struct Rr_BindBufferArgs Rr_BindBufferArgs;
struct Rr_BindBufferArgs
{
    Rr_GraphBuffer BufferHandle;
    uint32_t Slot;
    uint32_t Offset;
};

typedef struct Rr_DispatchArgs Rr_DispatchArgs;
struct Rr_DispatchArgs
{
    uint32_t GroupCountX;
    uint32_t GroupCountY;
    uint32_t GroupCountZ;
};

typedef struct Rr_DrawArgs Rr_DrawArgs;
struct Rr_DrawArgs
{
    uint32_t VertexCount;
    uint32_t InstanceCount;
    uint32_t FirstVertex;
    uint32_t FirstInstance;
};

typedef struct Rr_DrawIndirectArgs Rr_DrawIndirectArgs;
struct Rr_DrawIndirectArgs
{
    Rr_GraphBuffer BufferHandle;
    uint32_t Offset;
    uint32_t Count;
    uint32_t Stride;
};

typedef struct Rr_DrawIndexedArgs Rr_DrawIndexedArgs;
struct Rr_DrawIndexedArgs
{
    uint32_t IndexCount;
    uint32_t InstanceCount;
    uint32_t FirstIndex;
    int32_t VertexOffset;
    uint32_t FirstInstance;
};

typedef struct Rr_BindSamplerArgs Rr_BindSamplerArgs;
struct Rr_BindSamplerArgs
{
    Rr_Sampler *Sampler;
    VkImageLayout Layout;
    uint32_t Set;
    uint32_t Binding;
};

typedef struct Rr_BindSampledImageArgs Rr_BindSampledImageArgs;
struct Rr_BindSampledImageArgs
{
    Rr_GraphImage ImageHandle;
    VkImageLayout Layout;
    uint32_t Set;
    uint32_t Binding;
};

typedef struct Rr_BindCombinedImageSamplerArgs Rr_BindCombinedImageSamplerArgs;
struct Rr_BindCombinedImageSamplerArgs
{
    Rr_GraphImage ImageHandle;
    Rr_Sampler *Sampler;
    VkImageLayout Layout;
    uint32_t Set;
    uint32_t Binding;
};

typedef struct Rr_BindUniformBufferArgs Rr_BindUniformBufferArgs;
struct Rr_BindUniformBufferArgs
{
    Rr_GraphBuffer BufferHandle;
    uint32_t Set;
    uint32_t Binding;
    uint32_t Offset;
    uint32_t Size;
};

typedef struct Rr_BindStorageBufferArgs Rr_BindStorageBufferArgs;
struct Rr_BindStorageBufferArgs
{
    Rr_GraphBuffer BufferHandle;
    uint32_t Set;
    uint32_t Binding;
    uint32_t Offset;
    uint32_t Size;
};

typedef struct Rr_BindStorageImageArgs Rr_BindStorageImageArgs;
struct Rr_BindStorageImageArgs
{
    Rr_GraphImage ImageHandle;
    uint32_t Set;
    uint32_t Binding;
};

typedef struct Rr_Transfer Rr_Transfer;
struct Rr_Transfer
{
    size_t Size;
    Rr_GraphBuffer SrcBuffer;
    size_t SrcOffset;
    Rr_GraphBuffer DstBuffer;
    size_t DstOffset;
};

typedef struct Rr_TransferNode Rr_TransferNode;
struct Rr_TransferNode
{
    RR_ARRAY(Rr_Transfer) Transfers;
};

typedef struct Rr_BlitNode Rr_BlitNode;
struct Rr_BlitNode
{
    Rr_GraphImage SrcImageHandle;
    Rr_GraphImage DstImageHandle;
    Rr_IntVec4 SrcRect;
    Rr_IntVec4 DstRect;
    Rr_BlitMode Mode;
    VkImageAspectFlags AspectMask;
};

typedef struct Rr_NodeDependency Rr_NodeDependency;
struct Rr_NodeDependency
{
    Rr_SyncState State;
    Rr_GraphHandle Handle;
};

struct Rr_GraphNode
{
    union
    {
        Rr_ComputeNode Compute;
        Rr_GraphicsNode Graphics;
        Rr_ClearColorImageNode ClearColorImage;
        Rr_CopyBufferToImageNode CopyBufferToImage;
        Rr_CopyImageNode CopyImage;
        Rr_BlitNode Blit;
        Rr_TransferNode Transfer;
    } Union;
    Rr_GraphNodeType Type;
    const char *Name;
    size_t OriginalIndex;
    size_t DependencyLevel;
    RR_ARRAY(Rr_NodeDependency) Dependencies;
    Rr_Graph *Graph;
    bool UsesLateCommandBuffer;
};

typedef struct Rr_GraphResource Rr_GraphResource;
struct Rr_GraphResource
{
    Rr_GraphHandle Handle;
    void *Container;
    void *Allocated;
    uint32_t Generation;
    bool IsImage;
};

struct Rr_Graph
{
    RR_ARRAY(Rr_GraphNode *) Nodes;
    RR_ARRAY(Rr_GraphResource) Resources;
    Rr_Map *Handles;
    Rr_Map *ResourceWriteToNode;
    uint32_t SwapchainImageResourceIndex;
    struct Rr_Frame *Frame;
};

extern Rr_GraphBuffer *Rr_GetGraphBufferHandle(
    Rr_Graph *Graph,
    void *Container);

extern Rr_GraphImage *Rr_GetGraphImageHandle(Rr_Graph *Graph, void *Container);

extern Rr_GraphNode *Rr_AddGraphNode(
    struct Rr_Frame *Frame,
    Rr_GraphNodeType Type,
    const char *Name);

extern void Rr_ExecuteGraph(Rr_Graph *Graph, Rr_Arena *Arena);
