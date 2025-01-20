#pragma once

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Image.h>
#include <Rr/Rr_Material.h>
#include <Rr/Rr_Mesh.h>
#include <Rr/Rr_Pipeline.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_GraphicsNodeInfo Rr_GraphicsNodeInfo;
struct Rr_GraphicsNodeInfo
{
    Rr_DrawTarget *DrawTarget;
    Rr_Image *InitialColor;
    Rr_Image *InitialDepth;
    Rr_IntVec4 Viewport;
    Rr_GraphicsPipeline *BasePipeline;
};

extern Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphicsNodeInfo *Info,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount);

typedef struct Rr_BufferBinding Rr_BufferBinding;
struct Rr_BufferBinding
{
    struct Rr_Buffer *Buffer;
    uint32_t Offset;
    uint32_t Slot;
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

void Rr_DrawIndexed(Rr_GraphNode *Node, Rr_DrawIndexedArgs *Args);

void Rr_BindVertexBuffer(Rr_GraphNode *Node, Rr_BufferBinding *Binding);

void Rr_BindIndexBuffer(Rr_GraphNode *Node, Rr_BufferBinding *Binding);

void Rr_BindGraphicsPipeline(Rr_GraphNode *Node, Rr_GraphicsPipeline *GraphicsPipeline);

// extern void Rr_DrawStaticMesh(Rr_App *App, Rr_GraphNode *Node, Rr_StaticMesh *StaticMesh, Rr_Data PerDrawData);
//
// extern void Rr_DrawStaticMeshOverrideMaterials(
//     Rr_App *App,
//     Rr_GraphNode *Node,
//     Rr_Material **OverrideMaterials,
//     size_t OverrideMaterialCount,
//     Rr_StaticMesh *StaticMesh,
//     Rr_Data PerDrawData);

#ifdef __cplusplus
}
#endif
