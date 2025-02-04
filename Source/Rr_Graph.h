#pragma once

#include <Rr/Rr_Graph.h>

#include "Rr_Image.h"
#include "Rr_Buffer.h"
#include "Rr_Vulkan.h"

struct Rr_Frame;

typedef struct Rr_ImageSync Rr_ImageSync;
struct Rr_ImageSync
{
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
    VkImageLayout Layout;
};

typedef struct Rr_BufferSync Rr_BufferSync;
struct Rr_BufferSync
{
    VkPipelineStageFlags StageMask;
    VkAccessFlags AccessMask;
};

typedef struct Rr_GraphBufferRead Rr_GraphBufferRead;
struct Rr_GraphBufferRead
{
    Rr_BufferSync State;
    Rr_GraphBufferHandle Handle;
};

typedef struct Rr_GraphBufferWrite Rr_GraphBufferWrite;
struct Rr_GraphBufferWrite
{
    Rr_BufferSync State;
    Rr_GraphBufferHandle Handle;
};

typedef struct Rr_GraphImageRead Rr_GraphImageRead;
struct Rr_GraphImageRead
{
    Rr_ImageSync State;
    Rr_GraphImageHandle Handle;
};

typedef struct Rr_GraphImageWrite Rr_GraphImageWrite;
struct Rr_GraphImageWrite
{
    Rr_ImageSync State;
    Rr_GraphImageHandle Handle;
};

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
    Rr_GraphBufferHandle DstBufferHandle;
    RR_SLICE(Rr_Transfer) Transfers;
};

typedef enum
{
    RR_GRAPHICS_NODE_FUNCTION_TYPE_NO_OP,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_DRAW_INDEXED,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_SET_VIEWPORT,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_SET_SCISSOR,
    RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER,
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
    Rr_ColorTarget *ColorTargets;
    size_t ColorTargetCount;
    Rr_AllocatedImage **AllocatedColorTargets;

    Rr_DepthTarget *DepthTarget;
    Rr_AllocatedImage *AllocatedDepthTarget;

    Rr_GraphicsNodeFunction *EncodedFirst;
    Rr_GraphicsNodeFunction *Encoded;
};

typedef struct Rr_BindIndexBufferArgs Rr_BindIndexBufferArgs;
struct Rr_BindIndexBufferArgs
{
    Rr_Buffer *Buffer;
    uint32_t Slot;
    uint32_t Offset;
    VkIndexType Type;
};

typedef struct Rr_BindBufferArgs Rr_BindBufferArgs;
struct Rr_BindBufferArgs
{
    Rr_Buffer *Buffer;
    uint32_t Slot;
    uint32_t Offset;
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

typedef struct Rr_BindUniformBufferArgs Rr_BindUniformBufferArgs;
struct Rr_BindUniformBufferArgs
{
    Rr_Buffer *Buffer;
    uint32_t Set;
    uint32_t Binding;
    uint32_t Offset;
    uint32_t Size;
};

typedef struct Rr_PresentNode Rr_PresentNode;
struct Rr_PresentNode
{
    Rr_AllocatedImage *Image;
    Rr_PresentMode Mode;
    VkPipelineStageFlags Stage;
};

typedef struct Rr_BlitNode Rr_BlitNode;
struct Rr_BlitNode
{
    Rr_GraphImageHandle SrcImageHandle;
    Rr_GraphImageHandle DstImageHandle;
    Rr_IntVec4 SrcRect;
    Rr_IntVec4 DstRect;
    Rr_BlitMode Mode;
    VkImageAspectFlags AspectMask;
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
    RR_SLICE(Rr_GraphBufferRead) BufferReads;
    RR_SLICE(Rr_GraphBufferWrite) BufferWrites;
    RR_SLICE(Rr_GraphImageRead) ImageReads;
    RR_SLICE(Rr_GraphImageWrite) ImageWrites;
    Rr_Arena *Arena;
};

typedef struct Rr_GraphEdge Rr_GraphEdge;
struct Rr_GraphEdge
{
    Rr_GraphNode *From;
    Rr_GraphNode *To;
};

typedef struct Rr_GraphBatch Rr_GraphBatch;
struct Rr_GraphBatch
{
    RR_SLICE(Rr_GraphNode *) Nodes;
    RR_SLICE(VkImageMemoryBarrier) ImageBarriers;
    RR_SLICE(VkBuffer) Buffers;
    VkPipelineStageFlags SwapchainImageStage;
    Rr_Map *LocalSync;
    Rr_Arena *Arena;
};

struct Rr_Graph
{
    RR_SLICE(Rr_GraphNode *) Nodes;
    RR_SLICE(Rr_AllocatedBuffer *) Buffers;
    RR_SLICE(Rr_AllocatedImage *) Images;
};

extern Rr_GraphNode *Rr_AddGraphNode(struct Rr_Frame *Frame, Rr_GraphNodeType Type, const char *Name);

extern void Rr_AddBufferRead(Rr_GraphNode *Node, Rr_GraphBufferHandle *Handle);

extern void Rr_AddBufferWrite(Rr_GraphNode *Node, Rr_GraphBufferHandle *Handle);

extern void Rr_ExecuteGraph(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena);

extern bool Rr_BatchImagePossible(Rr_Map **Sync, VkImage Image);

extern void Rr_BatchImage(
    Rr_App *App,
    Rr_GraphBatch *Batch,
    VkImage Image,
    VkImageAspectFlags AspectMask,
    VkPipelineStageFlags StageMask,
    VkAccessFlags AccessMask,
    VkImageLayout Layout);

extern bool Rr_BatchBuffer(
    Rr_App *App,
    Rr_GraphBatch *Batch,
    VkBuffer Buffer,
    VkDeviceSize Size,
    VkDeviceSize Offset,
    VkPipelineStageFlags StageMask,
    VkAccessFlags AccessMask);

extern void Rr_ExecutePresentNode(Rr_App *App, Rr_PresentNode *Node, VkCommandBuffer CommandBuffer);

extern void Rr_ExecuteTransferNode(Rr_App *App, Rr_Graph *Graph, Rr_TransferNode *Node, VkCommandBuffer CommandBuffer);

extern void Rr_ExecuteBlitNode(Rr_App *App, Rr_BlitNode *Node, VkCommandBuffer CommandBuffer);

extern void Rr_ExecuteGraphicsNode(Rr_App *App, Rr_GraphicsNode *Node, Rr_Arena *Arena, VkCommandBuffer CommandBuffer);
