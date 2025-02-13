#include "Rr_Text.h"

#include "Rr/Rr_Load.h"
#include "Rr_App.h"
#include "Rr_BuiltinAssets.inc"
#include "Rr_Image.h"
#include "Rr_Pipeline.h"

#include <Rr/Rr_Utility.h>

#include <cJSON/cJSON.h>

void Rr_InitTextRenderer(Rr_App *App)
{
    // Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);
    //
    // Rr_Renderer *Renderer = &App->Renderer;
    // VkDevice Device = Renderer->Device;
    // Rr_TextPipeline *TextPipeline = &Renderer->TextPipeline;
    //
    // /* Descriptor Set Layouts */
    //
    // Rr_DescriptorLayoutBuilder DescriptorLayoutBuilder = { 0 };
    // Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    // TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS] = Rr_BuildDescriptorLayout(
    //     &DescriptorLayoutBuilder,
    //     Device,
    //     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    //
    // Rr_ClearDescriptors(&DescriptorLayoutBuilder);
    // Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    // Rr_AddDescriptor(&DescriptorLayoutBuilder, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    // TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT] = Rr_BuildDescriptorLayout(
    //     &DescriptorLayoutBuilder,
    //     Device,
    //     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    //
    // /* Pipeline Layout */
    //
    // VkPushConstantRange PushConstantRange = { .offset = 0,
    //                                           .size = 128,
    //                                           .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    //                                           };
    //
    // VkPipelineLayoutCreateInfo LayoutInfo = {
    //     .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    //     .pNext = NULL,
    //     .setLayoutCount = RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
    //     .pSetLayouts = TextPipeline->DescriptorSetLayouts,
    //     .pushConstantRangeCount = 1,
    //     .pPushConstantRanges = &PushConstantRange,
    // };
    // vkCreatePipelineLayout(Device, &LayoutInfo, NULL, &TextPipeline->Layout);
    //
    // /* Pipeline */
    //
    // Rr_Asset BuiltinTextVERT = Rr_LoadAsset(RR_BUILTIN_TEXT_VERT_SPV);
    // Rr_Asset BuiltinTextFRAG = Rr_LoadAsset(RR_BUILTIN_TEXT_FRAG_SPV);
    //
    // Rr_PipelineBuilder *Builder = Rr_CreatePipelineBuilder(Scratch.Arena);
    // Rr_EnableTriangleFan(Builder);
    // Rr_EnablePerVertexInputAttributes(
    //     Builder,
    //     &(Rr_VertexInput){ .Attributes = {
    //                            { .Type = RR_VERTEX_INPUT_TYPE_VEC2, .Location = 0 },
    //                        } });
    // Rr_EnablePerInstanceInputAttributes(
    //     Builder,
    //     &(Rr_VertexInput){ .Attributes = {
    //                            { .Type = RR_VERTEX_INPUT_TYPE_VEC2, .Location = 1 },
    //                            { .Type = RR_VERTEX_INPUT_TYPE_UINT, .Location = 2 },
    //                        } });
    // Rr_EnableVertexStage(Builder, &BuiltinTextVERT);
    // Rr_EnableFragmentStage(Builder, &BuiltinTextFRAG);
    // Rr_EnableColorAttachment(Builder, true);
    // Rr_EnableRasterizer(Builder, RR_POLYGON_MODE_FILL);
    // //     Rr_EnableRasterizer(&Builder, RR_POLYGON_MODE_LINE);
    // TextPipeline->Pipeline = Rr_CreatePipeline(App, Builder, TextPipeline->Layout);
    //
    // /* Quad Buffer */
    //
    // float Quad[8] = {
    //     0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
    // };
    // TextPipeline->QuadBuffer = Rr_CreateDeviceVertexBuffer(App, sizeof(Quad));
    // Rr_UploadToDeviceBufferImmediate(App, TextPipeline->QuadBuffer, RR_MAKE_DATA_ARRAY(Quad));
    //
    // /* Builtin Font */
    //
    // Renderer->BuiltinFont = Rr_CreateFont(App, RR_BUILTIN_IOSEVKA_PNG, RR_BUILTIN_IOSEVKA_JSON);
    //
    // Rr_DestroyArenaScratch(Scratch);
}

void Rr_CleanupTextRenderer(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    Rr_TextPipeline *TextPipeline = &Renderer->TextPipeline;
    VkDevice Device = Renderer->Device;
    // Rr_DestroyPipeline(App, TextPipeline->Pipeline);
    vkDestroyPipelineLayout(Device, TextPipeline->Layout, NULL);
    for(size_t Index = 0; Index < RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT; ++Index)
    {
        vkDestroyDescriptorSetLayout(Device, TextPipeline->DescriptorSetLayouts[Index], NULL);
    }
    Rr_DestroyBuffer(App, TextPipeline->QuadBuffer);
    Rr_DestroyFont(App, Renderer->BuiltinFont);
}

Rr_Font *Rr_CreateFont(Rr_App *App, Rr_AssetRef FontPNGRef, Rr_AssetRef FontJSONRef)
{
    Rr_Image *Atlas;
    Rr_LoadTask ImageLoadTask = (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
        .AssetRef = FontPNGRef,
        .Out.Image = &Atlas,
    };
    Rr_LoadImmediate(App, 1, &ImageLoadTask);

    Rr_Buffer *Buffer = Rr_CreateBuffer_Internal(
        App,
        sizeof(Rr_TextFontLayout),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false,
        false);

    Rr_Asset FontJSON = Rr_LoadAsset(FontJSONRef);

    cJSON *FontDataJSON = cJSON_ParseWithLength(FontJSON.Pointer, FontJSON.Size);

    cJSON *AtlasJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "atlas");
    cJSON *MetricsJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "metrics");

    Rr_TextFontLayout TextFontData = {
        .DistanceRange = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "distanceRange")),
        .AtlasSize = {
            (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "width")),
            (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "height")),
        },
    };

    Rr_Font *Font = RR_GET_FREE_LIST_ITEM(&App->Renderer.Fonts, App->PermanentArena);
    *Font = (Rr_Font){
        .Buffer = Buffer,
        .Atlas = Atlas,
        .LineHeight = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(MetricsJSON, "lineHeight")),
        .DefaultSize = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "size")),
        .Advances = Rr_Calloc(RR_TEXT_MAX_GLYPHS, sizeof(float)),
    };

    cJSON *GlyphsJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "glyphs");

    size_t GlyphCount = cJSON_GetArraySize(GlyphsJSON);
    for(size_t GlyphIndex = 0; GlyphIndex < GlyphCount; ++GlyphIndex)
    {
        cJSON *GlyphJSON = cJSON_GetArrayItem(GlyphsJSON, (int32_t)GlyphIndex);

        size_t Unicode = (size_t)cJSON_GetNumberValue(cJSON_GetObjectItem(GlyphJSON, "unicode"));

        cJSON *AtlasBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "atlasBounds");
        uint16_t AtlasBounds[4] = { 0 };
        if(cJSON_IsObject(AtlasBoundsJSON))
        {
            AtlasBounds[0] = (uint16_t)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "left"));
            AtlasBounds[1] = (uint16_t)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "bottom"));
            AtlasBounds[2] = (uint16_t)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "right"));
            AtlasBounds[3] = (uint16_t)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasBoundsJSON, "top"));
        }

        cJSON *PlaneBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "planeBounds");
        Rr_Vec4 PlaneBounds = { 0 };
        if(cJSON_IsObject(PlaneBoundsJSON))
        {
            PlaneBounds.X = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "left"));
            PlaneBounds.Y = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "bottom"));
            PlaneBounds.Z = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "right"));
            PlaneBounds.W = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(PlaneBoundsJSON, "top"));
        }

        Font->Advances[Unicode] = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(GlyphJSON, "advance"));

        uint32_t PlaneLB;
        uint32_t PlaneRT;

        Rr_PackVec4(PlaneBounds, &PlaneLB, &PlaneRT);

        TextFontData.Glyphs[Unicode] =
            (Rr_Glyph){ .AtlasXY = ((uint32_t)AtlasBounds[0] << 16) | (uint32_t)AtlasBounds[1],
                        .AtlasWH = ((uint32_t)(AtlasBounds[2] - AtlasBounds[0]) << 16) |
                                   (uint32_t)(AtlasBounds[3] - AtlasBounds[1]),
                        .PlaneLB = PlaneLB,
                        .PlaneRT = PlaneRT };
    }

    cJSON_Delete(FontDataJSON);

    Rr_UploadToDeviceBufferImmediate(App, Buffer, RR_MAKE_DATA_STRUCT(TextFontData));

    return Font;
}

void Rr_DestroyFont(Rr_App *App, Rr_Font *Font)
{
    Rr_Free(Font->Advances);

    Rr_DestroyImage(App, Font->Atlas);
    Rr_DestroyBuffer(App, Font->Buffer);

    RR_RETURN_FREE_LIST_ITEM(&App->Renderer.Fonts, Font);
}

Rr_Vec2 Rr_CalculateTextSize(Rr_Font *Font, float FontSize, Rr_String *String)
{
    if(String->Length == 0)
    {
        return (Rr_Vec2){ 0 };
    }

    /* @TODO: Optimize. */

    bool CodePending = false;
    bool PalleteIndexPending = false;

    float AdvanceX = 0.0f;
    float MaxX = 0.0f;
    int Lines = 1;
    for(size_t CharacterIndex = 0; CharacterIndex < String->Length; ++CharacterIndex)
    {
        uint32_t Unicode = String->Data[CharacterIndex];

        if(CodePending)
        {
            if(PalleteIndexPending)
            {
                if(Unicode >= '0' && Unicode <= '7')
                {
                    PalleteIndexPending = false;
                    CodePending = false;
                    continue;
                }
                else
                {
                    PalleteIndexPending = false;
                    CodePending = false;
                }
            }
            else if(Unicode == 'c')
            {
                PalleteIndexPending = true;
                continue;
            }
            else
            {
                Unicode = '$';
                CharacterIndex--;
                CodePending = false;
            }
        }
        else if(Unicode == '$')
        {
            CodePending = true;
            continue;
        }
        if(Unicode == '\n')
        {
            Lines++;
            MaxX = RR_MAX(MaxX, AdvanceX);
            AdvanceX = 0.0f;
        }
        else
        {
            AdvanceX += Font->Advances[Unicode];
            MaxX = RR_MAX(MaxX, AdvanceX);
        }
    }

    return (Rr_Vec2){ .Width = MaxX * FontSize, .Height = Lines * Font->LineHeight * FontSize };
}
