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

#include <qsort/qsort-inline.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_timer.h>

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
    GraphNode->Dependencies.Length = DependencyCount;
    // for (size_t Index = 0; Index < DependencyCount; ++Index)
    // {
    //     *RR_SLICE_PUSH(&GraphNode->Dependencies, Arena) =
    //     Dependencies[Index];
    // }
}

Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_App *App,
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

    Rr_PresentNode *PresentNode = &GraphNode->Union.PresentNode;
    *PresentNode = (Rr_PresentNode){
        .Info = *Info,
    };

    return GraphNode;
}

Rr_GraphNode *Rr_AddBuiltinNode(
    Rr_App *App,
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
        vkDestroyFramebuffer(Renderer->Device, DrawTarget->Frames[Index].Framebuffer, NULL);

        Rr_DestroyImage(App, DrawTarget->Frames[Index].ColorImage);
        Rr_DestroyImage(App, DrawTarget->Frames[Index].DepthImage);
    }

    Rr_DestroyObject(&App->ObjectStorage, DrawTarget);
}

Rr_DrawTarget *Rr_GetMainDrawTarget(Rr_App *App)
{
    return App->Renderer.DrawTarget;
}

static Rr_GenericRenderingContext Rr_MakeGenericRenderingContext(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_GraphicsNodeInfo *PassInfo,
    char *GlobalsData,
    Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;

    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    Rr_WriteBuffer *CommonBuffer = &Frame->CommonBuffer;

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(
        RR_MAX_TEXTURES_PER_MATERIAL,
        1,
        Scratch.Arena);

    Rr_GenericRenderingContext Context = {
        .BasePipeline = PassInfo->BasePipeline,
        .OverridePipeline = PassInfo->OverridePipeline,
    };

    /* Upload globals data. */
    /* @TODO: Make these take a Rr_WriteBuffer instead! */
    VkDeviceSize BufferOffset = CommonBuffer->Offset;
    Rr_UploadToUniformBuffer(
        App,
        UploadContext,
        CommonBuffer->Buffer,
        &CommonBuffer->Offset,
        (Rr_Data){ .Ptr = GlobalsData,
                   .Size = Context.BasePipeline->Sizes.Globals });

    /* Allocate, write and bind globals descriptor set. */
    Context.GlobalsDescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Renderer->Device,
        Renderer->GenericDescriptorSetLayouts
            [RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS]);
    Rr_WriteBufferDescriptor(
        &DescriptorWriter,
        0,
        CommonBuffer->Buffer->Handle,
        Context.BasePipeline->Sizes.Globals,
        BufferOffset,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        Scratch.Arena);
    //    Rr_WriteImageDescriptor(&DescriptorWriter, 1, PocDiffuseImage.View,
    //    Renderer->NearestSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Rr_UpdateDescriptorSet(
        &DescriptorWriter,
        Renderer->Device,
        Context.GlobalsDescriptorSet);
    Rr_ResetDescriptorWriter(&DescriptorWriter);

    Rr_DestroyArenaScratch(Scratch);

    return Context;
}

SDL_FORCE_INLINE int Rr_CompareDrawPrimitive(
    Rr_DrawPrimitiveInfo *A,
    Rr_DrawPrimitiveInfo *B)
{
    if (A->Material->GenericPipeline != B->Material->GenericPipeline)
    {
        return (A->Material->GenericPipeline > B->Material->GenericPipeline)
                   ? 1
                   : -1;
    }
    if (A->Material != B->Material)
    {
        return (A->Material > B->Material) ? 1 : -1;
    }
    if (A->Primitive != B->Primitive)
    {
        return (A->Primitive > B->Primitive) ? 1 : -1;
    }
    if (A->PerDrawOffset != B->PerDrawOffset)
    {
        return (A->PerDrawOffset > B->PerDrawOffset) ? 1 : -1;
    }
    return 0;
}

DEF_QSORT(Rr_DrawPrimitiveInfo, Rr_CompareDrawPrimitive) /* NOLINT */

static void Rr_RenderGeneric(
    Rr_App *App,
    Rr_GenericRenderingContext *GenericRenderingContext,
    Rr_DrawPrimitivesSlice DrawPrimitivesSlice,
    VkCommandBuffer CommandBuffer,
    Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;

    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    size_t FrameIndex = Renderer->CurrentFrameIndex;

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(
        RR_MAX_TEXTURES_PER_MATERIAL,
        1,
        Scratch.Arena);

    Rr_GenericPipeline *BoundPipeline = NULL;
    Rr_Material *BoundMaterial = NULL;
    Rr_Primitive *BoundPrimitive = NULL;
    uint32_t BoundPerDrawOffset = UINT32_MAX;

    /* @TODO: Sort indices instead! */
    QSORT(Rr_DrawPrimitiveInfo, Rr_CompareDrawPrimitive)
    (DrawPrimitivesSlice.Data, DrawPrimitivesSlice.Length);

    vkCmdBindDescriptorSets(
        CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        Renderer->GenericPipelineLayout,
        RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS,
        1,
        &GenericRenderingContext->GlobalsDescriptorSet,
        0,
        NULL);

    for (size_t Index = 0; Index < DrawPrimitivesSlice.Length; ++Index)
    {
        Rr_DrawPrimitiveInfo *Info = DrawPrimitivesSlice.Data + Index;

        Rr_GenericPipeline *TargetPipeline;
        if (GenericRenderingContext->OverridePipeline)
        {
            TargetPipeline = GenericRenderingContext->OverridePipeline;
        }
        else
        {
            TargetPipeline = Info->Material->GenericPipeline;
        }

        if (BoundPipeline != TargetPipeline)
        {
            BoundPipeline = TargetPipeline;

            vkCmdBindPipeline(
                CommandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                TargetPipeline->Pipeline->Handle);
        }

        if (BoundMaterial != Info->Material)
        {
            BoundMaterial = Info->Material;

            VkDescriptorSet MaterialDescriptorSet = Rr_AllocateDescriptorSet(
                &Frame->DescriptorAllocator,
                Renderer->Device,
                Renderer->GenericDescriptorSetLayouts
                    [RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL]);
            Rr_WriteBufferDescriptor(
                &DescriptorWriter,
                0,
                BoundMaterial->Buffer->Handle,
                GenericRenderingContext->BasePipeline->Sizes.Material,
                0,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                Scratch.Arena);
            for (size_t TextureIndex = 0;
                 TextureIndex < RR_MAX_TEXTURES_PER_MATERIAL;
                 ++TextureIndex)
            {
                VkImageView ImageView = VK_NULL_HANDLE;
                if (BoundMaterial->TextureCount > TextureIndex &&
                    BoundMaterial->Textures[TextureIndex] != NULL)
                {
                    ImageView = BoundMaterial->Textures[TextureIndex]->View;
                }
                else
                {
                    ImageView = Renderer->NullTextures.White->View;
                }
                Rr_WriteImageDescriptorAt(
                    &DescriptorWriter,
                    1,
                    TextureIndex,
                    ImageView,
                    Renderer->NearestSampler,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    Scratch.Arena);
            }
            Rr_UpdateDescriptorSet(
                &DescriptorWriter,
                Renderer->Device,
                MaterialDescriptorSet);
            Rr_ResetDescriptorWriter(&DescriptorWriter);

            vkCmdBindDescriptorSets(
                CommandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                Renderer->GenericPipelineLayout,
                RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL,
                1,
                &MaterialDescriptorSet,
                0,
                NULL);
        }

        if (BoundPrimitive != Info->Primitive)
        {
            BoundPrimitive = Info->Primitive;

            vkCmdBindVertexBuffers(
                CommandBuffer,
                0,
                1,
                &BoundPrimitive->VertexBuffer->Handle,
                &(VkDeviceSize){ 0 });
            vkCmdBindIndexBuffer(
                CommandBuffer,
                BoundPrimitive->IndexBuffer->Handle,
                0,
                VK_INDEX_TYPE_UINT32);
        }

        /* Raw offset should be enough to differentiate between draws
         * however note that each PerDraw size needs its own descriptor set.*/
        if (BoundPerDrawOffset != Info->PerDrawOffset)
        {
            BoundPerDrawOffset = Info->PerDrawOffset;

            vkCmdBindDescriptorSets(
                CommandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                Renderer->GenericPipelineLayout,
                RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW,
                1,
                &BoundPipeline->PerDrawDescriptorSets[FrameIndex],
                1,
                &BoundPerDrawOffset);
        }

        if (BoundPrimitive == NULL)
        {
            continue;
        }

        vkCmdDrawIndexed(CommandBuffer, BoundPrimitive->IndexCount, 1, 0, 0, 0);
    }

    Rr_DestroyArenaScratch(Scratch);
}

static void Rr_ExecuteGraphBatch(Rr_App *App, Rr_Graph *Graph, Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    /* Submit barriers. */

    size_t ImageBarrierCount =
        RR_SLICE_LENGTH(&Graph->Batch.ImageBarriersSlice);
    size_t BufferBarrierCount =
        RR_SLICE_LENGTH(&Graph->Batch.BufferBarriersSlice);
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
            Graph->Batch.BufferBarriersSlice.Length,
            Graph->Batch.BufferBarriersSlice.Data,
            Graph->Batch.ImageBarriersSlice.Length,
            Graph->Batch.ImageBarriersSlice.Data);
    }

    for (size_t Index = 0; Index < RR_SLICE_LENGTH(&Graph->Batch.NodesSlice);
         ++Index)
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
    size_t NodeCount = RR_SLICE_LENGTH(&Graph->NodesSlice);
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

        for (size_t Index = 0; Index < RR_SLICE_LENGTH(&Graph->NodesSlice);
             ++Index)
        {
            Rr_GraphNode *GraphNode = Graph->NodesSlice.Data[Index];

            if (GraphNode->Executed == RR_TRUE)
            {
                continue;
            }

            /* Dependency check. */

            Rr_Bool DependenciesResolved = RR_TRUE;
            for (size_t DepIndex = 0;
                 DepIndex < RR_SLICE_LENGTH(&GraphNode->Dependencies);
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

        if (RR_SLICE_LENGTH(&Graph->NodesSlice) == 0)
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

Rr_Bool Rr_BatchPresentNode(Rr_App *App, Rr_Graph *Graph, Rr_PresentNode *Node)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_DrawTarget *DrawTarget = Renderer->DrawTarget;

    if (Rr_SyncImage(
            App,
            Graph,
            DrawTarget->Frames[App->Renderer.CurrentFrameIndex].ColorImage->Handle,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT |
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) != RR_TRUE ||
        Rr_SyncImage(
            App,
            Graph,
            DrawTarget->Frames[App->Renderer.CurrentFrameIndex].DepthImage->Handle,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) != RR_TRUE ||
        Rr_SyncImage(
            App,
            Graph,
            Graph->SwapchainImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) != RR_TRUE)
    {
        return RR_FALSE;
    }

    return RR_TRUE;
}

void Rr_ExecutePresentNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_PresentNode *Node,
    Rr_Arena *Arena)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_DrawTarget *DrawTarget = Renderer->DrawTarget;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    /* Render Dear ImGui if needed. */

    Rr_ImGui *ImGui = &Renderer->ImGui;
    if (ImGui->IsInitialized)
    {
        VkRenderPassBeginInfo RenderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = Renderer->RenderPasses.ColorDepthLoad,
            .framebuffer = Renderer->DrawTarget->Frames[Renderer->CurrentFrameIndex].Framebuffer,
            .renderArea.extent.width =
                Renderer->DrawTarget->Frames[Renderer->CurrentFrameIndex].ColorImage->Extent.width,
            .renderArea.extent.height =
                Renderer->DrawTarget->Frames[Renderer->CurrentFrameIndex].ColorImage->Extent.height,
            .clearValueCount = 0,
            .pClearValues = NULL,
        };
        vkCmdBeginRenderPass(
            CommandBuffer,
            &RenderPassBeginInfo,
            VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(
            igGetDrawData(),
            CommandBuffer,
            VK_NULL_HANDLE);

        vkCmdEndRenderPass(CommandBuffer);
    }

    Rr_ImageBarrier ColorImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = DrawTarget->Frames[Renderer->CurrentFrameIndex].ColorImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .AccessMask = VK_ACCESS_TRANSFER_READ_BIT |
                      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT |
                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    Rr_ChainImageBarrier(
        &ColorImageTransition,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    Rr_ImageBarrier SwapchainImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = Graph->SwapchainImage,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_NONE,
        .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    Rr_ChainImageBarrier(
        &SwapchainImageTransition,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    Rr_BlitColorImage(
        CommandBuffer,
        DrawTarget->Frames[Renderer->CurrentFrameIndex].ColorImage->Handle,
        Graph->SwapchainImage,
        Renderer->SwapchainSize,
        Renderer->SwapchainSize);

    Rr_ChainImageBarrier(
        &SwapchainImageTransition,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

Rr_Bool Rr_BatchGraphicsNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node)
{
    Rr_DrawTarget *DrawTarget = Node->Info.DrawTarget;

    if (DrawTarget->Frames[App->Renderer.CurrentFrameIndex].ColorImage &&
        Rr_SyncImage(
            App,
            Graph,
            DrawTarget->Frames[App->Renderer.CurrentFrameIndex].ColorImage->Handle,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) != RR_TRUE)
    {
        return RR_FALSE;
    }

    if (DrawTarget->Frames[App->Renderer.CurrentFrameIndex].DepthImage &&
        Rr_SyncImage(
            App,
            Graph,
            DrawTarget->Frames[App->Renderer.CurrentFrameIndex].DepthImage->Handle,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) != RR_TRUE)
    {
        return RR_FALSE;
    }

    return RR_TRUE;
}

void Rr_ExecuteGraphicsNode(
    Rr_App *App,
    Rr_Graph *Graph,
    Rr_GraphicsNode *Node,
    Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_DrawTarget *DrawTarget = Node->Info.DrawTarget;

    Rr_IntVec4 Viewport = Node->Info.Viewport;

    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    Rr_UploadContext UploadContext = {
        .StagingBuffer = &Frame->StagingBuffer,
        .TransferCommandBuffer = CommandBuffer,
    };

    Rr_GenericRenderingContext GenericRenderingContext =
        Rr_MakeGenericRenderingContext(
            App,
            &UploadContext,
            &Node->Info,
            Node->GlobalsData,
            Scratch.Arena);

    /* Line up appropriate clear values. */

    uint32_t ColorAttachmentCount =
        Node->Info.BasePipeline->Pipeline->ColorAttachmentCount;
    VkClearValue *ClearValues =
        Rr_StackAlloc(VkClearValue, ColorAttachmentCount + 1);
    for (uint32_t Index = 0; Index < ColorAttachmentCount; ++Index)
    {
        ClearValues[Index] = (VkClearValue){ 0 };
    }
    ClearValues[ColorAttachmentCount] =
        (VkClearValue){ .depthStencil.depth = 1.0f };

    /* Begin render pass. */

    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = DrawTarget->Frames[Renderer->CurrentFrameIndex].Framebuffer,
        .renderArea = (VkRect2D){ { Viewport.X, Viewport.Y },
                                  { Viewport.Z, Viewport.W } },
        .renderPass = Node->Info.BasePipeline->Pipeline->RenderPass,
        .clearValueCount = ColorAttachmentCount + 1,
        .pClearValues = ClearValues,
    };
    vkCmdBeginRenderPass(
        CommandBuffer,
        &RenderPassBeginInfo,
        VK_SUBPASS_CONTENTS_INLINE);

    Rr_StackFree(ClearValues);

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

    Rr_RenderGeneric(
        App,
        &GenericRenderingContext,
        Node->DrawPrimitivesSlice,
        CommandBuffer,
        Scratch.Arena);

    vkCmdEndRenderPass(CommandBuffer);

    Rr_DestroyArenaScratch(Scratch);
}
