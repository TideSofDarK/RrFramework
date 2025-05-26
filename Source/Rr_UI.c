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
    size_t IndexCount;
    size_t FirstIndex;
    Rr_Rect Rect;
};

typedef struct Rr_UIWindow Rr_UIWindow;
struct Rr_UIWindow
{
    Rr_String Title;
    Rr_UIHash Hash;
    Rr_UIWindowFlags Flags;
    Rr_Rect Rect;
    Rr_Vec2 ContentsStart;
    Rr_Vec2 ContentsEnd;
    float VScroll;
    int ZOrder;
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
    const char **SelectedTabRef;
    const char *SelectedTab;
};

struct Rr_UIContext
{
    Rr_UIStyle Style;

    Rr_Map *WindowMap;
    int TotalWindowCount;
    RR_ARRAY(Rr_UIWindow *) ActiveWindows;
    Rr_UIWindow *HoveredWindow;

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
    Rr_Vec2 MousePosition;
    Rr_Vec2 MouseWheelDelta;

    Rr_UIDragOp DragOp;
    Rr_UIWindow *DragOpWindow;
    Rr_UIHash DragOpHash;
    Rr_Vec2 DragOpMouseStart;
    Rr_Vec2 DragOpWindowStart;
    bool DragOpBeganThisFrame;
    bool DragOpEndedThisFrame;

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

static Rr_UIContext *gContext;

#define RR_UI_ROUND(Value) (ceil((Value) / 2.0f) * 2.0f)

#define RR_UI_ROUND_V2(Value) \
    (Rr_V2(RR_UI_ROUND((Value).X), RR_UI_ROUND((Value).Y)))

Rr_UIFont *Rr_UICreateFont(
    Rr_UIContext *Context,
    Rr_AssetRef FontPNGRef,
    Rr_AssetRef FontJSONRef)
{
#define CJSON_GET_OBJECT_FLOAT(Object, Item) \
    ((float)cJSON_GetNumberValue(cJSON_GetObjectItem(Object, Item)))

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

    Rr_UIFont *Font = RR_GET_FREE_LIST_ITEM(&Context->Fonts, Context->Arena);
    *Font = (Rr_UIFont){
        .Atlas = Atlas,
        .LineHeight = CJSON_GET_OBJECT_FLOAT(MetricsJSON, "lineHeight"),
        .UnderlineY = CJSON_GET_OBJECT_FLOAT(MetricsJSON, "underlineY"),
        .UnderlineThickness =
            CJSON_GET_OBJECT_FLOAT(MetricsJSON, "underlineThickness"),
        .DefaultSize = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "size"),
        .DistanceRange = CJSON_GET_OBJECT_FLOAT(AtlasJSON, "distanceRange"),
    };

    cJSON *GlyphsJSON = cJSON_GetObjectItem(FontDataJSON, "glyphs");

    size_t GlyphCount = cJSON_GetArraySize(GlyphsJSON);
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

void Rr_UIDestroyFont(Rr_UIContext *Context, Rr_UIFont *Font)
{
    Rr_DestroyImage(gApp->Renderer, Font->Atlas);

    RR_RETURN_FREE_LIST_ITEM(&Context->Fonts, Font);
}

static inline Rr_UILayout *Rr_UICurrentLayout(void)
{
    return gContext->Stack.Count > 0
               ? &gContext->Stack.Data[gContext->Stack.Count - 1]
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
    size_t LengthHint)
{
    if (LengthHint == 0)
    {
        LengthHint = strlen(CString);
    }
    const char *ExplicitHash = strstr(CString, "###");
    if (ExplicitHash)
    {
        ExplicitHash += 3;
        assert(
            ExplicitHash < (CString + LengthHint) &&
            "Empty hash after ### sentinel!");

        return Rr_UIGetHash(
            ExplicitHash,
            LengthHint - ((CString + LengthHint) - ExplicitHash),
            Rr_UICurrentHash());
    }
    else
    {
        return Rr_UIGetHash(CString, LengthHint, Rr_UICurrentHash());
    }
}

static inline Rr_UIPrimitive Rr_UIReservePrimitive(
    size_t VertexCount,
    size_t IndexCount)
{
    Rr_UIIndex BaseVertex = (Rr_UIIndex)gContext->Vertices.Count;
    return (Rr_UIPrimitive){
        .Vertices = RR_PUSH_INTO_ARRAY_MANY(
            &gContext->Vertices,
            VertexCount,
            gContext->FrameArena),
        .Indices = RR_PUSH_INTO_ARRAY_MANY(
            &gContext->Indices,
            IndexCount,
            gContext->FrameArena),
        .BaseVertex = BaseVertex,
    };
}

static inline Rr_UIVertex *Rr_UIReserveQuads(size_t Count)
{
    Rr_UIIndex Base = (Rr_UIIndex)gContext->Vertices.Count;
    static const Rr_UIIndex QUAD_INDICES[] = { 0, 1, 2, 1, 3, 2 };

    Rr_UIVertex *FirstVertex = RR_PUSH_INTO_ARRAY_MANY(
        &gContext->Vertices,
        Count * 4,
        gContext->FrameArena);

    size_t IndexCount = Count * 6;
    Rr_UIIndex *FirstIndex = RR_PUSH_INTO_ARRAY_MANY(
        &gContext->Indices,
        IndexCount,
        gContext->FrameArena);

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

static inline void Rr_UIBevel(
    Rr_UIPrimitive Primitive,
    Rr_Rect *Rect,
    Rr_Vec4 *BaseColor,
    bool Pressed)
{
    Rr_Vec4 TopLeftColor;
    TopLeftColor.RGB = Rr_LerpV3(
        BaseColor->RGB,
        Pressed ? gContext->Style.BevelIntensityDark
                : gContext->Style.BevelIntensityLight,
        Rr_V3F(Pressed ? 0.0f : 1.0f));
    TopLeftColor.A = 1.0f;
    Rr_Vec4 BottomRightColor;

    BottomRightColor.RGB = Rr_LerpV3(
        BaseColor->RGB,
        Pressed ? gContext->Style.BevelIntensityLight
                : gContext->Style.BevelIntensityDark,
        Rr_V3F(Pressed ? 1.0f : 0.0f));
    BottomRightColor.A = 1.0f;

    Rr_Vec4 BackgroundColor = *BaseColor;
    if (Pressed)
    {
        BackgroundColor.RGB =
            Rr_LerpV3(BackgroundColor.RGB, 0.4f, Rr_V3F(0.0f));
    }
    BackgroundColor.A = 1.0f;

    Rr_UIVertex *Vertices = Primitive.Vertices;
    Rr_UIIndex *Indices = Primitive.Indices;

    float Thickness = gContext->BevelThickness;

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
        .Color = BackgroundColor,
    };
    Vertices[13] = (Rr_UIVertex){
        .Position = Vertices[3].Position,
        .Color = BackgroundColor,
    };
    Vertices[14] = (Rr_UIVertex){
        .Position = Vertices[5].Position,
        .Color = BackgroundColor,
    };
    Vertices[15] = (Rr_UIVertex){
        .Position = Vertices[8].Position,
        .Color = BackgroundColor,
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
    Rr_UIIndex Base = (Rr_UIIndex)gContext->Vertices.Count;
    Rr_UIIndex Indices[] = {
        Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2,
    };
    for (size_t Index = 0; Index < 4; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gContext->Vertices, gContext->FrameArena) =
            Vertices[Index];
    }

    for (size_t Index = 0; Index < 6; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gContext->Indices, gContext->FrameArena) =
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
        *RR_PUSH_INTO_ARRAY(&gContext->Indices, gContext->FrameArena) =
            (Rr_UIIndex)(gContext->Vertices.Count + Index);
    }

    for (size_t Index = 0; Index < 3; ++Index)
    {
        *RR_PUSH_INTO_ARRAY(&gContext->Vertices, gContext->FrameArena) =
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

static inline Rr_Vec2 Rr_UIDrawText(
    bool CalculateOnly,
    Rr_Vec2 Position,
    Rr_String *String,
    float AvailableWidth,
    Rr_Vec4 *Color,
    Rr_UITextFlags Flags)
{
    if (String->Length == 0)
    {
        return Rr_V2F(0.0f);
    }

    Rr_UIFont *Font = gContext->Font;
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

    if (Wrapped)
    {
        float CurrentWordWidth = 0.0f;
        size_t CurrentWordStart = 0;

        for (size_t Index = 0; Index < String->Length; ++Index)
        {
            uint32_t Codepoint = String->Data[Index];

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

            if (Codepoint == ' ' || Index == String->Length - 1)
            {
                size_t WordLength = Index - CurrentWordStart;
                if (WordLength > 0)
                {
                    if (CurrentWordWidth > AvailableWidth)
                    {
                        /* Fallback to per-character wrapping. */

                        for (size_t IndexInWord = CurrentWordStart;
                             IndexInWord <= Index;
                             ++IndexInWord)
                        {
                            Codepoint = String->Data[IndexInWord];
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
                            CurrentX +=
                                gContext->Font->Advances[Codepoint] * FontSize;
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
                             IndexInWord <= Index;
                             ++IndexInWord)
                        {
                            Codepoint = String->Data[IndexInWord];
                            if (!CalculateOnly)
                            {
                                Rr_UIDrawGlyph(
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
        for (size_t Index = 0; Index < String->Length; ++Index)
        {
            uint32_t Codepoint = String->Data[Index];

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
                CurrentX += gContext->Font->Advances[Codepoint] * FontSize;
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

            CurrentX += gContext->Font->Advances[Codepoint] * FontSize;
            MaxX = RR_MAX(MaxX, CurrentX);
        }
    }

    return (Rr_Vec2){ .Width = MaxX, .Height = CurrentY + LineHeight };
}

Rr_Vec2 Rr_UICalculateTextSize(
    Rr_UIFont *Font,
    float FontSize,
    Rr_String *String,
    float AvailableWidth,
    Rr_UITextFlags Flags)
{
    return Rr_UIDrawText(
        1,
        (Rr_Vec2){ 0 },
        String,
        AvailableWidth,
        &(Rr_Vec4){ 0 },
        Flags);
}

static inline void Rr_UIBeginDragOp(
    Rr_UIWindow *Window,
    Rr_UIDragOp DragOp,
    Rr_UIHash Hash,
    Rr_Vec2 WindowStart)
{
    gContext->DragOpMouseStart = gContext->MousePosition;
    gContext->DragOpWindow = Window;
    gContext->DragOp = DragOp;
    gContext->DragOpWindowStart = WindowStart;
    gContext->DragOpHash = Hash;
    gContext->DragOpBeganThisFrame = true;
}

static inline void Rr_UIEndDragOp(void)
{
    gContext->DragOpWindow = NULL;
    gContext->DragOp = 0;
    gContext->DragOpEndedThisFrame = true;
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

static inline bool Rr_UIScrollBehavior(
    Rr_UIWindow *Window,
    Rr_Rect *Rect,
    float *YScroll)
{
    if (Window == gContext->HoveredWindow && gContext->DragOpWindow == NULL &&
        Rr_UIRectContains(Window, Rect, gContext->MousePosition))
    {
        if (gContext->MouseWheelDelta.Y != 0.0f)
        {
            Rr_UIEndDragOp();
            *YScroll =
                *YScroll + gContext->MouseWheelDelta.Y * gContext->LineHeight;

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
    bool BlockedByDragOp = (gContext->DragOpWindow != NULL &&
                            gContext->DragOpBeganThisFrame == false) ||
                           gContext->DragOpEndedThisFrame == true;
    if (BlockedByDragOp == false && Window == gContext->HoveredWindow &&
        Rr_UIRectContains(Window, Rect, gContext->MousePosition))
    {
        if (gContext->LeftMouseButtonDown)
        {
            Rr_UIEndDragOp();
            gContext->DragOpWindow = NULL;
        }
        if (Down)
        {
            *Down = gContext->LeftMouseButtonDown;
        }
        if (Up)
        {
            *Up = gContext->LeftMouseButtonUp;
        }
        if (Held)
        {
            *Held = gContext->LeftMouseButtonHeld;
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
    bool *Hovered)
{
    bool Contains = Rr_UIRectContains(Window, Rect, gContext->MousePosition);
    if (Hovered)
    {
        *Hovered = Contains && gContext->HoveredWindow == Window;
    }

    /* Two things to note:
     * 1) Dragging resize handle also overlaps with moving and scrolling. Take
     * that into accoutn and override current drag opertion.
     * Watch out for Rr_DragBehavior() order!
     * 2) Faster mouse movements may actually result in
     * Contains == false while the drag operation is still going. */

    if (Contains && gContext->LeftMouseButtonDown &&
        (gContext->DragOpWindow == NULL || gContext->DragOpWindow == Window) &&
        gContext->HoveredWindow == Window)
    {
        Rr_UIBeginDragOp(Window, DragOp, Hash, Value);

        return false;
    }

    if (gContext->DragOpWindow == Window && gContext->DragOp == DragOp &&
        gContext->DragOpHash == Hash)
    {
        if (gContext->LeftMouseButtonHeld)
        {
            return true;
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
        RR_PUSH_INTO_ARRAY(&Window->ClipRects, gContext->FrameArena);

    *ClipRect = (Rr_UIClipRect){
        .FirstIndex = gContext->Indices.Count,
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
            Last->IndexCount = gContext->Indices.Count - Last->FirstIndex;
        }
    }
}

static inline Rr_Vec2 Rr_UIGetMinWindowSize(Rr_UIWindowFlags Flags)
{
    if (RR_HAS_BIT(Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT))
    {
        return gContext->MinWindowSizeNoTitle;
    }
    else
    {
        return gContext->MinWindowSize;
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

    float Width = gContext->TitleButtonSize * 0.7f;
    float Thickness = gContext->TitleButtonSize * 0.15f;
    Rr_Rect TitleRect = Window->Rect;
    TitleRect.Extent.Height = gContext->WindowTitleHeight;
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
        (TitleRect.Extent.Height + gContext->TitleButtonSize) * 0.5f,
    ButtonRect.Offset.Y =
        TitleRect.Offset.Y +
        (TitleRect.Extent.Height - gContext->TitleButtonSize) * 0.5f;
    ButtonRect.Extent = Rr_V2F(gContext->TitleButtonSize);

    bool Up, Held;
    Rr_UIButtonBehavior(Window, &ButtonRect, NULL, &Up, NULL, &Held);
    if (Up && Open)
    {
        *Open = false;
    }

    Rr_UIDrawBevel(&ButtonRect, &gContext->Style.TitleBackground, Held);

    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(45.0f),
        &gContext->Style.Foreground);
    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(-45.0f),
        &gContext->Style.Foreground);
}

static inline void Rr_UIAddWindowTitle(Rr_UIWindow *Window)
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
    Rr_UIDrawHorizontalGradientQuad(
        &TitleRect,
        &gContext->Style.TitleBackground,
        &ColorB);
    Rr_UIDrawText(
        0,
        Rr_AddV2(
            TitleRect.Offset,
            Rr_MulV2F(gContext->Style.TitlePadding, gContext->FontSize)),
        &Window->Title,
        0.0f,
        &gContext->Style.Foreground,
        0);
}

static inline bool Rr_UIAddResizeHandle(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

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

    bool Hovered, Dragging = Rr_UIDragBehavior(
                      Window,
                      &ResizeHandleRect,
                      RR_UI_DRAG_OP_RESIZE,
                      0,
                      Window->Rect.Extent,
                      &Hovered);

    if (Dragging)
    {
        Rr_Vec2 Delta =
            Rr_SubV2(gContext->MousePosition, gContext->DragOpMouseStart);
        Rr_Vec2 NewWindowSize = Rr_AddV2(gContext->DragOpWindowStart, Delta);
        Rr_Vec2 MinWindowSize = Rr_UIGetMinWindowSize(Window->Flags);
        NewWindowSize.X = RR_MAX(NewWindowSize.X, MinWindowSize.X);
        NewWindowSize.Y = RR_MAX(NewWindowSize.Y, MinWindowSize.Y);
        Window->Rect.Extent = Rr_FloorV2(NewWindowSize);
    }

    Layout->DeferredResizeHandleColor = gContext->Style.Foreground;
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
        Rect.Offset.Y += gContext->WindowTitleHeight;
        Rect.Extent.Height -= gContext->WindowTitleHeight;
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
            Rect.Extent.Width -= gContext->ScrollbarWidth;
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
        Layout->Cursor.X += Size.Width + gContext->HorizontalMargin;
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
        Rr_Vec2 ScrollbarSize = { gContext->ScrollbarWidth,
                                  ContentsAreaRect.Extent.Height };
        Rr_UIDrawSolidQuad(
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
        if (Rr_UIScrollBehavior(
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
        if (HasResize)
        {
            /* This cuts a bit of height from the scrollbar hitbox so the resize
             * handle is always on top. */

            float AvailableResizeButtonHeight =
                (ScrollbarPosition.Y + ScrollbarSize.Height -
                 gContext->ResizeHandleSize) -
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
                          &Hovered);

        if (Dragging)
        {
            Rr_Vec2 Delta =
                Rr_SubV2(gContext->MousePosition, gContext->DragOpMouseStart);
            float ContentsHeight =
                Window->ContentsEnd.Y - Window->ContentsStart.Y;
            float FillRatio = ContentsHeight / ContentsAreaRect.Extent.Height;
            Window->VScroll =
                gContext->DragOpWindowStart.Y + (Delta.Y * FillRatio);
        }

        Window->VScroll = RR_CLAMP(0.0f, Window->VScroll, MaxYScroll);

        Rr_UIDrawBevel(
            &(Rr_Rect){
                ScrollbarHandlePosition,
                ScrollbarHandleSize,
            },
            &gContext->Style.ScrollbarNormal,
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
    return &gContext->Style;
}

void Rr_UISetNextWindowPosition(Rr_Vec2 Position)
{
    gContext->NextWindowPosition = Position;
}

void Rr_UISetNextWindowSize(Rr_Vec2 Size)
{
    gContext->NextWindowSize = Size;
}

void Rr_UISetNextWindowPadding(Rr_Vec2 Padding)
{
    gContext->NextWindowPadding = Padding;
}

static inline void Rr_UIConsumeNextWindowPosition(Rr_UIWindow *Window)
{
    if (gContext->NextWindowPosition.X != INFINITY &&
        gContext->NextWindowPosition.Y != INFINITY)
    {
        Window->Rect.Offset = Rr_FloorV2(gContext->NextWindowPosition);
        gContext->NextWindowPosition = Rr_V2F(INFINITY);
    }
}

static inline void Rr_UIConsumeNextWindowSize(Rr_UIWindow *Window)
{
    if (gContext->NextWindowSize.Width != INFINITY &&
        gContext->NextWindowSize.Height != INFINITY)
    {
        Window->Rect.Extent = Rr_FloorV2(gContext->NextWindowSize);
        gContext->NextWindowSize = Rr_V2F(INFINITY);
    }
}

static inline bool Rr_UIBeginWindowEx(
    Rr_UIWindow *Window,
    bool *Open,
    Rr_UIWindowFlags Flags)
{
    Rr_UIConsumeNextWindowPosition(Window);
    Rr_UIConsumeNextWindowSize(Window);

    Window->Flags = Flags;

    /* Return if closed. Also handle show after being closed. */

    bool AutoResize =
        RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);

    bool WasClosed = Window->Open == false;
    Window->Open = (Open == NULL || *Open == true);
    if (Window->Open)
    {
        if (WasClosed && AutoResize)
        {
            Window->SkipDueToAutoResize = true;
        }
    }
    else
    {
        return false;
    }

    /* Have to access current window and finish its clip rect. */

    Rr_UIEndClipRect();

    Rr_UILayout *Layout =
        RR_PUSH_INTO_ARRAY(&gContext->Stack, gContext->FrameArena);
    *Layout = (Rr_UILayout){
        .Window = Window,
        .HorizontalX = INFINITY,
    };

    /* TODO: Make sure it's the correct call. */
    RR_RESET_ARRAY(&Window->ClipRects, gContext->FrameArena);

    /* Move and resize behavior.
     * Handle these early so following code may access updated window rect. */

    bool NoMove = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_MOVE_BIT);

    if (NoMove == false)
    {
        bool Dragging = Rr_UIDragBehavior(
            Window,
            &Window->Rect,
            RR_UI_DRAG_OP_MOVE,
            0,
            Window->Rect.Offset,
            NULL);

        if (Dragging)
        {
            Rr_Vec2 Delta =
                Rr_SubV2(gContext->MousePosition, gContext->DragOpMouseStart);
            Window->Rect.Offset = Rr_AddV2(gContext->DragOpWindowStart, Delta);
            Window->Rect.Offset = Rr_FloorV2(Window->Rect.Offset);
        }
    }

    bool NoResize = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT);

    if (NoResize == false && AutoResize == false)
    {
        /* Defer drawing the handle to Rr_UIEndWindow()! */

        Rr_UIAddResizeHandle(Layout);
    }

    /* Clip to total window area. */

    Rr_Rect WindowClipRect =
        Rr_ResizeRect(&Window->Rect, gContext->FrameThickness);

    Rr_UIBeginClipRect(&WindowClipRect);

    bool NoBorder = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_BORDER_BIT);

    if (NoBorder == false)
    {
        Rr_UIDrawOuterFrame(
            &Window->Rect,
            gContext->FrameThickness,
            &gContext->Style.Outline);
    }

    Layout->Cursor = Window->Rect.Offset;

    /* Add window title if necessary. */

    bool NoTitle = RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_TITLE_BIT);

    if (NoTitle == false)
    {
        Rr_UIAddWindowTitle(Window);
        if (RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_CLOSE_BIT) == true)
        {
            Rr_UIAddCloseButton(Window, Open);
        }
        Layout->Cursor.Y += gContext->WindowTitleHeight;
    }

    /* Add vertical scrollbar if necessary. */

    if (AutoResize)
    {
        Window->VScroll = 0;
        Layout->AvailableContentsWidth = Window->Rect.Extent.Width;
    }
    else
    {
        bool HasScrollbar = Rr_UIAddVerticalScrollbar(Window);
        Layout->Cursor.Y -= Window->VScroll;
        Layout->AvailableContentsWidth =
            HasScrollbar ? Window->Rect.Extent.Width - gContext->ScrollbarWidth
                         : Window->Rect.Extent.Width;
    }

    if (gContext->NextWindowPadding.Width != INFINITY &&
        gContext->NextWindowPadding.Height != INFINITY)
    {
        Layout->ContentsPadding = gContext->NextWindowPadding;
        gContext->NextWindowPadding = Rr_V2F(INFINITY);
    }
    else
    {
        Layout->ContentsPadding = gContext->ContentsPadding;
    }
    Layout->Cursor = Rr_AddV2(Layout->Cursor, Layout->ContentsPadding);
    Layout->AvailableContentsWidth -= Layout->ContentsPadding.X * 2.0f;

    Rr_UIEndClipRect();

    /* Clip to contents. */

    Rr_Rect ContentsAreaRect = Rr_UIGetWindowContentsArea(Window);
    Rr_UIBeginClipRect(&ContentsAreaRect);

    Rr_UIDrawSolidQuad(&ContentsAreaRect, &gContext->Style.Background);

    Window->ContentsStart = Window->ContentsEnd = Layout->Cursor;

    return true;
}

static inline void Rr_UIBeginPopupWindow(void)
{
    Rr_UIWindow *Window = &gContext->PopupWindow;
    Rr_UIBeginWindowEx(Window, NULL, gContext->PopupWindow.Flags);
}

static inline void Rr_UIClosePopupWindow(void)
{
    assert(gContext->PopupWindowParent != NULL);
    gContext->PopupWindowParent = NULL;
    gContext->PopupWindow.Open = false;
}

bool Rr_UIBeginWindow(const char *Title, bool *Open, Rr_UIWindowFlags Flags)
{
    size_t TitleLength = strlen(Title);
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, TitleLength);

    Rr_UIWindow **WindowRef =
        RR_GET_MAP_VALUE(&gContext->WindowMap, TitleHash, gContext->Arena);
    Rr_UIWindow *Window = *WindowRef;

    if (Window == NULL)
    {
        Window = RR_ALLOC_TYPE(gContext->Arena, Rr_UIWindow);
        Window->ZOrder = gContext->TotalWindowCount++;
        Window->Title = Rr_CreateString(Title, TitleLength, gContext->Arena);
        Window->Hash = TitleHash;
        Window->Rect.Offset = Rr_FloorV2(Rr_V2F(gContext->FontSize));
        Window->Rect.Extent = Rr_UIGetMinWindowSize(Flags);
        /* TODO: Wrapped text uses available width so we still need
         * some baseline width. Probably should come up with better solution. */
        /* Window->Rect.Extent.Width += 300.0f; */
        Window->SkipDueToAutoResize = true;
        *WindowRef = Window;
    }

    assert(
        Window->Added == false && "There already is a window with this title!");
    if (Rr_UIBeginWindowEx(Window, Open, Flags))
    {
        *RR_PUSH_INTO_ARRAY(&gContext->ActiveWindows, gContext->Arena) = Window;
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
            { BottomRight.X - gContext->ResizeHandleSize, BottomRight.Y },
            { BottomRight.X, BottomRight.Y - gContext->ResizeHandleSize },
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
            Window->Rect.Extent.Y += gContext->WindowTitleHeight;
        }
    }

    (void)RR_POP_FROM_ARRAY(&gContext->Stack);

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

    size_t TitleLength = strlen(Title);
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, TitleLength);

    Layout->SelectedTabRef =
        RR_GET_MAP_VALUE(&Window->WidgetMap, TitleHash, gContext->Arena);
    Layout->SelectedTab = *Layout->SelectedTabRef;
    Layout->TabCursor = Layout->Cursor;

    Rr_UIAdvance((Rr_Vec2){ 0.0f, gContext->LineHeight });

    Rr_Vec2 SeparatorSize = {
        Layout->AvailableContentsWidth,
        gContext->FrameThickness,
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
        &gContext->Style.Foreground);

    Rr_UIAdvance((Rr_Vec2){ 0.0f, Layout->ContentsPadding.Y });
}

bool Rr_UITab(const char *Title)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    assert(
        Layout->SelectedTabRef != NULL &&
        "Did you forget to call Rr_BeginTabs()?");
    Rr_UIWindow *Window = Layout->Window;

    Rr_UIQuad TabQuad = NULL;

    bool Selected = false;
    if (Layout->SelectedTab == NULL)
    {
        *Layout->SelectedTabRef = Layout->SelectedTab = Title;
        Selected = true;
    }
    else if (strcmp(Title, Layout->SelectedTab) == 0)
    {
        Selected = true;
    }

    TabQuad = Rr_UIReserveQuad();

    Rr_String TextString = Rr_CreateString(Title, 0, Scratch.Arena);
    Rr_Vec2 TextPosition = Layout->TabCursor;
    TextPosition.X += gContext->ButtonPadding.X;
    Rr_Vec2 TextSize = Rr_UIDrawText(
        0,
        TextPosition,
        &TextString,
        0.0f,
        Selected ? &gContext->Style.Background : &gContext->Style.Foreground,
        0);

    Rr_Vec2 ButtonPosition = TextPosition;
    ButtonPosition.X -= gContext->ButtonPadding.X;
    Rr_Vec2 ButtonSize = TextSize;
    ButtonSize.X += gContext->ButtonPadding.X * 2.0f;
    if (Selected)
    {
        ButtonSize.Y += gContext->FrameThickness;
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
        TabButtonColor = &gContext->Style.Foreground;
    }
    else if (Held)
    {
        TabButtonColor = &gContext->Style.ButtonHeld;
    }
    else if (Hovered)
    {
        TabButtonColor = &gContext->Style.ButtonHovered;
    }
    else
    {
        TabButtonColor = &gContext->Style.Background;
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
        *Layout->SelectedTabRef =
            Title; /* Newly selected tab will be rendered next frame. */
    }

    Rr_DestroyScratch(Scratch);

    return Selected;
}

void Rr_UIEndTabs(void)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    assert(
        Layout->SelectedTabRef != NULL &&
        "Did you forget to call Rr_UIBeginTabs()?");

    Layout->SelectedTabRef = NULL;
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

    Rr_UIQuad ButtonQuad = Rr_UIReserveQuad();

    size_t TitleLength = strlen(Title);
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, TitleLength);
    Rr_String TitleString = Rr_CreateString(Title, TitleLength, Scratch.Arena);

    bool **FoldValueRef =
        RR_GET_MAP_VALUE(&Window->WidgetMap, TitleHash, gContext->Arena);
    bool *FoldValue = *FoldValueRef;
    if (*FoldValueRef == NULL)
    {
        FoldValue = RR_ALLOC_TYPE(gContext->Arena, bool);
        *FoldValueRef = FoldValue;
    }

    float TriangleHeight = gContext->LineHeight * 0.575f;
    float TriangleBaseX = Layout->Cursor.X + Layout->ContentsPadding.Width;
    float TriangleBaseY =
        Layout->Cursor.Y + gContext->LineHeight -
        gContext->LineHeight *
            (gContext->Font->UnderlineY + gContext->Font->UnderlineThickness) -
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
    Rr_UIDrawSolidTriangle(TrianglePositions, &gContext->Style.Foreground);

    Rr_Vec2 TitlePosition = Rr_V2(
        TriangleBaseX + TriangleHeight + gContext->ButtonPadding.Width,
        Layout->Cursor.Y);
    Rr_UIDrawText(
        0,
        TitlePosition,
        &TitleString,
        0,
        &gContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize =
        Rr_V2(Layout->AvailableContentsWidth, gContext->LineHeight);

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
    Rr_Vec4 ButtonColor = gContext->Style.TitleBackground;
    if (Held)
    {
        ButtonColor.W *= 0.5f;
    }
    else if (Hovered)
    {
        ButtonColor.W *= 0.75f;
    }
    else
    {
        ButtonColor.W *= 0.85f;
    }
    Rr_UISolidQuad(ButtonQuad, &ButtonRect, &ButtonColor);

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
        gContext->FrameThickness,
    };
    Rr_Vec2 Position = {
        Layout->Cursor.X,
        Layout->Cursor.Y + (gContext->SeparatorLineHeight / 2.0f -
                            gContext->FrameThickness / 2.0f),
    };
    Rr_Vec4 Color = Rr_MulV4F(gContext->Style.Foreground, 0.75f);
    Rr_UIDrawSolidQuad(&(Rr_Rect){ Position, Size }, &Color);

    {
        Color.XYZ = Rr_MulV3F(Color.XYZ, 0.8f);
        Position.Y += Size.Height;
        Rr_UIDrawSolidQuad(&(Rr_Rect){ Position, Size }, &Color);
    }

    Layout->Cursor.Y += gContext->SeparatorLineHeight;
}

void Rr_UILabelEx(const char *Text, Rr_UITextFlags Flags)
{
    Rr_UIAssertWindow();

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    float AvailableContentsWidth =
        Window->ContentsEnd.X - Window->ContentsStart.X;

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextSize = Rr_UIDrawText(
        0,
        Layout->Cursor,
        &TextString,
        AvailableContentsWidth,
        &gContext->Style.Foreground,
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

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextSize = Rr_UIDrawText(
        0,
        Layout->Cursor,
        &TextString,
        0.0f,
        &gContext->Style.Foreground,
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

    char *Buffer = RR_ALLOC_NO_ZERO(Scratch.Arena, BufferSize + 1);

    va_start(Args, Format);
    BufferSize = vsnprintf(Buffer, BufferSize + 1, Format, Args);
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

    Rr_String TextString = Rr_CreateString(Text, 0, Scratch.Arena);
    Rr_Vec2 TextPosition = Rr_AddV2(ButtonPosition, gContext->ButtonPadding);
    Rr_Vec2 TextSize = Rr_UIDrawText(
        0,
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

    Rr_UIBevel(Primitive, &ButtonRect, &gContext->Style.ButtonNormal, Held);

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

    Rr_Vec2 CheckboxSize = { gContext->LineHeight, gContext->LineHeight };

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

    Rr_Vec4 BackgroundColor = gContext->Style.Background;
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
        Rr_UIDrawSolidQuad(&Inset, &gContext->Style.Foreground);
    }

    Rr_String TitleString = Rr_CreateString(Title, 0, Scratch.Arena);
    Rr_Vec2 TitlePosition = FramePosition;
    TitlePosition.X += CheckboxSize.X + gContext->ButtonPadding.Width;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        0,
        TitlePosition,
        &TitleString,
        0.0f,
        &gContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        TitleSize.Width + Layout->ContentsPadding.Width + gContext->LineHeight,
        gContext->LineHeight + Layout->ContentsPadding.Height,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return Up;
}

bool Rr_UIInputField(
    size_t BufferSize,
    char *Buffer,
    Rr_UIInputFieldFlags Flags)
{
    return false;
}

bool Rr_UICombobox(
    const char *Title,
    uint32_t OptionCount,
    const char **Options,
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

    size_t TitleLength = strlen(Title);
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, TitleLength);

    Rr_UIPrimitive Primitive = Rr_UIReserveBevel();

    Rr_String SelectedString =
        Rr_CreateString(Options[*SelectedIndex], 0, Scratch.Arena);
    Rr_Vec2 SelectedTextSize = Rr_UICalculateTextSize(
        gContext->Font,
        gContext->FontSize,
        &SelectedString,
        0.0f,
        0);

    Rr_Vec2 ButtonPosition = Layout->Cursor;

    Rr_Vec2 SelectedTextPosition = ButtonPosition;
    SelectedTextPosition.X += gContext->ButtonPadding.Width;
    Rr_UIDrawText(
        0,
        SelectedTextPosition,
        &SelectedString,
        0.0f,
        &gContext->Style.Foreground,
        0);

    Rr_Vec2 ButtonSize =
        Rr_AddV2(SelectedTextSize, Rr_MulV2F(gContext->ButtonPadding, 2.0f));
    ButtonSize.Height = gContext->LineHeight;

    Rr_Vec2 BorderSize = ButtonSize;
    BorderSize.X += gContext->LineHeight;

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
        gContext->PopupWindowParent = Window;
        gContext->PopupWindowHash = TitleHash;
    }

    bool OptionChanged = false;
    bool PopupOpen = gContext->PopupWindowParent == Window &&
                     gContext->PopupWindowHash == TitleHash;

    if (PopupOpen)
    {
        Rr_Vec2 PopupPosition = ButtonPosition;
        PopupPosition.Y += BorderSize.Height;
        PopupPosition.X += gContext->FrameThickness;
        Rr_UISetNextWindowPosition(PopupPosition);
        Rr_UISetNextWindowPadding(Rr_V2(gContext->ButtonPadding.Width, 0.0f));
        Rr_UIBeginPopupWindow();
        Rr_UILayout *PopupLayout = Rr_UICurrentLayout();
        for (uint32_t Index = 0; Index < OptionCount; ++Index)
        {
            Rr_UIQuad OptionButtonQuad = Rr_UIReserveQuad();
            Rr_String OptionString =
                Rr_CreateString(Options[Index], 0, Scratch.Arena);
            Rr_Vec2 OptionSize = Rr_UIDrawText(
                0,
                PopupLayout->Cursor,
                &OptionString,
                0,
                &gContext->Style.Foreground,
                0);
            Rr_Rect OptionButtonRect;
            OptionButtonRect.Offset.Y = PopupLayout->Cursor.Y;
            OptionButtonRect.Offset.X =
                PopupLayout->Cursor.X - gContext->ButtonPadding.Width;
            OptionButtonRect.Extent.Width =
                gContext->PopupWindow.Rect.Extent.Width;
            OptionButtonRect.Extent.Height = gContext->LineHeight;
            bool Up = false;
            bool Hovered = false;
            bool Held = false;
            Rr_UIButtonBehavior(
                &gContext->PopupWindow,
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
                OptionButtonColor = gContext->Style.ButtonHeld;
            }
            else if (Hovered)
            {
                OptionButtonColor = gContext->Style.ButtonHovered;
            }
            else
            {
                OptionButtonColor = gContext->Style.ButtonNormal;
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
    Rr_Vec4 BackgroundColor = gContext->Style.Background;
    BackgroundColor.XYZ = Rr_MulV3F(BackgroundColor.XYZ, 0.9f);

    Rr_UIBevel(Primitive, &ButtonRect, &BackgroundColor, false);

    /* Add handle. */
    {
        float HandleSize = ButtonSize.Height;
        Rr_Rect HandleRect = {
            {
                ButtonRect.Offset.X + ButtonRect.Extent.Width,
                ButtonRect.Offset.Y,
            },
            {
                HandleSize,
                HandleSize,
            },
        };
        HandleRect.Offset.X -= gContext->BevelThickness;

        Rr_UIDrawBevel(&HandleRect, &gContext->Style.ButtonNormal, Held);

        Rr_Vec2 HandleCenter = Rr_RectCenter(&HandleRect);
        Rr_Vec2 TrianglePositions[] = {
            Rr_AddV2(
                HandleCenter,
                Rr_MulV2F(Rr_V2F(-1.0f), gContext->LineHeight / 4.0f)),
            Rr_AddV2(
                HandleCenter,
                Rr_MulV2F(
                    (Rr_Vec2){ 1.0f, -1.0f },
                    gContext->LineHeight / 4.0f)),
            Rr_AddV2(
                HandleCenter,
                Rr_MulV2F(
                    (Rr_Vec2){ 0.0f, 1.0f },
                    gContext->LineHeight / 4.0f)),
        };
        Rr_UIDrawSolidTriangle(TrianglePositions, &gContext->Style.Foreground);
    }

    Rr_String TitleString = Rr_CreateString(Title, TitleLength, Scratch.Arena);
    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += gContext->ButtonPadding.Width + BorderSize.Width;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        0,
        TitlePosition,
        &TitleString,
        0.0f,
        &gContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        TitleSize.Width + gContext->ButtonPadding.Width + BorderSize.Width,
        gContext->LineHeight + Layout->ContentsPadding.Height,
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
    Position.X -= (gContext->ContentsPadding.Width + TargetSize) / 2.0f;
    Position.Y -= (gContext->ContentsPadding.Height + TargetSize) / 2.0f;
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
        Vertices[0].Position.X += Step * Index;
        Vertices[0].UV = Rr_V2F(0.0f);

        Vertices[1].Color = *ColorB;
        Vertices[1].Position = Layout->Cursor;
        Vertices[1].Position.X += Step * Index + Step;
        Vertices[1].UV = Rr_V2F(0.0f);

        Vertices[2].Color = *ColorA;
        Vertices[2].Position = Layout->Cursor;
        Vertices[2].Position.Y += TargetSize;
        Vertices[2].Position.X += Step * Index;
        Vertices[2].UV = Rr_V2F(0.0f);

        Vertices[3].Color = *ColorB;
        Vertices[3].Position = Layout->Cursor;
        Vertices[3].Position.X += Step * Index + Step;
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
        gContext->FrameThickness,
        &gContext->Style.Outline);

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

bool Rr_UIColorPicker(const char *Title, Rr_Vec4 *Color)
{
    Rr_UIAssertWindow();
    assert(Title != NULL);
    assert(Color != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength = strlen(Title);
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, TitleLength);

    Rr_Vec2 ColorBoxSize = Rr_V2F(gContext->LineHeight);

    Rr_Vec2 FramePosition = Layout->Cursor;

    bool Up = false;
    bool Hovered = false;
    bool Held = false;
    Rr_UIButtonBehavior(
        Window,
        &(Rr_Rect){
            FramePosition,
            ColorBoxSize,
        },
        NULL,
        &Up,
        &Hovered,
        &Held);

    if (Up)
    {
        gContext->PopupWindowParent = Window;
        gContext->PopupWindowHash = TitleHash;
    }

    bool ColorChanged = false;

    if (gContext->PopupWindowParent == Window &&
        gContext->PopupWindowHash == TitleHash)
    {
        Rr_Vec2 PopupCenter =
            Rr_AddV2(FramePosition, Rr_DivV2F(ColorBoxSize, 2.0f));
        Rr_UIColorPickerPopup(PopupCenter, Color);
    }

    Rr_Rect BevelRect = {
        FramePosition,
        ColorBoxSize,
    };
    Rr_UIDrawBevel(&BevelRect, Color, Held);

    Rr_String TitleString = Rr_CreateString(Title, TitleLength, Scratch.Arena);
    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += gContext->ButtonPadding.Width + ColorBoxSize.Width;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        0,
        TitlePosition,
        &TitleString,
        0.0f,
        &gContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        TitleSize.Width + Layout->ContentsPadding.Width + gContext->LineHeight,
        gContext->LineHeight + Layout->ContentsPadding.Height,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return ColorChanged;
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

    size_t TitleLength = strlen(Title);
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, TitleLength);
    Rr_String TitleString = Rr_CreateString(Title, TitleLength, Scratch.Arena);

    Rr_Vec2 TitleSize =
        Rr_UIDrawText(true, Layout->Cursor, &TitleString, 0.0f, NULL, 0);

    float SliderWidth = Layout->AvailableContentsWidth -
                        gContext->ButtonPadding.Width - TitleSize.Width;
    Rr_Rect SliderRect = {
        Layout->Cursor,
        {
            SliderWidth,
            gContext->LineHeight,
        },
    };

    Rr_UIDrawBevel(&SliderRect, &gContext->Style.ButtonDisabled, true);

    float HandleWidth = gContext->FontSize;
    Rr_Rect HandleRect = { Layout->Cursor,
                           Rr_V2(HandleWidth, gContext->LineHeight) };
    HandleRect.Offset.X += Normalized * (SliderWidth - HandleWidth);
    HandleRect = Rr_ResizeRect(&HandleRect, -gContext->BevelThickness);
    Rr_UIDrawBevel(&HandleRect, &gContext->Style.ButtonNormal, false);

    if (ValueCString != NULL)
    {
        Rr_String ValueString =
            Rr_CreateString(ValueCString, ValueCStringLength, Scratch.Arena);
        Rr_Vec2 ValueSize =
            Rr_UIDrawText(1, Rr_V2F(0.0f), &ValueString, 0.0f, NULL, 0);

        Rr_Vec2 ValuePosition = Layout->Cursor;
        ValuePosition.X = HandleRect.Offset.X + HandleWidth / 2.0f +
                          gContext->ButtonPadding.Width;
        if (ValuePosition.X + ValueSize.Width >
            SliderRect.Offset.X + SliderRect.Extent.Width)
        {
            ValuePosition.X = HandleRect.Offset.X -
                              gContext->ButtonPadding.Width - ValueSize.Width;
        }
        Rr_UIDrawText(
            0,
            ValuePosition,
            &ValueString,
            0.0f,
            &gContext->Style.Foreground,
            0);
    }

    float HandleDragOffset =
        gContext->MousePosition.X - (HandleRect.Offset.X + HandleWidth / 2.0f);

    bool Hovered, Dragging = Rr_UIDragBehavior(
                      Window,
                      &SliderRect,
                      RR_UI_DRAG_OP_WIDGET,
                      TitleHash,
                      Rr_V2(HandleDragOffset, 0.0f),
                      &Hovered);

    if (Dragging)
    {
        float SliderMin = Layout->Cursor.X + HandleWidth / 2.0f;
        float SliderMax = SliderMin + SliderWidth - HandleWidth;

        Normalized =
            (gContext->MousePosition.X - SliderMin) / (SliderMax - SliderMin);
        Normalized = RR_CLAMP(0.0f, Normalized, 1.0f);
    }
    else if (Hovered)
    {
        if (gContext->MouseWheelDelta.X != 0.0f)
        {
            /* NOTE: Probably shouldn't be hardcoded to 30.0f. */
            Normalized += gContext->MouseWheelDelta.X / 30.0f;
        }
    }

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += SliderWidth + gContext->ButtonPadding.Width;
    Rr_UIDrawText(
        0,
        TitlePosition,
        &TitleString,
        0.0f,
        &gContext->Style.Foreground,
        0);

    Rr_Vec2 TotalSize = {
        Layout->AvailableContentsWidth,
        gContext->LineHeight + Layout->ContentsPadding.Height,
    };

    Rr_UIAdvance(TotalSize);

    Rr_DestroyScratch(Scratch);

    return Normalized;
}

bool Rr_UISliderFloat(const char *Title, float *Value, float Min, float Max)
{
    assert(Value != NULL);
    assert(Max >= Min);

    char Buffer[32];
    int Length = snprintf(Buffer, 32, "%.4f", *Value);

    float In = RR_CLAMP(Min, *Value, Max);
    float InNormalized = (*Value - Min) / (Max - Min);
    float OutNormalized = Rr_UISlider(Title, InNormalized, Buffer, Length);
    float Out = OutNormalized * (Max - Min) + Min;
    *Value = Out;
    return false;
}

bool Rr_UIWantMouseCapture(void)
{
    return gContext &&
           (gContext->LeftMouseButtonDownOverWindow || gContext->HoveredWindow);
}

bool Rr_UIWantKeyboardCapture(void)
{
    return false;
}

void Rr_UIInit(void)
{
    assert(gContext == NULL);

    Rr_Renderer *Renderer = Rr_GetRenderer();

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gContext = RR_ALLOC_TYPE(Arena, Rr_UIContext);
    gContext->Arena = Arena;

    gContext->NextWindowPosition = Rr_V2F(INFINITY);
    gContext->NextWindowSize = Rr_V2F(INFINITY);
    gContext->NextWindowPadding = Rr_V2F(INFINITY);

    gContext->PopupWindow.Flags =
        RR_UI_WINDOW_FLAGS_NO_TITLE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_MINIMIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT | RR_UI_WINDOW_FLAGS_NO_MOVE_BIT |
        RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;

    gContext->NextFontSize =
        24.0f; /* TODO: Calculate default font size based on DPI. */

    gContext->Style = (Rr_UIStyle){
        .TitlePadding = { 0.5f, 0.125f },
        .ContentsPadding = { 0.5f, 0.5f },
        .BevelIntensityLight = 0.3f,
        .BevelIntensityDark = 0.7f,

        .Foreground = Rr_U32ToRGBA(0xD6D0B3FF),
        .Background = Rr_U32ToRGBA(0x292F33FA),
        .TitleBackground = Rr_U32ToRGBA(0xD54251FA),
        .Outline = Rr_U32ToRGBA(0x6C6F72FA),

        .ButtonNormal = Rr_U32ToRGBA(0x4c565dFF),
        .ButtonHovered = Rr_U32ToRGBA(0x687e8dFF),
        .ButtonHeld = Rr_U32ToRGBA(0x435866FF),
        .ButtonDisabled = Rr_U32ToRGBA(0x191e22FF),
    };

    gContext->Style.ScrollbarBackground = gContext->Style.ButtonDisabled;
    gContext->Style.ScrollbarNormal = gContext->Style.ButtonNormal;
    gContext->Style.ScrollbarHovered = gContext->Style.ButtonHovered;
    gContext->Style.ScrollbarHeld = gContext->Style.ButtonHeld;

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
    gContext->PipelineLayout = Rr_CreatePipelineLayout(
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
        .Layout = gContext->PipelineLayout,
        .VertexShaderSPV = Rr_LoadAsset(RR_BUILTIN_UI_VERT_SPV),
        .FragmentShaderSPV = Rr_LoadAsset(RR_BUILTIN_UI_FRAG_SPV),
        .ColorTargetCount = RR_ARRAY_COUNT(ColorTargets),
        .ColorTargets = ColorTargets,
        .VertexInputBindingCount = 1,
        .VertexInputBindings = &VertexInputBinding,
    };

    gContext->GraphicsPipeline =
        Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

    gContext->VertexBuffer = Rr_CreateBuffer(
        Renderer,
        RR_MEGABYTES(8),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    gContext->IndexBuffer = Rr_CreateBuffer(
        Renderer,
        RR_MEGABYTES(8),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    gContext->UniformBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(Rr_UIUniformData),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    gContext->Sampler = Rr_CreateSampler(
        Renderer,
        &(Rr_SamplerInfo){
            .MinFilter = RR_FILTER_LINEAR,
            .MagFilter = RR_FILTER_LINEAR,
        });

    gContext->Font = Rr_UICreateFont(
        gContext,
        RR_BUILTIN_SOURCESERIF4_PNG,
        RR_BUILTIN_SOURCESERIF4_JSON);
}

void Rr_UICleanup(void)
{
    assert(gContext != NULL);

    Rr_Renderer *Renderer = gApp->Renderer;
    Rr_DestroyBuffer(Renderer, gContext->VertexBuffer);
    Rr_DestroyBuffer(Renderer, gContext->IndexBuffer);
    Rr_DestroyBuffer(Renderer, gContext->UniformBuffer);
    Rr_DestroySampler(Renderer, gContext->Sampler);
    Rr_DestroyPipelineLayout(Renderer, gContext->PipelineLayout);
    Rr_DestroyGraphicsPipeline(Renderer, gContext->GraphicsPipeline);
    Rr_UIDestroyFont(gContext, gContext->Font);
    Rr_DestroyArena(gContext->Arena);
}

void Rr_UIProcessEvent(Rr_Event *Event)
{
    if (gContext == NULL)
    {
        return;
    }

    /* TODO: Set mouse position here. */

    switch (Event->Type)
    {
        case RR_EVENT_TYPE_MOUSE_BUTTON_DOWN:
        {
            if (Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                gContext->LeftMouseButtonDown = true;
                gContext->LeftMouseButtonHeld = true;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_UP:
        {
            if (Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
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

static inline void Rr_UIConsumeNextFontSize(void)
{
    if (gContext->NextFontSize != INFINITY)
    {
        gContext->FontSize = gContext->NextFontSize;
        gContext->NextFontSize = INFINITY;

        gContext->LineHeight = gContext->FontSize * gContext->Font->LineHeight;
        gContext->ContentsPadding =
            Rr_MulV2F(gContext->Style.ContentsPadding, gContext->FontSize);
        gContext->HorizontalMargin = gContext->FontSize * 0.5f;

        gContext->FrameThickness =
            floorf(RR_MAX(1.0f, gContext->FontSize * 0.075f));
        gContext->ResizeHandleSize = RR_UI_ROUND(gContext->FontSize);
        gContext->ScrollbarWidth = gContext->ResizeHandleSize;
        gContext->ScrollbarHandleWidth =
            RR_UI_ROUND(gContext->ResizeHandleSize * 0.75f);
        gContext->SeparatorLineHeight = gContext->LineHeight * 0.5f;
        gContext->ButtonPadding = (Rr_Vec2){ gContext->LineHeight * 0.25f,
                                             gContext->LineHeight * 0.125f };
        gContext->BevelThickness = ceilf(gContext->FontSize * 0.1f);

        gContext->WindowTitleHeight = RR_UI_ROUND(
            gContext->Style.TitlePadding.Y * gContext->FontSize * 2.0f +
            gContext->LineHeight);
        gContext->TitleButtonSize =
            RR_UI_ROUND(gContext->WindowTitleHeight * 0.8f);
        gContext->MinWindowSizeNoTitle =
            Rr_MulV2F(gContext->ContentsPadding, 2.0f);
        gContext->MinWindowSizeNoTitle.X += gContext->ScrollbarWidth;
        gContext->MinWindowSizeNoTitle.X += gContext->FontSize * 2.0f;
        gContext->MinWindowSizeNoTitle.Y += gContext->FontSize * 2.0f;
        gContext->MinWindowSizeNoTitle =
            RR_UI_ROUND_V2(gContext->MinWindowSizeNoTitle);
        gContext->MinWindowSize = gContext->MinWindowSizeNoTitle;
        gContext->MinWindowSize.Y += gContext->WindowTitleHeight;
        gContext->MinWindowSize = RR_UI_ROUND_V2(gContext->MinWindowSize);
    }
}

void Rr_UIBegin(void)
{
    assert(gContext != NULL);

    Rr_Renderer *Renderer = gApp->Renderer;

    gContext->FrameArena = Rr_GetFrameArena(Renderer);

    Rr_UIConsumeNextFontSize();

    gContext->MousePosition = Rr_GetMousePosition();
    if (gContext->SkipLeftMouseButtonUp && gContext->LeftMouseButtonUp)
    {
        gContext->SkipLeftMouseButtonUp = false;
        gContext->LeftMouseButtonUp = false;
    }

    gContext->HoveredWindow = NULL;
    if (gContext->PopupWindowParent)
    {
        if (Rr_RectContains(
                &gContext->PopupWindow.Rect,
                gContext->MousePosition))
        {
            gContext->HoveredWindow = &gContext->PopupWindow;

            if (gContext->LeftMouseButtonDown)
            {
                gContext->LeftMouseButtonDownOverWindow = true;
            }
        }
        else if (gContext->LeftMouseButtonDown)
        {
            Rr_UIClosePopupWindow();
            gContext->SkipLeftMouseButtonUp = true;
        }
    }
    else if (gContext->HoveredWindow == NULL)
    {
        int LastIndex = (int)gContext->ActiveWindows.Count - 1;
        for (int Index = LastIndex; Index >= 0; --Index)
        {
            Rr_UIWindow *Window = gContext->ActiveWindows.Data[Index];
            if (Rr_RectContains(&Window->Rect, gContext->MousePosition))
            {
                gContext->HoveredWindow = Window;

                if (gContext->LeftMouseButtonDown)
                {
                    Rr_UIWindow *HighestWindow =
                        gContext->ActiveWindows
                            .Data[gContext->ActiveWindows.Count - 1];
                    if (Window != HighestWindow)
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
    }

    RR_RESET_ARRAY(&gContext->Vertices, gContext->FrameArena);
    RR_RESET_ARRAY(&gContext->Indices, gContext->FrameArena);
    RR_RESET_ARRAY(&gContext->Stack, gContext->FrameArena);

    RR_CLEAR_ARRAY(&gContext->ActiveWindows);

    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
    gContext->ScreenSize.Width = (float)SwapchainSize.Width;
    gContext->ScreenSize.Height = (float)SwapchainSize.Height;
}

static inline int Rr_UIWindowSort(const void *A, const void *B)
{
    const Rr_UIWindow *WindowA = *(Rr_UIWindow **)A;
    const Rr_UIWindow *WindowB = *(Rr_UIWindow **)B;

    return WindowA->ZOrder > WindowB->ZOrder;
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

void Rr_UIEnd(void)
{
    Rr_UIAssertNoWindow();

    /* TODO: Consider active popup as well. */
    if (gContext->ActiveWindows.Count == 0)
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
        Rr_UIWindowSort);

    for (size_t Index = 0; Index < gContext->ActiveWindows.Count; ++Index)
    {
        Rr_UIDrawWindow(gContext->ActiveWindows.Data[Index], GraphicsNode);
    }

    if (gContext->PopupWindowParent)
    {
        Rr_UIDrawWindow(&gContext->PopupWindow, GraphicsNode);
    }

    if (gContext->LeftMouseButtonUp)
    {
        gContext->LeftMouseButtonHeld = false;
        gContext->LeftMouseButtonDownOverWindow = false;
    }
    gContext->LeftMouseButtonDown = false;
    gContext->LeftMouseButtonUp = false;
    gContext->MouseWheelDelta = Rr_V2F(0.0f);
    gContext->DragOpBeganThisFrame = false;
    gContext->DragOpEndedThisFrame = false;
}

void Rr_UISetFontSize(float Size)
{
    if (gContext)
    {
        gContext->NextFontSize = RR_CLAMP(8.0f, RR_UI_ROUND(Size), 96.0f);
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
    Rr_Renderer *Renderer = gApp->Renderer;

    if (Rr_UIBeginWindow(
            "Rr_DebugOverlay",
            NULL,
            RR_UI_WINDOW_FLAGS_NO_TITLE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
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
            Rr_UISeparator();
            uint32_t PresentModeCount;
            Rr_PresentMode *PresentModes =
                Rr_GetAvailablePresentModes(Renderer, &PresentModeCount);
            const char **PresentModeStrings =
                alloca(PresentModeCount * sizeof(const char *));
            uint32_t CurrentPresentModeIndex;
            for (uint32_t Index = 0; Index < PresentModeCount; ++Index)
            {
                if (Renderer->Swapchain.PresentMode == PresentModes[Index])
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
                Rr_SetPresentMode(
                    Renderer,
                    PresentModes[CurrentPresentModeIndex]);
            }
            Rr_UILabelF("FPS: %.2f", Rr_GetFramesPerSecond());
            Rr_UICheckbox(
                "Frame Limiter Enabled",
                &gApp->FrameTime.EnableFrameLimiter);
            Rr_UILabelF("Frame Limit: %d", gApp->FrameTime.TargetFramerate);
            if (Rr_UIButton("Toggle Fullscreen"))
            {
                Rr_ToggleFullscreen();
            }
        }
        if (Rr_UITab("Memory"))
        {
            Rr_UIDebugOverlayArena(gApp->Arena, "Application");
            Rr_UIDebugOverlayArena(gApp->Renderer->Arena, "Renderer");
            for (size_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
            {
                Rr_Frame *Frame = Renderer->Frames + Index;
                Rr_UIDebugOverlayArena(Frame->Arena, "Frame");
            }
            Rr_UIDebugOverlayArena(gContext->Arena, "UI");
        }
        if (Rr_UITab("Renderer"))
        {
            Rr_UILabelF("Frame: %zu", gApp->Renderer->FrameNumber);
            Rr_UILabelF(
                "RenderPasses: %zu",
                gApp->Renderer->RenderPasses.Count);
            Rr_UILabelF(
                "Framebuffers: %zu",
                gApp->Renderer->Framebuffers.Count);
            Rr_UILabelF(
                "SwapchainImages: %zu",
                gApp->Renderer->SwapchainImages.Count);
        }
        Rr_UIEndTabs();
        Rr_UIEndWindow();
    }
}
