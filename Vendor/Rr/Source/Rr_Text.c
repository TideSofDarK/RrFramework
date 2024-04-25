#include "Rr_Text.h"

#include <SDL_log.h>

#include <cJSON/cJSON.h>

#include "Rr_Buffer.h"
#include "Rr_Image.h"
#include "Rr_Renderer.h"
#include "Rr_Asset.h"
#include "Rr_Memory.h"

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
    Rr_EnablePerVertexInputAttribute(&Builder, VK_FORMAT_R32G32_SFLOAT);
    Rr_EnablePerInstanceInputAttribute(&Builder, VK_FORMAT_R32_UINT);
    Rr_EnableVertexStage(&Builder, &BuiltinTextVERT);
    Rr_EnableFragmentStage(&Builder, &BuiltinTextFRAG);
    Rr_EnableAlphaBlend(&Builder);
    Rr_EnableRasterizer(&Builder, RR_POLYGON_MODE_FILL);
//     Rr_EnableRasterizer(&Builder, RR_POLYGON_MODE_LINE);
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

    /* Buffers */
    for (int Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        TextPipeline->GlobalsBuffers[Index] = Rr_CreateDeviceUniformBuffer(
            Renderer,
            sizeof(Rr_TextGlobalsLayout));

        TextPipeline->TextBuffers[Index] = Rr_CreateMappedVertexBuffer(
            Renderer,
            RR_TEXT_BUFFER_SIZE);
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
        Rr_DestroyBuffer(&TextPipeline->TextBuffers[Index], Renderer->Allocator);
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
        .DistanceRange = (f32)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "distanceRange")),
        .AtlasSize = {
            (f32)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "width")),
            (f32)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "height")),
        }
    };

    cJSON* GlyphsJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "glyphs");

    size_t GlyphCount = cJSON_GetArraySize(GlyphsJSON);
    for (int GlyphIndex = 0; GlyphIndex < GlyphCount; ++GlyphIndex)
    {
        cJSON* GlyphJSON = cJSON_GetArrayItem(GlyphsJSON, GlyphIndex);

        size_t Unicode = (size_t)cJSON_GetNumberValue(cJSON_GetObjectItem(GlyphJSON, "unicode"));

        cJSON* AtlasBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "atlasBounds");
        u16 AtlasBounds[4] = {};
        if (cJSON_IsObject(AtlasBoundsJSON))
        {
            AtlasBounds[0] = (u16)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "left"));
            AtlasBounds[1] = (u16)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "bottom"));
            AtlasBounds[2] = (u16)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "right"));
            AtlasBounds[3] = (u16)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "top"));
        }

        cJSON* PlaneBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "planeBounds");
        f32 PlaneBounds[4] = {};
        if (cJSON_IsObject(PlaneBoundsJSON))
        {
            PlaneBounds[0] = (f32)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "left"));
            PlaneBounds[1] = (f32)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "bottom"));
            PlaneBounds[2] = (f32)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "right"));
            PlaneBounds[3] = (f32)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "top"));
        }

        TextFontData.Glyphs[Unicode] = (Rr_Glyph){
            .Advance = (f32)cJSON_GetNumberValue(cJSON_GetObjectItem(GlyphJSON, "advance")),
            .AtlasXY = ((u32)AtlasBounds[0] << 16) | (u32)AtlasBounds[1],
            .AtlasWH = ((u32)(AtlasBounds[2] - AtlasBounds[0]) << 16) | (u32)(AtlasBounds[3] - AtlasBounds[1]),
            .Left = PlaneBounds[0],
            .Bottom = PlaneBounds[1],
            .Right = PlaneBounds[2],
            .Top = PlaneBounds[3]
        };

//        TextFontData.Glyphs[Unicode] = (Rr_Glyph){
//            .NormalizedWidth = (AtlasBounds[2] - AtlasBounds[0]) / TextFontData.AtlasSize[0],
//            .NormalizedHeight = (AtlasBounds[3] - AtlasBounds[1]) / TextFontData.AtlasSize[1],
//            .NormalizedX = AtlasBounds[0] / TextFontData.AtlasSize[0],
//            .NormalizedY = AtlasBounds[1] / TextFontData.AtlasSize[1],
//        };

//        TextFontData.Advance = (f32)cJSON_GetNumberValue(cJSON_GetObjectItem(GlyphJSON, "advance"));
    }

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

Rr_String Rr_CreateString(const char* CString)
{
    static u32 Buffer[2048] = {};

    const u8 Ready = 128;
    const u8 Two = 192;
    const u8 Three = 224;
    const u8 Four = 240;
    const u8 Five = 248;

    size_t SourceLength = SDL_strlen(CString);

    u8 Carry = 0;
    size_t FinalIndex = 0;
    u32 FinalCharacter = 0;
    for (size_t SourceIndex = 0; SourceIndex < SourceLength; ++SourceIndex)
    {
        if (Carry > 0)
        {
            Carry--;
            FinalCharacter |= (u8)((~Two & CString[SourceIndex]) << (Carry * 6));

            if (Carry == 0)
            {
                Buffer[FinalIndex] = FinalCharacter;
                FinalIndex++;
            }
        }
        else
        {
            if ((CString[SourceIndex] & Four) == Four)
            {
                FinalCharacter = (u8)(~Five & CString[SourceIndex]);
                FinalCharacter <<= 3 * 6;
                Carry = 3;
                continue;
            }
            else if ((CString[SourceIndex] & Three) == Three)
            {
                FinalCharacter = (u8)(~Four & CString[SourceIndex]);
                FinalCharacter <<= 2 * 6;
                Carry = 2;
                continue;
            }
            else if ((CString[SourceIndex] & Two) == Two)
            {
                FinalCharacter = (u8)(~Three & CString[SourceIndex]);
                FinalCharacter <<= 1 * 6;
                Carry = 1;
                continue;
            }
            else
            {
                Buffer[FinalIndex] = CString[SourceIndex] & ~Ready;
                FinalIndex++;
            }
        }
    }

    const u32* Data = (u32*)Rr_Malloc(sizeof(u32) * FinalIndex);
    SDL_memcpy((void*)Data, Buffer, sizeof(u32) * FinalIndex);

    return (Rr_String){
        .Data = Data,
        .Length = FinalIndex
    };
}

void Rr_DestroyString(Rr_String* String)
{
    Rr_Free((void*)String->Data);
}
