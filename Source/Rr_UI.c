#include "Rr_UI.h"

#include "Rr_BuiltinAssets.inc"

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
    assert(gContext != NULL && "Did you forget to call Rr_BeginUI()?")

#define RR_UI_ASSERT_NO_WINDOW()           \
    RR_UI_ASSERT_GLOBAL();                 \
    assert(                                \
        gContext->CurrentWindow == NULL && \
        "Did you forget to call Rr_EndWindow()?")

#define RR_UI_ASSERT_WINDOW()              \
    RR_UI_ASSERT_GLOBAL();                 \
    assert(                                \
        gContext->CurrentWindow != NULL && \
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

typedef struct Rr_UIClipRect Rr_UIClipRect;
struct Rr_UIClipRect
{
    uint32_t FirstIndex;
    Rr_Vec2 Position;
    Rr_Vec2 Size;
};

typedef struct Rr_UIWindow Rr_UIWindow;
struct Rr_UIWindow
{
    Rr_String Title;
    Rr_UIWindowFlags Flags;
    Rr_Vec2 Position;
    Rr_Vec2 Size;
    float YStart;
    float YEnd;
    int ZOrder;
    bool Minimized;
    bool Added;

    Rr_Map *WidgetMap;

    uint32_t LastVertexCount;
    RR_SLICE(Rr_UIVertex) Vertices;
    uint32_t LastIndexCount;
    RR_SLICE(Rr_UIIndex) Indices;
    uint32_t LastClipRectCount;
    RR_SLICE(Rr_UIClipRect) ClipRects;
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

    float AvailableContentsWidth;

    bool Horizontal;
    RR_SLICE(float) HorizontalX;
    float HorizontalMaxHeight;

    Rr_Vec2 TabCursor;
    const char **SelectedTabRef;
    const char *SelectedTab;

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
    float ScrollbarWidth;
    float ScrollbarHandleWidth;
    Rr_Vec2 ButtonPadding;
    Rr_Vec2 CheckboxSize;

    Rr_Vec2 Cursor;

    Rr_Buffer *VertexBuffer;
    Rr_Buffer *IndexBuffer;

    Rr_Buffer *UniformBuffer;

    Rr_Sampler *Sampler;

    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UIContext *gContext;

#define CJSON_GET_OBJECT_FLOAT(Object, Item) \
    ((float)cJSON_GetNumberValue(cJSON_GetObjectItem(Object, Item)))

Rr_Font *Rr_CreateFont(
    Rr_UIContext *Context,
    Rr_AssetRef FontPNGRef,
    Rr_AssetRef FontJSONRef)
{
    Rr_Renderer *Renderer = gApp->Renderer;

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
    Rr_DestroyImage(gApp->Renderer, Font->Atlas);

    RR_RETURN_FREE_LIST_ITEM(&Context->Fonts, Font);
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
        RR_PUSH_SLICE(&Window->Vertices, gContext->FrameArena);
    }

    for(size_t Index = 0; Index < 6; ++Index)
    {
        *RR_PUSH_SLICE(&Window->Indices, gContext->FrameArena) = Indices[Index];
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
        *RR_PUSH_SLICE(&Window->Vertices, gContext->FrameArena) =
            Vertices[Index];
    }

    for(size_t Index = 0; Index < 6; ++Index)
    {
        *RR_PUSH_SLICE(&Window->Indices, gContext->FrameArena) = Indices[Index];
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

static inline void Rr_HorizontalGradientQuad(
    Rr_UIQuad Quad,
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    Rr_Vec4 *ColorA,
    Rr_Vec4 *ColorB)
{
    memcpy(
        Quad,
        (Rr_UIVertex[]){
            {
                .Position = Position,
                .Color = *ColorA,
            },
            {
                .Position = { Position.X + Size.X, Position.Y },
                .Color = *ColorB,
            },
            {
                .Position = { Position.X, Position.Y + Size.Y },
                .Color = *ColorA,
            },
            {
                .Position = { Position.X + Size.X, Position.Y + Size.Y },
                .Color = *ColorB,
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
        *RR_PUSH_SLICE(&Window->Indices, gContext->FrameArena) =
            Window->Vertices.Count + Index;
    }

    for(size_t Index = 0; Index < 3; ++Index)
    {
        *RR_PUSH_SLICE(&Window->Vertices, gContext->FrameArena) =
            Vertices[Index];
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

static inline void Rr_DrawHorizontalGradientQuad(
    Rr_UIWindow *Window,
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    Rr_Vec4 *ColorA,
    Rr_Vec4 *ColorB)
{
    Rr_UIVertex Vertices[4];
    Rr_HorizontalGradientQuad(Vertices, Position, Size, ColorA, ColorB);
    Rr_DrawQuad(Window, Vertices);
}

static inline void Rr_DrawOuterFrameQuad(
    Rr_UIWindow *Window,
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    float Thickness,
    Rr_Vec4 *Color)
{
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X, Position.Y - Thickness },
        (Rr_Vec2){ Size.X, Thickness },
        Color); /* Top */
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X, Position.Y + Size.Y },
        (Rr_Vec2){ Size.X, Thickness },
        Color); /* Bottom */
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X - Thickness, Position.Y - Thickness },
        (Rr_Vec2){ Thickness, Size.Y + Thickness * 2.0f },
        Color); /* Left */
    Rr_DrawSolidQuad(
        Window,
        (Rr_Vec2){ Position.X + Size.X, Position.Y - Thickness },
        (Rr_Vec2){ Thickness, Size.Y + Thickness * 2.0f },
        Color); /* Right */
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

static inline void Rr_DrawGlyph(
    Rr_UIWindow *Window,
    Rr_Font *Font,
    float FontSize,
    Rr_Glyph *Glyph,
    Rr_Vec2 Position,
    Rr_Vec4 *Color)
{
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
        Rr_AddV2(Position, (Rr_Vec2){ Left, Top }),
        (Rr_Vec2){ Width, Height },
        Color,
        UVs);
}

static inline Rr_Vec2 Rr_DrawText(
    Rr_UIWindow *Window,
    Rr_Vec2 Position,
    Rr_String *String,
    float AvailableWidth,
    Rr_Vec4 *Color,
    Rr_UITextFlags Flags)
{
    if(String->Length == 0)
    {
        return (Rr_Vec2){ 0 };
    }

    Rr_Font *Font = gContext->Font;
    float FontSize = gContext->FontSize;
    float LineHeight = Font->LineHeight * FontSize;
    float MaxX = 0.0f;
    float CurrentX = 0.0f;
    float CurrentY = 0.0f;

    bool Wrapped = RR_HAS_BIT(Flags, RR_UI_TEXT_FLAGS_WRAPPED_BIT);
    assert(
        (!Wrapped || AvailableWidth >= FontSize) &&
        "Available width must be larger than font size!");

    Rr_Vec2 ResultSize = { 0 };

    if(Wrapped)
    {
        float CurrentWordWidth = 0.0f;
        size_t CurrentWordStart = 0;

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

            if(Codepoint == ' ' || Index == String->Length - 1)
            {
                size_t WordLength = Index - CurrentWordStart;
                if(WordLength > 0)
                {
                    if(CurrentWordWidth > AvailableWidth)
                    {
                        /* Fallback to per-character wrapping. */

                        for(size_t IndexInWord = CurrentWordStart;
                            IndexInWord <= Index;
                            ++IndexInWord)
                        {
                            Codepoint = String->Data[IndexInWord];
                            if(CurrentX > AvailableWidth)
                            {
                                CurrentX = 0.0f;
                                CurrentY += LineHeight;
                            }
                            if(Window)
                            {
                                Rr_DrawGlyph(
                                    Window,
                                    Font,
                                    FontSize,
                                    &Font->Glyphs[Codepoint],
                                    Rr_AddV2(
                                        Position,
                                        (Rr_Vec2){ CurrentX, CurrentY }),
                                    Color);
                            }
                            CurrentX +=
                                gContext->Font->Advances[Codepoint] * FontSize;
                        }
                    }
                    else
                    {
                        if(CurrentX + CurrentWordWidth > AvailableWidth)
                        {
                            CurrentX = 0.0f;
                            CurrentY += LineHeight;
                        }

                        Rr_Vec2 PositionInWord =
                            Rr_AddV2(Position, (Rr_Vec2){ CurrentX, CurrentY });
                        for(size_t IndexInWord = CurrentWordStart;
                            IndexInWord <= Index;
                            ++IndexInWord)
                        {
                            Codepoint = String->Data[IndexInWord];
                            if(Window)
                            {
                                Rr_DrawGlyph(
                                    Window,
                                    Font,
                                    FontSize,
                                    &Font->Glyphs[Codepoint],
                                    PositionInWord,
                                    Color);
                            }
                            CurrentX +=
                                gContext->Font->Advances[Codepoint] * FontSize;
                            PositionInWord.X = Position.X + CurrentX;
                        }
                    }
                }
                else
                {
                    CurrentX += gContext->Font->Advances[Codepoint] * FontSize;
                }

                MaxX = RR_MAX(MaxX, CurrentX);

                CurrentWordWidth = 0.0f;
                CurrentWordStart = Index + 1;
            }
            else
            {
                CurrentWordWidth +=
                    gContext->Font->Advances[Codepoint] * FontSize;
            }
        }
    }
    else
    {
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
                CurrentX += gContext->Font->Advances[Codepoint] * FontSize;
                continue;
            }

            if(Window)
            {
                Rr_DrawGlyph(
                    Window,
                    Font,
                    FontSize,
                    &Font->Glyphs[Codepoint],
                    Rr_AddV2(Position, (Rr_Vec2){ CurrentX, CurrentY }),
                    Color);
            }

            CurrentX += gContext->Font->Advances[Codepoint] * FontSize;
            MaxX = RR_MAX(MaxX, CurrentX);
        }
    }

    return (Rr_Vec2){ .Width = MaxX, .Height = CurrentY + LineHeight };
}

Rr_Vec2 Rr_CalculateTextSize(
    Rr_Font *Font,
    float FontSize,
    Rr_String *String,
    float AvailableWidth,
    Rr_UITextFlags Flags)
{
    return Rr_DrawText(
        NULL,
        (Rr_Vec2){ 0 },
        String,
        AvailableWidth,
        &(Rr_Vec4){ 0 },
        Flags);
}

static inline void Rr_ButtonBehavior(
    Rr_UIWindow *Window,
    Rr_Vec2 Position,
    Rr_Vec2 Size,
    bool *Clicked,
    bool *Hovered,
    bool *Held)
{
    if(Window == gContext->HoveredWindow &&
       Rr_RectContains(Position, Size, gContext->MousePosition))
    {
        if(gContext->LeftMouseButtonDown)
        {
            gContext->MovingWindow = NULL;
            if(Clicked)
            {
                *Clicked = true;
            }
        }
        else if(gContext->LeftMouseButtonHeld)
        {
            gContext->MovingWindow = NULL;
            if(Held)
            {
                *Held = true;
            }
        }
        else if(!gContext->MovingWindow && !gContext->ResizingWindow)
        {
            if(Hovered)
            {
                *Hovered = true;
            }
        }
    }
}

static inline void Rr_AddClipRect(Rr_Vec2 Position, Rr_Vec2 Size)
{
    RR_UI_ASSERT_WINDOW();

    Rr_UIWindow *Window = gContext->CurrentWindow;

    *RR_PUSH_SLICE(&Window->ClipRects, gContext->FrameArena) = (Rr_UIClipRect){
        .FirstIndex = (uint32_t)(Window->Indices.Count),
        .Position = Position,
        .Size = Size,
    };
}

static inline Rr_Vec2 Rr_GetMinWindowSize(Rr_UIWindowFlags Flags)
{
    if(RR_HAS_BIT(Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT))
    {
        return gContext->MinWindowSizeNoTitle;
    }
    else
    {
        return gContext->MinWindowSize;
    }
}

static inline void Rr_DrawWindowTitle(Rr_UIWindow *Window)
{
    Rr_Vec2 TitlePosition = Window->Position;
    Rr_Vec2 TitleSize = {
        Window->Size.X,
        gContext->WindowTitleHeight,
    };
    Rr_Vec4 ColorB = gContext->Style.TitleBackground;
    ColorB.RGB = Rr_LerpV3(ColorB.RGB, 0.2f, (Rr_Vec3){ 0.0f, 0.0f, 0.0f });
    Rr_DrawHorizontalGradientQuad(
        Window,
        TitlePosition,
        TitleSize,
        &gContext->Style.TitleBackground,
        &ColorB);
    Rr_DrawText(
        Window,
        Rr_AddV2(
            TitlePosition,
            Rr_MulV2F(gContext->Style.TitlePadding, gContext->FontSize)),
        &Window->Title,
        0.0f,
        &gContext->Style.Foreground,
        0);
}

static inline void Rr_DrawResizeHandle(Rr_UIWindow *Window)
{
    Rr_Vec2 BottomRight = Rr_AddV2(Window->Position, Window->Size);
    BottomRight.X -= gContext->FrameThickness;
    BottomRight.Y -= gContext->FrameThickness;
    Rr_Vec2 ResizeHandlePosition = {
        BottomRight.X - gContext->ResizeHandleSize,
        BottomRight.Y - gContext->ResizeHandleSize,
    };
    Rr_Vec2 ResizeHandleSize = {
        gContext->ResizeHandleSize,
        gContext->ResizeHandleSize,
    };

    Rr_Vec4 ResizeHandleColor = gContext->Style.Foreground;

    bool Hovered = false;
    Rr_ButtonBehavior(
        Window,
        ResizeHandlePosition,
        ResizeHandleSize,
        NULL,
        &Hovered,
        NULL);

    if(Hovered || gContext->ResizingWindow == Window)
    {
        ResizeHandleColor = Rr_MulV4F(ResizeHandleColor, 0.75f);
    }

    Rr_DrawSolidTriangle(
        Window,
        (Rr_Vec2){ BottomRight.X - gContext->ResizeHandleSize, BottomRight.Y },
        (Rr_Vec2){ BottomRight.X, BottomRight.Y - gContext->ResizeHandleSize },
        (Rr_Vec2){ BottomRight.X, BottomRight.Y },
        &ResizeHandleColor);
}

static inline void Rr_GetWindowContentsAreaPositionAndSize(
    Rr_UIWindow *Window,
    Rr_Vec2 *OutPosition,
    Rr_Vec2 *OutSize)
{
    *OutPosition = Window->Position;
    *OutSize = Window->Size;

    bool HasTitle =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT) == false;
    if(HasTitle)
    {
        OutPosition->Y += gContext->WindowTitleHeight;
        OutSize->Y -= gContext->WindowTitleHeight;
    }

    bool HasScrollbar =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT) == false;
    float ContentsHeight = Window->YEnd - Window->YStart;
    float FillRatio = ContentsHeight / OutSize->Y;
    if(HasScrollbar && FillRatio > 1.0f)
    {
        OutSize->Width -= gContext->ScrollbarWidth;
    }
}

static inline bool Rr_DrawVerticalScrollbar(Rr_UIWindow *Window)
{
    bool HasScrollbar =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT) == false;
    if(HasScrollbar != true)
    {
        return false;
    }

    Rr_Vec2 ContentsPosition;
    Rr_Vec2 ContentsSize;
    Rr_GetWindowContentsAreaPositionAndSize(
        Window,
        &ContentsPosition,
        &ContentsSize);
    float ContentsHeight = Window->YEnd - Window->YStart;
    float FillRatio = ContentsHeight / ContentsSize.Y;

    if(FillRatio >= 1.0f)
    {
        Rr_Vec2 ScrollbarPosition = ContentsPosition;
        ScrollbarPosition.X += ContentsSize.Width;
        Rr_Vec2 ScrollbarSize = { gContext->ScrollbarWidth,
                                  ContentsSize.Height };
        Rr_DrawSolidQuad(
            Window,
            ScrollbarPosition,
            ScrollbarSize,
            &gContext->Style.ScrollbarBackground);

        ScrollbarPosition.X +=
            (gContext->ScrollbarWidth - gContext->ScrollbarHandleWidth) / 2.0f;
        ScrollbarSize.Width = gContext->ScrollbarHandleWidth;
        ScrollbarSize.Height *= 1.0f / FillRatio;

        Rr_DrawSolidQuad(
            Window,
            ScrollbarPosition,
            ScrollbarSize,
            &gContext->Style.ScrollbarForeground);

        return true;
    }

    return false;
}

static inline void Rr_Advance(Rr_Vec2 Size)
{
    RR_UI_ASSERT_WINDOW();

    if(gContext->Horizontal)
    {
        gContext->Cursor.X += Size.Width + gContext->HorizontalMargin;
        gContext->HorizontalMaxHeight =
            RR_MAX(gContext->HorizontalMaxHeight, Size.Height);
    }
    else
    {
        gContext->Cursor.Y += Size.Height;
    }

    gContext->CurrentWindow->YEnd =
        RR_MAX(gContext->CurrentWindow->YEnd, gContext->Cursor.Y);
}

void Rr_BeginWindow(const char *Title, Rr_UIWindowFlags Flags)
{
    RR_UI_ASSERT_NO_WINDOW();

    bool HasTitle = RR_HAS_BIT(Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT) == false;
    size_t TitleLength = strlen(Title);
    XXH64_hash_t Hash = XXH3_64bits(Title, TitleLength);

    Rr_UIWindow **WindowRef =
        RR_UPSERT(&gContext->WindowMap, Hash, gContext->Arena);
    Rr_UIWindow *Window = *WindowRef;

    if(Window == NULL)
    {
        Window = RR_ALLOC_TYPE(gContext->Arena, Rr_UIWindow);
        Window->ZOrder = gContext->TotalWindowCount++;
        Window->Title = Rr_CreateString(Title, TitleLength, gContext->Arena);
        Window->Position = (Rr_Vec2){
            .X = gContext->FontSize,
            .Y = gContext->FontSize,
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

    *RR_PUSH_SLICE(&gContext->Windows, gContext->FrameArena) = Window;
    gContext->CurrentWindow = Window;

    Window->Flags = Flags;
    Window->Added = true;

    RR_ZERO(Window->Vertices);
    RR_RESERVE_SLICE(
        &Window->Vertices,
        Window->LastVertexCount ? Window->LastVertexCount : (4 * 32),
        gContext->FrameArena);

    RR_ZERO(Window->Indices);
    RR_RESERVE_SLICE(
        &Window->Indices,
        Window->LastIndexCount ? Window->LastIndexCount : (6 * 32),
        gContext->FrameArena);

    RR_ZERO(Window->ClipRects);
    RR_RESERVE_SLICE(
        &Window->ClipRects,
        Window->LastClipRectCount ? Window->LastClipRectCount : 2,
        gContext->FrameArena);

    Rr_Vec2 ClipRectPosition = Window->Position;
    ClipRectPosition.X -= gContext->FrameThickness;
    ClipRectPosition.Y -= gContext->FrameThickness;
    Rr_Vec2 ClipRectSize = Window->Size;
    ClipRectSize.X += gContext->FrameThickness * 2.0f;
    ClipRectSize.Y += gContext->FrameThickness * 2.0f;
    Rr_AddClipRect(ClipRectPosition, ClipRectSize);

    bool HasResize =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT) == false;

    if(HasTitle)
    {
        Rr_DrawWindowTitle(Window);
    }

    bool HasScrollbar = Rr_DrawVerticalScrollbar(Window);

    if(HasScrollbar && HasResize)
    {
        Rr_DrawResizeHandle(Window);
    }

    Rr_DrawOuterFrameQuad(
        Window,
        Window->Position,
        Window->Size,
        gContext->FrameThickness,
        &gContext->Style.Outline);

    Rr_Vec2 ContentsAreaPosition;
    Rr_Vec2 ContentsAreaSize;
    Rr_GetWindowContentsAreaPositionAndSize(
        Window,
        &ContentsAreaPosition,
        &ContentsAreaSize);
    Rr_AddClipRect(ContentsAreaPosition, ContentsAreaSize);

    Rr_DrawSolidQuad(
        Window,
        ContentsAreaPosition,
        ContentsAreaSize,
        &gContext->Style.Background);

    if(HasScrollbar == false && HasResize)
    {
        Rr_DrawResizeHandle(Window);
    }

    gContext->AvailableContentsWidth =
        HasScrollbar ? Window->Size.Width - gContext->ScrollbarWidth
                     : Window->Size.Width;
    gContext->AvailableContentsWidth -= gContext->ContentsPadding.X * 2.0f;

    gContext->Cursor = Rr_AddV2(Window->Position, gContext->ContentsPadding);
    if(HasTitle)
    {
        gContext->Cursor.Y += gContext->WindowTitleHeight;
    }

    Window->YStart = Window->YEnd = gContext->Cursor.Y;
}

void Rr_EndWindow(void)
{
    RR_UI_ASSERT_WINDOW();

    gContext->CurrentWindow->YEnd += gContext->ContentsPadding.Y;
    gContext->CurrentWindow = NULL;
}

void Rr_BeginHorizontal(void)
{
    gContext->Horizontal = true;
    *RR_PUSH_SLICE(&gContext->HorizontalX, gContext->FrameArena) =
        gContext->Cursor.X;
}

void Rr_EndHorizontal(void)
{
    gContext->Horizontal = false;
    gContext->Cursor.X = RR_POP_SLICE(&gContext->HorizontalX);
    gContext->Cursor.Y += gContext->HorizontalMaxHeight;
}

void Rr_BeginTabs(const char *Title)
{
    RR_UI_ASSERT_WINDOW();

    Rr_UIWindow *Window = gContext->CurrentWindow;

    gContext->SelectedTabRef =
        RR_UPSERT(&Window->WidgetMap, (uint64_t)Title, gContext->Arena);
    gContext->SelectedTab = *gContext->SelectedTabRef;
    gContext->TabCursor = gContext->Cursor;

    Rr_Advance((Rr_Vec2){ 0.0f, gContext->LineHeight });

    Rr_Vec2 SeparatorSize = {
        gContext->AvailableContentsWidth,
        gContext->FrameThickness,
    };
    Rr_Vec2 SeparatorPosition = {
        gContext->Cursor.X,
        gContext->Cursor.Y,
    };
    Rr_DrawSolidQuad(
        Window,
        SeparatorPosition,
        SeparatorSize,
        &gContext->Style.Foreground);

    Rr_Advance((Rr_Vec2){ 0.0f, gContext->ContentsPadding.Y });
}

bool Rr_Tab(const char *Title)
{
    assert(
        gContext->SelectedTabRef != NULL &&
        "Did you forget to call Rr_BeginTabs()?");

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = gContext->CurrentWindow;

    Rr_UIQuad TabQuad = NULL;

    bool Selected = false;
    if(gContext->SelectedTab == NULL)
    {
        *gContext->SelectedTabRef = gContext->SelectedTab = Title;
        Selected = true;
    }
    else if(strcmp(Title, gContext->SelectedTab) == 0)
    {
        Selected = true;
    }

    TabQuad = Rr_ReserveQuad(Window);

    Rr_String TextString = Rr_CreateString(Title, 0, Scratch.Arena);
    Rr_Vec2 TextPosition = gContext->TabCursor;
    TextPosition.X += gContext->ButtonPadding.X;
    Rr_Vec2 TextSize = Rr_DrawText(
        Window,
        TextPosition,
        &TextString,
        0.0f,
        Selected ? &gContext->Style.Background : &gContext->Style.Foreground,
        0);

    Rr_Vec2 ButtonPosition = TextPosition;
    ButtonPosition.X -= gContext->ButtonPadding.X;
    Rr_Vec2 ButtonSize = TextSize;
    ButtonSize.X += gContext->ButtonPadding.X * 2.0f;
    if(Selected)
    {
        ButtonSize.Y += gContext->FrameThickness;
    }

    gContext->TabCursor.X += ButtonSize.Width;

    bool Clicked = false;
    bool Hovered = false;
    Rr_ButtonBehavior(
        Window,
        ButtonPosition,
        ButtonSize,
        &Clicked,
        &Hovered,
        NULL);

    if(Selected)
    {
        Rr_SolidQuad(
            TabQuad,
            ButtonPosition,
            ButtonSize,
            &gContext->Style.Foreground);
    }
    else if(Hovered)
    {
        Rr_SolidQuad(
            TabQuad,
            ButtonPosition,
            ButtonSize,
            &gContext->Style.ButtonHovered);
    }
    else
    {
        Rr_SolidQuad(
            TabQuad,
            ButtonPosition,
            ButtonSize,
            &gContext->Style.Background);
    }

    if(Clicked)
    {
        *gContext->SelectedTabRef =
            Title; /* Newly selected tab will be drawn next frame. */
    }

    Rr_DestroyScratch(Scratch);

    return Selected;
}

void Rr_EndTabs(void)
{
    gContext->SelectedTabRef = NULL;
}

void Rr_Separator(void)
{
    RR_UI_ASSERT_WINDOW();

    Rr_UIWindow *Window = gContext->CurrentWindow;

    Rr_Vec2 Size = {
        gContext->AvailableContentsWidth,
        gContext->FrameThickness,
    };
    Rr_Vec2 Position = {
        gContext->Cursor.X,
        gContext->Cursor.Y + (gContext->SeparatorLineHeight / 2.0f -
                              gContext->FrameThickness / 2.0f),
    };
    Rr_Vec4 Color = Rr_MulV4F(gContext->Style.Foreground, 0.75f);
    Rr_DrawSolidQuad(Window, Position, Size, &Color);

    gContext->Cursor.Y += gContext->SeparatorLineHeight;
}

void Rr_LabelEx(const char *Text, Rr_UITextFlags Flags)
{
    RR_UI_ASSERT_WINDOW();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = gContext->CurrentWindow;

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextSize = Rr_DrawText(
        Window,
        gContext->Cursor,
        &TextString,
        gContext->AvailableContentsWidth,
        &gContext->Style.Foreground,
        Flags);

    Rr_Advance(TextSize);

    Rr_DestroyScratch(Scratch);
}

void Rr_Label(const char *Text)
{
    RR_UI_ASSERT_WINDOW();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = gContext->CurrentWindow;

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextSize = Rr_DrawText(
        Window,
        gContext->Cursor,
        &TextString,
        0.0f,
        &gContext->Style.Foreground,
        0);

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

bool Rr_Button(const char *Text)
{
    RR_UI_ASSERT_WINDOW();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = gContext->CurrentWindow;

    Rr_Vec2 ButtonPosition = gContext->Cursor;
    Rr_UIQuad ButtonQuad = Rr_ReserveQuad(Window);

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextPosition = Rr_AddV2(ButtonPosition, gContext->ButtonPadding);
    Rr_Vec2 TextSize = Rr_DrawText(
        Window,
        TextPosition,
        &TextString,
        0.0f,
        &gContext->Style.Foreground,
        0);

    Rr_Vec2 ButtonSize =
        Rr_AddV2(TextSize, Rr_MulV2F(gContext->ButtonPadding, 2.0f));

    bool Clicked = false;
    bool Hovered = false;
    bool Held = false;
    Rr_ButtonBehavior(
        Window,
        ButtonPosition,
        ButtonSize,
        &Clicked,
        &Hovered,
        &Held);

    if(Held)
    {
        Rr_SolidQuad(
            ButtonQuad,
            ButtonPosition,
            ButtonSize,
            &gContext->Style.ButtonPressed);
    }
    else if(Hovered)
    {
        Rr_SolidQuad(
            ButtonQuad,
            ButtonPosition,
            ButtonSize,
            &gContext->Style.ButtonHovered);
    }
    else
    {
        Rr_SolidQuad(
            ButtonQuad,
            ButtonPosition,
            ButtonSize,
            &gContext->Style.ButtonNormal);
    }

    Rr_Advance(ButtonSize);

    Rr_DestroyScratch(Scratch);

    return Clicked;
}

bool Rr_Checkbox(const char *Text, bool *Checked)
{
    RR_UI_ASSERT_WINDOW();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIWindow *Window = gContext->CurrentWindow;

    Rr_Vec2 ContentsPadding =
        Rr_MulV2F(gContext->Style.ContentsPadding, gContext->FontSize);

    Rr_Vec2 FramePosition = Rr_AddV2(
        gContext->Cursor,
        (Rr_Vec2){
            gContext->FrameThickness,
            gContext->LineHeight / 2.0f - gContext->CheckboxSize.Y / 2.0f,
        });

    bool Clicked = false;
    bool Hovered = false;
    bool Held = false;
    Rr_ButtonBehavior(
        Window,
        FramePosition,
        gContext->CheckboxSize,
        &Clicked,
        &Hovered,
        &Held);

    if(Clicked)
    {
        *Checked = !*Checked;
    }

    Rr_Vec4 Color = Rr_MulV4F(
        gContext->Style.Foreground,
        Held      ? 0.5f
        : Hovered ? 0.75f
                  : 1.0f);

    Rr_DrawFrameQuad(
        Window,
        FramePosition,
        gContext->CheckboxSize,
        gContext->FrameThickness,
        &Color);

    if(*Checked)
    {
        Rr_Vec2 Inset = (Rr_Vec2){
            gContext->FrameThickness * 4.0f,
            gContext->FrameThickness * 4.0f,
        };
        Rr_Vec2 CheckmarkPosition = Rr_AddV2(FramePosition, Inset);
        Rr_Vec2 CheckmarkSize =
            Rr_SubV2(gContext->CheckboxSize, Rr_MulV2F(Inset, 2.0f));
        Rr_DrawSolidQuad(Window, CheckmarkPosition, CheckmarkSize, &Color);
    }

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextPosition = Rr_AddV2(
        gContext->Cursor,
        (Rr_Vec2){ ContentsPadding.X + gContext->CheckboxSize.X, 0.0f });
    Rr_Vec2 TextSize = Rr_DrawText(
        Window,
        TextPosition,
        &TextString,
        0.0f,
        &gContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        gContext->CheckboxSize.X + TextSize.X + ContentsPadding.X,
        RR_MAX(gContext->CheckboxSize.Y, TextSize.Y),
    };
    Rr_Advance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return Clicked;
}

bool Rr_WantMouseCapture(void)
{
    return gContext &&
           (gContext->MouseButtonCapture || gContext->HoveredWindow);
}

bool Rr_WantKeyboardCapture(void)
{
    return false;
}

Rr_UIContext *Rr_CreateUIContext(void)
{
    Rr_Renderer *Renderer = Rr_GetRenderer();

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_UIContext *Context = RR_ALLOC(Arena, sizeof(Rr_UIContext));
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
        .ScrollbarBackground = Rr_U32ToRGBA(0xFF0000FF),
        .ScrollbarForeground = Rr_U32ToRGBA(0x00FF00FF),

        .ButtonNormal = Rr_U32ToRGBA(0x4c565dFF),
        .ButtonHovered = Rr_U32ToRGBA(0x687e8dFF),
        .ButtonPressed = Rr_U32ToRGBA(0x435866FF),
        .ButtonDisabled = Rr_U32ToRGBA(0x191e22FF),
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

void Rr_DestroyUIContext(Rr_UIContext *Context)
{
    Rr_Renderer *Renderer = gApp->Renderer;
    Rr_DestroyBuffer(Renderer, Context->VertexBuffer);
    Rr_DestroyBuffer(Renderer, Context->IndexBuffer);
    Rr_DestroyBuffer(Renderer, Context->UniformBuffer);
    Rr_DestroySampler(Renderer, Context->Sampler);
    Rr_DestroyPipelineLayout(Renderer, Context->PipelineLayout);
    Rr_DestroyGraphicsPipeline(Renderer, Context->GraphicsPipeline);
    Rr_DestroyFont(Context, Context->Font);
    Rr_DestroyArena(Context->Arena);
}

void Rr_ProcessUIEvent(Rr_Event *Event)
{
    if(gContext == NULL)
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
                gContext->LeftMouseButtonDown = true;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_UP:
        {
            if(Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                gContext->LeftMouseButtonUp = true;
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

    Rr_Renderer *Renderer = gApp->Renderer;

    gContext = Context;
    gContext->FrameNumber = Renderer->FrameNumber;
    gContext->FrameArena = Rr_GetFrameArena(Renderer);

    if(gContext->NextFontSize != 0.0f)
    {
        gContext->FontSize = gContext->NextFontSize;
        gContext->NextFontSize = 0.0f;

        gContext->LineHeight = gContext->FontSize * gContext->Font->LineHeight;
        gContext->ContentsPadding =
            Rr_MulV2F(gContext->Style.ContentsPadding, gContext->FontSize);
        gContext->HorizontalMargin = gContext->FontSize * 0.5f;

        gContext->FrameThickness =
            floorf(RR_MAX(1.0f, gContext->FontSize * 0.075f));
        gContext->ResizeHandleSize = gContext->FontSize * 0.75f;
        gContext->SeparatorLineHeight = gContext->LineHeight * 0.5f;
        gContext->ScrollbarWidth = gContext->ResizeHandleSize;
        gContext->ScrollbarHandleWidth = gContext->ResizeHandleSize * 0.5f;
        gContext->ButtonPadding = (Rr_Vec2){ gContext->LineHeight * 0.25f,
                                             gContext->LineHeight * 0.125f };
        gContext->CheckboxSize = (Rr_Vec2){ gContext->LineHeight * 0.75f,
                                            gContext->LineHeight * 0.75f };

        gContext->WindowTitleHeight =
            gContext->Style.TitlePadding.Y * gContext->FontSize * 2.0f +
            gContext->LineHeight;
        gContext->MinWindowSizeNoTitle =
            Rr_MulV2F(gContext->ContentsPadding, 2.0f);
        gContext->MinWindowSizeNoTitle.X += gContext->ScrollbarWidth;
        gContext->MinWindowSizeNoTitle.X += gContext->FontSize * 2.0f;
        gContext->MinWindowSizeNoTitle.Y += gContext->FontSize * 2.0f;
        gContext->MinWindowSize = gContext->MinWindowSizeNoTitle;
        gContext->MinWindowSize.Y += gContext->WindowTitleHeight;
    }

    Rr_MouseButtonFlags MouseState = Rr_GetMouseState();
    gContext->MousePosition = Rr_GetMousePosition();

    Rr_UIWindow *OldHoveredWindow = gContext->HoveredWindow;
    gContext->HoveredWindow = NULL;
    for(int Index = gContext->Windows.Count - 1; Index >= 0; --Index)
    {
        Rr_UIWindow *Window = gContext->Windows.Data[Index];
        if(Rr_RectContains(
               Window->Position,
               Window->Size,
               gContext->MousePosition))
        {
            gContext->HoveredWindow = Window;

            if(gContext->LeftMouseButtonDown)
            {
                Rr_UIWindow *HighestWindow =
                    gContext->Windows.Data[gContext->Windows.Count - 1];
                if(Window != HighestWindow)
                {
                    int Temp = HighestWindow->ZOrder;
                    HighestWindow->ZOrder = Window->ZOrder;
                    Window->ZOrder = Temp;
                }

                gContext->MouseButtonCapture = true;
            }

            break;
        }
    }

    if(gContext->MovingWindow)
    {
        if(RR_HAS_BIT(MouseState, RR_MOUSE_BUTTON_LEFT_BIT))
        {
            Rr_Vec2 Delta =
                Rr_SubV2(gContext->MousePosition, gContext->MovingStart);
            gContext->MovingWindow->Position =
                Rr_AddV2(gContext->MovingWindowStart, Delta);
            gContext->MovingWindow->Position =
                Rr_FloorV2(gContext->MovingWindow->Position);
        }
        else
        {
            gContext->MovingWindow = NULL;
        }
    }
    else if(gContext->ResizingWindow)
    {
        if(RR_HAS_BIT(MouseState, RR_MOUSE_BUTTON_LEFT_BIT))
        {
            Rr_Vec2 Delta =
                Rr_SubV2(gContext->MousePosition, gContext->ResizingStart);
            Rr_Vec2 NewWindowSize =
                Rr_AddV2(gContext->ResizingWindowStart, Delta);
            Rr_Vec2 MinWindowSize =
                Rr_GetMinWindowSize(gContext->ResizingWindow->Flags);
            NewWindowSize.X = RR_MAX(NewWindowSize.X, MinWindowSize.X);
            NewWindowSize.Y = RR_MAX(NewWindowSize.Y, MinWindowSize.Y);
            gContext->ResizingWindow->Size = Rr_FloorV2(NewWindowSize);
        }
        else
        {
            gContext->ResizingWindow = NULL;
        }
    }
    else if(gContext->HoveredWindow && gContext->LeftMouseButtonDown)
    {
        Rr_Vec2 ResizeHandleSize = {
            gContext->ResizeHandleSize,
            gContext->ResizeHandleSize,
        };
        Rr_Vec2 ResizeHandlePosition = Rr_SubV2(
            Rr_AddV2(
                gContext->HoveredWindow->Position,
                gContext->HoveredWindow->Size),
            ResizeHandleSize);

        if(Rr_RectContains(
               ResizeHandlePosition,
               ResizeHandleSize,
               gContext->MousePosition))
        {
            gContext->ResizingStart = gContext->MousePosition;
            gContext->ResizingWindow = gContext->HoveredWindow;
            gContext->ResizingWindowStart = gContext->ResizingWindow->Size;
        }
        else
        {
            gContext->MovingStart = gContext->MousePosition;
            gContext->MovingWindow = gContext->HoveredWindow;
            gContext->MovingWindowStart = gContext->MovingWindow->Position;
        }
    }

    RR_ZERO(gContext->Windows);
    RR_RESERVE_SLICE(
        &gContext->Windows,
        gContext->LastWindowCount,
        gContext->FrameArena);
    gContext->CurrentWindow = NULL;

    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
    gContext->ScreenSize.Width = (float)SwapchainSize.Width;
    gContext->ScreenSize.Height = (float)SwapchainSize.Height;
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

    gContext->LastWindowCount = gContext->Windows.Count;

    if(gContext->Windows.Count == 0)
    {
        gContext = NULL;
        return;
    }

    Rr_Renderer *Renderer = gApp->Renderer;
    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

    Rr_UIUniformData UniformData = {
        .ScreenSize = gContext->ScreenSize,
        .DistanceRange = gContext->Font->DistanceRange,
        .Time = Rr_GetTimeSeconds(),
    };
    char *MappedUniformData =
        Rr_GetMappedBufferData(Renderer, gContext->UniformBuffer);
    memcpy(MappedUniformData, &UniformData, sizeof(UniformData));

    Rr_UIVertex *VertexBufferDataStart;
    Rr_UIVertex *VertexBufferData;
    VertexBufferData = VertexBufferDataStart =
        Rr_GetMappedBufferData(Renderer, gContext->VertexBuffer);

    Rr_UIIndex *IndexBufferDataStart;
    Rr_UIIndex *IndexBufferData;
    IndexBufferData = IndexBufferDataStart =
        Rr_GetMappedBufferData(Renderer, gContext->IndexBuffer);

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
    Rr_BindGraphicsPipeline(GraphicsNode, gContext->GraphicsPipeline);
    Rr_BindVertexBuffer(GraphicsNode, gContext->VertexBuffer, 0, 0);
    Rr_BindIndexBuffer(
        GraphicsNode,
        gContext->IndexBuffer,
        0,
        0,
        RR_INDEX_TYPE_UINT16);
    Rr_BindUniformBuffer(
        GraphicsNode,
        gContext->UniformBuffer,
        0,
        0,
        0,
        sizeof(Rr_UIUniformData));
    Rr_BindCombinedImageSampler(
        GraphicsNode,
        gContext->Font->Atlas,
        gContext->Sampler,
        0,
        1);

    qsort(
        gContext->Windows.Data,
        gContext->Windows.Count,
        sizeof(Rr_UIWindow *),
        Rr_WindowSort);

    for(size_t Index = 0; Index < gContext->Windows.Count; ++Index)
    {
        Rr_UIWindow *Window = gContext->Windows.Data[Index];

        int32_t VertexOffset =
            (int32_t)(VertexBufferData - VertexBufferDataStart);
        memcpy(
            VertexBufferData,
            Window->Vertices.Data,
            sizeof(Rr_UIVertex) * Window->Vertices.Count);
        VertexBufferData += Window->Vertices.Count;

        size_t FirstIndex = (size_t)(IndexBufferData - IndexBufferDataStart);
        memcpy(
            IndexBufferData,
            Window->Indices.Data,
            sizeof(Rr_UIIndex) * Window->Indices.Count);
        IndexBufferData += Window->Indices.Count;

        for(size_t ClipRectIndex = 0; ClipRectIndex < Window->ClipRects.Count;
            ++ClipRectIndex)
        {
            Rr_UIClipRect *ClipRect = Window->ClipRects.Data + ClipRectIndex;

            Rr_SetScissor(
                GraphicsNode,
                (Rr_IntVec4){
                    ClipRect->Position.X,
                    ClipRect->Position.Y,
                    ClipRect->Size.X,
                    ClipRect->Size.Y,
                });

            size_t IndexCount;
            if(ClipRectIndex == Window->ClipRects.Count - 1)
            {
                IndexCount = Window->Indices.Count - ClipRect->FirstIndex;
            }
            else
            {
                IndexCount =
                    Window->ClipRects.Data[ClipRectIndex + 1].FirstIndex -
                    ClipRect->FirstIndex;
            }

            Rr_DrawIndexed(
                GraphicsNode,
                IndexCount,
                1,
                (size_t)ClipRect->FirstIndex + FirstIndex,
                VertexOffset,
                0);
        }

        /* Prepare the window for the next frame. */

        Window->Added = false;
        Window->LastIndexCount = Window->Indices.Count;
        Window->LastVertexCount = Window->Vertices.Count;
        Window->LastClipRectCount = Window->ClipRects.Count;
    }

    if(gContext->LeftMouseButtonUp)
    {
        gContext->LeftMouseButtonHeld = false;
        gContext->MouseButtonCapture = false;
    }
    gContext->LeftMouseButtonDown = false;
    gContext->LeftMouseButtonUp = false;
    RR_ZERO(gContext->HorizontalX);
}

void Rr_SetFontSize(float Size)
{
    if(gContext)
    {
        gContext->NextFontSize = Size;
    }
}

void Rr_DebugOverlay(void)
{
    RR_UI_ASSERT_GLOBAL();

    Rr_Renderer *Renderer = gApp->Renderer;

    Rr_BeginWindow("Rr_DebugOverlay", RR_UI_WINDOW_FLAGS_NO_TITLE_BIT);
    Rr_BeginTabs("DebugOverlayTabs");
    if(Rr_Tab("General"))
    {
        Rr_LabelF("Time: %.2f", Rr_GetTimeSeconds());
        Rr_LabelF("FPS: %.2f", Rr_GetFramesPerSecond());
    }
    if(Rr_Tab("Memory"))
    {
        Rr_LabelF("Application: %d bytes", gApp->Arena->Commited);
        Rr_LabelF("Renderer: %d bytes", gApp->Renderer->Arena->Commited);
        for(size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
        {
            Rr_Frame *Frame = Renderer->Frames + Index;
            Rr_LabelF("Frame#%d: %d bytes", Index, Frame->Arena->Commited);
        }
        Rr_LabelF("UI: %d bytes", gContext->Arena->Commited);
    }
    if(Rr_Tab("Renderer"))
    {
        Rr_LabelF("Frame: %zu", gApp->Renderer->FrameNumber);
        Rr_LabelF("RenderPasses: %zu", gApp->Renderer->RenderPasses.Count);
        Rr_LabelF("Framebuffers: %zu", gApp->Renderer->Framebuffers.Count);
        Rr_LabelF(
            "SwapchainImages: %zu",
            gApp->Renderer->SwapchainImages.Count);
    }
    Rr_EndTabs();
    Rr_EndWindow();
}
