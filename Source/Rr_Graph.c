#include "Rr_Graph.h"

#include "Rr_App.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_Renderer.h"

#include <assert.h>

static VkAccessFlags AllVulkanReads =
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
    VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT |
    VK_ACCESS_HOST_READ_BIT | VK_ACCESS_MEMORY_READ_BIT;

static VkAccessFlags AllVulkanWrites = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
                                       VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

static Rr_AllocatedBuffer *Rr_GetGraphBuffer(Rr_App *App, Rr_Graph *Graph, Rr_GraphBufferHandle Handle)
{
    return Graph->ResolvedResources.Data[Handle.Values.Index];
}

static Rr_AllocatedImage *Rr_GetGraphImage(Rr_App *App, Rr_Graph *Graph, Rr_GraphImageHandle Handle)
{
    return Graph->ResolvedResources.Data[Handle.Values.Index];
}

Rr_GraphNode *Rr_AddGraphNode(Rr_Frame *Frame, Rr_GraphNodeType Type, const char *Name)
{
    Rr_GraphNode *GraphNode = RR_ALLOC(Frame->Arena, sizeof(Rr_GraphNode));
    GraphNode->Type = Type;
    GraphNode->Name = Name;
    GraphNode->OriginalIndex = Frame->Graph.Nodes.Count;
    GraphNode->Arena = Frame->Arena;

    *RR_PUSH_SLICE(&Frame->Graph.Nodes, Frame->Arena) = GraphNode;

    return GraphNode;
}

static inline void Rr_AddGraphWrite(Rr_Graph *Graph, Rr_GraphNode *Node, Rr_GraphResourceHandle *Handle, Rr_Arena *Arena)
{
    Rr_GraphNode **NodeInMap = RR_UPSERT(&Graph->ResourceWriteToNode, *(uintptr_t *)Handle, Arena);
    if(*NodeInMap == NULL)
    {
        if(Handle->Values.Generation == 0)
        {
            *RR_PUSH_SLICE(&Graph->RootResources, Arena) = *Handle;
        }
        Handle->Values.Generation++;
        *NodeInMap = Node;
    }
    else
    {
        Rr_LogAbort("A versioned resource can only be written once!");
    }
}

static inline Rr_NodeDependencyType Rr_GetNodeDependencyType(VkAccessFlags AccessMask)
{
    Rr_NodeDependencyType DependencyType = 0;
    if(RR_HAS_BIT(AccessMask, AllVulkanWrites))
    {
        DependencyType |= RR_NODE_DEPENDENCY_TYPE_WRITE_BIT;
    }
    if(RR_HAS_BIT(AccessMask, AllVulkanReads))
    {
        DependencyType |= RR_NODE_DEPENDENCY_TYPE_WRITE_BIT;
    }
    return DependencyType;
}

static inline void Rr_AddNodeImageDependency(
    Rr_Graph *Graph,
    Rr_GraphNode *Node,
    Rr_GraphResourceHandle *Handle,
    Rr_ImageSync *State,
    Rr_Arena *Arena)
{
    Rr_NodeDependencyType DependencyType = DependencyType = Rr_GetNodeDependencyType(State->AccessMask);
    *RR_PUSH_SLICE(&Node->Reads, Arena) = (Rr_NodeDependency){
        .State.Image = *State,
        .Handle = *Handle,
        .DependencyType = DependencyType,
        .IsImage = true,
    };
    if(RR_HAS_BIT(DependencyType, RR_NODE_DEPENDENCY_TYPE_WRITE_BIT))
    {
        Rr_AddGraphWrite(Graph, Node, Handle, Arena);
    }
}

static inline void Rr_AddNodeBufferDependency(
    Rr_Graph *Graph,
    Rr_GraphNode *Node,
    Rr_GraphResourceHandle *Handle,
    Rr_BufferSync *State,
    Rr_Arena *Arena)
{
    Rr_NodeDependencyType DependencyType = DependencyType = Rr_GetNodeDependencyType(State->AccessMask);
    *RR_PUSH_SLICE(&Node->Reads, Arena) = (Rr_NodeDependency){
        .State.Buffer = *State,
        .Handle = *Handle,
        .DependencyType = DependencyType,
        .IsImage = false,
    };
    if(RR_HAS_BIT(DependencyType, RR_NODE_DEPENDENCY_TYPE_WRITE_BIT))
    {
        Rr_AddGraphWrite(Graph, Node, Handle, Arena);
    }
}

// Rr_GraphNodeResource *Rr_AddGraphNodeRead(Rr_GraphNode *Node)
// {
//     return RR_PUSH_SLICE(&Node->Reads, Node->Arena);
// }
//
// Rr_GraphNodeResource *Rr_AddGraphNodeWrite(Rr_GraphNode *Node)
// {
//     return RR_PUSH_SLICE(&Node->Reads, Node->Arena);
// }

static void Rr_ExecuteGraphBatch(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_GraphBatch *Batch,
    VkCommandBuffer CommandBuffer,
    Rr_Arena *Arena)
{
    // Rr_Scratch Scratch = Rr_GetScratch(Arena);
    //
    // Rr_Renderer *Renderer = &App->Renderer;
    // Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    //
    // /* Submit barriers. */
    // /* @TODO: People suggesting batching these into two submits: pre vertex stage and post vertex stage. */
    //
    // size_t ImageBarrierCount = Batch->ImageBarriers.Count;
    // for(size_t Index = 0; Index < ImageBarrierCount; ++Index)
    // {
    //     VkImageMemoryBarrier *Barrier = Batch->ImageBarriers.Data + Index;
    //     Rr_ImageSync **GlobalState = RR_UPSERT(&Renderer->GlobalSync, Barrier->image, NULL);
    //     Rr_ImageSync **LocalState = RR_UPSERT(&Batch->LocalSync, Barrier->image, NULL);
    //     VkPipelineStageFlags StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //     if(*GlobalState != NULL)
    //     {
    //         StageMask = (*GlobalState)->StageMask;
    //     }
    //     vkCmdPipelineBarrier(CommandBuffer, StageMask, (*LocalState)->StageMask, 0, 0, NULL, 0, NULL, 1, Barrier);
    // }
    //
    // size_t BufferCount = Batch->Buffers.Count;
    // for(size_t Index = 0; Index < BufferCount; ++Index)
    // {
    //     VkBuffer Buffer = Batch->Buffers.Data[Index];
    //
    //     Rr_BufferSync **GlobalStatePtr = RR_UPSERT(&Renderer->GlobalSync, Buffer, NULL);
    //     Rr_BufferSync *GlobalState = *GlobalStatePtr;
    //     bool HasGlobalState = GlobalState != NULL;
    //
    //     Rr_BufferSync **LocalStatePtr = RR_UPSERT(&Batch->LocalSync, Buffer, NULL);
    //     Rr_BufferSync *LocalState = *LocalStatePtr;
    //
    //     VkBufferMemoryBarrier *Barriers =
    //         RR_ALLOC_STRUCT_COUNT(Scratch.Arena, VkBufferMemoryBarrier, LocalState->RangeCount);
    //
    //     size_t RangeIndex = 0;
    //     for(Rr_BufferRange *Range = LocalState->SentinelRange.Next; Range != NULL; Range = Range->Next, ++RangeIndex)
    //     {
    //         Barriers[RangeIndex] = (VkBufferMemoryBarrier){
    //             .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    //             .pNext = NULL,
    //             .srcAccessMask = HasGlobalState ? GlobalState->AccessMask : 0,
    //             .dstAccessMask = LocalState->AccessMask,
    //             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //             .buffer = Buffer,
    //             .offset = Range->Offset,
    //             .size = Range->Size,
    //         };
    //     }
    //     vkCmdPipelineBarrier(
    //         CommandBuffer,
    //         HasGlobalState ? GlobalState->StageMask : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    //         LocalState->StageMask,
    //         0,
    //         0,
    //         NULL,
    //         RangeIndex,
    //         Barriers,
    //         0,
    //         NULL);
    //     if(GlobalState == NULL)
    //     {
    //         *GlobalStatePtr = RR_ALLOC(App->PermanentArena, sizeof(Rr_BufferSync));
    //         GlobalState = *GlobalStatePtr;
    //     }
    //     *GlobalState = *LocalState;
    // }
    //
    // for(size_t Index = 0; Index < Batch->Nodes.Count; ++Index)
    // {
    //     Rr_GraphNode *GraphNode = Batch->Nodes.Data[Index];
    //
    //     switch(GraphNode->Type)
    //     {
    //         case RR_GRAPH_NODE_TYPE_GRAPHICS:
    //         {
    //             Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.Graphics;
    //             Rr_ExecuteGraphicsNode(App, GraphicsNode, Scratch.Arena);
    //         }
    //         break;
    //         case RR_GRAPH_NODE_TYPE_PRESENT:
    //         {
    //             Rr_PresentNode *PresentNode = &GraphNode->Union.Present;
    //             Rr_ExecutePresentNode(App, PresentNode);
    //         }
    //         break;
    //         case RR_GRAPH_NODE_TYPE_BUILTIN:
    //         {
    //             Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.Builtin;
    //             Rr_ExecuteBuiltinNode(App, BuiltinNode, Scratch.Arena);
    //         }
    //         break;
    //         case RR_GRAPH_NODE_TYPE_BLIT:
    //         {
    //             Rr_BlitNode *BlitNode = &GraphNode->Union.Blit;
    //             Rr_ExecuteBlitNode(App, BlitNode);
    //         }
    //         break;
    //         case RR_GRAPH_NODE_TYPE_TRANSFER:
    //         {
    //             Rr_TransferNode *TransferNode = &GraphNode->Union.Transfer;
    //             Rr_ExecuteTransferNode(App, Graph, TransferNode, Frame->MainCommandBuffer);
    //         }
    //         break;
    //         default:
    //         {
    //             Rr_LogAbort("Unsupported node type!");
    //         }
    //         break;
    //     }
    //
    //     GraphNode->Executed = true;
    // }
    //
    // for(size_t Index = 0; Index < ImageBarrierCount; ++Index)
    // {
    //     VkImageMemoryBarrier *Barrier = Batch->ImageBarriers.Data + Index;
    //     Rr_ImageSync **GlobalState = RR_UPSERT(&Renderer->GlobalSync, Barrier->image, App->PermanentArena);
    //     if(*GlobalState == NULL)
    //     {
    //         *GlobalState = RR_ALLOC(App->PermanentArena, sizeof(Rr_ImageSync));
    //     }
    //     Rr_ImageSync **BatchState = RR_UPSERT(&Batch->LocalSync, Barrier->image, NULL);
    //     *(*GlobalState) = *(*BatchState);
    // }
    //
    // Rr_DestroyScratch(Scratch);
}

typedef RR_SLICE(size_t) Rr_IndexSlice;

static void Rr_CreateGraphAdjacencyList(
    size_t NodeCount,
    Rr_GraphNode **Nodes,
    Rr_IndexSlice *AdjacencyList,
    Rr_Arena *Arena)
{
    for(size_t Index = 0; Index < NodeCount; ++Index)
    {
        for(size_t Index2 = 0; Index2 < NodeCount; ++Index2)
        {
            if(Index == Index2)
            {
                continue;
            }

            Rr_GraphNode *Node = Nodes[Index];
            Rr_GraphNode *Node2 = Nodes[Index2];
            for(size_t ReadIndex = 0; ReadIndex < Node->Reads.Count; ++ReadIndex)
            {
                for(size_t WriteIndex = 0; WriteIndex < Node2->Writes.Count; ++WriteIndex)
                {
                    if(Node->Reads.Data[ReadIndex].Handle.Hash == Node->Writes.Data[WriteIndex].Handle.Hash)
                    {
                        *RR_PUSH_SLICE(&AdjacencyList[Index], Arena) = Index2;
                        break;
                    }
                }
            }
        }
    }
}

static void Rr_SortGraph(size_t CurrentNodeIndex, Rr_IndexSlice *AdjacencyList, int *State, Rr_IndexSlice *Out)
{
    static const int VisitedBit = 1;
    static const int OnStackBit = 2;

    if((State[CurrentNodeIndex] & VisitedBit) != 0)
    {
        if((State[CurrentNodeIndex] & OnStackBit) != 0)
        {
            Rr_LogAbort("Cyclic graph detected!");
        }

        return;
    }

    State[CurrentNodeIndex] |= VisitedBit;
    State[CurrentNodeIndex] |= OnStackBit;

    Rr_IndexSlice *Dependencies = &AdjacencyList[CurrentNodeIndex];
    for(size_t Index = 0; Index < Dependencies->Count; ++Index)
    {
        Rr_SortGraph(Dependencies->Data[Index], AdjacencyList, State, Out);
    }

    *RR_PUSH_SLICE(Out, NULL) = CurrentNodeIndex;

    State[CurrentNodeIndex] &= ~OnStackBit;
}

static void Rr_ProcessGraphNodes(size_t NodeCount, Rr_GraphNode **Nodes, Rr_IndexSlice *Out, Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    RR_SLICE(Rr_IndexSlice) AdjacencyList;
    RR_RESERVE_SLICE(&AdjacencyList, NodeCount, Scratch.Arena);

    Rr_CreateGraphAdjacencyList(NodeCount, Nodes, AdjacencyList.Data, Scratch.Arena);

    int *SortState = RR_ALLOC(Scratch.Arena, sizeof(int) * NodeCount);

    for(size_t Index = 0; Index < NodeCount; ++Index)
    {
        Rr_SortGraph(0, AdjacencyList.Data, SortState, Out);
    }

    Rr_DestroyScratch(Scratch);
}

void Rr_ExecuteGraph(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena)
{
    size_t NodeCount = Graph->Nodes.Count;
    if(NodeCount == 0)
    {
        Rr_LogAbort("Graph doesn't contain any nodes!");
    }

    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    Rr_IndexSlice SortedIndices;
    RR_RESERVE_SLICE(&SortedIndices, NodeCount, Scratch.Arena);

    Rr_ProcessGraphNodes(NodeCount, Graph->Nodes.Data, &SortedIndices, Scratch.Arena);

    // while(true)
    // {
    //     /* Begin a new batch. */
    //
    //     Rr_Scratch Scratch = Rr_GetScratch(Arena);
    //
    //     Rr_GraphBatch Batch = {
    //         .Arena = Scratch.Arena,
    //     };
    //
    //     /* First Graph Pass */
    //     /* Here we go through the nodes and accumulate barriers
    //      * up until  */
    //
    //     for(size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
    //     {
    //         Rr_GraphNode *GraphNode = Graph->Nodes.Data[Index];
    //
    //         bool NodeBatched = false;
    //         switch(GraphNode->Type)
    //         {
    //             case RR_GRAPH_NODE_TYPE_GRAPHICS:
    //             {
    //                 Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.GraphicsNode;
    //                 NodeBatched = Rr_BatchGraphicsNode(App, &Batch, GraphicsNode);
    //             }
    //             break;
    //             case RR_GRAPH_NODE_TYPE_PRESENT:
    //             {
    //                 Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
    //                 NodeBatched = Rr_BatchPresentNode(App, &Batch, PresentNode);
    //             }
    //             break;
    //             case RR_GRAPH_NODE_TYPE_BUILTIN:
    //             {
    //                 Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.BuiltinNode;
    //                 NodeBatched = Rr_BatchBuiltinNode(App, Graph, &Batch, BuiltinNode);
    //             }
    //             break;
    //             case RR_GRAPH_NODE_TYPE_BLIT:
    //             {
    //                 Rr_BlitNode *BlitNode = &GraphNode->Union.BlitNode;
    //                 NodeBatched = Rr_BatchBlitNode(App, Graph, &Batch, BlitNode);
    //             }
    //             break;
    //             case RR_GRAPH_NODE_TYPE_TRANSFER:
    //             {
    //                 Rr_TransferNode *TransferNode = &GraphNode->Union.TransferNode;
    //                 NodeBatched = Rr_BatchTransferNode(App, &Batch, TransferNode);
    //             }
    //             break;
    //             default:
    //             {
    //                 Rr_LogAbort("Unsupported node type!");
    //             }
    //             break;
    //         }
    //     }
    //
    //     for(size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
    //     {
    //         Rr_GraphNode *GraphNode = Graph->Nodes.Data[Index];
    //
    //         if(GraphNode->Executed == true)
    //         {
    //             continue;
    //         }
    //
    //         /* Attempt batching current node. */
    //
    //         bool NodeBatched = false;
    //         switch(GraphNode->Type)
    //         {
    //             case RR_GRAPH_NODE_TYPE_GRAPHICS:
    //             {
    //                 Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.GraphicsNode;
    //                 NodeBatched = Rr_BatchGraphicsNode(App, &Batch, GraphicsNode);
    //             }
    //             break;
    //             case RR_GRAPH_NODE_TYPE_PRESENT:
    //             {
    //                 Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
    //                 NodeBatched = Rr_BatchPresentNode(App, &Batch, PresentNode);
    //             }
    //             break;
    //             case RR_GRAPH_NODE_TYPE_BUILTIN:
    //             {
    //                 Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.BuiltinNode;
    //                 NodeBatched = Rr_BatchBuiltinNode(App, Graph, &Batch, BuiltinNode);
    //             }
    //             break;
    //             case RR_GRAPH_NODE_TYPE_BLIT:
    //             {
    //                 Rr_BlitNode *BlitNode = &GraphNode->Union.BlitNode;
    //                 NodeBatched = Rr_BatchBlitNode(App, Graph, &Batch, BlitNode);
    //             }
    //             break;
    //             case RR_GRAPH_NODE_TYPE_TRANSFER:
    //             {
    //                 Rr_TransferNode *TransferNode = &GraphNode->Union.TransferNode;
    //                 NodeBatched = Rr_BatchTransferNode(App, &Batch, TransferNode);
    //             }
    //             break;
    //             default:
    //             {
    //                 Rr_LogAbort("Unsupported node type!");
    //             }
    //             break;
    //         }
    //         if(NodeBatched)
    //         {
    //             *RR_PUSH_SLICE(&Batch.Nodes, Batch.Arena) = GraphNode;
    //             NodeCount--;
    //         }
    //     }
    //
    //     if(Batch.Nodes.Count == 0)
    //     {
    //         Rr_LogAbort("Couldn't batch graph nodes, probably invalid graph!");
    //     }
    //
    //     Rr_ExecuteGraphBatch(App, Graph, &Batch, Scratch.Arena);
    //
    //     Rr_DestroyScratch(Scratch);
    //
    //     if(NodeCount <= 0)
    //     {
    //         break;
    //     }
    // }
}

bool Rr_BatchImagePossible(Rr_Map **Sync, VkImage Image)
{
    // Rr_ImageSync **State = RR_UPSERT(Sync, Image, NULL);
    //
    // if(State != NULL)
    // {
    //     /* @TODO: Should be true if for example current batch state matches
    //      * requested state. */
    //     return false;
    // }

    return true;
}

void Rr_BatchImage(
    Rr_App *App,
    Rr_GraphBatch *Batch,
    VkImage Image,
    VkImageAspectFlags AspectMask,
    VkPipelineStageFlags StageMask,
    VkAccessFlags AccessMask,
    VkImageLayout Layout)
{
    // assert(Image != VK_NULL_HANDLE);
    //
    // Rr_Renderer *Renderer = &App->Renderer;
    // Rr_Arena *Arena = Rr_GetFrameArena(App);
    //
    // Rr_ImageSync **GlobalState = RR_UPSERT(&Renderer->GlobalSync, Image, App->PermanentArena);
    //
    // VkImageSubresourceRange SubresourceRange = Rr_GetImageSubresourceRange(AspectMask);
    //
    // if(*GlobalState == NULL) /* First time referencing this image. */
    // {
    //     *RR_PUSH_SLICE(&Batch->ImageBarriers, Arena) = (VkImageMemoryBarrier){
    //         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    //         .pNext = NULL,
    //         .srcAccessMask = 0,
    //         .dstAccessMask = AccessMask,
    //         .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    //         .newLayout = Layout,
    //         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //         .image = Image,
    //         .subresourceRange = SubresourceRange,
    //     };
    // }
    // else /* Syncing with previous state. */
    // {
    //     *RR_PUSH_SLICE(&Batch->ImageBarriers, Arena) = (VkImageMemoryBarrier){
    //         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    //         .pNext = NULL,
    //         .srcAccessMask = (*GlobalState)->AccessMask,
    //         .dstAccessMask = AccessMask,
    //         .oldLayout = (*GlobalState)->Layout,
    //         .newLayout = Layout,
    //         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //         .image = Image,
    //         .subresourceRange = SubresourceRange,
    //     };
    // }
    //
    // Rr_ImageSync **BatchState = RR_UPSERT(&Batch->LocalSync, Image, Arena);
    // if(*BatchState == NULL)
    // {
    //     /* @TODO: This stores first usage of a swapchain image.
    //      * @TODO: Feels hacky. */
    //     Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    //     if(Frame->SwapchainImageStage == 0 && Image == Frame->AllocatedSwapchainImage->Handle)
    //     {
    //         Frame->SwapchainImageStage = StageMask;
    //     }
    //
    //     *BatchState = RR_ALLOC(Arena, sizeof(Rr_ImageSync));
    // }
    // (*BatchState)->AccessMask = AccessMask;
    // (*BatchState)->Layout = Layout;
    // (*BatchState)->StageMask = StageMask;
}

bool Rr_BatchBuffer(
    Rr_App *App,
    Rr_GraphBatch *Batch,
    VkBuffer Buffer,
    VkDeviceSize Size,
    VkDeviceSize Offset,
    VkPipelineStageFlags StageMask,
    VkAccessFlags AccessMask)
{
    // assert(Buffer != VK_NULL_HANDLE);
    // assert(Size != 0);
    //
    // Rr_BufferSync **LocalStatePtr = RR_UPSERT(&Batch->LocalSync, Buffer, Batch->Arena);
    // Rr_BufferSync *LocalState = *LocalStatePtr;
    // if(LocalState == NULL)
    // {
    //     *LocalStatePtr = RR_ALLOC(Batch->Arena, sizeof(Rr_BufferSync));
    //     LocalState = *LocalStatePtr;
    // }
    // else
    // {
    //     /* Syncing with previous state. */
    //
    //     bool IsRead = (AccessMask & AllVulkanReads) != 0;
    //     bool IsWrite = (AccessMask & AllVulkanWrites) != 0;
    //     bool LocalIsRead = (LocalState->AccessMask & AllVulkanReads) != 0;
    //     bool LocalIsWrite = (LocalState->AccessMask & AllVulkanWrites) != 0;
    //     if(IsRead && LocalIsRead)
    //     {
    //         LocalState->AccessMask |= AccessMask;
    //         LocalState->StageMask |= StageMask;
    //         LocalState->From = RR_MIN(LocalState->From, Offset);
    //         LocalState->To = RR_MAX(LocalState->To, Offset + Size);
    //         return true;
    //     }
    //     else if(IsWrite && LocalIsWrite)
    //     {
    //         if(LocalState->AccessMask != AccessMask || LocalState->StageMask != AccessMask)
    //         {
    //             return false;
    //         }
    //         if()
    //         {
    //             LocalState->From = RR_MIN(LocalState->From, Offset);
    //         }
    //         LocalState->To = RR_MAX(LocalState->To, Offset + Size);
    //         return true;
    //     }
    //     else
    //     {
    //         return false;
    //     }
    // }

    return true;
}

Rr_GraphBufferHandle Rr_RegisterGraphBuffer(Rr_App *App, Rr_Buffer *Buffer)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame(&App->Renderer);
    Rr_Graph *Graph = &Frame->Graph;
    Rr_GraphBufferHandle Handle = {
        .Values.Index = Graph->ResolvedResources.Count,
    };
    *RR_PUSH_SLICE(&Graph->ResolvedResources, Frame->Arena) = Rr_GetCurrentAllocatedBuffer(App, Buffer);
    return Handle;
}

Rr_GraphImageHandle Rr_RegisterGraphImage(Rr_App *App, Rr_Image *Image)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame(&App->Renderer);
    Rr_Graph *Graph = &Frame->Graph;
    Rr_GraphImageHandle Handle = {
        .Values.Index = Graph->ResolvedResources.Count,
    };
    *RR_PUSH_SLICE(&Graph->ResolvedResources, Frame->Arena) = Rr_GetCurrentAllocatedImage(App, Image);
    return Handle;
}

Rr_GraphNode *Rr_AddPresentNode(Rr_App *App, const char *Name, Rr_GraphImageHandle *ImageHandle, Rr_PresentMode Mode)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    // Rr_Graph *Graph = &Frame->Graph;

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_PRESENT, Name);

    Rr_PresentNode *PresentNode = &GraphNode->Union.Present;
    PresentNode->Mode = Mode;
    PresentNode->ImageHandle = *ImageHandle;

    return GraphNode;
}

bool Rr_BatchPresentNode(Rr_App *App, Rr_GraphBatch *Batch, Rr_PresentNode *Node)
{
    // Rr_Renderer *Renderer = &App->Renderer;
    // Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    //
    // VkImage Handle = Rr_GetCurrentAllocatedImage(App, &Frame->SwapchainImage)->Handle;
    //
    // if(Rr_BatchImagePossible(&Batch->LocalSync, Handle) != true)
    // {
    //     return false;
    // }
    //
    // Rr_BatchImage(
    //     App,
    //     Batch,
    //     Handle,
    //     VK_IMAGE_ASPECT_COLOR_BIT,
    //     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    //     0,
    //     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    return true;
}

void Rr_ExecutePresentNode(Rr_App *App, Rr_PresentNode *Node, VkCommandBuffer CommandBuffer)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkFramebuffer Framebuffer = Renderer->Swapchain.Framebuffers.Data[Frame->SwapchainImageIndex];

    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = Framebuffer,
        .renderArea = (VkRect2D){ { 0, 0 }, { Renderer->Swapchain.Extent.width, Renderer->Swapchain.Extent.height } },
        .renderPass = Renderer->PresentRenderPass,
        .clearValueCount = 0,
        .pClearValues = NULL,
    };
    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Renderer->PresentPipeline->Handle);
    vkCmdDraw(CommandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(CommandBuffer);
}

Rr_GraphNode *Rr_AddTransferNode(Rr_App *App, const char *Name, Rr_GraphBufferHandle *DstBufferHandle)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    Rr_Graph *Graph = &Frame->Graph;

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_TRANSFER, Name);

    Rr_TransferNode *TransferNode = &GraphNode->Union.Transfer;
    TransferNode->DstBufferHandle = *DstBufferHandle;
    RR_RESERVE_SLICE(&TransferNode->Transfers, 2, Frame->Arena);

    /* @TODO: Add staging dependency! */

    Rr_AddNodeBufferDependency(
        Graph,
        GraphNode,
        DstBufferHandle,
        &(Rr_BufferSync){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        },
        Frame->Arena);

    return GraphNode;
}

void Rr_TransferBufferData(Rr_App *App, Rr_GraphNode *Node, Rr_Data Data, size_t DstOffset)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    Rr_TransferNode *TransferNode = &Node->Union.Transfer;

    Rr_AllocatedBuffer *StagingBuffer = Rr_GetCurrentAllocatedBuffer(App, Frame->StagingBuffer.Buffer);

    size_t SrcOffset = Frame->StagingBuffer.Offset;

    if(SrcOffset + Data.Size > StagingBuffer->AllocationInfo.size)
    {
        Rr_LogAbort("Transfer: source buffer overflow!");
    }

    memcpy((char *)StagingBuffer->AllocationInfo.pMappedData + Frame->StagingBuffer.Offset, Data.Pointer, Data.Size);

    Frame->StagingBuffer.Offset += Data.Size;

    *RR_PUSH_SLICE(&TransferNode->Transfers, Frame->Arena) = (Rr_Transfer){
        .DstOffset = DstOffset,
        .SrcOffset = SrcOffset,
        .Size = Data.Size,
    };
}

void Rr_ExecuteTransferNode(Rr_App *App, Rr_Graph *Graph, Rr_TransferNode *Node, VkCommandBuffer CommandBuffer)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    VkBuffer SrcBuffer = Rr_GetCurrentAllocatedBuffer(App, Frame->StagingBuffer.Buffer)->Handle;
    VkBuffer DstBuffer = Rr_GetGraphBuffer(App, Graph, Node->DstBufferHandle)->Handle;

    for(size_t Index = 0; Index < Node->Transfers.Count; ++Index)
    {
        Rr_Transfer *Transfer = Node->Transfers.Data + Index;

        VkBufferCopy Copy = { .size = Transfer->Size,
                              .srcOffset = Transfer->SrcOffset,
                              .dstOffset = Transfer->DstOffset };
        vkCmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, 1, &Copy);
    }
}

Rr_GraphNode *Rr_AddBlitNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphImageHandle *SrcImageHandle,
    Rr_GraphImageHandle *DstImageHandle,
    Rr_IntVec4 SrcRect,
    Rr_IntVec4 DstRect,
    Rr_BlitMode Mode)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    Rr_Graph *Graph = &Frame->Graph;

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_BLIT, Name);

    Rr_BlitNode *BlitNode = &GraphNode->Union.Blit;
    *BlitNode = (Rr_BlitNode){
        .SrcImageHandle = *SrcImageHandle,
        .DstImageHandle = *DstImageHandle,
        .SrcRect = SrcRect,
        .DstRect = DstRect,
    };

    switch(Mode)
    {
        case RR_BLIT_MODE_COLOR:
        {
            BlitNode->AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        break;
        case RR_BLIT_MODE_DEPTH:
        {
            BlitNode->AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        break;
        default:
        {
            Rr_LogAbort("Unsupported blit mode!");
        }
        break;
    }

    Rr_AddNodeImageDependency(
        Graph,
        GraphNode,
        SrcImageHandle,
        &(Rr_ImageSync){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        },
        Frame->Arena);

    Rr_AddNodeImageDependency(
        Graph,
        GraphNode,
        DstImageHandle,
        &(Rr_ImageSync){
            .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        },
        Frame->Arena);

    return GraphNode;
}

static inline bool Rr_ClampBlitRect(Rr_IntVec4 *Rect, VkExtent3D *Extent)
{
    Rect->X = RR_CLAMP(0, Rect->X, (int)Extent->width);
    Rect->Y = RR_CLAMP(0, Rect->Y, (int)Extent->height);
    Rect->Width = RR_CLAMP(0, Rect->Width, (int)Extent->width - Rect->X);
    Rect->Height = RR_CLAMP(0, Rect->Height, (int)Extent->height - Rect->Y);

    return Rect->Width > 0 && Rect->Height > 0;
}

void Rr_ExecuteBlitNode(Rr_App *App, Rr_Graph *Graph, Rr_BlitNode *Node, VkCommandBuffer CommandBuffer)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame(&App->Renderer);

    Rr_AllocatedImage *SrcImage = Rr_GetGraphImage(App, &Frame->Graph, Node->SrcImageHandle);
    Rr_AllocatedImage *DstImage = Rr_GetGraphImage(App, &Frame->Graph, Node->DstImageHandle);

    if(Rr_ClampBlitRect(&Node->SrcRect, &SrcImage->Container->Extent) &&
       Rr_ClampBlitRect(&Node->DstRect, &DstImage->Container->Extent))
    {
        Rr_BlitColorImage(
            CommandBuffer,
            SrcImage->Handle,
            DstImage->Handle,
            Node->SrcRect,
            Node->DstRect,
            Node->AspectMask);
    }
}

static VkIndexType Rr_GetVulkanIndexType(Rr_IndexType Type)
{
    switch(Type)
    {
        case RR_INDEX_TYPE_UINT8:
            return VK_INDEX_TYPE_UINT8;
        case RR_INDEX_TYPE_UINT16:
            return VK_INDEX_TYPE_UINT16;
        default:
            return VK_INDEX_TYPE_UINT32;
    }
}

Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_App *App,
    const char *Name,
    Rr_ColorTarget *ColorTargets,
    size_t ColorTargetCount,
    Rr_DepthTarget *DepthTarget)
{
    assert(ColorTargetCount > 0 || DepthTarget != NULL);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    Rr_Graph *Graph = &Frame->Graph;

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_GRAPHICS, Name);

    Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.Graphics;
    if(ColorTargetCount > 0)
    {
        GraphicsNode->ColorTargets = RR_ALLOC_STRUCT_COUNT(Frame->Arena, Rr_ColorTarget, ColorTargetCount);
        memcpy(GraphicsNode->ColorTargets, ColorTargets, sizeof(Rr_ColorTarget) * ColorTargetCount);
        GraphicsNode->ColorTargetCount = ColorTargetCount;

        for(size_t Index = 0; Index < ColorTargetCount; ++Index)
        {
            /* @TODO: Infer access type from attachment struct! */

            Rr_AddNodeImageDependency(
                Graph,
                GraphNode,
                GraphicsNode->ColorTargets[Index].ImageHandle,
                &(Rr_ImageSync){
                    .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                },
                Frame->Arena);
        }
    }
    if(DepthTarget != NULL)
    {
        GraphicsNode->DepthTarget = RR_ALLOC_STRUCT(Frame->Arena, Rr_DepthTarget);
        memcpy(GraphicsNode->DepthTarget, DepthTarget, sizeof(Rr_DepthTarget));

        /* @TODO: Infer access type from attachment struct! */

        Rr_AddNodeImageDependency(
            Graph,
            GraphNode,
            GraphicsNode->DepthTarget->ImageHandle,
            &(Rr_ImageSync){
                .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .AccessMask =
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
            Frame->Arena);
    }
    GraphicsNode->Encoded = RR_ALLOC(Frame->Arena, sizeof(Rr_GraphicsNodeFunction));
    GraphicsNode->EncodedFirst = GraphicsNode->Encoded;

    return GraphNode;
}

void Rr_ExecuteGraphicsNode(Rr_App *App, Rr_Graph *Graph, Rr_GraphicsNode *Node, VkCommandBuffer CommandBuffer)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_IntVec4 Viewport = { 0 };
    Viewport.Width = INT32_MAX;
    Viewport.Height = INT32_MAX;

    /* Line up appropriate clear values. */

    uint32_t AttachmentCount = Node->ColorTargetCount + (Node->DepthTarget ? 1 : 0);

    VkImageView *ImageViews = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, VkImageView, AttachmentCount);
    Rr_Attachment *Attachments = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, Rr_Attachment, AttachmentCount);
    VkClearValue *ClearValues = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, VkClearValue, AttachmentCount);
    for(uint32_t Index = 0; Index < Node->ColorTargetCount; ++Index)
    {
        Rr_ColorTarget *ColorTarget = &Node->ColorTargets[Index];
        VkClearValue *ClearValue = &ClearValues[ColorTarget->Slot];
        memcpy(ClearValue, &ColorTarget->Clear, sizeof(VkClearValue));
        Attachments[ColorTarget->Slot] = (Rr_Attachment){
            .LoadOp = ColorTarget->LoadOp,
            .StoreOp = ColorTarget->StoreOp,
        };
        Rr_AllocatedImage *ColorImage = Rr_GetGraphImage(App, Graph, *ColorTarget->ImageHandle);
        ImageViews[ColorTarget->Slot] = ColorImage->View;

        Viewport.Width = RR_MIN(Viewport.Width, (int32_t)ColorImage->Container->Extent.width);
        Viewport.Height = RR_MIN(Viewport.Width, (int32_t)ColorImage->Container->Extent.height);
    }
    if(Node->DepthTarget != NULL)
    {
        Rr_DepthTarget *DepthTarget = Node->DepthTarget;
        VkClearValue *ClearValue = &ClearValues[DepthTarget->Slot];
        memcpy(ClearValue, &DepthTarget->Clear, sizeof(VkClearValue));
        Attachments[DepthTarget->Slot] = (Rr_Attachment){
            .LoadOp = DepthTarget->LoadOp,
            .StoreOp = DepthTarget->StoreOp,
            .Depth = true,
        };
        Rr_AllocatedImage *DepthImage = Rr_GetGraphImage(App, Graph, *DepthTarget->ImageHandle);
        ImageViews[DepthTarget->Slot] = DepthImage->View;

        Viewport.Width = RR_MIN(Viewport.Width, (int32_t)DepthImage->Container->Extent.width);
        Viewport.Height = RR_MIN(Viewport.Width, (int32_t)DepthImage->Container->Extent.height);
    }

    /* Begin render pass. */

    Rr_RenderPassInfo RenderPassInfo = { .AttachmentCount = AttachmentCount, .Attachments = Attachments };
    VkRenderPass RenderPass = Rr_GetRenderPass(App, &RenderPassInfo);
    VkFramebuffer Framebuffer = Rr_GetFramebufferViews(
        App,
        RenderPass,
        ImageViews,
        AttachmentCount,
        (VkExtent3D){ .width = Viewport.Width, .height = Viewport.Height, .depth = 1 });
    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = Framebuffer,
        .renderArea = (VkRect2D){ { Viewport.X, Viewport.Y }, { Viewport.Z, Viewport.W } },
        .renderPass = RenderPass,
        .clearValueCount = AttachmentCount,
        .pClearValues = ClearValues,
    };
    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    /* Set dynamic states. */

    vkCmdSetViewport(
        CommandBuffer,
        0,
        1,
        &(VkViewport){
            .x = (float)Viewport.X,
            .y = (float)Viewport.Y,
            .width = (float)Viewport.Width,
            .height = (float)Viewport.Height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        });

    vkCmdSetScissor(
        CommandBuffer,
        0,
        1,
        &(VkRect2D){
            .offset.x = Viewport.X,
            .offset.y = Viewport.Y,
            .extent.width = Viewport.Width,
            .extent.height = Viewport.Height,
        });

    Rr_GraphicsPipeline *GraphicsPipeline = NULL;
    Rr_DescriptorsState DescriptorsState = { 0 };

    for(Rr_GraphicsNodeFunction *Function = Node->EncodedFirst; Function != NULL; Function = Function->Next)
    {
        switch(Function->Type)
        {
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER:
            {
                Rr_BindIndexBufferArgs *Args = Function->Args;
                vkCmdBindIndexBuffer(
                    CommandBuffer,
                    Rr_GetGraphBuffer(App, Graph, Args->BufferHandle)->Handle,
                    Args->Offset,
                    Args->Type);
            }
            break;
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER:
            {
                Rr_BindBufferArgs *Args = Function->Args;
                vkCmdBindVertexBuffers(
                    CommandBuffer,
                    Args->Slot,
                    1,
                    &Rr_GetGraphBuffer(App, Graph, Args->BufferHandle)->Handle,
                    &(VkDeviceSize){ Args->Offset });
            }
            break;
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_DRAW_INDEXED:
            {
                Rr_ApplyDescriptorsState(
                    &DescriptorsState,
                    &Frame->DescriptorAllocator,
                    GraphicsPipeline->Layout,
                    Renderer->Device,
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS);
                Rr_DrawIndexedArgs *Args = (Rr_DrawIndexedArgs *)Function->Args;
                vkCmdDrawIndexed(
                    CommandBuffer,
                    Args->IndexCount,
                    Args->InstanceCount,
                    Args->FirstIndex,
                    Args->VertexOffset,
                    Args->FirstInstance);
            }
            break;
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE:
            {
                GraphicsPipeline = *(Rr_GraphicsPipeline **)Function->Args;
                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline->Handle);
            }
            break;
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_SET_VIEWPORT:
            {
                Rr_Vec4 *Viewport = Function->Args;
                vkCmdSetViewport(
                    CommandBuffer,
                    0,
                    1,
                    &(VkViewport){
                        .x = Viewport->X,
                        .y = Viewport->Y,
                        .width = Viewport->Width,
                        .height = Viewport->Height,
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f,
                    });
            }
            break;
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_SET_SCISSOR:
            {
                Rr_IntVec4 *Scissor = Function->Args;
                vkCmdSetScissor(
                    CommandBuffer,
                    0,
                    1,
                    &(VkRect2D){
                        .offset.x = Scissor->X,
                        .offset.y = Scissor->Y,
                        .extent.width = Scissor->Width,
                        .extent.height = Scissor->Height,
                    });
            }
            break;
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER:
            {
                Rr_BindUniformBufferArgs *Args = Function->Args;
                Rr_UpdateDescriptorsState(
                    &DescriptorsState,
                    Args->Set,
                    Args->Binding,
                    &(Rr_DescriptorSetBinding){
                        .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
                        .Buffer =
                            (Rr_DescriptorSetBufferBinding){
                                .Handle = Rr_GetGraphBuffer(App, Graph, Args->BufferHandle)->Handle,
                                .Size = Args->Size,
                                .Offset = Args->Offset,
                            },
                        .DescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                    });
            }
            break;
            default:
            {
            }
            break;
        }
    }

    vkCmdEndRenderPass(CommandBuffer);

    Rr_DestroyScratch(Scratch);
}

#define RR_GRAPHICS_NODE_ENCODE(FunctionType, ArgsType)                             \
    Rr_Arena *Arena = Node->Arena;                                                  \
    Rr_GraphicsNode *GraphicsNode = (Rr_GraphicsNode *)&Node->Union.Graphics;       \
    GraphicsNode->Encoded->Next = RR_ALLOC(Arena, sizeof(Rr_GraphicsNodeFunction)); \
    GraphicsNode->Encoded = GraphicsNode->Encoded->Next;                            \
    GraphicsNode->Encoded->Type = FunctionType;                                     \
    GraphicsNode->Encoded->Args = RR_ALLOC(Arena, sizeof(ArgsType));                \
    *(ArgsType *)GraphicsNode->Encoded->Args

void Rr_DrawIndexed(
    Rr_GraphNode *Node,
    uint32_t IndexCount,
    uint32_t InstanceCount,
    uint32_t FirstIndex,
    int32_t VertexOffset,
    uint32_t FirstInstance)
{
    RR_GRAPHICS_NODE_ENCODE(RR_GRAPHICS_NODE_FUNCTION_TYPE_DRAW_INDEXED, Rr_DrawIndexedArgs) = (Rr_DrawIndexedArgs){
        .IndexCount = IndexCount,
        .InstanceCount = InstanceCount,
        .FirstIndex = FirstIndex,
        .VertexOffset = VertexOffset,
        .FirstInstance = FirstInstance,
    };
}

void Rr_BindVertexBuffer(Rr_GraphNode *Node, Rr_GraphBufferHandle *BufferHandle, uint32_t Slot, uint32_t Offset)
{
    RR_GRAPHICS_NODE_ENCODE(RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER, Rr_BindIndexBufferArgs) =
        (Rr_BindIndexBufferArgs){
            .BufferHandle = *BufferHandle,
            .Slot = Slot,
            .Offset = Offset,
        };
}

void Rr_BindIndexBuffer(
    Rr_GraphNode *Node,
    Rr_GraphBufferHandle *BufferHandle,
    uint32_t Slot,
    uint32_t Offset,
    Rr_IndexType Type)
{
    RR_GRAPHICS_NODE_ENCODE(RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER, Rr_BindIndexBufferArgs) =
        (Rr_BindIndexBufferArgs){
            .BufferHandle = *BufferHandle,
            .Slot = Slot,
            .Offset = Offset,
            .Type = Rr_GetVulkanIndexType(Type),
        };
}

void Rr_BindGraphicsPipeline(Rr_GraphNode *Node, Rr_GraphicsPipeline *GraphicsPipeline)
{
    RR_GRAPHICS_NODE_ENCODE(RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_GRAPHICS_PIPELINE, Rr_GraphicsPipeline *) =
        GraphicsPipeline;
}

void Rr_SetViewport(Rr_GraphNode *Node, Rr_Vec4 Rect)
{
    RR_GRAPHICS_NODE_ENCODE(RR_GRAPHICS_NODE_FUNCTION_TYPE_SET_VIEWPORT, Rr_Vec4) = Rect;
}

void Rr_SetScissor(Rr_GraphNode *Node, Rr_IntVec4 Rect)
{
    RR_GRAPHICS_NODE_ENCODE(RR_GRAPHICS_NODE_FUNCTION_TYPE_SET_SCISSOR, Rr_IntVec4) = Rect;
}

void Rr_BindGraphicsUniformBuffer(
    Rr_GraphNode *Node,
    Rr_GraphBufferHandle *BufferHandle,
    uint32_t Set,
    uint32_t Binding,
    uint32_t Offset,
    uint32_t Size)
{
    assert(Set < RR_MAX_SETS);
    assert(Binding < RR_MAX_BINDINGS);
    assert(Size > 0);

    RR_GRAPHICS_NODE_ENCODE(RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_UNIFORM_BUFFER, Rr_BindUniformBufferArgs) =
        (Rr_BindUniformBufferArgs){
            .BufferHandle = *BufferHandle,
            .Set = Set,
            .Offset = Offset,
            .Size = Size,
            .Binding = Binding,
        };
}
