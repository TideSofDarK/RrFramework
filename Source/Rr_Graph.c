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

Rr_GraphNode *Rr_AddGraphNode(Rr_Frame *Frame, Rr_GraphNodeType Type, const char *Name)
{
    Rr_GraphNode *GraphNode = RR_ALLOC(Frame->Arena, sizeof(Rr_GraphNode));
    GraphNode->Arena = Frame->Arena;
    GraphNode->Type = Type;
    GraphNode->Name = Name;

    *RR_PUSH_SLICE(&Frame->Graph.Nodes, Frame->Arena) = GraphNode;

    return GraphNode;
}

static void Rr_ExecuteGraphBatch(Rr_App *App, Rr_Graph *Graph, Rr_GraphBatch *Batch, Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkCommandBuffer CommandBuffer = Frame->CommandBuffer.Handle;

    /* Submit barriers. */
    /* @TODO: People suggesting batching these into two submits: pre vertex stage and post vertex stage. */

    size_t ImageBarrierCount = Batch->ImageBarriers.Count;
    for(size_t Index = 0; Index < ImageBarrierCount; ++Index)
    {
        VkImageMemoryBarrier *Barrier = Batch->ImageBarriers.Data + Index;
        Rr_ImageSync **GlobalState = RR_UPSERT(&Renderer->GlobalSync, Barrier->image, NULL);
        Rr_ImageSync **LocalState = RR_UPSERT(&Batch->LocalSync, Barrier->image, NULL);
        VkPipelineStageFlags StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if(*GlobalState != NULL)
        {
            StageMask = (*GlobalState)->StageMask;
        }
        vkCmdPipelineBarrier(CommandBuffer, StageMask, (*LocalState)->StageMask, 0, 0, NULL, 0, NULL, 1, Barrier);
    }

    size_t BufferCount = Batch->Buffers.Count;
    for(size_t Index = 0; Index < BufferCount; ++Index)
    {
        VkBuffer Buffer = Batch->Buffers.Data[Index];

        Rr_BufferSync **GlobalStatePtr = RR_UPSERT(&Renderer->GlobalSync, Buffer, NULL);
        Rr_BufferSync *GlobalState = *GlobalStatePtr;
        bool HasGlobalState = GlobalState != NULL;

        Rr_BufferSync **LocalStatePtr = RR_UPSERT(&Batch->LocalSync, Buffer, NULL);
        Rr_BufferSync *LocalState = *LocalStatePtr;

        VkBufferMemoryBarrier *Barriers =
            RR_ALLOC_STRUCT_COUNT(Scratch.Arena, VkBufferMemoryBarrier, LocalState->RangeCount);

        size_t RangeIndex = 0;
        for(Rr_BufferRange *Range = LocalState->SentinelRange.Next; Range != NULL; Range = Range->Next, ++RangeIndex)
        {
            Barriers[RangeIndex] = (VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = HasGlobalState ? GlobalState->AccessMask : 0,
                .dstAccessMask = LocalState->AccessMask,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = Buffer,
                .offset = Range->Offset,
                .size = Range->Size,
            };
        }
        vkCmdPipelineBarrier(
            CommandBuffer,
            HasGlobalState ? GlobalState->StageMask : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            LocalState->StageMask,
            0,
            0,
            NULL,
            RangeIndex,
            Barriers,
            0,
            NULL);
        if(GlobalState == NULL)
        {
            *GlobalStatePtr = RR_ALLOC(App->PermanentArena, sizeof(Rr_BufferSync));
            GlobalState = *GlobalStatePtr;
        }
        *GlobalState = *LocalState;
    }

    for(size_t Index = 0; Index < Batch->Nodes.Count; ++Index)
    {
        Rr_GraphNode *GraphNode = Batch->Nodes.Data[Index];

        switch(GraphNode->Type)
        {
            case RR_GRAPH_NODE_TYPE_GRAPHICS:
            {
                Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.GraphicsNode;
                Rr_ExecuteGraphicsNode(App, GraphicsNode, Scratch.Arena);
            }
            break;
            case RR_GRAPH_NODE_TYPE_PRESENT:
            {
                Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
                Rr_ExecutePresentNode(App, PresentNode);
            }
            break;
            case RR_GRAPH_NODE_TYPE_BUILTIN:
            {
                Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.BuiltinNode;
                Rr_ExecuteBuiltinNode(App, BuiltinNode, Scratch.Arena);
            }
            break;
            case RR_GRAPH_NODE_TYPE_BLIT:
            {
                Rr_BlitNode *BlitNode = &GraphNode->Union.BlitNode;
                Rr_ExecuteBlitNode(App, BlitNode);
            }
            break;
            case RR_GRAPH_NODE_TYPE_TRANSFER:
            {
                Rr_TransferNode *TransferNode = &GraphNode->Union.TransferNode;
                Rr_ExecuteTransferNode(App, TransferNode, Scratch.Arena);
            }
            break;
            default:
            {
                Rr_LogAbort("Unsupported node type!");
            }
            break;
        }

        GraphNode->Executed = true;
    }

    for(size_t Index = 0; Index < ImageBarrierCount; ++Index)
    {
        VkImageMemoryBarrier *Barrier = Batch->ImageBarriers.Data + Index;
        Rr_ImageSync **GlobalState = RR_UPSERT(&Renderer->GlobalSync, Barrier->image, App->PermanentArena);
        if(*GlobalState == NULL)
        {
            *GlobalState = RR_ALLOC(App->PermanentArena, sizeof(Rr_ImageSync));
        }
        Rr_ImageSync **BatchState = RR_UPSERT(&Batch->LocalSync, Barrier->image, NULL);
        *(*GlobalState) = *(*BatchState);
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

    while(true)
    {
        /* Begin a new batch. */

        Rr_Scratch Scratch = Rr_GetScratch(Arena);

        Rr_GraphBatch Batch = {
            .Arena = Scratch.Arena,
        };

        /* First Graph Pass */
        /* Here we go through the nodes and accumulate barriers
         * up until  */

        for(size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
        {
            Rr_GraphNode *GraphNode = Graph->Nodes.Data[Index];

            bool NodeBatched = false;
            switch(GraphNode->Type)
            {
                case RR_GRAPH_NODE_TYPE_GRAPHICS:
                {
                    Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.GraphicsNode;
                    NodeBatched = Rr_BatchGraphicsNode(App, &Batch, GraphicsNode);
                }
                break;
                case RR_GRAPH_NODE_TYPE_PRESENT:
                {
                    Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
                    NodeBatched = Rr_BatchPresentNode(App, &Batch, PresentNode);
                }
                break;
                case RR_GRAPH_NODE_TYPE_BUILTIN:
                {
                    Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.BuiltinNode;
                    NodeBatched = Rr_BatchBuiltinNode(App, Graph, &Batch, BuiltinNode);
                }
                break;
                case RR_GRAPH_NODE_TYPE_BLIT:
                {
                    Rr_BlitNode *BlitNode = &GraphNode->Union.BlitNode;
                    NodeBatched = Rr_BatchBlitNode(App, Graph, &Batch, BlitNode);
                }
                break;
                case RR_GRAPH_NODE_TYPE_TRANSFER:
                {
                    Rr_TransferNode *TransferNode = &GraphNode->Union.TransferNode;
                    NodeBatched = Rr_BatchTransferNode(App, &Batch, TransferNode);
                }
                break;
                default:
                {
                    Rr_LogAbort("Unsupported node type!");
                }
                break;
            }
        }

        for(size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
        {
            Rr_GraphNode *GraphNode = Graph->Nodes.Data[Index];

            if(GraphNode->Executed == true)
            {
                continue;
            }

            /* Attempt batching current node. */

            bool NodeBatched = false;
            switch(GraphNode->Type)
            {
                case RR_GRAPH_NODE_TYPE_GRAPHICS:
                {
                    Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.GraphicsNode;
                    NodeBatched = Rr_BatchGraphicsNode(App, &Batch, GraphicsNode);
                }
                break;
                case RR_GRAPH_NODE_TYPE_PRESENT:
                {
                    Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
                    NodeBatched = Rr_BatchPresentNode(App, &Batch, PresentNode);
                }
                break;
                case RR_GRAPH_NODE_TYPE_BUILTIN:
                {
                    Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.BuiltinNode;
                    NodeBatched = Rr_BatchBuiltinNode(App, Graph, &Batch, BuiltinNode);
                }
                break;
                case RR_GRAPH_NODE_TYPE_BLIT:
                {
                    Rr_BlitNode *BlitNode = &GraphNode->Union.BlitNode;
                    NodeBatched = Rr_BatchBlitNode(App, Graph, &Batch, BlitNode);
                }
                break;
                case RR_GRAPH_NODE_TYPE_TRANSFER:
                {
                    Rr_TransferNode *TransferNode = &GraphNode->Union.TransferNode;
                    NodeBatched = Rr_BatchTransferNode(App, &Batch, TransferNode);
                }
                break;
                default:
                {
                    Rr_LogAbort("Unsupported node type!");
                }
                break;
            }
            if(NodeBatched)
            {
                *RR_PUSH_SLICE(&Batch.Nodes, Batch.Arena) = GraphNode;
                NodeCount--;
            }
        }

        if(Batch.Nodes.Count == 0)
        {
            Rr_LogAbort("Couldn't batch graph nodes, probably invalid graph!");
        }

        Rr_ExecuteGraphBatch(App, Graph, &Batch, Scratch.Arena);

        Rr_DestroyScratch(Scratch);

        if(NodeCount <= 0)
        {
            break;
        }
    }
}

bool Rr_BatchImagePossible(Rr_Map **Sync, VkImage Image)
{
    Rr_ImageSync **State = RR_UPSERT(Sync, Image, NULL);

    if(State != NULL)
    {
        /* @TODO: Should be true if for example current batch state matches
         * requested state. */
        return false;
    }

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
    assert(Image != VK_NULL_HANDLE);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Arena *Arena = Rr_GetFrameArena(App);

    Rr_ImageSync **GlobalState = RR_UPSERT(&Renderer->GlobalSync, Image, App->PermanentArena);

    VkImageSubresourceRange SubresourceRange = Rr_GetImageSubresourceRange(AspectMask);

    if(*GlobalState == NULL) /* First time referencing this image. */
    {
        *RR_PUSH_SLICE(&Batch->ImageBarriers, Arena) = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = 0,
            .dstAccessMask = AccessMask,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = Layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = Image,
            .subresourceRange = SubresourceRange,
        };
    }
    else /* Syncing with previous state. */
    {
        *RR_PUSH_SLICE(&Batch->ImageBarriers, Arena) = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = (*GlobalState)->AccessMask,
            .dstAccessMask = AccessMask,
            .oldLayout = (*GlobalState)->Layout,
            .newLayout = Layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = Image,
            .subresourceRange = SubresourceRange,
        };
    }

    Rr_ImageSync **BatchState = RR_UPSERT(&Batch->LocalSync, Image, Arena);
    if(*BatchState == NULL)
    {
        /* @TODO: This stores first usage of a swapchain image.
         * @TODO: Feels hacky. */
        Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
        if(Frame->SwapchainImageStage == 0 && Image == Frame->AllocatedSwapchainImage->Handle)
        {
            Frame->SwapchainImageStage = StageMask;
        }

        *BatchState = RR_ALLOC(Arena, sizeof(Rr_ImageSync));
    }
    (*BatchState)->AccessMask = AccessMask;
    (*BatchState)->Layout = Layout;
    (*BatchState)->StageMask = StageMask;
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
    assert(Buffer != VK_NULL_HANDLE);
    assert(Size != 0);

    Rr_BufferSync **LocalStatePtr = RR_UPSERT(&Batch->LocalSync, Buffer, Batch->Arena);
    Rr_BufferSync *LocalState = *LocalStatePtr;
    if(LocalState == NULL)
    {
        *LocalStatePtr = RR_ALLOC(Batch->Arena, sizeof(Rr_BufferSync));
        LocalState = *LocalStatePtr;
    }
    else
    {
        /* Syncing with previous state. */

        bool IsRead = (AccessMask & AllVulkanReads) != 0;
        bool IsWrite = (AccessMask & AllVulkanWrites) != 0;
        bool LocalIsRead = (LocalState->AccessMask & AllVulkanReads) != 0;
        bool LocalIsWrite = (LocalState->AccessMask & AllVulkanWrites) != 0;
        if(IsRead && LocalIsRead)
        {
            LocalState->AccessMask |= AccessMask;
            LocalState->StageMask |= StageMask;
            LocalState->From = RR_MIN(LocalState->From, Offset);
            LocalState->To = RR_MAX(LocalState->To, Offset + Size);
            return true;
        }
        else if(IsWrite && LocalIsWrite)
        {
            if(LocalState->AccessMask != AccessMask || LocalState->StageMask != AccessMask)
            {
                return false;
            }
            if()
            LocalState->From = RR_MIN(LocalState->From, Offset);
            LocalState->To = RR_MAX(LocalState->To, Offset + Size);
            return true;
        }
        else
        {
            return false;
        }
    }

    return true;
}
