#include "Rr_Rendering.h"

#include "Rr_UploadContext.h"
#include "Rr_Mesh.h"
#include "Rr_Material.h"
#include "Rr_Image.h"
#include "Rr_Pipeline.h"
#include "Rr_App.h"

#include <qsort/qsort-inline.h>

#include <SDL3/SDL.h>

#include <limits.h>

typedef struct Rr_GenericRenderingContext Rr_GenericRenderingContext;
struct Rr_GenericRenderingContext
{
    Rr_GenericPipelineSizes Sizes;
    VkDescriptorSet GlobalsDescriptorSet;
    VkDescriptorSet DrawDescriptorSet;
};

typedef struct Rr_TextRenderingContext Rr_TextRenderingContext;
struct Rr_TextRenderingContext
{
    VkDescriptorSet GlobalsDescriptorSet;
    VkDescriptorSet FontDescriptorSet;
};

static Rr_GenericRenderingContext Rr_MakeGenericRenderingContext(
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    Rr_Buffer* GlobalsBuffer,
    Rr_Buffer* DrawBuffer,
    Rr_GenericPipelineSizes* Sizes,
    const byte* GlobalsData,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer* Renderer = &App->Renderer;

    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(RR_MAX_TEXTURES_PER_MATERIAL, 1, Scratch.Arena);

    Rr_GenericRenderingContext Context = {
        .Sizes = *Sizes
    };

    /* Upload globals data. */
    /* @TODO: Make these take a Rr_WriteBuffer instead! */
    Rr_UploadToUniformBuffer(
        App,
        UploadContext,
        GlobalsBuffer,
        GlobalsData,
        Sizes->Globals);

    /* Allocate, write and bind globals descriptor set. */
    Context.GlobalsDescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Renderer->Device,
        Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS]);
    Rr_WriteBufferDescriptor(
        &DescriptorWriter,
        0,
        GlobalsBuffer->Handle,
        Sizes->Globals,
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        Scratch.Arena);
    //    Rr_WriteImageDescriptor(&DescriptorWriter, 1, PocDiffuseImage.View, Renderer->NearestSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, Context.GlobalsDescriptorSet);
    Rr_ResetDescriptorWriter(&DescriptorWriter);

    /* Allocate and write draw descriptor set. */
    Context.DrawDescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Renderer->Device,
        Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW]);
    Rr_WriteBufferDescriptor(
        &DescriptorWriter,
        0,
        DrawBuffer->Handle,
        Sizes->Draw,
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        Scratch.Arena);
    Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, Context.DrawDescriptorSet);
    Rr_ResetDescriptorWriter(&DescriptorWriter);

    Rr_DestroyArenaScratch(Scratch);

    return Context;
}

SDL_FORCE_INLINE int Rr_CompareDrawPrimitive(const Rr_DrawPrimitiveInfo* A, const Rr_DrawPrimitiveInfo* B)
{
    if (A->Material->GenericPipeline != B->Material->GenericPipeline)
    {
        return (A->Material->GenericPipeline > B->Material->GenericPipeline) ? 1 : -1;
    }
    if (A->Material != B->Material)
    {
        return (A->Material > B->Material) ? 1 : -1;
    }
    if (A->Primitive != B->Primitive)
    {
        return (A->Primitive > B->Primitive) ? 1 : -1;
    }
    if (A->OffsetIntoDrawBuffer != B->OffsetIntoDrawBuffer)
    {
        return (A->OffsetIntoDrawBuffer > B->OffsetIntoDrawBuffer) ? 1 : -1;
    }
    return 0;
}

DEF_QSORT(Rr_DrawPrimitiveInfo, Rr_CompareDrawPrimitive) /* NOLINT */

static void Rr_RenderGeneric(
    Rr_Renderer* Renderer,
    Rr_GenericRenderingContext* GenericRenderingContext,
    Rr_DrawPrimitivesSlice DrawPrimitivesSlice,
    VkCommandBuffer CommandBuffer,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(RR_MAX_TEXTURES_PER_MATERIAL, 1, Scratch.Arena);

    Rr_GenericPipeline* BoundPipeline = NULL;
    Rr_Material* BoundMaterial = NULL;
    Rr_Primitive* BoundPrimitive = NULL;
    u32 BoundDrawOffset = UINT_MAX;

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

    for (usize Index = 0; Index < DrawPrimitivesSlice.Length; ++Index)
    {
        Rr_DrawPrimitiveInfo* Info = DrawPrimitivesSlice.Data + Index;

        if (BoundPipeline != Info->Material->GenericPipeline)
        {
            BoundPipeline = Info->Material->GenericPipeline;

            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, BoundPipeline->Handle);
        }

        if (BoundMaterial != Info->Material)
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
                GenericRenderingContext->Sizes.Material,
                0,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                Scratch.Arena);
            for (usize TextureIndex = 0; TextureIndex < RR_MAX_TEXTURES_PER_MATERIAL; ++TextureIndex)
            {
                VkImageView ImageView = VK_NULL_HANDLE;
                if (BoundMaterial->TextureCount > TextureIndex && BoundMaterial->Textures[TextureIndex] != NULL)
                {
                    ImageView = BoundMaterial->Textures[TextureIndex]->View;
                }
                else
                {
                    ImageView = Renderer->NullTexture->View;
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

        if (BoundDrawOffset != Info->OffsetIntoDrawBuffer)
        {
            BoundDrawOffset = Info->OffsetIntoDrawBuffer;

            vkCmdBindDescriptorSets(
                CommandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                Renderer->GenericPipelineLayout,
                RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW,
                1,
                &GenericRenderingContext->DrawDescriptorSet,
                1,
                &BoundDrawOffset);
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
    Rr_App* App,
    Rr_UploadContext* UploadContext,
    VkExtent2D ActiveResolution,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Renderer* Renderer = &App->Renderer;

    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(0, 1, Scratch.Arena);

    Rr_TextRenderingContext TextRenderingContext = { 0 };

    u64 Ticks = SDL_GetTicks();
    float Time = (float)((double)Ticks / 1000.0);
    Rr_TextPipeline* TextPipeline = &Renderer->TextPipeline;
    Rr_TextGlobalsLayout TextGlobalsData = {
        .Reserved = 0.0f,
        .Time = Time,
        .ScreenSize = { (f32)ActiveResolution.width, (f32)ActiveResolution.height },
        .Palette = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };
    Rr_UploadToUniformBuffer(
        App,
        UploadContext,
        TextPipeline->GlobalsBuffers[Renderer->CurrentFrameIndex],
        &TextGlobalsData,
        sizeof(Rr_TextGlobalsLayout));

    TextRenderingContext.GlobalsDescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Renderer->Device,
        TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS]);
    Rr_WriteBufferDescriptor(
        &DescriptorWriter,
        0,
        TextPipeline->GlobalsBuffers[Renderer->CurrentFrameIndex]->Handle,
        sizeof(Rr_TextGlobalsLayout),
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        Scratch.Arena);
    Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, TextRenderingContext.GlobalsDescriptorSet);
    Rr_ResetDescriptorWriter(&DescriptorWriter);

    Rr_DestroyArenaScratch(Scratch);

    return TextRenderingContext;
}

static void Rr_RenderText(
    Rr_Renderer* Renderer,
    Rr_TextRenderingContext* TextRenderingContext,
    Rr_DrawTextsSlice DrawTextSlice,
    VkCommandBuffer CommandBuffer,
    Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    Rr_TextPipeline* TextPipeline = &Renderer->TextPipeline;

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(1, 1, Scratch.Arena);

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TextPipeline->Handle);
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

    usize TextDataOffset = 0;
    usize TextCount = Rr_SliceLength(&DrawTextSlice);
    //    Rr_TextPerInstanceVertexInput* TextData = (Rr_TextPerInstanceVertexInput*)Rr_Malloc(RR_TEXT_BUFFER_SIZE);
    Rr_TextPerInstanceVertexInput* TextData = Rr_ArenaAllocOne(Scratch.Arena, RR_TEXT_BUFFER_SIZE);
    for (usize TextIndex = 0; TextIndex < TextCount; ++TextIndex)
    {
        Rr_DrawTextInfo* DrawTextInfo = &DrawTextSlice.Data[TextIndex];

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
        usize TextLength = DrawTextInfo->String.Length;
        usize FinalTextLength = 0;
        u32 PalleteIndex = 0;
        bool bCodePending = false;
        bool bPalleteIndexPending = false;
        Rr_Vec2 AccumulatedAdvance = { 0 };
        for (usize CharacterIndex = 0; CharacterIndex < TextLength; ++CharacterIndex)
        {
            u32 Unicode = DrawTextInfo->String.Data[CharacterIndex];

            if (bCodePending)
            {
                if (bPalleteIndexPending)
                {
                    if (Unicode >= '0' && Unicode <= '7')
                    {
                        PalleteIndex = Unicode - '0';
                        bPalleteIndexPending = false;
                        bCodePending = false;
                        continue;
                    }
                    else
                    {
                        bPalleteIndexPending = false;
                        bCodePending = false;
                        PalleteIndex = 0;
                    }
                }
                else if (Unicode == 'c')
                {
                    bPalleteIndexPending = true;
                    continue;
                }
                else
                {
                    Unicode = '$';
                    CharacterIndex--;
                    bCodePending = false;
                }
            }
            else if (Unicode == '$')
            {
                bCodePending = true;
                continue;
            }
            Rr_TextPerInstanceVertexInput* Input = &TextData[TextDataOffset + FinalTextLength];
            if (Unicode == '\n')
            {
                AccumulatedAdvance.Y += DrawTextInfo->Font->LineHeight;
                AccumulatedAdvance.X = 0.0f;
                continue;
            }
            else if (Unicode == ' ')
            {
                f32 Advance = DrawTextInfo->Font->Advances[Unicode];
                AccumulatedAdvance.X += Advance;
                continue;
            }
            else
            {
                const u32 GlyphIndexMask = ~0xFFE00000;
                u32 GlyphPack = GlyphIndexMask & Unicode;
                GlyphPack |= PalleteIndex << 28;
                *Input = (Rr_TextPerInstanceVertexInput){
                    .Unicode = GlyphPack,
                    .Advance = AccumulatedAdvance
                };
                FinalTextLength++;
                f32 Advance = DrawTextInfo->Font->Advances[Unicode];
                AccumulatedAdvance.X += Advance;
            }
        }
        vkCmdBindVertexBuffers(
            CommandBuffer,
            1,
            1,
            &TextPipeline->TextBuffers[Renderer->CurrentFrameIndex]->Handle,
            &(VkDeviceSize){ TextDataOffset * sizeof(Rr_TextPerInstanceVertexInput) });
        TextDataOffset += FinalTextLength;
        vkCmdDraw(CommandBuffer, 4, FinalTextLength, 0, 0);
    }
    SDL_memcpy(TextPipeline->TextBuffers[Renderer->CurrentFrameIndex]->AllocationInfo.pMappedData, TextData, TextDataOffset * sizeof(Rr_TextPerInstanceVertexInput));

    Rr_DestroyArenaScratch(Scratch);
}

void Rr_FlushRenderingContext(Rr_DrawContext* DrawContext, Rr_Arena* Arena)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(Arena);

    Rr_App* App = DrawContext->App;
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);

    Rr_DrawTarget* DrawTarget = DrawContext->Info.DrawTarget;
    u32 Width = DrawContext->Info.Viewport.Width;
    u32 Height = DrawContext->Info.Viewport.Height;

    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    Rr_UploadContext UploadContext = {
        .StagingBuffer = Frame->StagingBuffer,
        .TransferCommandBuffer = CommandBuffer
    };

    Rr_GenericRenderingContext GenericRenderingContext = Rr_MakeGenericRenderingContext(
        App,
        &UploadContext,
        Frame->CommonBuffer.Buffer,
        Frame->DrawBuffer.Buffer,
        &DrawContext->Info.Sizes,
        DrawContext->GlobalsData,
        Scratch.Arena);
    Rr_TextRenderingContext TextRenderingContext = Rr_MakeTextRenderingContext(App,
        &UploadContext,
        (VkExtent2D){ .width = Width, .height = Height },
        Scratch.Arena);

    /* Begin render pass. */
    VkClearValue ClearColorValues[2] = {
        (VkClearValue){ 0 }, (VkClearValue){ .depthStencil.depth = 1.0f }
    };
    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = DrawTarget->Framebuffer,
        .renderArea = (VkRect2D){ { 0, 0 }, { Width, Height } },
        .renderPass = Renderer->RenderPass,
        .clearValueCount = 2,
        .pClearValues = ClearColorValues
    };
    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    /* Set dynamic states. */
    VkViewport Viewport = { 0 };
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = (float)Width;
    Viewport.height = (float)Height;
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;
    vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

    VkRect2D Scissor = { 0 };
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = Width;
    Scissor.extent.height = Height;
    vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

    Rr_RenderGeneric(
        Renderer,
        &GenericRenderingContext,
        DrawContext->DrawPrimitivesSlice,
        CommandBuffer,
        Scratch.Arena);
    Rr_RenderText(
        Renderer,
        &TextRenderingContext,
        DrawContext->DrawTextsSlice,
        CommandBuffer,
        Scratch.Arena);

    vkCmdEndRenderPass(CommandBuffer);

    Rr_DestroyArenaScratch(Scratch);
}
