#include "Rr_Renderer.h"
#include "Rr_Text.h"

#include <math.h>
#include <string.h>
#include <stdint.h>

#include <cglm/ivec2.h>
#include <cglm/mat4.h>
#include <cglm/cam.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_stdinc.h>

#include "Rr_Defines.h"
#include "Rr_App.h"
#include "Rr_Array.h"
#include "Rr_Vulkan.h"
#include "Rr_Descriptor.h"
#include "Rr_Image.h"
#include "Rr_Helpers.h"
#include "Rr_Buffer.h"
#include "Rr_Mesh.h"
#include "Rr_Pipeline.h"
#include "Rr_Memory.h"
#include "Rr_Util.h"
#include "Rr_Material.h"

static void Rr_BlitPrerenderedDepth(const VkCommandBuffer CommandBuffer, const VkImage Source, const VkImage Destination, const VkExtent2D SrcSize, const VkExtent2D DstSize)
{
    VkImageBlit2 BlitRegion = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = NULL,
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets = { { 0 }, { (i32)SrcSize.width, (i32)SrcSize.height, 1 } },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets = { { 0 }, { (i32)DstSize.width, (i32)DstSize.height, 1 } },
    };

    const VkBlitImageInfo2 BlitInfo = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = NULL,
        .srcImage = Source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = Destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &BlitRegion,
        .filter = VK_FILTER_NEAREST,
    };

    vkCmdBlitImage2(CommandBuffer, &BlitInfo);
}

static void Rr_BlitColorImage(const VkCommandBuffer CommandBuffer, const VkImage Source, const VkImage Destination, const VkExtent2D SrcSize, const VkExtent2D DstSize)
{
    VkImageBlit2 BlitRegion = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = NULL,
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcOffsets = { { 0 }, { (i32)SrcSize.width, (i32)SrcSize.height, 1 } },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .dstOffsets = { { 0 }, { (i32)DstSize.width, (i32)DstSize.height, 1 } },
    };

    const VkBlitImageInfo2 BlitInfo = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = NULL,
        .srcImage = Source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = Destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &BlitRegion,
        .filter = VK_FILTER_NEAREST,
    };

    vkCmdBlitImage2(CommandBuffer, &BlitInfo);
}

Rr_RenderingContext Rr_BeginRendering(Rr_Renderer* const Renderer, Rr_BeginRenderingInfo* const Info)
{
    Rr_RenderingContext RenderingContext = { 0 };
    if (Info->Pipeline == NULL)
    {
        abort();
    }

    const Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    RenderingContext.CommandBuffer = Frame->MainCommandBuffer;
    RenderingContext.Renderer = Renderer;
    RenderingContext.Info = Info;
    Rr_ArrayInit(RenderingContext.DrawMeshArray, Rr_DrawMeshInfo, 16);
    Rr_ArrayInit(RenderingContext.DrawTextArray, Rr_DrawTextInfo, 16);

    return RenderingContext;
}

void Rr_DrawMesh(Rr_RenderingContext* RenderingContext, const Rr_DrawMeshInfo* Info)
{
    Rr_ArrayPush(RenderingContext->DrawMeshArray, Info);
}

void Rr_DrawText(Rr_RenderingContext* RenderingContext, const Rr_DrawTextInfo* Info)
{
    Rr_ArrayPush(RenderingContext->DrawTextArray, Info);
    Rr_DrawTextInfo* NewInfo = &RenderingContext->DrawTextArray[Rr_ArrayCount(RenderingContext->DrawTextArray) - 1];
    if (NewInfo->Font == NULL)
    {
        NewInfo->Font = &RenderingContext->Renderer->BuiltinFont;
    }
    if (NewInfo->Size == RR_TEXT_DEFAULT_SIZE)
    {
        NewInfo->Size = NewInfo->Font->DefaultSize;
    }
}

void Rr_EndRendering(Rr_RenderingContext* RenderingContext)
{
    Rr_Renderer* Renderer = RenderingContext->Renderer;
    size_t CurrentFrameIndex = Rr_GetCurrentFrameIndex(Renderer);
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;
    VkExtent2D ActiveResolution = Renderer->ActiveResolution;

    Rr_GenericPipeline* Pipeline = RenderingContext->Info->Pipeline;
    Rr_GenericPipelineBuffers* PipelineBuffers = &Pipeline->Buffers[CurrentFrameIndex];

    Rr_BeginRenderingInfo* Info = RenderingContext->Info;

    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(RR_MAX_TEXTURES_PER_MATERIAL, 1);

    const u32 Alignment = Renderer->PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;

    /* Upload globals data. */
    Rr_CopyToDeviceUniformBuffer(
        Renderer,
        CommandBuffer,
        &Frame->StagingBuffer,
        &PipelineBuffers->Globals,
        RenderingContext->Info->GlobalsData,
        Pipeline->GlobalsSize);

    /* Allocate, write and bind globals descriptor set. */
    VkDescriptorSet GlobalsDescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Renderer->Device,
        Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS]);
    Rr_WriteBufferDescriptor(
        &DescriptorWriter,
        0,
        PipelineBuffers->Globals.Handle,
        Pipeline->GlobalsSize,
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    //    Rr_WriteImageDescriptor(&DescriptorWriter, 1, PocDiffuseImage.View, Renderer->NearestSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, GlobalsDescriptorSet);
    Rr_ResetDescriptorWriter(&DescriptorWriter);

    vkCmdBindDescriptorSets(
        CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        Renderer->GenericPipelineLayout,
        0,
        1,
        &GlobalsDescriptorSet,
        0,
        NULL);

    /* Generate draw lists. */
    size_t DrawMeshCount = Rr_ArrayCount(RenderingContext->DrawMeshArray);
    size_t DrawSize = Pipeline->DrawSize;
    size_t DrawSizeAligned = Rr_Align(DrawSize, Alignment);
    void* DrawData = Rr_Malloc(DrawSizeAligned * DrawMeshCount);

    Rr_Material** MaterialsArray;
    Rr_ArrayInit(MaterialsArray, Rr_Material*, DrawMeshCount);

    size_t** DrawMeshIndicesArrays;
    Rr_ArrayInit(DrawMeshIndicesArrays, size_t*, DrawMeshCount);

    size_t CurrentDrawMaterialIndex;
    for (size_t DrawMeshIndex = 0; DrawMeshIndex < DrawMeshCount; ++DrawMeshIndex)
    {
        Rr_DrawMeshInfo* DrawMeshInfo = &RenderingContext->DrawMeshArray[DrawMeshIndex];

        SDL_memcpy(DrawData + (DrawMeshIndex * DrawSizeAligned), DrawMeshInfo->DrawData, DrawSize);

        bool bMaterialFound = false;
        for (size_t MaterialIndex = 0; MaterialIndex < Rr_ArrayCount(MaterialsArray); ++MaterialIndex)
        {
            if (MaterialsArray[MaterialIndex] == DrawMeshInfo->Material)
            {
                bMaterialFound = true;
                CurrentDrawMaterialIndex = MaterialIndex;
                break;
            }
        }
        if (!bMaterialFound)
        {
            Rr_ArrayPush(MaterialsArray, &DrawMeshInfo->Material);
            size_t MaterialIndex = Rr_ArrayCount(MaterialsArray) - 1;
            CurrentDrawMaterialIndex = MaterialIndex;
        }
        if (CurrentDrawMaterialIndex >= Rr_ArrayCount(DrawMeshIndicesArrays))
        {
            size_t* DrawMeshIndicesArray;
            Rr_ArrayInit(DrawMeshIndicesArray, size_t, 2);
            Rr_ArrayPush(DrawMeshIndicesArrays, &DrawMeshIndicesArray);
        }
        size_t* DrawMeshIndicesArray = DrawMeshIndicesArrays[CurrentDrawMaterialIndex];
        Rr_ArrayPush(DrawMeshIndicesArray, &DrawMeshIndex);
    }

    /* Upload per-draw-call data. */
    size_t DrawOffset = 0;
    Rr_CopyToMappedUniformBuffer(
        Renderer,
        &PipelineBuffers->Draw,
        DrawData,
        DrawSizeAligned * DrawMeshCount,
        &DrawOffset);

    /* Allocate and write draw descriptor set. */
    VkDescriptorSet DrawDescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Renderer->Device,
        Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW]);
    Rr_WriteBufferDescriptor(
        &DescriptorWriter,
        0,
        PipelineBuffers->Draw.Handle,
        Pipeline->DrawSize,
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
    Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, DrawDescriptorSet);
    Rr_ResetDescriptorWriter(&DescriptorWriter);

    /* Upload Text Rendering Globals */
    u64 Ticks = SDL_GetTicks();
    float Time = (float)((double)Ticks / 1000.0);
    Rr_TextPipeline* TextPipeline = &Renderer->TextPipeline;
    Rr_TextGlobalsLayout TextGlobalsData = {
        .Reserved = 0.0f,
        .Time = Time,
        .ScreenSize = { (f32)ActiveResolution.width, (f32)ActiveResolution.height },
        .Pallete = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };
    Rr_CopyToDeviceUniformBuffer(
        Renderer,
        CommandBuffer,
        &Frame->StagingBuffer,
        &TextPipeline->GlobalsBuffers[CurrentFrameIndex],
        &TextGlobalsData,
        sizeof(Rr_TextGlobalsLayout));

    /* Render Loop */
    // Rr_ImageBarrier ColorImageTransition = {
    //     .CommandBuffer = CommandBuffer,
    //     .Image = Renderer->DrawTarget.ColorImage.Handle,
    //     .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
    //     .AccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
    //     .StageMask = VK_PIPELINE_STAGE_2_BLIT_BIT
    // };
    // if (Info->InitialColor != NULL)
    // {
    //     Rr_ChainImageBarrier_Aspect(&ColorImageTransition,
    //         VK_PIPELINE_STAGE_2_BLIT_BIT,
    //         VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,
    //         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //         VK_IMAGE_ASPECT_COLOR_BIT);
    //     Rr_BlitColorImage(CommandBuffer, Info->InitialColor->Handle, Renderer->DrawTarget.ColorImage.Handle,
    //         (VkExtent2D){ .width = Info->InitialColor->Extent.width, .height = Info->InitialColor->Extent.height },
    //         (VkExtent2D){ .width = ActiveResolution.width, .height = ActiveResolution.height });
    // }
    // Rr_ChainImageBarrier_Aspect(&ColorImageTransition,
    //     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    //     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
    //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //     VK_IMAGE_ASPECT_COLOR_BIT);
    //
    // Rr_ImageBarrier DepthImageTransition = {
    //     .CommandBuffer = CommandBuffer,
    //     .Image = Renderer->DrawTarget.DepthImage.Handle,
    //     .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
    //     .AccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    //     .StageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
    // };
    // if (Info->InitialDepth != NULL)
    // {
    //     Rr_ChainImageBarrier_Aspect(&DepthImageTransition,
    //         VK_PIPELINE_STAGE_2_BLIT_BIT,
    //         VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,
    //         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //         VK_IMAGE_ASPECT_DEPTH_BIT);
    //     Rr_BlitPrerenderedDepth(CommandBuffer, Info->InitialDepth->Handle, Renderer->DrawTarget.DepthImage.Handle,
    //         (VkExtent2D){ .width = Info->InitialDepth->Extent.width, .height = Info->InitialDepth->Extent.height },
    //         (VkExtent2D){ .width = ActiveResolution.width, .height = ActiveResolution.height });
    // }
    // Rr_ChainImageBarrier_Aspect(&DepthImageTransition,
    //     VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
    //     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    //     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    //     VK_IMAGE_ASPECT_DEPTH_BIT);

    // VkClearValue ClearColorValue = {
    //     0
    // };
    // VkRenderingAttachmentInfo ColorAttachments[2] = { GetRenderingAttachmentInfo_Color(
    //     Renderer->DrawTarget.ColorImage.View,
    //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //     Info->InitialColor == NULL ? &ClearColorValue : NULL) };
    // if (Info->AdditionalAttachment != NULL)
    // {
    //     ColorAttachments[1] = GetRenderingAttachmentInfo_Color(
    //         Info->AdditionalAttachment->View,
    //         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //         &ClearColorValue);
    // }
    //
    // VkRenderingAttachmentInfo DepthAttachment = GetRenderingAttachmentInfo_Depth(
    //     Renderer->DrawTarget.DepthImage.View,
    //     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    //     Info->InitialDepth == NULL);

    // VkRenderingInfo RenderingInfo = {
    //     .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    //     .pNext = NULL,
    //     .renderArea = (VkRect2D){ .offset = (VkOffset2D){ 0, 0 }, .extent = ActiveResolution },
    //     .layerCount = 1,
    //     .colorAttachmentCount = Info->AdditionalAttachment != NULL ? 2 : 1,
    //     .pColorAttachments = ColorAttachments,
    //     .pDepthAttachment = &DepthAttachment,
    //     .pStencilAttachment = NULL,
    // };

    // vkCmdBeginRendering(CommandBuffer, &RenderingInfo);

    VkClearValue ClearColorValues[2] = {
        (VkClearValue){}, (VkClearValue){ .depthStencil.depth = 1.0f }
    };
    VkRenderPassBeginInfo RenderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .framebuffer = Renderer->DrawTargets[CurrentFrameIndex].Framebuffer,
        .renderArea = (VkRect2D){ 0, 0, ActiveResolution.width, ActiveResolution.height },
        .renderPass = Renderer->RenderPass,
        .clearValueCount = 2,
        .pClearValues = ClearColorValues
    };

    vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport Viewport = { 0 };
    Viewport.x = 0.0f;
    Viewport.y = 0.0f;
    Viewport.width = (float)ActiveResolution.width;
    Viewport.height = (float)ActiveResolution.height;
    Viewport.minDepth = 0.0f;
    Viewport.maxDepth = 1.0f;

    vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

    VkRect2D Scissor = { 0 };
    Scissor.offset.x = 0;
    Scissor.offset.y = 0;
    Scissor.extent.width = ActiveResolution.width;
    Scissor.extent.height = ActiveResolution.height;

    vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

    /* Generic Rendering */
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline->Handle);

    for (size_t MaterialIndex = 0; MaterialIndex < Rr_ArrayCount(MaterialsArray); ++MaterialIndex)
    {
        Rr_Material* Material = MaterialsArray[MaterialIndex];

        VkDescriptorSet MaterialDescriptorSet = Rr_AllocateDescriptorSet(
            &Frame->DescriptorAllocator,
            Renderer->Device,
            Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL]);
        Rr_WriteBufferDescriptor(
            &DescriptorWriter,
            0,
            Material->Buffer.Handle,
            Pipeline->MaterialSize,
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        for (size_t TextureIndex = 0; TextureIndex < Material->TextureCount; ++TextureIndex)
        {
            Rr_WriteImageDescriptorAt(
                &DescriptorWriter,
                1,
                TextureIndex,
                Material->Textures[TextureIndex]->View,
                Renderer->NearestSampler,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }
        Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, MaterialDescriptorSet);
        Rr_ResetDescriptorWriter(&DescriptorWriter);

        vkCmdBindDescriptorSets(
            CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            Renderer->GenericPipelineLayout,
            1,
            1,
            &MaterialDescriptorSet,
            0,
            NULL);

        size_t* Indices = DrawMeshIndicesArrays[MaterialIndex];
        size_t IndexCount = Rr_ArrayCount(Indices);
        for (size_t DrawIndex = 0; DrawIndex < IndexCount; ++DrawIndex)
        {
            size_t DrawMeshIndex = Indices[DrawIndex];
            Rr_DrawMeshInfo* DrawMeshInfo = &RenderingContext->DrawMeshArray[DrawMeshIndex];

            u32 Offset = DrawMeshIndex * DrawSizeAligned;
            vkCmdBindDescriptorSets(
                CommandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                Renderer->GenericPipelineLayout,
                2,
                1,
                &DrawDescriptorSet,
                1,
                &Offset);
            //            vkCmdPushConstants(
            //                CommandBuffer,
            //                Pipeline->Layout,
            //                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            //                0,
            //                128,
            //                &(SUber3DPushConstants){ 0 });
            vkCmdBindIndexBuffer(CommandBuffer, DrawMeshInfo->MeshBuffers->IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(CommandBuffer, DrawMeshInfo->MeshBuffers->IndexCount, 1, 0, 0, 0);
        }
        Rr_ArrayFree(Indices);
    }

    /* Text Rendering */
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TextPipeline->Handle);
    vkCmdBindVertexBuffers(
        CommandBuffer,
        0,
        1,
        &TextPipeline->QuadBuffer.Handle,
        &(VkDeviceSize){ 0 });
    VkDescriptorSet TextGlobalsDescriptorSet = Rr_AllocateDescriptorSet(
        &Frame->DescriptorAllocator,
        Renderer->Device,
        TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS]);
    Rr_WriteBufferDescriptor(
        &DescriptorWriter,
        0,
        TextPipeline->GlobalsBuffers[CurrentFrameIndex].Handle,
        sizeof(Rr_TextGlobalsLayout),
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, TextGlobalsDescriptorSet);
    Rr_ResetDescriptorWriter(&DescriptorWriter);
    vkCmdBindDescriptorSets(
        CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        TextPipeline->Layout,
        RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS,
        1,
        &TextGlobalsDescriptorSet,
        0,
        NULL);
    size_t TextDataOffset = 0;
    size_t TextCount = Rr_ArrayCount(RenderingContext->DrawTextArray);
    Rr_TextPerInstanceVertexInput* TextData = (Rr_TextPerInstanceVertexInput*)Rr_Malloc(RR_TEXT_BUFFER_SIZE);
    for (size_t TextIndex = 0; TextIndex < TextCount; ++TextIndex)
    {
        Rr_DrawTextInfo* DrawTextInfo = &RenderingContext->DrawTextArray[TextIndex];

        VkDescriptorSet TextFontDescriptorSet = Rr_AllocateDescriptorSet(
            &Frame->DescriptorAllocator,
            Renderer->Device,
            TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT]);
        Rr_WriteBufferDescriptor(
            &DescriptorWriter,
            0,
            DrawTextInfo->Font->Buffer.Handle,
            sizeof(Rr_TextFontLayout),
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        Rr_WriteImageDescriptor(
            &DescriptorWriter,
            1,
            DrawTextInfo->Font->Atlas.View,
            Renderer->LinearSampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
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
        TextPushConstants.PositionScreenSpace[0] = DrawTextInfo->Position[0];
        TextPushConstants.PositionScreenSpace[1] = DrawTextInfo->Position[1];
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
        size_t TextLength = DrawTextInfo->String->Length;
        size_t FinalTextLength = 0;
        u32 PalleteIndex = 0;
        bool bCodePending = false;
        bool bPalleteIndexPending = false;
        vec2 AccumulatedAdvance = {};
        for (size_t CharacterIndex = 0; CharacterIndex < TextLength; ++CharacterIndex)
        {
            u32 Unicode = DrawTextInfo->String->Data[CharacterIndex];

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
                AccumulatedAdvance[1] += DrawTextInfo->Font->LineHeight;
                AccumulatedAdvance[0] = 0.0f;
                continue;
            }
            else if (Unicode == ' ')
            {
                f32 Advance = DrawTextInfo->Font->Advances[Unicode];
                AccumulatedAdvance[0] += Advance;
                continue;
            }
            else
            {
                const u32 GlyphIndexMask = ~0xFFE00000;
                u32 GlyphPack = GlyphIndexMask & Unicode;
                GlyphPack |= PalleteIndex << 28;
                *Input = (Rr_TextPerInstanceVertexInput){
                    .Unicode = GlyphPack,
                    .Advance = {
                        AccumulatedAdvance[0],
                        AccumulatedAdvance[1] }
                };
                FinalTextLength++;
                f32 Advance = DrawTextInfo->Font->Advances[Unicode];
                AccumulatedAdvance[0] += Advance;
            }
        }
        vkCmdBindVertexBuffers(
            CommandBuffer,
            1,
            1,
            &TextPipeline->TextBuffers[CurrentFrameIndex].Handle,
            &(VkDeviceSize){ TextDataOffset * sizeof(Rr_TextPerInstanceVertexInput) });
        TextDataOffset += FinalTextLength;
        vkCmdDraw(CommandBuffer, 4, FinalTextLength, 0, 0);
    }
    SDL_memcpy(TextPipeline->TextBuffers[CurrentFrameIndex].AllocationInfo.pMappedData, TextData, TextDataOffset * sizeof(Rr_TextPerInstanceVertexInput));

    vkCmdEndRenderPass(CommandBuffer);

    Rr_Free(DrawData);
    Rr_Free(TextData);
    Rr_ArrayFree(MaterialsArray);
    Rr_ArrayFree(DrawMeshIndicesArrays);
    Rr_ArrayFree(RenderingContext->DrawTextArray);
    Rr_ArrayFree(RenderingContext->DrawMeshArray);
    Rr_DestroyDescriptorWriter(&DescriptorWriter);
}

void Rr_Draw(Rr_App* const App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    const VkDevice Device = Renderer->Device;
    Rr_Swapchain* Swapchain = &Renderer->Swapchain;
    const size_t FrameIndex = Rr_GetCurrentFrameIndex(Renderer);
    Rr_Frame* Frame = Rr_GetCurrentFrame(Renderer);
    const Rr_Image* ColorImage = &Renderer->DrawTargets[FrameIndex].ColorImage;
    const Rr_Image* DepthImage = &Renderer->DrawTargets[FrameIndex].DepthImage;

    vkWaitForFences(Device, 1, &Frame->RenderFence, true, 1000000000);
    vkResetFences(Device, 1, &Frame->RenderFence);

    Frame->StagingBuffer.CurrentOffset = 0;

    const VkCommandBuffer CommandBuffer = Frame->MainCommandBuffer;

    Rr_ResetDescriptorAllocator(&Frame->DescriptorAllocator, Device);

    u32 SwapchainImageIndex;
    VkResult Result = vkAcquireNextImageKHR(Device, Swapchain->Handle, 1000000000, Frame->SwapchainSemaphore, VK_NULL_HANDLE, &SwapchainImageIndex);
    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 1);
        return;
    }
    if (Result == VK_SUBOPTIMAL_KHR)
    {
        SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 1);
    }
    SDL_assert(Result >= 0);

    const VkImage SwapchainImage = Swapchain->Images[SwapchainImageIndex].Handle;

    const VkCommandBufferBeginInfo CommandBufferBeginInfo = GetCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

    Rr_ImageBarrier ColorImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = ColorImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .StageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    };
    Rr_ChainImageBarrier(&ColorImageTransition,
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    Rr_ImageBarrier DepthImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = DepthImage->Handle,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .StageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
    };
    Rr_ChainImageBarrier(&DepthImageTransition,
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    App->Config->DrawFunc(App);

    // Rr_ImageBarrier ColorImageTransition = {
    //     .CommandBuffer = CommandBuffer,
    //     .Image = ColorImage->Handle,
    //     .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //     .AccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    //     .StageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    // };
    Rr_ChainImageBarrier(&ColorImageTransition,
        VK_PIPELINE_STAGE_2_BLIT_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    Rr_ImageBarrier SwapchainImageTransition = {
        .CommandBuffer = CommandBuffer,
        .Image = SwapchainImage,
        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .AccessMask = VK_ACCESS_2_NONE,
        .StageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    //
    // Rr_ChainImageBarrier(&SwapchainImageTransition,
    //     VK_PIPELINE_STAGE_2_CLEAR_BIT,
    //     VK_ACCESS_2_TRANSFER_WRITE_BIT,
    //     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // vkCmdClearColorImage(
    //     CommandBuffer,
    //     SwapchainImage,
    //     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //     &(VkClearColorValue){},
    //     1,
    //     &(VkImageSubresourceRange){
    //         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    //         .baseMipLevel = 0,
    //         .levelCount = 1,
    //         .baseArrayLayer = 0,
    //         .layerCount = 1,
    //     });
    Rr_ChainImageBarrier(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_BLIT_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    //
    Rr_BlitColorImage(CommandBuffer, ColorImage->Handle, SwapchainImage, Renderer->ActiveResolution,
        (VkExtent2D){
            .width = (Renderer->ActiveResolution.width) * Renderer->Scale,
            .height = (Renderer->ActiveResolution.height) * Renderer->Scale });

    // if (Renderer->ImGui.bInit)
    // {
    //     Rr_ChainImageBarrier(&SwapchainImageTransition,
    //         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    //         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
    //         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    //
    //     VkRenderingAttachmentInfo ColorAttachmentInfo = GetRenderingAttachmentInfo_Color(Swapchain->Images[SwapchainImageIndex].View, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, NULL);
    //     VkRenderingInfo RenderingInfo = GetRenderingInfo(Swapchain->Extent, &ColorAttachmentInfo, NULL);
    //
    //     vkCmdBeginRendering(CommandBuffer, &RenderingInfo);
    //
    //     ImGui_ImplVulkan_RenderDrawData(igGetDrawData(), CommandBuffer, VK_NULL_HANDLE);
    //
    //     vkCmdEndRendering(CommandBuffer);
    // }

    Rr_ChainImageBarrier(&SwapchainImageTransition,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Rr_ChainImageBarrier(&ColorImageTransition,
    //     VK_PIPELINE_STAGE_2_BLIT_BIT,
    //     VK_ACCESS_2_TRANSFER_READ_BIT,
    //     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vkEndCommandBuffer(CommandBuffer);

    SDL_LockMutex(Renderer->Graphics.Mutex);
    VkCommandBufferSubmitInfo CommandBufferSubmitInfo = GetCommandBufferSubmitInfo(CommandBuffer);
    VkSemaphoreSubmitInfo SignalSemaphoreSubmitInfo = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, Frame->RenderSemaphore);

    const size_t TransientSemaphoresCount = Rr_ArrayCount(Renderer->Graphics.TransientSemaphoresArray);
    const size_t WaitCount = TransientSemaphoresCount + 1;
    VkSemaphoreSubmitInfo WaitSemaphoreSubmitInfos[WaitCount];
    for (int SemaphoreIndex = 0; SemaphoreIndex < Rr_ArrayCount(Renderer->Graphics.TransientSemaphoresArray); ++SemaphoreIndex)
    {
        WaitSemaphoreSubmitInfos[SemaphoreIndex] = GetSemaphoreSubmitInfo(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            Renderer->Graphics.TransientSemaphoresArray[SemaphoreIndex]);
    }
    WaitSemaphoreSubmitInfos[TransientSemaphoresCount] = GetSemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, Frame->SwapchainSemaphore);
    Rr_ArrayEmpty(Renderer->Graphics.TransientSemaphoresArray);

    const VkSubmitInfo2 SubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = NULL,
        .waitSemaphoreInfoCount = WaitCount,
        .pWaitSemaphoreInfos = WaitSemaphoreSubmitInfos,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &CommandBufferSubmitInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &SignalSemaphoreSubmitInfo,
    };
    vkQueueSubmit2(Renderer->Graphics.Queue, 1, &SubmitInfo, Frame->RenderFence);
    SDL_UnlockMutex(Renderer->Graphics.Mutex);

    const VkPresentInfoKHR PresentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &Frame->RenderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &Swapchain->Handle,
        .pImageIndices = &SwapchainImageIndex,
    };

    Result = vkQueuePresentKHR(Renderer->Graphics.Queue, &PresentInfo);
    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SDL_AtomicSet(&Renderer->Swapchain.bResizePending, 1);
    }

    Renderer->FrameNumber++;
}
