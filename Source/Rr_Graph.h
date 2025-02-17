#pragma once

#include <Rr/Rr_Graph.h>

#include "Rr_Buffer.h"
#include "Rr_Image.h"
#include "Rr_Vulkan.h"

struct Rr_Frame;

typedef RR_SLICE(size_t) Rr_IndexSlice;
typedef RR_SLICE(Rr_GraphNode *) Rr_NodeSlice;

/* Nodes */

typedef struct Rr_Transfer Rr_Transfer;
struct Rr_Transfer
{
    size_t DstOffset;
    size_t SrcOffset;
    size_t Size;
};

typedef struct Rr_TransferNode Rr_TransferNode;
struct Rr_TransferNode
{
    Rr_GraphBuffer DstBufferHandle;
    RR_SLICE(Rr_Transfer) Transfers;
};

typedef enum
{
    RR_GRAPHICS_NODE_FUNCTION_TYPE_NO_OP,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_DRAW,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_DRAW_INDEXED,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_SET_VIEWPORT,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_SET_SCISSOR,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_SAMPLER,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_SAMPLED_IMAGE,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_COMBINED_IMAGE_SAMPLER,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_STORAGE_BUFFER,
} Rr_GraphicsNodeFunctionType;

typedef struct Rr_GraphicsNodeFunction Rr_GraphicsNodeFunction;
struct Rr_GraphicsNodeFunction
{
    Rr_GraphicsNodeFunctionType Type;
    void *Args;
    Rr_GraphicsNodeFunction *Next;
};

typedef struct Rr_GraphicsNode Rr_GraphicsNode;
struct Rr_GraphicsNode
{
    size_t ColorTargetCount;
    Rr_ColorTarget *ColorTargets;
    Rr_GraphImage *ColorImages;
    Rr_DepthTarget *DepthTarget;
    Rr_GraphImage DepthImage;
    Rr_GraphicsNodeFunction *EncodedFirst;
    Rr_GraphicsNodeFunction *Encoded;
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

typedef struct Rr_DrawArgs Rr_DrawArgs;
struct Rr_DrawArgs
{
    uint32_t VertexCount;
    uint32_t InstanceCount;
    uint32_t FirstVertex;
    uint32_t FirstInstance;
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

typedef struct Rr_PresentNode Rr_PresentNode;
struct Rr_PresentNode
{
    Rr_GraphImage ImageHandle;
    Rr_PresentMode Mode;
    Rr_Sampler *Sampler;
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

typedef enum
{
    RR_NODE_DEPENDENCY_TYPE_READ_BIT = 1,
    RR_NODE_DEPENDENCY_TYPE_WRITE_BIT = 2,
} Rr_NodeDependencyTypeBits;
typedef uint32_t Rr_NodeDependencyType;

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
        Rr_GraphicsNode Graphics;
        Rr_PresentNode Present;
        Rr_BlitNode Blit;
        Rr_TransferNode Transfer;
    } Union;
    Rr_GraphNodeType Type;
    const char *Name;
    size_t OriginalIndex;
    size_t DependencyLevel;
    RR_SLICE(Rr_NodeDependency) Dependencies;
    Rr_Graph *Graph;
};

typedef struct Rr_GraphResource Rr_GraphResource;
struct Rr_GraphResource
{
    void *Container;
    void *Allocated;
    bool IsImage;
};

struct Rr_Graph
{
    RR_SLICE(Rr_GraphNode *) Nodes;
    RR_SLICE(Rr_GraphNode *) RootNodes;
    RR_SLICE(Rr_GraphResource) Resources;
    Rr_Map *ResourceWriteToNode;
    Rr_Arena *Arena;
};

extern Rr_GraphNode *Rr_AddGraphNode(
    struct Rr_Frame *Frame,
    Rr_GraphNodeType Type,
    const char *Name);

extern void Rr_ExecuteGraph(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena);

extern void Rr_ExecutePresentNode(
    Rr_App *App,
    Rr_PresentNode *Node,
    VkCommandBuffer CommandBuffer);

extern void Rr_ExecuteTransferNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_TransferNode *Node,
    VkCommandBuffer CommandBuffer);

extern void Rr_ExecuteBlitNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_BlitNode *Node,
    VkCommandBuffer CommandBuffer);

extern void Rr_ExecuteGraphicsNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node,
    VkCommandBuffer CommandBuffer);
