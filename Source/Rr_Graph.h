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

#pragma once

#include <Rr/Rr_Graph.h>

#include "Rr_Descriptor.h"

struct Rr_Frame;
struct Rr_SyncStateStorage;

typedef RR_ARRAY(size_t) Rr_IndexArray;
typedef RR_ARRAY(Rr_GraphNode *) Rr_NodeArray;

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

typedef enum Rr_GraphNodeType
{
    RR_GRAPH_NODE_TYPE_COMPUTE,
    RR_GRAPH_NODE_TYPE_GRAPHICS,
    RR_GRAPH_NODE_TYPE_CLEAR_COLOR_IMAGE,
    RR_GRAPH_NODE_TYPE_RESOLVE_IMAGE,
    RR_GRAPH_NODE_TYPE_BLIT,
    RR_GRAPH_NODE_TYPE_TRANSFER,
    RR_GRAPH_NODE_TYPE_COPY_BUFFER_TO_IMAGE,
    RR_GRAPH_NODE_TYPE_COPY_IMAGE_TO_BUFFER,
    RR_GRAPH_NODE_TYPE_COPY_IMAGE,
} Rr_GraphNodeType;

typedef enum
{
    RR_NODE_FUNCTION_TYPE_NO_OP,
    RR_NODE_FUNCTION_TYPE_BIND_COMPUTE_PIPELINE,
    RR_NODE_FUNCTION_TYPE_DISPATCH,
    RR_NODE_FUNCTION_TYPE_COMPUTE_BARRIER,
    RR_NODE_FUNCTION_TYPE_DRAW,
    RR_NODE_FUNCTION_TYPE_DRAW_INDIRECT,
    RR_NODE_FUNCTION_TYPE_DRAW_INDEXED,
    RR_NODE_FUNCTION_TYPE_DRAW_INDEXED_INDIRECT,
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
    RR_NODE_FUNCTION_TYPE_DEBUG_LABEL,
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

/* TODO: Encode macro expects Encoded field to be first. */

typedef struct Rr_ComputeNode Rr_ComputeNode;
struct Rr_ComputeNode
{
    Rr_Encoded Encoded;
};

typedef struct Rr_GraphicsNode Rr_GraphicsNode;
struct Rr_GraphicsNode
{
    Rr_Encoded Encoded;
    uint32_t ColorTargetCount;
    Rr_ColorTarget *ColorTargets;
    Rr_DepthTarget *DepthTarget;
};

typedef struct Rr_ClearColorImageNode Rr_ClearColorImageNode;
struct Rr_ClearColorImageNode
{
    Rr_ColorClear ColorClear;
    Rr_GraphImage ColorImage;
};

typedef struct Rr_ResolveImageNode Rr_ResolveImageNode;
struct Rr_ResolveImageNode
{
    struct Rr_Image *SrcImage;
    uint32_t SrcImageLayerIndex;
    struct Rr_Image *DstImage;
    uint32_t DstImageLayerIndex;
    VkImageAspectFlags AspectFlags;
};

typedef struct Rr_CopyBufferToImageNode Rr_CopyBufferToImageNode;
struct Rr_CopyBufferToImageNode
{
    Rr_GraphBuffer Buffer;
    uint64_t BufferOffset;
    Rr_GraphImage Image;
    VkExtent3D Extent;
    uint32_t BaseLayer;
    uint32_t LayerCount;
    uint32_t MipLevel;
};

typedef struct Rr_CopyImageToBufferNode Rr_CopyImageToBufferNode;
struct Rr_CopyImageToBufferNode
{
    Rr_GraphImage Image;
    Rr_GraphBuffer Buffer;
    uint32_t BufferImageCopyCount;
    VkBufferImageCopy const *BufferImageCopies;
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
    uint64_t Offset;
    VkIndexType Type;
};

typedef struct Rr_BindBufferArgs Rr_BindBufferArgs;
struct Rr_BindBufferArgs
{
    Rr_GraphBuffer BufferHandle;
    uint32_t Slot;
    uint64_t Offset;
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
    uint64_t Offset;
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
    uint32_t ArrayIndex;
};

typedef struct Rr_BindSampledImageArgs Rr_BindSampledImageArgs;
struct Rr_BindSampledImageArgs
{
    Rr_GraphImage ImageHandle;
    VkImageViewType ViewType;
    VkImageSubresourceRange SubresourceRange;
    uint32_t Set;
    uint32_t Binding;
    uint32_t ArrayIndex;
};

typedef struct Rr_BindCombinedImageSamplerArgs Rr_BindCombinedImageSamplerArgs;
struct Rr_BindCombinedImageSamplerArgs
{
    Rr_GraphImage ImageHandle;
    Rr_Sampler *Sampler;
    VkImageViewType ViewType;
    VkImageSubresourceRange SubresourceRange;
    uint32_t Set;
    uint32_t Binding;
    uint32_t ArrayIndex;
};

typedef struct Rr_BindUniformBufferArgs Rr_BindUniformBufferArgs;
struct Rr_BindUniformBufferArgs
{
    Rr_GraphBuffer BufferHandle;
    uint64_t Size;
    uint64_t Offset;
    uint32_t Set;
    uint32_t Binding;
    uint32_t ArrayIndex;
};

typedef struct Rr_BindStorageBufferArgs Rr_BindStorageBufferArgs;
struct Rr_BindStorageBufferArgs
{
    Rr_GraphBuffer BufferHandle;
    uint64_t Size;
    uint64_t Offset;
    uint32_t Set;
    uint32_t Binding;
    uint32_t ArrayIndex;
};

typedef struct Rr_BindStorageImageArgs Rr_BindStorageImageArgs;
struct Rr_BindStorageImageArgs
{
    Rr_GraphImage ImageHandle;
    VkImageViewType ViewType;
    VkImageSubresourceRange SubresourceRange;
    uint32_t Set;
    uint32_t Binding;
    uint32_t ArrayIndex;
};

typedef struct Rr_Transfer Rr_Transfer;
struct Rr_Transfer
{
    uint64_t Size;
    Rr_GraphBuffer SrcBuffer;
    uint64_t SrcOffset;
    Rr_GraphBuffer DstBuffer;
    uint64_t DstOffset;
};

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
    VkImageAspectFlags AspectMask;
};

typedef struct Rr_NodeDependency Rr_NodeDependency;
struct Rr_NodeDependency
{
    Rr_SyncState State;
    Rr_GraphHandle Handle;
};

typedef RR_ARRAY(Rr_NodeDependency) Rr_NodeDependencyArray;

struct Rr_GraphNode
{
    union
    {
        Rr_ComputeNode Compute;
        Rr_GraphicsNode Graphics;
        Rr_ClearColorImageNode ClearColorImage;
        Rr_ResolveImageNode ResolveImage;
        Rr_CopyBufferToImageNode CopyBufferToImage;
        Rr_CopyImageToBufferNode CopyImageToBuffer;
        Rr_CopyImageNode CopyImage;
        Rr_BlitNode Blit;
        Rr_TransferNode Transfer;
    } Union;
    Rr_GraphNodeType Type;

    const char *Name;

    uint32_t OriginalIndex;
    uint32_t DependencyLevel;
    Rr_NodeDependencyArray BufferDeps;
    Rr_NodeDependencyArray ImageDeps;

    bool UsesLateCommandBuffer;
    Rr_PipelineLayout *CurrentLayout;

#ifdef RR_USE_GPU_DEBUG_UTILS
    size_t DebugLabelCount;
    bool *DebugLabelStates;
#endif

    Rr_Graph *Graph;
};

typedef struct Rr_GraphResource Rr_GraphResource;
struct Rr_GraphResource
{
    Rr_GraphHandle Handle;
    void *Container;
    /* TODO: Currently it's being resolved under sync storage lock.
     * Consider resolving on resource creation. */
    void *Allocated;
    uint32_t Generation;
    Rr_SyncState SyncState;
    uint32_t DstQueueFamilyIndex;
};

typedef RR_ARRAY(Rr_GraphResource) Rr_GraphResourceArray;

struct Rr_Graph
{
    Rr_QueueType QueueType;

    RR_ARRAY(Rr_GraphNode *) Nodes;

    Rr_GraphResourceArray BufferResources;
    Rr_Map *BufferWriteToNode;
    Rr_Map *BufferHandles;
    Rr_GraphResourceArray ImageResources;
    Rr_Map *ImageWriteToNode;
    Rr_Map *ImageHandles;

    Rr_GraphImage *SwapchainImageHandle;

    /* TODO: We already have buffers and images available... */
    Rr_HandleSet Buffers;
    Rr_HandleSet Images;
    Rr_HandleSet ComputePipelines;
    Rr_HandleSet GraphicsPipelines;
    Rr_HandleSet Samplers;

    Rr_DescriptorPoolList *DescriptorPoolList;
    VkDescriptorSet EmptyDescriptorSet;

    const char *NextNodeName;

#ifdef RR_USE_GPU_DEBUG_UTILS
    RR_ARRAY(bool) DebugLabelStates;
    RR_ARRAY(const char *) DebugLabelNames;
#endif

    Rr_Arena *Arena;
    uintptr_t ArenaPosition;
};

extern void Rr_MarkBufferUsed(Rr_Graph *Graph, Rr_Buffer *Buffer);

extern void Rr_MarkImageUsed(Rr_Graph *Graph, struct Rr_Image *Image);

extern void Rr_MarkSamplerUsed(Rr_Graph *Graph, Rr_Sampler *Sampler);

extern void Rr_MarkComputePipelineUsed(
    Rr_Graph *Graph,
    Rr_ComputePipeline *ComputePipeline);

extern void Rr_MarkGraphicsPipelineUsed(
    Rr_Graph *Graph,
    Rr_GraphicsPipeline *GraphicsPipeline);

extern void Rr_DecrementRefCounts(Rr_Graph *Graph);

extern Rr_GraphBuffer *Rr_GetGraphBufferHandle(
    Rr_Graph *Graph,
    void *Container);

extern Rr_GraphImage *Rr_GetGraphImageHandle(Rr_Graph *Graph, void *Container);

extern void Rr_ExecuteGraph(
    Rr_Graph *Graph,
    uint32_t QueueFamilyIndex,
    VkCommandBuffer EarlyCommandBuffer,
    VkCommandBuffer LateCommandBuffer);

extern void Rr_FinalizeGraph(Rr_Graph *Graph);
