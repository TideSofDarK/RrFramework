#include "Rr_Text.h"

#include <SDL_log.h>

#include <cJSON/cJSON.h>

#include "Rr_Buffer.h"
#include "Rr_Image.h"
#include "Rr_Renderer.h"
#include "Rr_Asset.h"

void Rr_InitTextRenderer(Rr_Renderer* Renderer)
{
    VkDevice Device = Renderer->Device;
    Rr_TextPipeline* TextPipeline = &Renderer->TextPipeline;

    /* Descriptor Set Layouts */
    Rr_DescriptorLayoutBuilder DescriptorLayoutBuilder = { 0 };
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS] =
        Rr_BuildDescriptorLayout(
            &DescriptorLayoutBuilder,
            Device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    Rr_ClearDescriptors(&DescriptorLayoutBuilder);
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT] =
        Rr_BuildDescriptorLayout(
            &DescriptorLayoutBuilder,
            Device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    /* Pipeline Layout */
    VkPushConstantRange PushConstantRange = {
        .offset = 0,
        .size = 128,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkPipelineLayoutCreateInfo LayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
        .pSetLayouts = TextPipeline->DescriptorSetLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };
    vkCreatePipelineLayout(Device, &LayoutInfo, NULL, &TextPipeline->Layout);

    /* Pipeline */
    Rr_ExternAsset(BuiltinTextVERT);
    Rr_ExternAsset(BuiltinTextFRAG);

    Rr_PipelineBuilder Builder = Rr_GetPipelineBuilder();
    Rr_EnableVertexInputAttribute(&Builder, VK_FORMAT_R32G32_SFLOAT);
    Rr_EnableVertexStage(&Builder, &BuiltinTextVERT);
    Rr_EnableFragmentStage(&Builder, &BuiltinTextFRAG);
    Rr_EnableAlphaBlend(&Builder);
    Rr_EnableRasterizer(&Builder, RR_POLYGON_MODE_FILL);
    // Rr_EnableRasterizer(&Builder, RR_POLYGON_MODE_LINE);
    TextPipeline->Handle = Rr_BuildPipeline(
        Renderer,
        &Builder,
        TextPipeline->Layout);

    /* Quad Buffer */
    f32 Quad[12] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f
    };
    TextPipeline->QuadBuffer = Rr_CreateDeviceVertexBuffer(
        Renderer,
        sizeof(Quad));
    Rr_UploadToDeviceBufferImmediate(Renderer, &TextPipeline->QuadBuffer, Quad, sizeof(Quad));

    /* Globals Buffers */
    for (int Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        TextPipeline->GlobalsBuffers[Index] = Rr_CreateDeviceUniformBuffer(
            Renderer,
            sizeof(Rr_TextGlobalsLayout));
    }

    /* Builtin Font */
    Rr_ExternAsset(BuiltinFontPNG);
    Rr_ExternAsset(BuiltinFontJSON);
    Renderer->BuiltinFont = Rr_CreateFont(Renderer, &BuiltinFontPNG, &BuiltinFontJSON);
}

void Rr_CleanupTextRenderer(Rr_Renderer* Renderer)
{
    Rr_TextPipeline* TextPipeline = &Renderer->TextPipeline;
    VkDevice Device = Renderer->Device;
    vkDestroyPipeline(Device, TextPipeline->Handle, NULL);
    vkDestroyPipelineLayout(Device, TextPipeline->Layout, NULL);
    for (int Index = 0; Index < RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT; ++Index)
    {
        vkDestroyDescriptorSetLayout(Device, TextPipeline->DescriptorSetLayouts[Index], NULL);
    }
    for (int Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_DestroyBuffer(&TextPipeline->GlobalsBuffers[Index], Renderer->Allocator);
    }
    Rr_DestroyBuffer(&TextPipeline->QuadBuffer, Renderer->Allocator);
    Rr_DestroyFont(Renderer, &Renderer->BuiltinFont);
}

Rr_Font Rr_CreateFont(Rr_Renderer* Renderer, Rr_Asset* FontPNG, Rr_Asset* FontJSON)
{
    Rr_Image Atlas = Rr_CreateImageFromPNG(
        FontPNG,
        Renderer,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        false,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Rr_Buffer Buffer = Rr_CreateBuffer(
        Renderer->Allocator,
        sizeof(Rr_TextFontLayout),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);

    cJSON* FontDataJSON = cJSON_ParseWithLength(FontJSON->Data, FontJSON->Length);

    cJSON* AtlasJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "atlas");

    Rr_TextFontLayout TextFontData = {
        .Reserved = { 0.0f, 1.0f },
        .AtlasSize = {
            (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "width")),
            (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "height")),
        }
    };

    cJSON* GlyphsJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "glyphs");

    size_t GlyphCount = cJSON_GetArraySize(GlyphsJSON);
    for (int GlyphIndex = 0; GlyphIndex < GlyphCount; ++GlyphIndex)
    {
        cJSON* GlyphJSON = cJSON_GetArrayItem(GlyphsJSON, GlyphIndex);

        size_t Unicode = (size_t)cJSON_GetNumberValue(cJSON_GetObjectItem(GlyphJSON, "unicode"));

        cJSON* AtlasBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "atlasBounds");

        float AtlasBounds[4] = {};

        if (cJSON_IsObject(AtlasBoundsJSON))
        {
            AtlasBounds[0] = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "left"));
            AtlasBounds[1] = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "bottom"));
            AtlasBounds[2] = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "right"));
            AtlasBounds[3] = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "top"));
        }

        TextFontData.Glyphs[Unicode] = (Rr_Glyph){
            .NormalizedWidth = (AtlasBounds[2] - AtlasBounds[0]) / TextFontData.AtlasSize[0],
            .NormalizedHeight = (AtlasBounds[3] - AtlasBounds[1]) / TextFontData.AtlasSize[1],
            .NormalizedX = AtlasBounds[0] / TextFontData.AtlasSize[0],
            .NormalizedY = AtlasBounds[1] / TextFontData.AtlasSize[1],
        };

        // SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "%f %f %f %f\n",
        //     TextFontData.Glyphs[Unicode].NormalizedWidth,
        //     TextFontData.Glyphs[Unicode].NormalizedHeight,
        //     TextFontData.Glyphs[Unicode].NormalizedX,
        //     TextFontData.Glyphs[Unicode].NormalizedY);
    }

    // SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "%d\n", cJSON_GetArraySize(GlyphsJSON));

    cJSON_Delete(FontDataJSON);

    Rr_UploadToDeviceBufferImmediate(
        Renderer,
        &Buffer,
        &TextFontData,
        sizeof(Rr_TextFontLayout));

    return (Rr_Font){
        .Buffer = Buffer,
        .Atlas = Atlas
    };
}

void Rr_DestroyFont(Rr_Renderer* Renderer, Rr_Font* Font)
{
    Rr_DestroyImage(Renderer, &Font->Atlas);
    Rr_DestroyBuffer(&Font->Buffer, Renderer->Allocator);
}
