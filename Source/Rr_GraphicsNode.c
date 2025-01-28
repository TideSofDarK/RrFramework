#include "Rr_GraphicsNode.h"

#include "Rr_App.h"

#include <assert.h>

Rr_GraphNode *Rr_AddGraphicsNode(
    Rr_App *App,
    const char *Name,
    Rr_ColorTarget *ColorTargets,
    size_t ColorTargetCount,
    Rr_DepthTarget *DepthTarget,
    Rr_GraphNode **Dependencies,
    size_t DependencyCount)
{
    assert(ColorTargetCount > 0 || DepthTarget != NULL);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_GRAPHICS, Name, Dependencies, DependencyCount);

    Rr_GraphicsNode *GraphicsNode = &GraphNode->Union.GraphicsNode;
    if(ColorTargetCount > 0)
    {
        GraphicsNode->ColorTargets = RR_ALLOC_STRUCT_COUNT(Frame->Arena, Rr_ColorTarget, ColorTargetCount);
        memcpy(GraphicsNode->ColorTargets, ColorTargets, sizeof(Rr_ColorTarget) * ColorTargetCount);
        GraphicsNode->ColorTargetCount = ColorTargetCount;

        GraphicsNode->AllocatedColorTargets =
            RR_ALLOC_STRUCT_COUNT(Frame->Arena, Rr_AllocatedImage *, ColorTargetCount);
        for(size_t Index = 0; Index < ColorTargetCount; ++Index)
        {
            GraphicsNode->AllocatedColorTargets[Index] = Rr_GetCurrentAllocatedImage(App, ColorTargets[Index].Image);
        }
    }
    if(DepthTarget != NULL)
    {
        GraphicsNode->DepthTarget = RR_ALLOC_STRUCT(Frame->Arena, Rr_DepthTarget);
        memcpy(GraphicsNode->DepthTarget, DepthTarget, sizeof(Rr_DepthTarget));

        GraphicsNode->AllocatedDepthTarget = Rr_GetCurrentAllocatedImage(App, DepthTarget->Image);
    }
    GraphicsNode->Encoded = RR_ALLOC(Frame->Arena, sizeof(Rr_GraphicsNodeFunction));
    GraphicsNode->EncodedFirst = GraphicsNode->Encoded;

    return GraphNode;
}

// static Rr_GenericRenderingContext Rr_MakeGenericRenderingContext(
//     Rr_App *App,
//     Rr_UploadContext *UploadContext,
//     Rr_GraphicsNodeInfo *PassInfo,
//     char *GlobalsData,
//     Rr_Arena *Arena)
// {
//     Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);
//
//     Rr_Renderer *Renderer = &App->Renderer;
//
//     Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
//     Rr_WriteBuffer *CommonBuffer = &Frame->CommonBuffer;
//
//     Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(RR_MAX_TEXTURES_PER_MATERIAL, 1, Scratch.Arena);
//
//     Rr_GenericRenderingContext Context = {
//         .BasePipeline = PassInfo->BasePipeline,
//         .OverridePipeline = PassInfo->OverridePipeline,
//     };
//
//     /* Upload globals data. */
//     /* @TODO: Make these take a Rr_WriteBuffer instead! */
//     VkDeviceSize BufferOffset = CommonBuffer->Offset;
//     Rr_UploadToUniformBuffer(
//         App,
//         UploadContext,
//         CommonBuffer->Buffer,
//         &CommonBuffer->Offset,
//         RR_MAKE_DATA(GlobalsData, Context.BasePipeline->Sizes.Globals));
//
//     /* Allocate, write and bind globals descriptor set. */
//     Context.GlobalsDescriptorSet = Rr_AllocateDescriptorSet(
//         &Frame->DescriptorAllocator,
//         Renderer->Device,
//         Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS]);
//     Rr_WriteBufferDescriptor(
//         &DescriptorWriter,
//         0,
//         CommonBuffer->Buffer->Handle,
//         Context.BasePipeline->Sizes.Globals,
//         BufferOffset,
//         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//         Scratch.Arena);
//     //    Rr_WriteImageDescriptor(&DescriptorWriter, 1, PocDiffuseImage.View,
//     //    Renderer->NearestSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//     //    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//     Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, Context.GlobalsDescriptorSet);
//     Rr_ResetDescriptorWriter(&DescriptorWriter);
//
//     Rr_DestroyArenaScratch(Scratch);
//
//     return Context;
//     return (Rr_GenericRenderingContext){ 0 };
// }

// DEF_QSORT(Rr_DrawPrimitiveInfo, Rr_CompareDrawPrimitive) /* NOLINT */

// static void Rr_RenderGeneric(
//     Rr_App *App,
//     Rr_GenericRenderingContext *GenericRenderingContext,
//     Rr_DrawPrimitivesSlice DrawPrimitivesSlice,
//     VkCommandBuffer CommandBuffer,
//     Rr_Arena *Arena)
// {
//     Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);
//
//     Rr_Renderer *Renderer = &App->Renderer;
//
//     Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
//     size_t FrameIndex = Renderer->CurrentFrameIndex;
//
//     Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(RR_MAX_TEXTURES_PER_MATERIAL, 1, Scratch.Arena);
//
//     Rr_GenericPipeline *BoundPipeline = NULL;
//     Rr_Material *BoundMaterial = NULL;
//     Rr_Primitive *BoundPrimitive = NULL;
//     uint32_t BoundPerDrawOffset = UINT32_MAX;
//
//     /* @TODO: Sort indices instead! */
//     // QSORT(Rr_DrawPrimitiveInfo, Rr_CompareDrawPrimitive)
//     // (DrawPrimitivesSlice.Data, DrawPrimitivesSlice.Count);
//
//     vkCmdBindDescriptorSets(
//         CommandBuffer,
//         VK_PIPELINE_BIND_POINT_GRAPHICS,
//         Renderer->GenericPipelineLayout,
//         RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS,
//         1,
//         &GenericRenderingContext->GlobalsDescriptorSet,
//         0,
//         NULL);
//
//     for(size_t Index = 0; Index < DrawPrimitivesSlice.Count; ++Index)
//     {
//         Rr_DrawPrimitiveInfo *Info = DrawPrimitivesSlice.Data + Index;
//
//         Rr_GenericPipeline *TargetPipeline;
//         if(GenericRenderingContext->OverridePipeline)
//         {
//             TargetPipeline = GenericRenderingContext->OverridePipeline;
//         }
//         else
//         {
//             TargetPipeline = Info->Material->GenericPipeline;
//         }
//
//         if(BoundPipeline != TargetPipeline)
//         {
//             BoundPipeline = TargetPipeline;
//
//             vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TargetPipeline->Pipeline->Handle);
//         }
//
//         if(BoundMaterial != Info->Material)
//         {
//             BoundMaterial = Info->Material;
//
//             VkDescriptorSet MaterialDescriptorSet = Rr_AllocateDescriptorSet(
//                 &Frame->DescriptorAllocator,
//                 Renderer->Device,
//                 Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL]);
//             Rr_WriteBufferDescriptor(
//                 &DescriptorWriter,
//                 0,
//                 BoundMaterial->Buffer->Handle,
//                 GenericRenderingContext->BasePipeline->Sizes.Material,
//                 0,
//                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//                 Scratch.Arena);
//             for(size_t TextureIndex = 0; TextureIndex < RR_MAX_TEXTURES_PER_MATERIAL; ++TextureIndex)
//             {
//                 VkImageView ImageView = VK_NULL_HANDLE;
//                 if(BoundMaterial->TextureCount > TextureIndex && BoundMaterial->Textures[TextureIndex] != NULL)
//                 {
//                     ImageView = BoundMaterial->Textures[TextureIndex]->View;
//                 }
//                 else
//                 {
//                     ImageView = Renderer->NullTextures.White->View;
//                 }
//                 Rr_WriteImageDescriptorAt(
//                     &DescriptorWriter,
//                     1,
//                     TextureIndex,
//                     ImageView,
//                     Renderer->NearestSampler,
//                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                     Scratch.Arena);
//             }
//             Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, MaterialDescriptorSet);
//             Rr_ResetDescriptorWriter(&DescriptorWriter);
//
//             vkCmdBindDescriptorSets(
//                 CommandBuffer,
//                 VK_PIPELINE_BIND_POINT_GRAPHICS,
//                 Renderer->GenericPipelineLayout,
//                 RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL,
//                 1,
//                 &MaterialDescriptorSet,
//                 0,
//                 NULL);
//         }
//
//         if(BoundPrimitive != Info->Primitive)
//         {
//             BoundPrimitive = Info->Primitive;
//
//             vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &BoundPrimitive->VertexBuffer->Handle, &(VkDeviceSize){ 0 });
//             vkCmdBindIndexBuffer(CommandBuffer, BoundPrimitive->IndexBuffer->Handle, 0, VK_INDEX_TYPE_UINT32);
//         }
//
//         /* Raw offset should be enough to differentiate between draws
//          * however note that each PerDraw size needs its own descriptor set.*/
//         if(BoundPerDrawOffset != Info->PerDrawOffset)
//         {
//             BoundPerDrawOffset = Info->PerDrawOffset;
//
//             vkCmdBindDescriptorSets(
//                 CommandBuffer,
//                 VK_PIPELINE_BIND_POINT_GRAPHICS,
//                 Renderer->GenericPipelineLayout,
//                 RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW,
//                 1,
//                 &BoundPipeline->PerDrawDescriptorSets[FrameIndex],
//                 1,
//                 &BoundPerDrawOffset);
//         }
//
//         if(BoundPrimitive == NULL)
//         {
//             continue;
//         }
//
//         vkCmdDrawIndexed(CommandBuffer, BoundPrimitive->IndexCount, 1, 0, 0, 0);
//     }
//
//     Rr_DestroyArenaScratch(Scratch);
// }

bool Rr_BatchGraphicsNode(Rr_App *App, Rr_GraphBatch *Batch, Rr_GraphicsNode *Node)
{
    for(size_t Index = 0; Index < Node->ColorTargetCount; ++Index)
    {
        if(Rr_BatchImagePossible(&Batch->LocalSync, Node->AllocatedColorTargets[Index]->Handle) != true)
        {
            return false;
        }
    }

    if(Node->DepthTarget && Rr_BatchImagePossible(&Batch->LocalSync, Node->AllocatedDepthTarget->Handle) != true)
    {
        return false;
    }

    for(size_t Index = 0; Index < Node->ColorTargetCount; ++Index)
    {
        Rr_BatchImage(
            App,
            Batch,
            Node->AllocatedColorTargets[Index]->Handle,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    if(Node->DepthTarget)
    {
        Rr_BatchImage(
            App,
            Batch,
            Node->AllocatedDepthTarget->Handle,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    return true;
}

void Rr_ExecuteGraphicsNode(Rr_App *App, Rr_GraphicsNode *Node, Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

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
        ImageViews[ColorTarget->Slot] = Node->AllocatedColorTargets[Index]->View;

        Viewport.Width = RR_MIN(Viewport.Width, (int32_t)ColorTarget->Image->Extent.width);
        Viewport.Height = RR_MIN(Viewport.Width, (int32_t)ColorTarget->Image->Extent.height);
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
        ImageViews[DepthTarget->Slot] = Node->AllocatedDepthTarget->View;

        Viewport.Width = RR_MIN(Viewport.Width, (int32_t)DepthTarget->Image->Extent.width);
        Viewport.Height = RR_MIN(Viewport.Width, (int32_t)DepthTarget->Image->Extent.height);
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

    for(Rr_GraphicsNodeFunction *Function = Node->EncodedFirst; Function != NULL; Function = Function->Next)
    {
        switch(Function->Type)
        {
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER:
            {
                Rr_BindBufferArgs *Binding = (Rr_BindBufferArgs *)Function->Args;
                vkCmdBindIndexBuffer(CommandBuffer, Binding->Buffer->Handle, Binding->Offset, VK_INDEX_TYPE_UINT32);
            }
            break;
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER:
            {
                Rr_BindBufferArgs *Binding = (Rr_BindBufferArgs *)Function->Args;
                vkCmdBindVertexBuffers(
                    CommandBuffer,
                    Binding->Slot,
                    1,
                    &Binding->Buffer->Handle,
                    &(VkDeviceSize){ Binding->Offset });
            }
            break;
            case RR_GRAPHICS_NODE_FUNCTION_TYPE_DRAW_INDEXED:
            {
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
                vkCmdBindPipeline(
                    CommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    (*(Rr_GraphicsPipeline **)Function->Args)->Handle);
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
    Rr_GraphicsNode *GraphicsNode = (Rr_GraphicsNode *)&Node->Union.GraphicsNode;   \
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

void Rr_BindVertexBuffer(Rr_GraphNode *Node, Rr_Buffer *Buffer, uint32_t Slot, uint32_t Offset)
{
    RR_GRAPHICS_NODE_ENCODE(RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_VERTEX_BUFFER, Rr_BindIndexBufferArgs) =
        (Rr_BindIndexBufferArgs){
            .Buffer = Buffer,
            .Slot = Slot,
            .Offset = Offset,
        };
}

void Rr_BindIndexBuffer(Rr_GraphNode *Node, Rr_Buffer *Buffer, uint32_t Slot, uint32_t Offset)
{
    RR_GRAPHICS_NODE_ENCODE(RR_GRAPHICS_NODE_FUNCTION_TYPE_BIND_INDEX_BUFFER, Rr_BindIndexBufferArgs) =
        (Rr_BindIndexBufferArgs){
            .Buffer = Buffer,
            .Slot = Slot,
            .Offset = Offset,
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
