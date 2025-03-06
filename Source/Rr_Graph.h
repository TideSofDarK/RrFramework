#pragma once

#include <Rr/Rr_Graph.h>

#include "Rr_Vulkan.h"

struct Rr_Frame;

typedef RR_SLICE(size_t) Rr_IndexSlice;
typedef RR_SLICE(Rr_GraphNode *) Rr_NodeSlice;

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
    RR_SLICE(Rr_Transfer) Transfers;
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
        Rr_BlitNode Blit;
        Rr_TransferNode Transfer;
    } Union;
    Rr_GraphNodeType Type;
    const char *Name;
    size_t OriginalIndex;
    size_t DependencyLevel;
    RR_SLICE(Rr_NodeDependency) Dependencies;
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
    RR_SLICE(Rr_GraphNode *) Nodes;
    RR_SLICE(Rr_GraphResource) Resources;
    Rr_Map *Handles;
    Rr_Map *ResourceWriteToNode;
    uint32_t SwapchainImageResourceIndex;
    Rr_Arena *Arena;
};

extern Rr_GraphBuffer *Rr_GetGraphBufferHandle(
    Rr_Graph *Graph,
    Rr_Buffer *Buffer);

extern Rr_GraphImage *Rr_GetGraphImageHandle(Rr_Graph *Graph, Rr_Image *Image);

extern Rr_GraphNode *Rr_AddGraphNode(
    struct Rr_Frame *Frame,
    Rr_GraphNodeType Type,
    const char *Name);

extern void Rr_ExecuteGraph(
    Rr_Renderer *Renderer,
    Rr_Graph *Graph,
    Rr_Arena *Arena);
