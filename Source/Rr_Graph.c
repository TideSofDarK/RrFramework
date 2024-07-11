#include "Rr_Graph.h"

#include "Rr/Rr_Defines.h"
#include "Rr/Rr_Graph.h"
#include "Rr_App.h"
#include "Rr_Image.h"
#include "Rr_Log.h"
#include "Rr_Material.h"
#include "Rr_Memory.h"
#include "Rr_Mesh.h"
#include "Rr_Renderer.h"

#include <SDL3/SDL_assert.h>

static void Rr_CopyDependencies(
    Rr_GraphNode *GraphNode,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount,
    Rr_Arena *Arena)
{
    RR_SLICE_RESERVE(&GraphNode->Dependencies, DependencyCount, Arena);
    memcpy(
        GraphNode->Dependencies.Data,
        Dependencies,
        sizeof(Rr_GraphNode *) * DependencyCount);
    GraphNode->Dependencies.Count = DependencyCount;
    // for (size_t Index = 0; Index < DependencyCount; ++Index)
    // {
    //     *RR_SLICE_PUSH(&GraphNode->Dependencies, Arena) =
    //     Dependencies[Index];
    // }
}

Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphicsNodeInfo *Info,
    char *GlobalsData,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_Graph *Graph = &Frame->Graph;

    Rr_GraphNode *GraphNode =
        RR_ARENA_ALLOC_ONE(&Frame->Arena, sizeof(Rr_GraphNode));
    *RR_SLICE_PUSH(&Graph->NodesSlice, &Frame->Arena) = GraphNode;

    Rr_CopyDependencies(
        GraphNode,
        Dependencies,
        DependencyCount,
        &Frame->Arena);
    GraphNode->Type = RR_GRAPH_NODE_TYPE_GRAPHICS;
    GraphNode->Name = Name;

    Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.GraphicsNode;
    *GraphicsNode = (Rr_GraphicsNode){
        .Info = *Info,
    };

    if (GraphicsNode->Info.DrawTarget == NULL)
    {
        GraphicsNode->Info.DrawTarget = Renderer->DrawTarget;
        GraphicsNode->Info.Viewport.Width =
            (int32_t)Renderer->SwapchainSize.width;
        GraphicsNode->Info.Viewport.Height =
            (int32_t)Renderer->SwapchainSize.height;
    }

    memcpy(
        GraphicsNode->GlobalsData,
        GlobalsData,
        Info->BasePipeline->Sizes.Globals);

    return GraphNode;
}

Rr_GraphNode *Rr_AddPresentNode(
    Rr_App *App,
    const char *Name,
    Rr_PresentNodeInfo *Info,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_Graph *Graph = &Frame->Graph;

    Rr_GraphNode *GraphNode =
        RR_ARENA_ALLOC_ONE(&Frame->Arena, sizeof(Rr_GraphNode));
    *RR_SLICE_PUSH(&Graph->NodesSlice, &Frame->Arena) = GraphNode;

    Rr_CopyDependencies(
        GraphNode,
        Dependencies,
        DependencyCount,
        &Frame->Arena);
    GraphNode->Type = RR_GRAPH_NODE_TYPE_PRESENT;
    GraphNode->Name = Name;

    Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
    *PresentNode = (Rr_PresentNode){
        .Info = *Info,
    };

    return GraphNode;
}

Rr_GraphNode *Rr_AddBuiltinNode(
    Rr_App *App,
    const char *Name,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_Graph *Graph = &Frame->Graph;

    Rr_GraphNode *GraphNode =
        RR_ARENA_ALLOC_ONE(&Frame->Arena, sizeof(Rr_GraphNode));
    *RR_SLICE_PUSH(&Graph->NodesSlice, &Frame->Arena) = GraphNode;

    Rr_CopyDependencies(
        GraphNode,
        Dependencies,
        DependencyCount,
        &Frame->Arena);
    GraphNode->Type = RR_GRAPH_NODE_TYPE_BUILTIN;
    GraphNode->Name = Name;

    Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.BuiltinNode;
    RR_ZERO_PTR(BuiltinNode);

    return GraphNode;
}

void Rr_DrawStaticMesh(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_StaticMesh *StaticMesh,
    Rr_Data PerDrawData)
{
    SDL_assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkDeviceSize Offset = Frame->PerDrawBuffer.Offset;

    for (size_t PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount;
         ++PrimitiveIndex)
    {
        *RR_SLICE_PUSH(
            &Node->Union.GraphicsNode.DrawPrimitivesSlice,
            &Frame->Arena) = (Rr_DrawPrimitiveInfo){
            .PerDrawOffset = Offset,
            .Primitive = StaticMesh->Primitives[PrimitiveIndex],
            .Material = StaticMesh->Materials[PrimitiveIndex],
        };
    }

    Rr_CopyToMappedUniformBuffer(
        App,
        Frame->PerDrawBuffer.Buffer,
        &Frame->PerDrawBuffer.Offset,
        PerDrawData);
}

void Rr_DrawStaticMeshOverrideMaterials(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_Material **OverrideMaterials,
    size_t OverrideMaterialCount,
    Rr_StaticMesh *StaticMesh,
    Rr_Data PerDrawData)
{
    SDL_assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkDeviceSize Offset = Frame->PerDrawBuffer.Offset;

    for (size_t PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount;
         ++PrimitiveIndex)
    {
        *RR_SLICE_PUSH(
            &Node->Union.GraphicsNode.DrawPrimitivesSlice,
            &Frame->Arena) = (Rr_DrawPrimitiveInfo){
            .PerDrawOffset = Offset,
            .Primitive = StaticMesh->Primitives[PrimitiveIndex],
            .Material = PrimitiveIndex < OverrideMaterialCount
                            ? OverrideMaterials[PrimitiveIndex]
                            : NULL,
        };
    }

    Rr_CopyToMappedUniformBuffer(
        App,
        Frame->PerDrawBuffer.Buffer,
        &Frame->PerDrawBuffer.Offset,
        PerDrawData);
}

static void Rr_DrawText(
    Rr_App *App,
    Rr_BuiltinNode *Node,
    Rr_DrawTextInfo *Info)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame(&App->Renderer);
    Rr_DrawTextInfo *NewInfo =
        RR_SLICE_PUSH(&Node->DrawTextsSlice, &Frame->Arena);
    *NewInfo = *Info;
    if (NewInfo->Font == NULL)
    {
        NewInfo->Font = App->Renderer.BuiltinFont;
    }
    if (NewInfo->Size == 0.0f)
    {
        NewInfo->Size = NewInfo->Font->DefaultSize;
    }
}

void Rr_DrawCustomText(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_Font *Font,
    Rr_String *String,
    Rr_Vec2 Position,
    float Size,
    Rr_DrawTextFlags Flags)
{
    SDL_assert(Node->Type == RR_GRAPH_NODE_TYPE_BUILTIN);

    Rr_DrawText(
        App,
        &Node->Union.BuiltinNode,
        &(Rr_DrawTextInfo){
            .Font = Font,
            .String = *String,
            .Position = Position,
            .Size = Size,
            .Flags = Flags,
        });
}

void Rr_DrawDefaultText(
    Rr_App *App,
    Rr_GraphNode *Node,
    Rr_String *String,
    Rr_Vec2 Position)
{
    SDL_assert(Node->Type == RR_GRAPH_NODE_TYPE_BUILTIN);

    Rr_DrawText(
        App,
        &Node->Union.BuiltinNode,
        &(Rr_DrawTextInfo){
            .String = *String,
            .Position = Position,
            .Size = 32.0f,
            .Flags = 0,
        });
}

Rr_DrawTarget *Rr_CreateDrawTarget(Rr_App *App, uint32_t Width, uint32_t Height)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_DrawTarget *DrawTarget = Rr_CreateObject(&App->ObjectStorage);

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Image *ColorImage =
            Rr_CreateColorAttachmentImage(App, Width, Height);
        Rr_Image *DepthImage =
            Rr_CreateDepthAttachmentImage(App, Width, Height);

        VkImageView Attachments[] = { ColorImage->View, DepthImage->View };

        VkFramebufferCreateInfo Info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .renderPass = Renderer->RenderPasses.ColorDepth,
            .height = Height,
            .width = Width,
            .layers = 1,
            .attachmentCount = SDL_arraysize(Attachments),
            .pAttachments = Attachments,
        };
        vkCreateFramebuffer(
            Renderer->Device,
            &Info,
            NULL,
            &DrawTarget->Frames[Index].Framebuffer);
        DrawTarget->Frames[Index].ColorImage = ColorImage;
        DrawTarget->Frames[Index].DepthImage = DepthImage;
    }

    return DrawTarget;
}

Rr_DrawTarget *Rr_CreateDrawTargetDepthOnly(
    Rr_App *App,
    uint32_t Width,
    uint32_t Height)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_DrawTarget *DrawTarget = Rr_CreateObject(&App->ObjectStorage);

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Image *DepthImage =
            Rr_CreateDepthAttachmentImage(App, Width, Height);

        VkImageView Attachments[] = { DepthImage->View };

        VkFramebufferCreateInfo Info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .renderPass = Renderer->RenderPasses.Depth,
            .height = Height,
            .width = Width,
            .layers = 1,
            .attachmentCount = SDL_arraysize(Attachments),
            .pAttachments = Attachments,
        };
        vkCreateFramebuffer(
            Renderer->Device,
            &Info,
            NULL,
            &DrawTarget->Frames[Index].Framebuffer);
        DrawTarget->Frames[Index].DepthImage = DepthImage;
    }

    return DrawTarget;
}

void Rr_DestroyDrawTarget(Rr_App *App, Rr_DrawTarget *DrawTarget)
{
    if (DrawTarget == NULL)
    {
        return;
    }

    Rr_Renderer *Renderer = &App->Renderer;

    for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        vkDestroyFramebuffer(
            Renderer->Device,
            DrawTarget->Frames[Index].Framebuffer,
            NULL);

        Rr_DestroyImage(App, DrawTarget->Frames[Index].ColorImage);
        Rr_DestroyImage(App, DrawTarget->Frames[Index].DepthImage);
    }

    Rr_DestroyObject(&App->ObjectStorage, DrawTarget);
}

Rr_DrawTarget *Rr_GetMainDrawTarget(Rr_App *App)
{
    return App->Renderer.DrawTarget;
}

static void Rr_ExecuteGraphBatch(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    /* Submit barriers. */

    size_t ImageBarrierCount = Graph->Batch.ImageBarriersSlice.Count;
    size_t BufferBarrierCount = Graph->Batch.BufferBarriersSlice.Count;
    if (ImageBarrierCount > 0 || BufferBarrierCount > 0)
    {
        if (Graph->StageMask == 0)
        {
            Graph->StageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(
            CommandBuffer,
            Graph->StageMask,
            Graph->Batch.StageMask,
            0,
            0,
            NULL,
            Graph->Batch.BufferBarriersSlice.Count,
            Graph->Batch.BufferBarriersSlice.Data,
            Graph->Batch.ImageBarriersSlice.Count,
            Graph->Batch.ImageBarriersSlice.Data);
    }

    for (size_t Index = 0; Index < Graph->Batch.NodesSlice.Count; ++Index)
    {
        Rr_GraphNode *GraphNode = Graph->Batch.NodesSlice.Data[Index];

        switch (GraphNode->Type)
        {
            case RR_GRAPH_NODE_TYPE_GRAPHICS:
            {
                Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.GraphicsNode;
                Rr_ExecuteGraphicsNode(App, Graph, GraphicsNode, Scratch.Arena);
            }
            break;
            case RR_GRAPH_NODE_TYPE_PRESENT:
            {
                Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
                Rr_ExecutePresentNode(App, Graph, PresentNode, Scratch.Arena);
            }
            break;
            case RR_GRAPH_NODE_TYPE_BUILTIN:
            {
                Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.BuiltinNode;
                Rr_ExecuteBuiltinNode(App, Graph, BuiltinNode, Scratch.Arena);
            }
            break;
            default:
            {
                Rr_LogAbort("Unsupported node type!");
            }
            break;
        }

        GraphNode->Executed = RR_TRUE;
    }

    /* Apply batch state. */
    /* @TODO: Account for node specific transitions. */

    Graph->StageMask = Graph->Batch.StageMask;
    for (size_t Index = 0; Index < ImageBarrierCount; ++Index)
    {
        VkImageMemoryBarrier *Barrier =
            Graph->Batch.ImageBarriersSlice.Data + Index;
        Rr_ImageSync **State = (Rr_ImageSync **)Rr_MapUpsert(
            &Graph->GlobalSyncMap,
            (uintptr_t)Barrier->image,
            &Frame->Arena);
        if (*State == NULL)
        {
            *State = RR_ARENA_ALLOC_ONE(&Frame->Arena, sizeof(Rr_ImageSync));
        }
        (*State)->AccessMask = Barrier->dstAccessMask;
        (*State)->Layout = Barrier->newLayout;
    }
    /* @TODO: Same for buffers. */

    /* Reset batch state. */

    RR_ZERO(Graph->Batch.ImageBarriersSlice);
    RR_ZERO(Graph->Batch.BufferBarriersSlice);
    RR_ZERO(Graph->Batch.NodesSlice);
    Graph->Batch.StageMask = 0;
    Graph->Batch.SyncMap = NULL;

    Rr_DestroyArenaScratch(Scratch);
}

void Rr_ExecuteGraph(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena)
{
    size_t NodeCount = Graph->NodesSlice.Count;
    if (NodeCount == 0)
    {
        Rr_LogAbort("Graph doesn't contain any nodes!");
    }

    size_t Counter = 0;
    while (RR_TRUE)
    {
        Counter++;

        Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

        Graph->Batch.Arena = Scratch.Arena;

        for (size_t Index = 0; Index < Graph->NodesSlice.Count; ++Index)
        {
            Rr_GraphNode *GraphNode = Graph->NodesSlice.Data[Index];

            if (GraphNode->Executed == RR_TRUE)
            {
                continue;
            }

            /* Dependency check. */

            Rr_Bool DependenciesResolved = RR_TRUE;
            for (size_t DepIndex = 0; DepIndex < GraphNode->Dependencies.Count;
                 ++DepIndex)
            {
                Rr_GraphNode *Dependency =
                    GraphNode->Dependencies.Data[DepIndex];
                if (Dependency->Executed != RR_TRUE)
                {
                    DependenciesResolved = RR_FALSE;
                    break;
                }
            }
            if (DependenciesResolved != RR_TRUE)
            {
                continue;
            }

            /* Attempt batching current node. */

            Rr_Bool NodeBatched = RR_FALSE;
            switch (GraphNode->Type)
            {
                case RR_GRAPH_NODE_TYPE_GRAPHICS:
                {
                    Rr_GraphicsNode *GraphicsNode =
                        &GraphNode->Union.GraphicsNode;
                    NodeBatched =
                        Rr_BatchGraphicsNode(App, Graph, GraphicsNode);
                }
                break;
                case RR_GRAPH_NODE_TYPE_PRESENT:
                {
                    Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
                    NodeBatched = Rr_BatchPresentNode(App, Graph, PresentNode);
                    if (NodeBatched)
                    {
                        Graph->Batch.Final = RR_TRUE;
                    }
                }
                break;
                case RR_GRAPH_NODE_TYPE_BUILTIN:
                {
                    Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.BuiltinNode;
                    NodeBatched = Rr_BatchBuiltinNode(App, Graph, BuiltinNode);
                }
                break;
                default:
                {
                    Rr_LogAbort("Unsupported node type!");
                }
                break;
            }
            if (NodeBatched)
            {
                *RR_SLICE_PUSH(&Graph->Batch.NodesSlice, Graph->Batch.Arena) =
                    GraphNode;
            }
        }

        if (Graph->NodesSlice.Count == 0)
        {
            Rr_LogAbort("Couldn't batch graph nodes, probably invalid graph!");
        }

        Rr_ExecuteGraphBatch(App, Graph, Scratch.Arena);

        Rr_DestroyArenaScratch(Scratch);

        if (Graph->Batch.Final)
        {
            break;
        }
    }
}

static Rr_Bool Rr_ImageBatchPossible(Rr_Graph *Graph, VkImage Image)
{
    Rr_ImageSync **State = (Rr_ImageSync **)
        Rr_MapUpsert(&Graph->Batch.SyncMap, (uintptr_t)Image, NULL);

    if (State != NULL)
    {
        /* @TODO: Should be true if for example current batch state matches
         * requested state. */
        return RR_FALSE;
    }

    return RR_TRUE;
}

Rr_Bool Rr_SyncImage(
    Rr_App *App,
    Rr_Graph *Graph,
    VkImage Image,
    VkImageAspectFlags AspectMask,
    VkPipelineStageFlags StageMask,
    VkAccessFlags AccessMask,
    VkImageLayout Layout)
{
    if (Image == NULL)
    {
        return RR_TRUE;
    }

    if (!Rr_ImageBatchPossible(Graph, Image))
    {
        return RR_FALSE;
    }

    Rr_Frame *Frame = Rr_GetCurrentFrame(&App->Renderer);

    Rr_ImageSync **GlobalState = (Rr_ImageSync **)
        Rr_MapUpsert(&Graph->GlobalSyncMap, (uintptr_t)Image, &Frame->Arena);

    VkImageSubresourceRange SubresourceRange =
        GetImageSubresourceRange(AspectMask);

    if (*GlobalState == NULL) /* First time referencing this image. */
    {
        *RR_SLICE_PUSH(&Graph->Batch.ImageBarriersSlice, Graph->Batch.Arena) =
            (VkImageMemoryBarrier){
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
        Rr_ImageSync *OldState = *GlobalState;

        *RR_SLICE_PUSH(&Graph->Batch.ImageBarriersSlice, Graph->Batch.Arena) =
            (VkImageMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = NULL,
                .srcAccessMask = OldState->AccessMask,
                .dstAccessMask = AccessMask,
                .oldLayout = OldState->Layout,
                .newLayout = Layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = Image,
                .subresourceRange = SubresourceRange,
            };
    }

    Rr_ImageSync **BatchState = (Rr_ImageSync **)Rr_MapUpsert(
        &Graph->Batch.SyncMap,
        (uintptr_t)Image,
        Graph->Batch.Arena);
    if (*BatchState == NULL)
    {
        *BatchState = RR_ARENA_ALLOC_ONE(&Frame->Arena, sizeof(Rr_ImageSync));
    }
    (*BatchState)->AccessMask = AccessMask;
    (*BatchState)->Layout = Layout;

    Graph->Batch.StageMask |= StageMask;

    return RR_TRUE;
}
