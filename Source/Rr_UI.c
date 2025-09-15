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

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Rr_UI.h"

#include "Rr_BuiltinAssets.inc"

#include "Rr_App.h"
#include "Rr_Log.h"
#include "Rr_Renderer.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Utility.h>

#include <xxHash/xxhash.h>

#include <cJSON/cJSON.h>

#include <stb/stb_image.h>

#include <assert.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>

#define RR_UI_MIN_FONT_SIZE (12.0f)
#define RR_UI_MAX_FONT_SIZE (48.0f)

typedef uint64_t Rr_UIHash;
typedef uint16_t Rr_UIIndex;

typedef struct Rr_UIVertex Rr_UIVertex;
struct Rr_UIVertex
{
    Rr_Vec2 Position;
    Rr_Vec2 UV;
    Rr_Vec4 Color;
};

typedef Rr_UIVertex *Rr_UIQuad; /* Implies 4 allocated vertices. */

typedef struct Rr_UIPrimitive Rr_UIPrimitive;
struct Rr_UIPrimitive
{
    Rr_UIVertex *Vertices;
    Rr_UIIndex *Indices;
    Rr_UIIndex BaseVertex;
};

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
    uint32_t IndexCount;
    uint32_t FirstIndex;
    Rr_Rect Rect;
};

typedef struct Rr_UIWindow Rr_UIWindow;
struct Rr_UIWindow
{
    const char *Title;
    Rr_UIHash Hash;
    Rr_UIWindowFlags Flags;
    Rr_Rect Rect;
    Rr_Vec2 ContentsStart;
    Rr_Vec2 ContentsEnd;
    float VScroll;
    int32_t Z;
    bool Minimized : 1;
    bool Added : 1;
    bool SkipDueToAutoResize : 1;
    bool Open : 1;

    Rr_Map *WidgetMap;

    RR_ARRAY(Rr_UIClipRect) ClipRects;
};

typedef enum
{
    RR_UI_DRAG_OP_NONE,
    RR_UI_DRAG_OP_MOVE,
    RR_UI_DRAG_OP_RESIZE,
    RR_UI_DRAG_OP_SCROLL,
    RR_UI_DRAG_OP_WIDGET,
} Rr_UIDragOp;

typedef struct Rr_UILayout Rr_UILayout;
struct Rr_UILayout
{
    Rr_UIWindow *Window;

    Rr_Vec2 Cursor;

    float HorizontalX;
    float HorizontalMaxHeight;

    Rr_Vec2 ContentsPadding;

    float AvailableContentsWidth;

    /* TODO: See if there is a better way to do it. */
    Rr_Vec4 DeferredResizeHandleColor;

    Rr_Vec2 TabCursor;
    Rr_UIHash *SelectedTabHash;
};

struct Rr_UIContext
{
    Rr_UIStyle Style;

    Rr_Map *WindowMap;
    int32_t TotalWindowCount;
    RR_ARRAY(Rr_UIWindow *) ActiveWindows;
    Rr_UIWindow *HoveredWindow;
    Rr_UIWindow *HighestWindow;

    Rr_UIWindow PopupWindow;
    Rr_UIWindow *PopupWindowParent;
    Rr_UIHash PopupWindowHash;
    bool PopupWindowOpen;

    RR_ARRAY(Rr_UILayout) Stack;

    Rr_Vec2 NextWindowSize;
    Rr_Vec2 NextWindowPosition;
    Rr_Vec2 NextWindowPadding;

    bool LeftMouseButtonDownOverWindow : 1;

    bool SkipLeftMouseButtonUp : 1;
    bool LeftMouseButtonDown : 1;
    bool LeftMouseButtonHeld : 1;
    bool LeftMouseButtonUp : 1;
    uint8_t LeftMouseButtonClicks;
    uint32_t LeftMouseButtonClickId;
    bool MouseMoved : 1;
    Rr_Vec2 MousePosition;
    Rr_Vec2 MouseWheelDelta;

    Rr_UIWindow *FocusedWindow;
    Rr_UIHash FocusedWidgetHash;
    Rr_UIWindow *PrevFocusedWindow;
    Rr_UIHash PrevFocusedWidgetHash;

    Rr_UIDragOp DragOp;
    Rr_UIWindow *DragOpWindow;
    Rr_UIHash DragOpHash;
    Rr_Vec2 DragOpMouseStart;
    Rr_Vec2 DragOpWindowStart;
    bool DragOpBeganThisFrame;
    bool DragOpEndedThisFrame;

    /* NOTE: Cursors are stored as raw offsets into UTF-8 string. */

    size_t TextInputCursorBegin;
    size_t TextInputCursorEnd;
    size_t TextInputCursorMaxCol;
    uint64_t TextInputCursorBlinkTime;
    uint32_t TextInputClickId;
    bool MouseOverTextInput : 1;
    RR_ARRAY(const char *) TextInputEvents;
    RR_ARRAY(char) TextInputBuffer;

    RR_ARRAY(Rr_KeyEvent) KeyboardInputEvents;

    Rr_Vec2 ScreenSize;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    RR_FREE_LIST(Rr_UIFont) Fonts;
    Rr_UIFont *Font;
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
    float BevelThickness;

    Rr_Buffer *VertexBuffer;
    Rr_Buffer *IndexBuffer;

    RR_ARRAY(Rr_UIVertex) Vertices;
    RR_ARRAY(Rr_UIIndex) Indices;

    Rr_Buffer *UniformBuffer;

    Rr_Sampler *Sampler;

    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UIContext *gUIContext;

#define RR_UI_ROUND(Value) (ceilf((Value) / 2.0f) * 2.0f)

#define RR_UI_ROUND_V2(Value) \
    (Rr_V2(RR_UI_ROUND((Value).X), RR_UI_ROUND((Value).Y)))

struct Rr_UIFont
{
    Rr_UIGlyph Glyphs[RR_TEXT_MAX_GLYPHS];
    float Advances[RR_TEXT_MAX_GLYPHS];
    Rr_Image2D *Atlas;
    float LineHeight;
    float DefaultSize;
    float Advance;
    float DistanceRange;
    float UnderlineY;
    float UnderlineThickness;
};

Rr_UIFont *Rr_UICreateFont(
    Rr_UIContext *Context,
    Rr_Graph *Graph,
    Rr_AssetRef FontPNGRef,
    Rr_AssetRef FontJSONRef)
{
#define CJSON_GET_OBJECT_FLOAT(Object, Item) \
    ((float)cJSON_GetNumberValue(cJSON_GetObjectItem(Object, Item)))

    Rr_Asset FontJSON = Rr_LoadAsset(FontJSONRef);

    cJSON *FontDataJSON =
        cJSON_ParseWithLength(FontJSON.Pointer, FontJSON.Size);

    cJSON *AtlasJSON = cJSON_GetObjectItem(FontDataJSON, "atlas");
    cJSON *MetricsJSON = cJSON_GetObjectItem(FontDataJSON, "metrics");

    Rr_Vec2 AtlasSize;
    AtlasSize.X = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "width");
    AtlasSize.Y = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "height");

    Rr_Asset ImageAsset = Rr_LoadAsset(FontPNGRef);

    Rr_UIFont *Font = RR_GET_FREE_LIST_ITEM(&Context->Fonts, Context->Arena);
    *Font = (Rr_UIFont){
        .Atlas = Rr_CreateSTBImage2D(
            Rr_GetGraph(),
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            ImageAsset.Size,
            ImageAsset.Pointer),
        .LineHeight = CJSON_GET_OBJECT_FLOAT(MetricsJSON, "lineHeight"),
        .UnderlineY = CJSON_GET_OBJECT_FLOAT(MetricsJSON, "underlineY"),
        .UnderlineThickness =
            CJSON_GET_OBJECT_FLOAT(MetricsJSON, "underlineThickness"),
        .DefaultSize = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "size"),
        .DistanceRange = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "distanceRange"),
    };

    cJSON *GlyphsJSON = cJSON_GetObjectItem(FontDataJSON, "glyphs");

    size_t GlyphCount = (size_t)cJSON_GetArraySize(GlyphsJSON);
    for (size_t GlyphIndex = 0; GlyphIndex < GlyphCount; ++GlyphIndex)
    {
        cJSON *GlyphJSON = cJSON_GetArrayItem(GlyphsJSON, (int32_t)GlyphIndex);

        uint32_t Codepoint =
            (uint32_t)CJSON_GET_OBJECT_FLOAT(GlyphJSON, "unicode");

        Rr_UIGlyph *Glyph = &Font->Glyphs[Codepoint];

        cJSON *AtlasBoundsJSON = cJSON_GetObjectItem(GlyphJSON, "atlasBounds");
        if (cJSON_IsObject(AtlasBoundsJSON))
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
        if (cJSON_IsObject(PlaneBoundsJSON))
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
#undef CJSON_GET_OBJECT_FLOAT
}

void Rr_UIReleaseFont(Rr_UIContext *Context, Rr_UIFont *Font)
{
    Rr_ReleaseImage(Font->Atlas);

    RR_RETURN_FREE_LIST_ITEM(&Context->Fonts, Font);
}

static inline Rr_UILayout *Rr_UICurrentLayout(void)
{
    return gUIContext->Stack.Count > 0
               ? &gUIContext->Stack.Data[gUIContext->Stack.Count - 1]
               : NULL;
}

static inline Rr_UIWindow *Rr_UICurrentWindow(void)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    return Layout ? Layout->Window : NULL;
}

static inline Rr_UIHash Rr_UICurrentHash(void)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    return Layout ? Layout->Window->Hash : 0;
}

static inline Rr_Rect Rr_UICurrentRect(Rr_UIWindow *Window)
{
    return Window
               ? (Window->ClipRects.Count > 0
                      ? Window->ClipRects.Data[Window->ClipRects.Count - 1].Rect
                      : Window->Rect)
               : (Rr_Rect){ 0 };
}

static inline bool Rr_UIIsHorizontal(void)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    return Layout && Layout->HorizontalX != INFINITY;
}

static inline void Rr_UIAssertNoWindow(void)
{
    assert(
        Rr_UICurrentWindow() == NULL &&
        "Did you forget to call Rr_EndWindow()?");
}

static inline void Rr_UIAssertWindow(void)
{
    assert(
        Rr_UICurrentWindow() != NULL &&
        "Did you forget to call Rr_BeginWindow()?");
}

static inline Rr_UIHash Rr_UIGetHash(
    const char *String,
    size_t Length,
    Rr_UIHash Seed)
{
    return XXH3_64bits_withSeed(String, Length, ~Seed);
}

static inline Rr_UIHash Rr_UIGetTitleHash(
    const char *CString,
    size_t *OutLength)
{
    Rr_UIHash Hash;
    size_t FullLength = strlen(CString);
    const char *ExplicitID = strstr(CString, "###");
    if (ExplicitID)
    {
        ExplicitID += 3;
        assert(
            (ExplicitID < (CString + FullLength)) &&
            "Empty ID after ### sentinel!");

        size_t IDLength = FullLength - (size_t)(ExplicitID - CString);

        Hash = Rr_UIGetHash(ExplicitID, IDLength, Rr_UICurrentHash());

        if (OutLength)
        {
            *OutLength = FullLength - IDLength - 3;
        }
    }
    else
    {
        Hash = Rr_UIGetHash(CString, FullLength, Rr_UICurrentHash());

        if (OutLength)
        {
            *OutLength = FullLength;
        }
    }

    return Hash;
}

static inline Rr_UIPrimitive Rr_UIReservePrimitive(
    size_t VertexCount,
    size_t IndexCount)
{
    Rr_UIIndex BaseVertex = (Rr_UIIndex)gUIContext->Vertices.Count;
    return (Rr_UIPrimitive){
        .Vertices = RR_PUSH_INTO_ARRAY_MANY(
            &gUIContext->Vertices,
            VertexCount,
            gUIContext->FrameArena),
        .Indices = RR_PUSH_INTO_ARRAY_MANY(
            &gUIContext->Indices,
            IndexCount,
            gUIContext->FrameArena),
        .BaseVertex = BaseVertex,
    };
}

static inline Rr_UIVertex *Rr_UIReserveQuads(size_t Count)
{
    Rr_UIIndex Base = (Rr_UIIndex)gUIContext->Vertices.Count;
    static const Rr_UIIndex QUAD_INDICES[] = { 0, 1, 2, 1, 3, 2 };

    Rr_UIVertex *FirstVertex = RR_PUSH_INTO_ARRAY_MANY(
        &gUIContext->Vertices,
        Count * 4,
        gUIContext->FrameArena);

    size_t IndexCount = Count * 6;
    Rr_UIIndex *FirstIndex = RR_PUSH_INTO_ARRAY_MANY(
        &gUIContext->Indices,
        IndexCount,
        gUIContext->FrameArena);

    for (size_t QuadIndex = 0; QuadIndex < Count; ++QuadIndex)
    {
        for (size_t Index = 0; Index < 6; ++Index)
        {
            *FirstIndex = Base + QUAD_INDICES[Index];
            FirstIndex++;
        }
        Base += 4;
    }

    return FirstVertex;
}

static inline Rr_UIQuad Rr_UIReserveQuad(void)
{
    return Rr_UIReserveQuads(1);
}

#define RR_UI_BEVEL_VERTEX_COUNT (16)
#define RR_UI_BEVEL_INDEX_COUNT  (30)

static inline Rr_UIPrimitive Rr_UIReserveBevel(void)
{
    return Rr_UIReservePrimitive(
        RR_UI_BEVEL_VERTEX_COUNT,
        RR_UI_BEVEL_INDEX_COUNT);
}

static inline void Rr_UIBevelEx(
    Rr_UIPrimitive Primitive,
    Rr_Rect *Rect,
    Rr_Vec4 *Colors,
    bool Pressed)
{
    Rr_Vec4 BaseColor = Colors[0];

    Rr_Vec4 TopLeftColor;
    TopLeftColor.RGB = Rr_LerpV3(
        BaseColor.RGB,
        Pressed ? gUIContext->Style.BevelIntensityDark
                : gUIContext->Style.BevelIntensityLight,
        Rr_V3F(Pressed ? 0.0f : 1.0f));
    TopLeftColor.A = 1.0f;
    Rr_Vec4 BottomRightColor;

    BottomRightColor.RGB = Rr_LerpV3(
        BaseColor.RGB,
        Pressed ? gUIContext->Style.BevelIntensityLight
                : gUIContext->Style.BevelIntensityDark,
        Rr_V3F(Pressed ? 1.0f : 0.0f));
    BottomRightColor.A = 1.0f;

    Rr_Vec4 BackgroundColor = BaseColor;
    if (Pressed)
    {
        BackgroundColor.RGB =
            Rr_LerpV3(BackgroundColor.RGB, 0.4f, Rr_V3F(0.0f));
    }
    BackgroundColor.A = 1.0f;

    Rr_UIVertex *Vertices = Primitive.Vertices;
    Rr_UIIndex *Indices = Primitive.Indices;

    float Thickness = gUIContext->BevelThickness;

    float Width = Rect->Extent.Width;
    float Height = Rect->Extent.Height;
    float TempWidth = Rect->Extent.Width - Thickness;
    float TempHeight = Rect->Extent.Height - Thickness;

    Vertices[0] = (Rr_UIVertex){
        .Position = Rect->Offset,
        .Color = TopLeftColor,
    };
    Vertices[1] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(Width, 0.0f)),
        .Color = TopLeftColor,
    };
    Vertices[2] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2F(Thickness)),
        .Color = TopLeftColor,
    };
    Vertices[3] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(TempWidth, Thickness)),
        .Color = TopLeftColor,
    };
    Vertices[4] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(0.0f, Height)),
        .Color = TopLeftColor,
    };
    Vertices[5] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(Thickness, TempHeight)),
        .Color = TopLeftColor,
    };

    Vertices[6] = (Rr_UIVertex){
        .Position = Vertices[3].Position,
        .Color = BottomRightColor,
    };
    Vertices[7] = (Rr_UIVertex){
        .Position = Vertices[1].Position,
        .Color = BottomRightColor,
    };
    Vertices[8] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(TempWidth, TempHeight)),
        .Color = BottomRightColor,
    };
    Vertices[9] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rect->Extent),
        .Color = BottomRightColor,
    };
    Vertices[10] = (Rr_UIVertex){
        .Position = Vertices[4].Position,
        .Color = BottomRightColor,
    };
    Vertices[11] = (Rr_UIVertex){
        .Position = Vertices[5].Position,
        .Color = BottomRightColor,
    };

    Vertices[12] = (Rr_UIVertex){
        .Position = Vertices[2].Position,
        .Color = Colors[0],
    };
    Vertices[13] = (Rr_UIVertex){
        .Position = Vertices[3].Position,
        .Color = Colors[1],
    };
    Vertices[14] = (Rr_UIVertex){
        .Position = Vertices[5].Position,
        .Color = Colors[2],
    };
    Vertices[15] = (Rr_UIVertex){
        .Position = Vertices[8].Position,
        .Color = Colors[3],
    };

    static const Rr_UIIndex BEVEL_INDICES[] = {
        0, 1, 2, 1,  3,  2, 0,  2, 4, 2,  5,  4,  6,  7,  8,
        7, 9, 8, 10, 11, 9, 11, 8, 9, 12, 13, 14, 13, 15, 14,
    };

    for (size_t Index = 0; Index < RR_UI_BEVEL_INDEX_COUNT; ++Index)
    {
        Indices[Index] = Primitive.BaseVertex + BEVEL_INDICES[Index];
    }
}

static inline void Rr_UIBevel(
    Rr_UIPrimitive Primitive,
    Rr_Rect *Rect,
    Rr_Vec4 *BaseColor,
    bool Pressed)
{
    Rr_Vec4 Colors[4] = { *BaseColor, *BaseColor, *BaseColor, *BaseColor };
    Rr_UIBevelEx(Primitive, Rect, Colors, Pressed);
}

static inline void Rr_UIDrawBevel(
    Rr_Rect *Rect,
    Rr_Vec4 *BaseColor,
    bool Pressed)
{
    Rr_UIBevel(
        Rr_UIReservePrimitive(
            RR_UI_BEVEL_VERTEX_COUNT,
            RR_UI_BEVEL_INDEX_COUNT),
        Rect,
        BaseColor,
        Pressed);
}

static inline void Rr_UIDrawQuad(Rr_UIVertex *Vertices)
{
    Rr_UIIndex Base = (Rr_UIIndex)gUIContext->Vertices.Count;
    Rr_UIIndex Indices[] = {
        Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2,
    };
    for (size_t Index = 0; Index < 4; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gUIContext->Vertices, gUIContext->FrameArena) =
            Vertices[Index];
    }

    for (size_t Index = 0; Index < 6; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gUIContext->Indices, gUIContext->FrameArena) =
            Indices[Index];
    }
}

static inline void Rr_UISolidQuad(Rr_UIQuad Quad, Rr_Rect *Rect, Rr_Vec4 *Color)
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

static inline void Rr_UIRotatedQuad(
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

static inline void Rr_UIHorizontalGradientQuad(
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

static inline void Rr_UIDrawSolidTriangle(Rr_Vec2 *Positions, Rr_Vec4 *Color)
{
    assert(Positions != NULL);

    Rr_UIVertex Vertices[3] = {
        {
            .Position = Positions[0],
            .Color = *Color,
        },
        {
            .Position = Positions[1],
            .Color = *Color,
        },
        {
            .Position = Positions[2],
            .Color = *Color,
        },
    };
    for (size_t Index = 0; Index < 3; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gUIContext->Indices, gUIContext->FrameArena) =
            (Rr_UIIndex)(gUIContext->Vertices.Count + Index);
    }

    for (size_t Index = 0; Index < 3; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gUIContext->Vertices, gUIContext->FrameArena) =
            Vertices[Index];
    }
}

static inline void Rr_UIDrawRotatedTriangle(
    Rr_Vec2 Center,
    float Extent,
    Rr_Vec4 *Color,
    float Angle)
{
    Rr_Vec2 Base = { Extent, 0.0f };
    float Third = RR_PI32 * 2.0f / 3.0f;
    Rr_Vec2 Positions[3] = {
        Rr_AddV2(Center, Rr_RotateV2(Base, Angle)),
        Rr_AddV2(Center, Rr_RotateV2(Base, Angle + Third)),
        Rr_AddV2(Center, Rr_RotateV2(Base, Angle + Third * 2.0f)),
    };
    Rr_UIDrawSolidTriangle(Positions, Color);
}

static inline void Rr_UIDrawSolidQuad(Rr_Rect *Rect, Rr_Vec4 *Color)
{
    Rr_UIVertex Vertices[4];
    Rr_UISolidQuad(Vertices, Rect, Color);
    Rr_UIDrawQuad(Vertices);
}

static inline void Rr_UIDrawRotatedQuad(
    Rr_Rect *Rect,
    float Angle,
    Rr_Vec4 *Color)
{
    Rr_UIVertex Vertices[4];
    Rr_UIRotatedQuad(Vertices, Rect, Angle, Color);
    Rr_UIDrawQuad(Vertices);
}

static inline void Rr_UIDrawHorizontalGradientQuad(
    Rr_Rect *Rect,
    Rr_Vec4 *ColorA,
    Rr_Vec4 *ColorB)
{
    Rr_UIVertex Vertices[4];
    Rr_UIHorizontalGradientQuad(Vertices, Rect, ColorA, ColorB);
    Rr_UIDrawQuad(Vertices);
}

static inline void Rr_UIDrawRect(Rr_Rect *Rect, Rr_Vec4 *Color)
{
    Rr_UIQuad Quad = Rr_UIReserveQuad();
    Rr_UISolidQuad(Quad, Rect, Color);
}

static inline void Rr_UIDrawOuterFrame(
    Rr_Rect *Rect,
    float Thickness,
    Rr_Vec4 *Color)
{
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            { Rect->Offset.X, Rect->Offset.Y - Thickness },
            { Rect->Extent.Width, Thickness },
        },
        Color); /* Top */
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            { Rect->Offset.X, Rect->Offset.Y + Rect->Extent.Height },
            { Rect->Extent.Width, Thickness },
        },
        Color); /* Bottom */
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            { Rect->Offset.X - Thickness, Rect->Offset.Y - Thickness },
            { Thickness, Rect->Extent.Height + Thickness * 2.0f },
        },
        Color); /* Left */
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            { Rect->Offset.X + Rect->Extent.Width, Rect->Offset.Y - Thickness },
            { Thickness, Rect->Extent.Height + Thickness * 2.0f },
        },
        Color); /* Right */
}

static inline void Rr_UIDrawInnerFrame(
    Rr_Rect *Rect,
    float Thickness,
    Rr_Vec4 *Color)
{
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            { Rect->Offset.X, Rect->Offset.Y },
            { Rect->Extent.Width, Thickness },
        },
        Color); /* Top */
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            { Rect->Offset.X,
              Rect->Offset.Y + Rect->Extent.Height - Thickness },
            { Rect->Extent.Width, Thickness },
        },
        Color); /* Bottom */
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            { Rect->Offset.X, Rect->Offset.Y + Thickness },
            { Thickness, Rect->Extent.Height - Thickness * 2.0f },
        },
        Color); /* Left */
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            { Rect->Offset.X + Rect->Extent.Width - Thickness,
              Rect->Offset.Y + Thickness },
            { Thickness, Rect->Extent.Height - Thickness * 2.0f },
        },
        Color); /* Right */
}

static inline void Rr_UIDrawTexturedQuad(
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

    Rr_UIDrawQuad(Vertices);
}

static inline void Rr_UIDrawGlyph(
    Rr_UIFont *Font,
    float FontSize,
    Rr_UIGlyph *Glyph,
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

    Rr_UIDrawTexturedQuad(
        &(Rr_Rect){
            Rr_AddV2(Position, (Rr_Vec2){ Left, Top }),
            { Width, Height },
        },
        Color,
        UVs);
}

static inline void Rr_UIDrawInteractiveTextCursor(
    Rr_Vec2 Position,
    Rr_Vec4 *Color)
{
    uint64_t TimeDelta = Rr_GetTimeMS() - gUIContext->TextInputCursorBlinkTime;
    if ((TimeDelta / 500) % 2 == 0)
    {
        Rr_UIDrawRect(
            &(Rr_Rect){
                Position,
                Rr_V2(
                    gUIContext->FrameThickness * 2.0f,
                    gUIContext->LineHeight),
            },
            Color);
    }
}

static inline Rr_Vec2 Rr_UIDrawInputText(
    const char *CString,
    bool Active,
    Rr_Vec2 Position,
    size_t CursorBegin,
    size_t *CursorEnd,
    float AvailableWidth,
    Rr_Vec4 *Color)
{
    Rr_UIFont *Font = gUIContext->Font;
    float FontSize = gUIContext->FontSize;
    float LineHeight = Font->LineHeight * FontSize;
    float MaxX = 0.0f;
    float CurrentX = 0.0f;
    float CurrentY = 0.0f;
    uint32_t LineIndex = 0;

    Rr_Vec2 MousePosition = gUIContext->MousePosition;
    Rr_Vec2 MouseOffset = Rr_SubV2(MousePosition, Position);
    uint32_t MouseLineIndex = RR_MAX(0, (uint32_t)(MouseOffset.Y / LineHeight));
    float MouseCharacterDistance = FLT_MAX;

    size_t NewCursorEnd = *CursorEnd;
    size_t OldCursorMin = RR_MIN(CursorBegin, *CursorEnd);
    size_t OldCursorMax = RR_MAX(CursorBegin, *CursorEnd);

    Rr_Vec2 ResultSize = { 0 };

    Rr_UTF8Decoder Decoder = { .CString = CString };
    while (true)
    {
        Rr_UTF8Decode(&Decoder);
        uint32_t Codepoint = Decoder.Codepoint;
        size_t CodepointIndex = Decoder.CodepointIndex - 1;
        size_t CStringIndex = Decoder.CStringIndex - 1;

        bool LineBreak = false;

        if (Codepoint >= RR_TEXT_MAX_GLYPHS)
        {
            RR_ABORT("Codepoint is not within range!");
        }

        if (Codepoint == '\n')
        {
            Codepoint = ' ';
            LineBreak = true;
        }

        Rr_Vec2 GlyphPosition =
            Rr_AddV2(Position, (Rr_Vec2){ CurrentX, CurrentY });

        if (LineIndex == MouseLineIndex)
        {
            float Distance = fabsf(GlyphPosition.X - MousePosition.X);
            if (Distance < MouseCharacterDistance)
            {
                MouseCharacterDistance = Distance;
                NewCursorEnd = CStringIndex;
            }
        }

        if (Active)
        {
            if ((OldCursorMin != OldCursorMax) &&
                (CodepointIndex >= OldCursorMin &&
                 CodepointIndex < OldCursorMax))
            {
                Rr_UIDrawRect(
                    &(Rr_Rect){
                        GlyphPosition,
                        Rr_V2(
                            gUIContext->Font->Advances[Codepoint] * FontSize,
                            gUIContext->LineHeight),
                    },
                    &gUIContext->Style.SelectedTextBackground);
            }

            if (*CursorEnd == CStringIndex)
            {
                Rr_UIDrawInteractiveTextCursor(GlyphPosition, Color);
            }
        }

        if (Codepoint == '\0')
        {
            break;
        }

        if (gUIContext->Font->Advances[Codepoint] == 0.0f)
        {
            /* TODO: Proper missing glyph handling! */

            CurrentX += FontSize;
            MaxX = RR_MAX(MaxX, CurrentX);

            Rr_Rect MissingGlyphRect = { GlyphPosition,
                                         Rr_V2(FontSize, LineHeight) };
            MissingGlyphRect = Rr_ResizeRect(&MissingGlyphRect, 1.0f);
            Rr_UIDrawInnerFrame(&MissingGlyphRect, 1.0f, Color);
        }
        else
        {
            if (Codepoint != ' ')
            {
                Rr_UIDrawGlyph(
                    Font,
                    FontSize,
                    &Font->Glyphs[Codepoint],
                    GlyphPosition,
                    Color);
            }

            CurrentX += gUIContext->Font->Advances[Codepoint] * FontSize;
            MaxX = RR_MAX(MaxX, CurrentX);

            if (LineBreak)
            {
                CurrentX = 0.0f;
                CurrentY += LineHeight;
                LineIndex++;
            }
        }
    }

    *CursorEnd = NewCursorEnd;

    return (Rr_Vec2){ .Width = MaxX, .Height = CurrentY + LineHeight };
}

static inline Rr_Vec2 Rr_UIDrawText(
    bool CalculateOnly,
    Rr_Vec2 Position,
    size_t UTF8StringLength,
    const char *UTF8String,
    float AvailableWidth,
    Rr_Vec4 *Color,
    Rr_UITextFlags Flags)
{
    if (UTF8StringLength == 0)
    {
        return Rr_V2F(0.0f);
    }

    bool NullTerminated = false;
    if (UTF8StringLength == SIZE_MAX)
    {
        NullTerminated = true;
    }

    Rr_UIFont *Font = gUIContext->Font;
    float FontSize = gUIContext->FontSize;
    float LineHeight = Font->LineHeight * FontSize;
    float MaxX = 0.0f;
    float CurrentX = 0.0f;
    float CurrentY = 0.0f;

    bool Wrapped = RR_HAS_BIT(Flags, RR_UI_TEXT_FLAGS_WRAPPED_BIT);
    assert(
        (!Wrapped || AvailableWidth >= FontSize) &&
        "Available width must be larger than font size!");

    Rr_Vec2 ResultSize = { 0 };

    if (Wrapped)
    {
        Rr_Scratch Scratch = Rr_GetScratch(NULL);

        float CurrentWordWidth = 0.0f;
        size_t CurrentWordStart = 0;

        /* TODO: See if it's possible to avoid allocating this much upfront */

        Rr_UTF8Decoder Decoder = { .CString = UTF8String };
        uint32_t *Decoded = RR_ALLOC_NO_ZERO(
            Scratch.Arena,
            sizeof(uint32_t) * UTF8StringLength);
        while (Rr_UTF8Decode(&Decoder) != '\0' &&
               (NullTerminated || Decoder.CStringIndex <= UTF8StringLength))
        {
            uint32_t Codepoint = Decoder.Codepoint;
            size_t CodepointIndex = Decoder.CodepointIndex - 1;
            Decoded[CodepointIndex] = Codepoint;

            if (Codepoint >= RR_TEXT_MAX_GLYPHS)
            {
                RR_ABORT("Codepoint is not within range!");
            }

            if (Codepoint == '\n')
            {
                CurrentX = 0.0f;
                CurrentY += LineHeight;
                continue;
            }

            if (Codepoint == ' ' || Codepoint == '\0')
            {
                size_t WordLength = CodepointIndex - CurrentWordStart;
                if (WordLength > 0)
                {
                    if (CurrentWordWidth > AvailableWidth)
                    {
                        /* Fallback to per-character wrapping. */

                        for (size_t IndexInWord = CurrentWordStart;
                             IndexInWord <= CodepointIndex;
                             ++IndexInWord)
                        {
                            Codepoint = Decoded[IndexInWord];
                            if (CurrentX > AvailableWidth)
                            {
                                CurrentX = 0.0f;
                                CurrentY += LineHeight;
                            }
                            if (!CalculateOnly)
                            {
                                Rr_UIDrawGlyph(
                                    Font,
                                    FontSize,
                                    &Font->Glyphs[Codepoint],
                                    Rr_AddV2(
                                        Position,
                                        (Rr_Vec2){ CurrentX, CurrentY }),
                                    Color);
                            }
                            CurrentX += gUIContext->Font->Advances[Codepoint] *
                                        FontSize;
                        }
                    }
                    else
                    {
                        if (CurrentX + CurrentWordWidth > AvailableWidth)
                        {
                            CurrentX = 0.0f;
                            CurrentY += LineHeight;
                        }

                        Rr_Vec2 PositionInWord =
                            Rr_AddV2(Position, (Rr_Vec2){ CurrentX, CurrentY });
                        for (size_t IndexInWord = CurrentWordStart;
                             IndexInWord <= CodepointIndex;
                             ++IndexInWord)
                        {
                            Codepoint = Decoded[IndexInWord];
                            if (!CalculateOnly)
                            {
                                Rr_UIDrawGlyph(
                                    Font,
                                    FontSize,
                                    &Font->Glyphs[Codepoint],
                                    PositionInWord,
                                    Color);
                            }
                            CurrentX += gUIContext->Font->Advances[Codepoint] *
                                        FontSize;
                            PositionInWord.X = Position.X + CurrentX;
                        }
                    }
                }
                else
                {
                    CurrentX +=
                        gUIContext->Font->Advances[Codepoint] * FontSize;
                }

                MaxX = RR_MAX(MaxX, CurrentX);

                CurrentWordWidth = 0.0f;
                CurrentWordStart = CodepointIndex + 1;
            }
            else
            {
                CurrentWordWidth +=
                    gUIContext->Font->Advances[Codepoint] * FontSize;
            }
        }

        Rr_DestroyScratch(Scratch);
    }
    else
    {
        Rr_UTF8Decoder Decoder = { .CString = UTF8String };
        while (Rr_UTF8Decode(&Decoder) != '\0' &&
               (NullTerminated || Decoder.CStringIndex <= UTF8StringLength))
        {
            uint32_t Codepoint = Decoder.Codepoint;
            size_t CodepointIndex = Decoder.CodepointIndex - 1;

            if (Codepoint >= RR_TEXT_MAX_GLYPHS)
            {
                RR_ABORT("Codepoint is not within range!");
            }

            if (Codepoint == '\n')
            {
                CurrentX = 0.0f;
                CurrentY += LineHeight;
                continue;
            }

            if (Codepoint == ' ')
            {
                CurrentX += gUIContext->Font->Advances[Codepoint] * FontSize;
                continue;
            }

            if (!CalculateOnly)
            {
                Rr_UIDrawGlyph(
                    Font,
                    FontSize,
                    &Font->Glyphs[Codepoint],
                    Rr_AddV2(Position, (Rr_Vec2){ CurrentX, CurrentY }),
                    Color);
            }

            CurrentX += gUIContext->Font->Advances[Codepoint] * FontSize;
            MaxX = RR_MAX(MaxX, CurrentX);
        }
    }

    return (Rr_Vec2){ .Width = MaxX, .Height = CurrentY + LineHeight };
}

static inline void Rr_UIBeginDragOp(
    Rr_UIWindow *Window,
    Rr_UIDragOp DragOp,
    Rr_UIHash Hash,
    Rr_Vec2 WindowStart)
{
    gUIContext->DragOpMouseStart = gUIContext->MousePosition;
    gUIContext->DragOpWindow = Window;
    gUIContext->DragOp = DragOp;
    gUIContext->DragOpWindowStart = WindowStart;
    gUIContext->DragOpHash = Hash;
    gUIContext->DragOpBeganThisFrame = true;
}

static inline void Rr_UIEndDragOp(void)
{
    gUIContext->DragOpWindow = NULL;
    gUIContext->DragOp = 0;
    gUIContext->DragOpEndedThisFrame = true;
}

static inline bool Rr_UIRectContains(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    Rr_Vec2 Point)
{
    assert(Window != NULL);
    Rr_Rect CurrentRect = Rr_UICurrentRect(Window);
    return Rr_RectContains(&CurrentRect, Point) && Rr_RectContains(Rect, Point);
}

static inline bool Rr_UIIsFocused(Rr_UIWindow *Window, Rr_UIHash Hash)
{
    return gUIContext->FocusedWindow == Window &&
           gUIContext->FocusedWidgetHash == Hash;
}

static inline bool Rr_UIWasFocused(Rr_UIWindow *Window, Rr_UIHash Hash)
{
    bool Result = gUIContext->PrevFocusedWindow == Window &&
                  gUIContext->PrevFocusedWidgetHash == Hash;
    if (Result)
    {
        gUIContext->PrevFocusedWindow = NULL;
    }

    return Result;
}

static inline void Rr_UISetFocus(Rr_UIWindow *Window, Rr_UIHash Hash)
{
    if (Rr_UIIsFocused(Window, Hash))
    {
        return;
    }
    if (gUIContext->FocusedWindow != Window ||
        gUIContext->FocusedWidgetHash != Hash)
    {
        gUIContext->PrevFocusedWindow = gUIContext->FocusedWindow;
        gUIContext->PrevFocusedWidgetHash = gUIContext->FocusedWidgetHash;
    }
    gUIContext->FocusedWindow = Window;
    gUIContext->FocusedWidgetHash = Hash;
}

static inline bool Rr_UIScrollBehavior(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    float *YScroll)
{
    if (Window == gUIContext->HoveredWindow &&
        gUIContext->DragOpWindow == NULL &&
        Rr_UIRectContains(Window, Rect, gUIContext->MousePosition))
    {
        if (gUIContext->MouseWheelDelta.Y != 0.0f)
        {
            Rr_UIEndDragOp();
            *YScroll = *YScroll +
                       gUIContext->MouseWheelDelta.Y * gUIContext->LineHeight;

            return true;
        }
    }

    return false;
}

static inline void Rr_UIButtonBehavior(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    bool *Down,
    bool *Up,
    bool *Hovered,
    bool *Held)
{
    if (Down)
    {
        *Down = false;
    }
    if (Up)
    {
        *Up = false;
    }
    if (Hovered)
    {
        *Hovered = false;
    }
    if (Held)
    {
        *Held = false;
    }
    bool BlockedByDragOp = (gUIContext->DragOpWindow != NULL &&
                            gUIContext->DragOpBeganThisFrame == false) ||
                           gUIContext->DragOpEndedThisFrame == true;
    if (BlockedByDragOp == false && Window == gUIContext->HoveredWindow &&
        Rr_UIRectContains(Window, Rect, gUIContext->MousePosition))
    {
        if (gUIContext->LeftMouseButtonDown)
        {
            Rr_UIEndDragOp();
            gUIContext->DragOpWindow = NULL;
            /* Rr_UIResetFocus(); */
        }
        if (Down)
        {
            *Down = gUIContext->LeftMouseButtonDown;
        }
        if (Up)
        {
            *Up = gUIContext->LeftMouseButtonUp;
        }
        if (Held)
        {
            *Held = gUIContext->LeftMouseButtonHeld;
        }
        if (Hovered)
        {
            *Hovered = true;
        }
    }
}

static inline bool Rr_UIDragBehavior(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    Rr_UIDragOp DragOp,
    Rr_UIHash Hash,
    Rr_Vec2 Value,
    bool *Hovered,
    bool *Began)
{
    bool Contains = Rr_UIRectContains(Window, Rect, gUIContext->MousePosition);
    if (Hovered)
    {
        *Hovered = Contains && gUIContext->HoveredWindow == Window;
    }
    if (Began)
    {
        *Began = false;
    }

    /* NOTE: Dragging resize handle also overlaps with moving and scrolling.
     * Take that into accoutn and override current drag opertion. Watch out for
     * Rr_DragBehavior() order!
     * Also, faster mouse movements may actually result in Contains == false
     * while the drag operation is still going. */

    if (Contains && gUIContext->LeftMouseButtonDown &&
        (gUIContext->DragOpWindow == NULL ||
         gUIContext->DragOpWindow == Window) &&
        gUIContext->HoveredWindow == Window)
    {
        Rr_UIBeginDragOp(Window, DragOp, Hash, Value);

        Rr_UISetFocus(Window, Hash);

        if (Began)
        {
            *Began = true;
        }

        return false;
    }

    if (gUIContext->DragOpWindow == Window && gUIContext->DragOp == DragOp &&
        gUIContext->DragOpHash == Hash)
    {
        if (gUIContext->LeftMouseButtonHeld)
        {
            return gUIContext->MouseMoved;
        }
        else
        {
            Rr_UIEndDragOp();
        }
    }

    return false;
}

static inline void Rr_UIBeginClipRect(Rr_Rect *Rect)
{
    Rr_UIAssertWindow();

    Rr_UIWindow *Window = Rr_UICurrentWindow();

    Rr_UIClipRect *ClipRect =
        RR_PUSH_INTO_ARRAY(&Window->ClipRects, gUIContext->FrameArena);

    *ClipRect = (Rr_UIClipRect){
        .FirstIndex = (uint32_t)gUIContext->Indices.Count,
        .Rect = { { Rect->Offset.X, Rect->Offset.Y },
                  { Rect->Extent.Width, Rect->Extent.Height } },
    };
}

static inline void Rr_UIEndClipRect(void)
{
    Rr_UIWindow *Window = Rr_UICurrentWindow();
    if (Window)
    {
        if (Window->ClipRects.Count > 0)
        {
            Rr_UIClipRect *Last =
                &Window->ClipRects.Data[Window->ClipRects.Count - 1];
            Last->IndexCount =
                (uint32_t)gUIContext->Indices.Count - Last->FirstIndex;
        }
    }
}

static inline Rr_Vec2 Rr_UIGetMinWindowSize(Rr_UIWindowFlags Flags)
{
    if (RR_HAS_BIT(Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT))
    {
        return gUIContext->MinWindowSizeNoTitle;
    }
    else
    {
        return gUIContext->MinWindowSize;
    }
}

static inline void Rr_UIAddCloseButton(Rr_UIWindow *Window, bool *Open)
{
    /* Assuming having a title bar. */

    bool HasClose = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_CLOSE_BIT);
    if (HasClose == false)
    {
        return;
    }

    float Width = gUIContext->TitleButtonSize * 0.75f;
    float Thickness = gUIContext->TitleButtonSize * 0.15f;
    Rr_Rect TitleRect = Window->Rect;
    TitleRect.Extent.Height = gUIContext->WindowTitleHeight;
    Rr_Rect BarRect;
    Rr_Vec2 Margin = {
        TitleRect.Extent.Width - (TitleRect.Extent.Height + Width) * 0.5f,
        TitleRect.Extent.Height * 0.5f - Thickness * 0.5f,
    };
    BarRect.Offset = Rr_AddV2(TitleRect.Offset, Margin);
    BarRect.Extent = (Rr_Vec2){
        Width,
        Thickness,
    };

    Rr_Rect ButtonRect = BarRect;
    ButtonRect.Offset.X =
        TitleRect.Offset.X + TitleRect.Extent.Width -
        (TitleRect.Extent.Height + gUIContext->TitleButtonSize) * 0.5f,
    ButtonRect.Offset.Y =
        TitleRect.Offset.Y +
        (TitleRect.Extent.Height - gUIContext->TitleButtonSize) * 0.5f;
    ButtonRect.Extent = Rr_V2F(gUIContext->TitleButtonSize);

    bool Up, Held;
    Rr_UIButtonBehavior(Window, &ButtonRect, NULL, &Up, NULL, &Held);
    if (Up && Open)
    {
        *Open = false;
    }

    Rr_UIDrawBevel(&ButtonRect, &gUIContext->Style.TitleButtonBackground, Held);

    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(45.0f),
        &gUIContext->Style.Foreground);
    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(-45.0f),
        &gUIContext->Style.Foreground);
}

static inline void Rr_UIAddWindowTitle(Rr_UIWindow *Window)
{
    Rr_Rect TitleRect = {
        Window->Rect.Offset,
        (Rr_Vec2){
            Window->Rect.Extent.X,
            gUIContext->WindowTitleHeight,
        },
    };
    bool HasClose = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_CLOSE_BIT);
    if (HasClose)
    {
        TitleRect.Extent.Width -= gUIContext->TitleButtonSize;
    }
    Rr_Vec4 ColorB = gUIContext->Style.TitleBackground;
    ColorB.RGB = Rr_LerpV3(ColorB.RGB, 0.25f, (Rr_Vec3){ 0.0f, 0.0f, 0.0f });
    Rr_UIPrimitive BevelPrimitive = Rr_UIReserveBevel();
    Rr_Vec4 Colors[4] = { ColorB,
                          gUIContext->Style.TitleBackground,
                          ColorB,
                          gUIContext->Style.TitleBackground };
    Rr_UIBevelEx(BevelPrimitive, &TitleRect, Colors, false);
    Rr_UIDrawText(
        false,
        Rr_AddV2(
            TitleRect.Offset,
            Rr_MulV2F(gUIContext->Style.TitlePadding, gUIContext->FontSize)),
        SIZE_MAX,
        Window->Title,
        0.0f,
        &gUIContext->Style.Foreground,
        0);
}

static inline bool Rr_UIAddResizeHandle(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 BottomRight = Rr_AddV2(Window->Rect.Offset, Window->Rect.Extent);
    Rr_Rect ResizeHandleRect = (Rr_Rect){
        {
            BottomRight.X - gUIContext->ResizeHandleSize,
            BottomRight.Y - gUIContext->ResizeHandleSize,
        },
        {
            gUIContext->ResizeHandleSize,
            gUIContext->ResizeHandleSize,
        },
    };

    bool Hovered, Dragging = Rr_UIDragBehavior(
                      Window,
                      &ResizeHandleRect,
                      RR_UI_DRAG_OP_RESIZE,
                      0,
                      Window->Rect.Extent,
                      &Hovered,
                      NULL);

    if (Dragging)
    {
        Rr_Vec2 Delta =
            Rr_SubV2(gUIContext->MousePosition, gUIContext->DragOpMouseStart);
        Rr_Vec2 NewWindowSize = Rr_AddV2(gUIContext->DragOpWindowStart, Delta);
        Rr_Vec2 MinWindowSize = Rr_UIGetMinWindowSize(Window->Flags);
        NewWindowSize.X = RR_MAX(NewWindowSize.X, MinWindowSize.X);
        NewWindowSize.Y = RR_MAX(NewWindowSize.Y, MinWindowSize.Y);
        Window->Rect.Extent = Rr_FloorV2(NewWindowSize);
    }

    Layout->DeferredResizeHandleColor = gUIContext->Style.Foreground;
    if (Hovered || Dragging)
    {
        Layout->DeferredResizeHandleColor =
            Rr_MulV4F(Layout->DeferredResizeHandleColor, 0.75f);
    }

    return Dragging;
}

static inline Rr_Rect Rr_UIGetWindowContentsArea(Rr_UIWindow *Window)
{
    Rr_Rect Rect = Window->Rect;

    bool HasTitle =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT) == false;

    if (HasTitle)
    {
        Rect.Offset.Y += gUIContext->WindowTitleHeight;
        Rect.Extent.Height -= gUIContext->WindowTitleHeight;
    }

    bool AutoResize =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);

    if (AutoResize == false)
    {
        bool HasScrollbar =
            RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT) ==
            false;
        float ContentsHeight = Window->ContentsEnd.Y - Window->ContentsStart.Y;
        float FillRatio = ContentsHeight / Rect.Extent.Height;
        if (HasScrollbar && FillRatio > 1.0f)
        {
            Rect.Extent.Width -= gUIContext->ScrollbarWidth;
        }
    }

    return Rect;
}

static inline void Rr_UIAdvance(Rr_Vec2 Size)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    if (Rr_UIIsHorizontal())
    {
        Layout->Cursor.X += Size.Width + gUIContext->HorizontalMargin;
        Layout->HorizontalMaxHeight =
            RR_MAX(Layout->HorizontalMaxHeight, Size.Height);

        Window->ContentsEnd.Y = RR_MAX(
            Window->ContentsEnd.Y,
            Layout->Cursor.Y + Layout->HorizontalMaxHeight);
        Window->ContentsEnd.X = RR_MAX(Window->ContentsEnd.X, Layout->Cursor.X);
    }
    else
    {
        Layout->Cursor.Y += Size.Height;

        Window->ContentsEnd.Y = RR_MAX(Window->ContentsEnd.Y, Layout->Cursor.Y);
        Window->ContentsEnd.X =
            RR_MAX(Window->ContentsEnd.X, Layout->Cursor.X + Size.X);
    }
}

static inline bool Rr_UIAddVerticalScrollbar(Rr_UIWindow *Window)
{
    bool HasScrollbar =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT) == false;
    if (HasScrollbar != true)
    {
        return false;
    }

    bool HasResize =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT) == false;

    Rr_Rect ContentsAreaRect = Rr_UIGetWindowContentsArea(Window);
    float ContentsHeight = Window->ContentsEnd.Y - Window->ContentsStart.Y;
    float FillRatio = ContentsAreaRect.Extent.Height / ContentsHeight;
    float MaxYScroll =
        RR_MAX(0.0f, ContentsHeight - ContentsAreaRect.Extent.Height);

    if (FillRatio < 1.0f)
    {
        Rr_Vec2 ScrollbarPosition = ContentsAreaRect.Offset;
        ScrollbarPosition.X += ContentsAreaRect.Extent.Width;
        Rr_Vec2 ScrollbarSize = { gUIContext->ScrollbarWidth,
                                  ContentsAreaRect.Extent.Height };
        Rr_UIDrawSolidQuad(
            &(Rr_Rect){
                ScrollbarPosition,
                ScrollbarSize,
            },
            &gUIContext->Style.ScrollbarBackground);

        float ScrollbarHandleOffset =
            (gUIContext->ScrollbarWidth - gUIContext->ScrollbarHandleWidth) /
            2.0f;

        Rr_Vec2 ScrollbarHandlePosition = ScrollbarPosition;
        Rr_Vec2 ScrollbarHandleSize = ScrollbarSize;
        ScrollbarHandlePosition.X += ScrollbarHandleOffset;
        ScrollbarHandleSize.Width = gUIContext->ScrollbarHandleWidth;
        ScrollbarHandleSize.Height *= FillRatio;

        Rr_Vec2 ScrollableArea = ContentsAreaRect.Extent;
        ScrollableArea.Width += gUIContext->ScrollbarWidth;
        if (Rr_UIScrollBehavior(
                Window,
                &(Rr_Rect){
                    ContentsAreaRect.Offset,
                    ScrollableArea,
                },
                &Window->VScroll))
        {
            Window->VScroll = RR_CLAMP(0.0f, Window->VScroll, MaxYScroll);
            Window->VScroll = roundf(Window->VScroll);
        }

        ScrollbarHandlePosition.Y += Window->VScroll * FillRatio;

        /* Vertical margins. */

        ScrollbarHandlePosition.Y += ScrollbarHandleOffset;
        ScrollbarHandleSize.Height -= ScrollbarHandleOffset * 2.0f;
        ScrollbarHandleSize.Height = RR_MAX(
            ScrollbarHandleSize.Height,
            gUIContext->BevelThickness * 3.0f);

        /* Hitbox is slightly adjusted for better experience. */

        Rr_Vec2 ScrollbarButtonSize = ScrollbarHandleSize;
        ScrollbarButtonSize.Width = ScrollbarSize.Width;
        if (HasResize)
        {
            /* This cuts a bix of height from the scrollbar hitbox so the resize
             * handle is always on top. */

            float AvailableResizeButtonHeight =
                (ScrollbarPosition.Y + ScrollbarSize.Height -
                 gUIContext->ResizeHandleSize) -
                (ScrollbarHandlePosition.Y + ScrollbarHandleSize.Height);
            if (AvailableResizeButtonHeight < 0.0f)
            {
                ScrollbarButtonSize.Height += AvailableResizeButtonHeight;
            }
        }

        bool Hovered, Dragging = Rr_UIDragBehavior(
                          Window,
                          &(Rr_Rect){
                              ScrollbarHandlePosition,
                              ScrollbarButtonSize,
                          },
                          RR_UI_DRAG_OP_SCROLL,
                          0,
                          (Rr_Vec2){ 0.0f, Window->VScroll },
                          &Hovered,
                          NULL);

        if (Dragging)
        {
            Rr_Vec2 Delta = Rr_SubV2(
                gUIContext->MousePosition,
                gUIContext->DragOpMouseStart);
            float ContentsHeight =
                Window->ContentsEnd.Y - Window->ContentsStart.Y;
            float FillRatio = ContentsHeight / ContentsAreaRect.Extent.Height;
            Window->VScroll =
                gUIContext->DragOpWindowStart.Y + (Delta.Y * FillRatio);
        }

        Window->VScroll = RR_CLAMP(0.0f, roundf(Window->VScroll), MaxYScroll);

        Rr_UIDrawBevel(
            &(Rr_Rect){
                ScrollbarHandlePosition,
                ScrollbarHandleSize,
            },
            &gUIContext->Style.ScrollbarNormal,
            false);

        return true;
    }
    else
    {
        Window->VScroll = 0.0f;
    }

    return false;
}

Rr_UIStyle *Rr_UIGetStyle(void)
{
    return &gUIContext->Style;
}

void Rr_UISetNextWindowPosition(Rr_Vec2 Position)
{
    gUIContext->NextWindowPosition = Position;
}

void Rr_UISetNextWindowSize(Rr_Vec2 Size)
{
    gUIContext->NextWindowSize = Size;
}

void Rr_UISetNextWindowPadding(Rr_Vec2 Padding)
{
    gUIContext->NextWindowPadding = Padding;
}

static inline void Rr_UIConsumeNextWindowPosition(Rr_UIWindow *Window)
{
    if (gUIContext->NextWindowPosition.X != INFINITY &&
        gUIContext->NextWindowPosition.Y != INFINITY)
    {
        Window->Rect.Offset = Rr_FloorV2(gUIContext->NextWindowPosition);
        gUIContext->NextWindowPosition = Rr_V2F(INFINITY);
    }
}

static inline void Rr_UIConsumeNextWindowSize(Rr_UIWindow *Window)
{
    if (gUIContext->NextWindowSize.Width != INFINITY &&
        gUIContext->NextWindowSize.Height != INFINITY)
    {
        Window->Rect.Extent = Rr_FloorV2(gUIContext->NextWindowSize);
        gUIContext->NextWindowSize = Rr_V2F(INFINITY);
    }
}

static inline void Rr_UISwapWindowZ(Rr_UIWindow *WindowA, Rr_UIWindow *WindowB)
{
    int32_t Temp = WindowA->Z;
    WindowA->Z = WindowB->Z;
    WindowB->Z = Temp;
}

static inline void Rr_UIPutWindowOnTop(Rr_UIWindow *Window)
{
    if (gUIContext->HighestWindow && gUIContext->HighestWindow != Window)
    {
        Window->Z = gUIContext->HighestWindow->Z + 1;
    }
}

static inline bool Rr_UIBeginWindowEx(
    const char *Title,
    Rr_UIWindow *Window,
    bool *Open,
    Rr_UIWindowFlags Flags)
{
    Rr_UIConsumeNextWindowPosition(Window);
    Rr_UIConsumeNextWindowSize(Window);

    Window->Flags = Flags;

    /* Return if closed.
     * Also handle show after being closed.
     * This will put window on top unless there is a flag
     * preventing that which is not currently implemented. */

    bool AutoResize =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);

    bool WasClosed = Window->Open == false;
    Window->Open = (Open == NULL || *Open == true);
    if (Window->Open)
    {
        if (WasClosed)
        {
            if (AutoResize)
            {
                Window->SkipDueToAutoResize = true;
            }
            if (gUIContext->HighestWindow)
            {
                Rr_UIPutWindowOnTop(Window);
            }
        }
    }
    else
    {
        return false;
    }

    /* Have to access current window and finish its clip rect. */

    Rr_UIEndClipRect();

    Rr_UILayout *Layout =
        RR_PUSH_INTO_ARRAY(&gUIContext->Stack, gUIContext->FrameArena);
    *Layout = (Rr_UILayout){
        .Window = Window,
        .HorizontalX = INFINITY,
    };

    /* TODO: Make sure it's the correct call. */
    RR_RESET_ARRAY(&Window->ClipRects, gUIContext->FrameArena);

    /* Move and resize behavior.
     * Handle these early so following code may access updated window rect. */

    bool Dragging = Rr_UIDragBehavior(
        Window,
        &Window->Rect,
        RR_UI_DRAG_OP_MOVE,
        0,
        Window->Rect.Offset,
        NULL,
        NULL);

    if (Dragging && !RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_MOVE_BIT))
    {
        Rr_Vec2 Delta =
            Rr_SubV2(gUIContext->MousePosition, gUIContext->DragOpMouseStart);
        Window->Rect.Offset = Rr_AddV2(gUIContext->DragOpWindowStart, Delta);
        Window->Rect.Offset = Rr_FloorV2(Window->Rect.Offset);
    }

    bool NoResize = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT);

    if (NoResize == false && AutoResize == false)
    {
        /* Defer drawing the handle to Rr_UIEndWindow()! */

        Rr_UIAddResizeHandle(Layout);
    }

    /* Clip to total window area. */

    Layout->Cursor = Window->Rect.Offset;

    Rr_Rect WindowClipRect =
        Rr_ResizeRect(&Window->Rect, gUIContext->FrameThickness);

    Rr_UIBeginClipRect(&WindowClipRect);

    bool NoBorder = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_BORDER_BIT);

    if (NoBorder == false)
    {
        Rr_UIDrawOuterFrame(
            &Window->Rect,
            gUIContext->FrameThickness,
            &gUIContext->Style.Outline);
    }

    /* Add window title if necessary. */

    bool NoTitle = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT);

    if (NoTitle == false)
    {
        Rr_UIAddWindowTitle(Window);
        if (RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_CLOSE_BIT) == true)
        {
            Rr_UIAddCloseButton(Window, Open);
        }
        Layout->Cursor.Y += gUIContext->WindowTitleHeight;
    }

    /* Add vertical scrollbar if necessary. */

    Layout->AvailableContentsWidth = Window->Rect.Extent.Width;
    if (AutoResize)
    {
        Window->VScroll = 0;
    }
    else
    {
        bool HasScrollbar = Rr_UIAddVerticalScrollbar(Window);
        Layout->Cursor.Y -= Window->VScroll;
        if (HasScrollbar)
        {
            Layout->AvailableContentsWidth -= gUIContext->ScrollbarWidth;
        }
    }

    if (gUIContext->NextWindowPadding.Width != INFINITY &&
        gUIContext->NextWindowPadding.Height != INFINITY)
    {
        Layout->ContentsPadding = gUIContext->NextWindowPadding;
        gUIContext->NextWindowPadding = Rr_V2F(INFINITY);
    }
    else
    {
        Layout->ContentsPadding = gUIContext->ContentsPadding;
    }
    Layout->Cursor = Rr_AddV2(Layout->Cursor, Layout->ContentsPadding);
    Layout->AvailableContentsWidth -= Layout->ContentsPadding.X * 2.0f;

    Rr_UIEndClipRect();

    /* Clip to contents. */

    Rr_Rect ContentsAreaRect = Rr_UIGetWindowContentsArea(Window);
    Rr_UIBeginClipRect(&ContentsAreaRect);

    Rr_UIDrawSolidQuad(&ContentsAreaRect, &gUIContext->Style.Background);

    Window->ContentsStart = Window->ContentsEnd = Layout->Cursor;

    return true;
}

static inline void Rr_UIBeginPopupWindow(void)
{
    Rr_UIWindow *Window = &gUIContext->PopupWindow;
    Rr_UIBeginWindowEx("", Window, NULL, gUIContext->PopupWindow.Flags);
}

static inline void Rr_UIClosePopupWindow(void)
{
    assert(gUIContext->PopupWindowParent != NULL);
    gUIContext->PopupWindowParent = NULL;
    gUIContext->PopupWindow.Open = false;
}

bool Rr_UIBeginWindow(const char *Title, bool *Open, Rr_UIWindowFlags Flags)
{
    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_UIWindow **WindowRef =
        RR_GET_MAP_VALUE(&gUIContext->WindowMap, TitleHash, gUIContext->Arena);
    Rr_UIWindow *Window = *WindowRef;

    if (Window == NULL)
    {
        Window = RR_ALLOC_TYPE(gUIContext->Arena, Rr_UIWindow);
        Window->Title = memcpy(
            RR_ALLOC(gUIContext->Arena, TitleLength + 1),
            Title,
            TitleLength + 1);
        Window->Z = gUIContext->TotalWindowCount++;
        Window->Hash = TitleHash;
        Window->Rect.Offset = Rr_FloorV2(Rr_V2F(gUIContext->FontSize));
        Window->Rect.Extent = Rr_UIGetMinWindowSize(Flags);
        Window->Rect.Extent.Width += gUIContext->FontSize * 16.0f;

        /* TODO: Wrapped text uses available width so we still need
         * some baseline width. Probably should come up with better solution. */
        Window->Rect.Extent.Width += gUIContext->FontSize * 16.0f;

        Window->SkipDueToAutoResize = true;

        *WindowRef = Window;
    }

    assert(
        Window->Added == false && "There already is a window with this title!");
    if (Rr_UIBeginWindowEx(Title, Window, Open, Flags))
    {
        *RR_PUSH_INTO_ARRAY(&gUIContext->ActiveWindows, gUIContext->Arena) =
            Window;
        Window->Added = true;
        return true;
    }
    return false;
}

void Rr_UIEndWindow(void)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_UIEndClipRect();

    bool NoResize = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT);

    if (NoResize == false)
    {
        Rr_UIBeginClipRect(&Window->Rect);
        Rr_Vec2 BottomRight =
            Rr_AddV2(Window->Rect.Offset, Window->Rect.Extent);
        Rr_Vec2 Positions[] = {
            { BottomRight.X - gUIContext->ResizeHandleSize, BottomRight.Y },
            { BottomRight.X, BottomRight.Y - gUIContext->ResizeHandleSize },
            { BottomRight.X, BottomRight.Y },
        };
        Rr_UIDrawSolidTriangle(Positions, &Layout->DeferredResizeHandleColor);
        Rr_UIEndClipRect();
    }

    Window->ContentsEnd =
        Rr_AddV2(Rr_MulV2F(Layout->ContentsPadding, 2.0f), Window->ContentsEnd);

    bool AutoResize =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);

    if (Window->SkipDueToAutoResize || AutoResize)
    {
        Window->Rect.Extent =
            Rr_SubV2(Window->ContentsEnd, Window->ContentsStart);

        bool HasTitle =
            RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT) == false;

        if (HasTitle)
        {
            Window->Rect.Extent.Y += gUIContext->WindowTitleHeight;
        }
    }

    (void)RR_POP_FROM_ARRAY(&gUIContext->Stack);

    /* Resume clip rect if there is a window on the stack. */

    Rr_UIWindow *CurrentWindow = Rr_UICurrentWindow();
    if (CurrentWindow && CurrentWindow->ClipRects.Count)
    {
        Rr_Rect Rect = Rr_UICurrentRect(CurrentWindow);
        Rr_UIBeginClipRect(&Rect);
    }
}

void Rr_UIBeginHorizontal(void)
{
    assert(
        Rr_UIIsHorizontal() == false &&
        "Did you forget to call Rr_EndHorizontal()?");
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Layout->HorizontalMaxHeight = 0;
    Layout->HorizontalX = Layout->Cursor.X;
}

void Rr_UIEndHorizontal(void)
{
    assert(
        Rr_UIIsHorizontal() && "Did you forget to call Rr_BeginHorizontal()?");
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Layout->Cursor.X = Layout->HorizontalX;
    Layout->Cursor.Y += Layout->HorizontalMaxHeight;
    Layout->HorizontalX = INFINITY;
}

void Rr_UIBeginTabs(const char *Title)
{
    Rr_UIAssertWindow();
    assert(
        Rr_UIIsHorizontal() == false && "Tabs can't be aligned horizontally!");

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, NULL);

    Rr_UIHash **SelectedTabHashRef =
        RR_GET_MAP_VALUE(&Window->WidgetMap, TitleHash, gUIContext->Arena);
    if (*SelectedTabHashRef == NULL)
    {
        *SelectedTabHashRef = RR_ALLOC_TYPE(gUIContext->Arena, Rr_UIHash);
    }
    Layout->SelectedTabHash = *SelectedTabHashRef;
    Layout->TabCursor = Layout->Cursor;

    Rr_UIAdvance((Rr_Vec2){ 0.0f, gUIContext->LineHeight });

    Rr_Vec2 SeparatorSize = {
        Layout->AvailableContentsWidth,
        gUIContext->FrameThickness,
    };
    Rr_Vec2 SeparatorPosition = {
        Layout->Cursor.X,
        Layout->Cursor.Y,
    };
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            SeparatorPosition,
            SeparatorSize,
        },
        &gUIContext->Style.Foreground);

    Rr_UIAdvance((Rr_Vec2){ 0.0f, Layout->ContentsPadding.Y });
}

bool Rr_UITab(const char *Title)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    assert(Layout->SelectedTabHash && "Did you forget to call Rr_BeginTabs()?");
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    bool Selected = false;
    if (*Layout->SelectedTabHash == TitleHash)
    {
        Selected = true;
    }
    else if (*Layout->SelectedTabHash == 0)
    {
        *Layout->SelectedTabHash = TitleHash;
        Selected = true;
    }

    Rr_UIQuad TabQuad = Rr_UIReserveQuad();

    Rr_Vec2 TextPosition = Layout->TabCursor;
    TextPosition.X += gUIContext->ButtonPadding.X;
    Rr_Vec2 TextSize = Rr_UIDrawText(
        0,
        TextPosition,
        TitleLength,
        Title,
        0.0f,
        Selected ? &gUIContext->Style.Background
                 : &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 ButtonPosition = TextPosition;
    ButtonPosition.X -= gUIContext->ButtonPadding.X;
    Rr_Vec2 ButtonSize = TextSize;
    ButtonSize.X += gUIContext->ButtonPadding.X * 2.0f;
    if (Selected)
    {
        ButtonSize.Y += gUIContext->FrameThickness;
    }

    Layout->TabCursor.X += ButtonSize.Width;

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_UIButtonBehavior(
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
    if (Selected)
    {
        TabButtonColor = &gUIContext->Style.Foreground;
    }
    else if (Held)
    {
        TabButtonColor = &gUIContext->Style.ButtonHeld;
    }
    else if (Hovered)
    {
        TabButtonColor = &gUIContext->Style.ButtonHovered;
    }
    else
    {
        TabButtonColor = &gUIContext->Style.Background;
    }

    Rr_UISolidQuad(
        TabQuad,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        },
        TabButtonColor);

    if (Up)
    {
        /* Newly selected tab will be rendered next frame. */
        *Layout->SelectedTabHash = TitleHash;
    }

    Rr_DestroyScratch(Scratch);

    return Selected;
}

void Rr_UIEndTabs(void)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    assert(
        Layout->SelectedTabHash != NULL &&
        "Did you forget to call Rr_UIBeginTabs()?");

    Layout->SelectedTabHash = NULL;
    Layout->Window->ContentsEnd.X =
        RR_MAX(Layout->Window->ContentsEnd.X, Layout->TabCursor.X);
}

bool Rr_UIFold(const char *Title)
{
    Rr_UIAssertWindow();
    assert(
        Rr_UIIsHorizontal() == false && "Folds can't be aligned horizontally!");

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_UIPrimitive BevelPrimitive = Rr_UIReserveBevel();

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    bool **FoldValueRef =
        RR_GET_MAP_VALUE(&Window->WidgetMap, TitleHash, gUIContext->Arena);
    bool *FoldValue = *FoldValueRef;
    if (*FoldValueRef == NULL)
    {
        FoldValue = RR_ALLOC_TYPE(gUIContext->Arena, bool);
        *FoldValueRef = FoldValue;
    }

    float TriangleHeight = gUIContext->LineHeight * 0.575f;
    float TriangleBaseX = Layout->Cursor.X + Layout->ContentsPadding.Width;
    float TriangleBaseY =
        gUIContext->ButtonPadding.Height + Layout->Cursor.Y +
        gUIContext->LineHeight -
        gUIContext->LineHeight * (gUIContext->Font->UnderlineY +
                                  gUIContext->Font->UnderlineThickness) -
        TriangleHeight / 2.0f;
    Rr_Vec2 TrianglePositions[3];
    if (*FoldValue)
    {
        TrianglePositions[0] =
            Rr_V2(TriangleBaseX + TriangleHeight / 2.0f, TriangleBaseY);
        TrianglePositions[1] =
            Rr_V2(TriangleBaseX, TriangleBaseY - TriangleHeight);
        TrianglePositions[2] = Rr_V2(
            TriangleBaseX + TriangleHeight,
            TriangleBaseY - TriangleHeight);
    }
    else
    {
        TrianglePositions[0] = Rr_V2(TriangleBaseX, TriangleBaseY);
        TrianglePositions[1] =
            Rr_V2(TriangleBaseX, TriangleBaseY - TriangleHeight);
        TrianglePositions[2] = Rr_V2(
            TriangleBaseX + TriangleHeight,
            TriangleBaseY - TriangleHeight / 2.0f);
    }
    Rr_UIDrawSolidTriangle(TrianglePositions, &gUIContext->Style.Foreground);

    Rr_Vec2 TitlePosition = Rr_V2(
        TriangleBaseX + TriangleHeight + gUIContext->ButtonPadding.Width,
        Layout->Cursor.Y);
    TitlePosition.Y += gUIContext->ButtonPadding.Height;
    Rr_UIDrawText(
        0,
        TitlePosition,
        TitleLength,
        Title,
        0,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = Rr_V2(
        Layout->AvailableContentsWidth,
        gUIContext->LineHeight + gUIContext->ButtonPadding.Height * 2.0f);

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_UIButtonBehavior(
        Window,
        &(Rr_Rect){
            Layout->Cursor,
            TotalSize,
        },
        NULL,
        &Up,
        &Hovered,
        &Held);

    if (Up)
    {
        *FoldValue = !*FoldValue;
    }

    Rr_Rect ButtonRect = {
        Layout->Cursor,
        TotalSize,
    };
    Rr_UIBevel(
        BevelPrimitive,
        &ButtonRect,
        &gUIContext->Style.TitleBackground,
        Held);

    TotalSize.Height += Layout->ContentsPadding.Height;

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return *FoldValue;
}

void Rr_UISeparator(void)
{
    /* TODO: Horizontal support. */
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 Size = {
        Layout->AvailableContentsWidth,
        gUIContext->FrameThickness,
    };
    Rr_Vec2 Position = {
        Layout->Cursor.X,
        Layout->Cursor.Y + (gUIContext->SeparatorLineHeight / 2.0f -
                            gUIContext->FrameThickness / 2.0f),
    };
    Rr_Vec4 Color = Rr_MulV4F(gUIContext->Style.Foreground, 0.75f);
    Rr_UIDrawSolidQuad(&(Rr_Rect){ Position, Size }, &Color);

    {
        Color.XYZ = Rr_MulV3F(Color.XYZ, 0.8f);
        Position.Y += Size.Height;
        Rr_UIDrawSolidQuad(&(Rr_Rect){ Position, Size }, &Color);
    }

    Layout->Cursor.Y += gUIContext->SeparatorLineHeight;
}

void Rr_UILabelEx(const char *Text, Rr_UITextFlags Flags)
{
    Rr_UIAssertWindow();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 TextSize = Rr_UIDrawText(
        0,
        Layout->Cursor,
        SIZE_MAX,
        Text,
        Layout->AvailableContentsWidth,
        &gUIContext->Style.Foreground,
        Flags);

    Rr_UIAdvance(TextSize);

    Rr_DestroyScratch(Scratch);
}

void Rr_UILabel(const char *Text)
{
    Rr_UIAssertWindow();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 TextSize = Rr_UIDrawText(
        0,
        Layout->Cursor,
        SIZE_MAX,
        Text,
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    Rr_UIAdvance(TextSize);

    Rr_DestroyScratch(Scratch);
}

void Rr_UILabelF(const char *Format, ...)
{
    int BufferSize;
    va_list Args;

    va_start(Args, Format);
    BufferSize = vsnprintf(NULL, 0, Format, Args);
    va_end(Args);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    char *Buffer = RR_ALLOC_NO_ZERO(Scratch.Arena, (size_t)BufferSize + 1);

    va_start(Args, Format);
    BufferSize = vsnprintf(Buffer, (size_t)BufferSize + 1, Format, Args);
    va_end(Args);

    Rr_UILabel(Buffer);

    Rr_DestroyScratch(Scratch);
}

bool Rr_UIButton(const char *Text)
{
    Rr_UIAssertWindow();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 ButtonPosition = Layout->Cursor;
    Rr_UIPrimitive Primitive = Rr_UIReserveBevel();

    Rr_Vec2 TextPosition = Rr_AddV2(ButtonPosition, gUIContext->ButtonPadding);
    Rr_Vec2 TextSize = Rr_UIDrawText(
        0,
        TextPosition,
        SIZE_MAX,
        Text,
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 ButtonSize =
        Rr_AddV2(TextSize, Rr_MulV2F(gUIContext->ButtonPadding, 2.0f));

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_UIButtonBehavior(
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

    Rr_UIBevel(Primitive, &ButtonRect, &gUIContext->Style.ButtonNormal, Held);

    ButtonSize.Height += gUIContext->ContentsPadding.Height;

    Rr_UIAdvance(ButtonSize);

    Rr_DestroyScratch(Scratch);

    return Up;
}

bool Rr_UICheckbox(const char *Title, bool *Checked)
{
    Rr_UIAssertWindow();
    assert(Checked != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 CheckboxSize = { gUIContext->LineHeight, gUIContext->LineHeight };

    Rr_Vec2 FramePosition = Layout->Cursor;

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_UIButtonBehavior(
        Window,
        &(Rr_Rect){
            FramePosition,
            CheckboxSize,
        },
        NULL,
        &Up,
        &Hovered,
        &Held);

    if (Up)
    {
        *Checked = !*Checked;
    }

    Rr_Vec4 BackgroundColor = gUIContext->Style.Background;
    BackgroundColor.XYZ = Rr_MulV3F(BackgroundColor.XYZ, 0.9f);

    Rr_Rect CheckboxRect = {
        FramePosition,
        CheckboxSize,
    };
    Rr_UIDrawBevel(&CheckboxRect, &BackgroundColor, Held || *Checked);

    if (*Checked)
    {
        Rr_Rect Inset =
            Rr_ResizeRect(&CheckboxRect, -CheckboxRect.Extent.Width / 3.0f);
        Rr_UIDrawSolidQuad(&Inset, &gUIContext->Style.Foreground);
    }

    Rr_Vec2 TitlePosition = FramePosition;
    TitlePosition.X += CheckboxSize.X + gUIContext->ButtonPadding.Width;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        0,
        TitlePosition,
        SIZE_MAX,
        Title,
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        TitleSize.Width + Layout->ContentsPadding.Width +
            gUIContext->LineHeight,
        gUIContext->LineHeight + Layout->ContentsPadding.Height,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return Up;
}

static inline bool Rr_UIConsumeTextInput(
    size_t UTF8StringLength,
    const char *UTF8String,
    size_t *BufferLength,
    size_t BufferCapacity,
    char *Buffer,
    Rr_UIInputFieldFilterFunc FilterFunc,
    size_t *CursorBegin,
    size_t *CursorEnd)
{
    if (FilterFunc && !FilterFunc(UTF8StringLength, UTF8String))
    {
        return false;
    }
    size_t CursorMin = RR_MIN(*CursorBegin, *CursorEnd);
    size_t CursorMax = RR_MAX(*CursorBegin, *CursorEnd);
    /* Take selected range into account since it will be replaced by input
     * string. */
    size_t Diff = CursorMax - CursorMin;
    if (*BufferLength + UTF8StringLength + 1 - Diff > BufferCapacity)
    {
        return false;
    }
    memmove(
        Buffer + CursorMin + UTF8StringLength,
        Buffer + CursorMax,
        *BufferLength - CursorMax);
    if (UTF8String != NULL)
    {
        memcpy(Buffer + CursorMin, UTF8String, UTF8StringLength);
    }
    *BufferLength += UTF8StringLength;
    *BufferLength -= CursorMax - CursorMin;
    Buffer[*BufferLength] = '\0';
    *CursorEnd = CursorMin + UTF8StringLength;
    *CursorBegin = *CursorEnd;
    return true;
}

static inline size_t Rr_UIThisLineCol(char *Buffer, size_t Cursor)
{
    if (Buffer[Cursor] == '\n')
    {
        size_t ThisLine = Rr_PreviousUTF8LFOffset(Buffer, Cursor);
        size_t Col = Cursor - Rr_PreviousUTF8LFOffset(Buffer, Cursor - 1);
        if (Col == ThisLine)
        {
            return Col;
        }
        else
        {
            return Col - 1;
        }
    }
    else
    {
        size_t ThisLine = Rr_PreviousUTF8LFOffset(Buffer, Cursor);
        if (ThisLine != 0)
        {
            return Cursor - ThisLine - 1;
        }
        else
        {
            return Cursor - ThisLine;
        }
    }
}

static inline size_t Rr_UILineStart(const char *Buffer, size_t Cursor)
{
    if (Buffer[Cursor] == '\n' && Cursor > 0)
    {
        Cursor--;
    }
    size_t LineStart = Rr_PreviousUTF8LFOffset(Buffer, Cursor);
    if (LineStart != 0)
    {
        LineStart++;
    }
    return LineStart;
}

static inline size_t Rr_UILineEnd(const char *Buffer, size_t Cursor)
{
    return Rr_NextUTF8LFOffset(Buffer, Cursor);
}

static bool Rr_UIEditUTF8Buffer(
    size_t *CursorBegin,
    size_t *CursorEnd,
    size_t BufferCapacity,
    char *Buffer,
    Rr_UIInputFieldFilterFunc FilterFunc,
    bool EnterToConfirm)
{
    if (gUIContext->TextInputEvents.Count == 0 &&
        gUIContext->KeyboardInputEvents.Count == 0)
    {
        return false;
    }

    bool ChangesConfirmed = false;

    uint64_t TimeMS = Rr_GetTimeMS();
    size_t BufferLength = strlen(Buffer);

    size_t NewCursorBegin;
    size_t NewCursorEnd;

    size_t CursorMin;
    size_t CursorMax;

    for (size_t Index = 0; Index < gUIContext->KeyboardInputEvents.Count;
         ++Index)
    {
        Rr_KeyEvent *Event = gUIContext->KeyboardInputEvents.Data + Index;

        if (!Event->Down)
        {
            continue;
        }

        NewCursorBegin = *CursorBegin;
        NewCursorEnd = *CursorEnd;

        CursorMin = RR_MIN(NewCursorBegin, NewCursorEnd);
        CursorMax = RR_MAX(NewCursorBegin, NewCursorEnd);

        bool Edited = false;
        bool ResetCol = false;

        if (Event->Scancode == RR_SCANCODE_A && Event->Keymod == RR_KEYMOD_CTRL)
        {
            NewCursorBegin = 0;
            NewCursorEnd = BufferLength;
            Edited = true;
            ResetCol = true;
        }

        if (Event->Scancode == RR_SCANCODE_C && Event->Keymod == RR_KEYMOD_CTRL)
        {
            Rr_Scratch Scratch = Rr_GetScratch(NULL);

            char *ClipboardBuffer = NULL;
            size_t ClipboardLength = 0;
            if (CursorMin != CursorMax)
            {
                ClipboardLength = CursorMax - CursorMin;
                ClipboardBuffer =
                    RR_ALLOC_NO_ZERO(Scratch.Arena, ClipboardLength);
                memcpy(ClipboardBuffer, Buffer + CursorMin, ClipboardLength);
            }
            else
            {
                size_t LineStart = Rr_UILineStart(Buffer, NewCursorEnd);
                size_t LineEnd = Rr_UILineEnd(Buffer, NewCursorEnd);
                if (LineEnd != BufferLength)
                {
                    LineEnd++;
                }
                ClipboardLength = LineEnd - LineStart;
                if (ClipboardLength > 0)
                {
                    ClipboardBuffer =
                        RR_ALLOC_NO_ZERO(Scratch.Arena, ClipboardLength);
                    memcpy(
                        ClipboardBuffer,
                        Buffer + LineStart,
                        ClipboardLength);
                }
            }
            if (ClipboardBuffer != NULL)
            {
                ClipboardBuffer[ClipboardLength] = '\0';
                Rr_SetClipboardText(ClipboardBuffer);
            }

            Rr_DestroyScratch(Scratch);
        }
        if (Event->Scancode == RR_SCANCODE_V && Event->Keymod == RR_KEYMOD_CTRL)
        {
            const char *ClipboardBuffer = Rr_GetClipboardText();
            if (ClipboardBuffer != NULL)
            {
                size_t ClipboardLength = strlen(ClipboardBuffer);
                Rr_UIConsumeTextInput(
                    ClipboardLength,
                    ClipboardBuffer,
                    &BufferLength,
                    BufferCapacity,
                    Buffer,
                    FilterFunc,
                    &NewCursorBegin,
                    &NewCursorEnd);
                Edited = true;
                ResetCol = true;
            }
        }
        if (Event->Scancode == RR_SCANCODE_X && Event->Keymod == RR_KEYMOD_CTRL)
        {
            Rr_Scratch Scratch = Rr_GetScratch(NULL);

            char *ClipboardBuffer = NULL;
            size_t ClipboardLength = 0;
            if (CursorMin != CursorMax)
            {
                ClipboardLength = CursorMax - CursorMin;
                ClipboardBuffer =
                    RR_ALLOC_NO_ZERO(Scratch.Arena, ClipboardLength);
                memcpy(ClipboardBuffer, Buffer + CursorMin, ClipboardLength);

                Rr_UIConsumeTextInput(
                    0,
                    NULL,
                    &BufferLength,
                    BufferCapacity,
                    Buffer,
                    FilterFunc,
                    &NewCursorBegin,
                    &NewCursorEnd);
                Edited = true;
                ResetCol = true;
            }
            else
            {
                size_t LineStart = Rr_UILineStart(Buffer, NewCursorEnd);
                size_t LineEnd = Rr_UILineEnd(Buffer, NewCursorEnd);
                if (LineEnd != BufferLength)
                {
                    LineEnd++;
                }
                ClipboardLength = LineEnd - LineStart;
                if (ClipboardLength > 0)
                {
                    ClipboardBuffer =
                        RR_ALLOC_NO_ZERO(Scratch.Arena, ClipboardLength);
                    memcpy(
                        ClipboardBuffer,
                        Buffer + LineStart,
                        ClipboardLength);
                }

                NewCursorBegin = LineStart;
                NewCursorEnd = LineEnd;
                Rr_UIConsumeTextInput(
                    0,
                    NULL,
                    &BufferLength,
                    BufferCapacity,
                    Buffer,
                    FilterFunc,
                    &NewCursorBegin,
                    &NewCursorEnd);

                Edited = true;
                ResetCol = true;
            }
            if (ClipboardBuffer != NULL)
            {
                ClipboardBuffer[ClipboardLength] = '\0';
                Rr_SetClipboardText(ClipboardBuffer);
            }

            Rr_DestroyScratch(Scratch);
        }

        if (Event->Scancode == RR_SCANCODE_ESCAPE)
        {
            NewCursorBegin = NewCursorEnd;
            Edited = true;
            ChangesConfirmed = true;
        }

        if (Event->Scancode == RR_SCANCODE_RETURN)
        {
            if (EnterToConfirm)
            {
                ChangesConfirmed = true;
            }
            else if (Event->Keymod == 0 && CursorMin > 0)
            {
                if (Rr_UIConsumeTextInput(
                        1,
                        "\n",
                        &BufferLength,
                        BufferCapacity,
                        Buffer,
                        FilterFunc,
                        &NewCursorBegin,
                        &NewCursorEnd))
                {
                    Edited = true;
                    ResetCol = true;
                }
            }
        }

        if (Event->Scancode == RR_SCANCODE_UP)
        {
            if (NewCursorEnd > 0)
            {
                size_t DesiredOffset = gUIContext->TextInputCursorMaxCol;
                size_t ThisLineCol = Rr_UIThisLineCol(Buffer, NewCursorEnd);
                size_t ThisLine = NewCursorEnd - ThisLineCol;
                if (ThisLine == 0)
                {
                    NewCursorEnd = 0;
                }
                else
                {
                    size_t PrevLineCol = Rr_UIThisLineCol(Buffer, ThisLine - 1);
                    size_t PrevLine = ThisLine - 1 - PrevLineCol;
                    NewCursorEnd = PrevLine + (DesiredOffset > PrevLineCol
                                                   ? PrevLineCol
                                                   : DesiredOffset);
                }
            }
            if ((Event->Keymod & RR_KEYMOD_SHIFT) == 0)
            {
                NewCursorBegin = NewCursorEnd;
            }
            Edited = true;
        }
        if (Event->Scancode == RR_SCANCODE_DOWN)
        {
            if (NewCursorEnd < BufferLength)
            {
                size_t DesiredOffset = gUIContext->TextInputCursorMaxCol;
                size_t NextLine = Rr_NextUTF8LFOffset(Buffer, NewCursorEnd);
                if (NextLine == BufferLength)
                {
                    NewCursorEnd = BufferLength;
                }
                else
                {
                    NextLine++;
                    size_t NextNextLine = Rr_NextUTF8LFOffset(Buffer, NextLine);
                    size_t NextLineLength = NextNextLine - NextLine;
                    NewCursorEnd = NextLine + (DesiredOffset > NextLineLength
                                                   ? NextLineLength
                                                   : DesiredOffset);
                }
            }
            if ((Event->Keymod & RR_KEYMOD_SHIFT) == 0)
            {
                NewCursorBegin = NewCursorEnd;
            }
            Edited = true;
        }

        if (Event->Scancode == RR_SCANCODE_LEFT)
        {
            if (NewCursorEnd > 0)
            {
                if ((Event->Keymod & RR_KEYMOD_CTRL) == 0)
                {
                    NewCursorEnd =
                        Rr_PreviousUTF8CodepointOffset(Buffer, NewCursorEnd);
                }
                else
                {
                    NewCursorEnd =
                        Rr_PreviousUTF8WordOffset(Buffer, NewCursorEnd);
                }
            }
            if ((Event->Keymod & RR_KEYMOD_SHIFT) == 0)
            {
                NewCursorBegin = NewCursorEnd;
            }
            Edited = true;
            ResetCol = true;
        }
        if (Event->Scancode == RR_SCANCODE_RIGHT)
        {
            if (NewCursorEnd < BufferLength)
            {
                if ((Event->Keymod & RR_KEYMOD_CTRL) == 0)
                {
                    NewCursorEnd =
                        Rr_NextUTF8CodepointOffset(Buffer, NewCursorEnd);
                }
                else
                {
                    NewCursorEnd = Rr_NextUTF8WordOffset(Buffer, NewCursorEnd);
                }
            }
            if ((Event->Keymod & RR_KEYMOD_SHIFT) == 0)
            {
                NewCursorBegin = NewCursorEnd;
            }
            Edited = true;
            ResetCol = true;
        }

        if (Event->Scancode == RR_SCANCODE_HOME ||
            Event->Scancode == RR_SCANCODE_KP_7)
        {
            if (Event->Keymod & RR_KEYMOD_CTRL)
            {
                NewCursorEnd = 0;
            }
            else
            {
                NewCursorEnd = Rr_UILineStart(Buffer, NewCursorEnd);
            }
            if ((Event->Keymod & RR_KEYMOD_SHIFT) == 0)
            {
                NewCursorBegin = NewCursorEnd;
            }
            Edited = true;
            ResetCol = true;
        }
        if (Event->Scancode == RR_SCANCODE_END ||
            Event->Scancode == RR_SCANCODE_KP_1)
        {
            if (Event->Keymod & RR_KEYMOD_CTRL)
            {
                NewCursorEnd = BufferLength;
            }
            else
            {
                NewCursorEnd = Rr_UILineEnd(Buffer, NewCursorEnd);
            }
            if ((Event->Keymod & RR_KEYMOD_SHIFT) == 0)
            {
                NewCursorBegin = NewCursorEnd;
            }
            Edited = true;
            ResetCol = true;
        }

        if (Event->Keymod == 0)
        {
            if (Event->Scancode == RR_SCANCODE_BACKSPACE && BufferLength > 0)
            {
                if (CursorMin == 0 && CursorMax == BufferLength)
                {
                    Buffer[0] = '\0';
                    NewCursorEnd = NewCursorBegin = 0;
                    BufferLength = 0;
                }
                else if (CursorMin != CursorMax)
                {
                    memmove(
                        Buffer + CursorMin,
                        Buffer + CursorMax,
                        BufferLength - CursorMax);
                    BufferLength -= CursorMax - CursorMin;
                    Buffer[BufferLength] = '\0';
                    NewCursorEnd = NewCursorBegin = CursorMin;
                }
                else
                {
                    CursorMin =
                        Rr_PreviousUTF8CodepointOffset(Buffer, CursorMin);
                    memmove(
                        Buffer + CursorMin,
                        Buffer + CursorMax,
                        BufferLength - CursorMax);
                    BufferLength -= CursorMax - CursorMin;
                    Buffer[BufferLength] = '\0';
                    NewCursorEnd = NewCursorBegin = CursorMin;
                }
                Edited = true;
            }
        }

        if (Edited)
        {
            *CursorBegin = NewCursorBegin;
            *CursorEnd = NewCursorEnd;
            gUIContext->TextInputCursorBlinkTime = TimeMS;
        }

        if (ResetCol)
        {
            gUIContext->TextInputCursorMaxCol =
                Rr_UIThisLineCol(Buffer, NewCursorEnd);
        }
    }

    for (size_t Index = 0; Index < gUIContext->TextInputEvents.Count; ++Index)
    {
        NewCursorBegin = *CursorBegin;
        NewCursorEnd = *CursorEnd;

        /* CursorMin = RR_MIN(NewCursorBegin, NewCursorEnd); */
        /* CursorMax = RR_MAX(NewCursorBegin, NewCursorEnd); */

        const char *CString = gUIContext->TextInputEvents.Data[Index];
        size_t Length = strlen(CString);

        if (Rr_UIConsumeTextInput(
                Length,
                CString,
                &BufferLength,
                BufferCapacity,
                Buffer,
                FilterFunc,
                &NewCursorBegin,
                &NewCursorEnd))
        {
            *CursorBegin = NewCursorBegin;
            *CursorEnd = NewCursorEnd;
            gUIContext->TextInputCursorBlinkTime = TimeMS;
            gUIContext->TextInputCursorMaxCol =
                Rr_UIThisLineCol(Buffer, NewCursorEnd);
        }
    }

    RR_CLEAR_ARRAY(&gUIContext->TextInputEvents);
    RR_CLEAR_ARRAY(&gUIContext->KeyboardInputEvents);

    return ChangesConfirmed;
}

/* NOTE: Generic input field is a building block for other widgets. It doesn't
 * alter the layout on its own. */
static inline bool Rr_UIGenericInputField(
    Rr_UIHash Hash,
    Rr_Vec2 Offset,
    size_t BufferCapacity,
    char *Buffer,
    const char *Placeholder,
    Rr_UIInputFieldFilterFunc FilterFunc,
    Rr_UIInputFieldFlags Flags,
    Rr_Vec2 *OutExtent)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_UIPrimitive FieldPrimitive = Rr_UIReserveBevel();

    bool UsePersistentBuffer =
        RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT);

    bool Focused = Rr_UIIsFocused(Window, Hash);
    bool WasFocused = !Focused && Rr_UIWasFocused(Window, Hash);

    Rr_Vec2 BufferPosition = Rr_AddV2(Offset, gUIContext->ButtonPadding);
    size_t NewCursorEnd = gUIContext->TextInputCursorEnd;
    Rr_Vec2 BufferSize = Rr_UIDrawInputText(
        UsePersistentBuffer && (Focused || WasFocused)
            ? gUIContext->TextInputBuffer.Data
            : Buffer,
        Focused,
        BufferPosition,
        gUIContext->TextInputCursorBegin,
        &NewCursorEnd,
        0.0f,
        &gUIContext->Style.Foreground);

    if (BufferSize.X == 0.0f)
    {
        if (Placeholder != NULL && !Focused)
        {
            BufferSize = Rr_UIDrawText(
                false,
                BufferPosition,
                SIZE_MAX,
                Placeholder,
                0.0f,
                &gUIContext->Style.ForegroundDimmed,
                0);
        }
        else
        {
            const float MIN_FIELD_WIDTH = gUIContext->FontSize / 2.0f;
            if (BufferSize.Width < MIN_FIELD_WIDTH)
            {
                BufferSize.Width = MIN_FIELD_WIDTH;
            }
        }
    }

    Rr_Rect FieldRect = {
        Offset,
        Rr_AddV2(BufferSize, Rr_MulV2F(gUIContext->ButtonPadding, 2.0f)),
    };
    if (OutExtent)
    {
        *OutExtent = FieldRect.Extent;
    }
    Rr_UIBevel(
        FieldPrimitive,
        &FieldRect,
        &gUIContext->Style.ButtonDisabled,
        true);

    bool Hovered, Began,
        Dragging = Rr_UIDragBehavior(
            Window,
            &FieldRect,
            RR_UI_DRAG_OP_WIDGET,
            Hash,
            Rr_V2F(0.0f),
            &Hovered,
            &Began);

    bool Autoselect = RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_AUTOSELECT_BIT);

    if (Began)
    {
        size_t BufferLength = strlen(Buffer);

        if (Autoselect && !Focused && !WasFocused)
        {
            /* Select all on first click. */

            gUIContext->TextInputCursorBegin = 0;
            gUIContext->TextInputCursorEnd = BufferLength;
            gUIContext->TextInputClickId = gUIContext->LeftMouseButtonClickId;
        }
        else
        {
            uint8_t Clicks = (gUIContext->LeftMouseButtonClicks - 1) % 3;
            if (Clicks == 2)
            {
                /* Select whole line. */

                gUIContext->TextInputCursorBegin =
                    Rr_UILineStart(Buffer, NewCursorEnd);
                gUIContext->TextInputCursorEnd =
                    Rr_UILineEnd(Buffer, NewCursorEnd);
            }
            else if (Clicks == 1)
            {
                /* Select word under cursor. */

                if (!(NewCursorEnd > 0 && Buffer[NewCursorEnd - 1] == ' '))
                {
                    gUIContext->TextInputCursorBegin =
                        Rr_PreviousUTF8WordOffset(Buffer, NewCursorEnd);
                }
                gUIContext->TextInputCursorEnd = Rr_LastUTF8CharInWordOffset(
                    Buffer,
                    gUIContext->TextInputCursorBegin);

                gUIContext->TextInputCursorEnd = RR_CLAMP(
                    gUIContext->TextInputCursorBegin,
                    gUIContext->TextInputCursorEnd,
                    BufferCapacity);
            }
            else if (Clicks == 0)
            {
                gUIContext->TextInputCursorBegin = NewCursorEnd;
                gUIContext->TextInputCursorEnd = NewCursorEnd;
            }
        }

        gUIContext->TextInputCursorMaxCol =
            Rr_UIThisLineCol(Buffer, gUIContext->TextInputCursorEnd);

        /* NOTE: A bit hacky way to make sure initial memcpy to persistent
         * buffer occurs only once. */

        if (UsePersistentBuffer && !Focused && !WasFocused)
        {
            /* NOTE: May waste quite a bit of memory. */
            if (gUIContext->TextInputBuffer.Capacity < BufferCapacity ||
                !gUIContext->TextInputBuffer.Data)
            {
                gUIContext->TextInputBuffer.Data =
                    RR_ALLOC_NO_ZERO(gUIContext->Arena, BufferCapacity);
                gUIContext->TextInputBuffer.Capacity = BufferCapacity;
            }
            memcpy(gUIContext->TextInputBuffer.Data, Buffer, BufferLength + 1);
        }
    }
    else if (Focused && Dragging)
    {
        if (!Autoselect ||
            gUIContext->LeftMouseButtonClickId > gUIContext->TextInputClickId)
        {
            gUIContext->TextInputCursorBlinkTime = Rr_GetTimeMS();
            gUIContext->TextInputCursorEnd = NewCursorEnd;
        }
    }

    if (Hovered)
    {
        gUIContext->MouseOverTextInput = true;
    }

    bool ChangesConfirmed = false;

    if (Focused)
    {
        ChangesConfirmed = Rr_UIEditUTF8Buffer(
            &gUIContext->TextInputCursorBegin,
            &gUIContext->TextInputCursorEnd,
            BufferCapacity,
            UsePersistentBuffer ? gUIContext->TextInputBuffer.Data : Buffer,
            FilterFunc,
            !RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT));
        if (ChangesConfirmed)
        {
            gUIContext->FocusedWindow = NULL;
        }
    }
    else
    {
        ChangesConfirmed |= WasFocused;
    }

    if (ChangesConfirmed && UsePersistentBuffer)
    {
        memcpy(Buffer, gUIContext->TextInputBuffer.Data, BufferCapacity);
    }

    Rr_DestroyScratch(Scratch);

    return ChangesConfirmed;
}

bool Rr_UIInputField(
    const char *Title,
    size_t BufferCapacity,
    char *Buffer,
    const char *Placeholder,
    Rr_UIInputFieldFilterFunc FilterFunc,
    Rr_UIInputFieldFlags Flags)
{
    Rr_UIAssertWindow();
    assert(Title != NULL);
    assert(BufferCapacity);
    assert(Buffer != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_Vec2 FieldExtent;
    bool ChangesConfirmed = Rr_UIGenericInputField(
        TitleHash,
        Layout->Cursor,
        BufferCapacity,
        Buffer,
        Placeholder,
        FilterFunc,
        Flags,
        &FieldExtent);

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += gUIContext->ButtonPadding.Width + FieldExtent.Width;
    TitlePosition.Y += gUIContext->ButtonPadding.Height;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        0,
        TitlePosition,
        TitleLength,
        Title,
        0,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        gUIContext->ButtonPadding.Width + FieldExtent.Width + TitleSize.Width,
        FieldExtent.Height + Layout->ContentsPadding.Height +
            gUIContext->ButtonPadding.Height * 2.0f,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return ChangesConfirmed;
}

bool Rr_UIInputText(const char *Title, size_t BufferCapacity, char *Buffer)
{
    return Rr_UIInputField(
        Title,
        BufferCapacity,
        Buffer,
        NULL,
        NULL,
        RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT);
}

static inline bool Rr_UIFloatFilter(size_t Length, const char *UTF8String)
{
    for (size_t Index = 0; Index < Length; ++Index)
    {
        char Char = UTF8String[Index];
        bool InRange = Char >= '0' && Char <= '9';
        bool Minus = Char == '-';
        bool Dot = Char == '.';
        if (!(InRange || Minus || Dot))
        {
            return false;
        }
    }
    return true;
}

bool Rr_UIInputFloat(const char *Title, float *Value)
{
    char Buffer[64];
    snprintf(Buffer, 64, "%g", *Value);
    bool Changed = Rr_UIInputField(
        Title,
        64,
        Buffer,
        NULL,
        Rr_UIFloatFilter,
        RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTOSELECT_BIT);
    if (Changed)
    {
        sscanf(Buffer, "%g", Value);
    }
    return Changed;
}

static inline bool Rr_UIInputFloatMulti(
    const char *Title,
    float *Values,
    int Count)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t Length = 1 + strlen(Title) + 3 + 2;
    bool Edited = false;

    Rr_Vec2 Cursor = Layout->Cursor;
    Rr_Vec2 TotalSize = { 0 };

    Rr_UIBeginHorizontal();
    const char *Titles[] = { "X", "Y", "Z", "W" };
    for (int Index = 0; Index < Count; ++Index)
    {
        char *CurrentTitle = RR_ALLOC_NO_ZERO(Scratch.Arena, Length);
        snprintf(
            CurrentTitle,
            Length,
            "%s###%s%d",
            Titles[Index],
            Title,
            Index);
        Rr_UIHash CurrentHash = Rr_UIGetTitleHash(CurrentTitle, NULL);
        char Buffer[64];
        snprintf(Buffer, 64, "%.2f", Values[Index]);
        Rr_Vec2 FieldExtent;
        Edited |= Rr_UIGenericInputField(
            CurrentHash,
            Cursor,
            64,
            Buffer,
            NULL,
            Rr_UIFloatFilter,
            RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
                RR_UI_INPUT_FIELD_FLAGS_AUTOSELECT_BIT,
            &FieldExtent);
        if (Edited)
        {
            sscanf(Buffer, "%g", &Values[Index]);
        }
        TotalSize.Width += FieldExtent.Width;
        TotalSize.Height = RR_MAX(TotalSize.Height, FieldExtent.Height);
        Cursor.X += FieldExtent.Width;
        Cursor.X += gUIContext->BevelThickness * 2.0f;
    }
    Rr_UIEndHorizontal();

    Cursor = Rr_AddV2(Cursor, gUIContext->ButtonPadding);

    Rr_Vec2 TitleSize = Rr_UIDrawText(
        0,
        Cursor,
        strlen(Title),
        Title,
        0,
        &gUIContext->Style.Foreground,
        0);

    TotalSize.Width += TitleSize.Width;

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return Edited;
}

bool Rr_UIInputFloat2(const char *Title, float *Values)
{
    return Rr_UIInputFloatMulti(Title, Values, 2);
}

bool Rr_UIInputFloat3(const char *Title, float *Values)
{
    return Rr_UIInputFloatMulti(Title, Values, 3);
}

bool Rr_UIInputFloat4(const char *Title, float *Values)
{
    return Rr_UIInputFloatMulti(Title, Values, 4);
}

static inline bool Rr_UIHexFilter(size_t Length, const char *UTF8String)
{
    for (size_t Index = 0; Index < Length; ++Index)
    {
        char Char = UTF8String[Index];
        bool InRange1 = Char >= '0' && Char <= '9';
        bool InRange2 = Char >= 'a' && Char <= 'f';
        bool InRange3 = Char >= 'A' && Char <= 'F';
        if (!(InRange1 || InRange2 || InRange3))
        {
            return false;
        }
    }
    return true;
}

static inline bool Rr_UIIntegerFilter(size_t Length, const char *UTF8String)
{
    for (size_t Index = 0; Index < Length; ++Index)
    {
        char Char = UTF8String[Index];
        bool InRange = Char >= '0' && Char <= '9';
        bool Minus = Char == '-';
        if (!(InRange || Minus))
        {
            return false;
        }
    }
    return true;
}

bool Rr_UIInputInt(const char *Title, int32_t *Value)
{
    char Buffer[64];
    snprintf(Buffer, 64, "%d", *Value);
    bool Changed = Rr_UIInputField(
        Title,
        64,
        Buffer,
        NULL,
        Rr_UIIntegerFilter,
        RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTOSELECT_BIT);
    if (Changed)
    {
        sscanf(Buffer, "%d", Value);
    }
    return Changed;
}

bool Rr_UICombobox(
    const char *Title,
    uint32_t OptionCount,
    const char *const *Options,
    uint32_t *SelectedIndex)
{
    Rr_UIAssertWindow();
    assert(Title != NULL);
    assert(OptionCount > 0);
    assert(Options != NULL);
    assert(SelectedIndex != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_UIPrimitive Primitive = Rr_UIReserveBevel();

    Rr_Vec2 SelectedTextSize = Rr_UIDrawText(
        true,
        Rr_V2F(0.0f),
        SIZE_MAX,
        Options[*SelectedIndex],
        0,
        NULL,
        0);

    Rr_Vec2 ButtonPosition = Layout->Cursor;

    Rr_Vec2 SelectedTextPosition =
        Rr_AddV2(ButtonPosition, gUIContext->ButtonPadding);
    Rr_UIDrawText(
        0,
        SelectedTextPosition,
        SIZE_MAX,
        Options[*SelectedIndex],
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 ButtonSize =
        Rr_AddV2(SelectedTextSize, Rr_MulV2F(gUIContext->ButtonPadding, 2.0f));

    Rr_Vec2 BorderSize = ButtonSize;
    BorderSize.X += gUIContext->LineHeight + gUIContext->ButtonPadding.Width;

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_UIButtonBehavior(
        Window,
        &(Rr_Rect){
            ButtonPosition,
            BorderSize,
        },
        NULL,
        &Up,
        &Hovered,
        &Held);

    if (Up)
    {
        gUIContext->PopupWindowParent = Window;
        gUIContext->PopupWindowHash = TitleHash;
    }

    bool OptionChanged = false;
    bool PopupOpen = gUIContext->PopupWindowParent == Window &&
                     gUIContext->PopupWindowHash == TitleHash;

    if (PopupOpen)
    {
        Rr_Vec2 PopupPosition = ButtonPosition;
        PopupPosition.Y += BorderSize.Height + gUIContext->FrameThickness;
        PopupPosition.X += gUIContext->FrameThickness;
        Rr_UISetNextWindowPosition(PopupPosition);
        Rr_UISetNextWindowPadding(Rr_V2(gUIContext->ButtonPadding.Width, 0.0f));
        Rr_UIBeginPopupWindow();
        Rr_UILayout *PopupLayout = Rr_UICurrentLayout();
        for (uint32_t Index = 0; Index < OptionCount; ++Index)
        {
            Rr_UIQuad OptionButtonQuad = Rr_UIReserveQuad();
            Rr_Vec2 OptionSize = Rr_UIDrawText(
                0,
                PopupLayout->Cursor,
                SIZE_MAX,
                Options[Index],
                0,
                &gUIContext->Style.Foreground,
                0);
            Rr_Rect OptionButtonRect;
            OptionButtonRect.Offset.Y = PopupLayout->Cursor.Y;
            OptionButtonRect.Offset.X =
                PopupLayout->Cursor.X - gUIContext->ButtonPadding.Width;
            OptionButtonRect.Extent.Width =
                gUIContext->PopupWindow.Rect.Extent.Width;
            OptionButtonRect.Extent.Height = gUIContext->LineHeight;
            bool Up = false;
            bool Hovered = false;
            bool Held = false;
            Rr_UIButtonBehavior(
                &gUIContext->PopupWindow,
                &OptionButtonRect,
                NULL,
                &Up,
                &Hovered,
                &Held);
            if (Up)
            {
                *SelectedIndex = Index;
                Rr_UIClosePopupWindow();
                OptionChanged = true;
            }
            Rr_Vec4 OptionButtonColor;
            if (Held)
            {
                OptionButtonColor = gUIContext->Style.ButtonHeld;
            }
            else if (Hovered)
            {
                OptionButtonColor = gUIContext->Style.ButtonHovered;
            }
            else
            {
                OptionButtonColor = gUIContext->Style.ButtonNormal;
                if (Index % 2 == 0)
                {
                    OptionButtonColor = Rr_MulV4F(OptionButtonColor, 0.85f);
                }
            }
            Rr_UISolidQuad(
                OptionButtonQuad,
                &OptionButtonRect,
                &OptionButtonColor);
            Rr_UIAdvance(OptionSize);
        }
        Rr_UIEndWindow();
    }

    Rr_Rect ButtonRect = {
        ButtonPosition,
        ButtonSize,
    };
    Rr_Vec4 BackgroundColor = gUIContext->Style.Background;
    BackgroundColor.XYZ = Rr_MulV3F(BackgroundColor.XYZ, 0.9f);

    Rr_UIBevel(Primitive, &ButtonRect, &gUIContext->Style.ButtonDisabled, Held);

    /* Add handle. */
    {
        float HandleSize = ButtonSize.Height;
        Rr_Rect HandleRect = { ButtonRect.Offset, Rr_V2F(HandleSize) };
        HandleRect.Offset.X += ButtonRect.Extent.Width;

        Rr_UIDrawBevel(&HandleRect, &gUIContext->Style.ButtonNormal, Held);

        Rr_Vec2 HandleCenter = Rr_RectCenter(&HandleRect);
        Rr_Vec2 TrianglePositions[] = {
            Rr_AddV2(
                HandleCenter,
                Rr_MulV2F(Rr_V2F(-1.0f), gUIContext->LineHeight / 4.0f)),
            Rr_AddV2(
                HandleCenter,
                Rr_MulV2F(
                    (Rr_Vec2){ 1.0f, -1.0f },
                    gUIContext->LineHeight / 4.0f)),
            Rr_AddV2(
                HandleCenter,
                Rr_MulV2F(
                    (Rr_Vec2){ 0.0f, 1.0f },
                    gUIContext->LineHeight / 4.0f)),
        };
        Rr_UIDrawSolidTriangle(
            TrianglePositions,
            &gUIContext->Style.Foreground);
    }

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += gUIContext->ButtonPadding.Width + BorderSize.Width;
    TitlePosition.Y += gUIContext->ButtonPadding.Height;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        0,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        TitleSize.Width + gUIContext->ButtonPadding.Width + BorderSize.Width,
        gUIContext->LineHeight + gUIContext->ButtonPadding.Height * 2.0f +
            Layout->ContentsPadding.Height,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return OptionChanged;
}

static inline void Rr_UIColorPickerPopup(Rr_Vec2 Center, Rr_Vec4 *Color)
{
    float TargetSize = 200.0f;
    float Step = TargetSize / 6.0f;

    Rr_Vec2 Position = Center;
    Position.X -= (gUIContext->ContentsPadding.Width + TargetSize) / 2.0f;
    Position.Y -= (gUIContext->ContentsPadding.Height + TargetSize) / 2.0f;
    Rr_UISetNextWindowPosition(Position);
    Rr_UIBeginPopupWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    float Grayscale = (Color->X + Color->Y + Color->Z) / 3.0f;

    Rr_UIVertex *Vertices = Rr_UIReserveQuads(6);
    Rr_Vec4 LightColors[6] = {
        Rr_V4(1.0f, 0.0f, 0.0f, 1.0f), Rr_V4(1.0f, 1.0f, 0.0f, 1.0f),
        Rr_V4(0.0f, 1.0f, 0.0f, 1.0f), Rr_V4(0.0f, 1.0f, 1.0f, 1.0f),
        Rr_V4(0.0f, 0.0f, 1.0f, 1.0f), Rr_V4(1.0f, 0.0f, 1.0f, 1.0f),
    };

    for (size_t Index = 0; Index < 6; ++Index)
    {
        Rr_Vec4 *ColorA = &LightColors[Index];
        Rr_Vec4 *ColorB = &LightColors[(Index + 1) % 6];

        Vertices[0].Color = *ColorA;
        Vertices[0].Position = Layout->Cursor;
        Vertices[0].Position.X += Step * (float)Index;
        Vertices[0].UV = Rr_V2F(0.0f);

        Vertices[1].Color = *ColorB;
        Vertices[1].Position = Layout->Cursor;
        Vertices[1].Position.X += Step * (float)Index + Step;
        Vertices[1].UV = Rr_V2F(0.0f);

        Vertices[2].Color = *ColorA;
        Vertices[2].Position = Layout->Cursor;
        Vertices[2].Position.Y += TargetSize;
        Vertices[2].Position.X += Step * (float)Index;
        Vertices[2].UV = Rr_V2F(0.0f);

        Vertices[3].Color = *ColorB;
        Vertices[3].Position = Layout->Cursor;
        Vertices[3].Position.X += Step * (float)Index + Step;
        Vertices[3].Position.Y += TargetSize;
        Vertices[3].UV = Rr_V2F(0.0f);

        Vertices += 4;
    }

    {
        Rr_UIVertex Vertices[4];

        Rr_Vec4 ColorA = Rr_V4F(0.0f);
        Rr_Vec4 ColorB = Rr_V4F(1.0f);

        Vertices[0].Color = ColorA;
        Vertices[0].Position = Layout->Cursor;
        Vertices[0].UV = Rr_V2F(0.0f);

        Vertices[1].Color = ColorA;
        Vertices[1].Position = Layout->Cursor;
        Vertices[1].Position.X += TargetSize;
        Vertices[1].UV = Rr_V2F(0.0f);

        Vertices[2].Color = ColorB;
        Vertices[2].Position = Layout->Cursor;
        Vertices[2].Position.Y += TargetSize;
        Vertices[2].UV = Rr_V2F(0.0f);

        Vertices[3].Color = ColorB;
        Vertices[3].Position = Layout->Cursor;
        Vertices[3].Position.X += TargetSize;
        Vertices[3].Position.Y += TargetSize;
        Vertices[3].UV = Rr_V2F(0.0f);

        Rr_UIDrawQuad(Vertices);
    }

    Rr_UIDrawInnerFrame(
        &(Rr_Rect){ Layout->Cursor, Rr_V2F(TargetSize) },
        gUIContext->FrameThickness,
        &gUIContext->Style.Outline);

    Rr_UIAdvance(Rr_V2F(TargetSize));
    Rr_UILabelF("%.2f %.2f %.2f %.2f", Color->X, Color->Y, Color->Z, Color->W);

    {
        unsigned char R = (unsigned char)(Color->X * 255.0f);
        unsigned char G = (unsigned char)(Color->Y * 255.0f);
        unsigned char B = (unsigned char)(Color->Z * 255.0f);
        unsigned char A = (unsigned char)(Color->W * 255.0f);
        Rr_UILabelF("%d %d %d %d", R, G, B, A);
    }

    Rr_UIEndWindow();
}

static inline float Rr_UISlider(
    const char *Title,
    float Normalized,
    const char *ValueCString,
    size_t ValueCStringLength)
{
    Rr_UIAssertWindow();
    assert(Title != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_Vec2 TitleSize =
        Rr_UIDrawText(true, Layout->Cursor, TitleLength, Title, 0.0f, NULL, 0);

    float SliderWidth = Layout->AvailableContentsWidth -
                        gUIContext->ButtonPadding.Width - TitleSize.Width;
    Rr_Rect SliderRect = {
        Layout->Cursor,
        {
            SliderWidth,
            gUIContext->LineHeight,
        },
    };

    Rr_UIDrawBevel(&SliderRect, &gUIContext->Style.ButtonDisabled, true);

    float HandleWidth = gUIContext->FontSize;
    Rr_Rect HandleRect = { Layout->Cursor,
                           Rr_V2(HandleWidth, gUIContext->LineHeight) };
    HandleRect.Offset.X += Normalized * (SliderWidth - HandleWidth);
    HandleRect.Offset.X = roundf(HandleRect.Offset.X);
    HandleRect = Rr_ResizeRect(&HandleRect, -gUIContext->BevelThickness);
    Rr_UIDrawBevel(&HandleRect, &gUIContext->Style.ButtonNormal, false);

    if (ValueCString != NULL)
    {
        Rr_Vec2 ValueSize = Rr_UIDrawText(
            true,
            Rr_V2F(0.0f),
            SIZE_MAX,
            ValueCString,
            0.0f,
            NULL,
            0);

        Rr_Vec2 ValuePosition = Layout->Cursor;
        ValuePosition.X = HandleRect.Offset.X + HandleWidth / 2.0f +
                          gUIContext->ButtonPadding.Width;
        bool ShowValue = true;
        if (ValuePosition.X + ValueSize.Width > SliderRect.Offset.X +
                                                    SliderRect.Extent.Width -
                                                    gUIContext->BevelThickness)
        {
            ValuePosition.X = HandleRect.Offset.X -
                              gUIContext->ButtonPadding.Width - ValueSize.Width;
            if (ValuePosition.X < SliderRect.Offset.X)
            {
                ShowValue = false;
            }
        }

        if (ShowValue)
        {
            Rr_UIDrawText(
                false,
                ValuePosition,
                SIZE_MAX,
                ValueCString,
                0.0f,
                &gUIContext->Style.Foreground,
                0);
        }
    }

    float HandleDragOffset = gUIContext->MousePosition.X -
                             (HandleRect.Offset.X + HandleWidth / 2.0f);

    bool Hovered, Dragging = Rr_UIDragBehavior(
                      Window,
                      &SliderRect,
                      RR_UI_DRAG_OP_WIDGET,
                      TitleHash,
                      Rr_V2(HandleDragOffset, 0.0f),
                      &Hovered,
                      NULL);

    if (Dragging)
    {
        float SliderMin = Layout->Cursor.X + HandleWidth / 2.0f;
        float SliderMax = SliderMin + SliderWidth - HandleWidth;

        Normalized =
            (gUIContext->MousePosition.X - SliderMin) / (SliderMax - SliderMin);
        Normalized = RR_CLAMP(0.0f, Normalized, 1.0f);
    }
    else if (Hovered)
    {
        if (gUIContext->MouseWheelDelta.X != 0.0f)
        {
            /* NOTE: Probably shouldn't be hardcoded to 30.0f. */
            Normalized += gUIContext->MouseWheelDelta.X / 30.0f;
        }
    }

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += SliderWidth + gUIContext->ButtonPadding.Width;
    Rr_UIDrawText(
        0,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        Layout->AvailableContentsWidth,
        gUIContext->LineHeight + Layout->ContentsPadding.Height,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return Normalized;
}

bool Rr_UISliderInt(const char *Title, int32_t *Value, int32_t Min, int32_t Max)
{
    assert(Value != NULL);
    assert(Max >= Min);

    char Buffer[32];
    size_t Length = (size_t)snprintf(Buffer, 32, "%d", *Value);

    int32_t In = RR_CLAMP(Min, *Value, Max);
    float InNormalized = (float)(*Value - Min) / (float)(Max - Min);
    float OutNormalized = Rr_UISlider(Title, InNormalized, Buffer, Length);
    OutNormalized =
        roundf(OutNormalized * (float)(Max - Min)) / (float)(Max - Min);
    int32_t Out = (int32_t)(OutNormalized * (float)((Max - Min) + Min));
    *Value = Out;
    return In != Out;
}

bool Rr_UISliderFloat(const char *Title, float *Value, float Min, float Max)
{
    assert(Value != NULL);
    assert(Max >= Min);

    char Buffer[32];
    size_t Length = (size_t)snprintf(Buffer, 32, "%.4f", *Value);

    float In = RR_CLAMP(Min, *Value, Max);
    float InNormalized = (*Value - Min) / (Max - Min);
    float OutNormalized = Rr_UISlider(Title, InNormalized, Buffer, Length);
    float Out = OutNormalized * (Max - Min) + Min;
    *Value = Out;
    return In != Out;
}

static inline void Rr_UIRGBAToHexString(Rr_Vec4 *Color, char *Buffer)
{
    for (size_t Index = 0; Index < 4; ++Index)
    {
        float FloatValue = Color->Elements[Index];
        uint8_t Value = (uint8_t)(RR_CLAMP(0.0f, FloatValue, 1.0f) * 255.0f);
        sprintf(Buffer + (Index * 2), "%02X", Value);
    }
    Buffer[8] = '\0';
}

bool Rr_UIColorPicker(const char *Title, Rr_Vec4 *Color)
{
    Rr_UIAssertWindow();
    assert(Title != NULL);
    assert(Color != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    char Buffer[8 + 1];
    Rr_UIRGBAToHexString(Color, Buffer);

    Rr_Vec2 FieldExtent;
    bool ChangesConfirmed = Rr_UIGenericInputField(
        TitleHash,
        Layout->Cursor,
        9,
        Buffer,
        NULL,
        Rr_UIHexFilter,
        RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT,
        &FieldExtent);
    if (ChangesConfirmed)
    {
        uint32_t NewColor;
        sscanf(Buffer, "%x", &NewColor);
        *Color = Rr_U32ToRGBA(NewColor);
    }

    Rr_Vec2 ColorBoxSize = Rr_V2F(FieldExtent.Height);

    Rr_Vec2 ColorBoxPosition = Layout->Cursor;
    ColorBoxPosition.X += FieldExtent.Width;
    ColorBoxPosition.X -= gUIContext->BevelThickness;

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_UIButtonBehavior(
        Window,
        &(Rr_Rect){
            ColorBoxPosition,
            ColorBoxSize,
        },
        NULL,
        &Up,
        &Hovered,
        &Held);

    if (Up)
    {
        gUIContext->PopupWindowParent = Window;
        gUIContext->PopupWindowHash = TitleHash;
    }

    bool ColorChanged = false;

    if (gUIContext->PopupWindowParent == Window &&
        gUIContext->PopupWindowHash == TitleHash)
    {
        Rr_Vec2 PopupCenter =
            Rr_AddV2(ColorBoxPosition, Rr_DivV2F(ColorBoxSize, 2.0f));
        Rr_UIColorPickerPopup(PopupCenter, Color);
    }

    Rr_Rect BevelRect = {
        ColorBoxPosition,
        ColorBoxSize,
    };
    Rr_UIDrawBevel(&BevelRect, Color, Held);

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.Y += gUIContext->ButtonPadding.Height;
    TitlePosition.X += gUIContext->ButtonPadding.Width + ColorBoxSize.Width +
                       FieldExtent.Width;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        0,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        TitleSize.Width + Layout->ContentsPadding.Width +
            gUIContext->LineHeight,
        FieldExtent.Height + Layout->ContentsPadding.Height,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return ColorChanged;
}

bool Rr_UIWantMouseCapture(void)
{
    return gUIContext && (gUIContext->LeftMouseButtonDownOverWindow ||
                          gUIContext->HoveredWindow);
}

bool Rr_UIWantKeyboardCapture(void)
{
    return false;
}

static inline Rr_Vec4 Rr_U32ToSRGB(uint32_t Color)
{
    Rr_Vec4 Result;

    Result.R = (float)(Color >> 24) / 255.0f;
    Result.G = (float)((Color >> 16) & (0x000000FF)) / 255.0f;
    Result.B = (float)((Color >> 8) & (0x000000FF)) / 255.0f;
    Result.A = (float)(Color & (0x000000FF)) / 255.0f;

    Result.R = powf(Result.R, 2.2f);
    Result.G = powf(Result.G, 2.2f);
    Result.B = powf(Result.B, 2.2f);

    return Result;
}

void Rr_InitUI(void)
{
    assert(gUIContext == NULL);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gUIContext = RR_ALLOC_TYPE(Arena, Rr_UIContext);
    gUIContext->Arena = Arena;

    gUIContext->NextWindowPosition = Rr_V2F(INFINITY);
    gUIContext->NextWindowSize = Rr_V2F(INFINITY);
    gUIContext->NextWindowPadding = Rr_V2F(INFINITY);

    gUIContext->PopupWindow.Flags =
        RR_UI_WINDOW_FLAGS_NO_TITLE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_MINIMIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT | RR_UI_WINDOW_FLAGS_NO_MOVE_BIT |
        RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;

    Rr_IntVec2 DisplaySize = Rr_GetDisplaySize();
    Rr_UISetFontSize((float)DisplaySize.Width / 112.0f);

    gUIContext->Style = (Rr_UIStyle){
        .TitlePadding = { 0.5f, 0.125f },
        .ContentsPadding = { 0.5f, 0.5f },
        .BevelIntensityLight = 0.3f,
        .BevelIntensityDark = 0.7f,

        .Foreground = Rr_U32ToSRGB(0xD6D0B3FF),
        .ForegroundDimmed = Rr_U32ToSRGB(0xA7A59CFF),
        .Background = Rr_U32ToSRGB(0x292F33FF),
        .TitleBackground = Rr_U32ToSRGB(0x5E2D96FF),
        .TitleButtonBackground = Rr_U32ToSRGB(0xD54251FF),
        .Outline = Rr_U32ToSRGB(0x6C6F72FF),
        .SelectedTextBackground = Rr_U32ToSRGB(0x6EA5FEFF),

        .ButtonNormal = Rr_U32ToSRGB(0x4c565dFF),
        .ButtonHovered = Rr_U32ToSRGB(0x687e8dFF),
        .ButtonHeld = Rr_U32ToSRGB(0x435866FF),
        .ButtonDisabled = Rr_U32ToSRGB(0x191e22FF),
    };

    gUIContext->Style.ScrollbarBackground = gUIContext->Style.ButtonDisabled;
    gUIContext->Style.ScrollbarNormal = gUIContext->Style.ButtonNormal;
    gUIContext->Style.ScrollbarHovered = gUIContext->Style.ButtonHovered;
    gUIContext->Style.ScrollbarHeld = gUIContext->Style.ButtonHeld;

    Rr_Binding Bindings[] = {
        {
            .Index = 0,
            .Type = RR_BINDING_TYPE_UNIFORM_BUFFER,
            .Stages = RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .Index = 1,
            .Type = RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
            .Stages = RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    Rr_BindingSet BindingSets[] = {
        {
            RR_ARRAY_COUNT(Bindings),
            Bindings,
        },
    };
    gUIContext->PipelineLayout =
        Rr_CreatePipelineLayout(RR_ARRAY_COUNT(BindingSets), BindingSets);

    Rr_ColorTargetInfo ColorTargets[] = {
        {
            .Format = Rr_GetSwapchainFormat(),
            .Blend = Rr_AlphaBlend(),
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
        .Layout = gUIContext->PipelineLayout,
        .VertexShaderSPV = Rr_LoadAsset(RR_BUILTIN_UI_VERT_SPV),
        .FragmentShaderSPV = Rr_LoadAsset(RR_BUILTIN_UI_FRAG_SPV),
        .ColorTargetCount = RR_ARRAY_COUNT(ColorTargets),
        .ColorTargets = ColorTargets,
        .VertexInputBindingCount = 1,
        .VertexInputBindings = &VertexInputBinding,
    };

    gUIContext->GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    gUIContext->VertexBuffer = Rr_CreateBuffer(
        RR_MEGABYTES(8),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    gUIContext->IndexBuffer = Rr_CreateBuffer(
        RR_MEGABYTES(8),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    gUIContext->UniformBuffer = Rr_CreateBuffer(
        sizeof(Rr_UIUniformData),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    gUIContext->Sampler = Rr_CreateSampler(&(Rr_SamplerInfo){
        .MinFilter = RR_FILTER_LINEAR,
        .MagFilter = RR_FILTER_LINEAR,
    });

    gUIContext->Font = Rr_UICreateFont(
        gUIContext,
        Rr_GetGraph(),
        RR_BUILTIN_SOURCESERIF4_PNG,
        RR_BUILTIN_SOURCESERIF4_JSON);
}

void Rr_CleanupUI(void)
{
    assert(gUIContext != NULL);

    Rr_ReleaseBuffer(gUIContext->VertexBuffer);
    Rr_ReleaseBuffer(gUIContext->IndexBuffer);
    Rr_ReleaseBuffer(gUIContext->UniformBuffer);
    Rr_ReleaseSampler(gUIContext->Sampler);
    Rr_ReleasePipelineLayout(gUIContext->PipelineLayout);
    Rr_ReleaseGraphicsPipeline(gUIContext->GraphicsPipeline);

    Rr_UIReleaseFont(gUIContext, gUIContext->Font);

    Rr_DestroyArena(gUIContext->Arena);

    gUIContext = NULL;
}

void Rr_ProcessUIEvent(Rr_Event *Event)
{
    if (gUIContext == NULL)
    {
        return;
    }

    /* TODO: Set mouse position here. */

    switch (Event->Type)
    {
        case RR_EVENT_TYPE_TEXT_INPUT:
        {
            size_t Length = strlen(Event->Text.Text);
            char *Text = RR_ALLOC_NO_ZERO(gUIContext->FrameArena, Length + 1);
            memcpy(Text, Event->Text.Text, Length + 1);
            *RR_PUSH_INTO_ARRAY(
                &gUIContext->TextInputEvents,
                gUIContext->FrameArena) = Text;
        }
        break;
        case RR_EVENT_TYPE_KEY_DOWN:
        case RR_EVENT_TYPE_KEY_UP:
        {
            *RR_PUSH_INTO_ARRAY(
                &gUIContext->KeyboardInputEvents,
                gUIContext->FrameArena) = Event->Key;
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_DOWN:
        {
            if (Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                gUIContext->LeftMouseButtonClicks = Event->MouseButton.Clicks;
                gUIContext->LeftMouseButtonDown = true;
                gUIContext->LeftMouseButtonHeld = true;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_UP:
        {
            if (Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                gUIContext->LeftMouseButtonUp = true;
                gUIContext->LeftMouseButtonHeld = false;
                gUIContext->LeftMouseButtonClickId++;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_MOTION:
        {
            gUIContext->MouseMoved = true;
            gUIContext->MousePosition = Event->MouseMotion.Position;
        }
        break;
        case RR_EVENT_TYPE_MOUSE_WHEEL:
        {
            gUIContext->MouseWheelDelta =
                Rr_SubV2(gUIContext->MouseWheelDelta, Event->Wheel.Amount);
        }
        break;
        default:
            break;
    }
}

static inline void Rr_UIConsumeNextFontSize(void)
{
    if (gUIContext->NextFontSize != INFINITY)
    {
        gUIContext->FontSize = gUIContext->NextFontSize;
        gUIContext->NextFontSize = INFINITY;

        gUIContext->LineHeight =
            gUIContext->FontSize * gUIContext->Font->LineHeight;
        gUIContext->ContentsPadding =
            Rr_MulV2F(gUIContext->Style.ContentsPadding, gUIContext->FontSize);
        gUIContext->HorizontalMargin = gUIContext->FontSize * 0.5f;

        gUIContext->FrameThickness =
            floorf(RR_MAX(1.0f, gUIContext->FontSize * 0.075f));
        gUIContext->ResizeHandleSize = RR_UI_ROUND(gUIContext->FontSize);
        gUIContext->ScrollbarWidth = gUIContext->ResizeHandleSize;
        gUIContext->ScrollbarHandleWidth =
            RR_UI_ROUND(gUIContext->ResizeHandleSize * 0.75f);
        gUIContext->SeparatorLineHeight = gUIContext->LineHeight * 0.5f;
        gUIContext->ButtonPadding =
            (Rr_Vec2){ gUIContext->LineHeight * 0.25f,
                       gUIContext->LineHeight * 0.125f };
        gUIContext->BevelThickness = ceilf(gUIContext->FontSize * 0.1f);

        gUIContext->WindowTitleHeight = RR_UI_ROUND(
            gUIContext->Style.TitlePadding.Height * 2.0f *
                gUIContext->FontSize +
            gUIContext->LineHeight);
        gUIContext->TitleButtonSize =
            RR_UI_ROUND(gUIContext->WindowTitleHeight);
        gUIContext->MinWindowSizeNoTitle =
            Rr_MulV2F(gUIContext->ContentsPadding, 2.0f);
        gUIContext->MinWindowSizeNoTitle.X += gUIContext->ScrollbarWidth;
        gUIContext->MinWindowSizeNoTitle.X += gUIContext->FontSize * 2.0f;
        gUIContext->MinWindowSizeNoTitle.Y += gUIContext->FontSize * 2.0f;
        gUIContext->MinWindowSizeNoTitle =
            RR_UI_ROUND_V2(gUIContext->MinWindowSizeNoTitle);
        gUIContext->MinWindowSize = gUIContext->MinWindowSizeNoTitle;
        gUIContext->MinWindowSize.Y += gUIContext->WindowTitleHeight;
        gUIContext->MinWindowSize = RR_UI_ROUND_V2(gUIContext->MinWindowSize);
    }
}

void Rr_NewUIFrame(void)
{
    gUIContext->FrameArena = gRenderer->Frames[gRenderer->FrameIndex].Arena;

    RR_RESET_ARRAY(&gUIContext->Vertices, gUIContext->FrameArena);
    RR_RESET_ARRAY(&gUIContext->Indices, gUIContext->FrameArena);
    RR_RESET_ARRAY(&gUIContext->Stack, gUIContext->FrameArena);

    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();
    gUIContext->ScreenSize.Width = (float)SwapchainSize.Width;
    gUIContext->ScreenSize.Height = (float)SwapchainSize.Height;
}

void Rr_BeginUI(void)
{
    Rr_UIConsumeNextFontSize();

    if (gUIContext->SkipLeftMouseButtonUp && gUIContext->LeftMouseButtonUp)
    {
        gUIContext->SkipLeftMouseButtonUp = false;
        gUIContext->LeftMouseButtonUp = false;
    }

    gUIContext->HoveredWindow = NULL;
    if (gUIContext->PopupWindowParent)
    {
        if (Rr_RectContains(
                &gUIContext->PopupWindow.Rect,
                gUIContext->MousePosition))
        {
            gUIContext->HoveredWindow = &gUIContext->PopupWindow;

            if (gUIContext->LeftMouseButtonDown)
            {
                gUIContext->LeftMouseButtonDownOverWindow = true;
            }
        }
        else if (gUIContext->LeftMouseButtonDown)
        {
            Rr_UIClosePopupWindow();
            gUIContext->SkipLeftMouseButtonUp = true;
        }
    }
    else if (gUIContext->HoveredWindow == NULL)
    {
        int LastIndex = (int)gUIContext->ActiveWindows.Count - 1;
        for (int Index = LastIndex; Index >= 0; --Index)
        {
            Rr_UIWindow *Window = gUIContext->ActiveWindows.Data[Index];
            if (Rr_RectContains(&Window->Rect, gUIContext->MousePosition))
            {
                gUIContext->HoveredWindow = Window;

                if (gUIContext->LeftMouseButtonDown)
                {
                    Rr_UIPutWindowOnTop(Window);

                    gUIContext->LeftMouseButtonDownOverWindow = true;
                }

                break;
            }
        }
    }

    RR_CLEAR_ARRAY(&gUIContext->ActiveWindows);
}

static inline int Rr_UIWindowSort(const void *A, const void *B)
{
    const Rr_UIWindow *WindowA = *(Rr_UIWindow **)A;
    const Rr_UIWindow *WindowB = *(Rr_UIWindow **)B;

    return WindowA->Z > WindowB->Z;
}

static inline void Rr_UIDrawWindow(
    Rr_UIWindow *Window,
    Rr_GraphNode *GraphicsNode)
{
    Window->Added = false;

    if (Window->SkipDueToAutoResize)
    {
        Window->SkipDueToAutoResize = false;
        return;
    }

    for (size_t ClipRectIndex = 0; ClipRectIndex < Window->ClipRects.Count;
         ++ClipRectIndex)
    {
        Rr_UIClipRect *ClipRect = Window->ClipRects.Data + ClipRectIndex;

        Rr_IntRect IntRect = {
            { (int32_t)floorf(ClipRect->Rect.Offset.X),
              (int32_t)floorf(ClipRect->Rect.Offset.Y) },
            { (int32_t)ceilf(ClipRect->Rect.Extent.Width),
              (int32_t)ceilf(ClipRect->Rect.Extent.Height) },
        };
        if (IntRect.Offset.X < 0)
        {
            IntRect.Extent.Width += IntRect.Offset.X;
            IntRect.Offset.X = 0;
        }
        if (IntRect.Offset.Y < 0)
        {
            IntRect.Extent.Height += IntRect.Offset.Y;
            IntRect.Offset.Y = 0;
        }
        if (IntRect.Extent.Width < 0 || IntRect.Extent.Height < 0)
        {
            continue;
        }
        Rr_SetScissor(GraphicsNode, &IntRect);

        Rr_DrawIndexed(
            GraphicsNode,
            ClipRect->IndexCount,
            1,
            ClipRect->FirstIndex,
            0,
            0);
    }
}

void Rr_EndUI(void)
{
    Rr_UIAssertNoWindow();

    /* TODO: Consider active popup as well. */
    if (gUIContext->ActiveWindows.Count == 0)
    {
        return;
    }

    Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

    Rr_UIUniformData UniformData = {
        .ScreenSize = gUIContext->ScreenSize,
        .DistanceRange = gUIContext->Font->DistanceRange,
        .Time = (float)Rr_GetTimeSeconds(),
    };
    char *MappedUniformData = Rr_GetMappedBufferData(gUIContext->UniformBuffer);
    memcpy(MappedUniformData, &UniformData, sizeof(UniformData));

    Rr_UIVertex *VertexBufferData =
        Rr_GetMappedBufferData(gUIContext->VertexBuffer);
    memcpy(
        VertexBufferData,
        gUIContext->Vertices.Data,
        sizeof(Rr_UIVertex) * gUIContext->Vertices.Count);

    Rr_UIIndex *IndexBufferData =
        Rr_GetMappedBufferData(gUIContext->IndexBuffer);
    memcpy(
        IndexBufferData,
        gUIContext->Indices.Data,
        sizeof(Rr_UIIndex) * gUIContext->Indices.Count);

    Rr_ColorTarget ColorTarget = {
        .Slot = 0,
        .LoadOp = RR_LOAD_OP_LOAD,
        .StoreOp = RR_STORE_OP_STORE,
        .Image = SwapchainImage,
    };
    Rr_GraphNode *GraphicsNode =
        Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, NULL);
    Rr_BindGraphicsPipeline(GraphicsNode, gUIContext->GraphicsPipeline);
    Rr_BindVertexBuffer(GraphicsNode, gUIContext->VertexBuffer, 0, 0);
    Rr_BindIndexBuffer(
        GraphicsNode,
        gUIContext->IndexBuffer,
        0,
        0,
        RR_INDEX_TYPE_UINT16);
    Rr_BindUniformBuffer(
        GraphicsNode,
        gUIContext->UniformBuffer,
        0,
        0,
        0,
        sizeof(Rr_UIUniformData));
    Rr_BindCombinedImage2DSampler(
        GraphicsNode,
        gUIContext->Font->Atlas,
        gUIContext->Sampler,
        0,
        1);

    qsort(
        gUIContext->ActiveWindows.Data,
        gUIContext->ActiveWindows.Count,
        sizeof(Rr_UIWindow *),
        Rr_UIWindowSort);

    gUIContext->HighestWindow =
        gUIContext->ActiveWindows.Data[gUIContext->ActiveWindows.Count - 1];

    for (size_t Index = 0; Index < gUIContext->ActiveWindows.Count; ++Index)
    {
        Rr_UIWindow *Window = gUIContext->ActiveWindows.Data[Index];
        Window->Z = (int32_t)Index;
        Rr_UIDrawWindow(Window, GraphicsNode);
    }

    if (gUIContext->PopupWindowParent)
    {
        Rr_UIDrawWindow(&gUIContext->PopupWindow, GraphicsNode);
    }

    if (gUIContext->LeftMouseButtonUp)
    {
        gUIContext->LeftMouseButtonHeld = false;
        gUIContext->LeftMouseButtonDownOverWindow = false;
    }
    gUIContext->MouseMoved = false;
    gUIContext->LeftMouseButtonClicks = 0;
    gUIContext->LeftMouseButtonDown = false;
    gUIContext->LeftMouseButtonUp = false;
    gUIContext->MouseWheelDelta = Rr_V2F(0.0f);
    gUIContext->DragOpBeganThisFrame = false;
    gUIContext->DragOpEndedThisFrame = false;
    if (gUIContext->MouseOverTextInput)
    {
        Rr_SetCursor(RR_UI_CURSOR_TYPE_TEXT);
    }
    else
    {
        Rr_SetCursor(RR_UI_CURSOR_TYPE_NORMAL);
    }
    gUIContext->MouseOverTextInput = false;
    RR_ZERO(gUIContext->TextInputEvents);
    RR_ZERO(gUIContext->KeyboardInputEvents);
}

void Rr_UISetFontSize(float Size)
{
    if (gUIContext)
    {
        gUIContext->NextFontSize = RR_CLAMP(
            RR_UI_MIN_FONT_SIZE,
            RR_UI_ROUND(Size),
            RR_UI_MAX_FONT_SIZE);
    }
}

static inline void Rr_UIDebugOverlayArena(Rr_Arena *Arena, const char *Comment)
{
    Rr_UILabelF(
        "%s: commited %d bytes, position %p",
        Comment,
        Arena->Commited,
        (Arena + Arena->Position));
}

void Rr_UIDebugOverlay(void)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    if (Rr_UIBeginWindow(
            "Rr_DebugOverlay",
            NULL,
            RR_UI_WINDOW_FLAGS_NO_TITLE_BIT |
                RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT |
                RR_UI_WINDOW_FLAGS_NO_MOVE_BIT))
    {
        Rr_UIBeginTabs("DebugOverlayTabs");
        if (Rr_UITab("General"))
        {
            Rr_UILabelF("Time: %.2f", Rr_GetTimeSeconds());
            Rr_Vec2 MousePosition = Rr_GetMousePosition();
            Rr_UILabelF(
                "Mouse Position: %.1f %.1f",
                MousePosition.X,
                MousePosition.Y);
            Rr_Vec2 MouseDelta = Rr_GetMousePositionDelta();
            Rr_UILabelF("Mouse Delta: %.1f %.1f", MouseDelta.X, MouseDelta.Y);
            Rr_UILabelF("UI Font Size: %.2f", gUIContext->FontSize);
            Rr_UISeparator();
            uint32_t PresentModeCount;
            Rr_PresentMode *PresentModes =
                Rr_GetAvailablePresentModes(&PresentModeCount);
            const char **PresentModeStrings = RR_ALLOC(
                Scratch.Arena,
                PresentModeCount * sizeof(const char *));
            uint32_t CurrentPresentModeIndex;
            for (uint32_t Index = 0; Index < PresentModeCount; ++Index)
            {
                if (gRenderer->Swapchain.PresentMode == PresentModes[Index])
                {
                    CurrentPresentModeIndex = Index;
                }
                PresentModeStrings[Index] =
                    Rr_GetPresentModeString(PresentModes[Index]);
            }
            if (Rr_UICombobox(
                    "Present Mode",
                    PresentModeCount,
                    PresentModeStrings,
                    &CurrentPresentModeIndex))
            {
                Rr_SetPresentMode(PresentModes[CurrentPresentModeIndex]);
            }

            {
                static double SamplingFreq = 0.5;
                static double LastSample = 0;
                static double LastFPS = 0;
                static uint64_t Frames = 0;
                Frames++;
                double Now = Rr_GetTimeSeconds();
                if (Now - LastSample > SamplingFreq)
                {
                    LastFPS = (double)Frames / SamplingFreq;
                    LastSample = Now;
                    Frames = 0;
                }
                Rr_UILabelF("FPS: %.2f", LastFPS);
            }

            Rr_UICheckbox(
                "Frame Limiter Enabled",
                &gApp->FrameTime.EnableFrameLimiter);
            static float TargetFramerate = INFINITY;
            if (TargetFramerate == INFINITY)
            {
                TargetFramerate = (float)gApp->FrameTime.TargetFramerate;
            }
            if (Rr_UIInputFloat("Frame Limit", &TargetFramerate))
            {
                gApp->FrameTime.TargetFramerate = (uint64_t)TargetFramerate;
            }
            if (Rr_UIButton("Toggle Fullscreen"))
            {
                Rr_ToggleWindowFullscreen();
            }
        }
        if (Rr_UITab("Memory"))
        {
            Rr_UIDebugOverlayArena(gApp->Arena, "Application");
            Rr_UIDebugOverlayArena(gRenderer->Arena, "Renderer");
            for (uint32_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
            {
                Rr_Frame *Frame = gRenderer->Frames + Index;
                char FrameString[64];
                sprintf(FrameString, "Frame#%d", Index);
                Rr_UIDebugOverlayArena(Frame->Arena, FrameString);
            }
            Rr_UIDebugOverlayArena(gUIContext->Arena, "UI");
            Rr_UIDebugOverlayArena(gPlatform->Arena, "Window");
        }
        if (Rr_UITab("Renderer"))
        {
            Rr_UILabelF("Frame: %zu", gRenderer->FrameNumber);
            Rr_UILabelF(
                "Images: %zu/%zu",
                gRenderer->Images.Count,
                gRenderer->Images.Capacity);
            Rr_UILabelF(
                "Buffers: %zu/%zu",
                gRenderer->Buffers.Count,
                gRenderer->Buffers.Capacity);
            Rr_UILabelF(
                "DescriptorSetLayouts: %zu/%zu",
                gRenderer->DescriptorSetLayoutStorage.Hive.Count,
                gRenderer->DescriptorSetLayoutStorage.Hive.Capacity);
            Rr_UILabelF(
                "DescriptorPools: %zu",
                gRenderer->DescriptorPoolListCount);
            Rr_UILabelF(
                "PipelineLayouts: %zu/%zu",
                gRenderer->PipelineLayouts.Count,
                gRenderer->PipelineLayouts.Capacity);
            Rr_UILabelF(
                "ComputePipelines: %zu/%zu",
                gRenderer->ComputePipelines.Count,
                gRenderer->ComputePipelines.Capacity);
            Rr_UILabelF(
                "GraphicsPipelines: %zu/%zu",
                gRenderer->GraphicsPipelines.Count,
                gRenderer->GraphicsPipelines.Capacity);
            Rr_UILabelF(
                "Samplers: %zu/%zu",
                gRenderer->Samplers.Count,
                gRenderer->Samplers.Capacity);
            Rr_UILabelF(
                "Render Passes: %zu/%zu",
                gRenderer->RenderPassStorage.Hive.Count,
                gRenderer->RenderPassStorage.Hive.Capacity);
            Rr_UILabelF(
                "Framebuffers: %zu/%zu",
                gRenderer->FramebufferStorage.Hive.Count,
                gRenderer->FramebufferStorage.Hive.Capacity);
            Rr_UILabelF(
                "SwapchainImages: %zu",
                gRenderer->SwapchainImages.Count);
            Rr_UILabelF(
                "SyncStates: %zu/%zu",
                gRenderer->SyncStateStorage.Hive.Count,
                gRenderer->SyncStateStorage.Hive.Capacity);
        }
        Rr_UIEndTabs();
        Rr_UIEndWindow();
    }

    Rr_DestroyScratch(Scratch);
}
