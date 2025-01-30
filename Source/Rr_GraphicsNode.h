#pragma once

#include <Rr/Rr_GraphicsNode.h>

#include "Rr_Image.h"
#include "Rr_Vulkan.h"
#include "Rr_Descriptor.h"

#include <Rr/Rr_Graph.h>

struct Rr_GraphBatch;

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

typedef struct Rr_BindBufferArgs Rr_BindIndexBufferArgs;
typedef struct Rr_BindBufferArgs Rr_BindIndexBufferArgs;
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
    VkPipelineStageFlags Stages;
};

extern bool Rr_BatchGraphicsNode(Rr_App *App, struct Rr_GraphBatch *Batch, Rr_GraphicsNode *Node);

extern void Rr_ExecuteGraphicsNode(Rr_App *App, Rr_GraphicsNode *Node, Rr_Arena *Arena);
