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

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb/stb_truetype.h>

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

typedef struct Rr_UIUniformData Rr_UIUniformData;
struct Rr_UIUniformData
{
    Rr_Vec2 ScreenSize;
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
    Rr_UIWindowFlags Flags;
    Rr_Rect Rect;
    Rr_Rect VisibleRect;
    Rr_Vec2 ContentsStart;
    Rr_Vec2 ContentsEnd;
    float VScroll;
    float VScrollTarget;
    float HScroll;
    int32_t Z;

    bool Collapsed;
    bool Added;
    bool Open;
    bool Child;

    bool ShownAtLeastOnce;
    bool CreatedThisFrame;
    bool OpenedThisFrame;
    bool SkipThisFrame;

    float MaxFlexibleWidgetTitleWidth;
    float MaxFlexibleWidgetWidth;
    float MaxRigidWidth;

    Rr_Map *WidgetMap;
    Rr_Map *ChildWindowMap;

    Rr_UIWindow *TopLevelParent;
    Rr_UIClipRectArray *TopLevelClipRects;
    Rr_UIClipRect *CurrentClipRect;
    Rr_UIClipRect *ContentsClipRect;
};

typedef enum
{
    RR_UI_CLICK_TYPE_RELEASE,
    RR_UI_CLICK_TYPE_DOWN,
    RR_UI_CLICK_TYPE_DOWN_MULTI,
    RR_UI_CLICK_TYPE_DRAG,
    RR_UI_CLICK_TYPE_DRAG_MULTI,
} Rr_UIClickType;

typedef struct Rr_UILayout Rr_UILayout;
struct Rr_UILayout
{
    Rr_UIWindow *Window;

    Rr_Rect Rect;
    Rr_Vec2 Cursor;
    uint32_t AdvanceCount;

    Rr_Vec2 ReservedExtent;

    bool WasCollapsed;

    bool NextAdvanceFlexible;

    float HorizontalX;
    Rr_Vec2 HorizontalMaxExtent;

    Rr_Vec2 ContentsPadding;

    float AvailableContentsWidth;

    bool MouseInsideClipRect;

    Rr_Vec2 TabCursor;
    Rr_UIHash *SelectedTabHash;

    Rr_Vec2 DeferredWindowOffset;
    Rr_Vec2 DeferredWindowExtent;
    Rr_Vec4 DeferredResizeHandleColor;
    float DeferredMaxFlexibleWidgetTitleWidth;
    float DeferredMaxFlexibleWidgetWidth;
    float DeferredMaxRigidWidth;
    bool DeferredAutoResize;
    bool DeferredClampOffsetToScreen;
    bool LockOffset;
    bool LockExtent;

    Rr_UILayout *TopLevelParent;

    Rr_UILayout *Previous;
};

typedef struct Rr_UIMouseButton Rr_UIMouseButton;
struct Rr_UIMouseButton
{
    bool Down;
    bool DownOverWindow;
    bool Held;
    bool Up;
    bool SkipUp;
    uint32_t Clicks;
    uint32_t ClickID;
};

struct Rr_UIContext
{
    Rr_UIColors Colors;
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

    /* TODO: We need nice stack data structure for these. */

    Rr_UILayout *LayoutStack;
    RR_ARRAY(Rr_UIHash) HashStack;
    RR_ARRAY(uint32_t) FormatFloatDecimalPlacesStack;

    Rr_Vec2 NextWindowExtent;
    Rr_Vec2 NextWindowOffset;
    Rr_Vec2 NextWindowOpenOffset;
    Rr_Vec2 NextWindowPadding;
    int32_t NextWindowCreateCollapsed;

    Rr_UIMouseButton LeftMouseButton;
    bool MouseMoved;
    Rr_Vec2 MousePosition;
    Rr_Vec2 MouseWheelDelta;

    Rr_UIWindow *FocusedWindow;
    Rr_UIHash FocusedWidgetHash;
    Rr_UIWindow *PrevFocusedWindow;
    Rr_UIHash PrevFocusedWidgetHash;

    Rr_UIWindow *ClickWindow;
    Rr_UIHash ClickHash;
    Rr_Vec2 ClickMouseStart;
    Rr_Vec2 ClickWindowStart;
    bool ClickConsumed;

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

    RR_FREE_LIST(Rr_UIFont) Fonts;
    Rr_UIFont *Font;
    float FontSize;
    float NextFontSize;

    float LineHeight;
    Rr_Vec2 ContentsPadding;
    float ComponentMargin;
    float FlexibleTitleMargin;
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
    Rr_Vec2 InputFieldPadding;

    RR_ARRAY(Rr_UIVertex) Vertices;
    RR_ARRAY(Rr_UIIndex) Indices;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *LinearPipeline;
    Rr_GraphicsPipeline *SRGBPipeline;
    Rr_Buffer *VertexBuffer;
    Rr_Buffer *IndexBuffer;
    Rr_Buffer *UniformBuffer;
    Rr_Sampler *Sampler;

    bool VisualizeAdvances;

    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UIContext *gUIContext;

#define RR_UI_ROUND(Value) (ceilf((Value) / 2.0f) * 2.0f)

#define RR_UI_ROUND_V2(Value) \
    (Rr_V2(RR_UI_ROUND((Value).X), RR_UI_ROUND((Value).Y)))

typedef struct Rr_UIRange Rr_UIRange;
struct Rr_UIRange
{
    int32_t First;
    int32_t Last;
};

static const Rr_UIRange CodepointRanges[] = {
    { .First = 0x0020, .Last = 0x007F }, /* Basic Latin */
    { .First = 0x00A0, .Last = 0x00FF }, /* Latin-1 Supplement */
    { .First = 0x0100, .Last = 0x017F }, /* Latin Extended-A */
    { .First = 0x0180, .Last = 0x024F }, /* Latin Extended-B */
    { .First = 0x0400, .Last = 0x04FF }, /* Basic Cyrillic */
    { .First = 0x0500, .Last = 0x052F }, /* Cyrillic Supplementary */
};

typedef struct Rr_UIGlyph Rr_UIGlyph;
struct Rr_UIGlyph
{
    Rr_Vec2 Offset;
    Rr_Vec2 Size;
    Rr_Vec2 UVMin;
    Rr_Vec2 UVMax;
    float XAdvance;
};

typedef struct Rr_UIFontRange Rr_UIFontRange;
struct Rr_UIFontRange
{
    uint32_t First;
    uint32_t Last;
    Rr_UIGlyph *Glyphs;
};

struct Rr_UIFont
{
    /* TODO: Handle swapchains recreated with different color space. */
    bool CreatedForSRGBSwapchain;
    Rr_Image2D *Image;
    size_t RangeCount;
    Rr_UIFontRange *Ranges;
};

static inline Rr_UIGlyph *Rr_UIGetGlyphForCodepoint(
    Rr_UIFont *Font,
    uint32_t Codepoint)
{
    for (size_t Index = 0; Index < Font->RangeCount; ++Index)
    {
        Rr_UIFontRange *Range = &Font->Ranges[Index];
        if (Range->Last < Codepoint)
        {
            continue;
        }
        if (Range->First > Codepoint)
        {
            continue;
        }
        return Range->Glyphs + (Codepoint - Range->First);
    }
    return NULL;
}

Rr_UIFont *Rr_UICreateFont(Rr_AssetRef AssetRef, float FontSize)
{
    FontSize *= 1.25f;

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    static int FontIndex = -1;
    FontIndex++;

    const int32_t ATLAS_SIZE = 1024;
    const Rr_IntVec2 ATLAS_EXTENT = { ATLAS_SIZE, ATLAS_SIZE };

    unsigned char *GrayscaleBuffer =
        RR_ALLOC_NO_ZERO(Scratch.Arena, (size_t)(ATLAS_SIZE * ATLAS_SIZE));

    Rr_Asset FontAsset = Rr_LoadAsset(AssetRef);
    stbtt_fontinfo FontInfo;
    if (!stbtt_InitFont(&FontInfo, (unsigned char *)FontAsset.Pointer, 0))
    {
        RR_LOG("Failed to parse .ttf file!");
        Rr_DestroyScratch(Scratch);
        return NULL;
    }

    stbtt_pack_context PackContext;
    if (!stbtt_PackBegin(
            &PackContext,
            GrayscaleBuffer,
            ATLAS_SIZE,
            ATLAS_SIZE,
            0,
            2,
            NULL))
    {
        RR_LOG("Failed to begin .ttf packing!");
        Rr_DestroyScratch(Scratch);
        return NULL;
    }

    stbtt_PackSetOversampling(&PackContext, 2, 2);

    size_t RangeCount = RR_ARRAY_COUNT(CodepointRanges);

    bool IsSRGBSwapchain =
        Rr_IsSRGBFormat(Rr_GetImageFormat(Rr_GetSwapchainImage()));
    Rr_ImageFormat ImageFormat = RR_IMAGE_FORMAT_R8G8B8A8_SRGB;

    Rr_UIFont *Font = RR_ALLOC_TYPE(gUIContext->Arena, Rr_UIFont);
    Font->CreatedForSRGBSwapchain = IsSRGBSwapchain;
    char FontNameBuffer[64];
    snprintf(
        FontNameBuffer,
        sizeof(FontNameBuffer),
        "Rr.UI.Font#%d",
        FontIndex);
    Rr_SetNextObjectName(FontNameBuffer);
    Font->Image = Rr_CreateImage2D(
        ATLAS_EXTENT,
        ImageFormat,
        RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
    Font->RangeCount = RangeCount;
    /* TODO: Could be single allocation. */
    Font->Ranges =
        RR_ALLOC_TYPE_COUNT(gUIContext->Arena, Rr_UIFontRange, RangeCount);

    stbtt_pack_range *PackRanges =
        RR_ALLOC_NO_ZERO(Scratch.Arena, RangeCount * sizeof(stbtt_pack_range));

    size_t TotalCharCount = 0;
    for (size_t Index = 0; Index < RangeCount; ++Index)
    {
        int32_t NumChars =
            CodepointRanges[Index].Last - CodepointRanges[Index].First;

        Font->Ranges[Index].First = (uint32_t)CodepointRanges[Index].First;
        Font->Ranges[Index].Last = (uint32_t)CodepointRanges[Index].Last;
        Font->Ranges[Index].Glyphs = RR_ALLOC_NO_ZERO(
            gUIContext->Arena,
            (size_t)NumChars * sizeof(Rr_UIGlyph));

        PackRanges[Index] = (stbtt_pack_range){
            .first_unicode_codepoint_in_range = CodepointRanges[Index].First,
            .num_chars = NumChars,
            .font_size = FontSize,
            .chardata_for_range = RR_ALLOC_NO_ZERO(
                Scratch.Arena,
                (size_t)NumChars * sizeof(stbtt_packedchar)),
        };

        TotalCharCount += (size_t)NumChars;
    }

    stbrp_rect *Rects =
        RR_ALLOC_NO_ZERO(Scratch.Arena, sizeof(stbrp_rect) * TotalCharCount);

    int NumRects = stbtt_PackFontRangesGatherRects(
        &PackContext,
        &FontInfo,
        PackRanges,
        (int32_t)RangeCount,
        Rects);
    stbtt_PackFontRangesPackRects(&PackContext, Rects, NumRects);
    stbtt_PackFontRangesRenderIntoRects(
        &PackContext,
        &FontInfo,
        PackRanges,
        (int32_t)RangeCount,
        Rects);
    stbtt_PackEnd(&PackContext);

    /* Create glyph data. */

    float FontScale = stbtt_ScaleForPixelHeight(&FontInfo, FontSize);

    int UnscaledAscent, UnscaledDescent, UnscaledLineGap;
    stbtt_GetFontVMetrics(
        &FontInfo,
        &UnscaledAscent,
        &UnscaledDescent,
        &UnscaledLineGap);

    float Ascent = (float)UnscaledAscent * FontScale;
    float Descent = (float)UnscaledDescent * FontScale;
    float LineGap = (float)UnscaledLineGap * FontScale;

    for (size_t Index = 0; Index < RangeCount; ++Index)
    {
        stbtt_pack_range *PackRange = PackRanges + Index;
        Rr_UIFontRange *FontRange = Font->Ranges + Index;
        size_t GlyphCount = FontRange->Last - FontRange->First;
        for (size_t GlyphIndex = 0; GlyphIndex < GlyphCount; ++GlyphIndex)
        {
            Rr_UIGlyph *Glyph = &FontRange->Glyphs[GlyphIndex];

            stbtt_packedchar *PackedChar =
                PackRange->chardata_for_range + GlyphIndex;

            float X = 0, Y = 0;
            stbtt_aligned_quad Quad;
            stbtt_GetPackedQuad(
                PackRange->chardata_for_range,
                ATLAS_SIZE,
                ATLAS_SIZE,
                (int32_t)GlyphIndex,
                &X,
                &Y,
                &Quad,
                0);
            Glyph->Size = (Rr_Vec2){ Quad.x1 - Quad.x0, Quad.y1 - Quad.y0 };
            Glyph->Offset = (Rr_Vec2){ Quad.x0, Quad.y0 + Ascent };
            Glyph->UVMin = (Rr_Vec2){ Quad.s0, Quad.t0 };
            Glyph->UVMax = (Rr_Vec2){ Quad.s1, Quad.t1 };
            Glyph->XAdvance = PackedChar->xadvance;
        }
    }

    /* Fill RGBA atlas. */

    size_t AtlasBufferSize =
        (size_t)(ATLAS_SIZE * ATLAS_SIZE) * sizeof(uint32_t);
    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        AtlasBufferSize,
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT);
    uint32_t *StagingData = Rr_GetMappedBufferData(StagingBuffer);
    for (int32_t Y = 0; Y < ATLAS_SIZE; ++Y)
    {
        for (int32_t X = 0; X < ATLAS_SIZE; ++X)
        {
            uint8_t Grayscale = GrayscaleBuffer[Y * ATLAS_SIZE + X];
            if (IsSRGBSwapchain)
            {
                Grayscale =
                    (uint8_t)(Rr_ToSRGBChannel((float)Grayscale / 255.0f) *
                              255.0f);
            }
            StagingData[Y * ATLAS_SIZE + X] =
                ((uint32_t)Grayscale << 24) | 0x00FFFFFF;
        }
    }
    StagingData[0] = 0xFFFFFFFF; /* Opaque pixel at [0,0]. */
    Rr_FlushBufferRange(StagingBuffer, 0, AtlasBufferSize);
    Rr_CopyBufferToImage2D(
        Rr_GetGraph(),
        StagingBuffer,
        0,
        ATLAS_EXTENT,
        Font->Image,
        0);

    Rr_ReleaseBuffer(StagingBuffer);

    Rr_DestroyScratch(Scratch);

    return Font;
}

static inline void Rr_UIReleaseFont(Rr_UIFont *Font)
{
    Rr_ReleaseImage(Font->Image);
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

static inline bool Rr_UIWindowNoBorders(Rr_UIWindow *Window)
{
    return RR_HAS_BIT(Window->Flags, RR_UI_WINDOW_FLAGS_NO_BORDERS_BIT);
}

static inline bool Rr_UIWindowNoVerticalScrollbar(Rr_UIWindow *Window)
{
    return RR_HAS_BIT(
        Window->Flags,
        RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT);
}

static inline Rr_UIHash Rr_UICurrentHash(void)
{
    return gUIContext->HashStack.Count > 0
               ? RR_LAST_ARRAY_ELEMENT(&gUIContext->HashStack)
               : 0;
}

static inline Rr_UIHash Rr_UIGetHash(
    size_t Size,
    const void *Data,
    Rr_UIHash Seed)
{
    return XXH3_64bits_withSeed(Data, Size, ~Seed);
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

        Hash = Rr_UIGetHash(IDLength, ExplicitID, Rr_UICurrentHash());

        if (OutLength)
        {
            *OutLength = FullLength - IDLength - 3;
        }
    }
    else
    {
        Hash = Rr_UIGetHash(FullLength, CString, Rr_UICurrentHash());

        if (OutLength)
        {
            *OutLength = FullLength;
        }
    }

    return Hash;
}

static inline void Rr_UIPushIDHash(Rr_UIHash Hash)
{
    *RR_PUSH_INTO_ARRAY(&gUIContext->HashStack, gUIContext->Arena) = Hash;
}

void Rr_UIPushID(const char *IDString)
{
    Rr_UIHash Hash =
        Rr_UIGetHash(strlen(IDString), IDString, Rr_UICurrentHash());
    Rr_UIPushIDHash(Hash);
}

void Rr_UIPopID(void)
{
    assert(
        gUIContext->HashStack.Count && "Did you forget to call Rr_UIPushID()?");
    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->HashStack));
}

static inline Rr_UILayout *Rr_UICurrentLayout(void)
{
    return gUIContext->LayoutStack;
}

static inline Rr_UILayout *Rr_UIPushLayout(
    Rr_UIHash Hash,
    Rr_UIWindow *Window,
    Rr_Vec2 ContentsPadding)
{
    /* TODO: Sort these. */
    Rr_UILayout *Layout = RR_ALLOC(gUIContext->FrameArena, sizeof(Rr_UILayout));
    *Layout = (Rr_UILayout){
        .WasCollapsed = Window->Collapsed,
        .Window = Window,
        .HorizontalX = INFINITY,
        .DeferredWindowOffset = Rr_V2F(INFINITY),
        .DeferredWindowExtent = Rr_V2F(INFINITY),
        .DeferredAutoResize = Rr_UIWindowAutoResize(Window),
        .ContentsPadding = ContentsPadding,
        .TopLevelParent =
            Window->Child ? gUIContext->LayoutStack->TopLevelParent : Layout,
        .Cursor = Window->Rect.Offset,
        .AvailableContentsWidth = Window->Rect.Extent.Width,
        .Previous = gUIContext->LayoutStack,
        .Rect = Window->Rect,
    };
    gUIContext->LayoutStack = Layout;
    Rr_UIPushIDHash(Hash);
    return Layout;
}

static inline void Rr_UIPopLayout(void)
{
    Rr_UIPopID();
    assert(gUIContext->LayoutStack);
    gUIContext->LayoutStack = gUIContext->LayoutStack->Previous;
}

static inline Rr_UIWindow *Rr_UICurrentWindow(void)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    return Layout ? Layout->Window : NULL;
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

static inline void Rr_UIAssertNoHorizontal(Rr_UILayout *Layout)
{
    assert(
        Layout->HorizontalX == INFINITY &&
        "Did you forget to call Rr_UIEndHorizontal()?");
}

Rr_UIPrimitive Rr_UIReservePrimitive(size_t VertexCount, size_t IndexCount)
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

void Rr_UIDrawTriangleVertices(Rr_UIVertex *Vertices)
{
    Rr_UIPrimitive Primitive = Rr_UIReservePrimitive(3, 3);

    memcpy(Primitive.Vertices, Vertices, sizeof(Rr_UIVertex) * 3);

    Primitive.Indices[0] = Primitive.BaseVertex;
    Primitive.Indices[1] = Primitive.BaseVertex + 1;
    Primitive.Indices[2] = Primitive.BaseVertex + 2;

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 8.0f);
}

void Rr_UIDrawTriangleFilled(Rr_Vec2 *Positions, Rr_Vec4 *Color)
{
    Rr_UIPrimitive Primitive = Rr_UIReservePrimitive(3, 3);

    Primitive.Vertices[0] = (Rr_UIVertex){
        .Position = Positions[0],
        .Color = *Color,
    };
    Primitive.Vertices[1] = (Rr_UIVertex){
        .Position = Positions[1],
        .Color = *Color,
    };
    Primitive.Vertices[2] = (Rr_UIVertex){
        .Position = Positions[2],
        .Color = *Color,
    };

    Primitive.Indices[0] = Primitive.BaseVertex;
    Primitive.Indices[1] = Primitive.BaseVertex + 1;
    Primitive.Indices[2] = Primitive.BaseVertex + 2;

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 8.0f);
}

void Rr_UIDrawFitTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 *Color)
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
    Primitive.Vertices[0].Color = *Color;

    Primitive.Vertices[1].Position = Rr_AddV2(Positions[1], Offset);
    Primitive.Vertices[1].UV = Rr_V2F(0.0f);
    Primitive.Vertices[1].Color = *Color;

    Primitive.Vertices[2].Position = Rr_AddV2(Positions[2], Offset);
    Primitive.Vertices[2].UV = Rr_V2F(0.0f);
    Primitive.Vertices[2].Color = *Color;

    Primitive.Indices[0] = Primitive.BaseVertex;
    Primitive.Indices[1] = Primitive.BaseVertex + 1;
    Primitive.Indices[2] = Primitive.BaseVertex + 2;

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 4.0f);
}

void Rr_UIDrawEquilateralTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 *Color)
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
    Primitive.Vertices[0].Color = *Color;

    Primitive.Vertices[1].Position = Rr_AddV2(Positions[1], Offset);
    Primitive.Vertices[1].UV = Rr_V2F(0.0f);
    Primitive.Vertices[1].Color = *Color;

    Primitive.Vertices[2].Position = Rr_AddV2(Positions[2], Offset);
    Primitive.Vertices[2].UV = Rr_V2F(0.0f);
    Primitive.Vertices[2].Color = *Color;

    Primitive.Indices[0] = Primitive.BaseVertex;
    Primitive.Indices[1] = Primitive.BaseVertex + 1;
    Primitive.Indices[2] = Primitive.BaseVertex + 2;

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 4.0f);
}

void Rr_UIDrawCircle(
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

void Rr_UIDrawCircleFilled(Rr_Vec2 Offset, float Radius, Rr_Vec4 *Color)
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

void Rr_UIDrawQuadVertices(Rr_UIVertex *Vertices)
{
    Rr_UIPrimitive Primitive = Rr_UIReserveQuad();
    memcpy(Primitive.Vertices, Vertices, sizeof(Rr_UIVertex) * 4);
}

static inline void Rr_UISolidQuad(
    Rr_UIVertex *Vertices,
    Rr_Rect *Rect,
    const Rr_Vec4 *Color)
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

static inline void Rr_UIDrawSolidQuad(Rr_Rect *Rect, const Rr_Vec4 *Color)
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

    Rr_UIDrawQuadVertices(Vertices);
}

static inline void Rr_UIDrawCheckerQuad(Rr_Rect *Rect, float Size)
{
    static const Rr_Vec4 WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };
    static const Rr_Vec4 GRAY = { 0.15f, 0.15f, 0.15f, 1.0f };

    Rr_Rect CurrectRect = {
        .Offset = Rect->Offset,
        .Extent = Rr_V2F(Size),
    };

    float XCount;
    float XFrac = modff(Rect->Extent.X / Size, &XCount);
    float YCount;
    float YFrac = modff(Rect->Extent.Y / Size, &YCount);
    int X = 0;
    int Y = 0;
    for (Y = 0; Y < (int)YCount; ++Y)
    {
        CurrectRect.Offset.X = Rect->Offset.X;
        for (X = 0; X < (int)XCount; ++X)
        {
            Rr_UIDrawSolidQuad(&CurrectRect, X % 2 != Y % 2 ? &WHITE : &GRAY);
            CurrectRect.Offset.X += Size;
        }
        CurrectRect.Offset.Y += Size;
    }

    CurrectRect.Offset.Y = Rect->Offset.Y;
    CurrectRect.Extent.X = Size * XFrac;
    CurrectRect.Extent.Y = Size;
    for (Y = 0; Y < (int)YCount; ++Y)
    {
        Rr_UIDrawSolidQuad(&CurrectRect, X % 2 != Y % 2 ? &WHITE : &GRAY);
        CurrectRect.Offset.Y += Size;
    }

    CurrectRect.Offset.X = Rect->Offset.X;
    CurrectRect.Extent.X = Size;
    CurrectRect.Extent.Y = Size * YFrac;
    for (X = 0; X < (int)XCount; ++X)
    {
        Rr_UIDrawSolidQuad(&CurrectRect, X % 2 != Y % 2 ? &WHITE : &GRAY);
        CurrectRect.Offset.X += Size;
    }

    CurrectRect.Extent.X = Size * XFrac;
    CurrectRect.Extent.Y = Size * YFrac;
    Rr_UIDrawSolidQuad(&CurrectRect, X % 2 != Y % 2 ? &WHITE : &GRAY);
}

static inline void Rr_UIDrawGlyph(
    Rr_UIGlyph *Glyph,
    Rr_Vec2 Position,
    Rr_Vec4 *Color)
{
    Rr_Vec2 UVs[4] = {
        { Glyph->UVMin.X, Glyph->UVMin.Y },
        { Glyph->UVMax.X, Glyph->UVMin.Y },
        { Glyph->UVMin.X, Glyph->UVMax.Y },
        { Glyph->UVMax.X, Glyph->UVMax.Y },
    };

    Rr_UIDrawTexturedQuad(
        &(Rr_Rect){
            Rr_AddV2(Position, Glyph->Offset),
            Glyph->Size,
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
    RR_BEGIN_FRAME_SECTION("Rr.UI.DrawInputText");

    Rr_UIFont *Font = gUIContext->Font;
    float FontSize = gUIContext->FontSize;
    float LineHeight = gUIContext->LineHeight;
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

        Rr_UIGlyph *Glyph = Rr_UIGetGlyphForCodepoint(Font, Codepoint);

        if (Active)
        {
            if ((OldCursorMin != OldCursorMax) &&
                (CodepointIndex >= OldCursorMin &&
                 CodepointIndex < OldCursorMax))
            {
                Rr_UIDrawRect(
                    &(Rr_Rect){
                        GlyphPosition,
                        Rr_V2(Glyph->XAdvance, gUIContext->LineHeight),
                    },
                    &gUIContext->Colors.SelectedTextBackground);
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

        if (!Glyph)
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
                Rr_UIDrawGlyph(Glyph, GlyphPosition, Color);
            }

            CurrentX += Glyph->XAdvance;
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

    RR_END_FRAME_SECTION("Rr.UI.DrawInputText");

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

    RR_BEGIN_FRAME_SECTION("Rr.UI.DrawText");

    bool NullTerminated = false;
    if (UTF8StringLength == SIZE_MAX)
    {
        NullTerminated = true;
    }

    Rr_UIFont *Font = gUIContext->Font;
    float FontSize = gUIContext->FontSize;
    float LineHeight = gUIContext->LineHeight;
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

                            Rr_UIGlyph *Glyph =
                                Rr_UIGetGlyphForCodepoint(Font, Codepoint);

                            if (!CalculateOnly)
                            {
                                Rr_UIDrawGlyph(
                                    Glyph,
                                    Rr_AddV2(
                                        Position,
                                        (Rr_Vec2){ CurrentX, CurrentY }),
                                    Color);
                            }
                            CurrentX += Glyph->XAdvance;
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
                            Rr_UIGlyph *Glyph =
                                Rr_UIGetGlyphForCodepoint(Font, Codepoint);
                            if (!CalculateOnly)
                            {
                                Rr_UIDrawGlyph(Glyph, PositionInWord, Color);
                            }
                            CurrentX += Glyph->XAdvance;
                            PositionInWord.X = Position.X + CurrentX;
                        }
                    }
                }
                else
                {
                    Rr_UIGlyph *Glyph =
                        Rr_UIGetGlyphForCodepoint(Font, Codepoint);
                    CurrentX += Glyph->XAdvance;
                }

                MaxX = RR_MAX(MaxX, CurrentX);

                CurrentWordWidth = 0.0f;
                CurrentWordStart = CodepointIndex + 1;
            }
            else
            {
                Rr_UIGlyph *Glyph = Rr_UIGetGlyphForCodepoint(Font, Codepoint);
                CurrentWordWidth += Glyph->XAdvance;
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

            Rr_UIGlyph *Glyph = Rr_UIGetGlyphForCodepoint(Font, Codepoint);

            if (Codepoint == '\n')
            {
                CurrentX = 0.0f;
                CurrentY += LineHeight;
                continue;
            }

            if (Codepoint == ' ')
            {
                CurrentX += Glyph->XAdvance;
                continue;
            }

            if (!CalculateOnly)
            {
                Rr_UIDrawGlyph(
                    Glyph,
                    Rr_AddV2(Position, (Rr_Vec2){ CurrentX, CurrentY }),
                    Color);
            }

            CurrentX += Glyph->XAdvance;
            MaxX = RR_MAX(MaxX, CurrentX);
        }
    }

    RR_END_FRAME_SECTION("Rr.UI.DrawText");

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

static inline void Rr_UISetClick(
    Rr_UIWindow *Window,
    Rr_UIHash Hash,
    Rr_Vec2 WindowStart)
{
    gUIContext->ClickMouseStart = gUIContext->MousePosition;
    gUIContext->ClickWindow = Window;
    gUIContext->ClickWindowStart = WindowStart;
    gUIContext->ClickHash = Hash;
}

static inline void Rr_UIResetClick(void)
{
    gUIContext->ClickWindow = NULL;
    gUIContext->ClickHash = 0;
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
        gUIContext->ClickConsumed == false &&
        Rr_RectContains(Rect, gUIContext->MousePosition))
    {
        if (gUIContext->MouseWheelDelta.Y != 0.0f)
        {
            Rr_UIResetClick();
            *YScroll = *YScroll +
                       gUIContext->MouseWheelDelta.Y * gUIContext->LineHeight;

            return true;
        }
    }

    return false;
}

typedef struct Rr_UIClickResult Rr_UIClickResult;
struct Rr_UIClickResult
{
    bool Moved : 1;
    bool Held : 1;
    bool Hovered : 1;
    uint8_t ClickCount;
};

static inline Rr_UIClickResult Rr_UIClickEx(
    Rr_UILayout *Layout,
    Rr_Rect *Rect,
    Rr_UIClickType Type,
    Rr_UIHash Hash,
    Rr_Vec2 Value)
{
    Rr_UIWindow *Window = Layout->Window;

    bool WindowHovered = Window == gUIContext->HoveredWindow;
    bool WindowMatch = Window == gUIContext->ClickWindow;
    bool HashMatch = Hash == gUIContext->ClickHash;

    bool Contains = Layout->MouseInsideClipRect &&
                    Rr_RectContains(Rect, gUIContext->MousePosition);

    Rr_UIClickResult ClickResult = { 0 };
    ClickResult.Hovered = WindowHovered && Contains;

    if (gUIContext->ClickConsumed)
    {
        return ClickResult;
    }

    if (gUIContext->LeftMouseButton.Down)
    {
        uint8_t ClickCount =
            (uint8_t)RR_CLAMP(1, gUIContext->LeftMouseButton.Clicks, UCHAR_MAX);

        if (Contains && WindowHovered)
        {
            if (Type == RR_UI_CLICK_TYPE_DOWN_MULTI)
            {
                if (ClickCount > 1)
                {
                    ClickResult.ClickCount = ClickCount;

                    Rr_UISetClick(Window, Hash, Value);
                    Rr_UISetFocus(Window, Hash);

                    gUIContext->ClickConsumed = true;
                }

                return ClickResult;
            }

            if (Type == RR_UI_CLICK_TYPE_DRAG_MULTI)
            {
                if (ClickCount > 1)
                {
                    ClickResult.ClickCount = ClickCount;
                }
            }
            else if (Type != RR_UI_CLICK_TYPE_RELEASE)
            {
                ClickResult.ClickCount = 1;
            }

            Rr_UISetClick(Window, Hash, Value);
            Rr_UISetFocus(Window, Hash);

            gUIContext->ClickConsumed = true;
        }

        return ClickResult;
    }

    if (gUIContext->LeftMouseButton.Up && Contains && WindowHovered)
    {
        if (Type == RR_UI_CLICK_TYPE_RELEASE)
        {
            if (WindowMatch && HashMatch)
            {
                ClickResult.ClickCount = 1;
                ClickResult.Moved = gUIContext->MouseMoved;

                gUIContext->ClickConsumed = true;
            }
        }

        return ClickResult;
    }

    if (gUIContext->LeftMouseButton.Held && WindowMatch && HashMatch)
    {
        ClickResult.Held = true;
        ClickResult.Moved = gUIContext->MouseMoved;

        return ClickResult;
    }

    return ClickResult;
}

static inline Rr_UIClickResult Rr_UIClickDrag(
    Rr_UILayout *Layout,
    Rr_Rect *Rect,
    Rr_UIHash Hash,
    Rr_Vec2 Value)
{
    return Rr_UIClickEx(Layout, Rect, RR_UI_CLICK_TYPE_DRAG, Hash, Value);
}

static inline Rr_UIClickResult Rr_UIClickSimple(
    Rr_UILayout *Layout,
    Rr_Rect *Rect,
    Rr_UIHash Hash)
{
    return Rr_UIClickEx(
        Layout,
        Rect,
        RR_UI_CLICK_TYPE_RELEASE,
        Hash,
        Rr_V2F(0.0f));
}

static inline Rr_UIClickResult Rr_UIClickDouble(
    Rr_UILayout *Layout,
    Rr_Rect *Rect,
    Rr_UIHash Hash)
{
    return Rr_UIClickEx(
        Layout,
        Rect,
        RR_UI_CLICK_TYPE_DOWN_MULTI,
        Hash,
        Rr_V2F(0.0f));
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

static inline void Rr_UIBeginClipRect(Rr_Rect *Rect)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_UIClipRect *ClipRect =
        RR_PUSH_INTO_ARRAY(Window->TopLevelClipRects, gUIContext->FrameArena);
    ClipRect->FirstIndex = (uint32_t)gUIContext->Indices.Count;
    ClipRect->Rect = *Rect;

    Window->CurrentClipRect = ClipRect;
    Layout->MouseInsideClipRect =
        Rr_RectContains(&ClipRect->Rect, gUIContext->MousePosition);
}

static inline void Rr_UIBeginVisibleClipRect(Rr_Rect *Rect)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Rect VisibleRect = Rr_UIRectIntersection(Rect, &Window->VisibleRect);

    Rr_UIBeginClipRect(&VisibleRect);
}

static inline void Rr_UIEndClipRect(void)
{
    Rr_UIWindow *Window = Rr_UICurrentWindow();
    if (Window)
    {
        if (Window->TopLevelClipRects->Count > 0)
        {
            Rr_UIClipRect *Last =
                &RR_LAST_ARRAY_ELEMENT(Window->TopLevelClipRects);
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

Rr_Vec2 Rr_UIGetCursor(void)
{
    Rr_UIAssertWindow();
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    return Layout->Cursor;
}

void Rr_UIAdvance(Rr_Vec2 Size)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    if (gUIContext->VisualizeAdvances)
    {
        Rr_UIHash Hash = Rr_UIGetHash(
            sizeof(Layout->AdvanceCount),
            &Layout->AdvanceCount,
            Rr_UICurrentHash());
        Rr_Vec4 Color = Rr_U32ToRGBA((uint32_t)Hash);
        Color.A = 0.1f;
        Rr_Rect Rect = {
            .Offset = Layout->Cursor,
            .Extent = Size,
        };
        Rr_UIDrawRect(&Rect, &Color);
        Layout->AdvanceCount++;
    }

    /* TODO: Can contents be updated later when ending the window? */

    if (Rr_UIIsHorizontal())
    {
        Layout->HorizontalMaxExtent.Y =
            RR_MAX(Layout->HorizontalMaxExtent.Y, Size.Height);
        Layout->HorizontalMaxExtent.X = RR_MAX(
            Layout->HorizontalMaxExtent.X,
            Layout->Cursor.X + Size.X - Layout->HorizontalX);

        Layout->Cursor.X += Size.Width + Layout->ContentsPadding.Width;

        /* Horizontal mode is expected to be disabled when ending a window so
         * these seem unimportant?
         * Basically I have to decide whether ContentsStart/End can be used
         * directly when calculating something.*/

        Window->ContentsEnd.X = RR_MAX(
            Window->ContentsEnd.X,
            Layout->HorizontalX + Layout->HorizontalMaxExtent.X);
        Window->ContentsEnd.Y = RR_MAX(
            Window->ContentsEnd.Y,
            Layout->Cursor.Y + Layout->HorizontalMaxExtent.Y);
    }
    else
    {
        Layout->Cursor.Y += Size.Height + Layout->ContentsPadding.Height;

        Window->ContentsEnd.Y = RR_MAX(Window->ContentsEnd.Y, Layout->Cursor.Y);
        Window->ContentsEnd.X =
            RR_MAX(Window->ContentsEnd.X, Layout->Cursor.X + Size.X);

        if (!Layout->NextAdvanceFlexible)
        {
            Layout->DeferredMaxRigidWidth =
                RR_MAX(Layout->DeferredMaxRigidWidth, Size.X);
        }
        else
        {
            Layout->NextAdvanceFlexible = false;
        }
    }
}

static inline bool Rr_UIAddCollapseButton(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

    /* Assuming having a title bar. */

    Rr_Rect ButtonRect;
    ButtonRect.Offset = Layout->Rect.Offset;
    ButtonRect.Extent = Rr_V2F(gUIContext->TitleButtonSize);

    Rr_UIHash Hash =
        Rr_UIGetHash(sizeof("Rr.Collapse"), "Rr.Collapse", Rr_UICurrentHash());

    Rr_UIClickResult ClickResult = Rr_UIClickSimple(Layout, &ButtonRect, Hash);
    if (ClickResult.ClickCount)
    {
        Window->Collapsed = !Window->Collapsed;
    }

    Rr_UIDrawBevel(
        &ButtonRect,
        &gUIContext->Colors.TitleCollapseButtonBackground,
        ClickResult.Held && ClickResult.Hovered);

    Rr_Vec2 TriangleCenter = Rr_AddV2(
        ButtonRect.Offset,
        Rr_MulV2F(Rr_V2F(gUIContext->TitleHeight), 0.5f));
    float TriangleSize = gUIContext->TitleHeight * 0.3f;
    Rr_UIDrawFitTriangleFilled(
        TriangleCenter,
        TriangleSize,
        !Window->Collapsed ? RR_ANGLE_DEG(90.0f) : 0.0f,
        &gUIContext->Colors.Foreground);

    return ClickResult.ClickCount;
}

static inline void Rr_UIAddCloseButton(Rr_UILayout *Layout, bool *Open)
{
    Rr_UIWindow *Window = Layout->Window;

    /* Assuming having a title bar. */

    float Width = gUIContext->TitleButtonSize * 0.7f;
    float Thickness = gUIContext->TitleButtonSize * 0.125f;
    Rr_Rect TitleRect = Layout->Rect;
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

    Rr_UIHash Hash =
        Rr_UIGetHash(sizeof("Rr.Close"), "Rr.Close", Rr_UICurrentHash());

    Rr_UIClickResult ClickResult = Rr_UIClickSimple(Layout, &ButtonRect, Hash);
    if (ClickResult.ClickCount && Open)
    {
        *Open = false;
    }

    Rr_UIDrawBevel(
        &ButtonRect,
        &gUIContext->Colors.TitleCloseButtonBackground,
        ClickResult.Held && ClickResult.Hovered);

    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(45.0f),
        &gUIContext->Colors.Foreground);
    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(-45.0f),
        &gUIContext->Colors.Foreground);
}

static inline Rr_Vec2 Rr_UICalculateTitleSize(Rr_UIWindow *Window)
{
    if (!Rr_UIWindowHasTitle(Window))
    {
        return Rr_V2F(0.0f);
    }

    bool HasCollapse = Rr_UIWindowHasCollapseButton(Window);
    bool HasClose = Rr_UIWindowHasCloseButton(Window);

    return Rr_V2(
        Rr_UICalculateTextSize(SIZE_MAX, Window->Title, 0.0f, 0).X +
            gUIContext->TitlePadding.Width * 2 +
            (HasClose ? gUIContext->TitleButtonSize : 0) +
            (HasCollapse ? gUIContext->TitleButtonSize : 0),
        gUIContext->TitleHeight);
}

static inline void Rr_UIAddWindowTitle(Rr_UILayout *Layout, bool *Open)
{
    Rr_UIWindow *Window = Layout->Window;
    Rr_UIPrimitive BevelPrimitive = Rr_UIReserveBevel();

    Rr_Rect TitleRect = {
        Layout->Rect.Offset,
        Rr_V2(Layout->Rect.Extent.Width, gUIContext->TitleHeight),
    };
    Rr_Vec2 TitlePosition =
        Rr_AddV2(Layout->Rect.Offset, gUIContext->TitlePadding);

    bool HasCollapse = Rr_UIWindowHasCollapseButton(Window);
    bool CollapseButtonClicked = false;
    if (HasCollapse)
    {
        CollapseButtonClicked = Rr_UIAddCollapseButton(Layout);

        TitlePosition.X += gUIContext->TitleHeight;
        TitleRect.Offset.X += gUIContext->TitleHeight;
        TitleRect.Extent.Width -= gUIContext->TitleHeight;
    }

    Rr_UIDrawText(
        false,
        TitlePosition,
        SIZE_MAX,
        Window->Title,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    bool HasClose = Rr_UIWindowHasCloseButton(Window);
    if (HasClose)
    {
        Rr_UIAddCloseButton(Layout, Open);

        TitleRect.Extent.Width -= gUIContext->TitleButtonSize;
    }

    /* Allow double clicking the title bevel to toggle collapse state. */

    if (HasCollapse && !CollapseButtonClicked)
    {
        Rr_UIHash Hash = Rr_UIGetHash(
            sizeof("Rr.CollapseTitle"),
            "Rr.CollapseTitle",
            Rr_UICurrentHash());
        Rr_UIClickResult ClickResult =
            Rr_UIClickDouble(Layout, &TitleRect, Hash);
        if (ClickResult.ClickCount == 2)
        {
            Window->Collapsed = !Window->Collapsed;
        }
    }

    Rr_Vec4 ColorB = gUIContext->Colors.TitleBackground;
    ColorB.RGB = Rr_LerpV3(ColorB.RGB, 0.25f, (Rr_Vec3){ 0.0f, 0.0f, 0.0f });
    Rr_Vec4 Colors[4] = { ColorB,
                          gUIContext->Colors.TitleBackground,
                          ColorB,
                          gUIContext->Colors.TitleBackground };
    Rr_UIBevelEx(BevelPrimitive, &TitleRect, Colors, false);
}

static inline bool Rr_UIAddResizeHandle(Rr_UILayout *Layout)
{
    Rr_Vec2 BottomRight = Rr_AddV2(Layout->Rect.Offset, Layout->Rect.Extent);
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

    Rr_UIHash ResizeHash =
        Rr_UIGetHash(sizeof("Rr.Resize"), "Rr.Resize", Rr_UICurrentHash());
    Rr_UIClickResult ClickResult = Rr_UIClickEx(
        Layout,
        &ResizeHandleRect,
        RR_UI_CLICK_TYPE_DRAG_MULTI,
        ResizeHash,
        Layout->Rect.Extent);

    if (ClickResult.ClickCount == 2)
    {
        Layout->DeferredAutoResize = true;
        Rr_UIResetClick();
    }

    if (ClickResult.Moved)
    {
        Rr_Vec2 Delta =
            Rr_SubV2(gUIContext->MousePosition, gUIContext->ClickMouseStart);
        Rr_Vec2 NewWindowSize = Rr_AddV2(gUIContext->ClickWindowStart, Delta);
        Rr_Vec2 MinWindowSize = Rr_UIGetMinWindowSize(Layout->Window->Flags);
        NewWindowSize.X = RR_MAX(NewWindowSize.X, MinWindowSize.X);
        NewWindowSize.Y = RR_MAX(NewWindowSize.Y, MinWindowSize.Y);
        Layout->DeferredWindowExtent = Rr_FloorV2(NewWindowSize);
    }

    Layout->DeferredResizeHandleColor = gUIContext->Colors.Foreground;
    if (ClickResult.Hovered || ClickResult.Moved)
    {
        Layout->DeferredResizeHandleColor =
            Rr_MulV4F(Layout->DeferredResizeHandleColor, 0.75f);
    }

    return ClickResult.Moved;
}

static inline Rr_Rect Rr_UIGetWindowContentsArea(
    Rr_UILayout *Layout,
    float *OutFillRatio)
{
    Rr_UIWindow *Window = Layout->Window;

    Rr_Rect Rect = Layout->Rect;

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
            &gUIContext->Colors.ScrollbarBackground);

        float ScrollbarHandleOffset =
            (gUIContext->ScrollbarWidth - gUIContext->ScrollbarHandleWidth) /
            2.0f;

        Rr_Vec2 ScrollbarHandlePosition = ScrollbarPosition;
        Rr_Vec2 ScrollbarHandleSize = ScrollbarSize;
        ScrollbarHandlePosition.X += ScrollbarHandleOffset;
        ScrollbarHandleSize.Width = gUIContext->ScrollbarHandleWidth;
        ScrollbarHandleSize.Height *= FillRatio;

        float ScrollbarHandleHeightUnpadded = ScrollbarHandleSize.Height;

        ScrollbarHandlePosition.Y += roundf(Window->VScroll * FillRatio);

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

        Rr_UIClickResult ClickResult = Rr_UIClickDrag(
            Layout,
            &ClickableRect,
            0,
            (Rr_Vec2){ 0.0f, Window->VScroll });

        if (ClickResult.ClickCount)
        {
            /* Handle clicking outside the handle. */

            if (gUIContext->MousePosition.Y >
                ScrollbarHandlePosition.Y + ScrollbarHandleSize.Y)
            {
                Window->VScroll = Window->VScrollTarget =
                    (gUIContext->MousePosition.Y - ScrollbarPosition.Y -
                     ScrollbarHandleOffset * 2.0f) /
                        (ScrollbarSize.Y / ContentsHeight) -
                    (ScrollbarHandleSize.Height / FillRatio);
                gUIContext->ClickWindowStart.Y = Window->VScroll;
            }
            else if (gUIContext->MousePosition.Y < ScrollbarHandlePosition.Y)
            {
                Window->VScroll = Window->VScrollTarget =
                    (gUIContext->MousePosition.Y - ScrollbarPosition.Y -
                     ScrollbarHandleOffset * 2.0f) /
                    ((ScrollbarSize.Y) / ContentsHeight);
                gUIContext->ClickWindowStart.Y = Window->VScroll;
            }
        }

        if (ClickResult.Moved)
        {
            float Delta =
                gUIContext->MousePosition.Y - gUIContext->ClickMouseStart.Y;
            Window->VScroll = Window->VScrollTarget =
                gUIContext->ClickWindowStart.Y + (Delta * 1.0f / FillRatio);
        }

        Rr_UIDrawBevel(
            &(Rr_Rect){
                ScrollbarHandlePosition,
                ScrollbarHandleSize,
            },
            &gUIContext->Colors.ScrollbarNormal,
            false);
    }
    else
    {
        /* Window->VScroll = 0.0f; */
        Window->VScrollTarget = 0.0f;
    }

    Window->VScroll = RR_CLAMP(0.0f, Window->VScroll, MaxYScroll);
    Window->VScrollTarget =
        RR_CLAMP(0.0f, roundf(Window->VScrollTarget), MaxYScroll);

    return FillRatio < 1.0f;
}

Rr_UIStyle *Rr_UIGetStyle(void)
{
    return &gUIContext->Style;
}

Rr_UIColors *Rr_UIGetColors(void)
{
    return &gUIContext->Colors;
}

void Rr_UIPushFormatFloatDecimalPlaces(uint32_t Places)
{
    *RR_PUSH_INTO_ARRAY(
        &gUIContext->FormatFloatDecimalPlacesStack,
        gUIContext->Arena) = Places;
}

void Rr_UIPopFormatFloatDecimalPlaces(void)
{
    assert(
        gUIContext->FormatFloatDecimalPlacesStack.Count &&
        "Did you forget to call Rr_UIPushFormatFloatDecimalPlaces()?");
    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->FormatFloatDecimalPlacesStack));
}

static inline const char *Rr_UICurrentFloatFormatString(void)
{
    static const char *DEFAULT_STRING = "%.2f";

    if (gUIContext->FormatFloatDecimalPlacesStack.Count > 0)
    {
        uint32_t Top =
            RR_LAST_ARRAY_ELEMENT(&gUIContext->FormatFloatDecimalPlacesStack);
        switch (Top)
        {
            case 0:
                return "%.0f";
            case 1:
                return "%.1f";
            case 2:
                return "%.2f";
            case 3:
                return "%.3f";
            case 4:
                return "%.4f";
            case 5:
                return "%.5f";
            case 6:
                return "%.6f";
            case 7:
                return "%.7f";
            case 8:
                return "%.8f";
            default:
                return DEFAULT_STRING;
        }
    }
    else
    {
        return DEFAULT_STRING;
    }
}

void Rr_UISetNextWindowOffset(Rr_Vec2 Offset)
{
    gUIContext->NextWindowOffset = Offset;
}

static inline bool Rr_UIConsumeNextWindowOffset(Rr_UIWindow *Window)
{
    if (gUIContext->NextWindowOffset.X != INFINITY &&
        gUIContext->NextWindowOffset.Y != INFINITY)
    {
        Window->Rect.Offset = Rr_FloorV2(gUIContext->NextWindowOffset);
        gUIContext->NextWindowOffset = Rr_V2F(INFINITY);

        return true;
    }

    return false;
}

void Rr_UISetNextWindowOpenOffset(Rr_Vec2 Offset)
{
    gUIContext->NextWindowOpenOffset = Offset;
}

static inline void Rr_UIConsumeNextWindowOpenOffset(Rr_UIWindow *Window)
{
    if (gUIContext->NextWindowOpenOffset.X != INFINITY &&
        gUIContext->NextWindowOpenOffset.Y != INFINITY)
    {
        Window->Rect.Offset = Rr_FloorV2(gUIContext->NextWindowOpenOffset);
        gUIContext->NextWindowOpenOffset = Rr_V2F(INFINITY);
    }
}

void Rr_UISetNextWindowExtent(Rr_Vec2 Extent)
{
    gUIContext->NextWindowExtent = Extent;
}

static inline bool Rr_UIConsumeNextWindowExtent(Rr_UIWindow *Window)
{
    if (gUIContext->NextWindowExtent.Width != INFINITY &&
        gUIContext->NextWindowExtent.Height != INFINITY)
    {
        Window->Rect.Extent = Rr_FloorV2(gUIContext->NextWindowExtent);
        gUIContext->NextWindowExtent = Rr_V2F(INFINITY);

        return true;
    }

    return false;
}

void Rr_UISetNextWindowPadding(Rr_Vec2 Padding)
{
    gUIContext->NextWindowPadding = Padding;
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

void Rr_UISetNextWindowCreateCollapsed(bool Collapsed)
{
    gUIContext->NextWindowCreateCollapsed = Collapsed ? 1 : -1;
}

static inline void Rr_UIConsumeNextWindowCreateCollapsed(Rr_UIWindow *Window)
{
    if (Window->CreatedThisFrame)
    {
        if (Window->Child)
        {
            Window->Collapsed = true;
        }
        if (gUIContext->NextWindowCreateCollapsed != 0)
        {
            Window->Collapsed = gUIContext->NextWindowCreateCollapsed == 1;
        }
    }
    gUIContext->NextWindowCreateCollapsed = 0;
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
    Rr_UIWindow *Window,
    Rr_UIHash Hash,
    bool *Open)
{
    Rr_UIConsumeNextWindowCreateCollapsed(Window);
    bool WindowOffsetConsumed = Rr_UIConsumeNextWindowOffset(Window);
    bool WindowExtentConsumed = Rr_UIConsumeNextWindowExtent(Window);
    Rr_Vec2 ContentsPadding = Rr_UIConsumeNextWindowPadding();

    /* Return if closed.
     * Also handle show after being closed.
     * This will put window on top unless there is a flag
     * preventing that which is not currently implemented. */

    bool NoBorders = Rr_UIWindowNoBorders(Window);
    bool HasTitle = Rr_UIWindowHasTitle(Window);

    bool WasClosed = Window->Open == false;
    Window->Open = (Open == NULL || *Open == true);
    if (!Window->Open)
    {
        return false;
    }
    if (WasClosed)
    {
        Rr_UIConsumeNextWindowOpenOffset(Window);

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

    Rr_UILayout *ParentLayout = NULL;
    Rr_UIWindow *ParentWindow = NULL;

    if (Window->Child)
    {
        ParentLayout = Rr_UICurrentLayout();
        ParentWindow = ParentLayout->Window;

        Window->TopLevelClipRects = ParentWindow->TopLevelClipRects;
    }
    else
    {
        Window->TopLevelClipRects =
            RR_ALLOC_TYPE(gUIContext->FrameArena, Rr_UIClipRectArray);
    }

    Rr_UILayout *Layout = Rr_UIPushLayout(Hash, Window, ContentsPadding);
    Layout->LockOffset = WindowOffsetConsumed;
    Layout->LockExtent = WindowExtentConsumed;

    if (!WindowExtentConsumed)
    {
        if (Window->Child)
        {
            Layout->Rect.Extent.X = ParentLayout->AvailableContentsWidth;
        }
        else
        {
            if (!Window->ShownAtLeastOnce && Window->OpenedThisFrame)
            {
                Layout->DeferredAutoResize = true;
            }

            /* NOTE: Consider title width when in auto resize mode. This will be
             * the baseline width (e.g. when window only has wrapped text). */

            if (HasTitle && Layout->DeferredAutoResize)
            {
                Rr_Vec2 TitleSize = Rr_UICalculateTitleSize(Window);
                Layout->Rect.Extent.X =
                    RR_MAX(Layout->Rect.Extent.X, TitleSize.X);
                Window->Rect.Extent.X = Layout->Rect.Extent.X;
            }
        }
    }

    Layout->AvailableContentsWidth = Layout->Rect.Extent.X;

    /* Calculate total and visible extents. */

    if (Window->Collapsed)
    {
        Layout->Rect.Extent.Y = gUIContext->TitleHeight;
    }
    Rr_Rect TotalClipRectWithBorders =
        Rr_ResizeRect(&Layout->Rect, gUIContext->FrameThickness);

    if (Window->Child)
    {
        Window->VisibleRect = Rr_UIRectIntersection(
            &TotalClipRectWithBorders,
            &ParentWindow->ContentsClipRect->Rect);

        Rr_UIBeginVisibleClipRect(&TotalClipRectWithBorders);
    }
    else
    {
        Window->VisibleRect = TotalClipRectWithBorders;

        Rr_UIBeginClipRect(&TotalClipRectWithBorders);
    }

    /* Add window title if necessary. */

    if (HasTitle)
    {
        Rr_UIAddWindowTitle(Layout, Open);

        Layout->Cursor.Y += gUIContext->TitleHeight;
    }

    /* NOTE: This allows calculating proper extents before showing the window
     * next frame. */

    if (Window->Collapsed && Layout->WasCollapsed)
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
        Window->VScroll = 0.0f;
        Window->VScrollTarget = 0.0f;
    }
    else
    {
        VerticalScrollbarAdded = Rr_UIAddVerticalScrollbar(Layout);
        if (VerticalScrollbarAdded ||
            (Layout->DeferredAutoResize && Window->SkipThisFrame))
        {
            Layout->AvailableContentsWidth -= gUIContext->ScrollbarWidth;
        };
        Window->VScroll = Rr_Damp(
            Window->VScroll,
            15.0f * (float)Rr_GetDeltaSeconds(),
            Window->VScrollTarget);
        Layout->Cursor.Y -= roundf(Window->VScroll);
    }

    /* NOTE: Defer drawing the handle to Rr_UIEndWindow()! */

    if (!Layout->DeferredAutoResize && !Window->Child)
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
    Rr_UIBeginVisibleClipRect(&VisibleContentsAreaRect);

    Window->ContentsClipRect =
        &RR_LAST_ARRAY_ELEMENT(Window->TopLevelClipRects);

    /* Window->ContentsClipRect = Rr_UICurr */

    Rr_Vec4 *BackgroundColor = Window->Child
                                   ? &gUIContext->Colors.ChildBackground
                                   : &gUIContext->Colors.Background;
    Rr_UIDrawSolidQuad(&ContentsAreaRect, BackgroundColor);

    Window->ContentsStart = Window->ContentsEnd = Layout->Cursor;

    return true;
}

static inline void Rr_UIBeginPopupWindow(Rr_UIHash Hash, Rr_UIWindowFlags Flags)
{
    Rr_UIWindow *Window = &gUIContext->PopupWindow;
    Window->Flags = Flags;
    Window->TopLevelParent = Window;
    if (Rr_UIBeginWindowEx(Window, Hash, NULL))
    {
        Rr_UICurrentLayout()->DeferredClampOffsetToScreen = true;
    }
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
    Window->Flags = Flags;
    Window->Rect.Extent = Rr_UIGetMinWindowSize(Flags);
    Window->CreatedThisFrame = true;

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
    return Rr_UIBeginWindowEx(Window, TitleHash, Open);
}

void Rr_UIEndWindow(void)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_UIAssertNoHorizontal(Layout);

    Rr_UIEndClipRect();

    /* Begin overlay clip rect for stuff such as borders, resize handle and
     * scroll area darkeners. */

    Rr_UIBeginVisibleClipRect(&Layout->Rect);

    if (!Window->Collapsed)
    {
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
                Rr_AddV2(Layout->Rect.Offset, Layout->Rect.Extent);
            Rr_Vec2 Positions[] = {
                { BottomRight.X - gUIContext->ResizeHandleSize, BottomRight.Y },
                { BottomRight.X, BottomRight.Y - gUIContext->ResizeHandleSize },
                { BottomRight.X, BottomRight.Y },
            };
            Rr_UIDrawTriangleFilled(
                Positions,
                &Layout->DeferredResizeHandleColor);
        }
    }

    /* Add border if necessary. */

    if (!Rr_UIWindowNoBorders(Window))
    {
        Rr_UIDrawInnerFrame(
            &Layout->Rect,
            gUIContext->FrameThickness,
            &gUIContext->Colors.Outline);
    }

    Rr_UIEndClipRect();

    /* NOTE: Forward scroll wheel behavior to the top-level parent. */

    if (!Window->Collapsed || Window->Child)
    {
        Rr_UIScrollBehavior(
            Window,
            &Window->VisibleRect,
            &Window->TopLevelParent->VScrollTarget);
    }

    /* Apply window extent. */

    if (!Layout->LockExtent)
    {
        if (Layout->DeferredWindowExtent.X != INFINITY)
        {
            Window->Rect.Extent = Layout->DeferredWindowExtent;
        }
        else if (Window->Child)
        {
            Window->Rect.Extent.Y = Window->ContentsEnd.Y -
                                    Window->ContentsStart.Y +
                                    Layout->ContentsPadding.Y;

            /* NOTE: Select between widths occupied by rigid widgets such as
             * buttons and flexible widgets such as input fields. */

            Rr_Vec2 TitleSize = Rr_UICalculateTitleSize(Window);

            Window->Rect.Extent.X = RR_MAX(
                Layout->DeferredMaxFlexibleWidgetTitleWidth +
                    gUIContext->FlexibleTitleMargin +
                    Layout->DeferredMaxFlexibleWidgetWidth,
                Layout->DeferredMaxRigidWidth);
            Window->Rect.Extent.X += Layout->ContentsPadding.X * 2.0f;
            Window->Rect.Extent.X = RR_MAX(Window->Rect.Extent.X, TitleSize.X);

            if (Rr_UIWindowHasTitle(Window))
            {
                Window->Rect.Extent.Y += gUIContext->TitleHeight;
            }

            Window->Rect.Extent =
                Rr_AddV2(Window->Rect.Extent, Layout->ReservedExtent);
        }
        else if (Layout->DeferredAutoResize)
        {
            Window->Rect.Extent =
                Rr_SubV2(Window->ContentsEnd, Window->ContentsStart);
            Window->Rect.Extent.X += Layout->ContentsPadding.X * 2.0f;
            Window->Rect.Extent.Y += Layout->ContentsPadding.Y;

            if (Rr_UIWindowHasTitle(Window))
            {
                Window->Rect.Extent.Y += gUIContext->TitleHeight;
            }

            Window->Rect.Extent =
                Rr_AddV2(Window->Rect.Extent, Layout->ReservedExtent);
        }
    }

    /* Apply window offset.
     * NOTE: Forward drag-to-move behavior to the top-level parent.
     * NOTE: Rr_UIClickDrag() gets called even if the window is
     * non-movable because this function resets widget focus. */

    Rr_UIHash MoveHash =
        Rr_UIGetHash(sizeof("Rr.Move"), "Rr.Move", Rr_UICurrentHash());
    Rr_UIClickResult ClickResult = Rr_UIClickDrag(
        Layout,
        &Layout->Rect,
        MoveHash,
        Layout->TopLevelParent->Rect.Offset);
    if (!Layout->LockOffset)
    {
        if (!Rr_UIWindowNoMove(Window->TopLevelParent) && ClickResult.Moved)
        {
            Rr_Vec2 Delta = Rr_SubV2(
                gUIContext->MousePosition,
                gUIContext->ClickMouseStart);
            Layout->TopLevelParent->DeferredWindowOffset =
                Rr_FloorV2(Rr_AddV2(gUIContext->ClickWindowStart, Delta));
        }

        if (Layout->DeferredWindowOffset.X != INFINITY)
        {
            Window->TopLevelParent->Rect.Offset = Layout->DeferredWindowOffset;
        }

        if (Layout->DeferredClampOffsetToScreen)
        {
            Rr_Vec2 *TopLevelOffset = &Window->TopLevelParent->Rect.Offset;
            Rr_Vec2 *TopLevelExtent = &Window->TopLevelParent->Rect.Extent;

            float MinOffset = Rr_UIWindowNoBorders(Window->TopLevelParent)
                                  ? 0.0f
                                  : gUIContext->FrameThickness;

            if (TopLevelOffset->X < MinOffset)
            {
                TopLevelOffset->X = MinOffset;
            }
            else if (
                TopLevelOffset->X >
                gUIContext->ScreenSize.X - TopLevelExtent->X - MinOffset)
            {
                TopLevelOffset->X =
                    gUIContext->ScreenSize.X - TopLevelExtent->X - MinOffset;
            }

            if (TopLevelOffset->Y < MinOffset)
            {
                TopLevelOffset->Y = MinOffset;
            }
            else if (
                TopLevelOffset->Y >
                gUIContext->ScreenSize.Y - TopLevelExtent->Y - MinOffset)
            {
                TopLevelOffset->Y =
                    gUIContext->ScreenSize.Y - TopLevelExtent->Y - MinOffset;
            }
        }
    }

    /* Apply deferred layout properties. */

    Window->MaxFlexibleWidgetTitleWidth =
        Layout->DeferredMaxFlexibleWidgetTitleWidth;
    Window->MaxFlexibleWidgetWidth = Layout->DeferredMaxFlexibleWidgetWidth;
    Window->MaxRigidWidth = Layout->DeferredMaxRigidWidth;

    /* Pop whatever was pushed in Rr_UIBeginWindowEx(). */

    Rr_UIPopLayout();

    Rr_UILayout *ParentLayout = Rr_UICurrentLayout();
    if (ParentLayout)
    {
        Rr_UIWindow *ParentWindow = ParentLayout->Window;

        if (Window->Child)
        {
            Rr_Vec2 TitleSize = Rr_UICalculateTitleSize(Window);
            Rr_Vec2 WindowExtent = Window->Rect.Extent;
            bool CollapsedThisFrame =
                Window->Collapsed && !Layout->WasCollapsed;
            bool UncollapsedThisFrame =
                !Window->Collapsed && Layout->WasCollapsed;
            if (CollapsedThisFrame)
            {
                ParentLayout->ReservedExtent.Y -= WindowExtent.Y - TitleSize.Y;
                WindowExtent.X = TitleSize.X;
                Rr_UIAdvance(WindowExtent);
            }
            else if (UncollapsedThisFrame)
            {
                ParentLayout->ReservedExtent.Y += WindowExtent.Y - TitleSize.Y;
                WindowExtent.Y = TitleSize.Y;
                Rr_UIAdvance(WindowExtent);
            }
            else if (Window->Collapsed)
            {
                Rr_UIAdvance(TitleSize);
            }
            else
            {
                Rr_UIAdvance(WindowExtent);
            }
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

    const Rr_UIWindowFlags CHILD_FLAGS = // RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_MINIMIZE_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_MOVE_BIT |
        RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT;
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
    return Rr_UIBeginWindowEx(Window, TitleHash, NULL);
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
    Layout->HorizontalMaxExtent = Rr_V2F(0.0f);
    Layout->HorizontalX = Layout->Cursor.X;
}

void Rr_UIEndHorizontal(void)
{
    assert(
        Rr_UIIsHorizontal() && "Did you forget to call Rr_BeginHorizontal()?");
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Layout->Cursor.X = Layout->HorizontalX;
    Layout->HorizontalX = INFINITY;
    Rr_UIAdvance(Layout->HorizontalMaxExtent);
}

static inline float Rr_UISetupFlexibleWidget(
    Rr_UILayout *Layout,
    size_t TitleLength,
    const char *Title,
    float DesiredWidgetWidth)
{
    float TitleWidth = Rr_UICalculateTextSize(TitleLength, Title, 0.0f, 0).X;
    Layout->DeferredMaxFlexibleWidgetTitleWidth =
        RR_MAX(Layout->DeferredMaxFlexibleWidgetTitleWidth, TitleWidth);

    Layout->DeferredMaxFlexibleWidgetWidth =
        RR_MAX(Layout->DeferredMaxFlexibleWidgetWidth, DesiredWidgetWidth);

    Rr_UIWindow *Window = Layout->Window;

    if (Layout->DeferredAutoResize)
    {
        DesiredWidgetWidth = RR_MAX(
            Window->MaxRigidWidth - Window->MaxFlexibleWidgetTitleWidth -
                gUIContext->FlexibleTitleMargin,
            RR_MAX(DesiredWidgetWidth, Layout->Window->MaxFlexibleWidgetWidth));
    }
    else
    {
        DesiredWidgetWidth = Layout->AvailableContentsWidth -
                             Window->MaxFlexibleWidgetTitleWidth -
                             gUIContext->FlexibleTitleMargin;
    }

    Layout->NextAdvanceFlexible = true;

    return DesiredWidgetWidth;
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
        &gUIContext->Colors.Foreground);

    /* TODO: Use window padding instead? */
    Rr_UIAdvance(Rr_V2(0.0f, gUIContext->LineHeight));
}

bool Rr_UITab(const char *Title)
{
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
        false,
        TextPosition,
        TitleLength,
        Title,
        0.0f,
        Selected ? &gUIContext->Colors.Background
                 : &gUIContext->Colors.Foreground,
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

    Rr_UIClickResult ClickResult = Rr_UIClickSimple(
        Layout,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        },
        TitleHash);

    Rr_Vec4 *TabButtonColor;
    if (Selected)
    {
        TabButtonColor = &gUIContext->Colors.Foreground;
    }
    else if (ClickResult.Held)
    {
        TabButtonColor = &gUIContext->Colors.ButtonHeld;
    }
    else if (ClickResult.Hovered)
    {
        TabButtonColor = &gUIContext->Colors.ButtonHovered;
    }
    else
    {
        TabButtonColor = &gUIContext->Colors.Background;
    }

    Rr_UISolidQuad(
        TabQuad.Vertices,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        },
        TabButtonColor);

    if (ClickResult.ClickCount)
    {
        /* Newly selected tab will be rendered next frame. */
        *Layout->SelectedTabHash = TitleHash;
    }

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
        &gUIContext->Colors.Foreground);

    Rr_Vec2 TitlePosition = Rr_AddV2(
        Rr_V2(
            Layout->Cursor.X + TriangleSize + gUIContext->ButtonPadding.Width,
            Layout->Cursor.Y),
        gUIContext->TitlePadding);
    Rr_UIDrawText(
        false,
        TitlePosition,
        TitleLength,
        Title,
        0,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Vec2 TotalExtent =
        Rr_V2(Layout->AvailableContentsWidth, FoldButtonHeight);

    Rr_UIClickResult ClickResult = Rr_UIClickSimple(
        Layout,
        &(Rr_Rect){
            Layout->Cursor,
            TotalExtent,
        },
        TitleHash);

    if (ClickResult.ClickCount)
    {
        *FoldValue = !*FoldValue;
    }

    Rr_Rect ButtonRect = {
        Layout->Cursor,
        TotalExtent,
    };
    Rr_UIBevel(
        BevelPrimitive,
        &ButtonRect,
        &gUIContext->Colors.TitleBackground,
        ClickResult.Held);

    Rr_UIAdvance(TotalExtent);

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
        &gUIContext->Colors.Outline);

    Layout->Cursor.Y += gUIContext->SeparatorLineHeight;
}

void Rr_UITextEx(const char *Text, Rr_UITextFlags Flags)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 TextSize = Rr_UIDrawText(
        false,
        Layout->Cursor,
        SIZE_MAX,
        Text,
        Layout->AvailableContentsWidth,
        &gUIContext->Colors.Foreground,
        Flags);

    Rr_UIAdvance(TextSize);
}

void Rr_UIText(const char *Text)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 TextSize = Rr_UIDrawText(
        false,
        Layout->Cursor,
        SIZE_MAX,
        Text,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    Rr_UIAdvance(TextSize);
}

void Rr_UITextF(const char *Format, ...)
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

    Rr_UIText(Buffer);

    Rr_DestroyScratch(Scratch);
}

void Rr_UILabelText(const char *Title, const char *Text)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength = strlen(Title);

    float TextWidth = Rr_UISetupFlexibleWidget(
        Layout,
        TitleLength,
        Title,
        Rr_UICalculateTextSize(SIZE_MAX, Text, 0.0f, 0).X);

    Rr_Vec2 TextSize = Rr_UIDrawText(
        false,
        Layout->Cursor,
        SIZE_MAX,
        Text,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += TextWidth + Layout->ContentsPadding.X;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Vec2 TotalExtent = {
        TextWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        TextSize.Y,
    };

    Rr_UIAdvance(TotalExtent);
}

bool Rr_UIButton(const char *Text)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Text, &TitleLength);

    Rr_Vec2 ButtonPosition = Layout->Cursor;
    Rr_UIPrimitive Primitive = Rr_UIReserveBevel();

    Rr_Vec2 TextPosition = Rr_AddV2(ButtonPosition, gUIContext->ButtonPadding);
    Rr_Vec2 TextSize = Rr_UIDrawText(
        false,
        TextPosition,
        TitleLength,
        Text,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Vec2 ButtonSize =
        Rr_AddV2(TextSize, Rr_MulV2F(gUIContext->ButtonPadding, 2.0f));

    Rr_UIClickResult ClickResult = Rr_UIClickSimple(
        Layout,
        &(Rr_Rect){
            ButtonPosition,
            ButtonSize,
        },
        TitleHash);

    Rr_Rect ButtonRect = {
        ButtonPosition,
        ButtonSize,
    };

    Rr_UIBevel(
        Primitive,
        &ButtonRect,
        &gUIContext->Colors.ButtonNormal,
        ClickResult.Held && ClickResult.Hovered);

    Rr_UIAdvance(ButtonSize);

    return ClickResult.ClickCount;
}

bool Rr_UIRadioButton(
    const char *Title,
    int32_t *SelectedOption,
    int32_t ThisOption)
{
    Rr_UIAssertWindow();
    assert(SelectedOption != NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    float ButtonSize = gUIContext->LineHeight;
    float OuterRadius = ButtonSize * 0.4f;
    float InnerRadius = OuterRadius * 0.6f;
    float InnerRadiusHeld = OuterRadius * 0.8f;
    float OutlineThickness = gUIContext->FontSize / 24.0f * 0.5f;

    Rr_Vec2 Cursor = Layout->Cursor;

    Rr_Vec2 TitlePosition = Cursor;
    TitlePosition.X += OuterRadius * 2.0f + gUIContext->FlexibleTitleMargin;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        false,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Rect ButtonRect = {
        Cursor,
        Rr_V2(
            TitleSize.X + gUIContext->FlexibleTitleMargin + OuterRadius * 2.0f,
            TitleSize.Y),
    };

    Rr_UIClickResult ClickResult =
        Rr_UIClickSimple(Layout, &ButtonRect, TitleHash);

    if (ClickResult.ClickCount)
    {
        *SelectedOption = ThisOption;
    }

    bool Selected = *SelectedOption == ThisOption;

    Rr_Vec2 CircleOffset =
        Rr_V2(Cursor.X + OuterRadius, Cursor.Y + ButtonSize * 0.5f);

    Rr_UIDrawCircleFilled(
        CircleOffset,
        OuterRadius,
        &gUIContext->Colors.ButtonNormal);
    if (ClickResult.Hovered && ClickResult.Held)
    {
        Rr_UIDrawCircleFilled(
            CircleOffset,
            InnerRadiusHeld,
            &gUIContext->Colors.Foreground);
    }
    else if (Selected)
    {
        Rr_UIDrawCircleFilled(
            CircleOffset,
            InnerRadius,
            &gUIContext->Colors.Foreground);
    }

    Rr_Vec4 OutlineColor = gUIContext->Colors.ButtonNormal;
    OutlineColor.XYZ = Rr_MulV3F(
        OutlineColor.XYZ,
        1.0f + gUIContext->Style.BevelIntensityLight);
    Rr_UIDrawCircle(
        CircleOffset,
        OuterRadius - OutlineThickness,
        OutlineThickness,
        &OutlineColor);

    Rr_UIAdvance(ButtonRect.Extent);

    return ClickResult.ClickCount;
}

bool Rr_UICheckbox(const char *Title, bool *Checked)
{
    Rr_UIAssertWindow();
    assert(Checked != NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_Vec2 CheckboxSize = { gUIContext->LineHeight, gUIContext->LineHeight };

    Rr_Vec2 FramePosition = Layout->Cursor;

    Rr_Vec2 TitlePosition = FramePosition;
    TitlePosition.X += CheckboxSize.X + gUIContext->ContentsPadding.X;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        false,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Rect ButtonRect = {
        FramePosition,
        Rr_V2(
            TitleSize.X + Layout->ContentsPadding.X + gUIContext->LineHeight,
            TitleSize.Y),
    };

    Rr_UIClickResult ClickResult =
        Rr_UIClickSimple(Layout, &ButtonRect, TitleHash);

    if (ClickResult.ClickCount)
    {
        *Checked = !*Checked;
    }

    Rr_Rect CheckboxRect = {
        FramePosition,
        CheckboxSize,
    };
    Rr_UIDrawBevel(
        &CheckboxRect,
        ClickResult.Held && ClickResult.Hovered
            ? &gUIContext->Colors.InputFieldActive
            : &gUIContext->Colors.InputFieldNormal,
        ClickResult.Held && ClickResult.Hovered);

    if (*Checked)
    {
        Rr_UIDrawCircleFilled(
            Rr_AddV2(CheckboxRect.Offset, Rr_DivV2F(CheckboxSize, 2.0f)),
            CheckboxSize.Width * 0.15f,
            &gUIContext->Colors.Foreground);
    }

    Rr_UIAdvance(ButtonRect.Extent);

    return ClickResult.ClickCount;
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

typedef struct Rr_UIEditResult Rr_UIEditResult;
struct Rr_UIEditResult
{
    bool Edited;
    bool Confirmed;
};

static Rr_UIEditResult Rr_UIEditUTF8Buffer(
    size_t *CursorBegin,
    size_t *CursorEnd,
    size_t BufferCapacity,
    char *Buffer,
    Rr_UIInputFieldFilterFunc FilterFunc,
    bool EnterToConfirm)
{
    Rr_UIEditResult Result = { 0 };
    if (gUIContext->TextInputEvents.Count == 0 &&
        gUIContext->KeyboardInputEvents.Count == 0)
    {
        return Result;
    }

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
            Result.Confirmed |= true;
        }

        if (Event->Scancode == RR_SCANCODE_RETURN)
        {
            if (EnterToConfirm)
            {
                Result.Confirmed |= true;
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
            Result.Edited |= true;
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
            Result.Edited |= true;
        }
    }

    RR_CLEAR_ARRAY(&gUIContext->TextInputEvents);
    RR_CLEAR_ARRAY(&gUIContext->KeyboardInputEvents);

    return Result;
}

typedef struct Rr_UIInputFieldResult Rr_UIInputFieldResult;
struct Rr_UIInputFieldResult
{
    Rr_Vec2 Extent;
    bool Edited;
};

/* NOTE: Generic input field is a building block for other widgets. It doesn't
 * alter the layout on its own. */
static inline Rr_UIInputFieldResult Rr_UIGenericInputField(
    Rr_UIHash Hash,
    Rr_Vec2 Offset,
    size_t BufferCapacity,
    char *Buffer,
    const char *PlaceholderString,
    Rr_UIInputFieldFilterFunc FilterFunc,
    Rr_UIInputFieldFlags Flags,
    float FixedWidth,
    bool DrawBackground)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    bool UseFixedWidth = FixedWidth != 0.0f;
    bool AutoCenter =
        RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT) &&
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

        Rr_UIBeginVisibleClipRect(
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
    Rr_Vec2 BufferPosition = Rr_AddV2(Offset, gUIContext->InputFieldPadding);
    Rr_Vec2 BufferSize;
    if (AutoCenter)
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
        &gUIContext->Colors.Foreground);

    if (BufferSize.X == 0.0f)
    {
        if (PlaceholderString != NULL && !Focused)
        {
            if (AutoCenter)
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
                &gUIContext->Colors.ForegroundDimmed,
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
        Rr_AddV2(BufferSize, Rr_MulV2F(gUIContext->InputFieldPadding, 2.0f)),
    };
    if (UseFixedWidth)
    {
        FieldRect.Extent.X = FixedWidth;
    }
    if (DrawBackground)
    {
        Rr_UIBevel(
            BackgroundBevelPrimitive,
            &FieldRect,
            Focused ? &gUIContext->Colors.InputFieldActive
                    : &gUIContext->Colors.InputFieldNormal,
            true);
    }

    Rr_UIClickResult ClickResult =
        Rr_UIClickDrag(Layout, &FieldRect, Hash, Rr_V2F(0.0f));

    bool AutoSelect =
        RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT);

    if (ClickResult.ClickCount)
    {
        size_t BufferLength = strlen(Buffer);

        if (AutoSelect && !Focused && !WasFocused)
        {
            /* Select all on first click. */

            gUIContext->TextInputCursorBegin = 0;
            gUIContext->TextInputCursorEnd = BufferLength;
            gUIContext->TextInputClickId = gUIContext->LeftMouseButton.ClickID;
        }
        else
        {
            uint32_t Clicks = (gUIContext->LeftMouseButton.Clicks - 1) % 3;
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
    else if (Focused && ClickResult.Moved)
    {
        if (!AutoSelect ||
            gUIContext->LeftMouseButton.ClickID > gUIContext->TextInputClickId)
        {
            gUIContext->TextInputCursorBlinkTime = Rr_GetTimeMS();
            gUIContext->TextInputCursorEnd = NewCursorEnd;
        }
    }

    if (ClickResult.Hovered)
    {
        gUIContext->MouseOverTextInput = true;
    }

    Rr_UIEditResult EditResult = { 0 };

    if (Focused)
    {
        bool EnterToConfirm =
            !RR_HAS_BIT(Flags, RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT);
        EditResult = Rr_UIEditUTF8Buffer(
            &gUIContext->TextInputCursorBegin,
            &gUIContext->TextInputCursorEnd,
            BufferCapacity,
            UsePersistentBuffer ? gUIContext->TextInputBuffer.Data : Buffer,
            FilterFunc,
            EnterToConfirm);
        if (EditResult.Confirmed)
        {
            gUIContext->FocusedWindow = NULL;
        }
    }
    else
    {
        EditResult.Edited |= WasFocused;
    }

    if (EditResult.Edited && UsePersistentBuffer)
    {
        memcpy(Buffer, gUIContext->TextInputBuffer.Data, BufferCapacity);
    }

    if (UseFixedWidth)
    {
        Rr_UIEndClipRect();

        Rr_UIBeginClipRect(&RestoreClipRect->Rect);
    }

    return (Rr_UIInputFieldResult){
        .Extent = FieldRect.Extent,
        .Edited = EditResult.Edited,
    };
}

typedef enum
{
    RR_UI_SCALAR_FORMAT_TYPE_INT,
    RR_UI_SCALAR_FORMAT_TYPE_UINT,
    RR_UI_SCALAR_FORMAT_TYPE_FLOAT,
    RR_UI_SCALAR_FORMAT_TYPE_DOUBLE,
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

static inline void Rr_UIFormatScalar(
    size_t BufferCapacity,
    char *Buffer,
    Rr_UIScalarFormatType ScalarFormatType,
    void *ElementData)
{
    switch (ScalarFormatType)
    {
        case RR_UI_SCALAR_FORMAT_TYPE_INT:
        {
            snprintf(Buffer, BufferCapacity, "%d", *(int *)ElementData);
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_UINT:
        {
            snprintf(
                Buffer,
                BufferCapacity,
                "%u",
                *(unsigned int *)ElementData);
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_FLOAT:
        {
            snprintf(
                Buffer,
                BufferCapacity,
                Rr_UICurrentFloatFormatString(),
                *(float *)ElementData);
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE:
        {
            snprintf(
                Buffer,
                BufferCapacity,
                Rr_UICurrentFloatFormatString(),
                *(double *)ElementData);
        }
        break;
        default:
        {
            RR_ABORT("Unsupported format type!");
        }
        break;
    }
}

static inline float Rr_UICalculateGenericInputScalarMultiWidth(
    int Cols,
    int Rows,
    void *Data,
    Rr_UIScalarFormatType ScalarFormatType)
{
    char ComponentBuffer[32];

    size_t ComponentSize;
    switch (ScalarFormatType)
    {
        case RR_UI_SCALAR_FORMAT_TYPE_INT:
        {
            ComponentSize = sizeof(int32_t);
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_UINT:
        {
            ComponentSize = sizeof(uint32_t);
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_FLOAT:
        {
            ComponentSize = sizeof(float);
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE:
        {
            ComponentSize = sizeof(double);
        }
        break;
        default:
        {
            RR_ABORT("Unsupported format type!");
        }
        break;
    }

    float MaxTextWidth = 0.0f;
    for (int Row = 0; Row < Rows; ++Row)
    {
        for (int Col = 0; Col < Cols; ++Col)
        {
            size_t Index = (size_t)(Col * Rows + Row);
            char *ComponentData = (char *)Data + Index * ComponentSize;

            Rr_UIFormatScalar(
                sizeof(ComponentBuffer),
                ComponentBuffer,
                ScalarFormatType,
                ComponentData);

            Rr_Vec2 TextExtent =
                Rr_UICalculateTextSize(SIZE_MAX, ComponentBuffer, 0.0f, 0);
            MaxTextWidth = RR_MAX(MaxTextWidth, TextExtent.X);
        }
    }

    MaxTextWidth += gUIContext->InputFieldPadding.X * 2.0f;

    return MaxTextWidth * (float)Cols +
           gUIContext->ComponentMargin * (float)(Cols - 1);
}

static inline Rr_UIInputFieldResult Rr_UIGenericInputScalarMulti(
    Rr_UIHash Hash,
    Rr_Vec2 Offset,
    int Cols,
    int Rows,
    void *Data,
    Rr_UIScalarFormatType ScalarFormatType,
    Rr_UIInputFieldFlags Flags,
    float FixedTotalWidth,
    bool DrawBackground)
{
    assert(Cols > 0 && Cols <= 4);
    assert(Rows > 0 && Rows <= 4);

    Rr_UIInputFieldFilterFunc FilterFunc;
    size_t ComponentSize;
    const char *ScanString;
    switch (ScalarFormatType)
    {
        case RR_UI_SCALAR_FORMAT_TYPE_INT:
        {
            ComponentSize = sizeof(int32_t);
            FilterFunc = Rr_UIIntegerFilter;
            ScanString = "%i";
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_UINT:
        {
            ComponentSize = sizeof(uint32_t);
            FilterFunc = Rr_UIUnsignedIntegerFilter;
            ScanString = "%u";
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_FLOAT:
        {
            ComponentSize = sizeof(float);
            FilterFunc = Rr_UIFloatFilter;
            ScanString = "%g";
        }
        break;
        case RR_UI_SCALAR_FORMAT_TYPE_DOUBLE:
        {
            ComponentSize = sizeof(double);
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

    float SingleFieldWidth = 0.0f;
    if (FixedTotalWidth != 0.0f)
    {
        SingleFieldWidth = (FixedTotalWidth -
                            (gUIContext->ComponentMargin * (float)(Cols - 1))) /
                           (float)Cols;
    }

    const char *COMPONENT_TITLES[] = {
        "X0", "Y0", "Z0", "W0", //
        "X1", "Y1", "Z1", "W1", //
        "X2", "Y2", "Z2", "W2", //
        "X3", "Y3", "Z3", "W3",
    };
    Rr_Vec2 Cursor = Offset;
    float CursorXStart = Cursor.X;
    float MaxFieldHeight = 0.0f;
    Rr_UIInputFieldResult Result = { 0 };
    for (int Row = 0; Row < Rows; ++Row)
    {
        float MaxFieldHeightRow = 0.0f;
        for (int Col = 0; Col < Cols; ++Col)
        {
            size_t Index = (size_t)(Col * Rows + Row);
            char *ComponentData = (char *)Data + Index * ComponentSize;
            char ComponentBuffer[32];

            Rr_UIFormatScalar(
                sizeof(ComponentBuffer),
                ComponentBuffer,
                ScalarFormatType,
                ComponentData);

            Rr_UIHash ComponentHash =
                Rr_UIGetHash(2, COMPONENT_TITLES[Index], Hash);

            Rr_UIInputFieldResult ComponentResult = Rr_UIGenericInputField(
                ComponentHash,
                Cursor,
                32,
                ComponentBuffer,
                NULL,
                FilterFunc,
                Flags,
                SingleFieldWidth,
                true);

            Result.Edited |= ComponentResult.Edited;

            if (Result.Edited)
            {
                sscanf(ComponentBuffer, ScanString, (void *)ComponentData);
            }

            MaxFieldHeightRow =
                RR_MAX(MaxFieldHeightRow, ComponentResult.Extent.Y);
            Cursor.X += SingleFieldWidth + gUIContext->ComponentMargin;
        }
        MaxFieldHeight = RR_MAX(MaxFieldHeight, MaxFieldHeightRow);
        Cursor.X = CursorXStart;
        Cursor.Y += MaxFieldHeightRow + gUIContext->ComponentMargin;
    }

    Result.Extent.X = SingleFieldWidth * (float)Cols +
                      gUIContext->ComponentMargin * (float)(Cols - 1);
    Result.Extent.Y = MaxFieldHeight * (float)Rows +
                      gUIContext->ComponentMargin * (float)(Rows - 1);

    return Result;
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

    float FieldsWidth = Rr_UICalculateGenericInputScalarMultiWidth(
        Cols,
        Rows,
        Data,
        ScalarFormatType);

    FieldsWidth =
        Rr_UISetupFlexibleWidget(Layout, TitleLength, Title, FieldsWidth);

    Rr_UIInputFieldResult Result = Rr_UIGenericInputScalarMulti(
        TitleHash,
        Layout->Cursor,
        Cols,
        Rows,
        Data,
        ScalarFormatType,
        RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT,
        FieldsWidth,
        true);

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += Result.Extent.X + Layout->ContentsPadding.X;
    TitleOffset.Y += gUIContext->InputFieldPadding.Y;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Vec2 TotalExtent = {
        Result.Extent.X + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        Result.Extent.Y,
    };

    Rr_UIAdvance(TotalExtent);

    return Result.Edited;
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

    Rr_Vec2 TextSize = Rr_UICalculateTextSize(SIZE_MAX, Buffer, 0.0f, 0);
    if (Placeholder)
    {
        TextSize.X = RR_MAX(
            TextSize.X,
            Rr_UICalculateTextSize(SIZE_MAX, Placeholder, 0.0f, 0).X);
    }

    float FieldWidth = Rr_UISetupFlexibleWidget(
        Layout,
        TitleLength,
        Title,
        TextSize.X + gUIContext->InputFieldPadding.X * 2.0f);

    Rr_UIInputFieldResult Result = Rr_UIGenericInputField(
        TitleHash,
        Layout->Cursor,
        BufferCapacity,
        Buffer,
        Placeholder,
        FilterFunc,
        Flags,
        FieldWidth,
        true);

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += Result.Extent.X + Layout->ContentsPadding.X;
    TitleOffset.Y += gUIContext->InputFieldPadding.Height;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Vec2 TotalExtent = {
        Result.Extent.X + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        Result.Extent.Y,
    };

    Rr_UIAdvance(TotalExtent);

    Rr_DestroyScratch(Scratch);

    return Result.Edited;
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
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT);
}

bool Rr_UIInputFloat2(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        2,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT);
}

bool Rr_UIInputFloat3(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        3,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT);
}

bool Rr_UIInputFloat4(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        4,
        1,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT);
}

bool Rr_UIInputFloat2x2(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        2,
        2,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT);
}

bool Rr_UIInputFloat3x3(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        3,
        3,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT);
}

bool Rr_UIInputFloat4x4(const char *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        4,
        4,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT);
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
    Rr_UIHash Hash,
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
    Rr_UISetNextWindowOpenOffset(Position);
    Rr_UIBeginPopupWindow(Hash, POPUP_WINDOW_FLAGS);

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

        Rr_UIDrawQuadVertices(Vertices);

        Vertices[0].Color = Rr_V4F(0.0f);
        Vertices[1].Color = Rr_V4F(0.0f);
        Vertices[2].Color = Rr_V4(0.0f, 0.0f, 0.0f, 1.0f);
        Vertices[3].Color = Rr_V4(0.0f, 0.0f, 0.0f, 1.0f);

        Rr_UIDrawQuadVertices(Vertices);
    }

    Rr_UIDrawInnerFrame(
        &(Rr_Rect){ Layout->Cursor, Rr_V2F(TargetSize) },
        gUIContext->FrameThickness,
        &gUIContext->Colors.Outline);

    float SVSelectorCircleSize = TargetSize * 0.035f;

    Rr_UIHash SVSelectorHash =
        Rr_UIGetHash(sizeof("SVSelector"), "SVSelector", Rr_UICurrentHash());

    Rr_Rect SVSelectorRect = {
        .Offset = Layout->Cursor,
        .Extent = Rr_V2F(TargetSize),
    };

    Rr_UIClickResult ClickResult =
        Rr_UIClickDrag(Layout, &SVSelectorRect, SVSelectorHash, Rr_V2F(0.0f));

    if (ClickResult.ClickCount || ClickResult.Held)
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

    bool SVSelectorHeld = ClickResult.Held;
    Rr_Vec2 SVSelectorCircleOffset = Rr_AddV2(
        Layout->Cursor,
        Rr_V2(StaticHSV.Y * TargetSize, (1.0f - StaticHSV.Z) * TargetSize));

    Rr_UIAdvance(Rr_V2F(TargetSize));

    Rr_UIHash HSelectorHash =
        Rr_UIGetHash(sizeof("HSelector"), "HSelector", Rr_UICurrentHash());

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
        &gUIContext->Colors.Outline);

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
        &(Rr_Vec4){ 0.0f, 0.0f, 0.0f, 1.0f });
    Rr_UIDrawFitTriangleFilled(
        LeftTriangleOffset,
        TriangleSize,
        RR_ANGLE_DEG(0.0f),
        &gUIContext->Colors.Foreground);
    Rr_Vec2 RightTriangleOffset = Rr_V2(
        Layout->Cursor.X + HSelectorWidth - TriangleSize * 0.5f,
        Layout->Cursor.Y + StaticHSV.X * TargetSize);
    Rr_UIDrawFitTriangleFilled(
        RightTriangleOffset,
        TriangleSize + TriangleOutline,
        RR_ANGLE_DEG(180.0f),
        &(Rr_Vec4){ 0.0f, 0.0f, 0.0f, 1.0f });
    Rr_UIDrawFitTriangleFilled(
        RightTriangleOffset,
        TriangleSize,
        RR_ANGLE_DEG(180.0f),
        &gUIContext->Colors.Foreground);

    ClickResult =
        Rr_UIClickDrag(Layout, &HSelectorRect, HSelectorHash, Rr_V2F(0.0f));

    if (ClickResult.ClickCount || ClickResult.Held)
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

    /* Draw saturation/value circle here so it's on top of hue selector. */

    float CircleOutlineThickness = 3.0f;
    if (SVSelectorHeld)
    {
        Rr_UIDrawCircleFilled(
            SVSelectorCircleOffset,
            SVSelectorCircleSize,
            &OpaqueColor);
    }
    Rr_UIDrawCircle(
        SVSelectorCircleOffset,
        SVSelectorCircleSize,
        CircleOutlineThickness,
        &(Rr_Vec4){ 0.0f, 0.0f, 0.0f, 1.0f });
    Rr_UIDrawCircle(
        SVSelectorCircleOffset,
        SVSelectorCircleSize,
        CircleOutlineThickness / 2.0f,
        &gUIContext->Colors.Foreground);

    /* Various input fields. */

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
                RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT |
                RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT))
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

        HSVChanged = false;
        StaticHSV = Rr_UIRGBToHSV((Rr_Vec3 *)Channels);
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

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    Rr_Vec2 ColorBoxExtent =
        Rr_V2F(gUIContext->LineHeight + gUIContext->InputFieldPadding.Y * 2.0f);

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    float ColorBoxWithMargin = gUIContext->ComponentMargin + ColorBoxExtent.X;

    float FieldsWidth = Rr_UICalculateGenericInputScalarMultiWidth(
        ChannelCount,
        1,
        Channels,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT);
    FieldsWidth += ColorBoxWithMargin;

    FieldsWidth =
        Rr_UISetupFlexibleWidget(Layout, TitleLength, Title, FieldsWidth);

    Rr_UIInputFieldResult InputResult = Rr_UIGenericInputScalarMulti(
        TitleHash,
        Layout->Cursor,
        ChannelCount,
        1,
        Channels,
        RR_UI_SCALAR_FORMAT_TYPE_FLOAT,
        RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT,
        FieldsWidth - ColorBoxWithMargin,
        true);

    Rr_Vec2 ColorBoxOffset = Layout->Cursor;
    ColorBoxOffset.X += InputResult.Extent.X + gUIContext->ComponentMargin;

    Rr_UIClickResult ClickResult = Rr_UIClickSimple(
        Layout,
        &(Rr_Rect){
            ColorBoxOffset,
            ColorBoxExtent,
        },
        TitleHash);

    if (ClickResult.ClickCount)
    {
        gUIContext->PopupWindowParent = Window;
        gUIContext->PopupWindowHash = TitleHash;
    }

    bool ColorChanged = false;

    if (gUIContext->PopupWindowParent == Window &&
        gUIContext->PopupWindowHash == TitleHash)
    {
        Rr_Vec2 PopupCenter =
            Rr_AddV2(ColorBoxOffset, Rr_DivV2F(ColorBoxExtent, 2.0f));
        Rr_UIColorPickerPopup(TitleHash, PopupCenter, ChannelCount, Channels);
    }

    Rr_Rect ColorBoxRect = {
        ColorBoxOffset,
        ColorBoxExtent,
    };
    Rr_Vec4 OpaqueColor;
    memcpy(&OpaqueColor, Channels, sizeof(float) * (size_t)ChannelCount);
    OpaqueColor.A = 1.0f;
    Rr_UIDrawBevel(
        &ColorBoxRect,
        &OpaqueColor,
        ClickResult.Held && ClickResult.Hovered);
    if (ChannelCount == 4)
    {
        Rr_Rect InnerRect =
            Rr_ResizeRect(&ColorBoxRect, -gUIContext->BevelThickness);
        Rr_UIDrawCheckerQuad(&InnerRect, gUIContext->FontSize * 0.5f);
        Rr_UIDrawVerticalGradientQuad(
            &InnerRect,
            (Rr_Vec4 *)Channels,
            &OpaqueColor);
    }

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += InputResult.Extent.X + gUIContext->ComponentMargin +
                     ColorBoxExtent.X + Layout->ContentsPadding.X;
    TitleOffset.Y += gUIContext->InputFieldPadding.Y;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Vec2 TotalExtent = {
        InputResult.Extent.X + ColorBoxWithMargin +
            gUIContext->FlexibleTitleMargin + TitleExtent.X,
        InputResult.Extent.Y,
    };

    Rr_UIAdvance(TotalExtent);

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

    /* Rr_Scratch Scratch = Rr_GetScratch(NULL); */

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_UIPrimitive Primitive = Rr_UIReserveBevel();

    Rr_Vec2 ButtonPosition = Layout->Cursor;

    Rr_Vec2 SelectedTextPosition =
        Rr_AddV2(ButtonPosition, gUIContext->InputFieldPadding);
    Rr_Vec2 SelectedTextSize = Rr_UIDrawText(
        false,
        SelectedTextPosition,
        SIZE_MAX,
        Options[*SelectedIndex],
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    float ButtonWidth = Rr_UISetupFlexibleWidget(
        Layout,
        TitleLength,
        Title,
        SelectedTextSize.X + gUIContext->InputFieldPadding.X * 2.0f +
            gUIContext->LineHeight + gUIContext->InputFieldPadding.Y * 2.0f);
    float ButtonHeight =
        SelectedTextSize.Height + gUIContext->InputFieldPadding.Y * 2.0f;

    Rr_Vec2 ButtonExtent = {
        ButtonWidth,
        ButtonHeight,
    };

    Rr_UIClickResult ClickResult = Rr_UIClickSimple(
        Layout,
        &(Rr_Rect){
            ButtonPosition,
            ButtonExtent,
        },
        TitleHash);

    if (ClickResult.ClickCount)
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
        PopupPosition.Y += ButtonExtent.Height + gUIContext->FrameThickness;
        PopupPosition.X += gUIContext->FrameThickness;
        Rr_UISetNextWindowOffset(PopupPosition);
        Rr_UISetNextWindowPadding(Rr_V2(gUIContext->InputFieldPadding.X, 0.0f));
        Rr_UIBeginPopupWindow(TitleHash, POPUP_WINDOW_FLAGS);
        Rr_UILayout *PopupLayout = Rr_UICurrentLayout();
        for (uint32_t Index = 0; Index < OptionCount; ++Index)
        {
            Rr_UIPrimitive OptionButtonQuad = Rr_UIReserveQuad();
            size_t OptionLength;
            Rr_UIHash OptionHash =
                Rr_UIGetTitleHash(Options[Index], &OptionLength);
            Rr_Vec2 OptionSize = Rr_UIDrawText(
                false,
                PopupLayout->Cursor,
                OptionLength,
                Options[Index],
                0,
                &gUIContext->Colors.Foreground,
                0);
            Rr_Rect OptionButtonRect;
            OptionButtonRect.Offset.Y = PopupLayout->Cursor.Y;
            OptionButtonRect.Offset.X =
                PopupLayout->Cursor.X - gUIContext->InputFieldPadding.Width;
            OptionButtonRect.Extent.Width =
                gUIContext->PopupWindow.Rect.Extent.Width;
            OptionButtonRect.Extent.Height = gUIContext->LineHeight;
            Rr_UIClickResult OptionClickResult =
                Rr_UIClickSimple(PopupLayout, &OptionButtonRect, OptionHash);
            if (OptionClickResult.ClickCount)
            {
                *SelectedIndex = Index;
                Rr_UIClosePopupWindow();
                OptionChanged = true;
            }
            Rr_Vec4 OptionButtonColor;
            if (OptionClickResult.Held)
            {
                OptionButtonColor = gUIContext->Colors.ButtonHeld;
            }
            else if (OptionClickResult.Hovered)
            {
                OptionButtonColor = gUIContext->Colors.ButtonHovered;
            }
            else
            {
                OptionButtonColor = gUIContext->Colors.ButtonNormal;
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
        ButtonExtent,
    };
    Rr_Vec4 BackgroundColor = gUIContext->Colors.Background;
    BackgroundColor.XYZ = Rr_MulV3F(BackgroundColor.XYZ, 0.9f);

    Rr_UIBevel(
        Primitive,
        &ButtonRect,
        &gUIContext->Colors.InputFieldNormal,
        ClickResult.Held);

    /* Add handle. */
    {
        float HandleSize = ButtonExtent.Height;
        Rr_Rect HandleRect = { ButtonRect.Offset, Rr_V2F(HandleSize) };
        HandleRect.Offset.X += ButtonRect.Extent.Width - ButtonExtent.Y;

        Rr_UIDrawBevel(
            &HandleRect,
            &gUIContext->Colors.ButtonNormal,
            ClickResult.Held);

        Rr_Vec2 TriangleCenter = Rr_RectCenter(&HandleRect);
        float TriangleSize = gUIContext->TitleHeight * 0.3f;
        Rr_UIDrawFitTriangleFilled(
            TriangleCenter,
            TriangleSize,
            !Window->Collapsed ? RR_ANGLE_DEG(90.0f) : 0.0f,
            &gUIContext->Colors.Foreground);
    }

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += ButtonWidth + Layout->ContentsPadding.X;
    TitlePosition.Y += gUIContext->InputFieldPadding.Y;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Vec2 TotalExtent = {
        ButtonExtent.X + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        gUIContext->LineHeight + gUIContext->InputFieldPadding.Y * 2.0f,
    };

    Rr_UIAdvance(TotalExtent);

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

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    /* NOTE: Reserve three handles worth of width just in case. Probably could
     * also use value text width. */
    float MinSliderWidth = gUIContext->FontSize * 10.0f;

    float SliderWidth =
        Rr_UISetupFlexibleWidget(Layout, TitleLength, Title, MinSliderWidth);

    Rr_Rect SliderRect = {
        Layout->Cursor,
        {
            SliderWidth,
            gUIContext->LineHeight + gUIContext->InputFieldPadding.Y * 2.0f,
        },
    };

    Rr_UIPrimitive BackgroundBevel = Rr_UIReserveBevel();

    float HandleWidth = gUIContext->FontSize;
    Rr_Rect HandleRect = { Layout->Cursor,
                           Rr_V2(HandleWidth, SliderRect.Extent.Y) };
    HandleRect.Offset.X += Normalized * (SliderWidth - HandleWidth);
    HandleRect.Offset.X = roundf(HandleRect.Offset.X);
    HandleRect = Rr_ResizeRect(&HandleRect, -gUIContext->BevelThickness);
    Rr_UIDrawBevel(&HandleRect, &gUIContext->Colors.ButtonNormal, false);

    if (ValueCString != NULL)
    {
        Rr_Vec2 ValueSize =
            Rr_UICalculateTextSize(ValueCStringLength, ValueCString, 0.0f, 0);

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
                &gUIContext->Colors.Foreground,
                0);
        }
    }

    float HandleDragOffset = gUIContext->MousePosition.X -
                             (HandleRect.Offset.X + HandleWidth / 2.0f);

    Rr_UIClickResult ClickResult = Rr_UIClickDrag(
        Layout,
        &SliderRect,
        TitleHash,
        Rr_V2(HandleDragOffset, 0.0f));

    Rr_UIBevel(
        BackgroundBevel,
        &SliderRect,
        ClickResult.Held ? &gUIContext->Colors.InputFieldActive
                         : &gUIContext->Colors.InputFieldNormal,
        true);

    if (ClickResult.ClickCount || ClickResult.Moved)
    {
        float SliderMin = Layout->Cursor.X + HandleWidth / 2.0f;
        float SliderMax = SliderMin + SliderWidth - HandleWidth;

        Normalized =
            (gUIContext->MousePosition.X - SliderMin) / (SliderMax - SliderMin);
    }
    else if (ClickResult.Hovered)
    {
        if (gUIContext->MouseWheelDelta.X != 0.0f &&
            gUIContext->MouseWheelDelta.Y == 0.0f)
        {
            /* NOTE: Probably shouldn't be hardcoded to 30.0f. */
            Normalized += gUIContext->MouseWheelDelta.X / 30.0f;
        }
    }
    Normalized = RR_CLAMP(0.0f, Normalized, 1.0f);

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += SliderRect.Extent.X + Layout->ContentsPadding.X;
    TitleOffset.Y += gUIContext->InputFieldPadding.Y;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground,
        0);

    Rr_Vec2 TotalExtent = {
        SliderRect.Extent.X + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        SliderRect.Extent.Y,
    };

    Rr_UIAdvance(TotalExtent);

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
    int Length = snprintf(Buffer, 32, Rr_UICurrentFloatFormatString(), *Value);

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
    return gUIContext && (gUIContext->LeftMouseButton.DownOverWindow ||
                          gUIContext->HoveredWindow);
}

bool Rr_UIWantKeyboardCapture(void)
{
    return false;
}

static void Rr_UIConvertColorsToSRGB(void)
{
    size_t ColorCount = sizeof(Rr_UIColors) / sizeof(Rr_Vec4);
    Rr_Vec4 *Colors = (Rr_Vec4 *)&gUIContext->Colors;
    for (size_t Index = 0; Index < ColorCount; ++Index)
    {
        Rr_UIToSRGBColor(&Colors[Index]);
    }
}

static void Rr_UIConvertColorsToLinear(void)
{
    size_t ColorCount = sizeof(Rr_UIColors) / sizeof(Rr_Vec4);
    Rr_Vec4 *Colors = (Rr_Vec4 *)&gUIContext->Colors;
    for (size_t Index = 0; Index < ColorCount; ++Index)
    {
        Rr_UIToLinearColor(&Colors[Index]);
    }
}

static inline void Rr_UIPrintColor(const char *Name, Rr_Vec4 *Color)
{
    fprintf(
        stdout,
        ".%s = {%ff,%ff,%ff,%ff},\n",
        Name,
        Color->R,
        Color->G,
        Color->B,
        Color->A);
}

void Rr_UIPrintColors(void)
{
    Rr_UIPrintColor("Foreground", &gUIContext->Colors.Foreground);
    Rr_UIPrintColor("ForegroundDimmed", &gUIContext->Colors.ForegroundDimmed);
    Rr_UIPrintColor("Background", &gUIContext->Colors.Background);
    Rr_UIPrintColor("ChildBackground", &gUIContext->Colors.ChildBackground);
    Rr_UIPrintColor("Outline", &gUIContext->Colors.Outline);

    Rr_UIPrintColor("TitleBackground", &gUIContext->Colors.TitleBackground);
    Rr_UIPrintColor(
        "TitleCloseButtonBackground",
        &gUIContext->Colors.TitleCloseButtonBackground);
    Rr_UIPrintColor(
        "TitleCollapseButtonBackground",
        &gUIContext->Colors.TitleCollapseButtonBackground);

    Rr_UIPrintColor(
        "ScrollbarBackground",
        &gUIContext->Colors.ScrollbarBackground);
    Rr_UIPrintColor("ScrollbarNormal", &gUIContext->Colors.ScrollbarNormal);
    Rr_UIPrintColor("ScrollbarHovered", &gUIContext->Colors.ScrollbarHovered);
    Rr_UIPrintColor("ScrollbarHeld", &gUIContext->Colors.ScrollbarHeld);

    Rr_UIPrintColor("ButtonNormal", &gUIContext->Colors.ButtonNormal);
    Rr_UIPrintColor("ButtonHovered", &gUIContext->Colors.ButtonHovered);
    Rr_UIPrintColor("ButtonHeld", &gUIContext->Colors.ButtonHeld);
    Rr_UIPrintColor("ButtonDisabled", &gUIContext->Colors.ButtonDisabled);

    Rr_UIPrintColor("InputFieldNormal", &gUIContext->Colors.InputFieldNormal);
    Rr_UIPrintColor("InputFieldActive", &gUIContext->Colors.InputFieldActive);
    Rr_UIPrintColor(
        "SelectedTextBackground",
        &gUIContext->Colors.SelectedTextBackground);
}

void Rr_InitUI(void)
{
    assert(gUIContext == NULL);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gUIContext = RR_ALLOC_TYPE(Arena, Rr_UIContext);
    gUIContext->Arena = Arena;

    gUIContext->NextWindowOffset = Rr_V2F(INFINITY);
    gUIContext->NextWindowOpenOffset = Rr_V2F(INFINITY);
    gUIContext->NextWindowExtent = Rr_V2F(INFINITY);
    gUIContext->NextWindowPadding = Rr_V2F(INFINITY);

    Rr_IntVec2 DisplaySize = Rr_GetDisplaySize();
    float DefaultFontSize = (float)DisplaySize.Width / 112.0f;
    Rr_UISetFontSize(DefaultFontSize);

    gUIContext->Style = (Rr_UIStyle){
        .TitlePadding = { 0.5f, 0.03f },
        .ContentsPadding = { 0.5f, 0.5f },
        .ComponentMargin = 0.2f,
        .FlexibleTitleMargin = 0.5f,
        .BevelIntensityLight = 0.3f,
        .BevelIntensityDark = 0.7f,
        .ButtonPadding = { 0.25f, 0.03f },
        .InputFieldPadding = { 0.2f, 0.05f },
    };

    gUIContext->Colors = (Rr_UIColors){
        .Foreground = { 0.899630f, 0.924908f, 0.933333f, 1.000000f },
        .ForegroundDimmed = { 0.617390f, 0.649893f, 0.660727f, 1.000000f },
        .Background = { 0.142532f, 0.168879f, 0.186284f, 1.000000f },
        .ChildBackground = { 0.100193f, 0.121744f, 0.136111f, 1.000000f },
        .Outline = { 0.556805f, 0.571476f, 0.586111f, 1.000000f },
        .TitleBackground = { 0.123951f, 0.467914f, 0.697222f, 1.000000f },
        .TitleCloseButtonBackground = { 0.839551f,
                                        0.250613f,
                                        0.313724f,
                                        1.000000f },
        .TitleCollapseButtonBackground = { 0.121569f,
                                           0.466667f,
                                           0.694118f,
                                           1.000000f },
        .ScrollbarBackground = { 0.070520f, 0.093346f, 0.111383f, 1.000000f },
        .ScrollbarNormal = { 0.292805f, 0.334535f, 0.363504f, 1.000000f },
        .ScrollbarHovered = { 0.408665f, 0.497836f, 0.557879f, 1.000000f },
        .ScrollbarHeld = { 0.254856f, 0.342832f, 0.400486f, 1.000000f },
        .ButtonNormal = { 0.165679f, 0.361108f, 0.488889f, 1.000000f },
        .ButtonHovered = { 0.408665f, 0.497836f, 0.557879f, 1.000000f },
        .ButtonHeld = { 0.254856f, 0.342832f, 0.400486f, 1.000000f },
        .ButtonDisabled = { 0.070520f, 0.093346f, 0.111383f, 1.000000f },
        .InputFieldNormal = { 0.089074f, 0.160347f, 0.216667f, 1.000000f },
        .InputFieldActive = { 0.070324f, 0.252329f, 0.408333f, 1.000000f },
        .SelectedTextBackground = { 0.433129f,
                                    0.652866f,
                                    0.996207f,
                                    1.000000f },
    };

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
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
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

    Rr_PipelineSpecialization Specializations[1] = {
        {
            .ConstantID = 0,
            .Size = sizeof(uint32_t),
        },
    };

    Rr_GraphicsPipelineCreateInfo PipelineInfo = {
        .Layout = gUIContext->PipelineLayout,
        .VertexShaderSPVSize = VertexShader.Size,
        .VertexShaderSPVData = VertexShader.Pointer,
        .VertexSpecializationCount = RR_ARRAY_COUNT(Specializations),
        .VertexSpecializations = Specializations,
        .FragmentShaderSPVSize = FragmentShader.Size,
        .FragmentShaderSPVData = FragmentShader.Pointer,
        .ColorTargetCount = RR_ARRAY_COUNT(ColorTargets),
        .ColorTargets = ColorTargets,
        .VertexInputBindingCount = 1,
        .VertexInputBindings = &VertexInputBinding,
    };

    const uint32_t DONT_CONVERT_TO_SRGB = 0;
    Specializations[0].Data = &DONT_CONVERT_TO_SRGB;
    gUIContext->LinearPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    const uint32_t CONVERT_TO_SRGB = 1;
    Specializations[0].Data = &CONVERT_TO_SRGB;
    gUIContext->SRGBPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

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
        .AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    });

    gUIContext->Font =
        Rr_UICreateFont(RR_BUILTIN_SOURCESERIF4_TTF, DefaultFontSize);
}

void Rr_CleanupUI(void)
{
    assert(gUIContext != NULL);

    Rr_ReleaseBuffer(gUIContext->VertexBuffer);
    Rr_ReleaseBuffer(gUIContext->IndexBuffer);
    Rr_ReleaseBuffer(gUIContext->UniformBuffer);
    Rr_ReleaseSampler(gUIContext->Sampler);
    Rr_ReleasePipelineLayout(gUIContext->PipelineLayout);
    Rr_ReleaseGraphicsPipeline(gUIContext->LinearPipeline);
    Rr_ReleaseGraphicsPipeline(gUIContext->SRGBPipeline);

    Rr_UIReleaseFont(gUIContext->Font);

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
                gUIContext->LeftMouseButton.Clicks = Event->MouseButton.Clicks;
                gUIContext->LeftMouseButton.Down = true;
                gUIContext->LeftMouseButton.Held = true;
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_UP:
        {
            if (Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                gUIContext->LeftMouseButton.Up = true;
                gUIContext->LeftMouseButton.Held = false;
                gUIContext->LeftMouseButton.ClickID++;
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
    if (gUIContext->NextFontSize == INFINITY)
    {
        return;
    }

    Rr_UIStyle *Style = &gUIContext->Style;
    float FontSize = gUIContext->NextFontSize;

    gUIContext->NextFontSize = INFINITY;
    gUIContext->FontSize = FontSize;
    gUIContext->LineHeight = FontSize * 1.2f;
    gUIContext->ContentsPadding =
        Rr_MulV2F(gUIContext->Style.ContentsPadding, FontSize);
    gUIContext->ComponentMargin =
        RR_UI_ROUND(Style->ComponentMargin * FontSize);
    gUIContext->FlexibleTitleMargin =
        RR_UI_ROUND(Style->FlexibleTitleMargin * FontSize);

    gUIContext->FrameThickness = floorf(RR_MAX(1.0f, FontSize * 0.075f));
    gUIContext->ResizeHandleSize = RR_UI_ROUND(FontSize);
    gUIContext->ScrollbarWidth = gUIContext->ResizeHandleSize;
    gUIContext->ScrollbarHandleWidth =
        RR_UI_ROUND(gUIContext->ResizeHandleSize * 0.75f);
    gUIContext->SeparatorLineHeight = gUIContext->LineHeight * 0.5f;
    gUIContext->ButtonPadding = RR_UI_ROUND_V2(
        Rr_MulV2F(gUIContext->Style.ButtonPadding, gUIContext->LineHeight));
    gUIContext->BevelThickness = ceilf(FontSize * 0.1f);
    gUIContext->InputFieldPadding = RR_UI_ROUND_V2(
        Rr_MulV2F(gUIContext->Style.InputFieldPadding, gUIContext->LineHeight));

    gUIContext->TitleHeight = RR_UI_ROUND(
        gUIContext->Style.TitlePadding.Height * 2.0f * FontSize +
        gUIContext->LineHeight);
    gUIContext->TitleButtonSize = RR_UI_ROUND(gUIContext->TitleHeight);
    gUIContext->TitlePadding =
        Rr_MulV2F(gUIContext->Style.TitlePadding, FontSize);
    gUIContext->MinWindowSizeNoTitle =
        Rr_MulV2F(gUIContext->ContentsPadding, 2.0f);
    gUIContext->MinWindowSizeNoTitle.X += gUIContext->ScrollbarWidth;
    gUIContext->MinWindowSizeNoTitle.X += FontSize * 2.0f;
    gUIContext->MinWindowSizeNoTitle.Y += FontSize * 2.0f;
    gUIContext->MinWindowSizeNoTitle =
        RR_UI_ROUND_V2(gUIContext->MinWindowSizeNoTitle);
    gUIContext->MinWindowSize = gUIContext->MinWindowSizeNoTitle;
    gUIContext->MinWindowSize.Y += gUIContext->TitleHeight;
    gUIContext->MinWindowSize = RR_UI_ROUND_V2(gUIContext->MinWindowSize);
}

void Rr_NewUIFrame(void)
{
    gUIContext->FrameArena = gRenderer->Frames[gRenderer->FrameIndex].Arena;

    RR_RESET_ARRAY(&gUIContext->Vertices, gUIContext->FrameArena);
    RR_RESET_ARRAY(&gUIContext->Indices, gUIContext->FrameArena);

    Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());
    gUIContext->ScreenSize.Width = (float)SwapchainSize.Width;
    gUIContext->ScreenSize.Height = (float)SwapchainSize.Height;
}

void Rr_BeginUI(void)
{
    Rr_UIConsumeNextFontSize();

    if (gUIContext->LeftMouseButton.SkipUp && gUIContext->LeftMouseButton.Up)
    {
        gUIContext->LeftMouseButton.SkipUp = false;
        gUIContext->LeftMouseButton.Up = false;
    }

    gUIContext->HoveredWindow = NULL;
    if (Rr_UIPopupWindowActive())
    {
        if (Rr_RectContains(
                &gUIContext->PopupWindow.VisibleRect,
                gUIContext->MousePosition))
        {
            gUIContext->HoveredWindow = &gUIContext->PopupWindow;

            if (gUIContext->LeftMouseButton.Down)
            {
                gUIContext->LeftMouseButton.DownOverWindow = true;
            }
        }
        else if (gUIContext->LeftMouseButton.Down)
        {
            Rr_UIClosePopupWindow();
            gUIContext->LeftMouseButton.SkipUp = true;
        }
    }
    else
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

                if (gUIContext->LeftMouseButton.Down)
                {
                    Rr_UIPutWindowOnTop(Window->TopLevelParent);

                    gUIContext->LeftMouseButton.DownOverWindow = true;
                }

                break;
            }
        }
    }

    if (!gUIContext->LeftMouseButton.DownOverWindow &&
        gUIContext->LeftMouseButton.Down)
    {
        Rr_UIResetClick();
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
    assert(
        gUIContext->HashStack.Count == 0 &&
        "ID/Hash stack is not empty; did you forget to call Rr_UIPopID()?");

    Rr_UIAssertNoWindow();

    if (gUIContext->ActiveWindows.Count > 0)
    {
        RR_BEGIN_FRAME_SECTION("Rr.UI.DrawWindows");

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        bool IsSRGBSwapchain =
            Rr_IsSRGBFormat(Rr_GetImageFormat(SwapchainImage));

        Rr_UIUniformData UniformData = {
            .ScreenSize = gUIContext->ScreenSize,
        };
        char *MappedUniformData =
            Rr_GetMappedBufferData(gUIContext->UniformBuffer);
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
        Rr_BindGraphicsPipeline(
            GraphicsNode,
            IsSRGBSwapchain ? gUIContext->SRGBPipeline
                            : gUIContext->LinearPipeline);
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
            gUIContext->Font->Image,
            gUIContext->Sampler,
            0,
            1);

        qsort(
            gUIContext->ActiveWindows.Data,
            gUIContext->ActiveWindows.Count,
            sizeof(Rr_UIWindow *),
            Rr_UIWindowSort);

        gUIContext->HighestWindow =
            RR_LAST_ARRAY_ELEMENT(&gUIContext->ActiveWindows);

        for (size_t Index = 0; Index < gUIContext->ActiveWindows.Count; ++Index)
        {
            Rr_UIWindow *Window = gUIContext->ActiveWindows.Data[Index];
            Window->Z = (int32_t)Index;
            Window->Added = false;
            Window->OpenedThisFrame = false;
            Window->CreatedThisFrame = false;
            if (Window->SkipThisFrame)
            {
                Window->SkipThisFrame = false;
                continue;
            }
            if (Window->TopLevelParent == Window)
            {
                Rr_UIDrawWindow(Window, GraphicsNode);
                Window->ShownAtLeastOnce = true;
            }
        }

        Rr_EndGraphLabel(Rr_GetGraph(), "Rr.UI");

        RR_END_FRAME_SECTION("Rr.UI.DrawWindows");
    }

    gUIContext->LayoutStack = NULL;
    gUIContext->ClickConsumed = false;
    if (gUIContext->LeftMouseButton.Up)
    {
        gUIContext->LeftMouseButton.Held = false;
        gUIContext->LeftMouseButton.DownOverWindow = false;
    }
    gUIContext->LeftMouseButton.Clicks = 0;
    gUIContext->LeftMouseButton.Down = false;
    gUIContext->LeftMouseButton.Up = false;
    gUIContext->MouseMoved = false;
    gUIContext->MouseWheelDelta = Rr_V2F(0.0f);
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
    Rr_UITextF(
        "%s: commited %d bytes, position %p",
        Comment,
        Arena->Commited,
        (Arena + Arena->Position));
}

void Rr_UIDebugOverlay(void)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    if (Rr_UIBeginWindow(
            "Rr.DebugOverlay",
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
            Rr_UITextF(
                "Time: %.2f\n"
                "Mouse Position: %.2f %.2f\n"
                "Mouse Delta: %.2f %.2f",
                Rr_GetTimeSeconds(),
                MousePosition.X,
                MousePosition.Y,
                MouseDelta.X,
                MouseDelta.Y);
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
            Rr_UIInputUnsignedInt(
                "Target Frame Rate",
                &gApp->FrameTime.TargetFrameRate);

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
                Rr_UITextF("FPS: %.2f", LastFPS);
            }

            double MainLoopMS =
                (double)(RR_GET_FRAME_SECTION("Rr.MainLoop") * 1000) /
                (double)Rr_GetPerformanceFrequency();
            double FrameGraphMS =
                (double)(RR_GET_FRAME_SECTION("Rr.FrameGraph") * 1000) /
                (double)Rr_GetPerformanceFrequency();

            Rr_UITextF(
                "Main Loop: %.3fms\n"
                "Frame Graph: %.3fms",
                MainLoopMS,
                FrameGraphMS);

            if (gRenderer->GraphicsQueue.TimestampsEnabled)
            {
                Rr_UITextF("GPU: %.3fms", gRenderer->LastFrameMS);
            }
            else
            {
                Rr_UITextF(
                    "GPU timestamps not supported!",
                    gRenderer->LastFrameMS);
            }

            if (Rr_UIButton("Toggle Fullscreen"))
            {
                Rr_ToggleWindowFullscreen();
            }
        }
        if (Rr_UITab("UI"))
        {
            Rr_UITextF(
                "UI Font Size: %.2f\n"
                "Vertices Capacity: %zu\n"
                "Indices Capacity: %zu",
                gUIContext->FontSize,
                gUIContext->Vertices.Capacity,
                gUIContext->Indices.Capacity);

            double DrawWindowsMS =
                (double)(RR_GET_FRAME_SECTION("Rr.UI.DrawWindows") * 1000) /
                (double)Rr_GetPerformanceFrequency();
            double DrawTextMS =
                (double)(RR_GET_FRAME_SECTION("Rr.UI.DrawText") * 1000) /
                (double)Rr_GetPerformanceFrequency();
            double DrawInputTextMS =
                (double)(RR_GET_FRAME_SECTION("Rr.UI.DrawInputText") * 1000) /
                (double)Rr_GetPerformanceFrequency();

            Rr_UITextF(
                "DrawWindows: %.3fms\n"
                "DrawText: %.3fms\n"
                "DrawInputText: %.3fms",
                DrawWindowsMS,
                DrawTextMS,
                DrawInputTextMS);

            Rr_UITextF(
                "Hovered Window: %s\n"
                "Active Windows: %b\n"
                "Popup Window Open: %b",
                gUIContext->HoveredWindow ? gUIContext->HoveredWindow->Title
                                          : "NULL",
                gUIContext->ActiveWindows.Count,
                gUIContext->PopupWindowOpen);

            Rr_UICheckbox("Visualize Advances", &gUIContext->VisualizeAdvances);
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
            Rr_UITextF("Frame: %zu", gRenderer->FrameNumber);
            Rr_UITextF(
                "Images: %zu/%zu",
                gRenderer->Images.Count,
                gRenderer->Images.Capacity);
            Rr_UITextF(
                "Buffers: %zu/%zu",
                gRenderer->Buffers.Count,
                gRenderer->Buffers.Capacity);
            Rr_UITextF(
                "DescriptorSetLayouts: %zu/%zu",
                gRenderer->DescriptorSetLayoutStorage.Hive.Count,
                gRenderer->DescriptorSetLayoutStorage.Hive.Capacity);
            Rr_UITextF(
                "DescriptorPools: %zu",
                gRenderer->DescriptorPoolListCount);
            Rr_UITextF(
                "PipelineLayouts: %zu/%zu",
                gRenderer->PipelineLayouts.Count,
                gRenderer->PipelineLayouts.Capacity);
            Rr_UITextF(
                "ComputePipelines: %zu/%zu",
                gRenderer->ComputePipelines.Count,
                gRenderer->ComputePipelines.Capacity);
            Rr_UITextF(
                "GraphicsPipelines: %zu/%zu",
                gRenderer->GraphicsPipelines.Count,
                gRenderer->GraphicsPipelines.Capacity);
            Rr_UITextF(
                "Samplers: %zu/%zu",
                gRenderer->Samplers.Count,
                gRenderer->Samplers.Capacity);
            Rr_UITextF(
                "Render Passes: %zu/%zu",
                gRenderer->RenderPassStorage.Hive.Count,
                gRenderer->RenderPassStorage.Hive.Capacity);
            Rr_UITextF(
                "Framebuffers: %zu/%zu",
                gRenderer->FramebufferStorage.Hive.Count,
                gRenderer->FramebufferStorage.Hive.Capacity);
            Rr_UITextF(
                "SwapchainImages: %zu",
                gRenderer->SwapchainImages.Count);
            Rr_UITextF(
                "SyncStates: %zu/%zu",
                gRenderer->SyncStateStorage.Hive.Count,
                gRenderer->SyncStateStorage.Hive.Capacity);
        }
        Rr_UIEndTabs();
        Rr_UIEndWindow();
    }

    Rr_DestroyScratch(Scratch);
}
