/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
    size_t IndexCount;
    size_t FirstIndex;
    Rr_Rect Rect;
};

typedef struct Rr_UIWindow Rr_UIWindow;
struct Rr_UIWindow
{
    Rr_String Title;
    Rr_UIWindowFlags Flags;
    Rr_Rect Rect;
    float YStart;
    float YEnd;
    float VScroll;
    int ZOrder;
    bool Minimized : 1;
    bool Added : 1;
    bool Closed : 1;

    Rr_Map *WidgetMap;

    RR_ARRAY(Rr_UIClipRect) ClipRects;
};

typedef enum
{
    RR_UI_DRAG_OP_NONE,
    RR_UI_DRAG_OP_MOVE,
    RR_UI_DRAG_OP_RESIZE,
    RR_UI_DRAG_OP_SCROLL,
} Rr_UIDragOp;

struct Rr_UIContext
{
    Rr_UIStyle Style;

    Rr_Map *WindowMap;
    int TotalWindowCount;
    RR_ARRAY(Rr_UIWindow *) ActiveWindows;
    Rr_UIWindow *CurrentWindow;
    Rr_UIWindow *HoveredWindow;

    bool DeferResizeHandle;
    Rr_Vec2 DeferredResizeHandlePositions[3];
    Rr_Vec4 DeferredResizeHandleColor;

    float AvailableContentsWidth;

    RR_ARRAY(float) HorizontalX;
    float HorizontalMaxHeight;

    Rr_Vec2 TabCursor;
    const char **SelectedTabRef;
    const char *SelectedTab;

    bool LeftMouseButtonDownOverWindow;
    bool MouseHoveredSomething;

    bool LeftMouseButtonDown;
    bool LeftMouseButtonHeld;
    bool LeftMouseButtonUp;
    Rr_Vec2 MousePosition;
    Rr_Vec2 MouseWheelDelta;

    Rr_UIDragOp DragOp;
    Rr_UIWindow *DragOpWindow;
    Rr_Vec2 DragOpMouseStart;
    Rr_Vec2 DragOpWindowStart;

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
    float TitleButtonSize;
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

    RR_ARRAY(Rr_UIVertex) Vertices;
    RR_ARRAY(Rr_UIIndex) Indices;

    Rr_Buffer *UniformBuffer;

    Rr_Sampler *Sampler;

    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UIContext *gContext;

#define RR_UI_IS_HORIZONTAL() (gContext->HorizontalX.Count > 0)

#define RR_UI_CLIP_RECT(Window)                                          \
    ((Window) && (Window)->ClipRects.Count > 0                           \
         ? &(Window)->ClipRects.Data[(Window)->ClipRects.Count - 1].Rect \
         : &(Window)->Rect)

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

        uint32_t Codepoint =
            (uint32_t)CJSON_GET_OBJECT_FLOAT(GlyphJSON, "unicode");

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
    Rr_UIIndex Base = (Rr_UIIndex)gContext->Vertices.Count;
    Rr_UIIndex Indices[] = {
        Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2,
    };

    Rr_UIQuad ReservedQuad = gContext->Vertices.Data + gContext->Vertices.Count;
    for(size_t Index = 0; Index < 4; ++Index)
    {
        RR_PUSH_INTO_ARRAY(&gContext->Vertices, gContext->FrameArena);
    }

    for(size_t Index = 0; Index < 6; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gContext->Indices, gContext->FrameArena) =
            Indices[Index];
    }

    return ReservedQuad;
}

static inline void Rr_DrawQuad(Rr_UIWindow *Window, Rr_UIVertex *Vertices)
{
    Rr_UIIndex Base = (Rr_UIIndex)gContext->Vertices.Count;
    Rr_UIIndex Indices[] = {
        Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2,
    };
    for(size_t Index = 0; Index < 4; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gContext->Vertices, gContext->FrameArena) =
            Vertices[Index];
    }

    for(size_t Index = 0; Index < 6; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gContext->Indices, gContext->FrameArena) =
            Indices[Index];
    }
}

static inline void Rr_SolidQuad(Rr_UIQuad Quad, Rr_Rect *Rect, Rr_Vec4 *Color)
{
    memcpy(
        Quad,
        (Rr_UIVertex[]){
            {
                .Position = Rect->Offset,
                .Color = *Color,
            },
            {
                .Position = { Rect->Offset.X + Rect->Extent.X, Rect->Offset.Y },
                .Color = *Color,
            },
            {
                .Position = { Rect->Offset.X, Rect->Offset.Y + Rect->Extent.Y },
                .Color = *Color,
            },
            {
                .Position = { Rect->Offset.X + Rect->Extent.X,
                              Rect->Offset.Y + Rect->Extent.Y },
                .Color = *Color,
            },
        },
        sizeof(Rr_UIVertex) * 4);
}

static inline void Rr_RotatedQuad(
    Rr_UIQuad Quad,
    Rr_Rect *Rect,
    float Angle,
    Rr_Vec4 *Color)
{
    Rr_Vec2 Center = Rr_RectCenter(Rect);

    memcpy(
        Quad,
        (Rr_UIVertex[]){
            {
                .Position = Rr_AddV2(
                    Center,
                    Rr_RotateV2(Rr_SubV2(Rect->Offset, Center), Angle)),
                .Color = *Color,
            },
            {
                .Position = Rr_AddV2(
                    Center,
                    Rr_RotateV2(
                        Rr_SubV2(
                            (Rr_Vec2){ Rect->Offset.X + Rect->Extent.X,
                                       Rect->Offset.Y },
                            Center),
                        Angle)),
                .Color = *Color,
            },
            {
                .Position = Rr_AddV2(
                    Center,
                    Rr_RotateV2(
                        Rr_SubV2(
                            (Rr_Vec2){ Rect->Offset.X,
                                       Rect->Offset.Y + Rect->Extent.Y },
                            Center),
                        Angle)),
                .Color = *Color,
            },
            {
                .Position = Rr_AddV2(
                    Center,
                    Rr_RotateV2(
                        Rr_SubV2(
                            (Rr_Vec2){ Rect->Offset.X + Rect->Extent.X,
                                       Rect->Offset.Y + Rect->Extent.Y },
                            Center),
                        Angle)),
                .Color = *Color,
            },
        },
        sizeof(Rr_UIVertex) * 4);
}

static inline void Rr_HorizontalGradientQuad(
    Rr_UIQuad Quad,
    Rr_Rect *Rect,
    Rr_Vec4 *ColorA,
    Rr_Vec4 *ColorB)
{
    memcpy(
        Quad,
        (Rr_UIVertex[]){
            {
                .Position = Rect->Offset,
                .Color = *ColorA,
            },
            {
                .Position = { Rect->Offset.X + Rect->Extent.X, Rect->Offset.Y },
                .Color = *ColorB,
            },
            {
                .Position = { Rect->Offset.X, Rect->Offset.Y + Rect->Extent.Y },
                .Color = *ColorA,
            },
            {
                .Position = { Rect->Offset.X + Rect->Extent.X,
                              Rect->Offset.Y + Rect->Extent.Y },
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
        *RR_PUSH_INTO_ARRAY(&gContext->Indices, gContext->FrameArena) =
            (Rr_UIIndex)(gContext->Vertices.Count + Index);
    }

    for(size_t Index = 0; Index < 3; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gContext->Vertices, gContext->FrameArena) =
            Vertices[Index];
    }
}

static inline void Rr_DrawSolidQuad(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    Rr_Vec4 *Color)
{
    Rr_UIVertex Vertices[4];
    Rr_SolidQuad(Vertices, Rect, Color);
    Rr_DrawQuad(Window, Vertices);
}

static inline void Rr_DrawRotatedQuad(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    float Angle,
    Rr_Vec4 *Color)
{
    Rr_UIVertex Vertices[4];
    Rr_RotatedQuad(Vertices, Rect, Angle, Color);
    Rr_DrawQuad(Window, Vertices);
}

static inline void Rr_DrawHorizontalGradientQuad(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    Rr_Vec4 *ColorA,
    Rr_Vec4 *ColorB)
{
    Rr_UIVertex Vertices[4];
    Rr_HorizontalGradientQuad(Vertices, Rect, ColorA, ColorB);
    Rr_DrawQuad(Window, Vertices);
}

static inline void Rr_DrawOuterFrameQuad(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    float Thickness,
    Rr_Vec4 *Color)
{
    Rr_DrawSolidQuad(
        Window,
        &(Rr_Rect){
            { Rect->Offset.X, Rect->Offset.Y - Thickness },
            { Rect->Extent.Width, Thickness },
        },
        Color); /* Top */
    Rr_DrawSolidQuad(
        Window,
        &(Rr_Rect){
            { Rect->Offset.X, Rect->Offset.Y + Rect->Extent.Height },
            { Rect->Extent.Width, Thickness },
        },
        Color); /* Bottom */
    Rr_DrawSolidQuad(
        Window,
        &(Rr_Rect){
            { Rect->Offset.X - Thickness, Rect->Offset.Y - Thickness },
            { Thickness, Rect->Extent.Height + Thickness * 2.0f },
        },
        Color); /* Left */
    Rr_DrawSolidQuad(
        Window,
        &(Rr_Rect){
            { Rect->Offset.X + Rect->Extent.Width, Rect->Offset.Y - Thickness },
            { Thickness, Rect->Extent.Height + Thickness * 2.0f },
        },
        Color); /* Right */
}

static inline void Rr_DrawFrameQuad(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    float Thickness,
    Rr_Vec4 *Color)
{
    Rr_DrawSolidQuad(
        Window,
        &(Rr_Rect){
            { Rect->Offset.X, Rect->Offset.Y },
            { Rect->Extent.Width, Thickness },
        },
        Color); /* Top */
    Rr_DrawSolidQuad(
        Window,
        &(Rr_Rect){
            { Rect->Offset.X,
              Rect->Offset.Y + Rect->Extent.Height - Thickness },
            { Rect->Extent.Width, Thickness },
        },
        Color); /* Bottom */
    Rr_DrawSolidQuad(
        Window,
        &(Rr_Rect){
            { Rect->Offset.X, Rect->Offset.Y + Thickness },
            { Thickness, Rect->Extent.Height - Thickness * 2.0f },
        },
        Color); /* Left */
    Rr_DrawSolidQuad(
        Window,
        &(Rr_Rect){
            { Rect->Offset.X + Rect->Extent.Width - Thickness,
              Rect->Offset.Y + Thickness },
            { Thickness, Rect->Extent.Height - Thickness * 2.0f },
        },
        Color); /* Right */
}

static inline void Rr_DrawTexturedQuad(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    Rr_Vec4 *Color,
    Rr_Vec2 *UVs)
{
    Rr_UIVertex Vertices[] = {
        {
            .Position = Rect->Offset,
            .UV = UVs[0],
            .Color = *Color,
        },
        {
            .Position = { Rect->Offset.X + Rect->Extent.Width, Rect->Offset.Y },
            .UV = UVs[1],
            .Color = *Color,
        },
        {
            .Position = { Rect->Offset.X,
                          Rect->Offset.Y + Rect->Extent.Height },
            .UV = UVs[2],
            .Color = *Color,
        },
        {
            .Position = { Rect->Offset.X + Rect->Extent.Width,
                          Rect->Offset.Y + Rect->Extent.Height },
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
        &(Rr_Rect){
            Rr_AddV2(Position, (Rr_Vec2){ Left, Top }),
            { Width, Height },
        },
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

static inline void Rr_BeginDragOp(
    Rr_UIWindow *Window,
    Rr_UIDragOp DragOp,
    Rr_Vec2 WindowStart)
{
    gContext->DragOpMouseStart = gContext->MousePosition;
    gContext->DragOpWindow = Window;
    gContext->DragOp = DragOp;
    gContext->DragOpWindowStart = WindowStart;
}

static inline void Rr_EndDragOp(void)
{
    gContext->DragOpWindow = NULL;
    gContext->DragOp = 0;
}

static inline bool Rr_RectContainsClipped(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    Rr_Vec2 Point)
{
    assert(Window != NULL);
    return Rr_RectContains(RR_UI_CLIP_RECT(Window), Point) &&
           Rr_RectContains(Rect, Point);
}

static inline bool Rr_ScrollBehavior(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    float *YScroll)
{
    if(Window == gContext->HoveredWindow && gContext->DragOpWindow == NULL &&
       Rr_RectContainsClipped(Window, Rect, gContext->MousePosition))
    {
        if(gContext->MouseWheelDelta.Y != 0.0f)
        {
            Rr_EndDragOp();
            *YScroll =
                *YScroll + gContext->MouseWheelDelta.Y * gContext->LineHeight;

            return true;
        }
    }

    return false;
}

static inline void Rr_ButtonBehavior(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    bool *Down,
    bool *Up,
    bool *Hovered,
    bool *Held)
{
    if(Down)
    {
        *Down = false;
    }
    if(Up)
    {
        *Up = false;
    }
    if(Hovered)
    {
        *Hovered = false;
    }
    if(Held)
    {
        *Held = false;
    }
    if(Window == gContext->HoveredWindow &&
       Rr_RectContainsClipped(Window, Rect, gContext->MousePosition))
    {
        if(gContext->LeftMouseButtonDown)
        {
            Rr_EndDragOp();
            gContext->DragOpWindow = NULL;
        }
        if(Down)
        {
            *Down = gContext->LeftMouseButtonDown;
        }
        if(Up)
        {
            *Up = gContext->LeftMouseButtonUp;
        }
        if(Held)
        {
            *Held = gContext->LeftMouseButtonHeld;
        }
        if(Hovered)
        {
            *Hovered = true;
        }
    }
}

static inline bool Rr_DragBehavior(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    Rr_UIDragOp DragOp,
    Rr_Vec2 Value,
    bool *Hovered)
{
    bool Contains =
        Rr_RectContainsClipped(Window, Rect, gContext->MousePosition);
    if(Hovered)
    {
        *Hovered = Contains;
    }

    /* Two things to note:
     * 1) Dragging resize handle also overlaps with moving and scrolling. Take
     * that into accoutn and override current drag opertion.
     * Watch out for Rr_DragBehavior() order!
     * 2) Faster mouse movements may actually result in
     * Contains == false while the drag operation is still going. */

    if(Contains && gContext->LeftMouseButtonDown &&
       (gContext->DragOpWindow == NULL || gContext->DragOpWindow == Window) &&
       gContext->HoveredWindow == Window)
    {
        Rr_BeginDragOp(Window, DragOp, Value);

        return false;
    }

    if(gContext->DragOpWindow == Window && gContext->DragOp == DragOp)
    {
        if(gContext->LeftMouseButtonHeld)
        {
            return true;
        }
        else
        {
            Rr_EndDragOp();
        }
    }

    return false;
}

static inline void Rr_SetLastClipRectIndexCount(Rr_UIWindow *Window)
{
    if(Window->ClipRects.Count > 0)
    {
        Rr_UIClipRect *Last =
            &Window->ClipRects.Data[Window->ClipRects.Count - 1];
        Last->IndexCount = gContext->Indices.Count - Last->FirstIndex;
    }
}

static inline void Rr_AddClipRect(Rr_Rect *Rect)
{
    RR_UI_ASSERT_WINDOW();

    Rr_UIWindow *Window = gContext->CurrentWindow;

    Rr_SetLastClipRectIndexCount(Window);

    Rr_UIClipRect *ClipRect =
        RR_PUSH_INTO_ARRAY(&Window->ClipRects, gContext->FrameArena);

    *ClipRect = (Rr_UIClipRect){
        .FirstIndex = gContext->Indices.Count,
        .Rect = { { floorf(Rect->Offset.X),
                    floorf(Rect->Offset.Y) },
                  { ceilf(Rect->Extent.Width),
                    ceilf(Rect->Extent.Height) } },
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

static inline void Rr_AddCloseButton(Rr_UIWindow *Window, Rr_Rect *TitleRect)
{
    /* Assuming having title bar i.e. calling from Rr_AddWindowTitle(). */

    bool HasClose = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_CLOSE_BIT);
    if(HasClose == false)
    {
        return;
    }

    float Thickness = gContext->TitleButtonSize * 0.15f;
    Rr_Rect BarRect;
    Rr_Vec2 Margin = {
        TitleRect->Extent.Width -
            (TitleRect->Extent.Height + gContext->TitleButtonSize) * 0.5f,
        TitleRect->Extent.Height * 0.5f - Thickness * 0.5f,
    };
    BarRect.Offset = Rr_AddV2(TitleRect->Offset, Margin);
    BarRect.Extent = (Rr_Vec2){
        gContext->TitleButtonSize,
        Thickness,
    };

    Rr_Rect ButtonRect = BarRect;
    ButtonRect.Offset.Y =
        TitleRect->Offset.Y +
        (TitleRect->Extent.Height - gContext->TitleButtonSize) * 0.5f;
    ButtonRect.Extent.Height = gContext->TitleButtonSize;

    Rr_Vec4 Color = gContext->Style.Foreground;

    bool Down, Up, Held, Hovered;
    Rr_ButtonBehavior(Window, &ButtonRect, &Down, &Up, &Hovered, &Held);
    if(Down)
    {
        Window->Closed = true;
    }
    if(Hovered)
    {
        Color.W *= 0.5f;
    }

    /* Rr_DrawFrameQuad(Window, &ButtonRect, gContext->FrameThickness,
     * &gContext->Style.Foreground); */

    Rr_DrawRotatedQuad(Window, &BarRect, RR_ANGLE_DEG(45.0f), &Color);

    Rr_DrawRotatedQuad(Window, &BarRect, RR_ANGLE_DEG(-45.0f), &Color);
}

static inline void Rr_AddWindowTitle(Rr_UIWindow *Window)
{
    Rr_Rect TitleRect = {
        Window->Rect.Offset,
        (Rr_Vec2){
            Window->Rect.Extent.X,
            gContext->WindowTitleHeight,
        },
    };
    Rr_Vec4 ColorB = gContext->Style.TitleBackground;
    ColorB.RGB = Rr_LerpV3(ColorB.RGB, 0.2f, (Rr_Vec3){ 0.0f, 0.0f, 0.0f });
    Rr_DrawHorizontalGradientQuad(
        Window,
        &TitleRect,
        &gContext->Style.TitleBackground,
        &ColorB);
    Rr_DrawText(
        Window,
        Rr_AddV2(
            TitleRect.Offset,
            Rr_MulV2F(gContext->Style.TitlePadding, gContext->FontSize)),
        &Window->Title,
        0.0f,
        &gContext->Style.Foreground,
        0);

    Rr_AddCloseButton(Window, &TitleRect);
}

static inline bool Rr_AddResizeHandle(Rr_UIWindow *Window, bool Defer)
{
    bool HasResize =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT) == false;
    if(HasResize == false)
    {
        return false;
    }

    Rr_Vec2 BottomRight = Rr_AddV2(Window->Rect.Offset, Window->Rect.Extent);
    Rr_Rect ResizeHandleRect = (Rr_Rect){
        {
            BottomRight.X - gContext->ResizeHandleSize,
            BottomRight.Y - gContext->ResizeHandleSize,
        },
        {
            gContext->ResizeHandleSize,
            gContext->ResizeHandleSize,
        },
    };

    bool Hovered, Dragging = Rr_DragBehavior(
                      Window,
                      &ResizeHandleRect,
                      RR_UI_DRAG_OP_RESIZE,
                      Window->Rect.Extent,
                      &Hovered);

    if(Dragging)
    {
        Rr_Vec2 Delta =
            Rr_SubV2(gContext->MousePosition, gContext->DragOpMouseStart);
        Rr_Vec2 NewWindowSize = Rr_AddV2(gContext->DragOpWindowStart, Delta);
        Rr_Vec2 MinWindowSize = Rr_GetMinWindowSize(Window->Flags);
        NewWindowSize.X = RR_MAX(NewWindowSize.X, MinWindowSize.X);
        NewWindowSize.Y = RR_MAX(NewWindowSize.Y, MinWindowSize.Y);
        Window->Rect.Extent = Rr_FloorV2(NewWindowSize);
    }

    if(Defer)
    {
        gContext->DeferResizeHandle = true;
        gContext->DeferredResizeHandlePositions[0] =
            (Rr_Vec2){ BottomRight.X - gContext->ResizeHandleSize,
                       BottomRight.Y };
        gContext->DeferredResizeHandlePositions[1] =
            (Rr_Vec2){ BottomRight.X,
                       BottomRight.Y - gContext->ResizeHandleSize };
        gContext->DeferredResizeHandlePositions[2] =
            (Rr_Vec2){ BottomRight.X, BottomRight.Y };
        gContext->DeferredResizeHandleColor = gContext->Style.Foreground;
        if(Hovered || Dragging)
        {
            gContext->DeferredResizeHandleColor =
                Rr_MulV4F(gContext->DeferredResizeHandleColor, 0.75f);
        }
    }
    else
    {
        Rr_Vec4 ResizeHandleColor = gContext->Style.Foreground;
        if(Hovered || Dragging)
        {
            ResizeHandleColor =
                Rr_MulV4F(gContext->DeferredResizeHandleColor, 0.75f);
        }
        Rr_DrawSolidTriangle(
            Window,
            (Rr_Vec2){ BottomRight.X - gContext->ResizeHandleSize,
                       BottomRight.Y },
            (Rr_Vec2){ BottomRight.X,
                       BottomRight.Y - gContext->ResizeHandleSize },
            (Rr_Vec2){ BottomRight.X, BottomRight.Y },
            &ResizeHandleColor);
    }

    return true;
}

static inline Rr_Rect Rr_GetWindowContentsAreaRect(Rr_UIWindow *Window)
{
    Rr_Rect Rect = Window->Rect;

    bool HasTitle =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT) == false;
    if(HasTitle)
    {
        Rect.Offset.Y += gContext->WindowTitleHeight;
        Rect.Extent.Height -= gContext->WindowTitleHeight;
    }

    bool HasScrollbar =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT) == false;
    float ContentsHeight = Window->YEnd - Window->YStart;
    float FillRatio = ContentsHeight / Rect.Extent.Height;
    if(HasScrollbar && FillRatio > 1.0f)
    {
        Rect.Extent.Width -= gContext->ScrollbarWidth;
    }

    return Rect;
}

static inline void Rr_Advance(Rr_Vec2 Size)
{
    RR_UI_ASSERT_WINDOW();

    if(RR_UI_IS_HORIZONTAL())
    {
        gContext->Cursor.X += Size.Width + gContext->HorizontalMargin;
        gContext->HorizontalMaxHeight =
            RR_MAX(gContext->HorizontalMaxHeight, Size.Height);

        gContext->CurrentWindow->YEnd = RR_MAX(
            gContext->CurrentWindow->YEnd,
            gContext->Cursor.Y + gContext->HorizontalMaxHeight);
    }
    else
    {
        gContext->Cursor.Y += Size.Height;

        gContext->CurrentWindow->YEnd =
            RR_MAX(gContext->CurrentWindow->YEnd, gContext->Cursor.Y);
    }
}

static inline bool Rr_AddVerticalScrollbar(Rr_UIWindow *Window)
{
    bool HasScrollbar =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT) == false;
    if(HasScrollbar != true)
    {
        return false;
    }

    bool HasResize =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT) == false;

    Rr_Rect ContentsAreaRect = Rr_GetWindowContentsAreaRect(Window);
    float ContentsHeight = Window->YEnd - Window->YStart;
    float FillRatio = ContentsAreaRect.Extent.Height / ContentsHeight;
    float MaxYScroll =
        RR_MAX(0.0f, ContentsHeight - ContentsAreaRect.Extent.Height);

    if(FillRatio < 1.0f)
    {
        Rr_Vec2 ScrollbarPosition = ContentsAreaRect.Offset;
        ScrollbarPosition.X += ContentsAreaRect.Extent.Width;
        Rr_Vec2 ScrollbarSize = { gContext->ScrollbarWidth,
                                  ContentsAreaRect.Extent.Height };
        Rr_DrawSolidQuad(
            Window,
            &(Rr_Rect){
                ScrollbarPosition,
                ScrollbarSize,
            },
            &gContext->Style.ScrollbarBackground);

        float ScrollbarHandleOffset =
            (gContext->ScrollbarWidth - gContext->ScrollbarHandleWidth) / 2.0f;

        Rr_Vec2 ScrollbarHandlePosition = ScrollbarPosition;
        Rr_Vec2 ScrollbarHandleSize = ScrollbarSize;
        ScrollbarHandlePosition.X += ScrollbarHandleOffset;
        ScrollbarHandleSize.Width = gContext->ScrollbarHandleWidth;
        ScrollbarHandleSize.Height *= FillRatio;

        Rr_Vec2 ScrollableArea = ContentsAreaRect.Extent;
        ScrollableArea.Width += gContext->ScrollbarWidth;
        if(Rr_ScrollBehavior(
               Window,
               &(Rr_Rect){
                   ContentsAreaRect.Offset,
                   ScrollableArea,
               },
               &Window->VScroll))
        {
            Window->VScroll = RR_CLAMP(0.0f, Window->VScroll, MaxYScroll);
        }

        ScrollbarHandlePosition.Y += Window->VScroll * FillRatio;

        /* Vertical margins. */

        ScrollbarHandlePosition.Y += ScrollbarHandleOffset;
        ScrollbarHandleSize.Y -= ScrollbarHandleOffset * 2.0f;

        /* Hitbox is slightly adjusted for better experience. */

        Rr_Vec2 ScrollbarButtonSize = ScrollbarHandleSize;
        ScrollbarButtonSize.Width = ScrollbarSize.Width;
        if(HasResize)
        {
            /* This cuts a bit of height from the scrollbar hitbox so the resize
             * handle is always on top. */

            float AvailableResizeButtonHeight =
                (ScrollbarPosition.Y + ScrollbarSize.Height -
                 gContext->ResizeHandleSize) -
                (ScrollbarHandlePosition.Y + ScrollbarHandleSize.Height);
            if(AvailableResizeButtonHeight < 0.0f)
            {
                ScrollbarButtonSize.Height += AvailableResizeButtonHeight;
            }
        }

        bool Hovered, Dragging = Rr_DragBehavior(
                          Window,
                          &(Rr_Rect){
                              ScrollbarHandlePosition,
                              ScrollbarButtonSize,
                          },
                          RR_UI_DRAG_OP_SCROLL,
                          (Rr_Vec2){ 0.0f, Window->VScroll },
                          &Hovered);

        if(Dragging)
        {
            Rr_Vec2 Delta =
                Rr_SubV2(gContext->MousePosition, gContext->DragOpMouseStart);
            float ContentsHeight = Window->YEnd - Window->YStart;
            float FillRatio = ContentsHeight / ContentsAreaRect.Extent.Height;
            Window->VScroll =
                gContext->DragOpWindowStart.Y + (Delta.Y * FillRatio);
        }

        Window->VScroll = RR_CLAMP(0.0f, Window->VScroll, MaxYScroll);

        Rr_Vec4 *ScrollbarHandleColor;
        if(Dragging)
        {
            ScrollbarHandleColor = &gContext->Style.ScrollbarHeld;
        }
        else if(Hovered)
        {
            ScrollbarHandleColor = &gContext->Style.ScrollbarHovered;
        }
        else
        {
            ScrollbarHandleColor = &gContext->Style.ScrollbarNormal;
        }
        Rr_DrawSolidQuad(
            Window,
            &(Rr_Rect){
                ScrollbarHandlePosition,
                ScrollbarHandleSize,
            },
            ScrollbarHandleColor);

        return true;
    }
    else
    {
        Window->VScroll = 0.0f;
    }

    return false;
}

void Rr_BeginWindow(const char *Title, Rr_UIWindowFlags Flags)
{
    RR_UI_ASSERT_NO_WINDOW();

    bool HasTitle = RR_HAS_BIT(Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT) == false;
    size_t TitleLength = strlen(Title);
    XXH64_hash_t Hash = XXH3_64bits(Title, TitleLength);

    Rr_UIWindow **WindowRef =
        RR_GET_MAP_VALUE(&gContext->WindowMap, Hash, gContext->Arena);
    Rr_UIWindow *Window = *WindowRef;

    if(Window == NULL)
    {
        Window = RR_ALLOC_TYPE(gContext->Arena, Rr_UIWindow);
        Window->ZOrder = gContext->TotalWindowCount++;
        Window->Title = Rr_CreateString(Title, TitleLength, gContext->Arena);
        Window->Rect.Offset = (Rr_Vec2){
            .X = gContext->FontSize,
            .Y = gContext->FontSize,
        };
        const Rr_Vec2 DEFAULT_WINDOW_SIZE = { 300.0f, 500.0f };
        Window->Rect.Extent =
            Rr_AddV2(Rr_GetMinWindowSize(Flags), DEFAULT_WINDOW_SIZE);
        *WindowRef = Window;
    }

    Window->Flags = Flags;

    if(!Window->Closed)
    {
        assert(
            Window->Added == false &&
            "There already is a window with this title!");

        *RR_PUSH_INTO_ARRAY(&gContext->ActiveWindows, gContext->FrameArena) =
            Window;
        Window->Added = true;
    }

    gContext->CurrentWindow = Window;

    RR_RESET_ARRAY(&Window->ClipRects, gContext->FrameArena);

    bool Hovered, Dragging = Rr_DragBehavior(
                      Window,
                      &Window->Rect,
                      RR_UI_DRAG_OP_MOVE,
                      Window->Rect.Offset,
                      &Hovered);

    if(Dragging)
    {
        Rr_Vec2 Delta =
            Rr_SubV2(gContext->MousePosition, gContext->DragOpMouseStart);
        Window->Rect.Offset = Rr_AddV2(gContext->DragOpWindowStart, Delta);
        Window->Rect.Offset = Rr_FloorV2(Window->Rect.Offset);
    }

    Rr_Rect ClipRect = {
        Rr_SubV2(
            Window->Rect.Offset,
            Rr_V2(gContext->FrameThickness, gContext->FrameThickness)),
        Rr_AddV2(
            Window->Rect.Extent,
            Rr_V2(
                gContext->FrameThickness * 2.0f,
                gContext->FrameThickness * 2.0f)),
    };
    Rr_AddClipRect(&ClipRect);

    /* Clipped to the whole window area. */

    if(HasTitle)
    {
        Rr_AddWindowTitle(Window);
    }

    bool HasScrollbar = Rr_AddVerticalScrollbar(Window);
    bool HasResize = Rr_AddResizeHandle(Window, !HasScrollbar);

    Rr_DrawOuterFrameQuad(
        Window,
        &Window->Rect,
        gContext->FrameThickness,
        &gContext->Style.Outline);

    Rr_Rect ContentsAreaRect = Rr_GetWindowContentsAreaRect(Window);
    Rr_AddClipRect(&ContentsAreaRect);

    /* Clipped to contents. */

    Rr_DrawSolidQuad(Window, &ContentsAreaRect, &gContext->Style.Background);

    gContext->AvailableContentsWidth =
        HasScrollbar ? Window->Rect.Extent.Width - gContext->ScrollbarWidth
                     : Window->Rect.Extent.Width;
    gContext->AvailableContentsWidth -= gContext->ContentsPadding.X * 2.0f;

    gContext->Cursor = Rr_AddV2(Window->Rect.Offset, gContext->ContentsPadding);
    if(HasTitle)
    {
        gContext->Cursor.Y += gContext->WindowTitleHeight;
    }

    gContext->Cursor.Y -= Window->VScroll;

    Window->YStart = Window->YEnd = gContext->Cursor.Y;
}

void Rr_EndWindow(void)
{
    RR_UI_ASSERT_WINDOW();

    if(gContext->DeferResizeHandle)
    {
        Rr_DrawSolidTriangle(
            gContext->CurrentWindow,
            gContext->DeferredResizeHandlePositions[0],
            gContext->DeferredResizeHandlePositions[1],
            gContext->DeferredResizeHandlePositions[2],
            &gContext->DeferredResizeHandleColor);
        gContext->DeferResizeHandle = false;
    }

    Rr_SetLastClipRectIndexCount(gContext->CurrentWindow);
    gContext->CurrentWindow->YEnd += gContext->ContentsPadding.Y * 2.0f;
    gContext->CurrentWindow = NULL;
}

void Rr_BeginHorizontal(void)
{
    gContext->HorizontalMaxHeight = 0;
    *RR_PUSH_INTO_ARRAY(&gContext->HorizontalX, gContext->FrameArena) =
        gContext->Cursor.X;
}

void Rr_EndHorizontal(void)
{
    assert(
        RR_UI_IS_HORIZONTAL() &&
        "Did you forget to call Rr_BeginHorizontal()?");
    gContext->Cursor.X = RR_POP_FROM_ARRAY(&gContext->HorizontalX);
    gContext->Cursor.Y += gContext->HorizontalMaxHeight;
}

void Rr_BeginTabs(const char *Title)
{
    RR_UI_ASSERT_WINDOW();

    Rr_UIWindow *Window = gContext->CurrentWindow;

    gContext->SelectedTabRef =
        RR_GET_MAP_VALUE(&Window->WidgetMap, (uint64_t)Title, gContext->Arena);
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
        &(Rr_Rect){
            SeparatorPosition,
            SeparatorSize,
        },
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

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_ButtonBehavior(
        Window,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        },
        NULL,
        &Up,
        &Hovered,
        &Held);

    Rr_Vec4 *TabButtonColor;
    if(Selected)
    {
        TabButtonColor = &gContext->Style.Foreground;
    }
    else if(Held)
    {
        TabButtonColor = &gContext->Style.ButtonHeld;
    }
    else if(Hovered)
    {
        TabButtonColor = &gContext->Style.ButtonHovered;
    }
    else
    {
        TabButtonColor = &gContext->Style.Background;
    }

    Rr_SolidQuad(
        TabQuad,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        },
        TabButtonColor);

    if(Up)
    {
        *gContext->SelectedTabRef =
            Title; /* Newly selected tab will be rendered next frame. */
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
    Rr_DrawSolidQuad(Window, &(Rr_Rect){ Position, Size }, &Color);

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

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_ButtonBehavior(
        Window,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        },
        NULL,
        &Up,
        &Hovered,
        &Held);

    Rr_Rect ButtonRect = {
        ButtonPosition,
        ButtonSize,
    };
    if(Held)
    {
        Rr_SolidQuad(ButtonQuad, &ButtonRect, &gContext->Style.ButtonHeld);
    }
    else if(Hovered)
    {
        Rr_SolidQuad(ButtonQuad, &ButtonRect, &gContext->Style.ButtonHovered);
    }
    else
    {
        Rr_SolidQuad(ButtonQuad, &ButtonRect, &gContext->Style.ButtonNormal);
    }

    Rr_Advance(ButtonSize);

    Rr_DestroyScratch(Scratch);

    return Up;
}

bool Rr_Checkbox(const char *Text, bool *Checked)
{
    RR_UI_ASSERT_WINDOW();
    assert(Checked != NULL);

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

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_ButtonBehavior(
        Window,
        &(Rr_Rect){
            FramePosition,
            gContext->CheckboxSize,
        },
        NULL,
        &Up,
        &Hovered,
        &Held);

    if(Up)
    {
        *Checked = !*Checked;
    }

    Rr_Vec4 Color = Rr_MulV4F(
        gContext->Style.Foreground,
        Held || Up ? 0.5f
        : Hovered  ? 0.75f
                   : 1.0f);

    Rr_DrawFrameQuad(
        Window,
        &(Rr_Rect){
            FramePosition,
            gContext->CheckboxSize,
        },
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
        Rr_DrawSolidQuad(
            Window,
            &(Rr_Rect){ CheckmarkPosition, CheckmarkSize },
            &Color);
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

    return Up;
}

bool Rr_InputField(size_t BufferSize, char *Buffer, Rr_UIInputFieldFlags Flags)
{
    return false;
}

bool Rr_Combobox(
    const char *Title,
    uint32_t OptionCount,
    const char **Options,
    uint32_t *SelectedIndex)
{
    RR_UI_ASSERT_WINDOW();
    assert(OptionCount > 0);
    assert(Options != NULL);
    assert(SelectedIndex != NULL);

    /* Rr_Scratch Scratch = Rr_GetScratch(NULL); */

    /* Rr_UIWindow *Window = gContext->CurrentWindow; */

    /* Rr_Vec2 ButtonPosition = gContext->Cursor; */
    /* Rr_UIQuad ButtonQuad = Rr_ReserveQuad(Window); */

    /* Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena); */
    /* Rr_Vec2 TextPosition = Rr_AddV2(ButtonPosition, gContext->ButtonPadding);
     */
    /* Rr_Vec2 TextSize = Rr_DrawText( */
    /*     Window, */
    /*     TextPosition, */
    /*     &TextString, */
    /*     0.0f, */
    /*     &gContext->Style.Foreground, */
    /*     0); */

    /* Rr_Vec2 ButtonSize = */
    /*     Rr_AddV2(TextSize, Rr_MulV2F(gContext->ButtonPadding, 2.0f)); */

    /* bool Up = false; */
    /* bool Hovered = false; */
    /* bool Held = false; */
    /* Rr_ButtonBehavior( */
    /*     Window, */
    /*     &(Rr_Rect){ */
    /*         ButtonPosition, */
    /*         ButtonSize, */
    /*     }, */
    /*     NULL, */
    /*     &Up, */
    /*     &Hovered, */
    /*     &Held); */

    /* Rr_Rect ButtonRect = { */
    /*     ButtonPosition, */
    /*     ButtonSize, */
    /* }; */
    /* if(Held) */
    /* { */
    /*     Rr_SolidQuad(ButtonQuad, &ButtonRect, &gContext->Style.ButtonHeld);
     */
    /* } */
    /* else if(Hovered) */
    /* { */
    /*     Rr_SolidQuad(ButtonQuad, &ButtonRect,
     * &gContext->Style.ButtonHovered); */
    /* } */
    /* else */
    /* { */
    /*     Rr_SolidQuad(ButtonQuad, &ButtonRect, &gContext->Style.ButtonNormal);
     */
    /* } */

    /* Rr_Advance(ButtonSize); */

    /* Rr_DestroyScratch(Scratch); */

    /* return Up; */
    return false;
}

bool Rr_WantMouseCapture(void)
{
    return gContext &&
           (gContext->LeftMouseButtonDownOverWindow || gContext->HoveredWindow);
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
        .ContentsPadding = { 0.5f, 0.5f },

        .Foreground = Rr_U32ToRGBA(0xD6D0B3FF),
        .Background = Rr_U32ToRGBA(0x292F33FA),
        .TitleBackground = Rr_U32ToRGBA(0xD54251FA),
        .Outline = Rr_U32ToRGBA(0x6C6F72FA),

        .ButtonNormal = Rr_U32ToRGBA(0x4c565dFF),
        .ButtonHovered = Rr_U32ToRGBA(0x687e8dFF),
        .ButtonHeld = Rr_U32ToRGBA(0x435866FF),
        .ButtonDisabled = Rr_U32ToRGBA(0x191e22FF),
    };

    Context->Style.ScrollbarBackground = Context->Style.ButtonDisabled;
    Context->Style.ScrollbarNormal = Context->Style.ButtonNormal;
    Context->Style.ScrollbarHovered = Context->Style.ButtonHovered;
    Context->Style.ScrollbarHeld = Context->Style.ButtonHeld;

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

    Context->Font = Rr_CreateFont(
        Context,
        RR_BUILTIN_SOURCESERIF4_PNG,
        RR_BUILTIN_SOURCESERIF4_JSON);

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
                gContext->LeftMouseButtonHeld = true;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_UP:
        {
            if(Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                gContext->LeftMouseButtonUp = true;
                gContext->LeftMouseButtonHeld = false;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_MOTION:
        {
        }
        break;
        case RR_EVENT_TYPE_MOUSE_WHEEL:
        {
            gContext->MouseWheelDelta =
                Rr_SubV2(gContext->MouseWheelDelta, Event->Wheel.Amount);
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
        gContext->ResizeHandleSize = gContext->FontSize;
        gContext->ScrollbarWidth = gContext->ResizeHandleSize;
        gContext->ScrollbarHandleWidth = gContext->ResizeHandleSize * 0.5f;
        gContext->SeparatorLineHeight = gContext->LineHeight * 0.5f;
        gContext->ButtonPadding = (Rr_Vec2){ gContext->LineHeight * 0.25f,
                                             gContext->LineHeight * 0.125f };
        gContext->CheckboxSize = (Rr_Vec2){ gContext->LineHeight * 0.75f,
                                            gContext->LineHeight * 0.75f };

        gContext->WindowTitleHeight =
            gContext->Style.TitlePadding.Y * gContext->FontSize * 2.0f +
            gContext->LineHeight;
        gContext->TitleButtonSize = gContext->WindowTitleHeight * 0.565f;
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

    gContext->HoveredWindow = NULL;
    for(int Index = (int)gContext->ActiveWindows.Count - 1; Index >= 0; --Index)
    {
        Rr_UIWindow *Window = gContext->ActiveWindows.Data[Index];
        if(Rr_RectContains(&Window->Rect, gContext->MousePosition))
        {
            gContext->HoveredWindow = Window;

            if(gContext->LeftMouseButtonDown)
            {
                Rr_UIWindow *HighestWindow =
                    gContext->ActiveWindows
                        .Data[gContext->ActiveWindows.Count - 1];
                if(Window != HighestWindow)
                {
                    int Temp = HighestWindow->ZOrder;
                    HighestWindow->ZOrder = Window->ZOrder;
                    Window->ZOrder = Temp;
                }

                gContext->LeftMouseButtonDownOverWindow = true;
            }

            break;
        }
    }

    RR_RESET_ARRAY(&gContext->Vertices, gContext->FrameArena);
    RR_RESET_ARRAY(&gContext->Indices, gContext->FrameArena);
    RR_RESET_ARRAY(&gContext->ActiveWindows, gContext->FrameArena);

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

    if(gContext->ActiveWindows.Count == 0)
    {
        gContext = NULL;
        return;
    }

    Rr_Renderer *Renderer = gApp->Renderer;
    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

    Rr_UIUniformData UniformData = {
        .ScreenSize = gContext->ScreenSize,
        .DistanceRange = gContext->Font->DistanceRange,
        .Time = (float)Rr_GetTimeSeconds(),
    };
    char *MappedUniformData =
        Rr_GetMappedBufferData(Renderer, gContext->UniformBuffer);
    memcpy(MappedUniformData, &UniformData, sizeof(UniformData));

    Rr_UIVertex *VertexBufferData =
        Rr_GetMappedBufferData(Renderer, gContext->VertexBuffer);
    memcpy(
        VertexBufferData,
        gContext->Vertices.Data,
        sizeof(Rr_UIVertex) * gContext->Vertices.Count);

    Rr_UIIndex *IndexBufferData =
        Rr_GetMappedBufferData(Renderer, gContext->IndexBuffer);
    memcpy(
        IndexBufferData,
        gContext->Indices.Data,
        sizeof(Rr_UIIndex) * gContext->Indices.Count);

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
        gContext->ActiveWindows.Data,
        gContext->ActiveWindows.Count,
        sizeof(Rr_UIWindow *),
        Rr_WindowSort);

    for(size_t Index = 0; Index < gContext->ActiveWindows.Count; ++Index)
    {
        Rr_UIWindow *Window = gContext->ActiveWindows.Data[Index];

        for(size_t ClipRectIndex = 0; ClipRectIndex < Window->ClipRects.Count;
            ++ClipRectIndex)
        {
            Rr_UIClipRect *ClipRect = Window->ClipRects.Data + ClipRectIndex;

            Rr_IntRect IntRect = {
                { (int32_t)roundf(ClipRect->Rect.Offset.X),
                  (int32_t)roundf(ClipRect->Rect.Offset.Y) },
                { (int32_t)roundf(ClipRect->Rect.Extent.Width),
                  (int32_t)roundf(ClipRect->Rect.Extent.Height) }
            };
            Rr_SetScissor(GraphicsNode, &IntRect);

            Rr_DrawIndexed(
                GraphicsNode,
                ClipRect->IndexCount,
                1,
                ClipRect->FirstIndex,
                0,
                0);
        }

        /* Prepare the window for the next frame. */

        Window->Added = false;
    }

    if(gContext->LeftMouseButtonUp)
    {
        gContext->LeftMouseButtonHeld = false;
        gContext->LeftMouseButtonDownOverWindow = false;
    }
    gContext->LeftMouseButtonDown = false;
    gContext->LeftMouseButtonUp = false;
    gContext->MouseWheelDelta = (Rr_Vec2){ 0 };
    RR_ZERO(gContext->HorizontalX);
}

void Rr_SetFontSize(float Size)
{
    if(gContext)
    {
        gContext->NextFontSize =
            RR_CLAMP(8.0f, floorf(Size / 2.0f) * 2.0f, 96.0f);
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
        Rr_Separator();
        size_t PresentModeCount;
        Rr_PresentMode *PresentModes =
            Rr_GetAvailablePresentModes(Renderer, &PresentModeCount);
        for(size_t Index = 0; Index < PresentModeCount; ++Index)
        {
            if(Rr_Button(Rr_GetPresentModeString(PresentModes[Index])))
            {
                Rr_SetPresentMode(Renderer, PresentModes[Index]);
            }
        }
        /* Rr_PresentMode PresentMode = Rr_GetSwapchainPresentMode(Renderer); */
        /* bool VSyncEnabled = PresentMode == RR_PRESENT_MODE_FIFO; */
        /* if(Rr_Checkbox("Use VSync", &VSyncEnabled)) */
        /* { */
        /*     Rr_SetSwapchainPresentMode( */
        /*         Rr_GetRenderer(), */
        /*         VSyncEnabled ? RR_PRESENT_MODE_FIFO */
        /*                      : RR_PRESENT_MODE_IMMEDIATE); */
        /* } */
        Rr_LabelF("FPS: %.2f", Rr_GetFramesPerSecond());
        Rr_Checkbox(
            "Frame Limiter Enabled",
            &gApp->FrameTime.EnableFrameLimiter);
        Rr_LabelF("Frame Limit: %d", gApp->FrameTime.TargetFramerate);
        if(Rr_Button("Toggle Fullscreen"))
        {
            Rr_ToggleFullscreen();
        }
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
