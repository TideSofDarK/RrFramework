#include "Rr_BuiltinNode.h"

#include "Rr_App.h"
#include "Rr_Graph.h"
#include "Rr_Memory.h"
#include "Rr_Renderer.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_timer.h>

Rr_GraphNode *Rr_AddBuiltinNode(Rr_App *App, const char *Name, Rr_GraphNode **Dependencies, size_t DependencyCount)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_GraphNode *GraphNode = Rr_AddGraphNode(Frame, RR_GRAPH_NODE_TYPE_BUILTIN, Name, Dependencies, DependencyCount);

    Rr_BuiltinNode *BuiltinNode = &GraphNode->Union.BuiltinNode;
    RR_ZERO_PTR(BuiltinNode);

    return GraphNode;
}

static void Rr_DrawText(Rr_App *App, Rr_BuiltinNode *Node, Rr_DrawTextInfo *Info)
{
    Rr_Frame *Frame = Rr_GetCurrentFrame(&App->Renderer);
    Rr_DrawTextInfo *NewInfo = RR_SLICE_PUSH(&Node->DrawTextsSlice, &Frame->Arena);
    *NewInfo = *Info;
    if(NewInfo->Font == NULL)
    {
        NewInfo->Font = App->Renderer.BuiltinFont;
    }
    if(NewInfo->Size == 0.0f)
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

void Rr_DrawDefaultText(Rr_App *App, Rr_GraphNode *Node, Rr_String *String, Rr_Vec2 Position)
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

static Rr_TextRenderingContext Rr_MakeTextRenderingContext(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    VkExtent2D ActiveResolution,
    Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;

    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    Rr_WriteBuffer *CommonBuffer = &Frame->CommonBuffer;

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(0, 1, Scratch.Arena);

    Rr_TextRenderingContext TextRenderingContext = { 0 };

    uint64_t Ticks = SDL_GetTicks();
    float Time = (float)Ticks / 1000.0f;
    Rr_TextPipeline *TextPipeline = &Renderer->TextPipeline;
    Rr_TextGlobalsLayout TextGlobalsData = { .Reserved = 0.0f,
                                             .Time = Time,
                                             .ScreenSize = { (float)ActiveResolution.width,
                                                             (float)ActiveResolution.height },
                                             .Palette = { { 1.0f, 1.0f, 1.0f, 1.0f },
                                                          { 1.0f, 0.0f, 0.0f, 1.0f },
                                                          { 0.0f, 1.0f, 0.0f, 1.0f },
                                                          { 0.0f, 0.0f, 1.0f, 1.0f } } };

    VkDeviceSize BufferOffset = CommonBuffer->Offset;
    Rr_UploadToUniformBuffer(
        App,
        UploadContext,
        CommonBuffer->Buffer,
        &CommonBuffer->Offset,
        RR_MAKE_DATA(TextGlobalsData));

    TextRenderingContext.GlobalsDescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Renderer->Device,
        TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS]);
    Rr_WriteBufferDescriptor(
        &DescriptorWriter,
        0,
        CommonBuffer->Buffer->Handle,
        sizeof(Rr_TextGlobalsLayout),
        BufferOffset,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        Scratch.Arena);
    Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, TextRenderingContext.GlobalsDescriptorSet);
    Rr_ResetDescriptorWriter(&DescriptorWriter);

    Rr_DestroyArenaScratch(Scratch);

    return TextRenderingContext;
}

static void Rr_RenderText(
    Rr_App *App,
    Rr_TextRenderingContext *TextRenderingContext,
    Rr_DrawTextsSlice DrawTextSlice,
    VkCommandBuffer CommandBuffer,
    Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;

    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);
    Rr_TextPipeline *TextPipeline = &Renderer->TextPipeline;

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(1, 1, Scratch.Arena);

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TextPipeline->Pipeline->Handle);
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &TextPipeline->QuadBuffer->Handle, &(VkDeviceSize){ 0 });
    vkCmdBindDescriptorSets(
        CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        TextPipeline->Layout,
        RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS,
        1,
        &TextRenderingContext->GlobalsDescriptorSet,
        0,
        NULL);

    size_t TextDataOffset = 0;
    size_t TextCount = DrawTextSlice.Count;
    Rr_TextPerInstanceVertexInput *TextData = RR_ARENA_ALLOC_ONE(Scratch.Arena, RR_TEXT_BUFFER_SIZE);
    for(size_t TextIndex = 0; TextIndex < TextCount; ++TextIndex)
    {
        Rr_DrawTextInfo *DrawTextInfo = &DrawTextSlice.Data[TextIndex];

        VkDescriptorSet TextFontDescriptorSet = Rr_AllocateDescriptorSet(
            &Frame->DescriptorAllocator,
            Renderer->Device,
            TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT]);
        Rr_WriteBufferDescriptor(
            &DescriptorWriter,
            0,
            DrawTextInfo->Font->Buffer->Handle,
            sizeof(Rr_TextFontLayout),
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            Scratch.Arena);
        Rr_WriteImageDescriptor(
            &DescriptorWriter,
            1,
            DrawTextInfo->Font->Atlas->View,
            Renderer->LinearSampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            Scratch.Arena);
        Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, TextFontDescriptorSet);
        Rr_ResetDescriptorWriter(&DescriptorWriter);

        vkCmdBindDescriptorSets(
            CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            TextPipeline->Layout,
            RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT,
            1,
            &TextFontDescriptorSet,
            0,
            NULL);

        Rr_TextPushConstants TextPushConstants;
        TextPushConstants.PositionScreenSpace = DrawTextInfo->Position;
        TextPushConstants.Size = DrawTextInfo->Size;
        TextPushConstants.Flags = DrawTextInfo->Flags;
        vkCmdPushConstants(
            CommandBuffer,
            TextPipeline->Layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            128,
            &TextPushConstants);
        /* Upload and bind text data. */
        VkDeviceSize BaseTextDataOffset = Frame->PerDrawBuffer.Offset;
        size_t TextLength = DrawTextInfo->String.Length;
        size_t FinalTextLength = 0;
        uint32_t PalleteIndex = 0;
        Rr_Bool CodePending = RR_FALSE;
        Rr_Bool PalleteIndexPending = RR_FALSE;
        Rr_Vec2 AccumulatedAdvance = { 0 };
        for(size_t CharacterIndex = 0; CharacterIndex < TextLength; ++CharacterIndex)
        {
            uint32_t Unicode = DrawTextInfo->String.Data[CharacterIndex];

            if(CodePending)
            {
                if(PalleteIndexPending)
                {
                    if(Unicode >= '0' && Unicode <= '7')
                    {
                        PalleteIndex = Unicode - '0';
                        PalleteIndexPending = RR_FALSE;
                        CodePending = RR_FALSE;
                        continue;
                    }
                    else
                    {
                        PalleteIndexPending = RR_FALSE;
                        CodePending = RR_FALSE;
                        PalleteIndex = 0;
                    }
                }
                else if(Unicode == 'c')
                {
                    PalleteIndexPending = RR_TRUE;
                    continue;
                }
                else
                {
                    Unicode = '$';
                    CharacterIndex--;
                    CodePending = RR_FALSE;
                }
            }
            else if(Unicode == '$')
            {
                CodePending = RR_TRUE;
                continue;
            }
            Rr_TextPerInstanceVertexInput *Input = &TextData[TextDataOffset + FinalTextLength];
            if(Unicode == '\n')
            {
                AccumulatedAdvance.Y += DrawTextInfo->Font->LineHeight;
                AccumulatedAdvance.X = 0.0f;
                continue;
            }
            else if(Unicode == ' ')
            {
                float Advance = DrawTextInfo->Font->Advances[Unicode];
                AccumulatedAdvance.X += Advance;
                continue;
            }
            else
            {
                uint32_t GlyphIndexMask = ~0xFFE00000;
                uint32_t GlyphPack = GlyphIndexMask & Unicode;
                GlyphPack |= PalleteIndex << 28;
                *Input = (Rr_TextPerInstanceVertexInput){
                    .Unicode = GlyphPack,
                    .Advance = AccumulatedAdvance,
                };
                FinalTextLength++;
                float Advance = DrawTextInfo->Font->Advances[Unicode];
                AccumulatedAdvance.X += Advance;
            }
        }
        vkCmdBindVertexBuffers(
            CommandBuffer,
            1,
            1,
            &Frame->PerDrawBuffer.Buffer->Handle,
            &(VkDeviceSize){ BaseTextDataOffset + TextDataOffset * sizeof(Rr_TextPerInstanceVertexInput) });
        TextDataOffset += FinalTextLength;
        vkCmdDraw(CommandBuffer, 4, FinalTextLength, 0, 0);
    }
    /* @TODO: Probably not the best choice of buffer. */
    Rr_CopyToMappedUniformBuffer(
        App,
        Frame->PerDrawBuffer.Buffer,
        &Frame->PerDrawBuffer.Offset,
        (Rr_Data){ .Ptr = TextData, .Size = TextDataOffset * sizeof(Rr_TextPerInstanceVertexInput) });

    Rr_DestroyArenaScratch(Scratch);
}

Rr_Bool Rr_BatchBuiltinNode(Rr_App *App, Rr_Graph *Graph, Rr_GraphBatch *Batch, Rr_BuiltinNode *Node)
{
    Rr_DrawTarget *DrawTarget = App->Renderer.DrawTarget;

    if(Rr_SyncImage(
           App,
           Graph,
           Batch,
           DrawTarget->Frames[App->Renderer.CurrentFrameIndex].ColorImage->Handle,
           VK_IMAGE_ASPECT_COLOR_BIT,
           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
           VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) != RR_TRUE ||
       Rr_SyncImage(
           App,
           Graph,
           Batch,
           DrawTarget->Frames[App->Renderer.CurrentFrameIndex].DepthImage->Handle,
           VK_IMAGE_ASPECT_DEPTH_BIT,
           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) != RR_TRUE)
    {
        return RR_FALSE;
    }

    return RR_TRUE;
}

void Rr_ExecuteBuiltinNode(Rr_App *App, Rr_BuiltinNode *Node, Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_DrawTarget *DrawTarget = Renderer->DrawTarget;

    Rr_IntVec4 Viewport = {
        0,
        0,
        (int32_t)DrawTarget->Frames[App->Renderer.CurrentFrameIndex].ColorImage->Extent.width,
        (int32_t)DrawTarget->Frames[App->Renderer.CurrentFrameIndex].ColorImage->Extent.height,
    };

    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    Rr_UploadContext UploadContext = {
        .StagingBuffer = &Frame->StagingBuffer,
        .TransferCommandBuffer = CommandBuffer,
    };

    Rr_TextRenderingContext TextRenderingContext = Rr_MakeTextRenderingContext(
        App,
        &UploadContext,
        (VkExtent2D){ .width = Viewport.Width, .height = Viewport.Height },
        Scratch.Arena);

    /* Line up appropriate clear values. */

    VkClearValue ClearValues[] = {
        (VkClearValue){ 0 },
        (VkClearValue){ .depthStencil.depth = 1.0f },
    };

    /* Begin render pass. */

    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = DrawTarget->Frames[App->Renderer.CurrentFrameIndex].Framebuffer,
        .renderArea = (VkRect2D){ { Viewport.X, Viewport.Y }, { Viewport.Z, Viewport.W } },
        .renderPass = Renderer->RenderPasses.ColorDepthLoad,
        .clearValueCount = RR_ARRAY_COUNT(ClearValues),
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

    Rr_RenderText(App, &TextRenderingContext, Node->DrawTextsSlice, CommandBuffer, Scratch.Arena);

    vkCmdEndRenderPass(CommandBuffer);

    Rr_DestroyArenaScratch(Scratch);
}
