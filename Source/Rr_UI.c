#include "Rr_BuiltinAssets.inc"

#include "Rr_UI.h"

#include "Rr_App.h"
#include "Rr_Log.h"
#include "Rr_Renderer.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Utility.h>

#include <xxHash/xxhash.h>

#include <cJSON/cJSON.h>

#include <assert.h>
#include <stdio.h>

#define RR_UI_ASSERT_GLOBAL() \
    assert(Global != NULL && "Did you forget to call Rr_BeginUI()?")

#define RR_UI_ASSERT_NO_WINDOW()         \
    RR_UI_ASSERT_GLOBAL();               \
    assert(                              \
        Global->CurrentWindow == NULL && \
        "Did you forget to call Rr_EndWindow()?")

#define RR_UI_ASSERT_WINDOW()            \
    RR_UI_ASSERT_GLOBAL();               \
    assert(                              \
        Global->CurrentWindow != NULL && \
        "Did you forget to call Rr_BeginWindow()?")

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
    Rr_UIWindowFlags Flags;
    int ZOrder;
    bool Minimized;
    bool Added;
};

struct Rr_UIContext
{
    size_t FrameNumber;

    Rr_UIStyle Style;

    Rr_Map *WindowMap;
    int TotalWindowCount;
    RR_SLICE(Rr_UIWindow *) Windows;
    size_t LastWindowCount;
    Rr_UIWindow *CurrentWindow;
    Rr_UIWindow *HoveredWindow;

    bool Horizontal;
    RR_SLICE(float) HorizontalX;
    float HorizontalMaxHeight;

    bool MouseButtonCapture;

    bool LeftMouseButtonDown;
    bool LeftMouseButtonHeld;
    bool LeftMouseButtonUp;
    Rr_Vec2 MousePosition;

    Rr_UIWindow *MovingWindow;
    Rr_Vec2 MovingStart;
    Rr_Vec2 MovingWindowStart;

    Rr_UIWindow *ResizingWindow;
    Rr_Vec2 ResizingStart;
    Rr_Vec2 ResizingWindowStart;

    Rr_Vec2 ScreenSize;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    RR_FREE_LIST(Rr_Font) Fonts;
    Rr_Font *Font;
    float FontSize;
    float NextFontSize;

    Rr_Vec2 ContentsPadding;
    float LineHeight;
    Rr_Vec2 MinWindowSize;
    Rr_Vec2 MinWindowSizeNoTitle;
    float WindowTitleHeight;
    float ResizeHandleSize;
    float HorizontalMargin;
    float FrameThickness;
    float SeparatorLineHeight;
    Rr_Vec2 ButtonPadding;
    Rr_Vec2 CheckboxSize;

    Rr_Vec2 Cursor;

    Rr_Buffer *VertexBuffer;
    Rr_Buffer *IndexBuffer;

    Rr_Buffer *UniformBuffer;

    Rr_Sampler *Sampler;

    Rr_App *App;
    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UIContext *Global;

#define CJSON_GET_OBJECT_FLOAT(Object, Item) \
    ((float)cJSON_GetNumberValue(cJSON_GetObjectItem(Object, Item)))

Rr_Font *Rr_CreateFont(
    Rr_UIContext *Context,
    Rr_AssetRef FontPNGRef,
    Rr_AssetRef FontJSONRef)
{
    Rr_Renderer *Renderer = Context->App->Renderer;

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

    cJSON *AtlasJSON = cJSON_GetObjectItem(FontDataJSON, "atlas");
    cJSON *MetricsJSON = cJSON_GetObjectItem(FontDataJSON, "metrics");

    Rr_Vec2 AtlasSize;
    AtlasSize.X = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "width");
    AtlasSize.Y = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "height");

    Rr_Font *Font = RR_GET_FREE_LIST_ITEM(&Context->Fonts, Context->Arena);
    *Font = (Rr_Font){
        .Atlas = Atlas,
        .LineHeight = CJSON_GET_OBJECT_FLOAT(MetricsJSON, "lineHeight"),
        .DefaultSize = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "size"),
        .DistanceRange = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "distanceRange"),
    };

    cJSON *GlyphsJSON = cJSON_GetObjectItem(FontDataJSON, "glyphs");

    size_t GlyphCount = cJSON_GetArraySize(GlyphsJSON);
    for(size_t GlyphIndex = 0; GlyphIndex < GlyphCount; ++GlyphIndex)
    {
        cJSON *GlyphJSON = cJSON_GetArrayItem(GlyphsJSON, (int32_t)GlyphIndex);

        uint32_t Codepoint = CJSON_GET_OBJECT_FLOAT(GlyphJSON, "unicode");

        Rr_Glyph *Glyph = &Font->Glyphs[Codepoint];

        cJSON *AtlasBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "atlasBounds");
        if(cJSON_IsObject(AtlasBoundsJSON))
        {
            Glyph->AtlasBounds.X =
                CJSON_GET_OBJECT_FLOAT(AtlasBoundsJSON, "left") / AtlasSize.X;
            Glyph->AtlasBounds.Y =
                1.0f -
                CJSON_GET_OBJECT_FLOAT(AtlasBoundsJSON, "bottom") / AtlasSize.Y;
            Glyph->AtlasBounds.Z =
                CJSON_GET_OBJECT_FLOAT(AtlasBoundsJSON, "right") / AtlasSize.X;
            Glyph->AtlasBounds.W =
                1.0f -
                CJSON_GET_OBJECT_FLOAT(AtlasBoundsJSON, "top") / AtlasSize.Y;
        }

        cJSON *PlaneBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "planeBounds");
        if(cJSON_IsObject(PlaneBoundsJSON))
        {
            Glyph->PlaneBounds.X =
                CJSON_GET_OBJECT_FLOAT(PlaneBoundsJSON, "left");
            Glyph->PlaneBounds.Y =
                CJSON_GET_OBJECT_FLOAT(PlaneBoundsJSON, "bottom");
            Glyph->PlaneBounds.Z =
                CJSON_GET_OBJECT_FLOAT(PlaneBoundsJSON, "right");
            Glyph->PlaneBounds.W =
                CJSON_GET_OBJECT_FLOAT(PlaneBoundsJSON, "top");
        }

        Font->Advances[Codepoint] =
            CJSON_GET_OBJECT_FLOAT(GlyphJSON, "advance");
    }

    cJSON_Delete(FontDataJSON);

    return Font;
}

void Rr_DestroyFont(Rr_UIContext *Context, Rr_Font *Font)
{
    Rr_DestroyImage(Context->App->Renderer, Font->Atlas);

    RR_RETURN_FREE_LIST_ITEM(&Context->Fonts, Font);
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
    /* TODO: Bounds checking! */

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
    float Thickness,
    Rr_Vec4 *Color)
{
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X, Position.Y },
        (Rr_Vec2){ Size.X, Thickness },
        Color); /* Top */
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X, Position.Y + Size.Y - Thickness },
        (Rr_Vec2){ Size.X, Thickness },
        Color); /* Bottom */
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X, Position.Y + Thickness },
        (Rr_Vec2){ Thickness, Size.Y - Thickness * 2.0f },
        Color); /* Left */
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X + Size.X - Thickness, Position.Y + Thickness },
        (Rr_Vec2){ Thickness, Size.Y - Thickness * 2.0f },
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

static inline Rr_Vec2 Rr_GetMinWindowSize(Rr_UIWindowFlags Flags)
{
    if(RR_HAS_BIT(Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT))
    {
        return Global->MinWindowSizeNoTitle;
    }
    else
    {
        return Global->MinWindowSize;
    }
}

void Rr_BeginWindow(const char *Title, Rr_UIWindowFlags Flags)
{
    RR_UI_ASSERT_NO_WINDOW();

    bool HasTitle = RR_HAS_BIT(Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT) == false;
    size_t TitleLength = strlen(Title);
    XXH64_hash_t Hash = XXH3_64bits(Title, TitleLength);

    Rr_UIWindow **WindowRef =
        RR_UPSERT(&Global->WindowMap, Hash, Global->Arena);
    Rr_UIWindow *Window = *WindowRef;

    if(Window == NULL)
    {
        Window = RR_ALLOC_TYPE(Global->Arena, Rr_UIWindow);
        Window->ZOrder = Global->TotalWindowCount++;
        Window->Title = Rr_CreateString(Title, TitleLength, Global->Arena);
        Window->Position = (Rr_Vec2){
            .X = Global->FontSize,
            .Y = Global->FontSize,
        };
        const Rr_Vec2 DEFAULT_WINDOW_SIZE = { 300.0f, 500.0f };
        Window->Size =
            Rr_AddV2(Rr_GetMinWindowSize(Flags), DEFAULT_WINDOW_SIZE);
        *WindowRef = Window;
    }
    else
    {
        assert(
            Window->Added == false &&
            "There already is a window with this title!");
    }

    Window->Flags = Flags;

    *RR_PUSH_SLICE(&Global->Windows, Global->FrameArena) = Window;
    Window->Added = true;

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

    Global->Cursor = Rr_AddV2(Window->Position, Global->ContentsPadding);
    if(HasTitle)
    {
        Global->Cursor.Y += Global->WindowTitleHeight;
    }

    Global->CurrentWindow = Window;
}

void Rr_EndWindow(void)
{
    RR_UI_ASSERT_WINDOW();

    Global->CurrentWindow = NULL;
}

void Rr_BeginHorizontal(void)
{
    Global->Horizontal = true;
    *RR_PUSH_SLICE(&Global->HorizontalX, Global->FrameArena) = Global->Cursor.X;
}

void Rr_EndHorizontal(void)
{
    Global->Horizontal = false;
    Global->Cursor.X = RR_POP_SLICE(&Global->HorizontalX);
    Global->Cursor.Y += Global->HorizontalMaxHeight;
}

static inline void Rr_Advance(Rr_Vec2 Size)
{
    if(Global->Horizontal)
    {
        Global->Cursor.X += Size.Width + Global->HorizontalMargin;
        Global->HorizontalMaxHeight =
            RR_MAX(Global->HorizontalMaxHeight, Size.Height);
    }
    else
    {
        Global->Cursor.Y += Size.Height;
    }
}

void Rr_Separator(void)
{
    RR_UI_ASSERT_WINDOW();

    Rr_UIWindow *Window = Global->CurrentWindow;

    Rr_Vec2 Size = {
        Window->Size.X - (Global->ContentsPadding.X * 2.0f),
        Global->FrameThickness,
    };
    Rr_Vec2 Position = {
        Global->Cursor.X,
        Global->Cursor.Y + (Global->SeparatorLineHeight / 2.0f -
                            Global->FrameThickness / 2.0f),
    };
    Rr_Vec4 Color = Rr_MulV4F(Global->Style.Foreground, 0.75f);
    Rr_DrawSolidQuad(Window, Position, Size, &Color);

    Global->Cursor.Y += Global->SeparatorLineHeight;
}

void Rr_Label(const char *Text)
{
    RR_UI_ASSERT_WINDOW();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = Global->CurrentWindow;

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextSize =
        Rr_CalculateTextSize(Global->Font, Global->FontSize, &TextString);

    Rr_DrawText(Window, Global->Cursor, &TextString, &Global->Style.Foreground);

    Rr_Advance(TextSize);

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

static inline void Rr_ButtonBehavior(
    Rr_UIWindow *Window,
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    bool *Clicked,
    bool *Hovered)
{
    if(Window == Global->HoveredWindow &&
       Rr_RectContains(Position, Size, Global->MousePosition))
    {
        if(Global->LeftMouseButtonDown)
        {
            Global->MovingWindow = NULL;
            if(Clicked)
            {
                *Clicked = true;
            }
        }
        else if(!Global->MovingWindow && !Global->ResizingWindow)
        {
            if(Hovered)
            {
                *Hovered = true;
            }
        }
    }
}

bool Rr_Button(const char *Text)
{
    RR_UI_ASSERT_WINDOW();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = Global->CurrentWindow;

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextSize =
        Rr_CalculateTextSize(Global->Font, Global->FontSize, &TextString);

    Rr_Vec2 ButtonSize =
        Rr_AddV2(TextSize, Rr_MulV2F(Global->ButtonPadding, 2.0f));
    Rr_Vec2 ButtonPosition = Global->Cursor;

    bool Clicked = false;
    bool Hovered = false;
    Rr_ButtonBehavior(Window, ButtonPosition, ButtonSize, &Clicked, &Hovered);

    Rr_DrawSolidQuad(
        Window,
        ButtonPosition,
        ButtonSize,
        &Global->Style.Foreground);

    if(Clicked)
    {
        Rr_Vec4 ClickedColor = Rr_MulV4F(Global->Style.Foreground, 0.5f);
        Rr_DrawSolidQuad(Window, ButtonPosition, ButtonSize, &ClickedColor);
    }
    else if(Hovered)
    {
        Rr_Vec4 HoveredColor = Rr_MulV4F(Global->Style.Foreground, 0.75f);
        Rr_DrawSolidQuad(Window, ButtonPosition, ButtonSize, &HoveredColor);
    }
    else
    {
        Rr_DrawSolidQuad(
            Window,
            ButtonPosition,
            ButtonSize,
            &Global->Style.Foreground);
    }

    Rr_Vec2 TextPosition = Rr_AddV2(ButtonPosition, Global->ButtonPadding);
    Rr_DrawText(Window, TextPosition, &TextString, &Global->Style.Background);

    Rr_Advance(ButtonSize);

    Rr_DestroyScratch(Scratch);

    return Clicked;
}

bool Rr_Checkbox(const char *Text, bool *Checked)
{
    RR_UI_ASSERT_WINDOW();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = Global->CurrentWindow;

    Rr_Vec2 ContentsPadding =
        Rr_MulV2F(Global->Style.ContentsPadding, Global->FontSize);

    Rr_Vec2 FramePosition = Rr_AddV2(
        Global->Cursor,
        (Rr_Vec2){
            Global->FrameThickness,
            Global->LineHeight / 2.0f - Global->CheckboxSize.Y / 2.0f,
        });

    bool Clicked = false;
    bool Hovered = false;
    Rr_ButtonBehavior(
        Window,
        FramePosition,
        Global->CheckboxSize,
        &Clicked,
        &Hovered);

    if(Clicked)
    {
        *Checked = !*Checked;
    }

    Rr_Vec4 Color = Rr_MulV4F(Global->Style.Foreground, Hovered ? 0.75f : 1.0f);

    Rr_DrawFrameQuad(
        Window,
        FramePosition,
        Global->CheckboxSize,
        Global->FrameThickness,
        &Color);

    if(*Checked)
    {
        Rr_Vec2 Inset = (Rr_Vec2){
            Global->FrameThickness * 4.0f,
            Global->FrameThickness * 4.0f,
        };
        Rr_Vec2 CheckmarkPosition = Rr_AddV2(FramePosition, Inset);
        Rr_Vec2 CheckmarkSize =
            Rr_SubV2(Global->CheckboxSize, Rr_MulV2F(Inset, 2.0f));
        Rr_DrawSolidQuad(Window, CheckmarkPosition, CheckmarkSize, &Color);
    }

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextSize =
        Rr_CalculateTextSize(Global->Font, Global->FontSize, &TextString);

    Rr_Vec2 TextPosition = Rr_AddV2(
        Global->Cursor,
        (Rr_Vec2){ ContentsPadding.X + Global->CheckboxSize.X, 0.0f });
    Rr_DrawText(Window, TextPosition, &TextString, &Global->Style.Foreground);

    Rr_Vec2 TotalSize = {
        Global->CheckboxSize.X + TextSize.X + ContentsPadding.X,
        RR_MAX(Global->CheckboxSize.Y, TextSize.Y),
    };
    Rr_Advance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return Clicked;
}

bool Rr_WantMouseCapture(void)
{
    return Global && (Global->MouseButtonCapture || Global->HoveredWindow);
}

bool Rr_WantKeyboardCapture(void)
{
    return false;
}

Rr_UIContext *Rr_CreateUIContext(Rr_App *App)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_UIContext *Context = RR_ALLOC(Arena, sizeof(Rr_UIContext));
    Context->App = App;
    Context->Arena = Arena;

    Context->NextFontSize =
        24.0f; /* TODO: Calculate default font size based on DPI. */

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

    Context->Font =
        Rr_CreateFont(Context, RR_BUILTIN_IOSEVKA_PNG, RR_BUILTIN_IOSEVKA_JSON);

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
    Rr_DestroyFont(Context, Context->Font);
    Rr_DestroyArena(Context->Arena);
}

void Rr_ProcessUIEvent(Rr_App *App, Rr_Event *Event)
{
    if(Global == NULL)
    {
        return;
    }

    /* TODO: Set mouse position here. */

    switch(Event->Type)
    {
        case RR_EVENT_TYPE_MOUSE_BUTTON_DOWN:
        {
            if(Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                Global->LeftMouseButtonDown = true;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_UP:
        {
            if(Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                Global->LeftMouseButtonUp = true;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_MOTION:
        {
        }
        break;
        default:
            break;
    }
}

void Rr_BeginUI(Rr_UIContext *Context)
{
    assert(Context);
    assert(Context->App);

    Rr_App *App = Context->App;
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Global = Context;
    Global->FrameNumber = Renderer->FrameNumber;
    Global->FrameArena = Rr_GetFrameArena(Renderer);

    if(Global->NextFontSize != 0.0f)
    {
        Global->FontSize = Global->NextFontSize;
        Global->NextFontSize = 0.0f;

        Global->LineHeight = Global->FontSize * Global->Font->LineHeight;
        Global->ContentsPadding =
            Rr_MulV2F(Global->Style.ContentsPadding, Global->FontSize);
        Global->HorizontalMargin = Global->FontSize * 0.5f;

        Global->WindowTitleHeight =
            Global->Style.TitlePadding.Y * Global->FontSize * 2.0f +
            Global->LineHeight;
        Global->MinWindowSizeNoTitle = Rr_MulV2F(Global->ContentsPadding, 2.0f);
        Global->MinWindowSizeNoTitle.X += Global->FontSize;
        Global->MinWindowSizeNoTitle.Y += Global->FontSize;
        Global->MinWindowSize = Global->MinWindowSizeNoTitle;
        Global->MinWindowSize.Y += Global->WindowTitleHeight;

        Global->FrameThickness =
            floorf(RR_MAX(1.0f, Global->FontSize * 0.075f));
        Global->ResizeHandleSize = Global->FontSize * 0.75f;
        Global->SeparatorLineHeight = Global->LineHeight * 0.5f;
        Global->ButtonPadding = (Rr_Vec2){ Global->LineHeight * 0.25f,
                                           Global->LineHeight * 0.125f };
        Global->CheckboxSize =
            (Rr_Vec2){ Global->LineHeight * 0.75f, Global->LineHeight * 0.75f };
    }

    Rr_MouseButtonFlags MouseState = Rr_GetMouseState();
    Global->MousePosition = Rr_GetMousePosition(App);

    Rr_UIWindow *OldHoveredWindow = Global->HoveredWindow;
    Global->HoveredWindow = NULL;
    for(int Index = Global->Windows.Count - 1; Index >= 0; --Index)
    {
        Rr_UIWindow *Window = Global->Windows.Data[Index];
        if(Rr_RectContains(
               Window->Position,
               Window->Size,
               Global->MousePosition))
        {
            Global->HoveredWindow = Window;

            if(Global->LeftMouseButtonDown)
            {
                Rr_UIWindow *HighestWindow =
                    Global->Windows.Data[Global->Windows.Count - 1];
                if(Window != HighestWindow)
                {
                    int Temp = HighestWindow->ZOrder;
                    HighestWindow->ZOrder = Window->ZOrder;
                    Window->ZOrder = Temp;
                }

                Global->MouseButtonCapture = true;
            }

            break;
        }
    }

    if(Global->MovingWindow)
    {
        if(RR_HAS_BIT(MouseState, RR_MOUSE_BUTTON_LEFT_BIT))
        {
            Rr_Vec2 Delta =
                Rr_SubV2(Global->MousePosition, Global->MovingStart);
            Global->MovingWindow->Position =
                Rr_AddV2(Global->MovingWindowStart, Delta);
            Global->MovingWindow->Position =
                Rr_FloorV2(Global->MovingWindow->Position);
        }
        else
        {
            Global->MovingWindow = NULL;
        }
    }
    else if(Global->ResizingWindow)
    {
        if(RR_HAS_BIT(MouseState, RR_MOUSE_BUTTON_LEFT_BIT))
        {
            Rr_Vec2 Delta =
                Rr_SubV2(Global->MousePosition, Global->ResizingStart);
            Rr_Vec2 NewWindowSize =
                Rr_AddV2(Global->ResizingWindowStart, Delta);
            Rr_Vec2 MinWindowSize =
                Rr_GetMinWindowSize(Global->ResizingWindow->Flags);
            NewWindowSize.X = RR_MAX(NewWindowSize.X, MinWindowSize.X);
            NewWindowSize.Y = RR_MAX(NewWindowSize.Y, MinWindowSize.Y);
            Global->ResizingWindow->Size = Rr_FloorV2(NewWindowSize);
        }
        else
        {
            Global->ResizingWindow = NULL;
        }
    }
    else if(Global->HoveredWindow && Global->LeftMouseButtonDown)
    {
        Rr_Vec2 ResizeHandleSize = {
            Global->ResizeHandleSize,
            Global->ResizeHandleSize,
        };
        Rr_Vec2 ResizeHandlePosition = Rr_SubV2(
            Rr_AddV2(
                Global->HoveredWindow->Position,
                Global->HoveredWindow->Size),
            ResizeHandleSize);

        if(Rr_RectContains(
               ResizeHandlePosition,
               ResizeHandleSize,
               Global->MousePosition))
        {
            Global->ResizingStart = Global->MousePosition;
            Global->ResizingWindow = Global->HoveredWindow;
            Global->ResizingWindowStart = Global->ResizingWindow->Size;
        }
        else
        {
            Global->MovingStart = Global->MousePosition;
            Global->MovingWindow = Global->HoveredWindow;
            Global->MovingWindowStart = Global->MovingWindow->Position;
        }
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
}

static inline void Rr_DrawWindowTitle(Rr_UIWindow *Window)
{
    Rr_Vec2 TitlePosition = Window->Position;
    Rr_Vec2 TitleSize = {
        Window->Size.X,
        Global->WindowTitleHeight,
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
}

static inline void Rr_DrawResizeHandle(Rr_UIWindow *Window)
{
    Rr_Vec2 BottomRight = Rr_AddV2(Window->Position, Window->Size);
    BottomRight.X -= Global->FrameThickness;
    BottomRight.Y -= Global->FrameThickness;
    Rr_Vec2 ResizeHandlePosition = {
        BottomRight.X - Global->ResizeHandleSize,
        BottomRight.Y - Global->ResizeHandleSize,
    };
    Rr_Vec2 ResizeHandleSize = {
        Global->ResizeHandleSize,
        Global->ResizeHandleSize,
    };

    Rr_Vec4 ResizeHandleColor = Global->Style.Foreground;

    bool Hovered = false;
    Rr_ButtonBehavior(
        Window,
        ResizeHandlePosition,
        ResizeHandleSize,
        NULL,
        &Hovered);

    if(Hovered || Global->ResizingWindow == Window)
    {
        ResizeHandleColor = Rr_MulV4F(ResizeHandleColor, 0.75f);
    }

    Rr_DrawSolidTriangle(
        Window,
        (Rr_Vec2){ BottomRight.X - Global->ResizeHandleSize, BottomRight.Y },
        (Rr_Vec2){ BottomRight.X, BottomRight.Y - Global->ResizeHandleSize },
        (Rr_Vec2){ BottomRight.X, BottomRight.Y },
        &ResizeHandleColor);
}

int Rr_WindowSort(const void *A, const void *B)
{
    const Rr_UIWindow *WindowA = *(Rr_UIWindow **)A;
    const Rr_UIWindow *WindowB = *(Rr_UIWindow **)B;

    return WindowA->ZOrder > WindowB->ZOrder;
}

void Rr_EndUI(void)
{
    RR_UI_ASSERT_NO_WINDOW();

    Global->LastWindowCount = Global->Windows.Count;

    if(Global->Windows.Count == 0)
    {
        Global = NULL;
        return;
    }

    Rr_App *App = Global->App;
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

    qsort(
        Global->Windows.Data,
        Global->Windows.Count,
        sizeof(Rr_UIWindow *),
        Rr_WindowSort);

    for(size_t Index = 0; Index < Global->Windows.Count; ++Index)
    {
        Rr_UIWindow *Window = Global->Windows.Data[Index];

        bool HasTitle =
            RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT) == false;

        /* Contents background quad was already reserved. */

        Rr_Vec2 ContentsPosition = {
            Window->Position.X,
            Window->Position.Y,
        };
        if(HasTitle)
        {
            ContentsPosition.Y += Global->WindowTitleHeight;
        }
        Rr_Vec2 ContentsSize = {
            Window->Size.X,
            Window->Size.Y,
        };
        if(HasTitle)
        {
            ContentsSize.Y -= Global->WindowTitleHeight;
        }
        Rr_UIQuad ContentsBackgroundQuad = Window->Vertices.Data;
        Rr_SolidQuad(
            ContentsBackgroundQuad,
            ContentsPosition,
            ContentsSize,
            &Global->Style.Background);

        if(HasTitle)
        {
            Rr_DrawWindowTitle(Window);
        }

        if(RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT) == false)
        {
            Rr_DrawResizeHandle(Window);
        }

        Rr_DrawFrameQuad(
            Window,
            Window->Position,
            Window->Size,
            Global->FrameThickness,
            &Global->Style.Outline);

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

        /* Prepare the window for the next frame. */

        Window->Added = false;
        Window->LastIndexCount = Window->Indices.Count;
        Window->LastVertexCount = Window->Vertices.Count;
    }

    if(Global->LeftMouseButtonUp)
    {
        Global->LeftMouseButtonHeld = false;
        Global->MouseButtonCapture = false;
    }
    Global->LeftMouseButtonDown = false;
    Global->LeftMouseButtonUp = false;
    RR_ZERO(Global->HorizontalX);
}

void Rr_SetFontSize(float Size)
{
    if(Global)
    {
        Global->NextFontSize = Size;
    }
}

void Rr_DebugOverlay(void)
{
    RR_UI_ASSERT_GLOBAL();

    static double Timer = 1.0f;
    static double FPSCount = 0.0f;

    Timer += Rr_GetDeltaSeconds(Global->App);

    if(Timer > 0.25f)
    {
        FPSCount = Rr_GetFramesPerSecond(Global->App);
        Timer = 0.0f;
    }

    Rr_Renderer *Renderer = Global->App->Renderer;

    Rr_BeginWindow("Rr_DebugOverlay", RR_UI_WINDOW_FLAGS_NO_TITLE_BIT);
    Rr_LabelF("FPS: %.2f", FPSCount);
    Rr_Separator();
    Rr_LabelF("Renderer Arena: %d", Global->App->Renderer->Arena->Commited);
    for(size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
    {
        Rr_Frame *Frame = Renderer->Frames + Index;
        Rr_LabelF("Frame#%d Arena: %d", Index, Frame->Arena->Commited);
    }
    Rr_Separator();
    Rr_Label("More text...");
    Rr_EndWindow();
}
