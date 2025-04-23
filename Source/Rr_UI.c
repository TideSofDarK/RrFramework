#include "Rr_BuiltinAssets.inc"

#include "Rr_UI.h"

#include "Rr_Log.h"
#include "Rr_Renderer.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Utility.h>

#include <xxHash/xxhash.h>

#include <cJSON/cJSON.h>

#include <assert.h>
#include <stdio.h>

#define RR_ASSERT_GLOBAL() \
    assert(Global != NULL && "Did you forget to call Rr_BeginUI()?")

#define RR_ASSERT_NO_WINDOW()            \
    RR_ASSERT_GLOBAL();                  \
    assert(                              \
        Global->CurrentWindow == NULL && \
        "Did you forget to call Rr_EndWindow()?")

#define RR_ASSERT_WIDGET() \
    RR_ASSERT_GLOBAL();    \
    assert(Global->CurrentWindow && "Did you forget to call Rr_BeginWindow()?")

typedef uint16_t Rr_UIIndex;

typedef struct Rr_UIVertex Rr_UIVertex;
struct Rr_UIVertex
{
    Rr_Vec2 Position;
    Rr_Vec2 UV;
    Rr_Vec4 Color;
};

typedef Rr_UIVertex *Rr_UIQuad; /* Implies 4 allocated vertices. */

typedef struct Rr_UIUniformData Rr_UIUniformData;
struct Rr_UIUniformData
{
    Rr_Vec2 ScreenSize;
    float DistanceRange;
    float Time;
};

typedef struct Rr_UIWindow Rr_UIWindow;
struct Rr_UIWindow
{
    Rr_String Title;
    Rr_Vec2 Position;
    Rr_Vec2 Size;
    uint32_t LastVertexCount;
    RR_SLICE(Rr_UIVertex) Vertices;
    uint32_t LastIndexCount;
    RR_SLICE(Rr_UIIndex) Indices;
    Rr_Map *WidgetMap;
    size_t LastFrameNumber;
    Rr_UIWindowFlags Flags;
    bool Minimized;
};

struct Rr_UIContext
{
    size_t FrameNumber;

    Rr_UIStyle Style;

    Rr_Map *WindowMap;
    RR_SLICE(Rr_UIWindow *) Windows;
    size_t LastWindowCount;
    Rr_UIWindow *CurrentWindow;
    Rr_UIWindow *HoveredWindow;

    Rr_Vec2 ScreenSize;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    Rr_Font *Font;
    float FontSize;

    Rr_Vec2 Cursor;

    Rr_Buffer *VertexBuffer;
    Rr_Buffer *IndexBuffer;

    Rr_Buffer *UniformBuffer;

    Rr_Sampler *Sampler;

    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UIContext *Global;

static inline float Rr_GetSeperatorLineHeight(void)
{
    return Global->FontSize * Global->Font->LineHeight * 0.5f;
}

static inline float Rr_GetFrameThickness(void)
{
    return roundf(RR_MAX(1.0f, Global->FontSize * 0.05f));
}

static float Rr_GetWindowTitleHeight(void)
{
    return Global->Style.TitlePadding.Y * Global->FontSize * 2.0f +
           Global->Font->LineHeight * Global->FontSize;
}

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

static inline Rr_UIQuad Rr_ReserveQuad(Rr_UIWindow *Window)
{
    /* @TODO: Bounds checking! */

    Rr_UIIndex Base = Window->Vertices.Count;
    Rr_UIIndex Indices[] = {
        Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2,
    };

    Rr_UIQuad ReservedQuad = Window->Vertices.Data + Window->Vertices.Count;
    for(size_t Index = 0; Index < 4; ++Index)
    {
        RR_PUSH_SLICE(&Window->Vertices, Global->FrameArena);
    }

    for(size_t Index = 0; Index < 6; ++Index)
    {
        *RR_PUSH_SLICE(&Window->Indices, Global->FrameArena) = Indices[Index];
    }

    return ReservedQuad;
}

static inline void Rr_DrawQuad(Rr_UIWindow *Window, Rr_UIVertex *Vertices)
{
    Rr_UIIndex Base = Window->Vertices.Count;
    Rr_UIIndex Indices[] = {
        Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2,
    };
    for(size_t Index = 0; Index < 4; ++Index)
    {
        *RR_PUSH_SLICE(&Window->Vertices, Global->FrameArena) = Vertices[Index];
    }

    for(size_t Index = 0; Index < 6; ++Index)
    {
        *RR_PUSH_SLICE(&Window->Indices, Global->FrameArena) = Indices[Index];
    }
}

static inline void Rr_SolidQuad(
    Rr_UIQuad Quad,
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    Rr_Vec4 *Color)
{
    memcpy(
        Quad,
        (Rr_UIVertex[]){
            {
                .Position = Position,
                .Color = *Color,
            },
            {
                .Position = { Position.X + Size.X, Position.Y },
                .Color = *Color,
            },
            {
                .Position = { Position.X, Position.Y + Size.Y },
                .Color = *Color,
            },
            {
                .Position = { Position.X + Size.X, Position.Y + Size.Y },
                .Color = *Color,
            },
        },
        sizeof(Rr_UIVertex) * 4);
}

static inline void Rr_DrawSolidTriangle(
    Rr_UIWindow *Window,
    Rr_Vec2 PositionA,
    Rr_Vec2 PositionB,
    Rr_Vec2 PositionC,
    Rr_Vec4 *Color)
{
    Rr_UIVertex Vertices[3] = {
        {
            .Position = PositionA,
            .Color = *Color,
        },
        {
            .Position = PositionB,
            .Color = *Color,
        },
        {
            .Position = PositionC,
            .Color = *Color,
        },
    };
    for(size_t Index = 0; Index < 3; ++Index)
    {
        *RR_PUSH_SLICE(&Window->Indices, Global->FrameArena) =
            Window->Vertices.Count + Index;
    }

    for(size_t Index = 0; Index < 3; ++Index)
    {
        *RR_PUSH_SLICE(&Window->Vertices, Global->FrameArena) = Vertices[Index];
    }
}

static inline void Rr_DrawSolidQuad(
    Rr_UIWindow *Window,
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    Rr_Vec4 *Color)
{
    Rr_UIVertex Vertices[4];
    Rr_SolidQuad(Vertices, Position, Size, Color);
    Rr_DrawQuad(Window, Vertices);
}

static inline void Rr_DrawFrameQuad(
    Rr_UIWindow *Window,
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    Rr_Vec4 *Color)
{
    float FrameThickness = Rr_GetFrameThickness();
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X, Position.Y },
        (Rr_Vec2){ Size.X, FrameThickness },
        Color); /* Top */
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X, Position.Y + Size.Y - FrameThickness },
        (Rr_Vec2){ Size.X, FrameThickness },
        Color); /* Bottom */
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X, Position.Y + FrameThickness },
        (Rr_Vec2){ FrameThickness, Size.Y - FrameThickness * 2.0f },
        Color); /* Left */
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X + Size.X - FrameThickness,
                   Position.Y + FrameThickness },
        (Rr_Vec2){ FrameThickness, Size.Y - FrameThickness * 2.0f },
        Color); /* Right */
}

static inline void Rr_DrawTexturedQuad(
    Rr_UIWindow *Window,
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

    Rr_DrawQuad(Window, Vertices);
}

static inline void Rr_DrawText(
    Rr_UIWindow *Window,
    Rr_Vec2 Position,
    Rr_String *String,
    Rr_Vec4 *Color)
{
    Rr_Font *Font = Global->Font;
    float FontSize = Global->FontSize;
    float LineHeight = Font->LineHeight * FontSize;
    float CurrentX = 0.0f;
    float CurrentY = 0.0f;
    for(size_t Index = 0; Index < String->Length; ++Index)
    {
        uint32_t Codepoint = String->Data[Index];

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
            Window,
            Rr_AddV2(Position, (Rr_Vec2){ CurrentX + Left, CurrentY + Top }),
            (Rr_Vec2){ Width, Height },
            Color,
            UVs);

        CurrentX += Global->Font->Advances[Codepoint] * FontSize;
    }
}

void Rr_BeginWindow(const char *Title, Rr_UIWindowFlags Flags)
{
    RR_ASSERT_NO_WINDOW();

    size_t TitleLength = strlen(Title);
    XXH64_hash_t Hash = XXH3_64bits(Title, TitleLength);

    Rr_UIWindow **WindowRef =
        RR_UPSERT(&Global->WindowMap, Hash, Global->Arena);
    Rr_UIWindow *Window = *WindowRef;

    if(Window == NULL)
    {
        Window = RR_ALLOC_TYPE(Global->Arena, Rr_UIWindow);
        Window->Title = Rr_CreateString(Title, TitleLength, Global->Arena);
        Window->Position = (Rr_Vec2){
            .X = Global->FontSize,
            .Y = Global->FontSize,
        };
        const float DEFAULT_WINDOW_WIDTH = 300;
        const float DEFAULT_WINDOW_HEIGHT = 500;
        Window->Size = (Rr_Vec2){
            .X = Global->Style.ContentsPadding.X * 2.0f * Global->FontSize +
                 DEFAULT_WINDOW_WIDTH,
            .Y = Global->Style.ContentsPadding.Y * 2.0f * Global->FontSize +
                 Rr_GetWindowTitleHeight() + DEFAULT_WINDOW_HEIGHT,
        };
        *WindowRef = Window;
    }
    else
    {
        assert(Window->LastFrameNumber != Global->FrameNumber);
    }

    Window->Flags = Flags;
    Window->LastFrameNumber = Global->FrameNumber;

    *RR_PUSH_SLICE(&Global->Windows, Global->FrameArena) = Window;

    RR_ZERO(Window->Vertices);
    RR_RESERVE_SLICE(
        &Window->Vertices,
        Window->LastVertexCount ? Window->LastVertexCount : (4 * 32),
        Global->FrameArena);

    RR_ZERO(Window->Indices);
    RR_RESERVE_SLICE(
        &Window->Indices,
        Window->LastIndexCount ? Window->LastIndexCount : (6 * 32),
        Global->FrameArena);

    Rr_UIQuad _ =
        Rr_ReserveQuad(Window); /* Reserved contents background quad. */
    (void)_;

    Global->Cursor = (Rr_Vec2){
        .X = Window->Position.X +
             Global->Style.ContentsPadding.X * Global->FontSize,
        .Y = Window->Position.Y +
             Global->Style.ContentsPadding.Y * Global->FontSize +
             Rr_GetWindowTitleHeight(),
    };

    Global->CurrentWindow = Window;
}

void Rr_EndWindow(void)
{
    RR_ASSERT_WIDGET();

    Rr_UIWindow *Window = Global->CurrentWindow;
    Window->LastVertexCount = Window->Vertices.Count;
    Window->LastIndexCount = Window->Indices.Count;
    Global->CurrentWindow = NULL;
}

void Rr_Separator(void)
{
    RR_ASSERT_WIDGET();

    Rr_UIWindow *Window = Global->CurrentWindow;

    float SeparatorLineHeight = Rr_GetSeperatorLineHeight();
    float FrameThickness = Rr_GetFrameThickness();

    /* Rr_Vec2 Size = { */
    /*     Window->Size.X - */
    /*         (Global->Style.ContentsPadding.X * Global->FontSize * 2.0f) - */
    /*         (Window->Size.X * 0.1f), */
    /*     FrameThickness, */
    /* }; */
    /* Rr_Vec2 Position = { */
    /*     Global->Cursor.X + (Window->Size.X * 0.05f), */
    /*     Global->Cursor.Y + (SeparatorLineHeight / 2.0f - FrameThickness
     * / 2.0f), */
    /* }; */
    Rr_Vec2 Size = {
        Window->Size.X -
            (Global->Style.ContentsPadding.X * Global->FontSize * 2.0f),
        FrameThickness,
    };
    Rr_Vec2 Position = {
        Global->Cursor.X,
        Global->Cursor.Y + (SeparatorLineHeight / 2.0f - FrameThickness / 2.0f),
    };
    Rr_Vec4 Color = Rr_MulV4F(Global->Style.Foreground, 0.75f);
    Rr_DrawSolidQuad(Window, Position, Size, &Color);

    Global->Cursor.Y += SeparatorLineHeight;
    /* Window->Size.Height += SeparatorLineHeight; */
}

void Rr_Label(const char *Text)
{
    RR_ASSERT_WIDGET();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = Global->CurrentWindow;

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextSize =
        Rr_CalculateTextSize(Global->Font, Global->FontSize, &TextString);

    Rr_DrawText(Window, Global->Cursor, &TextString, &Global->Style.Foreground);

    Global->Cursor.Y += TextSize.Height;
    /* Window->Size.Width = RR_MAX( */
    /*     Window->Size.Width, */
    /*     TextSize.Width + */
    /*         (Global->Style.ContentsPadding.X * Global->FontSize * 2.0f)); */
    /* Window->Size.Height += TextSize.Height; */

    Rr_DestroyScratch(Scratch);
}

void Rr_LabelF(const char *Format, ...)
{
    int BufferSize;
    va_list Args;

    va_start(Args, Format);
    BufferSize = vsnprintf(NULL, 0, Format, Args);
    va_end(Args);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    char *Buffer = RR_ALLOC_NO_ZERO(Scratch.Arena, BufferSize + 1);

    va_start(Args, Format);
    BufferSize = vsnprintf(Buffer, BufferSize + 1, Format, Args);
    va_end(Args);

    Rr_Label(Buffer);

    Rr_DestroyScratch(Scratch);
}

void Rr_Button(const char *Text)
{
}

static inline Rr_Vec2 Rr_GetCheckboxSize(void)
{
    float Size = Global->FontSize * Global->Font->LineHeight * 0.6f;
    return (Rr_Vec2){ Size, Size };
}

void Rr_Checkbox(const char *Text, bool *Checked)
{
    RR_ASSERT_WIDGET();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = Global->CurrentWindow;

    float FrameThickness = Rr_GetFrameThickness();
    float LineHeight = Global->FontSize * Global->Font->LineHeight;
    Rr_Vec2 ContentsPadding =
        Rr_MulV2F(Global->Style.ContentsPadding, Global->FontSize);
    Rr_Vec2 CheckboxSize = Rr_GetCheckboxSize();

    Rr_Vec2 CheckboxPosition = Rr_AddV2(
        Global->Cursor,
        (Rr_Vec2){
            FrameThickness,
            LineHeight / 2.0f - CheckboxSize.Y / 2.0f,
        });
    Rr_DrawFrameQuad(
        Window,
        CheckboxPosition,
        CheckboxSize,
        &Global->Style.Foreground);

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextSize =
        Rr_CalculateTextSize(Global->Font, Global->FontSize, &TextString);

    Rr_Vec2 TextPosition = Rr_AddV2(
        Global->Cursor,
        (Rr_Vec2){ ContentsPadding.X + CheckboxSize.X, 0.0f });
    Rr_DrawText(Window, TextPosition, &TextString, &Global->Style.Foreground);

    float YOffset = RR_MAX(CheckboxSize.Y, TextSize.Y);
    Global->Cursor.Y += YOffset;
    /* Window->Size.Width = RR_MAX( */
    /*     Window->Size.Width, */
    /*     (TextSize.Width + CheckboxSize.Width) + */
    /*         (Global->Style.ContentsPadding.X * Global->FontSize * 2.0f)); */
    /* Window->Size.Height += YOffset; */

    Rr_DestroyScratch(Scratch);
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

    Global->HoveredWindow = NULL;
    for(int Index = Global->Windows.Count - 1; Index >= 0; --Index)
    {
    }

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

void Rr_EndUI(Rr_App *App)
{
    RR_ASSERT_NO_WINDOW();

    Global->LastWindowCount = Global->Windows.Count;

    if(Global->Windows.Count == 0)
    {
        Global = NULL;
        return;
    }

    Rr_Renderer *Renderer = Rr_GetRenderer(App);
    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

    Rr_UIUniformData UniformData = {
        .ScreenSize = Global->ScreenSize,
        .DistanceRange = Global->Font->DistanceRange,
        .Time = Rr_GetTimeSeconds(App),
    };
    char *MappedUniformData =
        Rr_GetMappedBufferData(Renderer, Global->UniformBuffer);
    memcpy(MappedUniformData, &UniformData, sizeof(UniformData));

    Rr_UIVertex *VertexBufferDataStart;
    Rr_UIVertex *VertexBufferData;
    VertexBufferData = VertexBufferDataStart =
        Rr_GetMappedBufferData(Renderer, Global->VertexBuffer);

    Rr_UIIndex *IndexBufferDataStart;
    Rr_UIIndex *IndexBufferData;
    IndexBufferData = IndexBufferDataStart =
        Rr_GetMappedBufferData(Renderer, Global->IndexBuffer);

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

    const float ResizeHandleSize = Global->FontSize * 0.75f;

    for(size_t Index = 0; Index < Global->Windows.Count; ++Index)
    {
        Rr_UIWindow *Window = Global->Windows.Data[Index];

        /* Contents background quad is reserved first. */

        Rr_Vec2 ContentsPosition = {
            Window->Position.X,
            Window->Position.Y + Rr_GetWindowTitleHeight(),
        };
        Rr_Vec2 ContentsSize = {
            Window->Size.X,
            Window->Size.Y - Rr_GetWindowTitleHeight(),
        };
        Rr_UIQuad ContentsBackgroundQuad = Window->Vertices.Data;
        Rr_SolidQuad(
            ContentsBackgroundQuad,
            ContentsPosition,
            ContentsSize,
            &Global->Style.Background);

        Rr_Vec2 TitlePosition = Window->Position;
        Rr_Vec2 TitleSize = {
            Window->Size.X,
            Rr_GetWindowTitleHeight(),
        };
        Rr_DrawSolidQuad(
            Window,
            TitlePosition,
            TitleSize,
            &Global->Style.TitleBackground);
        Rr_DrawText(
            Window,
            Rr_AddV2(
                TitlePosition,
                Rr_MulV2F(Global->Style.TitlePadding, Global->FontSize)),
            &Window->Title,
            &Global->Style.Foreground);

        Rr_DrawFrameQuad(
            Window,
            Window->Position,
            Window->Size,
            &Global->Style.Outline);

        if(RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT) == false)
        {
            Rr_Vec2 BottomRight = Rr_AddV2(Window->Position, Window->Size);
            Rr_DrawSolidTriangle(
                Window,
                (Rr_Vec2){ BottomRight.X - ResizeHandleSize, BottomRight.Y },
                (Rr_Vec2){ BottomRight.X, BottomRight.Y - ResizeHandleSize },
                (Rr_Vec2){ BottomRight.X, BottomRight.Y },
                &Global->Style.Foreground);
        }

        /* Finished generating geometry. */

        int32_t VertexOffset =
            (int32_t)(VertexBufferData - VertexBufferDataStart);

        size_t FirstIndex = (size_t)(IndexBufferData - IndexBufferDataStart);

        memcpy(
            VertexBufferData,
            Window->Vertices.Data,
            sizeof(Rr_UIVertex) * Window->Vertices.Count);
        VertexBufferData += Window->Vertices.Count;

        memcpy(
            IndexBufferData,
            Window->Indices.Data,
            sizeof(Rr_UIIndex) * Window->Indices.Count);
        IndexBufferData += Window->Indices.Count;

        Rr_SetScissor(
            GraphicsNode,
            (Rr_IntVec4){
                (int)Window->Position.X,
                (int)Window->Position.Y,
                (int)ceilf(Window->Size.X),
                (int)ceilf(Window->Size.Y),
            });

        Rr_DrawIndexed(
            GraphicsNode,
            Window->Indices.Count,
            1,
            FirstIndex,
            VertexOffset,
            0);
    }

    Global = NULL;
}
