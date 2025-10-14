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

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#define STBI_NO_GIF
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#define STBI_NO_TGA
#define STBI_NO_FAILURE_STRINGS
#include <stb/stb_image.h>

#include <assert.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>

typedef uint64_t Rr_UIHash;
typedef uint16_t Rr_UIIndex;

typedef struct Rr_UIVertex Rr_UIVertex;
struct Rr_UIVertex
{
    Rr_Vec2 Position;
    Rr_Vec2 UV;
    Rr_Vec4 Color;
};

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

typedef RR_ARRAY(Rr_UIClipRect) Rr_UIClipRectArray;

typedef struct Rr_UIWindow Rr_UIWindow;
struct Rr_UIWindow
{
    const char *Title;
    Rr_UIHash Hash;
    Rr_UIWindowFlags Flags;
    Rr_Rect Rect;
    Rr_Rect VisibleRect;
    Rr_Vec2 ContentsStart;
    Rr_Vec2 ContentsEnd;
    float VScroll;
    float HScroll;
    int32_t Z;

    bool Collapsed;
    bool Added;
    bool Open;
    bool Child;

    bool OpenedThisFrame;
    bool SkipThisFrame;

    float MaxFlexibleWidgetTitleWidth;

    Rr_Map *WidgetMap;
    Rr_Map *ChildWindowMap;

    Rr_UIWindow *TopLevelParent;
    Rr_UIClipRectArray *TopLevelClipRects;
    Rr_UIClipRect *CurrentClipRect;
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

    bool VerticalScrollbarAdded;

    bool LeftMouseButtonInsideRect;

    Rr_Vec2 TabCursor;
    Rr_UIHash *SelectedTabHash;

    Rr_Vec2 DeferredWindowOffset;
    Rr_Vec2 DeferredWindowExtent;
    Rr_Vec4 DeferredResizeHandleColor;
    bool DeferredAutoResize;

    Rr_UILayout *TopLevelLayout;
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
    RR_ARRAY(Rr_Rect) ClipRectBoundsStack;

    Rr_Vec2 NextWindowSize;
    Rr_Vec2 NextWindowPosition;
    Rr_Vec2 NextWindowOpenPosition;
    Rr_Vec2 NextWindowPadding;

    bool LeftMouseButtonDownOverWindow;

    bool SkipLeftMouseButtonUp;
    bool LeftMouseButtonDown;
    bool LeftMouseButtonHeld;
    bool LeftMouseButtonUp;
    uint32_t LeftMouseButtonClicks;
    uint32_t LeftMouseButtonClickId;
    bool MouseMoved;
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
    bool MouseOverTextInput;
    RR_ARRAY(const char *) TextInputEvents;
    RR_ARRAY(char) TextInputBuffer;
    bool DeferTextInputBufferCopy;

    RR_ARRAY(Rr_KeyEvent) KeyboardInputEvents;

    Rr_Vec2 ScreenSize;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    RR_FREE_LIST(Rr_UIFont) Fonts;
    Rr_UIFont *Font;
    float FontSize;
    float NextFontSize;

    float LineHeight;
    Rr_Vec2 ContentsPadding;
    float ComponentMargin;
    Rr_Vec2 MinWindowSize;
    Rr_Vec2 MinWindowSizeNoTitle;
    float TitleHeight;
    Rr_Vec2 TitlePadding;
    float TitleButtonSize;
    float ResizeHandleSize;
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

static inline bool Rr_UIIsHorizontal(void)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    return Layout && Layout->HorizontalX != INFINITY;
}

static inline void Rr_UIAssertNoWindow(void)
{
    assert(
        Rr_UICurrentWindow() == NULL &&
        "Did you forget to call Rr_UIEndWindow() or Rr_UIEndChild()?");
}

static inline void Rr_UIAssertWindow(void)
{
    assert(
        Rr_UICurrentWindow() != NULL &&
        "Did you forget to call Rr_UIBeginWindow() or Rr_UIBeginChild()?");
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

static inline Rr_UIPrimitive Rr_UIReserveQuads(size_t Count)
{
    Rr_UIPrimitive Primitive = Rr_UIReservePrimitive(Count * 4, Count * 6);

    static const Rr_UIIndex QUAD_INDICES[] = { 0, 1, 2, 3, 0, 2 };

    Rr_UIIndex Base = Primitive.BaseVertex;
    for (Rr_UIIndex QuadIndex = 0; QuadIndex < Count; ++QuadIndex)
    {
        for (size_t Index = 0; Index < 6; ++Index)
        {
            Primitive.Indices[QuadIndex * 6 + Index] =
                Base + QUAD_INDICES[Index];
        }
    }

    return Primitive;
}

static inline Rr_UIPrimitive Rr_UIReserveQuad(void)
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

static inline Rr_Vec2 Rr_UICalculateEdgeNormal(Rr_Vec2 A, Rr_Vec2 B)
{
    Rr_Vec2 C = Rr_NormV2(Rr_SubV2(B, A));
    return Rr_V2(C.Y, -C.X);
}

static inline void Rr_UIFeatherConvexPrimitive(
    Rr_UIPrimitive *SourcePrimitive,
    int VertexCount,
    float Amount)
{
#ifndef RR_UI_NO_FEATHERING
    Rr_UIVertex *Vertices = SourcePrimitive->Vertices;

    Rr_UIPrimitive Primitive =
        Rr_UIReservePrimitive((size_t)VertexCount, (size_t)VertexCount * 6);

    for (int Index = 0; Index < VertexCount; ++Index)
    {
        int NextIndex = (Index + 1) % VertexCount;

        Rr_UIVertex Previous =
            Vertices[(Index + VertexCount - 1) % VertexCount];
        Rr_UIVertex Current = Vertices[Index];
        Rr_UIVertex Next = Vertices[NextIndex];

        Rr_Vec2 N0 =
            Rr_UICalculateEdgeNormal(Previous.Position, Current.Position);
        Rr_Vec2 N1 = Rr_UICalculateEdgeNormal(Current.Position, Next.Position);

        Rr_Vec2 N = Rr_MulV2F(Rr_AddV2(N0, N1), 0.5f * Amount);
        Rr_Vec2 Position = Rr_AddV2(Current.Position, N);

        Primitive.Vertices[Index] = (Rr_UIVertex){
            .Position = Position,
            .UV = Rr_V2F(0.0f),
            .Color = { Current.Color.X,
                       Current.Color.Y,
                       Current.Color.Z,
                       0.0f, },
        };

        Primitive.Indices[Index * 6] =
            SourcePrimitive->BaseVertex + (Rr_UIIndex)Index;
        Primitive.Indices[Index * 6 + 1] =
            SourcePrimitive->BaseVertex + (Rr_UIIndex)NextIndex;
        Primitive.Indices[Index * 6 + 2] =
            Primitive.BaseVertex + (Rr_UIIndex)NextIndex;
        Primitive.Indices[Index * 6 + 3] =
            Primitive.BaseVertex + (Rr_UIIndex)NextIndex;
        Primitive.Indices[Index * 6 + 4] =
            Primitive.BaseVertex + (Rr_UIIndex)Index;
        Primitive.Indices[Index * 6 + 5] =
            SourcePrimitive->BaseVertex + (Rr_UIIndex)Index;
    }
#endif
}

static inline void Rr_UIDrawCircle(
    Rr_Vec2 Offset,
    float Radius,
    float Thickness,
    Rr_Vec4 *Color)
{
    static const int SEGMENTS = 20;

    Rr_UIPrimitive Primitive =
        Rr_UIReservePrimitive((size_t)SEGMENTS * 2, (size_t)SEGMENTS * 6);

    float Step = 2.0f * RR_PI32 / (float)SEGMENTS;

    for (int Index = 0; Index < SEGMENTS; ++Index)
    {
        Rr_Vec2 Base =
            Rr_V2(cosf((float)Index * Step), sinf((float)Index * Step));

        int InnerIndex = Index + SEGMENTS;

        Primitive.Vertices[Index].Position =
            Rr_AddV2(Offset, Rr_MulV2F(Base, Radius + Thickness / 2.0f));
        Primitive.Vertices[Index].UV = Rr_V2F(0.0f);
        Primitive.Vertices[Index].Color = *Color;

        Primitive.Vertices[InnerIndex].Position =
            Rr_AddV2(Offset, Rr_MulV2F(Base, Radius - Thickness / 2.0f));
        Primitive.Vertices[InnerIndex].UV = Rr_V2F(0.0f);
        Primitive.Vertices[InnerIndex].Color = *Color;

        int NextOuterIndex = (Index + 1) % SEGMENTS;
        int NextInnerIndex = SEGMENTS + NextOuterIndex;

        Primitive.Indices[Index * 6] =
            (Rr_UIIndex)(Primitive.BaseVertex + NextInnerIndex);
        Primitive.Indices[Index * 6 + 1] =
            (Rr_UIIndex)(Primitive.BaseVertex + NextOuterIndex);
        Primitive.Indices[Index * 6 + 2] =
            (Rr_UIIndex)(Primitive.BaseVertex + Index);
        Primitive.Indices[Index * 6 + 3] =
            (Rr_UIIndex)(Primitive.BaseVertex + NextInnerIndex);
        Primitive.Indices[Index * 6 + 4] =
            (Rr_UIIndex)(Primitive.BaseVertex + Index);
        Primitive.Indices[Index * 6 + 5] =
            (Rr_UIIndex)(Primitive.BaseVertex + InnerIndex);
    }

    Rr_UIFeatherConvexPrimitive(&Primitive, SEGMENTS, 1.0f);
    /* NOTE: A hack to feather inner part of the circle. */
    Primitive.BaseVertex += (Rr_UIIndex)SEGMENTS;
    Primitive.Vertices += SEGMENTS;
    Rr_UIFeatherConvexPrimitive(&Primitive, SEGMENTS, -1.0f);
}

static inline void Rr_UIDrawCircleFilled(
    Rr_Vec2 Offset,
    float Radius,
    Rr_Vec4 *Color)
{
    static const size_t SEGMENTS = 20;

    Rr_UIPrimitive Primitive =
        Rr_UIReservePrimitive((size_t)SEGMENTS, (size_t)(SEGMENTS - 2) * 3);

    const float Step = 2.0f * RR_PI32 / (float)SEGMENTS;

    for (size_t Index = 0; Index < SEGMENTS; ++Index)
    {
        Primitive.Vertices[Index].Position = Rr_AddV2(
            Offset,
            Rr_MulV2F(
                Rr_V2(cosf((float)Index * Step), sinf((float)Index * Step)),
                Radius));
        Primitive.Vertices[Index].UV = Rr_V2F(0.0f);
        Primitive.Vertices[Index].Color = *Color;
    }

    for (size_t Index = 0; Index < SEGMENTS - 2; ++Index)
    {
        Primitive.Indices[Index * 3] = (Rr_UIIndex)(Primitive.BaseVertex);
        Primitive.Indices[Index * 3 + 1] =
            (Rr_UIIndex)(Primitive.BaseVertex + Index + 1);
        Primitive.Indices[Index * 3 + 2] =
            (Rr_UIIndex)(Primitive.BaseVertex + Index + 2);
    }

    Rr_UIFeatherConvexPrimitive(&Primitive, (int)SEGMENTS, 1.0f);
}

static inline void Rr_UIDrawFitTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 Color)
{
    Rr_UIPrimitive Primitive = Rr_UIReservePrimitive(3, 3);

    Rr_Vec2 Positions[3];

    float HalfSize = Size * 0.5f;

    Positions[0] = Rr_RotateV2(Rr_V2(-HalfSize, -HalfSize), Angle);
    Positions[0].X = RR_CLAMP(-HalfSize, Positions[0].X, HalfSize);
    Positions[0].Y = RR_CLAMP(-HalfSize, Positions[0].Y, HalfSize);
    Positions[1] = Rr_RotateV2(Rr_V2(HalfSize, 0.0f), Angle);
    Positions[1].X = RR_CLAMP(-HalfSize, Positions[1].X, HalfSize);
    Positions[1].Y = RR_CLAMP(-HalfSize, Positions[1].Y, HalfSize);
    Positions[2] = Rr_RotateV2(Rr_V2(-HalfSize, HalfSize), Angle);
    Positions[2].X = RR_CLAMP(-HalfSize, Positions[2].X, HalfSize);
    Positions[2].Y = RR_CLAMP(-HalfSize, Positions[2].Y, HalfSize);

    Primitive.Vertices[0].Position = Rr_AddV2(Positions[0], Offset);
    Primitive.Vertices[0].UV = Rr_V2F(0.0f);
    Primitive.Vertices[0].Color = Color;

    Primitive.Vertices[1].Position = Rr_AddV2(Positions[1], Offset);
    Primitive.Vertices[1].UV = Rr_V2F(0.0f);
    Primitive.Vertices[1].Color = Color;

    Primitive.Vertices[2].Position = Rr_AddV2(Positions[2], Offset);
    Primitive.Vertices[2].UV = Rr_V2F(0.0f);
    Primitive.Vertices[2].Color = Color;

    Primitive.Indices[0] = Primitive.BaseVertex;
    Primitive.Indices[1] = Primitive.BaseVertex + 1;
    Primitive.Indices[2] = Primitive.BaseVertex + 2;

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 4.0f);
}

static inline void Rr_UIDrawEquilateralTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 Color)
{
    Rr_UIPrimitive Primitive = Rr_UIReservePrimitive(3, 3);

    /* NOTE: Find center of mass for equilateral triangle. */
    float X = sqrtf(Size * Size / 2.0f);

    Rr_Vec2 Positions[3];

    Positions[0] = Rr_RotateV2(Rr_V2(X, 0), Angle);
    Positions[1] = Rr_RotateV2(
        Rr_MulV2F(
            Rr_V2(
                cosf(RR_PI32 * 0.3333f * 2.0f),
                sinf(RR_PI32 * 0.3333f * 2.0f)),
            X),
        Angle);
    Positions[2] = Rr_RotateV2(
        Rr_MulV2F(
            Rr_V2(
                cosf(RR_PI32 * 0.6666f * 2.0f),
                sinf(RR_PI32 * 0.6666f * 2.0f)),
            X),
        Angle);

    Primitive.Vertices[0].Position = Rr_AddV2(Positions[0], Offset);
    Primitive.Vertices[0].UV = Rr_V2F(0.0f);
    Primitive.Vertices[0].Color = Color;

    Primitive.Vertices[1].Position = Rr_AddV2(Positions[1], Offset);
    Primitive.Vertices[1].UV = Rr_V2F(0.0f);
    Primitive.Vertices[1].Color = Color;

    Primitive.Vertices[2].Position = Rr_AddV2(Positions[2], Offset);
    Primitive.Vertices[2].UV = Rr_V2F(0.0f);
    Primitive.Vertices[2].Color = Color;

    Primitive.Indices[0] = Primitive.BaseVertex;
    Primitive.Indices[1] = Primitive.BaseVertex + 1;
    Primitive.Indices[2] = Primitive.BaseVertex + 2;

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 4.0f);
}

static inline void Rr_UIDrawQuad(Rr_UIVertex *Vertices)
{
    Rr_UIPrimitive Primitive = Rr_UIReserveQuad();
    memcpy(Primitive.Vertices, Vertices, sizeof(Rr_UIVertex) * 4);
}

static inline void Rr_UISolidQuad(
    Rr_UIVertex *Vertices,
    Rr_Rect *Rect,
    Rr_Vec4 *Color)
{
    memcpy(
        Vertices,
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
                .Position = { Rect->Offset.X + Rect->Extent.X,
                              Rect->Offset.Y + Rect->Extent.Y },
                .Color = *Color,
            },
            {
                .Position = { Rect->Offset.X, Rect->Offset.Y + Rect->Extent.Y },
                .Color = *Color,
            },
        },
        sizeof(Rr_UIVertex) * 4);
}

static inline void Rr_UIRotatedQuad(
    Rr_UIVertex *Vertices,
    Rr_Rect *Rect,
    float Angle,
    Rr_Vec4 *Color)
{
    Rr_Vec2 Center = Rr_RectCenter(Rect);

    memcpy(
        Vertices,
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
                            (Rr_Vec2){ Rect->Offset.X + Rect->Extent.X,
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
                            (Rr_Vec2){ Rect->Offset.X,
                                       Rect->Offset.Y + Rect->Extent.Y },
                            Center),
                        Angle)),
                .Color = *Color,
            },
        },
        sizeof(Rr_UIVertex) * 4);
}

static inline void Rr_UIHorizontalGradientQuad(
    Rr_UIVertex *Vertices,
    Rr_Rect *Rect,
    Rr_Vec4 *ColorA,
    Rr_Vec4 *ColorB)
{
    memcpy(
        Vertices,
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
                .Position = { Rect->Offset.X + Rect->Extent.X,
                              Rect->Offset.Y + Rect->Extent.Y },
                .Color = *ColorB,
            },
            {
                .Position = { Rect->Offset.X, Rect->Offset.Y + Rect->Extent.Y },
                .Color = *ColorA,
            },
        },
        sizeof(Rr_UIVertex) * 4);
}

static inline void Rr_UIVerticalGradientQuad(
    Rr_UIVertex *Vertices,
    Rr_Rect *Rect,
    Rr_Vec4 *ColorA,
    Rr_Vec4 *ColorB)
{
    memcpy(
        Vertices,
        (Rr_UIVertex[]){
            {
                .Position = Rect->Offset,
                .Color = *ColorA,
            },
            {
                .Position = { Rect->Offset.X + Rect->Extent.X, Rect->Offset.Y },
                .Color = *ColorA,
            },
            {
                .Position = { Rect->Offset.X + Rect->Extent.X,
                              Rect->Offset.Y + Rect->Extent.Y },
                .Color = *ColorB,
            },
            {
                .Position = { Rect->Offset.X, Rect->Offset.Y + Rect->Extent.Y },
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

static inline void Rr_UIDrawSolidQuad(Rr_Rect *Rect, Rr_Vec4 *Color)
{
    Rr_UIPrimitive Primitive = Rr_UIReserveQuad();
    Rr_UISolidQuad(Primitive.Vertices, Rect, Color);
}

static inline void Rr_UIDrawRotatedQuad(
    Rr_Rect *Rect,
    float Angle,
    Rr_Vec4 *Color)
{
    Rr_UIPrimitive Primitive = Rr_UIReserveQuad();
    Rr_UIRotatedQuad(Primitive.Vertices, Rect, Angle, Color);
    Rr_UIFeatherConvexPrimitive(&Primitive, 4, 2.0f);
}

static inline void Rr_UIDrawHorizontalGradientQuad(
    Rr_Rect *Rect,
    Rr_Vec4 *ColorA,
    Rr_Vec4 *ColorB)
{
    Rr_UIPrimitive Primitive = Rr_UIReserveQuad();
    Rr_UIHorizontalGradientQuad(Primitive.Vertices, Rect, ColorA, ColorB);
}

static inline void Rr_UIDrawVerticalGradientQuad(
    Rr_Rect *Rect,
    Rr_Vec4 *ColorA,
    Rr_Vec4 *ColorB)
{
    Rr_UIPrimitive Primitive = Rr_UIReserveQuad();
    Rr_UIVerticalGradientQuad(Primitive.Vertices, Rect, ColorA, ColorB);
}

static inline void Rr_UIDrawRect(Rr_Rect *Rect, Rr_Vec4 *Color)
{
    Rr_UIPrimitive Primitive = Rr_UIReserveQuad();
    Rr_UISolidQuad(Primitive.Vertices, Rect, Color);
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
            .Position = { Rect->Offset.X + Rect->Extent.Width,
                          Rect->Offset.Y + Rect->Extent.Height },
            .UV = UVs[3],
            .Color = *Color,
        },
        {
            .Position = { Rect->Offset.X,
                          Rect->Offset.Y + Rect->Extent.Height },
            .UV = UVs[2],
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

static inline Rr_Vec2 Rr_UICalculateTextSize(
    size_t UTF8StringLength,
    const char *UTF8String,
    float AvailableWidth,
    Rr_UITextFlags Flags)
{
    return Rr_UIDrawText(
        true,
        Rr_V2F(0.0f),
        UTF8StringLength,
        UTF8String,
        AvailableWidth,
        NULL,
        Flags);
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
    gUIContext->DragOpHash = 0;
}

static inline bool Rr_UIClipRectContains(Rr_UIWindow *Window, Rr_Vec2 Point)
{
    assert(Window != NULL);
    return Rr_RectContains(&Window->CurrentClipRect->Rect, Point);
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
    if (gUIContext->FocusedWindow != NULL)
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
        Rr_RectContains(Rect, gUIContext->MousePosition))
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

typedef struct Rr_UIButtonResult Rr_UIButtonResult;
struct Rr_UIButtonResult
{
    bool Down;
    bool Up;
    bool Hovered;
    bool Held;
};

static inline Rr_UIButtonResult Rr_UIButtonBehavior(
    Rr_UILayout *Layout,
    Rr_Rect *Rect)
{
    Rr_UIWindow *Window = Layout->Window;

    Rr_UIButtonResult Result = { 0 };

    if (!Layout->LeftMouseButtonInsideRect)
    {
        return Result;
    }

    bool WindowHovered = Window == gUIContext->HoveredWindow;
    bool BlockedByDragOp = (gUIContext->DragOpWindow != NULL &&
                            gUIContext->DragOpBeganThisFrame == false) ||
                           gUIContext->DragOpEndedThisFrame == true;

    if (!BlockedByDragOp && WindowHovered &&
        Rr_RectContains(Rect, gUIContext->MousePosition))
    {
        if (gUIContext->LeftMouseButtonDown)
        {
            Rr_UIEndDragOp();
            gUIContext->DragOpWindow = NULL;
        }

        Result.Down = gUIContext->LeftMouseButtonDown;
        Result.Up = gUIContext->LeftMouseButtonUp;
        /* NOTE: Not actual click-and-held behavior! */
        Result.Held = gUIContext->LeftMouseButtonHeld;
        Result.Hovered = true;
    }

    return Result;
}

typedef struct Rr_UIDragResult Rr_UIDragResult;
struct Rr_UIDragResult
{
    bool Moved;
    bool Held;
    bool Hovered;
    bool Began;
};

static inline Rr_UIDragResult Rr_UIDragBehavior(
    Rr_UILayout *Layout,
    Rr_Rect *Rect,
    Rr_UIDragOp DragOp,
    Rr_UIHash Hash,
    Rr_Vec2 Value)
{
    Rr_UIWindow *Window = Layout->Window;

    bool WindowHovered = Window == gUIContext->HoveredWindow;
    bool Contains = Rr_RectContains(Rect, gUIContext->MousePosition);

    Rr_UIDragResult Result = { 0 };

    /* NOTE: Dragging resize handle also overlaps with moving and scrolling.
     * Take that into accoutn and override current drag operation. Watch out for
     * Rr_DragBehavior() order!
     * Also, faster mouse movements may actually result in Contains == false
     * while the drag operation is still going. */

    if (Contains && gUIContext->LeftMouseButtonDown &&
        (gUIContext->DragOpWindow == NULL ||
         gUIContext->DragOpWindow == Window) &&
        WindowHovered)
    {
        if (!Layout->LeftMouseButtonInsideRect)
        {
            return Result;
        }

        Result.Hovered = true;

        Rr_UIBeginDragOp(Window, DragOp, Hash, Value);

        if (DragOp == RR_UI_DRAG_OP_WIDGET)
        {
            Rr_UISetFocus(Window, Hash);
        }
        else
        {
            Rr_UISetFocus(NULL, 0);
        }

        Result.Began = true;

        return Result;
    }

    Result.Hovered = Contains && gUIContext->HoveredWindow == Window &&
                     Layout->LeftMouseButtonInsideRect;

    if (gUIContext->DragOpWindow == Window && gUIContext->DragOp == DragOp &&
        gUIContext->DragOpHash == Hash)
    {
        if (gUIContext->LeftMouseButtonHeld)
        {
            Result.Hovered = true;
            Result.Moved = gUIContext->MouseMoved;
            Result.Held = true;
        }
        else
        {
            Rr_UIEndDragOp();
        }
    }

    return Result;
}

static inline Rr_Rect Rr_UIRectIntersection(Rr_Rect *RectA, Rr_Rect *RectB)
{
    Rr_Rect Result;
    Result.Offset.X = RR_MAX(RectA->Offset.X, RectB->Offset.X);
    Result.Offset.Y = RR_MAX(RectA->Offset.Y, RectB->Offset.Y);
    Rr_Vec2 BottomRightA = Rr_AddV2(RectA->Offset, RectA->Extent);
    Rr_Vec2 BottomRightB = Rr_AddV2(RectB->Offset, RectB->Extent);
    Rr_Vec2 Delta = Rr_V2(
        RR_MIN(BottomRightA.X, BottomRightB.X),
        RR_MIN(BottomRightA.Y, BottomRightB.Y));
    Result.Extent = Rr_SubV2(Delta, Result.Offset);
    return Result;
}

static inline void Rr_UIPushClipRectBounds(Rr_Rect *Rect)
{
    *RR_PUSH_INTO_ARRAY(&gUIContext->ClipRectBoundsStack, gUIContext->Arena) =
        *Rect;
}

static inline void Rr_UIPopClipRectBounds(void)
{
    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->ClipRectBoundsStack));
}

static inline Rr_Rect *Rr_UIClipRectBounds(void)
{
    return gUIContext->ClipRectBoundsStack.Data +
           (gUIContext->ClipRectBoundsStack.Count - 1);
}

static inline void Rr_UIBeginClipRect(Rr_Rect *Rect)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;
    Rr_Rect *ClipRectBounds = Rr_UIClipRectBounds();

    Rr_UIClipRect *ClipRect =
        RR_PUSH_INTO_ARRAY(Window->TopLevelClipRects, gUIContext->FrameArena);
    ClipRect->FirstIndex = (uint32_t)gUIContext->Indices.Count;
    ClipRect->Rect = Rr_UIRectIntersection(Rect, ClipRectBounds);

    Window->CurrentClipRect = ClipRect;
    Layout->LeftMouseButtonInsideRect =
        Rr_RectContains(&ClipRect->Rect, gUIContext->MousePosition);
}

static inline void Rr_UIEndClipRect(void)
{
    Rr_UIWindow *Window = Rr_UICurrentWindow();
    if (Window)
    {
        if (Window->TopLevelClipRects->Count > 0)
        {
            Rr_UIClipRect *Last =
                &Window->TopLevelClipRects
                     ->Data[Window->TopLevelClipRects->Count - 1];
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

static inline bool Rr_UIWindowHasTitle(Rr_UIWindow *Window)
{
    return !RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT);
}

static inline bool Rr_UIWindowHasCloseButton(Rr_UIWindow *Window)
{
    return RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_CLOSE_BIT);
}

static inline bool Rr_UIWindowHasCollapseButton(Rr_UIWindow *Window)
{
    return !RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_COLLAPSE_BIT);
}

static inline bool Rr_UIWindowHasResizeHandle(Rr_UIWindow *Window)
{
    return !RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT);
}

static inline bool Rr_UIWindowAutoResize(Rr_UIWindow *Window)
{
    return RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
}

static inline bool Rr_UIWindowNoMove(Rr_UIWindow *Window)
{
    return RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_MOVE_BIT);
}

static inline bool Rr_UIWindowNoBorder(Rr_UIWindow *Window)
{
    return RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_BORDER_BIT);
}

static inline bool Rr_UIWindowNoVerticalScrollbar(Rr_UIWindow *Window)
{
    return RR_HAS_BIT(
        Window->Flags,
        RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT);
}

static inline void Rr_UIAdvance(Rr_Vec2 Size)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    /* TODO: Can contents be updated later when ending the window? */

    if (Rr_UIIsHorizontal())
    {
        Layout->Cursor.X += Size.Width + Layout->ContentsPadding.Width;
        Layout->HorizontalMaxHeight =
            RR_MAX(Layout->HorizontalMaxHeight, Size.Height);

        Window->ContentsEnd.Y = RR_MAX(
            Window->ContentsEnd.Y,
            Layout->Cursor.Y + Layout->HorizontalMaxHeight);
        Window->ContentsEnd.X = RR_MAX(Window->ContentsEnd.X, Layout->Cursor.X);
    }
    else
    {
        Layout->Cursor.Y += Size.Height + Layout->ContentsPadding.Height;

        Window->ContentsEnd.Y = RR_MAX(Window->ContentsEnd.Y, Layout->Cursor.Y);
        /* NOTE: Only used to undo horizontal padding after ending horizontal
         * section. */
        if (Size.X < 0.0f)
        {
            Window->ContentsEnd.X += Size.X;
        }
        else
        {
            Window->ContentsEnd.X =
                RR_MAX(Window->ContentsEnd.X, Layout->Cursor.X + Size.X);
        }
    }
}

static inline void Rr_UIAddCollapseButton(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

    /* Assuming having a title bar. */

    Rr_Rect TitleRect = Window->Rect;

    Rr_Rect ButtonRect;
    ButtonRect.Offset = Window->Rect.Offset;
    ButtonRect.Extent = Rr_V2F(gUIContext->TitleButtonSize);

    Rr_UIButtonResult Result = Rr_UIButtonBehavior(Layout, &ButtonRect);
    if (Result.Up)
    {
        Window->Collapsed = !Window->Collapsed;
    }

    Rr_UIDrawBevel(
        &ButtonRect,
        &gUIContext->Style.TitleCollapseButtonBackground,
        Result.Held);

    Rr_Vec2 TriangleCenter = Rr_AddV2(
        ButtonRect.Offset,
        Rr_MulV2F(Rr_V2F(gUIContext->TitleHeight), 0.5f));
    float TriangleSize = gUIContext->TitleHeight * 0.3f;
    Rr_UIDrawFitTriangleFilled(
        TriangleCenter,
        TriangleSize,
        !Window->Collapsed ? RR_ANGLE_DEG(90.0f) : 0.0f,
        gUIContext->Style.Foreground);
}

static inline void Rr_UIAddCloseButton(Rr_UILayout *Layout, bool *Open)
{
    Rr_UIWindow *Window = Layout->Window;

    /* Assuming having a title bar. */

    float Width = gUIContext->TitleButtonSize * 0.7f;
    float Thickness = gUIContext->TitleButtonSize * 0.125f;
    Rr_Rect TitleRect = Window->Rect;
    TitleRect.Extent.Height = gUIContext->TitleHeight;
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

    Rr_Rect ButtonRect;
    ButtonRect.Offset.X =
        TitleRect.Offset.X + TitleRect.Extent.Width -
        (TitleRect.Extent.Height + gUIContext->TitleButtonSize) * 0.5f,
    ButtonRect.Offset.Y =
        TitleRect.Offset.Y +
        (TitleRect.Extent.Height - gUIContext->TitleButtonSize) * 0.5f;
    ButtonRect.Extent = Rr_V2F(gUIContext->TitleButtonSize);

    Rr_UIButtonResult Result = Rr_UIButtonBehavior(Layout, &ButtonRect);
    if (Result.Up && Open)
    {
        *Open = false;
    }

    Rr_UIDrawBevel(
        &ButtonRect,
        &gUIContext->Style.TitleCloseButtonBackground,
        Result.Held);

    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(45.0f),
        &gUIContext->Style.Foreground);
    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(-45.0f),
        &gUIContext->Style.Foreground);
}

static inline float Rr_UIAddWindowTitle(Rr_UILayout *Layout, bool *Open)
{
    Rr_UIWindow *Window = Layout->Window;
    Rr_UIPrimitive BevelPrimitive = Rr_UIReserveBevel();

    Rr_Rect TitleRect = {
        Window->Rect.Offset,
        Rr_V2(Window->Rect.Extent.Width, gUIContext->TitleHeight),
    };
    Rr_Vec2 TitlePosition =
        Rr_AddV2(Window->Rect.Offset, gUIContext->TitlePadding);

    bool HasCollapse = Rr_UIWindowHasCollapseButton(Window);
    if (HasCollapse)
    {
        Rr_UIAddCollapseButton(Layout);

        TitlePosition.X += gUIContext->TitleHeight;
        TitleRect.Offset.X += gUIContext->TitleHeight;
        TitleRect.Extent.Width -= gUIContext->TitleHeight;
    }

    Rr_Vec2 TitleSize = Rr_UIDrawText(
        false,
        TitlePosition,
        SIZE_MAX,
        Window->Title,
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    bool HasClose = Rr_UIWindowHasCloseButton(Window);
    if (HasClose)
    {
        Rr_UIAddCloseButton(Layout, Open);

        TitleRect.Extent.Width -= gUIContext->TitleButtonSize;
    }

    Rr_Vec4 ColorB = gUIContext->Style.TitleBackground;
    ColorB.RGB = Rr_LerpV3(ColorB.RGB, 0.25f, (Rr_Vec3){ 0.0f, 0.0f, 0.0f });
    Rr_Vec4 Colors[4] = { ColorB,
                          gUIContext->Style.TitleBackground,
                          ColorB,
                          gUIContext->Style.TitleBackground };
    Rr_UIBevelEx(BevelPrimitive, &TitleRect, Colors, false);

    return TitleSize.Width + gUIContext->TitlePadding.Width * 2 +
           (HasClose ? gUIContext->TitleButtonSize : 0) +
           (HasCollapse ? gUIContext->TitleButtonSize : 0);
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

    Rr_UIDragResult Result = Rr_UIDragBehavior(
        Layout,
        &ResizeHandleRect,
        RR_UI_DRAG_OP_RESIZE,
        0,
        Window->Rect.Extent);

    if (Result.Moved)
    {
        Rr_Vec2 Delta =
            Rr_SubV2(gUIContext->MousePosition, gUIContext->DragOpMouseStart);
        Rr_Vec2 NewWindowSize = Rr_AddV2(gUIContext->DragOpWindowStart, Delta);
        Rr_Vec2 MinWindowSize = Rr_UIGetMinWindowSize(Window->Flags);
        NewWindowSize.X = RR_MAX(NewWindowSize.X, MinWindowSize.X);
        NewWindowSize.Y = RR_MAX(NewWindowSize.Y, MinWindowSize.Y);
        Layout->DeferredWindowExtent = Rr_FloorV2(NewWindowSize);
    }

    Layout->DeferredResizeHandleColor = gUIContext->Style.Foreground;
    if (Result.Hovered || Result.Moved)
    {
        Layout->DeferredResizeHandleColor =
            Rr_MulV4F(Layout->DeferredResizeHandleColor, 0.75f);
    }

    return Result.Moved;
}

static inline Rr_Rect Rr_UIGetWindowContentsArea(
    Rr_UILayout *Layout,
    float *OutFillRatio)
{
    Rr_UIWindow *Window = Layout->Window;

    Rr_Rect Rect = Window->Rect;

    if (Rr_UIWindowHasTitle(Window))
    {
        Rect.Offset.Y += gUIContext->TitleHeight;
        Rect.Extent.Height -= gUIContext->TitleHeight;
    }

    if (!Layout->DeferredAutoResize)
    {
        float ContentsHeight = Window->ContentsEnd.Y - Window->ContentsStart.Y;
        ContentsHeight += gUIContext->ContentsPadding.Height;
        if (ContentsHeight == 0.0f)
        {
            return Rect;
        }
        float FillRatio = ContentsHeight / Rect.Extent.Height;
        if (OutFillRatio)
        {
            *OutFillRatio = FillRatio;
        }
        if (!Rr_UIWindowNoVerticalScrollbar(Window) && FillRatio > 1.0f)
        {
            Rect.Extent.Width -= gUIContext->ScrollbarWidth;
        }
    }

    return Rect;
}

static inline bool Rr_UIAddVerticalScrollbar(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

    bool HasResize = Rr_UIWindowHasResizeHandle(Window);

    Rr_Rect ContentsAreaRect = Rr_UIGetWindowContentsArea(Layout, NULL);
    float ContentsHeight = Window->ContentsEnd.Y - Window->ContentsStart.Y;
    if (ContentsHeight == 0.0f)
    {
        return false;
    }
    ContentsHeight += Layout->ContentsPadding.Height;
    float FillRatio = ContentsAreaRect.Extent.Height / ContentsHeight;

    float MaxYScroll =
        RR_MAX(0.0f, ContentsHeight - ContentsAreaRect.Extent.Height);
    Window->VScroll = RR_CLAMP(0.0f, Window->VScroll, MaxYScroll);

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

        float ScrollbarHandleHeightUnpadded = ScrollbarHandleSize.Height;

        ScrollbarHandlePosition.Y += Window->VScroll * FillRatio;

        /* Vertical margins. */

        ScrollbarHandlePosition.Y += ScrollbarHandleOffset;
        ScrollbarHandleSize.Height -= ScrollbarHandleOffset * 2.0f;
        ScrollbarHandleSize.Height = RR_MAX(
            ScrollbarHandleSize.Height,
            gUIContext->BevelThickness * 3.0f);

        /* This cuts a bix of height from the scrollbar hitbox so the resize
         * handle is always on top. */

        Rr_Rect ClickableRect = {
            ScrollbarPosition,
            ScrollbarSize,
        };
        if (HasResize)
        {
            float ResizeHandleY = ScrollbarPosition.Y + ScrollbarSize.Y -
                                  gUIContext->ResizeHandleSize;
            if (gUIContext->MousePosition.Y >= ResizeHandleY)
            {
                ClickableRect.Extent = Rr_V2F(-1.0f);
            }
        }

        Rr_UIDragResult Result = Rr_UIDragBehavior(
            Layout,
            &ClickableRect,
            RR_UI_DRAG_OP_SCROLL,
            0,
            (Rr_Vec2){ 0.0f, Window->VScroll });

        if (Result.Began)
        {
            /* Handle clicking outside of the handle. */

            if (gUIContext->MousePosition.Y >
                ScrollbarHandlePosition.Y + ScrollbarHandleSize.Y)
            {
                Window->VScroll =
                    (gUIContext->MousePosition.Y - ScrollbarPosition.Y -
                     ScrollbarHandleOffset * 2.0f) /
                        (ScrollbarSize.Y / ContentsHeight) -
                    (ScrollbarHandleSize.Height / FillRatio);
                gUIContext->DragOpWindowStart.Y = Window->VScroll;
            }
            else if (gUIContext->MousePosition.Y < ScrollbarHandlePosition.Y)
            {
                Window->VScroll =
                    (gUIContext->MousePosition.Y - ScrollbarPosition.Y -
                     ScrollbarHandleOffset * 2.0f) /
                    ((ScrollbarSize.Y) / ContentsHeight);
                gUIContext->DragOpWindowStart.Y = Window->VScroll;
            }
        }

        if (Result.Moved)
        {
            float Delta =
                gUIContext->MousePosition.Y - gUIContext->DragOpMouseStart.Y;
            float ContentsHeight =
                Window->ContentsEnd.Y - Window->ContentsStart.Y;
            float FillRatio = ContentsHeight / ContentsAreaRect.Extent.Height;
            Window->VScroll =
                gUIContext->DragOpWindowStart.Y + (Delta * FillRatio);
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

void Rr_UISetNextWindowOpenPosition(Rr_Vec2 Position)
{
    gUIContext->NextWindowOpenPosition = Position;
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

static inline void Rr_UIConsumeNextWindowOpenPosition(Rr_UIWindow *Window)
{
    if (gUIContext->NextWindowOpenPosition.X != INFINITY &&
        gUIContext->NextWindowOpenPosition.Y != INFINITY)
    {
        Window->Rect.Offset = Rr_FloorV2(gUIContext->NextWindowOpenPosition);
        gUIContext->NextWindowOpenPosition = Rr_V2F(INFINITY);
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

static inline Rr_Vec2 Rr_UIConsumeNextWindowPadding(void)
{
    Rr_Vec2 ContentsPadding;
    if (gUIContext->NextWindowPadding.Width != INFINITY &&
        gUIContext->NextWindowPadding.Height != INFINITY)
    {
        ContentsPadding = gUIContext->NextWindowPadding;
        gUIContext->NextWindowPadding = Rr_V2F(INFINITY);
    }
    else
    {
        ContentsPadding = gUIContext->ContentsPadding;
    }
    return ContentsPadding;
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
        Window->TopLevelParent->Z = gUIContext->HighestWindow->Z + 1;
    }
}

static inline bool Rr_UIBeginWindowEx(
    const char *Title,
    Rr_UIWindow *Window,
    bool *Open)
{
    Rr_UIConsumeNextWindowPosition(Window);
    Rr_UIConsumeNextWindowSize(Window);
    Rr_Vec2 ContentsPadding = Rr_UIConsumeNextWindowPadding();

    /* Return if closed.
     * Also handle show after being closed.
     * This will put window on top unless there is a flag
     * preventing that which is not currently implemented. */

    bool NoBorder = Rr_UIWindowNoBorder(Window);

    bool WasClosed = Window->Open == false;
    Window->Open = (Open == NULL || *Open == true);
    if (!Window->Open)
    {
        return false;
    }
    if (WasClosed)
    {
        Rr_UIConsumeNextWindowOpenPosition(Window);

        Window->OpenedThisFrame = true;
        Window->SkipThisFrame = true;
        if (gUIContext->HighestWindow)
        {
            Rr_UIPutWindowOnTop(Window);
        }
    }

    /* NOTE: Have to access current window and finish its clip rect. */

    Rr_UIEndClipRect();

    *RR_PUSH_INTO_ARRAY(&gUIContext->ActiveWindows, gUIContext->Arena) = Window;
    Window->Added = true;

    Rr_UILayout *ParentLayout = Rr_UICurrentLayout();
    Rr_UIWindow *ParentWindow = Rr_UICurrentWindow();

    if (Window->Child)
    {
        Window->TopLevelClipRects = ParentWindow->TopLevelClipRects;
    }
    else
    {
        Window->TopLevelClipRects =
            RR_ALLOC_TYPE(gUIContext->FrameArena, Rr_UIClipRectArray);
    }

    Rr_UILayout *Layout =
        RR_PUSH_INTO_ARRAY(&gUIContext->Stack, gUIContext->FrameArena);
    *Layout = (Rr_UILayout){
        .Window = Window,
        .HorizontalX = INFINITY,
        .DeferredWindowOffset = Rr_V2F(INFINITY),
        .DeferredWindowExtent = Rr_V2F(INFINITY),
        .DeferredAutoResize = Rr_UIWindowAutoResize(Window),
        .ContentsPadding = ContentsPadding,
        .TopLevelLayout = Window->Child ? ParentLayout->TopLevelLayout : Layout,
        .Cursor = Window->Rect.Offset,
        .AvailableContentsWidth = Window->Rect.Extent.Width,
    };

    /* BUG: Broken at the moment! */
    if (Window->Child)
    {
        /* if (!ParentLayout->DeferredAutoResize) */
        {
            Window->Rect.Extent.Width = ParentLayout->AvailableContentsWidth;
        }
    }

    bool WasCollapsed = Window->Collapsed;

    /* Calculate total and visible extents. */

    Rr_Rect TotalClipRect = Window->Rect;
    if (WasCollapsed)
    {
        TotalClipRect.Extent.Height = gUIContext->TitleHeight;
    }
    Rr_Rect TotalClipRectWithBorder =
        Rr_ResizeRect(&TotalClipRect, gUIContext->FrameThickness);

    if (Window->Child)
    {
        Window->VisibleRect = Rr_UIRectIntersection(
            &TotalClipRectWithBorder,
            Rr_UIClipRectBounds());
    }
    else
    {
        Window->VisibleRect = TotalClipRectWithBorder;
    }

    Rr_UIPushClipRectBounds(&Window->VisibleRect);

    /* Clip to the whole area. */

    Rr_UIBeginClipRect(&TotalClipRectWithBorder);

    /* Move and resize behavior.
     * Changes to window rect are deferred to Rr_UIEndWindow()!
     * Rr_UIDragBehavior() gets called even if the window is
     * non-movable because this function resets widget focus. */

    /* NOTE: Forward move behavior to the top-level parent. */

    Rr_UIDragResult DragResult = Rr_UIDragBehavior(
        Layout,
        &TotalClipRect,
        RR_UI_DRAG_OP_MOVE,
        0,
        Window->TopLevelParent->Rect.Offset);
    bool Moved = DragResult.Moved || DragResult.Began;
    if (!Rr_UIWindowNoMove(Window->TopLevelParent) && Moved)
    {
        Rr_Vec2 Delta =
            Rr_SubV2(gUIContext->MousePosition, gUIContext->DragOpMouseStart);
        Layout->TopLevelLayout->DeferredWindowOffset =
            Rr_FloorV2(Rr_AddV2(gUIContext->DragOpWindowStart, Delta));
    }

    /* Add window title if necessary. */

    bool HasTitle = Rr_UIWindowHasTitle(Window);
    float DesiredTitleWidth = 0;
    if (HasTitle)
    {
        DesiredTitleWidth = Rr_UIAddWindowTitle(Layout, Open);

        Layout->Cursor.Y += gUIContext->TitleHeight;
    }

    /* Adding title might have changed collapsed state. */

    if (Window->Collapsed != WasCollapsed)
    {
        Rr_UIEndClipRect();

        TotalClipRect = Window->Rect;
        if (Window->Collapsed)
        {
            TotalClipRect.Extent.Height = gUIContext->TitleHeight;
        }
        TotalClipRectWithBorder =
            Rr_ResizeRect(&TotalClipRect, gUIContext->FrameThickness);
        Rr_UIBeginClipRect(&TotalClipRectWithBorder);
        Window->VisibleRect = Window->CurrentClipRect->Rect;
    }

    /* Add border if necessary. */

    if (!Rr_UIWindowNoBorder(Window))
    {
        Rr_UIDrawOuterFrame(
            &TotalClipRect,
            gUIContext->FrameThickness,
            &gUIContext->Style.Outline);
    }

    /* Now that title is added, try to return early in case the window is
     * collapsed. */

    if (Window->Collapsed)
    {
        if (Window->Child)
        {
            Rr_UIEndChild();
        }
        else
        {
            Rr_UIEndWindow();
        }

        return false;
    }

    /* Add vertical scrollbar if necessary. */

    bool VerticalScrollbarAdded = false;
    if (Layout->DeferredAutoResize || Rr_UIWindowNoVerticalScrollbar(Window))
    {
        Window->VScroll = 0;
    }
    else
    {
        VerticalScrollbarAdded = Rr_UIAddVerticalScrollbar(Layout);
        if (VerticalScrollbarAdded ||
            (Layout->DeferredAutoResize && Window->SkipThisFrame))
        {
            Layout->AvailableContentsWidth -= gUIContext->ScrollbarWidth;
        }
        Window->VScroll = roundf(Window->VScroll);
        Layout->Cursor.Y -= Window->VScroll;
    }

    /* NOTE: Defer drawing the handle to Rr_UIEndWindow()! */

    if (!Layout->DeferredAutoResize)
    {
        /* TODO: Support resizing child windows. */

        Rr_UIAddResizeHandle(Layout);
    }

    Layout->AvailableContentsWidth -= Layout->ContentsPadding.X * 2.0f;
    Layout->Cursor = Rr_AddV2(Layout->Cursor, Layout->ContentsPadding);

    Rr_UIEndClipRect();

    /* Clip to contents. */

    Rr_Rect ContentsAreaRect = Rr_UIGetWindowContentsArea(Layout, NULL);
    Rr_Rect VisibleContentsAreaRect =
        Rr_UIRectIntersection(&ContentsAreaRect, &Window->VisibleRect);
    Rr_UIPushClipRectBounds(&VisibleContentsAreaRect);
    Rr_UIBeginClipRect(&VisibleContentsAreaRect);

    Rr_UIDrawSolidQuad(&ContentsAreaRect, &gUIContext->Style.Background);

    Window->ContentsStart = Window->ContentsEnd = Layout->Cursor;

    /* Consider title length when in auto size mode. */

    if (HasTitle && Layout->DeferredAutoResize)
    {
        float TitleWidth =
            DesiredTitleWidth - Layout->ContentsPadding.Width * 2;
        Window->ContentsEnd.Width += TitleWidth;
        Layout->AvailableContentsWidth =
            RR_MAX(Layout->AvailableContentsWidth, TitleWidth);
    }

    return true;
}

static inline void Rr_UIBeginPopupWindow(Rr_UIWindowFlags Flags)
{
    Rr_UIWindow *Window = &gUIContext->PopupWindow;
    Window->Flags = Flags;
    Window->TopLevelParent = Window;
    Rr_UIBeginWindowEx("", Window, NULL);
}

static inline void Rr_UIClosePopupWindow(void)
{
    assert(gUIContext->PopupWindowParent != NULL);
    gUIContext->PopupWindowParent = NULL;
    gUIContext->PopupWindowHash = 0;
    gUIContext->PopupWindow.Open = false;
}

static bool Rr_UIPopupWindowActive(void)
{
    return gUIContext->PopupWindowParent;
}

static inline Rr_UIWindow *Rr_UICreateWindow(
    size_t TitleLength,
    const char *Title,
    uint64_t TitleHash,
    Rr_UIWindowFlags Flags)
{
    Rr_UIWindow *Window = RR_ALLOC_TYPE(gUIContext->Arena, Rr_UIWindow);
    Window->Title = memcpy(
        RR_ALLOC(gUIContext->Arena, TitleLength + 1),
        Title,
        TitleLength + 1);
    Window->Hash = TitleHash;
    Window->Flags = Flags;
    Window->Rect.Extent = Rr_UIGetMinWindowSize(Flags);

    Window->SkipThisFrame = true;
    Window->Flags |= RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;

    return Window;
}

bool Rr_UIBeginWindow(const char *Title, bool *Open, Rr_UIWindowFlags Flags)
{
    Rr_UIAssertNoWindow();

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_UIWindow **WindowRef =
        RR_GET_MAP_VALUE(&gUIContext->WindowMap, TitleHash, gUIContext->Arena);
    Rr_UIWindow *Window = *WindowRef;

    if (Window == NULL)
    {
        Window = Rr_UICreateWindow(TitleLength, Title, TitleHash, Flags);
        Window->Z = gUIContext->TotalWindowCount++;
        Window->Rect.Offset = Rr_FloorV2(Rr_V2F(gUIContext->FontSize));
        *WindowRef = Window;
    }
    else
    {
        Window->Flags = Flags;
    }

    Window->TopLevelParent = Window;

    assert(
        Window->Added == false && "There already is a window with this title!");
    return Rr_UIBeginWindowEx(Title, Window, Open);
}

void Rr_UIEndWindow(void)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    if (!Window->Collapsed)
    {
        Rr_UIEndClipRect();

        Rr_UIPopClipRectBounds();

        /* Begin overlay clip rect for stuff such as resize handle and scroll
         * area darkeners. */

        Rr_UIBeginClipRect(&Window->Rect);

        /* NOTE: Flooring these fixed imprecise FillRatio calculation.
         * If the bug ever returns it probably means the fix should be applied
         * somewhere else. */

        Window->ContentsStart = Rr_FloorV2(Window->ContentsStart);
        Window->ContentsEnd = Rr_FloorV2(Window->ContentsEnd);

        float ContentsHeight = Window->ContentsEnd.Y - Window->ContentsStart.Y;

        float FillRatio = 0.0f;
        Rr_Rect CurrentRect = Rr_UIGetWindowContentsArea(Layout, &FillRatio);

        if (FillRatio > 1.0f)
        {
            float DarkenSize =
                RR_MIN(gUIContext->FontSize / 2.0f, ContentsHeight);
            float DarkenColor = 0.005f;

            Rr_Rect DarkenRect = CurrentRect;
            DarkenRect.Extent.Height =
                RR_MIN(CurrentRect.Extent.Height, DarkenSize);

            float TopDarkenAlpha =
                RR_CLAMP(0.0f, Window->VScroll / DarkenSize, 1.0f);
            if (TopDarkenAlpha > 0.0f)
            {
                Rr_UIDrawVerticalGradientQuad(
                    &DarkenRect,
                    &(Rr_Vec4){ DarkenColor,
                                DarkenColor,
                                DarkenColor,
                                TopDarkenAlpha },
                    &(Rr_Vec4){ DarkenColor, DarkenColor, DarkenColor, 0.0f });
            }

            float BottomDarkenAlpha = RR_CLAMP(
                0.0f,
                (ContentsHeight - CurrentRect.Extent.Height - Window->VScroll +
                 Layout->ContentsPadding.Height) /
                    DarkenSize,
                1.0f);
            if (BottomDarkenAlpha > 0.0f)
            {
                DarkenRect.Offset.Y =
                    CurrentRect.Offset.Y + CurrentRect.Extent.Y - DarkenSize;

                Rr_UIDrawVerticalGradientQuad(
                    &DarkenRect,
                    &(Rr_Vec4){ DarkenColor, DarkenColor, DarkenColor, 0.0f },
                    &(Rr_Vec4){ DarkenColor,
                                DarkenColor,
                                DarkenColor,
                                BottomDarkenAlpha });
            }
        }

        /* Add resize handle if necessary. */

        if (Rr_UIWindowHasResizeHandle(Window))
        {
            Rr_Vec2 BottomRight =
                Rr_AddV2(Window->Rect.Offset, Window->Rect.Extent);
            Rr_Vec2 Positions[] = {
                { BottomRight.X - gUIContext->ResizeHandleSize, BottomRight.Y },
                { BottomRight.X, BottomRight.Y - gUIContext->ResizeHandleSize },
                { BottomRight.X, BottomRight.Y },
            };
            Rr_UIDrawSolidTriangle(
                Positions,
                &Layout->DeferredResizeHandleColor);
        }
    }

    Rr_UIEndClipRect();

    Rr_UIPopClipRectBounds();

    /* NOTE: Forward scroll wheel behavior to the top-level parent. */

    if (!Window->Collapsed || Window->Child)
    {
        Rr_UIScrollBehavior(
            Window,
            &Window->VisibleRect,
            &Window->TopLevelParent->VScroll);
    }

    /* Apply deferred window offset. */

    if (Layout->DeferredWindowOffset.X != INFINITY)
    {
        Window->TopLevelParent->Rect.Offset = Layout->DeferredWindowOffset;
    }

    /* Calculate window extent if necessary or apply manual resize. */

    if (Layout->DeferredAutoResize)
    {
        Window->Rect.Extent =
            Rr_SubV2(Window->ContentsEnd, Window->ContentsStart);
        Window->Rect.Extent.Width += Layout->ContentsPadding.Width * 2.0f;
        Window->Rect.Extent.Height += Layout->ContentsPadding.Height;

        if (Rr_UIWindowHasTitle(Window))
        {
            Window->Rect.Extent.Y += gUIContext->TitleHeight;
        }
    }
    else if (Layout->DeferredWindowExtent.X != INFINITY)
    {
        Window->Rect.Extent = Layout->DeferredWindowExtent;
    }

    /* Pop current layout from the stack. */

    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->Stack));

    Rr_UILayout *ParentLayout = Rr_UICurrentLayout();
    if (ParentLayout)
    {
        Rr_UIWindow *ParentWindow = ParentLayout->Window;

        if (Window->Child)
        {
            Rr_Vec2 WindowExtent = Window->Rect.Extent;
            if (Window->Collapsed)
            {
                WindowExtent.Height = gUIContext->TitleHeight;
            }
            Rr_UIAdvance(WindowExtent);
        }

        /* Resume clip rect. */

        Rr_UIBeginClipRect(&ParentWindow->CurrentClipRect->Rect);
    }
}

bool Rr_UIBeginChild(const char *Title)
{
    Rr_UIAssertWindow();

    Rr_UILayout *ParentLayout = Rr_UICurrentLayout();
    Rr_UIWindow *ParentWindow = ParentLayout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_UIWindow **WindowRef = RR_GET_MAP_VALUE(
        &ParentWindow->ChildWindowMap,
        TitleHash,
        gUIContext->Arena);
    Rr_UIWindow *Window = *WindowRef;

    const Rr_UIWindowFlags CHILD_FLAGS = RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT |
                                         RR_UI_WINDOW_FLAGS_NO_MINIMIZE_BIT |
                                         RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
                                         RR_UI_WINDOW_FLAGS_NO_MOVE_BIT;
    if (Window == NULL)
    {
        Window = Rr_UICreateWindow(TitleLength, Title, TitleHash, CHILD_FLAGS);
        Window->Child = true;
        *WindowRef = Window;
    }
    else
    {
        Window->Flags = CHILD_FLAGS;
    }

    Window->Rect.Offset = ParentLayout->Cursor;
    Window->Z = ParentWindow->TopLevelParent->Z;
    Window->TopLevelParent = ParentWindow->TopLevelParent;

    assert(
        Window->Added == false && "There already is a window with this title!");
    return Rr_UIBeginWindowEx(Title, Window, NULL);
}

void Rr_UIEndChild(void)
{
    Rr_UIAssertWindow();

    Rr_UIWindow *Window = Rr_UICurrentWindow();
    assert(Window->Child);

    Rr_UIEndWindow();
}

void Rr_UIBeginHorizontal(void)
{
    assert(
        !Rr_UIIsHorizontal() && "Did you forget to call Rr_EndHorizontal()?");
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
    Layout->HorizontalX = INFINITY;
    Rr_UIAdvance(
        Rr_V2(-Layout->ContentsPadding.Width, Layout->HorizontalMaxHeight));
}

typedef struct Rr_UIFlexibleWidgetLayout Rr_UIFlexibleWidgetLayout;
struct Rr_UIFlexibleWidgetLayout
{
    Rr_Vec2 TitleSize;
    float TitleCursorOffsetX;
    float WidgetWidth;
};

static inline Rr_UIFlexibleWidgetLayout Rr_UICalculateFlexibleWidgetLayout(
    Rr_UILayout *Layout,
    size_t TitleLength,
    const char *Title)
{
    Rr_UIWindow *Window = Layout->Window;

    Rr_UIFlexibleWidgetLayout Result;

    Result.TitleSize = Rr_UICalculateTextSize(TitleLength, Title, 0.0f, 0);

    Window->MaxFlexibleWidgetTitleWidth =
        RR_MAX(Window->MaxFlexibleWidgetTitleWidth, Result.TitleSize.Width);

    Result.TitleCursorOffsetX =
        Layout->AvailableContentsWidth - Window->MaxFlexibleWidgetTitleWidth;

    Result.WidgetWidth = Layout->AvailableContentsWidth -
                         Window->MaxFlexibleWidgetTitleWidth -
                         Layout->ContentsPadding.X;

    return Result;
}

void Rr_UIBeginTabs(const char *Title)
{
    Rr_UIAssertWindow();
    assert(!Rr_UIIsHorizontal() && "Tabs can't be aligned horizontally!");

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

    Rr_Vec2 SeparatorSize = {
        Layout->AvailableContentsWidth,
        gUIContext->FrameThickness,
    };
    Rr_Vec2 SeparatorPosition = {
        Layout->Cursor.X,
        Layout->Cursor.Y + gUIContext->LineHeight,
    };
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){
            SeparatorPosition,
            SeparatorSize,
        },
        &gUIContext->Style.Foreground);

    /* TODO: Use window padding instead? */
    Rr_UIAdvance(Rr_V2(0.0f, gUIContext->LineHeight));
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

    Rr_UIPrimitive TabQuad = Rr_UIReserveQuad();

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

    Rr_UIButtonResult Result = Rr_UIButtonBehavior(
        Layout,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        });

    Rr_Vec4 *TabButtonColor;
    if (Selected)
    {
        TabButtonColor = &gUIContext->Style.Foreground;
    }
    else if (Result.Held)
    {
        TabButtonColor = &gUIContext->Style.ButtonHeld;
    }
    else if (Result.Hovered)
    {
        TabButtonColor = &gUIContext->Style.ButtonHovered;
    }
    else
    {
        TabButtonColor = &gUIContext->Style.Background;
    }

    Rr_UISolidQuad(
        TabQuad.Vertices,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        },
        TabButtonColor);

    if (Result.Up)
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

    float FoldButtonHeight = gUIContext->TitleHeight;

    float TriangleSize = FoldButtonHeight * 0.3f;
    float TriangleOffset = TriangleSize * 0.5f;
    Rr_Vec2 TriangleCenter = Rr_V2(
        Layout->Cursor.X + TriangleOffset + gUIContext->TitlePadding.Width,
        Layout->Cursor.Y + FoldButtonHeight * 0.5f);

    Rr_UIDrawFitTriangleFilled(
        TriangleCenter,
        TriangleSize,
        *FoldValue ? RR_ANGLE_DEG(90.0f) : 0.0f,
        gUIContext->Style.Foreground);

    Rr_Vec2 TitlePosition = Rr_AddV2(
        Rr_V2(
            Layout->Cursor.X + TriangleSize + gUIContext->ButtonPadding.Width,
            Layout->Cursor.Y),
        gUIContext->TitlePadding);
    Rr_UIDrawText(
        0,
        TitlePosition,
        TitleLength,
        Title,
        0,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = Rr_V2(Layout->AvailableContentsWidth, FoldButtonHeight);

    Rr_UIButtonResult Result = Rr_UIButtonBehavior(
        Layout,
        &(Rr_Rect){
            Layout->Cursor,
            TotalSize,
        });

    if (Result.Up)
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
        Result.Held);

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return *FoldValue;
}

void Rr_UISeparator(void)
{
    /* TODO: Horizontal support. */
    Rr_UIAssertWindow();
    assert(Rr_UIIsHorizontal() == false);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 Size = {
        Layout->AvailableContentsWidth,
        gUIContext->FrameThickness,
    };
    Rr_Vec2 Position = {
        Layout->Cursor.X,
        Layout->Cursor.Y + Size.Height,
    };
    Rr_UIDrawSolidQuad(
        &(Rr_Rect){ Position, Size },
        &gUIContext->Style.Outline);

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

    Rr_UIButtonResult Result = Rr_UIButtonBehavior(
        Layout,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        });

    Rr_Rect ButtonRect = {
        ButtonPosition,
        ButtonSize,
    };

    Rr_UIBevel(
        Primitive,
        &ButtonRect,
        &gUIContext->Style.ButtonNormal,
        Result.Held);

    Rr_UIAdvance(ButtonSize);

    Rr_DestroyScratch(Scratch);

    return Result.Up;
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

    Rr_UIButtonResult Result = Rr_UIButtonBehavior(
        Layout,
        &(Rr_Rect){
            FramePosition,
            CheckboxSize,
        });

    if (Result.Up)
    {
        *Checked = !*Checked;
    }

    Rr_Vec4 BackgroundColor = gUIContext->Style.Background;
    BackgroundColor.XYZ = Rr_MulV3F(BackgroundColor.XYZ, 0.9f);

    Rr_Rect CheckboxRect = {
        FramePosition,
        CheckboxSize,
    };
    Rr_UIDrawBevel(&CheckboxRect, &BackgroundColor, Result.Held || *Checked);

    if (*Checked)
    {
        /* Rr_Rect Inset = */
        /*     Rr_ResizeRect(&CheckboxRect, -CheckboxRect.Extent.Width / 3.0f);
         */
        /* Rr_UIDrawSolidQuad(&Inset, &gUIContext->Style.Foreground); */
        Rr_UIDrawCircleFilled(
            Rr_AddV2(CheckboxRect.Offset, Rr_DivV2F(CheckboxSize, 2.0f)),
            CheckboxSize.Width * 0.15f,
            &gUIContext->Style.Foreground);
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
        TitleSize.Width + gUIContext->LineHeight +
            gUIContext->ButtonPadding.Width,
        gUIContext->LineHeight,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return Result.Up;
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
    const char *PlaceholderString,
    Rr_UIInputFieldFilterFunc FilterFunc,
    Rr_UIInputFieldFlags Flags,
    float FixedWidth,
    bool DrawBackground,
    Rr_Vec2 *OutFieldExtent)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    bool UseFixedWidth = FixedWidth != INFINITY;
    bool Autocenter =
        RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_AUTOCENTER_BIT) &&
        UseFixedWidth;
    Rr_UIClipRect *RestoreClipRect = NULL;

    Rr_UIPrimitive BackgroundBevelPrimitive;
    if (DrawBackground)
    {
        BackgroundBevelPrimitive = Rr_UIReserveBevel();
    }

    if (UseFixedWidth)
    {
        RestoreClipRect = Window->CurrentClipRect;

        Rr_UIEndClipRect();

        Rr_Vec2 ClipRectExtent = { FixedWidth, RestoreClipRect->Rect.Extent.Y };

        /* BUG: DOESN'T TAKE PARENT RECT INTO ACCOUNT IN THIS CASE! */

        Rr_UIBeginClipRect(
            &(Rr_Rect){ .Offset = Offset, .Extent = ClipRectExtent });
    }

    /* Rr_UIPrimitive FieldPrimitive = Rr_UIReserveBevel(); */

    bool UsePersistentBuffer =
        RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT);

    bool Focused = Rr_UIIsFocused(Window, Hash);
    bool WasFocused = !Focused && Rr_UIWasFocused(Window, Hash);

    /* NOTE: A bit hacky way to make sure initial memcpy to persistent
     * buffer occurs only once. Defering the copy also protects from issues
     * coming from unfocusing an input field that goes after current one. */
    if (UsePersistentBuffer && Focused && gUIContext->DeferTextInputBufferCopy)
    {
        /* NOTE: May waste quite a bit of memory. */
        if (gUIContext->TextInputBuffer.Capacity < BufferCapacity ||
            !gUIContext->TextInputBuffer.Data)
        {
            gUIContext->TextInputBuffer.Data =
                RR_ALLOC_NO_ZERO(gUIContext->Arena, BufferCapacity);
            gUIContext->TextInputBuffer.Capacity = BufferCapacity;
        }
        /* NOTE: It was BufferLength + 1 before. */
        memcpy(gUIContext->TextInputBuffer.Data, Buffer, BufferCapacity);

        gUIContext->DeferTextInputBufferCopy = false;
    }

    size_t NewCursorEnd = gUIContext->TextInputCursorEnd;
    const char *BufferString = UsePersistentBuffer && (Focused || WasFocused)
                                   ? gUIContext->TextInputBuffer.Data
                                   : Buffer;
    Rr_Vec2 BufferPosition = Rr_AddV2(Offset, gUIContext->ButtonPadding);
    Rr_Vec2 BufferSize;
    if (Autocenter)
    {
        BufferSize = Rr_UICalculateTextSize(SIZE_MAX, BufferString, 0.0f, 0);
        BufferPosition.X = Offset.X + FixedWidth * 0.5f - BufferSize.X * 0.5f;
    }
    BufferSize = Rr_UIDrawInputText(
        BufferString,
        Focused,
        BufferPosition,
        gUIContext->TextInputCursorBegin,
        &NewCursorEnd,
        0.0f,
        &gUIContext->Style.Foreground);

    if (BufferSize.X == 0.0f)
    {
        if (PlaceholderString != NULL && !Focused)
        {
            if (Autocenter)
            {
                BufferSize = Rr_UICalculateTextSize(
                    SIZE_MAX,
                    PlaceholderString,
                    0.0f,
                    0);
                BufferPosition.X =
                    Offset.X + FixedWidth * 0.5f - BufferSize.X * 0.5f;
            }
            BufferSize = Rr_UIDrawText(
                false,
                BufferPosition,
                SIZE_MAX,
                PlaceholderString,
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
    if (UseFixedWidth)
    {
        FieldRect.Extent.X = FixedWidth;
    }
    if (OutFieldExtent)
    {
        *OutFieldExtent = FieldRect.Extent;
    }
    if (DrawBackground)
    {
        Rr_UIBevel(
            BackgroundBevelPrimitive,
            &FieldRect,
            &gUIContext->Style.InputFieldNormal,
            true);
    }

    Rr_UIDragResult Result = Rr_UIDragBehavior(
        Layout,
        &FieldRect,
        RR_UI_DRAG_OP_WIDGET,
        Hash,
        Rr_V2F(0.0f));

    bool Autoselect = RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_AUTOSELECT_BIT);

    if (Result.Began)
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
            uint32_t Clicks = (gUIContext->LeftMouseButtonClicks - 1) % 3;
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

        if (UsePersistentBuffer && !Focused)
        {
            gUIContext->DeferTextInputBufferCopy = true;
        }

        gUIContext->TextInputCursorMaxCol =
            Rr_UIThisLineCol(Buffer, gUIContext->TextInputCursorEnd);
        gUIContext->TextInputCursorBlinkTime = Rr_GetTimeMS();
    }
    else if (Focused && Result.Moved)
    {
        if (!Autoselect ||
            gUIContext->LeftMouseButtonClickId > gUIContext->TextInputClickId)
        {
            gUIContext->TextInputCursorBlinkTime = Rr_GetTimeMS();
            gUIContext->TextInputCursorEnd = NewCursorEnd;
        }
    }

    if (Result.Hovered)
    {
        gUIContext->MouseOverTextInput = true;
    }

    bool ChangesConfirmed = false;

    if (Focused)
    {
        bool EnterToConfirm =
            !RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT);
        ChangesConfirmed = Rr_UIEditUTF8Buffer(
            &gUIContext->TextInputCursorBegin,
            &gUIContext->TextInputCursorEnd,
            BufferCapacity,
            UsePersistentBuffer ? gUIContext->TextInputBuffer.Data : Buffer,
            FilterFunc,
            EnterToConfirm);
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

    if (UseFixedWidth)
    {
        Rr_UIEndClipRect();

        Rr_UIBeginClipRect(&RestoreClipRect->Rect);
    }

    Rr_DestroyScratch(Scratch);

    return ChangesConfirmed;
}

typedef enum
{
    RR_UI_SCALAR_FORMAT_TYPE_INT,
    RR_UI_SCALAR_FORMAT_TYPE_UINT,
    RR_UI_SCALAR_FORMAT_TYPE_FLOAT,
    RR_UI_SCALAR_FORMAT_TYPE_FLOAT1,
    RR_UI_SCALAR_FORMAT_TYPE_FLOAT2,
    RR_UI_SCALAR_FORMAT_TYPE_FLOAT3,
    RR_UI_SCALAR_FORMAT_TYPE_FLOAT4,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE1,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE2,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE3,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE4,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE5,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE6,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE7,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE8,
} Rr_UIScalarFormatType;

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

static inline bool Rr_UIUnsignedIntegerFilter(
    size_t Length,
    const char *UTF8String)
{
    for (size_t Index = 0; Index < Length; ++Index)
    {
        char Char = UTF8String[Index];
        bool InRange = Char >= '0' && Char <= '9';
        if (!InRange)
        {
            return false;
        }
    }
    return true;
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

static inline bool Rr_UIGenericInputFieldScalarMulti(
    Rr_UIHash Hash,
    Rr_Vec2 Offset,
    int Cols,
    int Rows,
    void *Data,
    Rr_UIScalarFormatType ScalarFormatType,
    Rr_UIInputFieldFlags Flags,
    float FixedWidth,
    bool DrawBackground,
    Rr_Vec2 *OutTotalExtent)
{
    assert(Cols > 0 && Cols <= 4);
    assert(Rows > 0 && Rows <= 4);

    float SingleFieldWidth = INFINITY;
    if (FixedWidth != INFINITY)
    {
        SingleFieldWidth =
            (FixedWidth - (gUIContext->ComponentMargin * (float)(Cols - 1))) /
            (float)Cols;
    }

    Rr_UIInputFieldFilterFunc FilterFunc;
    size_t ElementSize;
    const char *ScanString;
    switch (ScalarFormatType)
    {
        case RR_UI_SCALAR_FORMAT_TYPE_INT:
        {
            ElementSize = sizeof(int32_t);
            FilterFunc = Rr_UIIntegerFilter;
            ScanString = "%i";
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_UINT:
        {
            ElementSize = sizeof(uint32_t);
            FilterFunc = Rr_UIUnsignedIntegerFilter;
            ScanString = "%u";
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_FLOAT:
        case RR_UI_SCALAR_FORMAT_TYPE_FLOAT1:
        case RR_UI_SCALAR_FORMAT_TYPE_FLOAT2:
        case RR_UI_SCALAR_FORMAT_TYPE_FLOAT3:
        case RR_UI_SCALAR_FORMAT_TYPE_FLOAT4:
        {
            ElementSize = sizeof(float);
            FilterFunc = Rr_UIFloatFilter;
            ScanString = "%g";
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE:
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE1:
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE2:
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE3:
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE4:
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE5:
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE6:
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE7:
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE8:
        {
            ElementSize = sizeof(double);
            FilterFunc = Rr_UIFloatFilter;
            ScanString = "%lg";
        }
        break;
        default:
        {
            RR_ABORT("Unsupported format type!");
        }
        break;
    }

    const char *COMPONENT_TITLES[] = {
        "X0", "Y0", "Z0", "W0", //
        "X1", "Y1", "Z1", "W1", //
        "X2", "Y2", "Z2", "W2", //
        "X3", "Y3", "Z3", "W3",
    };
    Rr_Vec2 Cursor = Offset;
    float CursorXStart = Cursor.X;
    Rr_Vec2 TotalExtent = { 0 };
    char Buffer[64];
    bool Edited = false;
    for (int Row = 0; Row < Rows; ++Row)
    {
        float MaxRowHeight = 0.0f;
        for (int Col = 0; Col < Cols; ++Col)
        {
            size_t Index = (size_t)(Col * Rows + Row);
            char *ElementData = (char *)Data + Index * ElementSize;

            switch (ScalarFormatType)
            {
                case RR_UI_SCALAR_FORMAT_TYPE_INT:
                {
                    snprintf(Buffer, sizeof(Buffer), "%d", *(int *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_UINT:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%u",
                        *(unsigned int *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_FLOAT:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%f",
                        *(float *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_FLOAT1:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.1f",
                        *(float *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_FLOAT2:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.2f",
                        *(float *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_FLOAT3:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.3f",
                        *(float *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_FLOAT4:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.4f",
                        *(float *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%f",
                        *(double *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE1:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.1f",
                        *(double *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE2:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.2f",
                        *(double *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE3:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.3f",
                        *(double *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE4:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.4f",
                        *(double *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE5:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.5f",
                        *(double *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE6:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.6f",
                        *(double *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE7:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.7f",
                        *(double *)ElementData);
                }
                break;
                case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE8:
                {
                    snprintf(
                        Buffer,
                        sizeof(Buffer),
                        "%.8f",
                        *(double *)ElementData);
                }
                break;
                default:
                {
                    RR_ABORT("Unsupported format type!");
                }
                break;
            }

            Rr_UIHash ComponentHash =
                Rr_UIGetHash(COMPONENT_TITLES[Index], 2, Hash);

            Rr_Vec2 FieldExtent;
            if (Rr_UIGenericInputField(
                    ComponentHash,
                    Cursor,
                    sizeof(Buffer),
                    Buffer,
                    NULL,
                    FilterFunc,
                    Flags,
                    SingleFieldWidth,
                    true,
                    &FieldExtent))
            {
                Edited = true;
                sscanf(Buffer, ScanString, (void *)ElementData);
            }

            MaxRowHeight = RR_MAX(MaxRowHeight, FieldExtent.Height);
            Cursor.X += FieldExtent.Width + gUIContext->ComponentMargin;
        }
        TotalExtent.Width = RR_MAX(
            TotalExtent.Width,
            Cursor.X - CursorXStart - gUIContext->ComponentMargin);
        TotalExtent.Height += MaxRowHeight + gUIContext->ComponentMargin;
        Cursor.X = CursorXStart;
        Cursor.Y += MaxRowHeight + gUIContext->ComponentMargin;
    }

    /* Undo last vertical margin */
    TotalExtent.Height -= gUIContext->ComponentMargin;

    if (OutTotalExtent)
    {
        *OutTotalExtent = TotalExtent;
    }

    return Edited;
}

static inline bool Rr_UIInputScalarMulti(
    const char *Title,
    void *Data,
    int Cols,
    int Rows,
    Rr_UIScalarFormatType ScalarFormatType)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_UIFlexibleWidgetLayout FlexibleWidgetLayout =
        Rr_UICalculateFlexibleWidgetLayout(Layout, TitleLength, Title);

    Rr_Vec2 TotalExtent;
    bool Edited = Rr_UIGenericInputFieldScalarMulti(
        TitleHash,
        Layout->Cursor,
        Cols,
        Rows,
        Data,
        ScalarFormatType,
        RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTOSELECT_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTOCENTER_BIT,
        FlexibleWidgetLayout.WidgetWidth,
        true,
        &TotalExtent);

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += FlexibleWidgetLayout.TitleCursorOffsetX;
    TitleOffset.Y += gUIContext->ButtonPadding.Y;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    Rr_UIAdvance(Rr_V2(Layout->AvailableContentsWidth, TotalExtent.Height));

    return Edited;
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

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_UIFlexibleWidgetLayout FlexibleWidgetLayout =
        Rr_UICalculateFlexibleWidgetLayout(Layout, TitleLength, Title);

    Rr_Vec2 FieldExtent;
    bool ChangesConfirmed = Rr_UIGenericInputField(
        TitleHash,
        Layout->Cursor,
        BufferCapacity,
        Buffer,
        Placeholder,
        FilterFunc,
        Flags,
        FlexibleWidgetLayout.WidgetWidth,
        true,
        &FieldExtent);

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += FlexibleWidgetLayout.TitleCursorOffsetX;
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
        Layout->AvailableContentsWidth,
        FieldExtent.Height,
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

bool Rr_UIInputFloat(const char *Title, float *Value)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        1,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT2);
}

bool Rr_UIInputFloat2(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        2,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT2);
}

bool Rr_UIInputFloat3(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        3,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT2);
}

bool Rr_UIInputFloat4(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        4,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT2);
}

bool Rr_UIInputFloat2x2(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        2,
        2,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT2);
}

bool Rr_UIInputFloat3x3(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        3,
        3,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT2);
}

bool Rr_UIInputFloat4x4(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        4,
        4,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT2);
}

bool Rr_UIInputInt(const char *Title, int32_t *Value)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        1,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_INT);
}

bool Rr_UIInputInt2(const char *Title, int32_t *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        2,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_INT);
}

bool Rr_UIInputInt3(const char *Title, int32_t *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        3,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_INT);
}

bool Rr_UIInputInt4(const char *Title, int32_t *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        4,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_INT);
}

bool Rr_UIInputUnsignedInt(const char *Title, uint32_t *Value)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        1,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_UINT);
}

static inline void Rr_UIRGBAToHexString(
    char *Buffer,
    int ChannelCount,
    float *Channels)
{
    for (int Index = 0; Index < ChannelCount; ++Index)
    {
        float FloatValue = Channels[Index];
        uint8_t Value = (uint8_t)(RR_CLAMP(0.0f, FloatValue, 1.0f) * 255.0f);
        sprintf(Buffer + (Index * 2), "%02X", Value);
    }
    Buffer[ChannelCount * 2] = '\0';
}

/* https://stackoverflow.com/a/17897228 */
static inline Rr_Vec3 Rr_UIRGBToHSV(Rr_Vec3 *Color)
{
    const Rr_Vec4 K = Rr_V4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
    Rr_Vec4 P;
    if (Color->G >= Color->B)
    {
        P = Rr_V4(Color->G, Color->B, K.X, K.Y);
    }
    else
    {
        P = Rr_V4(Color->B, Color->G, K.W, K.Z);
    }
    Rr_Vec4 Q;
    if (Color->R >= P.X)
    {
        Q = Rr_V4(Color->R, P.Y, P.Z, P.X);
    }
    else
    {
        Q = Rr_V4(P.X, P.Y, P.W, Color->R);
    }
    float D = Q.X - RR_MIN(Q.W, Q.Y);
    float E = 1.0e-10f;
    return Rr_V3(fabsf(Q.Z + (Q.W - Q.Y) / (6.0f * D + E)), D / (Q.X + E), Q.X);
}

static inline Rr_Vec3 Rr_UIHSVToRGB(Rr_Vec3 *HSV)
{
    const Rr_Vec4 K = Rr_V4(1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f);
    Rr_Vec3 A = Rr_V3(HSV->X, HSV->X, HSV->X);
    A = Rr_AddV3(A, K.RGB);
    A.X = A.X - (float)(int)A.X;
    A.Y = A.Y - (float)(int)A.Y;
    A.Z = A.Z - (float)(int)A.Z;
    A = Rr_MulV3F(A, 6.0f);
    A = Rr_SubV3(A, Rr_V3(K.W, K.W, K.W));
    A.X = fabsf(A.X);
    A.Y = fabsf(A.Y);
    A.Z = fabsf(A.Z);
    Rr_Vec3 B = Rr_SubV3(A, Rr_V3(K.X, K.X, K.X));
    B.X = RR_CLAMP(0.0f, B.X, 1.0f);
    B.Y = RR_CLAMP(0.0f, B.Y, 1.0f);
    B.Z = RR_CLAMP(0.0f, B.Z, 1.0f);
    return Rr_MulV3F(Rr_LerpV3(Rr_V3(K.X, K.X, K.X), HSV->Y, B), HSV->Z);
}

static inline void Rr_UIColorPickerPopup(
    Rr_Vec2 Center,
    int ChannelCount,
    float *Channels)
{
    const Rr_UIWindowFlags POPUP_WINDOW_FLAGS =
        RR_UI_WINDOW_FLAGS_NO_TITLE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_MINIMIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT |
        RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;

    float TargetSize = gUIContext->FontSize * 15.0f;
    float Step = TargetSize / 6.0f;

    Rr_Vec2 Position = Center;
    Position.X -= (gUIContext->ContentsPadding.Width + TargetSize) / 2.0f;
    Position.Y -= (gUIContext->ContentsPadding.Height + TargetSize) / 2.0f;
    Rr_UISetNextWindowOpenPosition(Position);
    Rr_UIBeginPopupWindow(POPUP_WINDOW_FLAGS);

    Rr_UIWindow *Window = &gUIContext->PopupWindow;

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    Rr_Vec4 OpaqueColor;
    memcpy(&OpaqueColor, Channels, sizeof(float) * (size_t)ChannelCount);
    OpaqueColor.A = 1.0f;

    bool HSVChanged = false;

    Rr_UIBeginHorizontal();

    /* Draw saturation and value selector. */

    static Rr_Vec3 StaticHSV;
    if (Window->OpenedThisFrame)
    {
        StaticHSV = Rr_UIRGBToHSV((Rr_Vec3 *)Channels);
    }

    Rr_Vec3 TopRightColorHSV = StaticHSV;
    TopRightColorHSV.Y = 1.0f;
    TopRightColorHSV.Z = 1.0f;
    Rr_Vec3 TopRightColor = Rr_UIHSVToRGB(&TopRightColorHSV);

    {
        Rr_UIVertex Vertices[4];

        Vertices[0].Color = Rr_V4F(1.0f);
        Vertices[0].Position = Layout->Cursor;
        Vertices[0].UV = Rr_V2F(0.0f);

        Vertices[1].Color =
            Rr_V4(TopRightColor.X, TopRightColor.Y, TopRightColor.Z, 1.0f);
        Vertices[1].Position = Layout->Cursor;
        Vertices[1].Position.X += TargetSize;
        Vertices[1].UV = Rr_V2F(0.0f);

        Vertices[2].Color =
            Rr_V4(TopRightColor.X, TopRightColor.Y, TopRightColor.Z, 1.0f);
        Vertices[2].Position = Layout->Cursor;
        Vertices[2].Position.X += TargetSize;
        Vertices[2].Position.Y += TargetSize;
        Vertices[2].UV = Rr_V2F(0.0f);

        Vertices[3].Color = Rr_V4F(1.0f);
        Vertices[3].Position = Layout->Cursor;
        Vertices[3].Position.Y += TargetSize;
        Vertices[3].UV = Rr_V2F(0.0f);

        Rr_UIDrawQuad(Vertices);

        Vertices[0].Color = Rr_V4F(0.0f);
        Vertices[1].Color = Rr_V4F(0.0f);
        Vertices[2].Color = Rr_V4(0.0f, 0.0f, 0.0f, 1.0f);
        Vertices[3].Color = Rr_V4(0.0f, 0.0f, 0.0f, 1.0f);

        Rr_UIDrawQuad(Vertices);
    }

    Rr_UIDrawInnerFrame(
        &(Rr_Rect){ Layout->Cursor, Rr_V2F(TargetSize) },
        gUIContext->FrameThickness,
        &gUIContext->Style.Outline);

    float SVSelectorCircleSize = TargetSize * 0.035f;

    Rr_UIHash SVSelectorHash =
        Rr_UIGetHash("SVSelector", sizeof("SVSelector"), Rr_UICurrentHash());

    Rr_Rect SVSelectorRect = {
        .Offset = Layout->Cursor,
        .Extent = Rr_V2F(TargetSize),
    };

    Rr_UIDragResult Result = Rr_UIDragBehavior(
        Layout,
        &SVSelectorRect,
        RR_UI_DRAG_OP_WIDGET,
        SVSelectorHash,
        Rr_V2F(0.0f));

    if (Result.Began || Result.Held)
    {
        Rr_Vec2 Delta = Rr_SubV2(gUIContext->MousePosition, Layout->Cursor);
        Delta = Rr_DivV2F(Delta, TargetSize);
        Delta.X = RR_CLAMP(0.0f, Delta.X, 1.0f);
        Delta.Y = RR_CLAMP(0.0f, Delta.Y, 1.0f);

        StaticHSV.Y = Delta.X;
        StaticHSV.Z = 1.0f - Delta.Y;

        *(Rr_Vec3 *)Channels = Rr_UIHSVToRGB(&StaticHSV);

        SVSelectorCircleSize *= 1.5f;

        HSVChanged = true;
    }

    Rr_Vec2 SVSelectorCircleOffset = Rr_AddV2(
        Layout->Cursor,
        Rr_V2(StaticHSV.Y * TargetSize, (1.0f - StaticHSV.Z) * TargetSize));

    Rr_UIDrawCircle(
        SVSelectorCircleOffset,
        SVSelectorCircleSize,
        3.0f,
        &(Rr_Vec4){ 0.0f, 0.0f, 0.0f, 1.0f });
    Rr_UIDrawCircle(
        SVSelectorCircleOffset,
        SVSelectorCircleSize,
        1.5f,
        &gUIContext->Style.Foreground);
    if (Result.Held)
    {
        Rr_UIDrawCircleFilled(
            SVSelectorCircleOffset,
            SVSelectorCircleSize * 0.9f - 1.5f,
            &OpaqueColor);
    }

    Rr_UIAdvance(Rr_V2F(TargetSize));

    Rr_UIHash HSelectorHash =
        Rr_UIGetHash("HSelector", sizeof("HSelector"), Rr_UICurrentHash());

    Rr_Vec4 HColors[6] = {
        Rr_V4(1.0f, 0.0f, 0.0f, 1.0f), Rr_V4(1.0f, 1.0f, 0.0f, 1.0f),
        Rr_V4(0.0f, 1.0f, 0.0f, 1.0f), Rr_V4(0.0f, 1.0f, 1.0f, 1.0f),
        Rr_V4(0.0f, 0.0f, 1.0f, 1.0f), Rr_V4(1.0f, 0.0f, 1.0f, 1.0f),
    };

    float HSelectorWidth = TargetSize * 0.15f;

    Rr_Rect HSelectorRect = {
        .Offset = Layout->Cursor,
        .Extent = Rr_V2(HSelectorWidth, TargetSize),
    };

    for (size_t Index = 0; Index < 6; ++Index)
    {
        Rr_UIDrawVerticalGradientQuad(
            &(Rr_Rect){
                .Offset = Rr_V2(
                    HSelectorRect.Offset.X,
                    HSelectorRect.Offset.Y +
                        (float)Index * HSelectorRect.Extent.Height / 6.0f),
                .Extent = Rr_V2(
                    HSelectorRect.Extent.Width,
                    HSelectorRect.Extent.Height / 6.0f),
            },
            &HColors[Index],
            &HColors[(Index + 1) % 6]);
    }

    Rr_UIDrawInnerFrame(
        &HSelectorRect,
        gUIContext->FrameThickness,
        &gUIContext->Style.Outline);

    /* Draw hue handles. */

    float TriangleOutline = 2.0f;
    float TriangleSize = TargetSize * 0.035f;
    Rr_Vec2 LeftTriangleOffset = Rr_V2(
        Layout->Cursor.X + TriangleSize * 0.5f,
        Layout->Cursor.Y + StaticHSV.X * TargetSize);
    Rr_UIDrawFitTriangleFilled(
        LeftTriangleOffset,
        TriangleSize + TriangleOutline,
        RR_ANGLE_DEG(0.0f),
        Rr_V4(0.0f, 0.0f, 0.0f, 1.0f));
    Rr_UIDrawFitTriangleFilled(
        LeftTriangleOffset,
        TriangleSize,
        RR_ANGLE_DEG(0.0f),
        gUIContext->Style.Foreground);
    Rr_Vec2 RightTriangleOffset = Rr_V2(
        Layout->Cursor.X + HSelectorWidth - TriangleSize * 0.5f,
        Layout->Cursor.Y + StaticHSV.X * TargetSize);
    Rr_UIDrawFitTriangleFilled(
        RightTriangleOffset,
        TriangleSize + TriangleOutline,
        RR_ANGLE_DEG(180.0f),
        Rr_V4(0.0f, 0.0f, 0.0f, 1.0f));
    Rr_UIDrawFitTriangleFilled(
        RightTriangleOffset,
        TriangleSize,
        RR_ANGLE_DEG(180.0f),
        gUIContext->Style.Foreground);

    Result = Rr_UIDragBehavior(
        Layout,
        &HSelectorRect,
        RR_UI_DRAG_OP_WIDGET,
        HSelectorHash,
        Rr_V2F(0.0f));

    if (Result.Began || Result.Held)
    {
        float Delta = gUIContext->MousePosition.Y - Layout->Cursor.Y;
        Delta /= TargetSize;
        Delta = RR_CLAMP(0.0f, Delta, 1.0f);

        StaticHSV.X = Delta;

        *(Rr_Vec3 *)Channels = Rr_UIHSVToRGB(&StaticHSV);

        HSVChanged = true;
    }

    Rr_UIAdvance(HSelectorRect.Extent);

    Rr_UIEndHorizontal();

    bool RGBChanged = ChannelCount == 3
                          ? Rr_UIInputFloat3("RGB###RGB32", Channels)
                          : Rr_UIInputFloat4("RGBA###RGBA32", Channels);

    if (RGBChanged)
    {
        HSVChanged = false;
        StaticHSV = Rr_UIRGBToHSV((Rr_Vec3 *)Channels);
    }

    if (Rr_UIInputFloat3("HSV###HSV32", StaticHSV.Elements))
    {
        HSVChanged = true;
    }

    char HexBuffer[8 + 1];
    size_t HexBufferCapacity = (size_t)(ChannelCount * 2) + 1;
    Rr_UIRGBAToHexString(HexBuffer, ChannelCount, Channels);
    if (Rr_UIInputField(
            "RGB (Hex)",
            HexBufferCapacity,
            HexBuffer,
            "",
            Rr_UIHexFilter,
            RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
                RR_UI_INPUT_FIELD_FLAGS_AUTOSELECT_BIT |
                RR_UI_INPUT_FIELD_FLAGS_AUTOCENTER_BIT))
    {
        uint32_t NewColor;
        sscanf(HexBuffer, "%x", &NewColor);
        if (ChannelCount == 3)
        {
            NewColor <<= 8;
            *(Rr_Vec3 *)Channels = Rr_U32ToRGB(NewColor);
        }
        else if (ChannelCount == 4)
        {
            *(Rr_Vec4 *)Channels = Rr_U32ToRGBA(NewColor);
        }
    }

    {
        /* unsigned char R = (unsigned char)(Color->X * 255.0f); */
        /* unsigned char G = (unsigned char)(Color->Y * 255.0f); */
        /* unsigned char B = (unsigned char)(Color->Z * 255.0f); */
        /* unsigned char A = (unsigned char)(Color->W * 255.0f); */
        /* Rr_UILabelF("%d %d %d %d", R, G, B, A); */
    }

    if (HSVChanged)
    {
        *(Rr_Vec3 *)Channels = Rr_UIHSVToRGB(&StaticHSV);
    }

    Rr_UIEndWindow();
}

static inline bool Rr_UIInputColorEx(
    const char *Title,
    int ChannelCount,
    float *Channels)
{
    Rr_UIAssertWindow();
    assert(Title != NULL);
    assert(Channels != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_UIFlexibleWidgetLayout FlexibleWidgetLayout =
        Rr_UICalculateFlexibleWidgetLayout(Layout, TitleLength, Title);

    Rr_Vec2 ColorBoxSize =
        Rr_V2F(gUIContext->LineHeight + gUIContext->ButtonPadding.X);

    float TotalFloatInputsWidth = FlexibleWidgetLayout.WidgetWidth -
                                  ColorBoxSize.X - gUIContext->ComponentMargin;

    Rr_Vec2 TotalExtent;
    bool Edited = Rr_UIGenericInputFieldScalarMulti(
        TitleHash,
        Layout->Cursor,
        ChannelCount,
        1,
        Channels,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT2,
        RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTOSELECT_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTOCENTER_BIT,
        TotalFloatInputsWidth,
        true,
        &TotalExtent);

    Rr_Vec2 ColorBoxPosition = Layout->Cursor;
    ColorBoxPosition.X += TotalExtent.Width + gUIContext->ComponentMargin;

    Rr_UIButtonResult Result = Rr_UIButtonBehavior(
        Layout,
        &(Rr_Rect){
            ColorBoxPosition,
            ColorBoxSize,
        });

    if (Result.Up)
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
        Rr_UIColorPickerPopup(PopupCenter, ChannelCount, Channels);
    }

    Rr_Rect ColorBoxRect = {
        ColorBoxPosition,
        ColorBoxSize,
    };
    Rr_Vec4 OpaqueColor;
    memcpy(&OpaqueColor, Channels, sizeof(float) * (size_t)ChannelCount);
    OpaqueColor.A = 1.0f;
    Rr_UIDrawBevel(&ColorBoxRect, &OpaqueColor, Result.Held);

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.Y += gUIContext->ButtonPadding.Height;
    TitlePosition.X += FlexibleWidgetLayout.TitleCursorOffsetX;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        0,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        Layout->AvailableContentsWidth,
        TotalExtent.Height,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return ColorChanged;
}

bool Rr_UIInputColor3(const char *Title, float *Channels)
{
    return Rr_UIInputColorEx(Title, 3, Channels);
}

bool Rr_UIInputColor4(const char *Title, float *Channels)
{
    return Rr_UIInputColorEx(Title, 4, Channels);
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

    Rr_UIButtonResult Result = Rr_UIButtonBehavior(
        Layout,
        &(Rr_Rect){
            ButtonPosition,
            BorderSize,
        });

    if (Result.Up)
    {
        gUIContext->PopupWindowParent = Window;
        gUIContext->PopupWindowHash = TitleHash;
    }

    bool OptionChanged = false;
    bool PopupOpen = gUIContext->PopupWindowParent == Window &&
                     gUIContext->PopupWindowHash == TitleHash;

    if (PopupOpen)
    {
        const Rr_UIWindowFlags POPUP_WINDOW_FLAGS =
            RR_UI_WINDOW_FLAGS_NO_TITLE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
            RR_UI_WINDOW_FLAGS_NO_MINIMIZE_BIT |
            RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT |
            RR_UI_WINDOW_FLAGS_NO_MOVE_BIT | RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;

        Rr_Vec2 PopupPosition = ButtonPosition;
        PopupPosition.Y += BorderSize.Height + gUIContext->FrameThickness;
        PopupPosition.X += gUIContext->FrameThickness;
        Rr_UISetNextWindowPosition(PopupPosition);
        Rr_UISetNextWindowPadding(Rr_V2(gUIContext->ButtonPadding.Width, 0.0f));
        Rr_UIBeginPopupWindow(POPUP_WINDOW_FLAGS);
        Rr_UILayout *PopupLayout = Rr_UICurrentLayout();
        for (uint32_t Index = 0; Index < OptionCount; ++Index)
        {
            Rr_UIPrimitive OptionButtonQuad = Rr_UIReserveQuad();
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
            Rr_UIButtonResult Result =
                Rr_UIButtonBehavior(PopupLayout, &OptionButtonRect);
            if (Result.Up)
            {
                *SelectedIndex = Index;
                Rr_UIClosePopupWindow();
                OptionChanged = true;
            }
            Rr_Vec4 OptionButtonColor;
            if (Result.Held)
            {
                OptionButtonColor = gUIContext->Style.ButtonHeld;
            }
            else if (Result.Hovered)
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
                OptionButtonQuad.Vertices,
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

    Rr_UIBevel(
        Primitive,
        &ButtonRect,
        &gUIContext->Style.InputFieldNormal,
        Result.Held);

    /* Add handle. */
    {
        float HandleSize = ButtonSize.Height;
        Rr_Rect HandleRect = { ButtonRect.Offset, Rr_V2F(HandleSize) };
        HandleRect.Offset.X += ButtonRect.Extent.Width;

        Rr_UIDrawBevel(
            &HandleRect,
            &gUIContext->Style.ButtonNormal,
            Result.Held);

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
        gUIContext->LineHeight + gUIContext->ButtonPadding.Height * 2.0f,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return OptionChanged;
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

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_UIFlexibleWidgetLayout FlexibleWidgetLayout =
        Rr_UICalculateFlexibleWidgetLayout(Layout, TitleLength, Title);

    float SliderWidth = FlexibleWidgetLayout.WidgetWidth;
    Rr_Rect SliderRect = {
        Layout->Cursor,
        {
            SliderWidth,
            gUIContext->LineHeight,
        },
    };

    Rr_UIDrawBevel(&SliderRect, &gUIContext->Style.InputFieldNormal, true);

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

    Rr_UIDragResult Result = Rr_UIDragBehavior(
        Layout,
        &SliderRect,
        RR_UI_DRAG_OP_WIDGET,
        TitleHash,
        Rr_V2(HandleDragOffset, 0.0f));

    if (Result.Moved)
    {
        float SliderMin = Layout->Cursor.X + HandleWidth / 2.0f;
        float SliderMax = SliderMin + SliderWidth - HandleWidth;

        Normalized =
            (gUIContext->MousePosition.X - SliderMin) / (SliderMax - SliderMin);
        Normalized = RR_CLAMP(0.0f, Normalized, 1.0f);
    }
    else if (Result.Hovered)
    {
        if (gUIContext->MouseWheelDelta.X != 0.0f)
        {
            /* NOTE: Probably shouldn't be hardcoded to 30.0f. */
            Normalized += gUIContext->MouseWheelDelta.X / 30.0f;
        }
    }

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += FlexibleWidgetLayout.TitleCursorOffsetX;
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
        gUIContext->LineHeight,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return Normalized;
}

bool Rr_UISliderInt(const char *Title, int32_t *Value, int32_t Min, int32_t Max)
{
    assert(Value != NULL);
    assert(Max > Min);

    char Buffer[32];
    int Length = snprintf(Buffer, 32, "%d", *Value);

    int32_t In = *Value;
    int32_t Clamped = RR_CLAMP(Min, *Value, Max);
    float InNormalized = (float)(Clamped - Min) / (float)(Max - Min);
    float OutNormalized =
        Rr_UISlider(Title, InNormalized, Buffer, (size_t)Length);
    OutNormalized =
        roundf(OutNormalized * (float)(Max - Min)) / (float)(Max - Min);
    int32_t Out = (int32_t)(OutNormalized * (float)(Max - Min)) + Min;
    *Value = Out;
    return In != Out;
}

bool Rr_UISliderFloat(const char *Title, float *Value, float Min, float Max)
{
    assert(Value != NULL);
    assert(Max > Min);

    char Buffer[32];
    int Length = snprintf(Buffer, 32, "%.4f", *Value);

    float In = *Value;
    float Clamped = RR_CLAMP(Min, *Value, Max);
    float InNormalized = (Clamped - Min) / (Max - Min);
    float OutNormalized =
        Rr_UISlider(Title, InNormalized, Buffer, (size_t)Length);
    float Out = OutNormalized * (Max - Min) + Min;
    *Value = Out;
    return In != Out;
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
    gUIContext->NextWindowOpenPosition = Rr_V2F(INFINITY);
    gUIContext->NextWindowSize = Rr_V2F(INFINITY);
    gUIContext->NextWindowPadding = Rr_V2F(INFINITY);

    Rr_IntVec2 DisplaySize = Rr_GetDisplaySize();
    Rr_UISetFontSize((float)DisplaySize.Width / 112.0f);

    gUIContext->Style = (Rr_UIStyle){
        .TitlePadding = { 0.5f, 0.03f },
        .ContentsPadding = { 0.5f, 0.5f },
        .ComponentMargin = 0.2f,
        .BevelIntensityLight = 0.3f,
        .BevelIntensityDark = 0.7f,

        .Foreground = Rr_U32ToSRGB(0xD6D0B3FF),
        .ForegroundDimmed = Rr_U32ToSRGB(0xA7A59CFF),
        .Background = Rr_U32ToSRGB(0x292F33FF),
        .Outline = Rr_U32ToSRGB(0x6C6F72FF),

        .TitleBackground = Rr_U32ToSRGB(0x5E2D96FF),
        .TitleCloseButtonBackground = Rr_U32ToSRGB(0xD54251FF),
        .TitleCollapseButtonBackground = Rr_U32ToSRGB(0x5E2D96FF),

        .ButtonNormal = Rr_U32ToSRGB(0x4c565dFF),
        .ButtonHovered = Rr_U32ToSRGB(0x687e8dFF),
        .ButtonHeld = Rr_U32ToSRGB(0x435866FF),
        .ButtonDisabled = Rr_U32ToSRGB(0x191e22FF),

        .SelectedTextBackground = Rr_U32ToSRGB(0x6EA5FEFF),
        .InputFieldNormal = Rr_U32ToSRGB(0x191e22FF),
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

    Rr_Asset VertexShader = Rr_LoadAsset(RR_BUILTIN_UI_VERT_SPV);
    Rr_Asset FragmentShader = Rr_LoadAsset(RR_BUILTIN_UI_FRAG_SPV);

    Rr_GraphicsPipelineCreateInfo PipelineInfo = {
        .Layout = gUIContext->PipelineLayout,
        .VertexShaderSPVSize = VertexShader.Size,
        .VertexShaderSPVData = VertexShader.Pointer,
        .FragmentShaderSPVSize = FragmentShader.Size,
        .FragmentShaderSPVData = FragmentShader.Pointer,
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

        Rr_UIStyle *Style = &gUIContext->Style;

        gUIContext->LineHeight =
            gUIContext->FontSize * gUIContext->Font->LineHeight;
        gUIContext->ContentsPadding =
            Rr_MulV2F(gUIContext->Style.ContentsPadding, gUIContext->FontSize);
        gUIContext->ComponentMargin =
            RR_UI_ROUND(Style->ComponentMargin * gUIContext->FontSize);

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

        gUIContext->TitleHeight = RR_UI_ROUND(
            gUIContext->Style.TitlePadding.Height * 2.0f *
                gUIContext->FontSize +
            gUIContext->LineHeight);
        gUIContext->TitleButtonSize = RR_UI_ROUND(gUIContext->TitleHeight);
        gUIContext->TitlePadding =
            Rr_MulV2F(gUIContext->Style.TitlePadding, gUIContext->FontSize);
        gUIContext->MinWindowSizeNoTitle =
            Rr_MulV2F(gUIContext->ContentsPadding, 2.0f);
        gUIContext->MinWindowSizeNoTitle.X += gUIContext->ScrollbarWidth;
        gUIContext->MinWindowSizeNoTitle.X += gUIContext->FontSize * 2.0f;
        gUIContext->MinWindowSizeNoTitle.Y += gUIContext->FontSize * 2.0f;
        gUIContext->MinWindowSizeNoTitle =
            RR_UI_ROUND_V2(gUIContext->MinWindowSizeNoTitle);
        gUIContext->MinWindowSize = gUIContext->MinWindowSizeNoTitle;
        gUIContext->MinWindowSize.Y += gUIContext->TitleHeight;
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

    gUIContext->ClipRectBoundsStack.Count = 0;
    Rr_UIPushClipRectBounds(&(Rr_Rect){
        .Offset = Rr_V2F(0.0f),
        .Extent = gUIContext->ScreenSize,
    });

    if (gUIContext->SkipLeftMouseButtonUp && gUIContext->LeftMouseButtonUp)
    {
        gUIContext->SkipLeftMouseButtonUp = false;
        gUIContext->LeftMouseButtonUp = false;
    }

    gUIContext->HoveredWindow = NULL;
    if (Rr_UIPopupWindowActive())
    {
        if (Rr_RectContains(
                &gUIContext->PopupWindow.VisibleRect,
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
            if (Rr_RectContains(
                    &Window->VisibleRect,
                    gUIContext->MousePosition))
            {
                gUIContext->HoveredWindow = Window;

                if (gUIContext->LeftMouseButtonDown)
                {
                    Rr_UIPutWindowOnTop(Window->TopLevelParent);

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
    size_t ClipRectCount = Window->TopLevelClipRects->Count;
    for (size_t ClipRectIndex = 0; ClipRectIndex < ClipRectCount;
         ++ClipRectIndex)
    {
        Rr_UIClipRect *ClipRect =
            Window->TopLevelClipRects->Data + ClipRectIndex;

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
    RR_BEGIN_FRAME_SECTION("Rr.UI");

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

    Rr_BeginGraphLabel(Rr_GetGraph(), "Rr.UI");

    Rr_ColorTarget ColorTarget = {
        .Image = SwapchainImage,
        .LoadOp = RR_LOAD_OP_LOAD,
        .StoreOp = RR_STORE_OP_STORE,
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
        Window->Added = false;
        Window->OpenedThisFrame = false;
        if (Window->SkipThisFrame)
        {
            Window->SkipThisFrame = false;
            continue;
        }
        if (Window->TopLevelParent == Window)
        {
            Rr_UIDrawWindow(Window, GraphicsNode);
        }
    }

    Rr_EndGraphLabel(Rr_GetGraph(), "Rr.UI");

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

    RR_END_FRAME_SECTION("Rr.UI");
}

float Rr_UIGetFontSize(void)
{
    return gUIContext->FontSize;
}

void Rr_UISetFontSize(float Size)
{
    gUIContext->NextFontSize =
        RR_CLAMP(RR_UI_MIN_FONT_SIZE, RR_UI_ROUND(Size), RR_UI_MAX_FONT_SIZE);
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
            Rr_Vec2 MousePosition = Rr_GetMousePosition();
            Rr_Vec2 MouseDelta = Rr_GetMousePositionDelta();
            Rr_UILabelF(
                "Time: %.2f\n"
                "Mouse Position: %.2f %.2f\n"
                "Mouse Delta: %.2f %.2f\n"
                "UI Font Size: %.2f",
                Rr_GetTimeSeconds(),
                MousePosition.X,
                MousePosition.Y,
                MouseDelta.X,
                MouseDelta.Y,
                gUIContext->FontSize);
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

            double MainLoopMS =
                (double)(RR_GET_FRAME_SECTION("Rr.MainLoop") * 1000) /
                (double)Rr_GetPerformanceFrequency();
            double UIMS = (double)(RR_GET_FRAME_SECTION("Rr.UI") * 1000) /
                          (double)Rr_GetPerformanceFrequency();
            double FrameGraphMS =
                (double)(RR_GET_FRAME_SECTION("Rr.FrameGraph") * 1000) /
                (double)Rr_GetPerformanceFrequency();

            Rr_UILabelF(
                "Main Loop: %.3fms\n"
                "UI: %.3fms\n"
                "Frame Graph: %.3fms",
                MainLoopMS,
                UIMS,
                FrameGraphMS);

            if (gRenderer->GraphicsQueue.TimestampsEnabled)
            {
                Rr_UILabelF("GPU: %.3fms", gRenderer->LastFrameMS);
            }
            else
            {
                Rr_UILabelF(
                    "GPU timestamps not supported!",
                    gRenderer->LastFrameMS);
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
            Rr_UIDebugOverlayArena(Rr_GetThreadContext()->Arena, "Main Thread");
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
