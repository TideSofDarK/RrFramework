#include "Rr_Graph.h"

#include "Rr_App.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_Memory.h"
#include "Rr_Renderer.h"

#include <SDL3/SDL_assert.h>

static void Rr_CopyDependencies(
    Rr_GraphNode *GraphNode,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount,
    Rr_Arena *Arena)
{
    RR_SLICE_RESERVE(&GraphNode->Dependencies, DependencyCount, Arena);
    memcpy(GraphNode->Dependencies.Data, Dependencies, sizeof(Rr_GraphNode *) * DependencyCount);
    GraphNode->Dependencies.Count = DependencyCount;
}

Rr_GraphNode *Rr_AddGraphNode(
    Rr_Frame *Frame,
    Rr_GraphNodeType Type,
    const char *Name,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    Rr_GraphNode *GraphNode = RR_ALLOC(Frame->Arena, sizeof(Rr_GraphNode));
    GraphNode->Arena = Frame->Arena;
    GraphNode->Type = Type;
    GraphNode->Name = Name;
    Rr_CopyDependencies(GraphNode, Dependencies, DependencyCount, Frame->Arena);

    *RR_SLICE_PUSH(&Frame->Graph.Nodes, Frame->Arena) = GraphNode;

    return GraphNode;
}

static void Rr_ExecuteGraphBatch(Rr_App *App, Rr_Graph *Graph, Rr_GraphBatch *Batch, Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    /* Submit barriers. */

    size_t ImageBarrierCount = Batch->ImageBarriers.Count;
    size_t BufferBarrierCount = Batch->BufferBarriers.Count;
    if(ImageBarrierCount > 0 || BufferBarrierCount > 0)
    {
        VkPipelineStageFlags StageMask = 0;
        for(size_t Index = 0; Index < ImageBarrierCount; ++Index)
        {
            VkImageMemoryBarrier *Barrier = Batch->ImageBarriers.Data + Index;
            Rr_ImageSync **State =
                (Rr_ImageSync **)Rr_MapUpsert(&Renderer->GlobalSync, (uintptr_t)Barrier->image, NULL);
            if(*State != NULL)
            {
                StageMask |= (*State)->StageMask;
            }
        }

        if(StageMask == 0)
        {
            StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(
            CommandBuffer,
            StageMask,
            Batch->StageMask,
            0,
            0,
            NULL,
            Batch->BufferBarriers.Count,
            Batch->BufferBarriers.Data,
            Batch->ImageBarriers.Count,
            Batch->ImageBarriers.Data);
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
            default:
            {
                Rr_LogAbort("Unsupported node type!");
            }
            break;
        }

        GraphNode->Executed = true;
    }

    /* Apply batch state. */
    /* @TODO: Account for node specific transitions. */

    for(size_t Index = 0; Index < ImageBarrierCount; ++Index)
    {
        VkImageMemoryBarrier *Barrier = Batch->ImageBarriers.Data + Index;
        Rr_ImageSync **GlobalState =
            (Rr_ImageSync **)Rr_MapUpsert(&Renderer->GlobalSync, (uintptr_t)Barrier->image, App->PermanentArena);
        if(*GlobalState == NULL)
        {
            *GlobalState = RR_ALLOC(App->PermanentArena, sizeof(Rr_ImageSync));
        }
        Rr_ImageSync **BatchState = (Rr_ImageSync **)Rr_MapUpsert(&Batch->LocalSync, (uintptr_t)Barrier->image, NULL);
        *(*GlobalState) = *(*BatchState);
    }
    /* @TODO: Same for buffers. */

    Rr_DestroyArenaScratch(Scratch);
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

        Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

        Rr_GraphBatch Batch = {
            .Arena = Scratch.Arena,
        };

        for(size_t Index = 0; Index < Graph->Nodes.Count; ++Index)
        {
            Rr_GraphNode *GraphNode = Graph->Nodes.Data[Index];

            if(GraphNode->Executed == true)
            {
                continue;
            }

            /* Dependency check. */

            bool DependenciesResolved = true;
            for(size_t DepIndex = 0; DepIndex < GraphNode->Dependencies.Count; ++DepIndex)
            {
                Rr_GraphNode *Dependency = GraphNode->Dependencies.Data[DepIndex];
                if(Dependency->Executed != true)
                {
                    DependenciesResolved = false;
                    break;
                }
            }
            if(DependenciesResolved != true)
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
                default:
                {
                    Rr_LogAbort("Unsupported node type!");
                }
                break;
            }
            if(NodeBatched)
            {
                *RR_SLICE_PUSH(&Batch.Nodes, Batch.Arena) = GraphNode;
                NodeCount--;
            }
        }

        if(Batch.Nodes.Count == 0)
        {
            Rr_LogAbort("Couldn't batch graph nodes, probably invalid graph!");
        }

        Rr_ExecuteGraphBatch(App, Graph, &Batch, Scratch.Arena);

        Rr_DestroyArenaScratch(Scratch);

        if(NodeCount <= 0)
        {
            break;
        }
    }
}

bool Rr_BatchImagePossible(Rr_Map **Sync, VkImage Image)
{
    Rr_ImageSync **State = (Rr_ImageSync **)Rr_MapUpsert(Sync, (uintptr_t)Image, NULL);

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
    if(Image == NULL)
    {
        return;
    }

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Arena *Arena = Rr_GetFrameArena(App);

    Rr_ImageSync **GlobalState =
        (Rr_ImageSync **)Rr_MapUpsert(&Renderer->GlobalSync, (uintptr_t)Image, App->PermanentArena);

    VkImageSubresourceRange SubresourceRange = Rr_GetImageSubresourceRange(AspectMask);

    if(*GlobalState == NULL) /* First time referencing this image. */
    {
        *RR_SLICE_PUSH(&Batch->ImageBarriers, Arena) = (VkImageMemoryBarrier){
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
        *RR_SLICE_PUSH(&Batch->ImageBarriers, Arena) = (VkImageMemoryBarrier){
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

    Rr_ImageSync **BatchState = (Rr_ImageSync **)Rr_MapUpsert(&Batch->LocalSync, (uintptr_t)Image, Arena);
    if(*BatchState == NULL)
    {
        Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
        if(Frame->SwapchainImageStage == 0 && Image == Frame->SwapchainImage->Handle)
        {
            Frame->SwapchainImageStage = StageMask;
        }

        *BatchState = RR_ALLOC(Arena, sizeof(Rr_ImageSync));
    }
    (*BatchState)->AccessMask = AccessMask;
    (*BatchState)->Layout = Layout;
    (*BatchState)->StageMask = StageMask;

    Batch->StageMask |= StageMask;
}
