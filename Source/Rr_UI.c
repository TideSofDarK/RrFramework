#include "Rr_BuiltinAssets.inc"

#include "Rr_UI.h"

#include "Rr_Log.h"
#include "Rr_Renderer.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Utility.h>

#include <xxHash/xxhash.h>

#include <cJSON/cJSON.h>

#include <assert.h>

typedef uint16_t Rr_UIIndex;

typedef struct Rr_UIVertex Rr_UIVertex;
struct Rr_UIVertex
{
    Rr_Vec2 Position;
    Rr_Vec2 UV;
    Rr_Vec4 Color;
};

typedef struct Rr_UIUniformData Rr_UIUniformData;
struct Rr_UIUniformData
{
    Rr_Vec2 ScreenSize;
    float DistanceRange;
    float Time;
};

typedef enum
{
    RR_UI_WIDGET_TYPE_SEPARATOR,
    RR_UI_WIDGET_TYPE_LABEL,
    RR_UI_WIDGET_TYPE_BUTTON,
    RR_UI_WIDGET_TYPE_CHECKBOX,
} Rr_UIWidgetType;

typedef struct Rr_UIWidget Rr_UIWidget;
struct Rr_UIWidget
{
    union
    {
        bool *Checked;
    };
    Rr_String Text;
    Rr_Vec2 Position;
    Rr_UIWidgetType Type;
    Rr_UIWidget *Next;
};

typedef struct Rr_UIWindow Rr_UIWindow;
struct Rr_UIWindow
{
    Rr_String Title;
    Rr_Vec2 Position;
    Rr_Vec2 Size;
    Rr_UIWidget *FirstWidget;
    Rr_UIWidget *CurrentWidget;
    size_t LastFrameNumber;
    bool Minimized;
};

struct Rr_UIContext
{
    size_t FrameNumber;

    Rr_UIStyle Style;

    Rr_Map *WindowMap;
    size_t LastWindowCount;
    RR_SLICE(Rr_UIWindow *) Windows;
    Rr_UIWindow *CurrentWindow;

    Rr_Vec2 Cursor;

    Rr_Vec2 ScreenSize;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    Rr_Font *Font;
    float FontSize;

    Rr_Buffer *VertexBuffer;
    Rr_Buffer *IndexBuffer;

    Rr_UIVertex *VertexBufferDataStart;
    Rr_UIVertex *VertexBufferData;
    Rr_UIIndex *IndexBufferDataStart;
    Rr_UIIndex *IndexBufferData;

    Rr_Buffer *UniformBuffer;

    Rr_Sampler *Sampler;

    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UIContext *Global;

Rr_Font *Rr_CreateFont(
    Rr_Renderer *Renderer,
    Rr_AssetRef FontPNGRef,
    Rr_AssetRef FontJSONRef)
{
    Rr_Image *Atlas;
    Rr_LoadTask ImageLoadTask = (Rr_LoadTask){
        .LoadType = RR_LOAD_TYPE_IMAGE_RGBA8_FROM_PNG,
        .AssetRef = FontPNGRef,
        .Out.Image = &Atlas,
    };
    Rr_LoadImmediate(Renderer, 1, &ImageLoadTask);

    Rr_Asset FontJSON = Rr_LoadAsset(FontJSONRef);

    cJSON *FontDataJSON =
        cJSON_ParseWithLength(FontJSON.Pointer, FontJSON.Size);

    cJSON *AtlasJSON = cJSON_GetObjectItemCaseSensitive(FontDataJSON, "atlas");
    cJSON *MetricsJSON =
        cJSON_GetObjectItemCaseSensitive(FontDataJSON, "metrics");

    Rr_Vec2 AtlasSize;
    AtlasSize.X =
        (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "width"));
    AtlasSize.Y =
        (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "height"));

    Rr_Font *Font = RR_GET_FREE_LIST_ITEM(&Renderer->Fonts, Renderer->Arena);
    *Font = (Rr_Font){
        .Atlas = Atlas,
        .LineHeight = (float)cJSON_GetNumberValue(
            cJSON_GetObjectItem(MetricsJSON, "lineHeight")),
        .DefaultSize =
            (float)cJSON_GetNumberValue(cJSON_GetObjectItem(AtlasJSON, "size")),
        .DistanceRange = (float)cJSON_GetNumberValue(
            cJSON_GetObjectItem(AtlasJSON, "distanceRange")),
    };

    cJSON *GlyphsJSON =
        cJSON_GetObjectItemCaseSensitive(FontDataJSON, "glyphs");

    size_t GlyphCount = cJSON_GetArraySize(GlyphsJSON);
    for(size_t GlyphIndex = 0; GlyphIndex < GlyphCount; ++GlyphIndex)
    {
        cJSON *GlyphJSON = cJSON_GetArrayItem(GlyphsJSON, (int32_t)GlyphIndex);

        uint32_t Codepoint = (size_t)cJSON_GetNumberValue(
            cJSON_GetObjectItem(GlyphJSON, "unicode"));

        Rr_Glyph *Glyph = &Font->Glyphs[Codepoint];

        cJSON *AtlasBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "atlasBounds");
        if(cJSON_IsObject(AtlasBoundsJSON))
        {
            Glyph->AtlasBounds.X =
                (float)cJSON_GetNumberValue(
                    cJSON_GetObjectItem(AtlasBoundsJSON, "left")) /
                AtlasSize.X;
            Glyph->AtlasBounds.Y =
                1.0f - ((float)cJSON_GetNumberValue(
                            cJSON_GetObjectItem(AtlasBoundsJSON, "bottom")) /
                        AtlasSize.Y);
            Glyph->AtlasBounds.Z =
                (float)cJSON_GetNumberValue(
                    cJSON_GetObjectItem(AtlasBoundsJSON, "right")) /
                AtlasSize.X;
            Glyph->AtlasBounds.W =
                1.0f - ((float)cJSON_GetNumberValue(
                            cJSON_GetObjectItem(AtlasBoundsJSON, "top")) /
                        AtlasSize.Y);
        }

        cJSON *PlaneBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "planeBounds");
        if(cJSON_IsObject(PlaneBoundsJSON))
        {
            Glyph->PlaneBounds.X = (float)cJSON_GetNumberValue(
                cJSON_GetObjectItem(PlaneBoundsJSON, "left"));
            Glyph->PlaneBounds.Y = (float)cJSON_GetNumberValue(
                cJSON_GetObjectItem(PlaneBoundsJSON, "bottom"));
            Glyph->PlaneBounds.Z = (float)cJSON_GetNumberValue(
                cJSON_GetObjectItem(PlaneBoundsJSON, "right"));
            Glyph->PlaneBounds.W = (float)cJSON_GetNumberValue(
                cJSON_GetObjectItem(PlaneBoundsJSON, "top"));
        }

        Font->Advances[Codepoint] = (float)cJSON_GetNumberValue(
            cJSON_GetObjectItem(GlyphJSON, "advance"));
    }

    cJSON_Delete(FontDataJSON);

    return Font;
}

void Rr_DestroyFont(Rr_Renderer *Renderer, Rr_Font *Font)
{
    Rr_DestroyImage(Renderer, Font->Atlas);

    RR_RETURN_FREE_LIST_ITEM(&Renderer->Fonts, Font);
}

Rr_Vec2 Rr_CalculateTextSize(Rr_Font *Font, float FontSize, Rr_String *String)
{
    if(String->Length == 0)
    {
        return (Rr_Vec2){ 0 };
    }

    float AdvanceX = 0.0f;
    float MaxX = 0.0f;
    int Lines = 1;
    for(size_t CharacterIndex = 0; CharacterIndex < String->Length;
        ++CharacterIndex)
    {
        uint32_t Codepoint = String->Data[CharacterIndex];

        if(Codepoint >= RR_TEXT_MAX_GLYPHS)
        {
            RR_ABORT("Codepoint is not within range!");
        }

        if(Codepoint == '\n')
        {
            MaxX = RR_MAX(MaxX, AdvanceX);
            AdvanceX = 0.0f;
            Lines++;
            continue;
        }

        AdvanceX += Font->Advances[Codepoint];
        MaxX = RR_MAX(MaxX, AdvanceX);
    }

    return (Rr_Vec2){ .Width = MaxX * FontSize,
                      .Height = Lines * Font->LineHeight * FontSize };
}

static float Rr_GetWindowTitleHeight(void)
{
    return Global->Style.TitlePadding.Y * Global->FontSize * 2.0f +
           Global->Font->LineHeight * Global->FontSize;
}

void Rr_BeginWindow(const char *Title)
{
    size_t TitleLength = strlen(Title);
    XXH64_hash_t Hash = XXH3_64bits(Title, TitleLength);

    Rr_UIWindow **Window = RR_UPSERT(&Global->WindowMap, Hash, Global->Arena);

    if(*Window == NULL)
    {
        *Window = RR_ALLOC(Global->Arena, sizeof(Rr_UIWindow));
        Global->CurrentWindow = *Window;
        Global->CurrentWindow->Title =
            Rr_CreateString(Title, TitleLength, Global->Arena);
        Global->CurrentWindow->Position = (Rr_Vec2){
            .X = Global->FontSize,
            .Y = Global->FontSize,
        };
    }
    else
    {
        Global->CurrentWindow = *Window;
        assert(Global->CurrentWindow->LastFrameNumber != Global->FrameNumber);
    }

    *RR_PUSH_SLICE(&Global->Windows, Global->FrameArena) =
        Global->CurrentWindow;
    Global->CurrentWindow->LastFrameNumber = Global->FrameNumber;

    Global->CurrentWindow->Size = (Rr_Vec2){
        .X = Global->Style.ContentsPadding.X * 2.0f * Global->FontSize,
        .Y = Global->Style.ContentsPadding.Y * 2.0f * Global->FontSize +
             Rr_GetWindowTitleHeight(),
    };

    Global->Cursor.X = Global->CurrentWindow->Position.X +
                       Global->Style.ContentsPadding.X * Global->FontSize;
    Global->Cursor.Y = Global->CurrentWindow->Position.Y +
                       Global->Style.ContentsPadding.Y * Global->FontSize +
                       Rr_GetWindowTitleHeight();

    Global->CurrentWindow->CurrentWidget = Global->CurrentWindow->FirstWidget =
        NULL;
}

void Rr_EndWindow(void)
{
    Global->CurrentWindow = NULL;
}

static Rr_UIWidget *Rr_PushWidget(Rr_UIWindow *Window, Rr_UIWidgetType Type)
{
    if(Window->CurrentWidget == NULL)
    {
        Window->FirstWidget = RR_ALLOC(Global->FrameArena, sizeof(Rr_UIWidget));
        Window->CurrentWidget = Window->FirstWidget;
    }
    else
    {
        Window->CurrentWidget->Next =
            RR_ALLOC(Global->FrameArena, sizeof(Rr_UIWidget));
        Window->CurrentWidget = Window->CurrentWidget->Next;
    }

    Window->CurrentWidget->Type = Type;

    return Window->CurrentWidget;
}

static inline float Rr_GetSeperatorLineHeight(void)
{
    return Global->FontSize * Global->Font->LineHeight * 0.5f;
}

static inline float Rr_GetFrameThickness(void)
{
    return roundf(RR_MAX(1.0f, Global->FontSize * 0.05f));
}

void Rr_Separator(void)
{
    if(Global->CurrentWindow == NULL)
    {
        return;
    }

    Rr_UIWidget *Widget =
        Rr_PushWidget(Global->CurrentWindow, RR_UI_WIDGET_TYPE_SEPARATOR);

    Widget->Position = Global->Cursor;
    float Offset = Rr_GetSeperatorLineHeight();
    Global->Cursor.Y += Offset;
    Global->CurrentWindow->Size.Height += Offset;
}

void Rr_Label(const char *Text)
{
    if(Global->CurrentWindow == NULL)
    {
        return;
    }

    Rr_UIWidget *Widget =
        Rr_PushWidget(Global->CurrentWindow, RR_UI_WIDGET_TYPE_LABEL);
    Widget->Text = Rr_CreateString(Text, 0, Global->FrameArena);
    Rr_Vec2 Size =
        Rr_CalculateTextSize(Global->Font, Global->FontSize, &Widget->Text);

    Widget->Position = Global->Cursor;

    Global->Cursor.Y += Size.Height;
    Global->CurrentWindow->Size.Width = RR_MAX(
        Global->CurrentWindow->Size.Width,
        Size.Width +
            (Global->Style.ContentsPadding.X * Global->FontSize * 2.0f));
    Global->CurrentWindow->Size.Height += Size.Height;
}

void Rr_Button(const char *Text)
{
}

static inline Rr_Vec2 Rr_GetCheckboxSize()
{
    float Size = Global->FontSize * Global->Font->LineHeight * 0.6f;
    return (Rr_Vec2){ Size, Size };
}

void Rr_Checkbox(const char *Text, bool *Checked)
{
    if(Global->CurrentWindow == NULL)
    {
        return;
    }

    Rr_UIWidget *Widget =
        Rr_PushWidget(Global->CurrentWindow, RR_UI_WIDGET_TYPE_CHECKBOX);
    Widget->Text = Rr_CreateString(Text, 0, Global->FrameArena);
    Widget->Checked = Checked;
    Rr_Vec2 Size =
        Rr_CalculateTextSize(Global->Font, Global->FontSize, &Widget->Text);
    Rr_Vec2 CheckboxSize = Rr_GetCheckboxSize();
    Size.Y = RR_MAX(Size.Y, CheckboxSize.Y);
    Size.X += CheckboxSize.X;
    Size.X += Global->Style.ContentsPadding.X * Global->FontSize;

    Widget->Position = Global->Cursor;

    Global->Cursor.Y += Size.Height;
    Global->CurrentWindow->Size.Width = RR_MAX(
        Global->CurrentWindow->Size.Width,
        Size.Width +
            (Global->Style.ContentsPadding.X * Global->FontSize * 2.0f));
    Global->CurrentWindow->Size.Height += Size.Height;
}

void Rr_BeginHorizontal(void)
{
}

void Rr_EndHorizontal(void)
{
}

Rr_UIContext *Rr_CreateUIContext(Rr_App *App)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_UIContext *Context = RR_ALLOC(Arena, sizeof(Rr_UIContext));
    Context->Arena = Arena;

    /* Context->FontSize = 12.0f; */
    Context->FontSize = 24.0f;

    Context->Style = (Rr_UIStyle){
        .TitlePadding = { 0.5f, 0.25f },
        .ContentsPadding = { 0.5f, 0.25f },
        .Foreground = Rr_U32ToRGBA(0xD6D0B3FF),
        .Background = Rr_U32ToRGBA(0x292F33FA),
        .TitleBackground = Rr_U32ToRGBA(0xD54251FA),
        .Outline = Rr_U32ToRGBA(0x6C6F72FA),
    };

    Rr_PipelineBinding Bindings[] = {
        { 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        { 1, 1, RR_PIPELINE_BINDING_TYPE_COMBINED_IMAGE_SAMPLER },
    };
    Rr_PipelineBindingSet BindingSets[] = {
        {
            RR_ARRAY_COUNT(Bindings),
            Bindings,
            RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    Context->PipelineLayout = Rr_CreatePipelineLayout(
        Renderer,
        RR_ARRAY_COUNT(BindingSets),
        BindingSets);

    Rr_ColorTargetInfo ColorTargets[] = {
        {
            .Format = Rr_GetSwapchainFormat(Renderer),
            .Blend.BlendEnable = true,
            .Blend.SrcColorBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA,
            .Blend.DstColorBlendFactor = RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .Blend.ColorBlendOp = RR_BLEND_OP_ADD,
            .Blend.SrcAlphaBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA,
            .Blend.DstAlphaBlendFactor = RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .Blend.AlphaBlendOp = RR_BLEND_OP_ADD,
        },
    };

    Rr_VertexInputAttribute VertexInputAttributes[] = {
        {
            .Location = 0,
            .Format = RR_FORMAT_VEC2,
        },
        {
            .Location = 1,
            .Format = RR_FORMAT_VEC2,
        },
        {
            .Location = 2,
            .Format = RR_FORMAT_VEC4,
        },
    };

    Rr_VertexInputBinding VertexInputBinding = {
        .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
        .AttributeCount = RR_ARRAY_COUNT(VertexInputAttributes),
        .Attributes = VertexInputAttributes,
    };

    Rr_GraphicsPipelineCreateInfo PipelineInfo = {
        .Layout = Context->PipelineLayout,
        .VertexShaderSPV = Rr_LoadAsset(RR_BUILTIN_UI_VERT_SPV),
        .FragmentShaderSPV = Rr_LoadAsset(RR_BUILTIN_UI_FRAG_SPV),
        .ColorTargetCount = RR_ARRAY_COUNT(ColorTargets),
        .ColorTargets = ColorTargets,
        .VertexInputBindingCount = 1,
        .VertexInputBindings = &VertexInputBinding,
    };

    Context->GraphicsPipeline =
        Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

    Context->VertexBuffer = Rr_CreateBuffer(
        Renderer,
        RR_MEGABYTES(8),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    Context->IndexBuffer = Rr_CreateBuffer(
        Renderer,
        RR_MEGABYTES(8),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    Context->UniformBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(Rr_UIUniformData),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    Context->Sampler = Rr_CreateSampler(
        Renderer,
        &(Rr_SamplerInfo){
            .MinFilter = RR_FILTER_LINEAR,
            .MagFilter = RR_FILTER_LINEAR,
        });

    return Context;
}

void Rr_DestroyUIContext(Rr_App *App, Rr_UIContext *Context)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);
    Rr_DestroyBuffer(Renderer, Context->VertexBuffer);
    Rr_DestroyBuffer(Renderer, Context->IndexBuffer);
    Rr_DestroyBuffer(Renderer, Context->UniformBuffer);
    Rr_DestroySampler(Renderer, Context->Sampler);
    Rr_DestroyPipelineLayout(Renderer, Context->PipelineLayout);
    Rr_DestroyGraphicsPipeline(Renderer, Context->GraphicsPipeline);
    Rr_DestroyArena(Context->Arena);
}

void Rr_BeginUI(Rr_App *App, Rr_UIContext *Context)
{
    assert(Global == NULL);

    Rr_Renderer *Renderer = Rr_GetRenderer(App);
    // UI->Arena->Position = sizeof(Rr_UI);
    Global = Context;
    Global->FrameNumber = Renderer->FrameNumber;
    Global->FrameArena = Rr_GetCurrentFrame(Renderer)->Arena;

    RR_ZERO(Global->Windows);
    RR_RESERVE_SLICE(
        &Global->Windows,
        Global->LastWindowCount,
        Global->FrameArena);
    Global->CurrentWindow = NULL;

    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
    Global->ScreenSize.Width = (float)SwapchainSize.Width;
    Global->ScreenSize.Height = (float)SwapchainSize.Height;

    Global->Font = Renderer->BuiltinFont;
}

static inline void Rr_DrawQuad(Rr_UIVertex *Vertices)
{
    /* @TODO: Bounds checking! */

    Rr_UIIndex Base =
        (Rr_UIIndex)(Global->VertexBufferData - Global->VertexBufferDataStart);
    Rr_UIIndex Indices[] = {
        Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2,
    };
    memcpy(Global->IndexBufferData, Indices, sizeof(Indices));
    Global->IndexBufferData += 6;

    memcpy(Global->VertexBufferData, Vertices, sizeof(Rr_UIVertex) * 4);
    Global->VertexBufferData += 4;
}

static inline void Rr_DrawSolidQuad(
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    Rr_Vec4 *Color)
{
    Rr_UIVertex Vertices[] = {
        {
            .Position = Position,
            .UV = (Rr_Vec2){ 0.0f, 0.0f },
            .Color = *Color,
        },
        {
            .Position = { Position.X + Size.X, Position.Y },
            .UV = (Rr_Vec2){ 0.0f, 0.0f },
            .Color = *Color,
        },
        {
            .Position = { Position.X, Position.Y + Size.Y },
            .UV = (Rr_Vec2){ 0.0f, 0.0f },
            .Color = *Color,
        },
        {
            .Position = { Position.X + Size.X, Position.Y + Size.Y },
            .UV = (Rr_Vec2){ 0.0f, 0.0f },
            .Color = *Color,
        },
    };

    Rr_DrawQuad(Vertices);
}

static inline void Rr_DrawFrameQuad(
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    Rr_Vec4 *Color)
{
    float FrameThickness = Rr_GetFrameThickness();
    Rr_DrawSolidQuad(
        (Rr_Vec2){ Position.X, Position.Y - FrameThickness },
        (Rr_Vec2){ Size.X, FrameThickness },
        Color);
    Rr_DrawSolidQuad(
        (Rr_Vec2){ Position.X, Position.Y + Size.Y },
        (Rr_Vec2){ Size.X, FrameThickness },
        Color);
    Rr_DrawSolidQuad(
        (Rr_Vec2){ Position.X - FrameThickness, Position.Y - FrameThickness },
        (Rr_Vec2){ FrameThickness, Size.Y + FrameThickness * 2.0f },
        Color);
    Rr_DrawSolidQuad(
        (Rr_Vec2){ Position.X + Size.X, Position.Y - FrameThickness },
        (Rr_Vec2){ FrameThickness, Size.Y + FrameThickness * 2.0f },
        Color);
}

static inline void Rr_DrawTexturedQuad(
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    Rr_Vec4 *Color,
    Rr_Vec2 *UVs)
{
    Rr_UIVertex Vertices[] = {
        {
            .Position = Position,
            .UV = UVs[0],
            .Color = *Color,
        },
        {
            .Position = { Position.X + Size.X, Position.Y },
            .UV = UVs[1],
            .Color = *Color,
        },
        {
            .Position = { Position.X, Position.Y + Size.Y },
            .UV = UVs[2],
            .Color = *Color,
        },
        {
            .Position = { Position.X + Size.X, Position.Y + Size.Y },
            .UV = UVs[3],
            .Color = *Color,
        },
    };

    Rr_DrawQuad(Vertices);
}

static inline void Rr_DrawText(
    Rr_Vec2 Position,
    Rr_String String,
    Rr_Vec4 *Color)
{
    Rr_Font *Font = Global->Font;
    float FontSize = Global->FontSize;
    float LineHeight = Font->LineHeight * FontSize;
    float CurrentX = 0.0f;
    float CurrentY = 0.0f;
    for(size_t Index = 0; Index < String.Length; ++Index)
    {
        uint32_t Codepoint = String.Data[Index];

        if(Codepoint >= RR_TEXT_MAX_GLYPHS)
        {
            RR_ABORT("Codepoint is not within range!");
        }

        if(Codepoint == '\n')
        {
            CurrentX = 0.0f;
            CurrentY += LineHeight;
            continue;
        }

        if(Codepoint == ' ')
        {
            CurrentX += Global->Font->Advances[Codepoint] * FontSize;
            continue;
        }

        Rr_Glyph *Glyph = &Font->Glyphs[Codepoint];

        float Left = Glyph->PlaneBounds.X * FontSize;
        float Width = (Glyph->PlaneBounds.Z - Glyph->PlaneBounds.X) * FontSize;

        float Top = (1.0f - Glyph->PlaneBounds.W) * FontSize;
        float Height = (Glyph->PlaneBounds.W - Glyph->PlaneBounds.Y) * FontSize;

        Rr_Vec2 UVs[] = {
            { Glyph->AtlasBounds.X, Glyph->AtlasBounds.W },
            { Glyph->AtlasBounds.Z, Glyph->AtlasBounds.W },
            { Glyph->AtlasBounds.X, Glyph->AtlasBounds.Y },
            { Glyph->AtlasBounds.Z, Glyph->AtlasBounds.Y },
        };

        Rr_DrawTexturedQuad(
            Rr_AddV2(Position, (Rr_Vec2){ CurrentX + Left, CurrentY + Top }),
            (Rr_Vec2){ Width, Height },
            Color,
            UVs);

        CurrentX += Global->Font->Advances[Codepoint] * FontSize;
    }
}

void Rr_DrawWidgets(Rr_UIWindow *Window)
{
    float LineHeight = Global->FontSize * Global->Font->LineHeight;
    float FrameThickness = Rr_GetFrameThickness();
    Rr_Vec2 ContentsPadding =
        Rr_MulV2F(Global->Style.ContentsPadding, Global->FontSize);
    Rr_Vec2 CheckboxSize = Rr_GetCheckboxSize();

    for(Rr_UIWidget *Widget = Window->FirstWidget; Widget != NULL;
        Widget = Widget->Next)
    {
        switch(Widget->Type)
        {
            case RR_UI_WIDGET_TYPE_CHECKBOX:
            {
                Rr_Vec2 CheckboxPosition = Rr_AddV2(
                    Widget->Position,
                    (Rr_Vec2){
                        FrameThickness,
                        LineHeight / 2.0f - CheckboxSize.Y / 2.0f,
                    });
                Rr_DrawFrameQuad(
                    CheckboxPosition,
                    CheckboxSize,
                    &Global->Style.Foreground);
                Rr_Vec2 TextPosition = Rr_AddV2(
                    Widget->Position,
                    (Rr_Vec2){ ContentsPadding.X + CheckboxSize.X, 0.0f });
                Rr_DrawText(
                    TextPosition,
                    Widget->Text,
                    &Global->Style.Foreground);
            }
            break;
            case RR_UI_WIDGET_TYPE_SEPARATOR:
            {
                Rr_Vec2 Size = {
                    Window->Size.X -
                        (Global->Style.ContentsPadding.X * Global->FontSize *
                         2.0f) -
                        (Window->Size.X * 0.1f),
                    Rr_GetFrameThickness(),
                };
                Rr_Vec2 Position = {
                    Widget->Position.X + (Window->Size.X * 0.05f),
                    Widget->Position.Y + (Rr_GetSeperatorLineHeight() / 2.0f -
                                          Rr_GetFrameThickness() / 2.0f),
                };
                Rr_Vec4 Color = Rr_MulV4F(Global->Style.Foreground, 0.85f);
                Rr_DrawSolidQuad(Position, Size, &Color);
            }
            break;
            case RR_UI_WIDGET_TYPE_LABEL:
            {
                Rr_DrawText(
                    Widget->Position,
                    Widget->Text,
                    &Global->Style.Foreground);
            }
            break;
            case RR_UI_WIDGET_TYPE_BUTTON:
            {
                RR_ABORT("Not implemented!");
            }
            break;
            default:
                break;
        }
    }
}

void Rr_EndUI(Rr_App *App)
{
    assert(Global != NULL);

    Rr_Renderer *Renderer = Rr_GetRenderer(App);
    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

    Rr_UIUniformData UniformData = {
        .ScreenSize = Global->ScreenSize,
        .DistanceRange = Global->Font->DistanceRange,
        .Time = Rr_GetTimeSeconds(App),
    };

    memcpy(
        Rr_GetMappedBufferData(Renderer, Global->UniformBuffer),
        &UniformData,
        sizeof(UniformData));

    Global->VertexBufferData = Global->VertexBufferDataStart =
        Rr_GetMappedBufferData(Renderer, Global->VertexBuffer);

    Global->IndexBufferData = Global->IndexBufferDataStart =
        Rr_GetMappedBufferData(Renderer, Global->IndexBuffer);

    Global->LastWindowCount = Global->Windows.Count;
    for(size_t Index = 0; Index < Global->Windows.Count; ++Index)
    {
        Rr_UIWindow *Window = Global->Windows.Data[Index];

        Rr_DrawFrameQuad(
            Window->Position,
            Window->Size,
            &Global->Style.Outline);

        Rr_Vec2 TitlePosition = Window->Position;
        Rr_Vec2 TitleSize = {
            Window->Size.X,
            Rr_GetWindowTitleHeight(),
        };
        Rr_DrawSolidQuad(
            TitlePosition,
            TitleSize,
            &Global->Style.TitleBackground);

        Rr_DrawText(
            Rr_AddV2(
                TitlePosition,
                Rr_MulV2F(Global->Style.TitlePadding, Global->FontSize)),
            Window->Title,
            &Global->Style.Foreground);

        Rr_Vec2 ContentsPosition = {
            Window->Position.X,
            Window->Position.Y + Rr_GetWindowTitleHeight(),
        };
        Rr_Vec2 ContentsSize = {
            Window->Size.X,
            Window->Size.Y - Rr_GetWindowTitleHeight(),
        };
        Rr_DrawSolidQuad(
            ContentsPosition,
            ContentsSize,
            &Global->Style.Background);

        Rr_DrawWidgets(Window);
    }

    if(Global->Windows.Count > 0)
    {
        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
        };

        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
            Renderer,
            "ui",
            1,
            &ColorTarget,
            &SwapchainImage,
            NULL,
            NULL);
        Rr_BindGraphicsPipeline(GraphicsNode, Global->GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, Global->VertexBuffer, 0, 0);
        Rr_BindIndexBuffer(
            GraphicsNode,
            Global->IndexBuffer,
            0,
            0,
            RR_INDEX_TYPE_UINT16);
        Rr_BindUniformBuffer(
            GraphicsNode,
            Global->UniformBuffer,
            0,
            0,
            0,
            sizeof(Rr_UIUniformData));
        Rr_BindCombinedImageSampler(
            GraphicsNode,
            Global->Font->Atlas,
            Global->Sampler,
            0,
            1);
        Rr_DrawIndexed(
            GraphicsNode,
            (size_t)(Global->IndexBufferData - Global->IndexBufferDataStart),
            1,
            0,
            0,
            0);
    }

    Global = NULL;

    // Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);
    // Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
    // Rr_GraphNode *Node = Rr_AddGraphicsNode(Renderer);
}
