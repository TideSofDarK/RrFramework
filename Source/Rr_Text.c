#include "Rr_Text.h"

#include "Rr/Rr_Utility.h"
#include "Rr_App.h"
#include "Rr_Pipeline.h"
#include "Rr_Image.h"
#include "Rr_BuiltinAssets.inc"

#include <cJSON/cJSON.h>

void Rr_InitTextRenderer(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
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
    Rr_Asset BuiltinTextVERT = Rr_LoadAsset(RR_BUILTIN_TEXT_VERT_SPV);
    Rr_Asset BuiltinTextFRAG = Rr_LoadAsset(RR_BUILTIN_TEXT_FRAG_SPV);

    Rr_PipelineBuilder* Builder = Rr_CreatePipelineBuilder();
    Rr_EnableTriangleFan(Builder);
    Rr_EnablePerVertexInputAttributes(
        Builder,
        &(Rr_VertexInput){
            .Attributes = {
                { .Type = RR_VERTEX_INPUT_TYPE_VEC2, .Location = 0 },
            } });
    Rr_EnablePerInstanceInputAttributes(
        Builder,
        &(Rr_VertexInput){
            .Attributes = {
                { .Type = RR_VERTEX_INPUT_TYPE_VEC2, .Location = 1 },
                { .Type = RR_VERTEX_INPUT_TYPE_UINT, .Location = 2 },
            } });
    Rr_EnableVertexStage(Builder, &BuiltinTextVERT);
    Rr_EnableFragmentStage(Builder, &BuiltinTextFRAG);
    Rr_EnableColorAttachment(Builder, RR_TRUE);
    Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
    //     Rr_EnableRasterizer(&Builder, RR_POLYGON_MODE_LINE);
    TextPipeline->Handle = Rr_BuildPipeline(
        Renderer,
        Builder,
        TextPipeline->Layout);

    /* Quad Buffer */
    Rr_F32 Quad[8] = {
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
    };
    TextPipeline->QuadBuffer = Rr_CreateDeviceVertexBuffer(
        App,
        sizeof(Quad));
    Rr_UploadToDeviceBufferImmediate(App, TextPipeline->QuadBuffer, Quad, sizeof(Quad));

    /* Buffers */
    for (int FrameIndex = 0; FrameIndex < RR_FRAME_OVERLAP; ++FrameIndex)
    {
        TextPipeline->GlobalsBuffers[FrameIndex] = Rr_CreateDeviceUniformBuffer(
            App,
            sizeof(Rr_TextGlobalsLayout));

        TextPipeline->TextBuffers[FrameIndex] = Rr_CreateMappedVertexBuffer(
            App,
            RR_TEXT_BUFFER_SIZE);
    }

    /* Builtin Font */
    Renderer->BuiltinFont = Rr_CreateFont(App, RR_BUILTIN_IOSEVKA_PNG, RR_BUILTIN_IOSEVKA_JSON);
}

void Rr_CleanupTextRenderer(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
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
        Rr_DestroyBuffer(App, TextPipeline->GlobalsBuffers[Index]);
        Rr_DestroyBuffer(App, TextPipeline->TextBuffers[Index]);
    }
    Rr_DestroyBuffer(App, TextPipeline->QuadBuffer);
    Rr_DestroyFont(App, Renderer->BuiltinFont);
}

Rr_Font* Rr_CreateFont(Rr_App* App, Rr_AssetRef FontPNGRef, Rr_AssetRef FontJSONRef)
{
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_Image* Atlas;
    Rr_LoadTask ImageLoadTask = Rr_LoadColorImageFromPNG(FontPNGRef, &Atlas);
    Rr_LoadImmediate(App, &ImageLoadTask, 1);

    Rr_Buffer* Buffer = Rr_CreateBuffer(
        App,
        sizeof(Rr_TextFontLayout),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        RR_FALSE);

    Rr_Asset FontJSON = Rr_LoadAsset(FontJSONRef);

    cJSON* FontDataJSON = cJSON_ParseWithLength(FontJSON.Data, FontJSON.Length);

    cJSON* AtlasJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "atlas");
    cJSON* MetricsJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "metrics");

    Rr_TextFontLayout TextFontData = {
        .DistanceRange = (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "distanceRange")),
        .AtlasSize = {
            (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "width")),
            (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "height")),
        },
    };

    Rr_Font* Font = Rr_CreateObject(&App->ObjectStorage);
    *Font = (Rr_Font){
        .Buffer = Buffer,
        .Atlas = Atlas,
        .LineHeight = (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(MetricsJSON, "lineHeight")),
        .DefaultSize = (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "size")),
        .Advances = Rr_Calloc(RR_TEXT_MAX_GLYPHS, sizeof(Rr_F32))
    };

    cJSON* GlyphsJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "glyphs");

    size_t GlyphCount = cJSON_GetArraySize(GlyphsJSON);
    for (size_t GlyphIndex = 0; GlyphIndex < GlyphCount; ++GlyphIndex)
    {
        cJSON* GlyphJSON = cJSON_GetArrayItem(GlyphsJSON, (Rr_I32)GlyphIndex);

        size_t Unicode = (size_t)cJSON_GetNumberValue(cJSON_GetObjectItem(GlyphJSON, "unicode"));

        cJSON* AtlasBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "atlasBounds");
        Rr_U16 AtlasBounds[4] = { 0 };
        if (cJSON_IsObject(AtlasBoundsJSON))
        {
            AtlasBounds[0] = (Rr_U16)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "left"));
            AtlasBounds[1] = (Rr_U16)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "bottom"));
            AtlasBounds[2] = (Rr_U16)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "right"));
            AtlasBounds[3] = (Rr_U16)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "top"));
        }

        cJSON* PlaneBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "planeBounds");
        Rr_Vec4 PlaneBounds = { 0 };
        if (cJSON_IsObject(PlaneBoundsJSON))
        {
            PlaneBounds.X = (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "left"));
            PlaneBounds.Y = (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "bottom"));
            PlaneBounds.Z = (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "right"));
            PlaneBounds.W = (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "top"));
        }

        Font->Advances[Unicode] = (Rr_F32)cJSON_GetNumberValue(cJSON_GetObjectItem(GlyphJSON, "advance"));

        Rr_U32 PlaneLB;
        Rr_U32 PlaneRT;

        Rr_PackVec4(PlaneBounds, &PlaneLB, &PlaneRT);

        TextFontData.Glyphs[Unicode] = (Rr_Glyph){
            .AtlasXY = ((Rr_U32)AtlasBounds[0] << 16) | (Rr_U32)AtlasBounds[1],
            .AtlasWH = ((Rr_U32)(AtlasBounds[2] - AtlasBounds[0]) << 16) | (Rr_U32)(AtlasBounds[3] - AtlasBounds[1]),
            .PlaneLB = PlaneLB,
            .PlaneRT = PlaneRT
        };
    }

    cJSON_Delete(FontDataJSON);

    Rr_UploadToDeviceBufferImmediate(
        App,
        Buffer,
        &TextFontData,
        sizeof(Rr_TextFontLayout));

    return Font;
}

void Rr_DestroyFont(Rr_App* App, Rr_Font* Font)
{
    Rr_Renderer* Renderer = &App->Renderer;

    Rr_Free(Font->Advances);

    Rr_DestroyImage(App, Font->Atlas);
    Rr_DestroyBuffer(App, Font->Buffer);

    Rr_DestroyObject(&App->ObjectStorage, Font);
}
