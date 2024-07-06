#include "Rr_Rendering.h"

#include "Rr_App.h"
#include "Rr_Draw.h"
#include "Rr_Image.h"
#include "Rr_Material.h"
#include "Rr_Mesh.h"
#include "Rr_Pipeline.h"
#include "Rr_UploadContext.h"

#include <qsort/qsort-inline.h>

#include <SDL3/SDL.h>

#include <limits.h>

typedef struct Rr_GenericRenderingContext Rr_GenericRenderingContext;
struct Rr_GenericRenderingContext
{
    Rr_GenericPipeline *BasePipeline;
    Rr_GenericPipeline *OverridePipeline;
    VkDescriptorSet GlobalsDescriptorSet;
};

typedef struct Rr_TextRenderingContext Rr_TextRenderingContext;
struct Rr_TextRenderingContext
{
    VkDescriptorSet GlobalsDescriptorSet;
    VkDescriptorSet FontDescriptorSet;
};

static Rr_GenericRenderingContext Rr_MakeGenericRenderingContext(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_DrawContextInfo *DrawContextInfo,
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
        .BasePipeline = DrawContextInfo->BasePipeline,
        .OverridePipeline = DrawContextInfo->OverridePipeline,
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
         * however note that each draw size needs its own descriptor set.*/
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

    Rr_DescriptorWriter DescriptorWriter =
        Rr_CreateDescriptorWriter(0, 1, Scratch.Arena);

    Rr_TextRenderingContext TextRenderingContext = { 0 };

    uint64_t Ticks = SDL_GetTicks();
    float Time = (float)Ticks / 1000.0f;
    Rr_TextPipeline *TextPipeline = &Renderer->TextPipeline;
    Rr_TextGlobalsLayout TextGlobalsData = {
        .Reserved = 0.0f,
        .Time = Time,
        .ScreenSize = { (float)ActiveResolution.width,
                        (float)ActiveResolution.height },
        .Palette = { { 1.0f, 1.0f, 1.0f, 1.0f },
                     { 1.0f, 0.0f, 0.0f, 1.0f },
                     { 0.0f, 1.0f, 0.0f, 1.0f },
                     { 0.0f, 0.0f, 1.0f, 1.0f } }
    };

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
        TextPipeline
            ->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS]);
    Rr_WriteBufferDescriptor(
        &DescriptorWriter,
        0,
        CommonBuffer->Buffer->Handle,
        sizeof(Rr_TextGlobalsLayout),
        BufferOffset,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        Scratch.Arena);
    Rr_UpdateDescriptorSet(
        &DescriptorWriter,
        Renderer->Device,
        TextRenderingContext.GlobalsDescriptorSet);
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

    Rr_DescriptorWriter DescriptorWriter =
        Rr_CreateDescriptorWriter(1, 1, Scratch.Arena);

    vkCmdBindPipeline(
        CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        TextPipeline->Pipeline->Handle);
    vkCmdBindVertexBuffers(
        CommandBuffer,
        0,
        1,
        &TextPipeline->QuadBuffer->Handle,
        &(VkDeviceSize){ 0 });
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
    size_t TextCount = RR_SLICE_LENGTH(&DrawTextSlice);
    Rr_TextPerInstanceVertexInput *TextData =
        RR_ARENA_ALLOC_ONE(Scratch.Arena, RR_TEXT_BUFFER_SIZE);
    for (size_t TextIndex = 0; TextIndex < TextCount; ++TextIndex)
    {
        Rr_DrawTextInfo *DrawTextInfo = &DrawTextSlice.Data[TextIndex];

        VkDescriptorSet TextFontDescriptorSet = Rr_AllocateDescriptorSet(
            &Frame->DescriptorAllocator,
            Renderer->Device,
            TextPipeline
                ->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT]);
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
        Rr_UpdateDescriptorSet(
            &DescriptorWriter,
            Renderer->Device,
            TextFontDescriptorSet);
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
        for (size_t CharacterIndex = 0; CharacterIndex < TextLength;
             ++CharacterIndex)
        {
            uint32_t Unicode = DrawTextInfo->String.Data[CharacterIndex];

            if (CodePending)
            {
                if (PalleteIndexPending)
                {
                    if (Unicode >= '0' && Unicode <= '7')
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
                else if (Unicode == 'c')
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
            else if (Unicode == '$')
            {
                CodePending = RR_TRUE;
                continue;
            }
            Rr_TextPerInstanceVertexInput *Input =
                &TextData[TextDataOffset + FinalTextLength];
            if (Unicode == '\n')
            {
                AccumulatedAdvance.Y += DrawTextInfo->Font->LineHeight;
                AccumulatedAdvance.X = 0.0f;
                continue;
            }
            else if (Unicode == ' ')
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
                *Input =
                    (Rr_TextPerInstanceVertexInput){ .Unicode = GlyphPack,
                                                     .Advance =
                                                         AccumulatedAdvance };
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
            &(VkDeviceSize){ BaseTextDataOffset +
                             TextDataOffset *
                                 sizeof(Rr_TextPerInstanceVertexInput) });
        TextDataOffset += FinalTextLength;
        vkCmdDraw(CommandBuffer, 4, FinalTextLength, 0, 0);
    }
    /* @TODO: Probably not the best choice of buffer. */
    Rr_CopyToMappedUniformBuffer(
        App,
        Frame->PerDrawBuffer.Buffer,
        &Frame->PerDrawBuffer.Offset,
        (Rr_Data){ .Ptr = TextData,
                   .Size = TextDataOffset *
                           sizeof(Rr_TextPerInstanceVertexInput) });

    Rr_DestroyArenaScratch(Scratch);
}

void Rr_FlushDrawContext(Rr_DrawContext *DrawContext, Rr_Arena *Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_App *App = DrawContext->App;
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_Frame *Frame = Rr_GetCurrentFrame(Renderer);

    Rr_DrawTarget *DrawTarget = DrawContext->Info.DrawTarget;

    Rr_IntVec4 Viewport = DrawContext->Info.Viewport;

    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    Rr_UploadContext UploadContext = {
        .StagingBuffer = &Frame->StagingBuffer,
        .TransferCommandBuffer = CommandBuffer,
    };

    Rr_GenericRenderingContext GenericRenderingContext =
        Rr_MakeGenericRenderingContext(
            App,
            &UploadContext,
            &DrawContext->Info,
            DrawContext->GlobalsData,
            Scratch.Arena);

    Rr_TextRenderingContext TextRenderingContext;
    if (DrawContext->Info.EnableTextRendering)
    {
        TextRenderingContext = Rr_MakeTextRenderingContext(
            App,
            &UploadContext,
            (VkExtent2D){ .width = Viewport.Width, .height = Viewport.Height },
            Scratch.Arena);
    }

    /* Line up appropriate clear values. */

    uint32_t ColorAttachmentCount =
        DrawContext->Info.BasePipeline->Pipeline->ColorAttachmentCount;
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
        .framebuffer = DrawTarget->Framebuffer,
        .renderArea = (VkRect2D){ { Viewport.X, Viewport.Y },
                                  { Viewport.Z, Viewport.W } },
        .renderPass = DrawContext->Info.BasePipeline->Pipeline->RenderPass,
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
        DrawContext->DrawPrimitivesSlice,
        CommandBuffer,
        Scratch.Arena);

    if (DrawContext->Info.EnableTextRendering)
    {
        Rr_RenderText(
            App,
            &TextRenderingContext,
            DrawContext->DrawTextsSlice,
            CommandBuffer,
            Scratch.Arena);
    }

    vkCmdEndRenderPass(CommandBuffer);

    Rr_DestroyArenaScratch(Scratch);
}
