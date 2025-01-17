#include "Rr_GraphicsNode.h"

#include "Rr_App.h"
#include "Rr_Material.h"
#include "Rr_Mesh.h"

#include <qsort/qsort-inline.h>

#include <SDL3/SDL_assert.h>

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

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_GRAPHICS, Name, Dependencies, DependencyCount);

    Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.GraphicsNode;
    *GraphicsNode = (Rr_GraphicsNode){
        .Info = *Info,
    };

    if(GraphicsNode->Info.DrawTarget == NULL)
    {
        GraphicsNode->Info.DrawTarget = Renderer->DrawTarget;
        GraphicsNode->Info.Viewport.Width = (int32_t)Renderer->SwapchainSize.width;
        GraphicsNode->Info.Viewport.Height = (int32_t)Renderer->SwapchainSize.height;
    }

    memcpy(GraphicsNode->GlobalsData, GlobalsData, Info->BasePipeline->Sizes.Globals);

    return GraphNode;
}

void Rr_DrawStaticMesh(Rr_App *App, Rr_GraphNode *Node, Rr_StaticMesh *StaticMesh, Rr_Data PerDrawData)
{
    SDL_assert(Node->Type == RR_GRAPH_NODE_TYPE_GRAPHICS);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    VkDeviceSize Offset = Frame->PerDrawBuffer.Offset;

    for(size_t PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount; ++PrimitiveIndex)
    {
        *RR_SLICE_PUSH(&Node->Union.GraphicsNode.DrawPrimitivesSlice, Frame->Arena) = (Rr_DrawPrimitiveInfo){
            .PerDrawOffset = Offset,
            .Primitive = StaticMesh->Primitives[PrimitiveIndex],
            .Material = StaticMesh->Materials[PrimitiveIndex],
        };
    }

    Rr_CopyToMappedUniformBuffer(App, Frame->PerDrawBuffer.Buffer, &Frame->PerDrawBuffer.Offset, PerDrawData);
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

    for(size_t PrimitiveIndex = 0; PrimitiveIndex < StaticMesh->PrimitiveCount; ++PrimitiveIndex)
    {
        *RR_SLICE_PUSH(&Node->Union.GraphicsNode.DrawPrimitivesSlice, Frame->Arena) = (Rr_DrawPrimitiveInfo){
            .PerDrawOffset = Offset,
            .Primitive = StaticMesh->Primitives[PrimitiveIndex],
            .Material = PrimitiveIndex < OverrideMaterialCount ? OverrideMaterials[PrimitiveIndex] : NULL,
        };
    }

    Rr_CopyToMappedUniformBuffer(App, Frame->PerDrawBuffer.Buffer, &Frame->PerDrawBuffer.Offset, PerDrawData);
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

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(RR_MAX_TEXTURES_PER_MATERIAL, 1, Scratch.Arena);

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
        (Rr_Data){ .Ptr = GlobalsData, .Size = Context.BasePipeline->Sizes.Globals });

    /* Allocate, write and bind globals descriptor set. */
    Context.GlobalsDescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Renderer->Device,
        Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS]);
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
    Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, Context.GlobalsDescriptorSet);
    Rr_ResetDescriptorWriter(&DescriptorWriter);

    Rr_DestroyArenaScratch(Scratch);

    return Context;
}

SDL_FORCE_INLINE int Rr_CompareDrawPrimitive(Rr_DrawPrimitiveInfo *A, Rr_DrawPrimitiveInfo *B)
{
    if(A->Material->GenericPipeline != B->Material->GenericPipeline)
    {
        return (A->Material->GenericPipeline > B->Material->GenericPipeline) ? 1 : -1;
    }
    if(A->Material != B->Material)
    {
        return (A->Material > B->Material) ? 1 : -1;
    }
    if(A->Primitive != B->Primitive)
    {
        return (A->Primitive > B->Primitive) ? 1 : -1;
    }
    if(A->PerDrawOffset != B->PerDrawOffset)
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

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(RR_MAX_TEXTURES_PER_MATERIAL, 1, Scratch.Arena);

    Rr_GenericPipeline *BoundPipeline = NULL;
    Rr_Material *BoundMaterial = NULL;
    Rr_Primitive *BoundPrimitive = NULL;
    uint32_t BoundPerDrawOffset = UINT32_MAX;

    /* @TODO: Sort indices instead! */
    QSORT(Rr_DrawPrimitiveInfo, Rr_CompareDrawPrimitive)
    (DrawPrimitivesSlice.Data, DrawPrimitivesSlice.Count);

    vkCmdBindDescriptorSets(
        CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        Renderer->GenericPipelineLayout,
        RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS,
        1,
        &GenericRenderingContext->GlobalsDescriptorSet,
        0,
        NULL);

    for(size_t Index = 0; Index < DrawPrimitivesSlice.Count; ++Index)
    {
        Rr_DrawPrimitiveInfo *Info = DrawPrimitivesSlice.Data + Index;

        Rr_GenericPipeline *TargetPipeline;
        if(GenericRenderingContext->OverridePipeline)
        {
            TargetPipeline = GenericRenderingContext->OverridePipeline;
        }
        else
        {
            TargetPipeline = Info->Material->GenericPipeline;
        }

        if(BoundPipeline != TargetPipeline)
        {
            BoundPipeline = TargetPipeline;

            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TargetPipeline->Pipeline->Handle);
        }

        if(BoundMaterial != Info->Material)
        {
            BoundMaterial = Info->Material;

            VkDescriptorSet MaterialDescriptorSet = Rr_AllocateDescriptorSet(
                &Frame->DescriptorAllocator,
                Renderer->Device,
                Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL]);
            Rr_WriteBufferDescriptor(
                &DescriptorWriter,
                0,
                BoundMaterial->Buffer->Handle,
                GenericRenderingContext->BasePipeline->Sizes.Material,
                0,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                Scratch.Arena);
            for(size_t TextureIndex = 0; TextureIndex < RR_MAX_TEXTURES_PER_MATERIAL; ++TextureIndex)
            {
                VkImageView ImageView = VK_NULL_HANDLE;
                if(BoundMaterial->TextureCount > TextureIndex && BoundMaterial->Textures[TextureIndex] != NULL)
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
            Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, MaterialDescriptorSet);
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

        if(BoundPrimitive != Info->Primitive)
        {
            BoundPrimitive = Info->Primitive;

            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &BoundPrimitive->VertexBuffer->Handle, &(VkDeviceSize){ 0 });
            vkCmdBindIndexBuffer(CommandBuffer, BoundPrimitive->IndexBuffer->Handle, 0, VK_INDEX_TYPE_UINT32);
        }

        /* Raw offset should be enough to differentiate between draws
         * however note that each PerDraw size needs its own descriptor set.*/
        if(BoundPerDrawOffset != Info->PerDrawOffset)
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

        if(BoundPrimitive == NULL)
        {
            continue;
        }

        vkCmdDrawIndexed(CommandBuffer, BoundPrimitive->IndexCount, 1, 0, 0, 0);
    }

    Rr_DestroyArenaScratch(Scratch);
}

bool Rr_BatchGraphicsNode(Rr_App *App, Rr_Graph *Graph, Rr_GraphBatch *Batch, Rr_GraphicsNode *Node)
{
    Rr_DrawTarget *DrawTarget = Node->Info.DrawTarget;

    if(DrawTarget->Frames[App->Renderer.CurrentFrameIndex].ColorImage &&
       Rr_SyncImage(
           App,
           Graph,
           Batch,
           DrawTarget->Frames[App->Renderer.CurrentFrameIndex].ColorImage->Handle,
           VK_IMAGE_ASPECT_COLOR_BIT,
           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
           VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) != true)
    {
        return false;
    }

    if(DrawTarget->Frames[App->Renderer.CurrentFrameIndex].DepthImage &&
       Rr_SyncImage(
           App,
           Graph,
           Batch,
           DrawTarget->Frames[App->Renderer.CurrentFrameIndex].DepthImage->Handle,
           VK_IMAGE_ASPECT_DEPTH_BIT,
           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) != true)
    {
        return false;
    }

    return true;
}

void Rr_ExecuteGraphicsNode(Rr_App *App, Rr_GraphicsNode *Node, Rr_Arena *Arena)
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
        Rr_MakeGenericRenderingContext(App, &UploadContext, &Node->Info, Node->GlobalsData, Scratch.Arena);

    /* Line up appropriate clear values. */

    uint32_t ColorAttachmentCount = Node->Info.BasePipeline->Pipeline->ColorAttachmentCount;
    VkClearValue *ClearValues = Rr_StackAlloc(VkClearValue, ColorAttachmentCount + 1);
    for(uint32_t Index = 0; Index < ColorAttachmentCount; ++Index)
    {
        ClearValues[Index] = (VkClearValue){ 0 };
    }
    ClearValues[ColorAttachmentCount] = (VkClearValue){ .depthStencil.depth = 1.0f };

    /* Begin render pass. */

    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = DrawTarget->Frames[Renderer->CurrentFrameIndex].Framebuffer,
        .renderArea = (VkRect2D){ { Viewport.X, Viewport.Y }, { Viewport.Z, Viewport.W } },
        .renderPass = Node->Info.BasePipeline->Pipeline->RenderPass,
        .clearValueCount = ColorAttachmentCount + 1,
        .pClearValues = ClearValues,
    };
    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

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

    Rr_RenderGeneric(App, &GenericRenderingContext, Node->DrawPrimitivesSlice, CommandBuffer, Scratch.Arena);

    vkCmdEndRenderPass(CommandBuffer);

    Rr_DestroyArenaScratch(Scratch);
}
