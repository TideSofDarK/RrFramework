/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Rr_UI.h"

#include "Rr_BuiltinAssets.inc"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_UI
#include "Rr_LogMacro.h"

#include "Rr_App.h"
#include "Rr_Hash.h"
#include "Rr_Image.h"
#include "Rr_Platform.h"
#include "Rr_Renderer.h"
#include "Rr_System.h"
#include "Rr_Thread.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>
#include <Rr/Rr_Utility.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb/stb_truetype.h>

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#define RR_UI_SCALAR_BUFFER_SIZE 32

static Rr_Vec4 const RR_UI_VEC4_NEG = { -1.0f, -1.0f, -1.0f, -1.0f };
static Rr_Vec4 const RR_UI_VEC4_ZERO = { 0.0f, 0.0f, 0.0f, 0.0f };
static Rr_Vec4 const RR_UI_VEC4_ONE = { 1.0f, 1.0f, 1.0f, 1.0f };

static Rr_Vec3 const RR_UI_VEC3_ZERO = { 0.0f, 0.0f, 0.0f };
static Rr_Vec3 const RR_UI_VEC3_ONE = { 1.0f, 1.0f, 1.0f };

#define RR_UI_ROUND(Value) (ceilf((Value) / 2.0f) * 2.0f)
#define RR_UI_ROUND_V2(Value) \
    (Rr_V2(RR_UI_ROUND((Value).X), RR_UI_ROUND((Value).Y)))

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
    Rr_Image2D *Image;
    bool ForceLinearPipeline;
};

typedef RR_ARRAY(Rr_UIClipRect) Rr_UIClipRectArray;

typedef struct Rr_UIWindow Rr_UIWindow;
struct Rr_UIWindow
{
    char const *Title;
    Rr_UIWindowFlags Flags;
    Rr_Rect Rect;
    Rr_Rect ContentsRect;
    float VScroll;
    float VScrollTarget;
    float HScroll;
    int32_t Z;

    bool Collapsed;
    bool Added;
    bool Open;
    bool Child;
    bool Tab;

    bool Undocked;
    bool UndockNextFrame;
    Rr_Vec2 UndockedOffset;

    Rr_UIWindow *SelectedTab;
    Rr_UIWindow *TabsParent;

    bool ShownAtLeastOnce;
    bool CreatedThisFrame;
    bool OpenedThisFrame;
    bool SkipThisFrame;

    float MaxFlexibleWidgetTitleWidth;
    float MaxFlexibleWidgetWidth;
    float MaxRigidWidth;

    Rr_HashTrie *WidgetMap;

    Rr_UIWindow *TopLevelParent;
};

typedef enum
{
    RR_UI_CLICK_TYPE_RELEASE,
    RR_UI_CLICK_TYPE_DOWN,
    RR_UI_CLICK_TYPE_DRAG,
    RR_UI_CLICK_TYPE_DRAG_AND_RELEASE,
    RR_UI_CLICK_TYPE_DRAG_RELAXED, /* Only triggers on move. */
} Rr_UIClickType;

typedef struct Rr_UITree Rr_UITree;
struct Rr_UITree
{
    Rr_Vec2 ParentPoint;
};

typedef struct Rr_UILayout Rr_UILayout;
struct Rr_UILayout
{
    Rr_UIWindow *Window;
    bool WasCollapsed;
    bool SkipItems;
    bool SkipCompletely;
    bool *Open;

    bool VerticalScrollbarAdded;

    Rr_Vec2 WindowPadding;

    Rr_Rect Rect;

    Rr_Vec2 Cursor;
    float TotalAvailableContentsWidth;
    float HorizontalX;
    Rr_Vec2 HorizontalMaxExtent;

    RR_ARRAY(Rr_UITree) TreeStack;
    int32_t TreeDepth;
    int32_t TreeExpandCollapseDepth;

    Rr_Vec2 TabsCursorStart;
    Rr_Vec2 TabsCursor;

    bool MouseInsideClipRect;

    Rr_Rect DeferredContentsRect;
    Rr_Vec4 const *DeferredResizeHandleColor;
    float DeferredMaxFlexibleWidgetTitleWidth;
    float DeferredMaxFlexibleWidgetWidth;
    float DeferredMaxRigidWidth;
    bool DeferredAutoResize;
    bool DeferredClampOffsetToScreen;
    Rr_UIWindow *DeferredSelectedTab;

    Rr_UIWindow *LastAddedTab;

    bool LockOffset;
    bool LockExtentX;
    bool LockExtentY;

    Rr_Vec2 MinExtent;
    Rr_Vec2 MaxExtent;

    Rr_Rect VisibleRect;

    Rr_UIClipRect *ParentClipRect;
    Rr_UIClipRect *CurrentClipRect;
    Rr_UIClipRectArray *ClipRects;

    Rr_UILayout *TopLevelParent;
    Rr_UILayout *Previous;

    uint32_t AdvanceCount;
};

typedef struct Rr_UIMouseButton Rr_UIMouseButton;
struct Rr_UIMouseButton
{
    bool Down;
    bool DownOverWindow;
    bool Held;
    bool Up;
    uint32_t Clicks;
    uint32_t ClickID;
};

typedef enum
{
    RR_UI_RESIZE_TYPE_N,
    RR_UI_RESIZE_TYPE_E,
    RR_UI_RESIZE_TYPE_S,
    RR_UI_RESIZE_TYPE_W,
    RR_UI_RESIZE_TYPE_SE,
} Rr_UIResizeType;

struct Rr_UIContext
{
    Rr_UIColors Colors;
    Rr_UIStyle Style;

    Rr_HashTrie *WindowMap;
    int32_t TotalWindowCount;
    RR_ARRAY(Rr_UILayout *) ActiveLayouts;
    Rr_UIWindow *HoveredWindow;
    Rr_UIWindow *HighestWindow;

    Rr_UIWindow PopupWindow;
    Rr_UIWindow *PopupWindowParent;
    Rr_UIHash PopupWindowHash;
    bool PopupWindowOpen;

    /* TODO: We need nice stack data structure for these. */

    RR_ARRAY(Rr_UILayout *) LayoutStack;
    RR_ARRAY(Rr_UIHash) HashStack;
    RR_ARRAY(Rr_UIFont *) FontStack;
    RR_ARRAY(Rr_Vec2) WindowPaddingStack;
    RR_ARRAY(Rr_Vec2) ContentsMarginStack;
    RR_ARRAY(Rr_Vec2) WidgetExtentStack;
    RR_ARRAY(uint32_t) FormatFloatDecimalPlacesStack;

    Rr_Vec2 NextWindowExtent;
    Rr_Vec2 NextWindowMinExtent;
    Rr_Vec2 NextWindowMaxExtent;
    Rr_Vec2 NextWindowOffset;
    Rr_Vec2 NextWindowOpenOffset;
    Rr_Vec2 NextWindowPadding;
    int8_t NextWindowCreateCollapsed;

    Rr_UIMouseButton LeftMouseButton;
    bool MouseMoved;
    Rr_Vec2 MousePosition;
    Rr_Vec2 MouseWheelDelta;
    Rr_CursorType CursorType;

    Rr_UIWindow *FocusedWidgetParent;
    Rr_UIHash FocusedWidgetHash;
    Rr_UIWindow *PrevFocusedWidgetParent;
    Rr_UIHash PrevFocusedWidgetHash;

    Rr_UIWindow *DragParent;
    Rr_UIHash DragHash;
    bool DragConsumed;
    bool DragMoved;
    Rr_Vec2 DragMouseStart;
    Rr_Rect DragValueStart;
    union
    {
        int32_t Int32;
        uint32_t UnsignedInt32;
        float Float;
        double Double;
    } DragScalarValue;

    Rr_UIWindow *ClickParent;
    Rr_UIHash ClickHash;
    bool ClickConsumed;

    /* NOTE: Cursors are stored as raw offsets into UTF-8 string. */

    size_t TextInputCursorBegin;
    size_t TextInputCursorEnd;
    size_t TextInputCursorCodepointMaxCol;
    uint64_t TextInputCursorBlinkTime;
    uint32_t TextInputClickID;
    RR_ARRAY(char const *) TextInputEvents;
    RR_ARRAY(char) TextInputBuffer;
    bool DeferTextInputBufferCopy;

    RR_ARRAY(Rr_KeyEvent) KeyboardInputEvents;

    Rr_Vec2 ScreenSize;

    RR_FREE_LIST(Rr_UIFont) Fonts;
    Rr_UIFont *DefaultFont;

    float BorderThickness;
    Rr_Vec2 WindowPadding;
    Rr_Vec2 ContentsMargin;
    float ComponentMargin;
    float FlexibleTitleMargin;
    Rr_Vec2 MinWindowSize;
    Rr_Vec2 MinWindowSizeNoTitle;
    float TitleBarHeight;
    Rr_Vec2 TitleBarPadding;
    float ResizeHandleSize;
    float FrameThickness;
    float SeparatorLineHeight;
    float ScrollbarWidth;
    float ScrollbarHandleWidth;
    Rr_Vec2 ButtonPadding;
    float BevelThickness;
    float DoubleBevelThickness;
    Rr_Vec2 InputFieldPadding;
    float TopLevelTreeOffset;
    float TreeOffset;

    RR_ARRAY(Rr_UIVertex) Vertices;
    RR_ARRAY(Rr_UIIndex) Indices;

    Rr_GraphicsPipeline *LinearPipeline;
    Rr_GraphicsPipeline *SRGBPipeline;
    Rr_Buffer *VertexBuffer;
    Rr_Buffer *IndexBuffer;
    Rr_Buffer *UniformBuffer;
    Rr_Sampler *Sampler;

    bool SRGBSwapchain;

    bool VisualizeAdvances;

    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UIContext *gUIContext;

typedef enum
{
    RR_UI_STORAGE_TYPE_INVALID,
    RR_UI_STORAGE_TYPE_TREE,
    RR_UI_STORAGE_TYPE_TABS,
} Rr_UIStorageType;

typedef struct Rr_UIStorage Rr_UIStorage;
struct Rr_UIStorage
{
    union
    {
        bool TreeExpanded;
        Rr_UIHash SelectedTabHash;
    } Union;
    Rr_UIStorageType Type;
};

static inline Rr_UIStorage *Rr_UIGetStorage(
    Rr_UIWindow *Window,
    Rr_UIHash Hash,
    Rr_UIStorageType Type)
{
    Rr_UIStorage **StorageRef =
        RR_FIND_IN_HASH_TRIE(&Window->WidgetMap, Hash, gUIContext->Arena);
    if (*StorageRef == NULL)
    {
        /* TODO: Storages could be allocated from a hive. */
        *StorageRef = Rr_Alloc(sizeof(Rr_UIStorage), gUIContext->Arena);
        Rr_UIStorage *Storage = *StorageRef;
        Storage->Type = Type;

        return *StorageRef;
    }
    Rr_UIStorage *Storage = *StorageRef;
    if (Storage->Type != Type)
    {
        /* NOTE: Maybe report hash collision here? */
        RR_ZERO_PTR(Storage);
        Storage->Type = Type;

        return Storage;
    }

    return Storage;
}

static Rr_UIRange const RR_UI_DEFAULT_RANGES[] = {
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
    Rr_Vec2 Extent;
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
    size_t AllocationSize;
    /* TODO: Handle swapchains recreated with different color space. */
    bool CreatedForSRGBSwapchain;
    Rr_Image2D *Image;
    float Size;
    float LineHeight;
    float Ascent;
    float Descent;
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

#define RR_UI_FONT_OVERSAMPLING 2

static inline Rr_UIFont *Rr_UICreateFontEx(
    size_t TTFSize,
    void const *TTFData,
    float FontSize,
    size_t CodepointRangeCount,
    Rr_UIRange const *CodepointRanges)
{
    assert(FontSize > RR_UI_MIN_FONT_SIZE && FontSize < RR_UI_MAX_FONT_SIZE);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    static int FontIndex = -1;
    FontIndex++;

    int32_t const ATLAS_SIZE = 2048;
    Rr_IntVec2 const ATLAS_EXTENT = { ATLAS_SIZE, ATLAS_SIZE };

    unsigned char *GrayscaleBuffer =
        Rr_AllocNoZero((size_t)(ATLAS_SIZE * ATLAS_SIZE), Scratch.Arena);

    stbtt_fontinfo FontInfo;
    if (!stbtt_InitFont(&FontInfo, (unsigned char const *)TTFData, 0))
    {
        RR_LOG_ERROR("Failed to parse .ttf file!");
        Rr_DestroyScratch(Scratch);

        return NULL;
    }

    int UnscaledAscent, UnscaledDescent, UnscaledLineGap;

    float PixelHeightScale = stbtt_ScaleForPixelHeight(&FontInfo, FontSize);
    stbtt_GetFontVMetrics(
        &FontInfo,
        &UnscaledAscent,
        &UnscaledDescent,
        &UnscaledLineGap);

    float PixelAscent = (float)UnscaledAscent * PixelHeightScale;
    float PixelDescent = (float)UnscaledDescent * PixelHeightScale;
    /* float PixelLineGap = (float)UnscaledLineGap * PixelHeightScale; */

    float BakeScale = FontSize / (PixelAscent + PixelDescent);

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
        RR_LOG_ERROR("Failed to begin .ttf packing!");
        Rr_DestroyScratch(Scratch);

        return NULL;
    }

    stbtt_PackSetOversampling(
        &PackContext,
        RR_UI_FONT_OVERSAMPLING,
        RR_UI_FONT_OVERSAMPLING);

    bool IsSRGBSwapchain =
        Rr_IsSRGBFormat(Rr_GetImageFormat(Rr_GetSwapchainImage()));
    Rr_ImageFormat ImageFormat = RR_IMAGE_FORMAT_R8G8B8A8_SRGB;

    /* Pack everything into single allocation. */

    size_t GlyphsOffset =
        sizeof(Rr_UIFont) + sizeof(Rr_UIFontRange) * CodepointRangeCount;
    size_t AllocationSize = GlyphsOffset;
    for (size_t Index = 0; Index < CodepointRangeCount; ++Index)
    {
        Rr_UIRange const *CodepointRange = &CodepointRanges[Index];
        size_t GlyphCount =
            (size_t)(CodepointRange->Last - CodepointRange->First);
        AllocationSize += GlyphCount * sizeof(Rr_UIGlyph);
    }

    Rr_UIFont *Font = Rr_AllocNoZero(AllocationSize, gUIContext->Arena);
    Font->AllocationSize = AllocationSize;
    Font->CreatedForSRGBSwapchain = IsSRGBSwapchain;
    Font->RangeCount = CodepointRangeCount;
    Font->Ranges = (Rr_UIFontRange *)(Font + 1);
    Rr_SetNextObjectNameF("Rr.UI.Font#%d", FontIndex);
    Font->Image = Rr_CreateImage2D(
        ATLAS_EXTENT,
        ImageFormat,
        RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
    Font->Size = FontSize;
    Font->LineHeight = (PixelAscent - PixelDescent) * BakeScale;
    Font->Ascent = PixelAscent * BakeScale;
    Font->Descent = PixelDescent * BakeScale;

    stbtt_pack_range *PackRanges = Rr_AllocNoZero(
        CodepointRangeCount * sizeof(stbtt_pack_range),
        Scratch.Arena);

    size_t TotalCharCount = 0;
    for (size_t Index = 0; Index < CodepointRangeCount; ++Index)
    {
        Rr_UIRange const *CodepointRange = &CodepointRanges[Index];
        Rr_UIFontRange *FontRange = &Font->Ranges[Index];

        int32_t NumChars = CodepointRange->Last - CodepointRange->First;

        FontRange->First = (uint32_t)CodepointRange->First;
        FontRange->Last = (uint32_t)CodepointRange->Last;
        FontRange->Glyphs = (void *)((char *)Font + GlyphsOffset);

        PackRanges[Index] = (stbtt_pack_range){
            .first_unicode_codepoint_in_range = CodepointRange->First,
            .num_chars = NumChars,
            .font_size = FontSize * BakeScale,
            .chardata_for_range = Rr_AllocNoZero(
                (size_t)NumChars * sizeof(stbtt_packedchar),
                Scratch.Arena),
        };

        TotalCharCount += (size_t)NumChars;
        GlyphsOffset += sizeof(Rr_UIGlyph) * (size_t)NumChars;
    }

    stbrp_rect *Rects =
        Rr_AllocNoZero(sizeof(stbrp_rect) * TotalCharCount, Scratch.Arena);

    int NumRects = stbtt_PackFontRangesGatherRects(
        &PackContext,
        &FontInfo,
        PackRanges,
        (int32_t)CodepointRangeCount,
        Rects);
    assert(NumRects);
    stbtt_PackFontRangesPackRects(&PackContext, Rects, NumRects);
    int Result = stbtt_PackFontRangesRenderIntoRects(
        &PackContext,
        &FontInfo,
        PackRanges,
        (int32_t)CodepointRangeCount,
        Rects);
    assert(Result);
    stbtt_PackEnd(&PackContext);

    /* Create glyph data. */

    for (size_t Index = 0; Index < CodepointRangeCount; ++Index)
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

            Glyph->Extent = (Rr_Vec2){ Quad.x1 - Quad.x0, Quad.y1 - Quad.y0 };
            Glyph->Offset =
                (Rr_Vec2){ Quad.x0, Quad.y0 + PixelAscent * BakeScale };
            Glyph->UVMin = (Rr_Vec2){ Quad.s0, Quad.t0 };
            Glyph->UVMax = (Rr_Vec2){ Quad.s1, Quad.t1 };
            Glyph->XAdvance = PackedChar->xadvance;
        }
    }

    /* Fill RGBA atlas. */

    size_t AtlasBufferSize =
        (size_t)(ATLAS_SIZE * ATLAS_SIZE) * sizeof(uint32_t);
    Rr_SetNextObjectName("Rr.UI.StagingBuffer");
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

Rr_UIFont *Rr_UICreateFont(size_t TTFSize, void const *TTFData, float FontSize)
{
    return Rr_UICreateFontEx(
        TTFSize,
        TTFData,
        FontSize,
        RR_ARRAY_COUNT(RR_UI_DEFAULT_RANGES),
        RR_UI_DEFAULT_RANGES);
}

Rr_UIFont *Rr_UICreateFontRanges(
    size_t TTFSize,
    void const *TTFData,
    float FontSize,
    size_t CodepointRangeCount,
    Rr_UIRange const *CodepointRanges)
{
    return Rr_UICreateFontEx(
        TTFSize,
        TTFData,
        FontSize,
        CodepointRangeCount,
        CodepointRanges);
}

void Rr_UIReleaseFont(Rr_UIFont *Font)
{
    if (Font)
    {
        Rr_ReleaseImage(Font->Image);
    }
}

static inline Rr_UIFont *Rr_UICurrentFont(void)
{
    if (gUIContext->FontStack.Count)
    {
        return RR_LAST_ARRAY_ELEMENT(&gUIContext->FontStack);
    }

    return gUIContext->DefaultFont;
}

static inline void Rr_UIConsumeMouseInput(void)
{
    gUIContext->LeftMouseButton.Down = false;
    gUIContext->LeftMouseButton.DownOverWindow = false;
    gUIContext->LeftMouseButton.Up = false;
    gUIContext->LeftMouseButton.Held = false;
}

static inline void Rr_UIConsumeMouseAndKeyboardInput(void)
{
    Rr_UIConsumeMouseInput();
    RR_CLEAR_ARRAY(&gUIContext->KeyboardInputEvents);
}

static inline bool Rr_UIWindowNoTitleBar(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_NO_TITLE_BAR_BIT;
}

static inline bool Rr_UIWindowNoClose(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_NO_CLOSE_BIT;
}

static inline bool Rr_UIWindowNoBackground(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_NO_BACKGROUND_BIT;
}

static inline bool Rr_UIWindowNoCollapse(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_NO_COLLAPSE_BIT;
}

static inline bool Rr_UIWindowNoResize(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT;
}

static inline bool Rr_UIWindowAutoResize(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;
}

static inline bool Rr_UIWindowNoMove(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_NO_MOVE_BIT;
}

static inline bool Rr_UIWindowNoBorders(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_NO_BORDERS_BIT;
}

static inline bool Rr_UIWindowNoVerticalScrollbar(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT;
}

static inline bool Rr_UIWindowEscapeCloses(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_ESCAPE_CLOSES_BIT;
}

static inline bool Rr_UIWindowUndockable(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT;
}

static inline bool Rr_UIWindowTabs(Rr_UIWindow *Window)
{
    return Window->Flags & RR_UI_WINDOW_FLAGS_TABS_BIT;
}

static inline Rr_UIHash Rr_UICurrentHash(void)
{
    return gUIContext->HashStack.Count > 0
               ? RR_LAST_ARRAY_ELEMENT(&gUIContext->HashStack)
               : 0;
}

static inline Rr_UIHash Rr_UIGetHash(
    size_t Size,
    void const *Data,
    Rr_UIHash Seed)
{
    return Rr_Hash64WithSeed(Size, Data, ~Seed);
}

static inline Rr_UIHash Rr_UIGetTitleHash(
    char const *CString,
    size_t *OutLength)
{
    Rr_UIHash Hash;
    size_t FullLength = strlen(CString);
    char const *ExplicitID = strstr(CString, "###");
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
    *RR_PUSH_INTO_ARRAY(&gUIContext->HashStack, gUIContext->FrameArena) = Hash;
}

void Rr_UIPushID(char const *IDString)
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

static inline Rr_Vec2 Rr_UICurrentWindowPadding(void)
{
    return gUIContext->WindowPaddingStack.Count
               ? RR_LAST_ARRAY_ELEMENT(&gUIContext->WindowPaddingStack)
               : gUIContext->WindowPadding;
}

void Rr_UIPushWindowPadding(Rr_Vec2 WindowPadding)
{
    *RR_PUSH_INTO_ARRAY(
        &gUIContext->WindowPaddingStack,
        gUIContext->FrameArena) = WindowPadding;
}

void Rr_UIPopWindowPadding(void)
{
    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->WindowPaddingStack));
}

static inline Rr_Vec2 Rr_UICurrentContentsMargin(void)
{
    return gUIContext->ContentsMarginStack.Count
               ? RR_LAST_ARRAY_ELEMENT(&gUIContext->ContentsMarginStack)
               : gUIContext->ContentsMargin;
}

void Rr_UIPushContentsMargin(Rr_Vec2 ContentsMargin)
{
    *RR_PUSH_INTO_ARRAY(
        &gUIContext->ContentsMarginStack,
        gUIContext->FrameArena) = ContentsMargin;
}

void Rr_UIPopContentsMargin(void)
{
    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->ContentsMarginStack));
}

static inline Rr_Rect Rr_UIRectIntersection(
    Rr_Rect const *RectA,
    Rr_Rect const *RectB)
{
    Rr_Rect Result;
    Result.Offset = Rr_MaxV2(RectA->Offset, RectB->Offset);
    Rr_Vec2 BottomRightA = Rr_AddV2(RectA->Offset, RectA->Extent);
    Rr_Vec2 BottomRightB = Rr_AddV2(RectB->Offset, RectB->Extent);
    Rr_Vec2 Delta = Rr_MinV2(BottomRightA, BottomRightB);
    Result.Extent = Rr_SubV2(Delta, Result.Offset);

    return Result;
}

static inline Rr_UILayout *Rr_UICurrentLayout(void)
{
    if (gUIContext->LayoutStack.Count == 0)
    {
        return NULL;
    }

    return RR_LAST_ARRAY_ELEMENT(&gUIContext->LayoutStack);
}

static inline void Rr_UIPushEmptyLayout(Rr_UIHash Hash, Rr_UIWindow *Window)
{
    /* I guess we can skip zeroing since skip flags should make it clear it's an
     * empty layout. */

    Rr_UILayout *Layout =
        Rr_AllocNoZero(sizeof(Rr_UILayout), gUIContext->FrameArena);
    Layout->Window = Window;
    Layout->SkipCompletely = true;
    Layout->SkipItems = true;

    Rr_UIPushIDHash(Hash);

    *RR_PUSH_INTO_ARRAY(&gUIContext->LayoutStack, gUIContext->FrameArena) =
        Layout;
}

static inline Rr_UILayout *Rr_UIPushLayout(Rr_UIHash Hash)
{
    Rr_UILayout *Layout = Rr_Alloc(sizeof(Rr_UILayout), gUIContext->FrameArena);

    Rr_UIPushIDHash(Hash);

    *RR_PUSH_INTO_ARRAY(&gUIContext->LayoutStack, gUIContext->FrameArena) =
        Layout;
    *RR_PUSH_INTO_ARRAY(&gUIContext->ActiveLayouts, gUIContext->Arena) = Layout;

    return Layout;
}

static inline void Rr_UIPopLayout(void)
{
    Rr_UIPopID();
    assert(gUIContext->LayoutStack.Count);
    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->LayoutStack));
}

static inline bool Rr_UISkipItems(void)
{
    return Rr_UICurrentLayout()->SkipItems;
}

static inline Rr_UIWindow *Rr_UICurrentWindow(void)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();

    return Layout ? Layout->Window : NULL;
}

static inline float Rr_UIGetAvailableContentsWidth(Rr_UILayout *Layout)
{
    return Layout->TotalAvailableContentsWidth -
           (Layout->Cursor.X - Layout->DeferredContentsRect.Offset.X);
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

    static Rr_UIIndex const QUAD_INDICES[] = { 0, 1, 2, 3, 0, 2 };

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

static inline void Rr_UIFeatherConvexPrimitive(
    Rr_UIPrimitive *SourcePrimitive,
    int32_t VertexCount,
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

        Rr_Vec2 Normal0 = Rr_SubV2(Previous.Position, Current.Position);
        Rr_Vec2 Normal1 = Rr_SubV2(Next.Position, Current.Position);
        float Normal0Length = Rr_LenV2(Normal0);
        float Normal1Length = Rr_LenV2(Normal1);
        Rr_Vec2 Offset = Rr_MulV2F(
            Rr_DivV2F(
                Rr_AddV2(
                    Rr_MulV2F(Normal0, Normal1Length),
                    Rr_MulV2F(Normal1, Normal0Length)),
                (Normal0.X * Normal1.Y - Normal0.Y * Normal1.X)),
            Amount);
        Rr_Vec2 Position = Rr_AddV2(Current.Position, Offset);

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

void Rr_UIDrawTriangleVertices(Rr_UIVertex const *Vertices)
{
    Rr_UIPrimitive Primitive = Rr_UIReservePrimitive(3, 3);

    memcpy(Primitive.Vertices, Vertices, sizeof(Rr_UIVertex) * 3);

    Primitive.Indices[0] = Primitive.BaseVertex;
    Primitive.Indices[1] = Primitive.BaseVertex + 1;
    Primitive.Indices[2] = Primitive.BaseVertex + 2;

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 1.5f);
}

void Rr_UIDrawTriangleFilled(Rr_Vec2 const *Positions, Rr_Vec4 const *Color)
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

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 1.5f);
}

void Rr_UIDrawFitTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 const *Color)
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

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 1.5f);
}

void Rr_UIDrawEquilateralTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 const *Color)
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

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, 1.5f);
}

void Rr_UIDrawCircle(
    Rr_Vec2 Offset,
    float Radius,
    float Thickness,
    Rr_Vec4 const *Color)
{
    static int const SEGMENTS = 20;

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

    Rr_UIFeatherConvexPrimitive(&Primitive, SEGMENTS, 1.5f);
    /* NOTE: A hack to feather inner part of the circle. */
    Primitive.BaseVertex += (Rr_UIIndex)SEGMENTS;
    Primitive.Vertices += SEGMENTS;
    Rr_UIFeatherConvexPrimitive(&Primitive, SEGMENTS, -1.5f);
}

void Rr_UIDrawCircleFilled(Rr_Vec2 Offset, float Radius, Rr_Vec4 const *Color)
{
    static size_t const SEGMENTS = 20;

    Rr_UIPrimitive Primitive =
        Rr_UIReservePrimitive((size_t)SEGMENTS, (size_t)(SEGMENTS - 2) * 3);

    float const STEP = 2.0f * RR_PI32 / (float)SEGMENTS;

    for (size_t Index = 0; Index < SEGMENTS; ++Index)
    {
        Primitive.Vertices[Index].Position = Rr_AddV2(
            Offset,
            Rr_MulV2F(
                Rr_V2(cosf((float)Index * STEP), sinf((float)Index * STEP)),
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

    Rr_UIFeatherConvexPrimitive(&Primitive, (int32_t)SEGMENTS, 1.5f);
}

void Rr_UIDrawQuadVertices(Rr_UIVertex const *Vertices)
{
    Rr_UIPrimitive Primitive = Rr_UIReserveQuad();
    memcpy(Primitive.Vertices, Vertices, sizeof(Rr_UIVertex) * 4);
}

static inline void Rr_UISolidQuad(
    Rr_UIVertex *Vertices,
    Rr_Rect *Rect,
    Rr_Vec4 const *Color)
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

static inline void Rr_UIDrawSolidQuad(Rr_Rect *Rect, Rr_Vec4 const *Color)
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
    Rr_UIFeatherConvexPrimitive(&Primitive, 4, 1.5f);
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
    Rr_Vec4 const *Color,
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
    static Rr_Vec4 const WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };
    static Rr_Vec4 const GRAY = { 0.15f, 0.15f, 0.15f, 1.0f };

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

static inline void Rr_UIDrawCheckmark(
    Rr_Vec2 Offset,
    float Size,
    Rr_Vec4 *Color)
{
    Rr_Vec2 const NE = { -cosf(RR_PI32 * 0.25f), sinf(RR_PI32 * 0.25f) };
    Rr_Vec2 const NW = { cosf(RR_PI32 * 0.25f), sinf(RR_PI32 * 0.25f) };

    float ShortX = Size * gUIContext->Style.CheckmarkRatios.X;
    float LongX = Size - ShortX;
    float Thickness = Size * gUIContext->Style.CheckmarkRatios.Y;
    float HypoY = sqrtf(Thickness * Thickness + Thickness * Thickness);
    float ShortY = Thickness / sqrtf(2.0f);

    Offset.Y -= (Size - LongX - ShortY) * 0.5f;

    Rr_Vec2 MiddleTop = Rr_AddV2(Offset, Rr_V2(ShortX, Size - HypoY));
    Rr_Vec2 MiddleBottom = Rr_AddV2(Offset, Rr_V2(ShortX, Size));

    {
        Rr_UIPrimitive Primitive = Rr_UIReservePrimitive(4, 6);
        Rr_UIVertex *Vertices = Primitive.Vertices;
        Rr_UIIndex *Indices = Primitive.Indices;

        Vertices[1].Position = MiddleTop;
        Vertices[2].Position = Rr_AddV2(MiddleTop, Rr_MulV2F(NE, Thickness));
        Vertices[3].Position = Rr_AddV2(Offset, Rr_V2(0.0f, Size - ShortX));
        Vertices[0].Position =
            Rr_AddV2(Vertices[3].Position, Rr_MulV2F(NE, -Thickness));

        for (size_t Index = 0; Index < 7; ++Index)
        {
            Vertices[Index].Color = *Color;
            Vertices[Index].UV = Rr_V2F(0.0f);
        }

        Indices[0] = Primitive.BaseVertex + 0;
        Indices[1] = Primitive.BaseVertex + 1;
        Indices[2] = Primitive.BaseVertex + 2;
        Indices[3] = Primitive.BaseVertex + 3;
        Indices[4] = Primitive.BaseVertex + 0;
        Indices[5] = Primitive.BaseVertex + 2;

        Rr_UIFeatherConvexPrimitive(&Primitive, 4, 1.5f);
    }

    {
        Rr_UIPrimitive Primitive = Rr_UIReservePrimitive(4, 6);
        Rr_UIVertex *Vertices = Primitive.Vertices;
        Rr_UIIndex *Indices = Primitive.Indices;

        Vertices[0].Position = Rr_AddV2(MiddleTop, Rr_MulV2F(NE, Thickness));
        Vertices[2].Position = Rr_AddV2(Offset, Rr_V2(Size, Size - LongX));
        Vertices[1].Position =
            Rr_AddV2(Vertices[2].Position, Rr_MulV2F(NW, -Thickness));
        Vertices[3].Position = MiddleBottom;

        for (size_t Index = 0; Index < 7; ++Index)
        {
            Vertices[Index].Color = *Color;
            Vertices[Index].UV = Rr_V2F(0.0f);
        }

        Indices[0] = Primitive.BaseVertex + 0;
        Indices[1] = Primitive.BaseVertex + 1;
        Indices[2] = Primitive.BaseVertex + 2;
        Indices[3] = Primitive.BaseVertex + 3;
        Indices[4] = Primitive.BaseVertex + 0;
        Indices[5] = Primitive.BaseVertex + 2;

        Rr_UIFeatherConvexPrimitive(&Primitive, 4, 1.5f);
    }
}

#define RR_UI_BEVEL_FRAME_VERTEX_COUNT (12)
#define RR_UI_BEVEL_FRAME_INDEX_COUNT  (24)

static inline Rr_UIPrimitive Rr_UIReserveBevelFrame(void)
{
    return Rr_UIReservePrimitive(
        RR_UI_BEVEL_FRAME_VERTEX_COUNT,
        RR_UI_BEVEL_FRAME_INDEX_COUNT);
}

static inline void Rr_UIBevelFrame(
    Rr_Rect *Rect,
    Rr_UIPrimitive *Primitive,
    Rr_Vec4 *ColorLight,
    Rr_Vec4 *ColorDark,
    float Thickness)
{
    Rr_UIVertex *Vertices = Primitive->Vertices;
    Rr_UIIndex *Indices = Primitive->Indices;

    float Width = Rect->Extent.Width;
    float Height = Rect->Extent.Height;
    float TempWidth = Rect->Extent.Width - Thickness;
    float TempHeight = Rect->Extent.Height - Thickness;

    Vertices[0] = (Rr_UIVertex){
        .Position = Rect->Offset,
        .Color = *ColorLight,
    };
    Vertices[1] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(Width, 0.0f)),
        .Color = *ColorLight,
    };
    Vertices[2] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2F(Thickness)),
        .Color = *ColorLight,
    };
    Vertices[3] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(TempWidth, Thickness)),
        .Color = *ColorLight,
    };
    Vertices[4] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(0.0f, Height)),
        .Color = *ColorLight,
    };
    Vertices[5] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(Thickness, TempHeight)),
        .Color = *ColorLight,
    };

    Vertices[6] = (Rr_UIVertex){
        .Position = Vertices[3].Position,
        .Color = *ColorDark,
    };
    Vertices[7] = (Rr_UIVertex){
        .Position = Vertices[1].Position,
        .Color = *ColorDark,
    };
    Vertices[8] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rr_V2(TempWidth, TempHeight)),
        .Color = *ColorDark,
    };
    Vertices[9] = (Rr_UIVertex){
        .Position = Rr_AddV2(Rect->Offset, Rect->Extent),
        .Color = *ColorDark,
    };
    Vertices[10] = (Rr_UIVertex){
        .Position = Vertices[4].Position,
        .Color = *ColorDark,
    };
    Vertices[11] = (Rr_UIVertex){
        .Position = Vertices[5].Position,
        .Color = *ColorDark,
    };

    static Rr_UIIndex const BEVEL_FRAME_INDICES[] = {
        0, 1, 2, 1, 3, 2, 0,  2,  4, 2,  5, 4,
        6, 7, 8, 7, 9, 8, 10, 11, 9, 11, 8, 9,
    };

    for (size_t Index = 0; Index < RR_UI_BEVEL_FRAME_INDEX_COUNT; ++Index)
    {
        Indices[Index] = Primitive->BaseVertex + BEVEL_FRAME_INDICES[Index];
    }
}

static inline void Rr_UIDrawDoubleBevel(
    Rr_Rect *Rect,
    Rr_Vec4 *Color,
    float Thickness)
{
    Rr_Vec4 ColorLight;
    ColorLight.RGB = Rr_LerpV3(
        Color->RGB,
        gUIContext->Style.BevelIntensityLight,
        RR_UI_VEC3_ONE);
    ColorLight.A = 1.0f;
    Rr_Vec4 ColorDark;
    ColorDark.RGB = Rr_LerpV3(
        Color->RGB,
        gUIContext->Style.BevelIntensityDark,
        RR_UI_VEC3_ZERO);
    ColorDark.A = 1.0f;

    Rr_UIPrimitive BevelFramePrimitive = Rr_UIReserveBevelFrame();
    Rr_UIBevelFrame(
        Rect,
        &BevelFramePrimitive,
        &ColorLight,
        &ColorDark,
        Thickness * 0.5f);

    BevelFramePrimitive = Rr_UIReserveBevelFrame();
    Rr_Rect InnerRect = Rr_ResizeRect(Rect, -Thickness * 0.5f);
    Rr_UIBevelFrame(
        &InnerRect,
        &BevelFramePrimitive,
        &ColorDark,
        &ColorLight,
        Thickness * 0.5f);
}

static inline void Rr_UIDrawDoubleBevelFilled(
    Rr_Rect *Rect,
    Rr_Vec4 *Color,
    float Thickness)
{
    Rr_UIDrawDoubleBevel(Rect, Color, Thickness);
    Rr_Rect InnerRect = Rr_ResizeRect(Rect, -Thickness);
    Rr_UIDrawRect(&InnerRect, Color);
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
    Rr_Rect const *Rect,
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

    static Rr_UIIndex const BEVEL_INDICES[] = {
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
    Rr_Rect const *Rect,
    Rr_Vec4 const *BaseColor,
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
    Rr_UIBevel(Rr_UIReserveBevel(), Rect, BaseColor, Pressed);
}

static inline void Rr_UIDrawGlyph(
    Rr_UIGlyph *Glyph,
    Rr_Vec2 Position,
    Rr_Vec4 const *Color)
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
            Glyph->Extent,
        },
        Color,
        UVs);
}

static inline void Rr_UIDrawInteractiveTextCursor(
    Rr_Vec2 Position,
    Rr_Vec4 *Color,
    Rr_UIFont *Font)
{
    uint64_t TimeDelta = Rr_GetTimeMS() - gUIContext->TextInputCursorBlinkTime;
    if ((TimeDelta / 500) % 2 == 0)
    {
        Rr_UIDrawRect(
            &(Rr_Rect){
                Position,
                Rr_V2(gUIContext->FrameThickness * 2.0f, Font->LineHeight),
            },
            Color);
    }
}

static inline Rr_Vec2 Rr_UIDrawInputText(
    char const *CString,
    bool Active,
    Rr_Vec2 Position,
    size_t CursorBegin,
    size_t *CursorEnd,
    float AvailableWidth,
    Rr_Vec4 *Color)
{
    Rr_BeginFrameSection("Rr.UI.DrawInputText");

    Rr_UIFont *Font = Rr_UICurrentFont();
    float FontSize = Font->Size;
    float LineHeight = Font->LineHeight;
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

    Rr_UTF8Decoder Decoder = { .CString = CString };
    while (true)
    {
        Rr_UTF8Decode(&Decoder);
        uint32_t Codepoint = Decoder.Codepoint;
        size_t CStringIndex = Decoder.CStringCodepointIndex;

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

        bool GlyphSelected = false;
        if (Active)
        {
            if ((OldCursorMin != OldCursorMax) &&
                (CStringIndex >= OldCursorMin && CStringIndex < OldCursorMax))
            {
                GlyphSelected = true;
                Rr_UIDrawRect(
                    &(Rr_Rect){
                        GlyphPosition,
                        Rr_V2(Glyph->XAdvance, Font->LineHeight),
                    },
                    &gUIContext->Colors.SelectedTextBackground);
            }
            if (*CursorEnd == CStringIndex)
            {
                Rr_UIDrawInteractiveTextCursor(GlyphPosition, Color, Font);
            }
        }

        if (Codepoint == '\0')
        {
            break;
        }

        Rr_Vec4 *GlyphColor =
            GlyphSelected ? &gUIContext->Colors.SelectedTextForeground : Color;
        if (!Glyph)
        {
            /* TODO: Proper missing glyph handling! */

            CurrentX += FontSize;
            MaxX = RR_MAX(MaxX, CurrentX);

            Rr_Rect MissingGlyphRect = { GlyphPosition,
                                         Rr_V2(FontSize, LineHeight) };
            MissingGlyphRect = Rr_ResizeRect(&MissingGlyphRect, 1.0f);
            Rr_UIDrawInnerFrame(&MissingGlyphRect, 1.0f, GlyphColor);
        }
        else
        {
            if (Codepoint != ' ')
            {
                Rr_UIDrawGlyph(Glyph, GlyphPosition, GlyphColor);
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

    Rr_EndFrameSection("Rr.UI.DrawInputText");

    return (Rr_Vec2){ .Width = MaxX, .Height = CurrentY + LineHeight };
}

static inline Rr_Vec2 Rr_UIDrawText(
    bool CalculateOnly,
    Rr_Vec2 Position,
    size_t UTF8StringLength,
    char const *UTF8String,
    float WrapWidth,
    Rr_Vec4 const *Color)
{
    if (UTF8StringLength == 0)
    {
        return Rr_V2F(0.0f);
    }

    Rr_BeginFrameSection("Rr.UI.DrawText");

    bool NullTerminated = false;
    if (UTF8StringLength == SIZE_MAX)
    {
        NullTerminated = true;
    }

    Rr_UIFont *Font = Rr_UICurrentFont();
    float LineHeight = Font->LineHeight;
    float MaxX = 0.0f;
    float CurrentX = 0.0f;
    float CurrentY = 0.0f;

    if (WrapWidth > 0.0f)
    {
        Rr_Scratch Scratch = Rr_GetScratch(NULL);

        WrapWidth = RR_MAX(WrapWidth, LineHeight);

        float CurrentWordWidth = 0.0f;

        Rr_UTF8Decoder Decoder = { .CString = UTF8String };
        RR_ARRAY(uint32_t) Word = { 0 };
        RR_RESERVE_ARRAY(&Word, 32, Scratch.Arena);
        while (true)
        {
            Rr_UTF8Decode(&Decoder);

            uint32_t Codepoint = Decoder.Codepoint;
            bool End = (NullTerminated && Codepoint == '\0') ||
                       Decoder.CStringParserIndex >= UTF8StringLength;

            if (!End)
            {
                *RR_PUSH_INTO_ARRAY(&Word, Scratch.Arena) = Codepoint;
            }

            if (End || Codepoint == ' ')
            {
                size_t WordLength = Word.Count;
                if (WordLength > 0)
                {
                    if (CurrentWordWidth > WrapWidth)
                    {
                        /* Fallback to per-character wrapping. */

                        for (size_t IndexInWord = 0; IndexInWord < Word.Count;
                             ++IndexInWord)
                        {
                            Codepoint = Word.Data[IndexInWord];
                            if (CurrentX > WrapWidth)
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
                        if (CurrentX + CurrentWordWidth > WrapWidth)
                        {
                            CurrentX = 0.0f;
                            CurrentY += LineHeight;
                        }

                        Rr_Vec2 PositionInWord =
                            Rr_AddV2(Position, (Rr_Vec2){ CurrentX, CurrentY });
                        for (size_t IndexInWord = 0; IndexInWord < Word.Count;
                             ++IndexInWord)
                        {
                            Codepoint = Word.Data[IndexInWord];
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
                RR_CLEAR_ARRAY(&Word);

                if (End)
                {
                    break;
                }
            }
            else if (Codepoint == '\n')
            {
                CurrentX = 0.0f;
                CurrentY += LineHeight;
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
        while (
            Rr_UTF8Decode(&Decoder) != '\0' &&
            (NullTerminated || Decoder.CStringParserIndex <= UTF8StringLength))
        {
            uint32_t Codepoint = Decoder.Codepoint;

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

    Rr_EndFrameSection("Rr.UI.DrawText");

    return (Rr_Vec2){ .Width = MaxX, .Height = CurrentY + LineHeight };
}

static inline Rr_Vec2 Rr_UICalculateTextSize(
    size_t UTF8StringLength,
    char const *UTF8String,
    float WrapWidth)
{
    return Rr_UIDrawText(
        true,
        Rr_V2F(0.0f),
        UTF8StringLength,
        UTF8String,
        WrapWidth,
        NULL);
}

static inline bool Rr_UIIsFocused(Rr_UIWindow *Window, Rr_UIHash Hash)
{
    return gUIContext->FocusedWidgetParent == Window &&
           gUIContext->FocusedWidgetHash == Hash;
}

static inline bool Rr_UIWasFocused(Rr_UIWindow *Window, Rr_UIHash Hash)
{
    bool Result = gUIContext->PrevFocusedWidgetParent == Window &&
                  gUIContext->PrevFocusedWidgetHash == Hash;
    if (Result)
    {
        gUIContext->PrevFocusedWidgetParent = NULL;
    }

    return Result;
}

static inline void Rr_UISetFocus(Rr_UIWindow *Window, Rr_UIHash Hash)
{
    if (Rr_UIIsFocused(Window, Hash))
    {
        return;
    }
    if (gUIContext->FocusedWidgetParent != NULL)
    {
        gUIContext->PrevFocusedWidgetParent = gUIContext->FocusedWidgetParent;
        gUIContext->PrevFocusedWidgetHash = gUIContext->FocusedWidgetHash;
    }
    gUIContext->FocusedWidgetParent = Window;
    gUIContext->FocusedWidgetHash = Hash;
}

static inline void Rr_UIResetClickAndDrag(void)
{
    gUIContext->ClickParent = NULL;
    gUIContext->ClickHash = 0;
    gUIContext->DragParent = NULL;
    gUIContext->DragHash = 0;
}

static inline void Rr_UIResetDrag(void)
{
    gUIContext->DragParent = NULL;
    gUIContext->DragHash = 0;
}

typedef struct Rr_UIClickResult Rr_UIClickResult;
struct Rr_UIClickResult
{
    bool Moved : 1;
    bool MovedFirstTime : 1;
    bool Held : 1;
    bool Hovered : 1;
    uint8_t ClickCount;
};

static inline Rr_UIClickResult Rr_UIClickEx(
    Rr_UILayout *Layout,
    Rr_Rect const *Rect,
    Rr_UIClickType Type,
    Rr_UIHash Hash,
    Rr_Rect Value)
{
    Rr_UIWindow *Window = Layout->Window;

    bool Up = gUIContext->LeftMouseButton.Up;
    bool Down = gUIContext->LeftMouseButton.Down;
    bool Held = gUIContext->LeftMouseButton.Held;

    bool Contains = Layout->MouseInsideClipRect &&
                    Window == gUIContext->HoveredWindow &&
                    Rr_RectContains(Rect, gUIContext->MousePosition);

    Rr_UIClickResult ClickResult = { 0 };
    ClickResult.Hovered = Contains;

    uint8_t ClickCount =
        (uint8_t)RR_CLAMP(1, gUIContext->LeftMouseButton.Clicks, UCHAR_MAX);

    bool DragMatch =
        Window == gUIContext->DragParent && Hash == gUIContext->DragHash;
    bool ClickMatch =
        Window == gUIContext->ClickParent && Hash == gUIContext->ClickHash;

    if (Type == RR_UI_CLICK_TYPE_DRAG_RELAXED)
    {
        if (Down && Contains && !gUIContext->DragConsumed)
        {
            gUIContext->DragParent = Window;
            gUIContext->DragHash = Hash;
            gUIContext->DragMouseStart = gUIContext->MousePosition;
            gUIContext->DragValueStart = Value;
            gUIContext->DragMoved = false;

            if (!gUIContext->ClickConsumed)
            {
                gUIContext->ClickParent = Window;
                gUIContext->ClickHash = Hash;
            }

            Rr_UISetFocus(NULL, 0);
        }
        else if (Up && (DragMatch || ClickMatch))
        {
            gUIContext->DragParent = NULL;
        }
        else if (Held && DragMatch)
        {
            if (gUIContext->MouseMoved)
            {
                gUIContext->ClickParent = Window;
                gUIContext->ClickHash = Hash;
            }

            gUIContext->ClickConsumed = true;

            ClickResult.Held = true;
            ClickResult.Moved = gUIContext->MouseMoved;
        }
    }

    if (gUIContext->ClickConsumed)
    {
        return ClickResult;
    }

    if (Type == RR_UI_CLICK_TYPE_DOWN)
    {
        if (Down && Contains && !gUIContext->MouseMoved)
        {
            if (ClickCount == 1 || (ClickCount > 1 && ClickMatch))
            {
                gUIContext->ClickParent = Window;
                gUIContext->ClickHash = Hash;

                ClickResult.ClickCount = ClickCount;

                gUIContext->ClickConsumed = true;

                Rr_UISetFocus(NULL, 0);
            }
        }
    }

    if (Type == RR_UI_CLICK_TYPE_RELEASE)
    {
        if (Down && Contains)
        {
            gUIContext->DragParent = Window;
            gUIContext->DragHash = Hash;
            gUIContext->DragConsumed = true;

            gUIContext->ClickParent = Window;
            gUIContext->ClickHash = Hash;
            gUIContext->ClickConsumed = true;

            Rr_UISetFocus(NULL, 0);
        }
        else if (Up && DragMatch)
        {
            gUIContext->DragParent = NULL;
            gUIContext->DragConsumed = true;

            gUIContext->ClickConsumed = true;

            if (Contains)
            {
                ClickResult.ClickCount = 1;
            }
        }
        else if (Held && DragMatch)
        {
            gUIContext->ClickConsumed = true;

            gUIContext->DragConsumed = true;

            ClickResult.Held = Contains;
        }
    }

    if (Type == RR_UI_CLICK_TYPE_DRAG)
    {
        if (Down && Contains)
        {
            gUIContext->DragParent = Window;
            gUIContext->DragHash = Hash;
            gUIContext->DragMouseStart = gUIContext->MousePosition;
            gUIContext->DragValueStart = Value;
            gUIContext->DragConsumed = true;
            gUIContext->DragMoved = false;

            gUIContext->ClickParent = Window;
            gUIContext->ClickHash = Hash;
            gUIContext->ClickConsumed = true;

            ClickResult.ClickCount = ClickCount;

            Rr_UISetFocus(Window, Hash);
        }
        else if (Up && DragMatch)
        {
            gUIContext->DragParent = NULL;
            gUIContext->DragConsumed = true;

            gUIContext->ClickConsumed = true;
        }
        else if (Held && DragMatch)
        {
            ClickResult.MovedFirstTime =
                gUIContext->MouseMoved && !gUIContext->DragMoved;

            gUIContext->DragConsumed = true;
            gUIContext->DragMoved |= gUIContext->MouseMoved;

            gUIContext->ClickConsumed = true;

            ClickResult.Held = true;
            ClickResult.Moved = gUIContext->MouseMoved;
        }
    }

    if (Type == RR_UI_CLICK_TYPE_DRAG_AND_RELEASE)
    {
        if (Down && Contains)
        {
            gUIContext->DragParent = Window;
            gUIContext->DragHash = Hash;
            gUIContext->DragMouseStart = gUIContext->MousePosition;
            gUIContext->DragValueStart = Value;
            gUIContext->DragConsumed = true;
            gUIContext->DragMoved = false;

            gUIContext->ClickParent = Window;
            gUIContext->ClickHash = Hash;
            gUIContext->ClickConsumed = true;

            Rr_UISetFocus(NULL, 0);
        }
        else if (Up && DragMatch)
        {
            gUIContext->DragParent = NULL;
            gUIContext->DragConsumed = true;

            gUIContext->ClickConsumed = true;

            if (!gUIContext->DragMoved)
            {
                ClickResult.ClickCount = ClickCount;
            }
        }
        else if (Held && DragMatch)
        {
            ClickResult.MovedFirstTime =
                gUIContext->MouseMoved && !gUIContext->DragMoved;

            gUIContext->DragConsumed = true;
            gUIContext->DragMoved |= gUIContext->MouseMoved;

            gUIContext->ClickConsumed = true;

            ClickResult.Held = true;
            ClickResult.Moved = gUIContext->MouseMoved;
        }
    }

    return ClickResult;
}

static inline Rr_UIClickResult Rr_UIClickDrag(
    Rr_UILayout *Layout,
    Rr_Rect *Rect,
    Rr_UIHash Hash,
    Rr_Rect Value)
{
    return Rr_UIClickEx(Layout, Rect, RR_UI_CLICK_TYPE_DRAG, Hash, Value);
}

static inline Rr_UIClickResult Rr_UIClickSimple(
    Rr_UILayout *Layout,
    Rr_Rect const *Rect,
    Rr_UIHash Hash)
{
    return Rr_UIClickEx(
        Layout,
        Rect,
        RR_UI_CLICK_TYPE_RELEASE,
        Hash,
        (Rr_Rect){ 0 });
}

static inline Rr_UIClickResult Rr_UIClickMulti(
    Rr_UILayout *Layout,
    Rr_Rect *Rect,
    Rr_UIHash Hash)
{
    return Rr_UIClickEx(
        Layout,
        Rect,
        RR_UI_CLICK_TYPE_DOWN,
        Hash,
        (Rr_Rect){ 0 });
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
            Rr_UIResetDrag();
            *YScroll = *YScroll + gUIContext->MouseWheelDelta.Y *
                                      gUIContext->DefaultFont->LineHeight;

            return true;
        }
    }

    return false;
}

/* TODO: Returning by value would be less error-prone. */
static inline Rr_UIClipRect *Rr_UIEndClipRect(Rr_UILayout *Layout)
{
    if (!Layout || Layout->SkipCompletely)
    {
        return NULL;
    }
    if (Layout->ClipRects->Count > 0)
    {
        Rr_UIClipRect *Last = &RR_LAST_ARRAY_ELEMENT(Layout->ClipRects);
        Last->IndexCount =
            (uint32_t)gUIContext->Indices.Count - Last->FirstIndex;

        return Last;
    }

    return NULL;
}

static inline Rr_UIClipRect *Rr_UIBeginClipRect(
    Rr_UILayout *Layout,
    Rr_Rect *Rect)
{
    Rr_UIClipRect *ClipRect =
        RR_PUSH_INTO_ARRAY(Layout->ClipRects, gUIContext->FrameArena);
    ClipRect->FirstIndex = (uint32_t)gUIContext->Indices.Count;
    if (Layout->ParentClipRect)
    {
        ClipRect->Rect =
            Rr_UIRectIntersection(Rect, &Layout->ParentClipRect->Rect);
    }
    else
    {
        ClipRect->Rect = *Rect;
    }
    ClipRect->Image = Rr_UICurrentFont()->Image;
    ClipRect->ForceLinearPipeline = false;

    Layout->CurrentClipRect = ClipRect;
    Layout->MouseInsideClipRect =
        Rr_RectContains(&ClipRect->Rect, gUIContext->MousePosition);

    return ClipRect;
}

static inline Rr_UIClipRect *Rr_UIPushSubClipRect(
    Rr_UILayout *Layout,
    Rr_Rect const *Rect)
{
    Rr_UIClipRect *Last = Rr_UIEndClipRect(Layout);
    assert(Last);

    Rr_Rect NewRect = Rr_UIRectIntersection(&Last->Rect, Rect);

    return Rr_UIBeginClipRect(Layout, &NewRect);
}

static inline void Rr_UIPopSubClipRect(Rr_UILayout *Layout)
{
    assert(Layout->ClipRects->Count > 1);

    Rr_UIEndClipRect(Layout);

    Rr_UIClipRect *SecondToLast =
        &Layout->ClipRects->Data[Layout->ClipRects->Count - 2];

    Rr_UIBeginClipRect(Layout, &SecondToLast->Rect);
}

static inline void Rr_UIRecalculateStyle(void)
{
    Rr_UIStyle *Style = &gUIContext->Style;
    Rr_UIFont *Font = Rr_UICurrentFont();
    float LineHeight = Font->LineHeight;

    gUIContext->WindowPadding =
        RR_UI_ROUND_V2(Rr_MulV2F(Style->WindowPadding, LineHeight));
    gUIContext->ContentsMargin =
        RR_UI_ROUND_V2(Rr_MulV2F(Style->ContentsMargin, LineHeight));
    gUIContext->ComponentMargin =
        RR_UI_ROUND(Style->ComponentMargin * LineHeight);

    gUIContext->FrameThickness = floorf(LineHeight * Style->FrameThickness);
    gUIContext->ResizeHandleSize =
        RR_UI_ROUND(LineHeight * Style->ScrollbarAreaWidth);
    gUIContext->ScrollbarWidth = gUIContext->ResizeHandleSize;
    gUIContext->ScrollbarHandleWidth =
        RR_UI_ROUND(gUIContext->ResizeHandleSize * 0.75f);
    gUIContext->SeparatorLineHeight = RR_UI_ROUND(LineHeight * 0.5f);
    gUIContext->ButtonPadding =
        RR_UI_ROUND_V2(Rr_MulV2F(Style->ButtonPadding, LineHeight));
    gUIContext->BevelThickness = ceilf(LineHeight * Style->BevelThickness);
    gUIContext->DoubleBevelThickness =
        RR_UI_ROUND(LineHeight * Style->DoubleBevelThickness);
    gUIContext->InputFieldPadding =
        RR_UI_ROUND_V2(Rr_MulV2F(Style->InputFieldPadding, LineHeight));
    gUIContext->FlexibleTitleMargin =
        ceilf(Style->FlexibleTitleMargin * LineHeight);

    gUIContext->TitleBarPadding = Rr_MulV2F(Style->TitleBarPadding, LineHeight);
    gUIContext->TitleBarHeight =
        RR_UI_ROUND(gUIContext->TitleBarPadding.Y * 2.0f + LineHeight);
    gUIContext->MinWindowSizeNoTitle =
        Rr_MulV2F(gUIContext->WindowPadding, 2.0f);
    gUIContext->MinWindowSizeNoTitle.X += gUIContext->ScrollbarWidth;
    gUIContext->MinWindowSizeNoTitle.X += LineHeight * 2.0f;
    gUIContext->MinWindowSizeNoTitle.Y += LineHeight * 2.0f;
    gUIContext->MinWindowSizeNoTitle =
        RR_UI_ROUND_V2(gUIContext->MinWindowSizeNoTitle);
    gUIContext->MinWindowSize = gUIContext->MinWindowSizeNoTitle;
    gUIContext->MinWindowSize.Y += gUIContext->TitleBarHeight;
    gUIContext->MinWindowSize = RR_UI_ROUND_V2(gUIContext->MinWindowSize);

    gUIContext->TopLevelTreeOffset =
        LineHeight * 0.75f + gUIContext->ButtonPadding.X;
    gUIContext->TreeOffset = LineHeight * 0.75f;
}

void Rr_UIPushFont(Rr_UIFont *Font)
{
    assert(Font);

    *RR_PUSH_INTO_ARRAY(&gUIContext->FontStack, gUIContext->FrameArena) = Font;

    Rr_UIRecalculateStyle();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIClipRect *CurrentClipRect = Rr_UIEndClipRect(Layout);
    if (CurrentClipRect)
    {
        Rr_UIBeginClipRect(Layout, &CurrentClipRect->Rect);
    }
}

void Rr_UIPopFont(void)
{
    assert(
        gUIContext->FontStack.Count &&
        "Did you forget to call Rr_UIPushFont()?");
    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->FontStack));

    Rr_UIRecalculateStyle();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIClipRect *CurrentClipRect = Rr_UIEndClipRect(Layout);
    if (CurrentClipRect)
    {
        Rr_UIBeginClipRect(Layout, &CurrentClipRect->Rect);
    }
}

static inline Rr_Vec2 Rr_UIGetMinWindowExtent(Rr_UIWindowFlags Flags)
{
    Rr_Vec2 Size = Rr_V2F(Rr_UICurrentLineHeight());
    Size = Rr_AddV2(Size, Rr_MulV2F(Rr_UICurrentWindowPadding(), 2.0f));
    if (!(Flags & RR_UI_WINDOW_FLAGS_NO_TITLE_BAR_BIT))
    {
        Size.Y += gUIContext->TitleBarHeight;
    }
    if (!(Flags & RR_UI_WINDOW_FLAGS_NO_BORDERS_BIT))
    {
        Size = Rr_AddV2(
            Size,
            Rr_MulV2F(Rr_V2F(gUIContext->DoubleBevelThickness), 2.0f));
    }

    return Rr_FloorV2(Size);
}

Rr_Vec2 Rr_UIGetCursor(void)
{
    Rr_UIAssertWindow();
    Rr_UILayout *Layout = Rr_UICurrentLayout();

    return Layout->Cursor;
}

void Rr_UIAdvance(Rr_Vec2 RigidSize, Rr_Vec2 FlexibleSize)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    Rr_Rect *ContentsRect = &Layout->DeferredContentsRect;
    Rr_Vec2 ContentsMargin = Rr_UICurrentContentsMargin();

    bool Flexible = FlexibleSize.X > 0.0f || FlexibleSize.Y > 0.0f;
    Rr_Vec2 Size;
    if (Flexible)
    {
        Size = FlexibleSize;
    }
    else
    {
        Size = RigidSize;
    }

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

    if (Rr_UIIsHorizontal())
    {
        Layout->HorizontalMaxExtent.Y =
            RR_MAX(Layout->HorizontalMaxExtent.Y, Size.Y);
        Layout->HorizontalMaxExtent.X = RR_MAX(
            Layout->HorizontalMaxExtent.X,
            Layout->Cursor.X + Size.X - Layout->HorizontalX);

        Layout->Cursor.X += Size.X + ContentsMargin.X;
    }
    else
    {
        ContentsRect->Extent.Y = RR_MAX(
            ContentsRect->Extent.Y,
            Layout->Cursor.Y + Size.Y - ContentsRect->Offset.Y);

        Layout->Cursor.Y += Size.Y + ContentsMargin.Y;

        float TotalWidth = (Layout->Cursor.X - ContentsRect->Offset.X) + Size.X;
        ContentsRect->Extent.X = RR_MAX(ContentsRect->Extent.X, TotalWidth);

        if (!Flexible)
        {
            Layout->DeferredMaxRigidWidth =
                RR_MAX(Layout->DeferredMaxRigidWidth, Size.X);
        }
    }
}

static inline void Rr_UISetWindowOffsetChecked(
    Rr_UILayout *Layout,
    Rr_Vec2 Offset)
{
    if (Offset.X != INFINITY)
    {
        Layout->Window->Rect.Offset.X = Offset.X;
    }

    if (Offset.Y != INFINITY)
    {
        Layout->Window->Rect.Offset.Y = Offset.Y;
    }
}

static inline void Rr_UISetWindowExtentChecked(
    Rr_UILayout *Layout,
    Rr_Vec2 Extent)
{
    Rr_Vec2 MinWindowExtent = Rr_UIGetMinWindowExtent(Layout->Window->Flags);

    if (!Layout->LockExtentX && Extent.X != INFINITY)
    {
        Extent.X = floorf(Extent.X);

        if (Layout->MinExtent.X != INFINITY)
        {
            Extent.X = RR_MAX(Extent.X, Layout->MinExtent.X);
        }

        if (Layout->MaxExtent.X != INFINITY)
        {
            Extent.X = RR_MIN(Extent.X, Layout->MaxExtent.X);
        }

        Extent.X = RR_MAX(Extent.X, MinWindowExtent.X);

        Layout->Window->Rect.Extent.X = Extent.X;
    }

    if (!Layout->LockExtentY && Extent.Y != INFINITY)
    {
        Extent.Y = floorf(Extent.Y);

        if (Layout->MinExtent.Y != INFINITY)
        {
            Extent.Y = RR_MAX(Extent.Y, Layout->MinExtent.Y);
        }

        if (Layout->MaxExtent.Y != INFINITY)
        {
            Extent.Y = RR_MIN(Extent.Y, Layout->MaxExtent.Y);
        }

        Extent.Y = RR_MAX(Extent.Y, MinWindowExtent.Y);

        Layout->Window->Rect.Extent.Y = Extent.Y;
    }
}

static inline bool Rr_UIAddCollapseButton(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

    /* Assuming having a title/tab bar. */

    Rr_Rect ButtonRect;
    ButtonRect.Offset = Layout->Rect.Offset;
    ButtonRect.Extent = Rr_V2F(gUIContext->TitleBarHeight);

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
        Rr_MulV2F(Rr_V2F(gUIContext->TitleBarHeight), 0.5f));
    float TriangleSize = gUIContext->TitleBarHeight * 0.3f;
    Rr_UIDrawFitTriangleFilled(
        TriangleCenter,
        TriangleSize,
        !Window->Collapsed ? RR_ANGLE_DEG(90.0f) : 0.0f,
        &gUIContext->Colors.TitleForeground);

    return ClickResult.ClickCount;
}

static inline void Rr_UIAddCloseButton(Rr_UILayout *Layout)
{
    /* Assuming having a title/tab bar. */

    float Width = gUIContext->TitleBarHeight * gUIContext->Style.CrossWidth;
    float Thickness =
        gUIContext->TitleBarHeight * gUIContext->Style.CrossThickness;
    Rr_Rect TitleRect = Layout->Rect;
    TitleRect.Extent.Height = gUIContext->TitleBarHeight;
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
        (TitleRect.Extent.Height + gUIContext->TitleBarHeight) * 0.5f,
    ButtonRect.Offset.Y =
        TitleRect.Offset.Y +
        (TitleRect.Extent.Height - gUIContext->TitleBarHeight) * 0.5f;
    ButtonRect.Extent = Rr_V2F(gUIContext->TitleBarHeight);

    Rr_UIHash Hash =
        Rr_UIGetHash(sizeof("Rr.Close"), "Rr.Close", Rr_UICurrentHash());

    Rr_UIClickResult ClickResult = Rr_UIClickSimple(Layout, &ButtonRect, Hash);
    if (ClickResult.ClickCount && Layout->Open)
    {
        *Layout->Open = false;
    }

    Rr_UIDrawBevel(
        &ButtonRect,
        &gUIContext->Colors.TitleCloseButtonBackground,
        ClickResult.Held && ClickResult.Hovered);

    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(45.0f),
        &gUIContext->Colors.TitleForeground);
    Rr_UIDrawRotatedQuad(
        &BarRect,
        RR_ANGLE_DEG(-45.0f),
        &gUIContext->Colors.TitleForeground);
}

static inline Rr_Vec2 Rr_UIGetWindowOffsetRelativeToTitle(
    Rr_UIWindow *Window,
    Rr_Vec2 Offset)
{
    if (!Rr_UIWindowNoBorders(Window))
    {
        Offset = Rr_SubV2(Offset, Rr_V2F(gUIContext->DoubleBevelThickness));
    }
    if (!Rr_UIWindowNoCollapse(Window))
    {
        Offset.X -= gUIContext->TitleBarHeight;
    }
    Offset = Rr_SubV2(Offset, gUIContext->TitleBarPadding);

    return Offset;
}

static inline Rr_Vec2 Rr_UICalculateTitleBarSize(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

    if (Rr_UIWindowNoTitleBar(Window))
    {
        return Rr_V2F(0.0f);
    }

    float Width = 0.0f;
    float Height = 0.0f;

    if (Rr_UIWindowTabs(Window))
    {
        Width += Layout->TabsCursor.X - Layout->TabsCursorStart.X;
    }
    else
    {
        Width += Rr_UICalculateTextSize(SIZE_MAX, Window->Title, 0.0f).X;
        Width += gUIContext->TitleBarPadding.Width * 2.0f;
    }

    if (!Rr_UIWindowNoClose(Window) && Layout->Open)
    {
        Width += gUIContext->TitleBarHeight;
    }
    if (!Rr_UIWindowNoCollapse(Window))
    {
        Width += gUIContext->TitleBarHeight;
    }
    Height += gUIContext->TitleBarHeight;

    return Rr_V2(Width, Height);
}

static inline void Rr_UIAddWindowTabBar(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

    /* TODO: Better background. */

    Rr_Rect TabBarBackgroundRect = {
        .Offset = Layout->Cursor,
        .Extent = Rr_V2(Layout->Rect.Extent.Width, gUIContext->TitleBarHeight),
    };
    Rr_UIDrawSolidQuad(
        &TabBarBackgroundRect,
        Window->Child ? &gUIContext->Colors.ChildBackground
                      : &gUIContext->Colors.Background);

    Rr_Rect TitleBarRect = {
        Layout->Rect.Offset,
        Rr_V2(Layout->Rect.Extent.Width, gUIContext->TitleBarHeight),
    };

    Rr_UIDrawRect(&TitleBarRect, &gUIContext->Colors.TitleBackgroundTabs);

    Rr_Vec2 TabsCursor = Layout->Rect.Offset;

    bool HasCollapse = !Rr_UIWindowNoCollapse(Window);
    if (HasCollapse)
    {
        Rr_UIAddCollapseButton(Layout);

        TabsCursor.X += gUIContext->TitleBarHeight;
        TitleBarRect.Offset.X += gUIContext->TitleBarHeight;
        TitleBarRect.Extent.X -= gUIContext->TitleBarHeight;
    }

    if (!Rr_UIWindowNoClose(Window) && Layout->Open)
    {
        Rr_UIAddCloseButton(Layout);

        TitleBarRect.Extent.Width -= gUIContext->TitleBarHeight;
    }

    Layout->TabsCursorStart = TabsCursor;
    Layout->TabsCursor = TabsCursor;
}

static inline void Rr_UIAddWindowTitleBar(Rr_UILayout *Layout, bool *Open)
{
    Rr_UIWindow *Window = Layout->Window;
    Rr_UIPrimitive BevelPrimitive = Rr_UIReserveBevel();

    Rr_Rect TitleBarRect = {
        Layout->Rect.Offset,
        Rr_V2(Layout->Rect.Extent.Width, gUIContext->TitleBarHeight),
    };

    Rr_Vec2 TitleOffset =
        Rr_AddV2(Layout->Rect.Offset, gUIContext->TitleBarPadding);

    bool HasCollapse = !Rr_UIWindowNoCollapse(Window);
    bool CollapseButtonClicked = false;
    if (HasCollapse)
    {
        CollapseButtonClicked = Rr_UIAddCollapseButton(Layout);

        TitleOffset.X += gUIContext->TitleBarHeight;
        TitleBarRect.Offset.X += gUIContext->TitleBarHeight;
        TitleBarRect.Extent.X -= gUIContext->TitleBarHeight;
    }

    Rr_UIDrawText(
        false,
        TitleOffset,
        SIZE_MAX,
        Window->Title,
        0.0f,
        &gUIContext->Colors.TitleForeground);

    if (!Rr_UIWindowNoClose(Window) && Layout->Open)
    {
        Rr_UIAddCloseButton(Layout);

        TitleBarRect.Extent.Width -= gUIContext->TitleBarHeight;
    }

    /* Allow double clicking the title bevel to toggle collapse state. */

    if (HasCollapse && !CollapseButtonClicked)
    {
        Rr_UIHash Hash = Rr_UIGetHash(
            sizeof("Rr.CollapseTitle"),
            "Rr.CollapseTitle",
            Rr_UICurrentHash());
        Rr_UIClickResult ClickResult =
            Rr_UIClickMulti(Layout, &TitleBarRect, Hash);
        if (ClickResult.ClickCount == 1 && Rr_UIWindowUndockable(Window) &&
            (gPlatform.Keymod & RR_KEYMOD_CTRL))
        {
            if (Window->Undocked)
            {
                Window->Undocked = false;
            }
            else
            {
                Window->UndockedOffset = Layout->Rect.Offset;
                Window->UndockNextFrame = true;
            }

            Rr_UIConsumeMouseInput();
        }
        else if (ClickResult.ClickCount == 2)
        {
            Window->Collapsed = !Window->Collapsed;

            Rr_UIConsumeMouseInput();
        }
    }

    Rr_Vec4 Colors[4] = { gUIContext->Colors.TitleBackground,
                          gUIContext->Colors.TitleBackground2,
                          gUIContext->Colors.TitleBackground,
                          gUIContext->Colors.TitleBackground2 };
    Rr_UIBevelEx(BevelPrimitive, &TitleBarRect, Colors, false);
}

static inline Rr_UIClickResult Rr_UIAddResizeHandle(
    Rr_UILayout *Layout,
    char const *HashName,
    Rr_UIResizeType ResizeType)
{
    Rr_UIWindow *Window = Layout->Window;

    float DoubleBevelThickness = gUIContext->DoubleBevelThickness;

    Rr_Rect Rect;
    switch (ResizeType)
    {
        case RR_UI_RESIZE_TYPE_N:
        {
            Rect.Offset = Window->Rect.Offset;
            Rect.Extent =
                Rr_V2(Window->Rect.Extent.Width, DoubleBevelThickness);
        }
        break;
        case RR_UI_RESIZE_TYPE_S:
        {
            Rect.Offset = Rr_AddV2(
                Window->Rect.Offset,
                Rr_V2(0.0f, Window->Rect.Extent.Y - DoubleBevelThickness));
            Rect.Extent =
                Rr_V2(Window->Rect.Extent.Width, DoubleBevelThickness);
        }
        break;
        case RR_UI_RESIZE_TYPE_E:
        {
            Rect.Offset = Rr_AddV2(
                Window->Rect.Offset,
                Rr_V2(Window->Rect.Extent.Width - DoubleBevelThickness, 0.0f));
            Rect.Extent =
                Rr_V2(DoubleBevelThickness, Window->Rect.Extent.Height);
        }
        break;
        case RR_UI_RESIZE_TYPE_W:
        {
            Rect.Offset = Window->Rect.Offset;
            Rect.Extent =
                Rr_V2(DoubleBevelThickness, Window->Rect.Extent.Height);
        }
        break;
        case RR_UI_RESIZE_TYPE_SE:
        {
            Rr_Vec2 ResizeHandleExtent = Rr_V2F(gUIContext->ResizeHandleSize);
            Rect.Offset = Rr_SubV2(
                Rr_AddV2(Window->Rect.Offset, Window->Rect.Extent),
                ResizeHandleExtent);
            Rect.Extent = ResizeHandleExtent;
        }
        break;
        default:
        {
            RR_LOG_ABORT("Invalid resize type!");
        }
        break;
    }

    Rr_UIHash ResizeHash =
        Rr_UIGetHash(strlen(HashName), HashName, Rr_UICurrentHash());
    Rr_UIClickResult ClickResult = Rr_UIClickEx(
        Layout,
        &Rect,
        RR_UI_CLICK_TYPE_DRAG,
        ResizeHash,
        Window->Rect);

    if (ClickResult.Held ||
        (gUIContext->DragParent == Window &&
         gUIContext->DragHash == ResizeHash) ||
        (ClickResult.Hovered && gUIContext->DragParent == NULL))
    {
        Rr_CursorType CursorType;
        switch (ResizeType)
        {
            case RR_UI_RESIZE_TYPE_N:
            case RR_UI_RESIZE_TYPE_S:
            {
                CursorType = RR_CURSOR_TYPE_RESIZE_NS;
            }
            break;
            case RR_UI_RESIZE_TYPE_E:
            case RR_UI_RESIZE_TYPE_W:
            {
                CursorType = RR_CURSOR_TYPE_RESIZE_EW;
            }
            break;
            case RR_UI_RESIZE_TYPE_SE:
            {
                CursorType = RR_CURSOR_TYPE_RESIZE_NWSE;
            }
            break;
            default:
            {
                RR_LOG_ABORT("Invalid resize type!");
            }
            break;
        }
        gUIContext->CursorType = CursorType;
    }

    if (ClickResult.ClickCount == 2)
    {
        Layout->DeferredAutoResize = true;
        Rr_UIResetDrag();
    }
    else if (ClickResult.Moved)
    {
        switch (ResizeType)
        {
            case RR_UI_RESIZE_TYPE_N:
            {
                float Height = gUIContext->DragValueStart.Extent.Y +
                               (gUIContext->DragMouseStart.Y -
                                gUIContext->MousePosition.Y);
                Rr_UISetWindowExtentChecked(Layout, Rr_V2(INFINITY, Height));
                float Offset = gUIContext->MousePosition.Y +
                               (gUIContext->DragValueStart.Offset.Y -
                                gUIContext->DragMouseStart.Y);
                Offset = RR_MIN(
                    Offset,
                    gUIContext->DragValueStart.Offset.Y +
                        gUIContext->DragValueStart.Extent.Y -
                        Layout->MinExtent.Y);
                Rr_UISetWindowOffsetChecked(Layout, Rr_V2(INFINITY, Offset));
            }
            break;
            case RR_UI_RESIZE_TYPE_S:
            {
                float Height = gUIContext->MousePosition.Y -
                               Window->Rect.Offset.Y +
                               (gUIContext->DragValueStart.Offset.Y +
                                gUIContext->DragValueStart.Extent.Y -
                                gUIContext->DragMouseStart.Y);
                Rr_UISetWindowExtentChecked(Layout, Rr_V2(INFINITY, Height));
            }
            break;
            case RR_UI_RESIZE_TYPE_E:
            {
                float Width = gUIContext->MousePosition.X -
                              Window->Rect.Offset.X +
                              (gUIContext->DragValueStart.Offset.X +
                               gUIContext->DragValueStart.Extent.X -
                               gUIContext->DragMouseStart.X);
                Rr_UISetWindowExtentChecked(Layout, Rr_V2(Width, INFINITY));
            }
            break;
            case RR_UI_RESIZE_TYPE_W:
            {
                float Width = gUIContext->DragValueStart.Extent.X +
                              (gUIContext->DragMouseStart.X -
                               gUIContext->MousePosition.X);
                Rr_UISetWindowExtentChecked(Layout, Rr_V2(Width, INFINITY));
                float Offset = gUIContext->MousePosition.X +
                               (gUIContext->DragValueStart.Offset.X -
                                gUIContext->DragMouseStart.X);
                Offset = RR_MIN(
                    Offset,
                    gUIContext->DragValueStart.Offset.X +
                        gUIContext->DragValueStart.Extent.X -
                        Layout->MinExtent.X);
                Rr_UISetWindowOffsetChecked(Layout, Rr_V2(Offset, INFINITY));
            }
            break;
            case RR_UI_RESIZE_TYPE_SE:
            {
                Rr_Vec2 Extent = Rr_AddV2(
                    Rr_SubV2(gUIContext->MousePosition, Window->Rect.Offset),
                    Rr_SubV2(
                        Rr_AddV2(
                            gUIContext->DragValueStart.Offset,
                            gUIContext->DragValueStart.Extent),
                        gUIContext->DragMouseStart));
                Rr_UISetWindowExtentChecked(Layout, Extent);
            }
            break;
            default:
            {
                RR_LOG_ABORT("Invalid resize type!");
            }
            break;
        }
    }

    return ClickResult;
}

static inline void Rr_UIAddResizeHandles(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

    /* NOTE: Work with Window->Rect directly since Layout->Rect is transformed
     * to accomodate for borders. */

    if (!Rr_UIWindowNoBorders(Window) &&
        (gUIContext->HoveredWindow == Window ||
         gUIContext->DragParent ==
             Window)) /* Don't test every window out there. */
    {
        Rr_UIAddResizeHandle(Layout, "Rr.ResizeN", RR_UI_RESIZE_TYPE_N);
        Rr_UIAddResizeHandle(Layout, "Rr.ResizeS", RR_UI_RESIZE_TYPE_S);
        Rr_UIAddResizeHandle(Layout, "Rr.ResizeE", RR_UI_RESIZE_TYPE_E);
        Rr_UIAddResizeHandle(Layout, "Rr.ResizeW", RR_UI_RESIZE_TYPE_W);
    }

    Rr_UIClickResult ClickResult =
        Rr_UIAddResizeHandle(Layout, "Rr.ResizeSE", RR_UI_RESIZE_TYPE_SE);

    if (ClickResult.Held)
    {
        Layout->DeferredResizeHandleColor =
            &gUIContext->Colors.ResizeHandleHeld;
    }
    else if (ClickResult.Hovered)
    {
        Layout->DeferredResizeHandleColor =
            &gUIContext->Colors.ResizeHandleHovered;
    }
    else
    {
        Layout->DeferredResizeHandleColor =
            &gUIContext->Colors.ResizeHandleNormal;
    }
}

static inline Rr_Rect Rr_UIGetWindowContentsArea(
    Rr_UILayout *Layout,
    float *OutFillRatio)
{
    Rr_UIWindow *Window = Layout->Window;

    Rr_Rect Rect = Layout->Rect;

    if (!Rr_UIWindowNoTitleBar(Window))
    {
        Rect.Offset.Y += gUIContext->TitleBarHeight;
        Rect.Extent.Y -= gUIContext->TitleBarHeight;
    }

    float ContentsHeight = Window->ContentsRect.Extent.Y;
    ContentsHeight += Layout->WindowPadding.Y;
    if (ContentsHeight == 0.0f)
    {
        return Rect;
    }
    float FillRatio = ContentsHeight / Rect.Extent.Y;
    if (OutFillRatio)
    {
        *OutFillRatio = FillRatio;
    }
    if (!Rr_UIWindowNoVerticalScrollbar(Window) && FillRatio > 1.0f)
    {
        Rect.Extent.X -= gUIContext->ScrollbarWidth;
    }

    return Rect;
}

static inline bool Rr_UIAddVerticalScrollbar(Rr_UILayout *Layout)
{
    Rr_UIWindow *Window = Layout->Window;

    bool HasResize = !Rr_UIWindowNoResize(Window);

    Rr_Rect ContentsAreaRect = Rr_UIGetWindowContentsArea(Layout, NULL);
    float ContentsHeight = Window->ContentsRect.Extent.Y;
    if (ContentsHeight == 0.0f)
    {
        return false;
    }
    ContentsHeight += Layout->WindowPadding.Y;
    float FillRatio = ContentsAreaRect.Extent.Y / ContentsHeight;
    float MaxYScroll = RR_MAX(0.0f, ContentsHeight - ContentsAreaRect.Extent.Y);

    if (FillRatio < 1.0f)
    {
        if (!Rr_UIWindowNoVerticalScrollbar(Window))
        {
            Rr_Vec2 ScrollbarPosition = ContentsAreaRect.Offset;
            ScrollbarPosition.X += ContentsAreaRect.Extent.X;
            Rr_Vec2 ScrollbarSize = { gUIContext->ScrollbarWidth,
                                      ContentsAreaRect.Extent.Y };
            Rr_UIDrawSolidQuad(
                &(Rr_Rect){
                    ScrollbarPosition,
                    ScrollbarSize,
                },
                &gUIContext->Colors.ScrollbarBackground);

            float ScrollbarHandleOffset = (gUIContext->ScrollbarWidth -
                                           gUIContext->ScrollbarHandleWidth) /
                                          2.0f;

            Rr_Vec2 ScrollbarHandlePosition = ScrollbarPosition;
            Rr_Vec2 ScrollbarHandleSize = ScrollbarSize;
            ScrollbarHandlePosition.X += ScrollbarHandleOffset;
            ScrollbarHandleSize.X = gUIContext->ScrollbarHandleWidth;
            ScrollbarHandleSize.Y *= FillRatio;

            ScrollbarHandlePosition.Y += roundf(Window->VScroll * FillRatio);

            /* Vertical margins. */

            ScrollbarHandlePosition.Y += ScrollbarHandleOffset;
            ScrollbarHandleSize.Y -= ScrollbarHandleOffset * 2.0f;
            ScrollbarHandleSize.Y = RR_MAX(
                ScrollbarHandleSize.Y,
                gUIContext->BevelThickness * 3.0f);

            /* This cuts a bit of height from the scrollbar hitbox so the resize
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
                (Rr_Rect){ .Offset = { 0.0f, Window->VScroll } });

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
                    gUIContext->DragValueStart.Offset.Y = Window->VScroll;
                }
                else if (
                    gUIContext->MousePosition.Y < ScrollbarHandlePosition.Y)
                {
                    Window->VScroll = Window->VScrollTarget =
                        (gUIContext->MousePosition.Y - ScrollbarPosition.Y -
                         ScrollbarHandleOffset * 2.0f) /
                        ((ScrollbarSize.Y) / ContentsHeight);
                    gUIContext->DragValueStart.Offset.Y = Window->VScroll;
                }
            }

            if (ClickResult.Moved)
            {
                float Delta =
                    gUIContext->MousePosition.Y - gUIContext->DragMouseStart.Y;
                Window->VScroll = Window->VScrollTarget =
                    gUIContext->DragValueStart.Offset.Y +
                    (Delta * 1.0f / FillRatio);
            }

            Rr_UIDrawBevel(
                &(Rr_Rect){
                    ScrollbarHandlePosition,
                    ScrollbarHandleSize,
                },
                ClickResult.Held ? &gUIContext->Colors.ScrollbarHeld
                                 : &gUIContext->Colors.ScrollbarNormal,
                false);

            Layout->VerticalScrollbarAdded = true;
        }
    }
    else
    {
        /* Window->VScroll = 0.0f; */
        Window->VScrollTarget = 0.0f;
    }

    Window->VScroll = RR_CLAMP(0.0f, Window->VScroll, MaxYScroll);
    Window->VScrollTarget =
        RR_CLAMP(0.0f, roundf(Window->VScrollTarget), MaxYScroll);

    return Layout->VerticalScrollbarAdded;
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
        gUIContext->FrameArena) = Places;
}

void Rr_UIPopFormatFloatDecimalPlaces(void)
{
    assert(
        gUIContext->FormatFloatDecimalPlacesStack.Count &&
        "Did you forget to call Rr_UIPushFormatFloatDecimalPlaces()?");
    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->FormatFloatDecimalPlacesStack));
}

static inline int Rr_UIFormatDouble(
    size_t BufferCapacity,
    char *Buffer,
    double Value)
{
    if (gUIContext->FormatFloatDecimalPlacesStack.Count > 0)
    {
        uint32_t Top =
            RR_LAST_ARRAY_ELEMENT(&gUIContext->FormatFloatDecimalPlacesStack);
        switch (Top)
        {
            case 0:
                return snprintf(Buffer, BufferCapacity, "%.0f", Value);
            case 1:
                return snprintf(Buffer, BufferCapacity, "%.1f", Value);
            case 2:
                return snprintf(Buffer, BufferCapacity, "%.2f", Value);
            case 3:
                return snprintf(Buffer, BufferCapacity, "%.3f", Value);
            case 4:
                return snprintf(Buffer, BufferCapacity, "%.4f", Value);
            case 5:
                return snprintf(Buffer, BufferCapacity, "%.5f", Value);
            case 6:
                return snprintf(Buffer, BufferCapacity, "%.6f", Value);
            case 7:
                return snprintf(Buffer, BufferCapacity, "%.7f", Value);
            case 8:
                return snprintf(Buffer, BufferCapacity, "%.8f", Value);
            case 9:
                return snprintf(Buffer, BufferCapacity, "%.9f", Value);
            default:
                break;
        }
    }

    return snprintf(Buffer, BufferCapacity, "%.2f", Value);
}

static inline bool Rr_UIConsumeWindowFloat(float *Src, float *Dst)
{
    if (*Src != INFINITY)
    {
        *Dst = floorf(*Src);
        *Src = INFINITY;

        return true;
    }

    return false;
}

static inline bool Rr_UIConsumeWindowFloat2(Rr_Vec2 *Src, Rr_Vec2 *Dst)
{
    return Rr_UIConsumeWindowFloat(&Src->X, &Dst->X) ||
           Rr_UIConsumeWindowFloat(&Src->Y, &Dst->Y);
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

static inline bool Rr_UIConsumeNextWindowOpenOffset(Rr_Vec2 *OutResult)
{
    if (gUIContext->NextWindowOpenOffset.X != INFINITY &&
        gUIContext->NextWindowOpenOffset.Y != INFINITY)
    {
        *OutResult = Rr_FloorV2(gUIContext->NextWindowOpenOffset);
        gUIContext->NextWindowOpenOffset = Rr_V2F(INFINITY);

        return true;
    }

    return false;
}

void Rr_UISetNextWindowExtent(Rr_Vec2 Extent)
{
    gUIContext->NextWindowExtent = Extent;
}

void Rr_UISetNextWindowMinExtent(Rr_Vec2 Extent)
{
    gUIContext->NextWindowMinExtent = Extent;
}

void Rr_UISetNextWindowMaxExtent(Rr_Vec2 Extent)
{
    gUIContext->NextWindowMaxExtent = Extent;
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

static inline void Rr_UIPutWindowOnTop(Rr_UIWindow *Window)
{
    if (Window->Child)
    {
        return;
    }
    if (!gUIContext->HighestWindow || gUIContext->HighestWindow == Window)
    {
        return;
    }
    Window->TopLevelParent->Z = gUIContext->HighestWindow->Z + 1;
}

static inline bool Rr_UIPushWindowLayout(
    Rr_UIWindow *Window,
    Rr_UIHash Hash,
    bool *Open)
{
    assert(!Window->Added && "There already is a window with this hash!");

    bool LockExtentX = Rr_UIConsumeWindowFloat(
        &gUIContext->NextWindowExtent.X,
        &Window->Rect.Extent.X);
    bool LockExtentY = Rr_UIConsumeWindowFloat(
        &gUIContext->NextWindowExtent.Y,
        &Window->Rect.Extent.Y);

    Rr_Vec2 GenericMinExtent = Rr_UIGetMinWindowExtent(Window->Flags);

    float MinExtentX = GenericMinExtent.X;
    bool LockMinExtentX = Rr_UIConsumeWindowFloat(
        &gUIContext->NextWindowMinExtent.X,
        &MinExtentX);
    float MinExtentY = GenericMinExtent.Y;
    bool LockMinExtentY = Rr_UIConsumeWindowFloat(
        &gUIContext->NextWindowMinExtent.Y,
        &MinExtentY);

    float MaxExtentX = INFINITY;
    bool LockMaxExtentX = Rr_UIConsumeWindowFloat(
        &gUIContext->NextWindowMaxExtent.X,
        &MaxExtentX);
    float MaxExtentY = INFINITY;
    bool LockMaxExtentY = Rr_UIConsumeWindowFloat(
        &gUIContext->NextWindowMaxExtent.Y,
        &MaxExtentY);

    bool LockExtent = Rr_UIConsumeNextWindowOffset(Window);

    Rr_Vec2 OpenOffset;
    bool WindowOpenOffsetConsumed =
        Rr_UIConsumeNextWindowOpenOffset(&OpenOffset);

    Rr_UIConsumeNextWindowCreateCollapsed(Window);

    /* Return if closed.
     * Also handle show after being closed.
     * This will put window on top unless there is a flag
     * preventing that which is not currently implemented. */

    /* bool NoBorders = Rr_UIWindowNoBorders(Window); */
    bool HasTitleBar = !Rr_UIWindowNoTitleBar(Window);

    bool WasClosed = Window->Open == false;
    Window->Open = (Open == NULL || *Open == true);
    if (!Window->Open)
    {
        Rr_UIPushEmptyLayout(Hash, Window);

        return false;
    }
    else if (WasClosed)
    {
        if (WindowOpenOffsetConsumed)
        {
            Window->Rect.Offset = OpenOffset;
        }

        Window->OpenedThisFrame = true;
        Window->SkipThisFrame = true;
        Rr_UIPutWindowOnTop(Window);
    }

    Window->Added = true;

    Rr_UILayout *ParentLayout = Rr_UICurrentLayout();
    Rr_UIEndClipRect(ParentLayout);

    Rr_UILayout *Layout = Rr_UIPushLayout(Hash);
    Layout->Window = Window;
    Layout->Rect = Window->Rect;
    Layout->Cursor = Window->Rect.Offset;
    Layout->TotalAvailableContentsWidth = Window->Rect.Extent.X;
    Layout->WasCollapsed = Window->Collapsed;
    Layout->HorizontalX = INFINITY;
    Layout->DeferredAutoResize = Rr_UIWindowAutoResize(Window);
    Layout->Open = Open;
    Layout->LockOffset = LockExtent;
    Layout->LockExtentX = LockExtentX;
    Layout->LockExtentY = LockExtentY;
    Layout->MinExtent = Rr_V2(MinExtentX, MinExtentY);
    Layout->MaxExtent = Rr_V2(MaxExtentX, MaxExtentY);

    if (Window->Child)
    {
        Layout->ParentClipRect = ParentLayout->CurrentClipRect;
        Layout->ClipRects = ParentLayout->ClipRects;
        Layout->TopLevelParent = ParentLayout->TopLevelParent;
    }
    else
    {
        Layout->ClipRects =
            Rr_Alloc(sizeof(Rr_UIClipRectArray), gUIContext->FrameArena);
        Layout->TopLevelParent = Layout;
    }

    if (Rr_UIWindowTabs(Window))
    {
        Layout->WindowPadding = Rr_V2F(0.0f);
        Layout->DeferredSelectedTab = Window->SelectedTab;
    }
    else
    {
        Layout->WindowPadding = Rr_UICurrentWindowPadding();
    }

    if (!(LockExtentX || LockExtentY))
    {
        if (Window->Child)
        {
            Layout->Rect.Extent.X =
                Rr_UIGetAvailableContentsWidth(ParentLayout);
        }
        else
        {
            if (!Window->ShownAtLeastOnce && Window->OpenedThisFrame)
            {
                Layout->DeferredAutoResize = true;
            }
        }
    }

    /* Calculate total and visible extents. */

    Rr_UIBeginClipRect(Layout, &Layout->Rect);

    if (Layout->WasCollapsed)
    {
        Layout->Rect.Extent.Y = gUIContext->TitleBarHeight;

        if (!Rr_UIWindowNoBorders(Window))
        {
            Layout->Rect.Extent.Y += gUIContext->DoubleBevelThickness * 2.0f;
        }
    }

    /* Transform working area if using borders. */

    if (!Rr_UIWindowNoBorders(Window))
    {
        Layout->Rect =
            Rr_ResizeRect(&Layout->Rect, -gUIContext->DoubleBevelThickness);
        Layout->Cursor =
            Rr_AddV2F(Layout->Cursor, gUIContext->DoubleBevelThickness);
    }

    /* Add title bar or tab bar if necessary. */

    if (Rr_UIWindowTabs(Window))
    {
        Rr_UIAddWindowTabBar(Layout);

        Layout->Cursor.Y += gUIContext->TitleBarHeight;
    }
    else if (HasTitleBar)
    {
        Rr_UIAddWindowTitleBar(Layout, Open);

        Layout->Cursor.Y += gUIContext->TitleBarHeight;
    }

    /* NOTE: At this point window might have become collapsed! */

    if (Layout->WasCollapsed)
    {
        Layout->SkipItems = true;

        return false;
    }

    /* Draw background if needed. */

    if (!Rr_UIWindowNoBackground(Window))
    {
        Rr_Rect BackgroundRect = {
            .Offset = Layout->Cursor,
            .Extent = Layout->Rect.Extent,
        };
        Rr_UIDrawSolidQuad(
            &BackgroundRect,
            Window->Child ? &gUIContext->Colors.ChildBackground
                          : &gUIContext->Colors.Background);
    }

    Layout->TotalAvailableContentsWidth = Layout->Rect.Extent.X;

    /* Add vertical scrollbar if necessary. */

    if (!Layout->WasCollapsed)
    {
        if (Rr_UIAddVerticalScrollbar(Layout))
        {
            Layout->TotalAvailableContentsWidth -= gUIContext->ScrollbarWidth;
        }

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

        Rr_UIAddResizeHandles(Layout);
    }

    Layout->TotalAvailableContentsWidth -= Layout->WindowPadding.X * 2.0f;
    Layout->Cursor = Rr_AddV2(Layout->Cursor, Layout->WindowPadding);

    /* Clip to contents. */

    Rr_UIEndClipRect(Layout);
    Rr_Rect ContentsAreaRect = Rr_UIGetWindowContentsArea(Layout, NULL);
    Rr_UIBeginClipRect(Layout, &ContentsAreaRect);

    Layout->DeferredContentsRect.Offset = Layout->Cursor;

    return true;
}

static inline void Rr_UIShowPopupWindow(
    Rr_UIWindow *ParentWindow,
    Rr_UIHash Hash)
{
    gUIContext->PopupWindowParent = ParentWindow;
    gUIContext->PopupWindowHash = Hash;
    gUIContext->PopupWindowOpen = true;
    /* gUIContext->PopupWindow.SkipThisFrame = true; */
}

static inline bool Rr_UIShouldShowPopupWindow(
    Rr_UIWindow *ParentWindow,
    Rr_UIHash Hash)
{
    return gUIContext->PopupWindowOpen &&
           gUIContext->PopupWindowParent == ParentWindow &&
           gUIContext->PopupWindowHash == Hash;
}

static inline void Rr_UIBeginPopupWindow(Rr_UIHash Hash, Rr_UIWindowFlags Flags)
{
    Rr_UIWindow *Window = &gUIContext->PopupWindow;
    Window->Flags = Flags;
    Window->TopLevelParent = Window;
    Rr_UIPushWindowLayout(Window, Hash, NULL);
    Rr_UICurrentLayout()->DeferredClampOffsetToScreen = true;
}

static inline void Rr_UIClosePopupWindow(void)
{
    gUIContext->PopupWindowParent = NULL;
    gUIContext->PopupWindowHash = 0;
    gUIContext->PopupWindowOpen = false;
    gUIContext->PopupWindow.Open = false;
}

static bool Rr_UIPopupWindowActive(void)
{
    return gUIContext->PopupWindowParent && gUIContext->PopupWindowOpen;
}

static inline Rr_UIWindow *Rr_UICreateWindow(
    size_t TitleLength,
    char const *Title,
    uint64_t TitleHash,
    Rr_UIWindowFlags Flags)
{
    Rr_UIWindow *Window = Rr_Alloc(sizeof(Rr_UIWindow), gUIContext->Arena);
    char *TitleCopy = memcpy(
        Rr_Alloc(TitleLength + 1, gUIContext->Arena),
        Title,
        TitleLength);
    TitleCopy[TitleLength] = '\0';
    Window->Title = TitleCopy;
    Window->Flags = Flags;
    Window->Rect.Extent = Rr_UIGetMinWindowExtent(Flags);
    Window->CreatedThisFrame = true;

    Window->SkipThisFrame = true;
    Window->Flags |= RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;

    return Window;
}

static inline bool Rr_UIGenericButton(
    Rr_UILayout *Layout,
    char const *Text,
    Rr_Rect const *Rect,
    Rr_Vec4 const *ColorText,
    Rr_Vec4 const *ColorNormal,
    Rr_Vec4 const *ColorHeld)
{
    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Text, &TitleLength);

    Rr_Vec2 TitleSize = Rr_UICalculateTextSize(TitleLength, Text, 0.0f);

    Rr_UIPrimitive Primitive = Rr_UIReserveBevel();

    Rr_Vec2 TitlePosition = Rr_AddV2(
        Rect->Offset,
        Rr_SubV2(Rr_MulV2F(Rect->Extent, 0.5f), Rr_MulV2F(TitleSize, 0.5f)));
    Rr_UIDrawText(false, TitlePosition, TitleLength, Text, 0.0f, ColorText);

    Rr_UIClickResult ClickResult = Rr_UIClickSimple(Layout, Rect, TitleHash);

    Rr_UIBevel(
        Primitive,
        Rect,
        ClickResult.Held ? ColorHeld : ColorNormal,
        ClickResult.Held);

    return ClickResult.ClickCount;
}

static bool Rr_UIBeginDockedChildWindow(
    Rr_UIWindow *Window,
    Rr_UIHash TitleHash,
    bool *Open,
    Rr_UILayout *ParentLayout,
    Rr_UIWindow *ParentWindow)
{
    if (ParentLayout->SkipCompletely)
    {
        Rr_UIPushEmptyLayout(TitleHash, Window);

        return false;
    }

    if (Rr_UIWindowTabs(ParentWindow))
    {
        /* TODO: Investigate better ways of adding tab buttons (without
         * pushing more clip rects). */

        Rr_Rect TabContentsRect = Rr_UIEndClipRect(ParentLayout)->Rect;
        Rr_UIBeginClipRect(ParentLayout, &ParentLayout->Rect);

        Window->TabsParent = ParentWindow;

        bool Selected = ParentWindow->SelectedTab == Window;
        if (!ParentLayout->DeferredSelectedTab)
        {
            ParentLayout->DeferredSelectedTab = Window;
        }
        if (!ParentWindow->SelectedTab)
        {
            ParentWindow->SelectedTab = Window;
            Selected = true;
        }

        Rr_Vec2 TitleSize = Rr_UICalculateTextSize(SIZE_MAX, Window->Title, 0);
        Rr_Rect ButtonRect = {
            .Offset = ParentLayout->TabsCursor,
            .Extent = Rr_V2(
                TitleSize.X + gUIContext->TitleBarPadding.X * 2.0f,
                gUIContext->TitleBarHeight),
        };
        if (Rr_UIGenericButton(
                ParentLayout,
                Window->Title,
                &ButtonRect,
                &gUIContext->Colors.TitleForeground,
                Selected ? &gUIContext->Colors.TitleBackground
                         : &gUIContext->Colors.TitleBackgroundInactive,
                Selected ? &gUIContext->Colors.TitleBackground
                         : &gUIContext->Colors.ButtonHeld))
        {
            if (Rr_UIWindowUndockable(Window) &&
                (gPlatform.Keymod & RR_KEYMOD_CTRL))
            {
                Rr_Vec2 TitlePosition = ButtonRect.Offset;
                TitlePosition.X += gUIContext->TitleBarPadding.X;
                TitlePosition.Y += gUIContext->TitleBarPadding.Y;
                Window->UndockedOffset =
                    Rr_UIGetWindowOffsetRelativeToTitle(Window, TitlePosition);
                Window->UndockNextFrame = true;

                if (Selected)
                {
                    ParentLayout->DeferredSelectedTab =
                        ParentLayout->LastAddedTab;
                }

                Rr_UIConsumeMouseInput();
            }
            else if (!Selected)
            {
                ParentLayout->DeferredSelectedTab = Window;

                Rr_UIConsumeMouseInput();
            }
        }

        ParentLayout->TabsCursor.X += ButtonRect.Extent.X;
        ParentLayout->LastAddedTab = Window;

        Rr_UIEndClipRect(ParentLayout);
        Rr_UIBeginClipRect(ParentLayout, &TabContentsRect);

        if (!Selected || ParentLayout->WasCollapsed)
        {
            Rr_UIPushEmptyLayout(TitleHash, Window);

            return false;
        }

        Window->Flags |= RR_UI_WINDOW_FLAGS_NO_TITLE_BAR_BIT;
        Window->Flags |= RR_UI_WINDOW_FLAGS_NO_BORDERS_BIT;
        Window->Tab = true;
        Window->Collapsed = false;
    }

    Rr_UIWindowFlags const CHILD_FLAGS =
        RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT | RR_UI_WINDOW_FLAGS_NO_MOVE_BIT |
        RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT;

    Window->Flags |= CHILD_FLAGS;
    Window->Child = true;
    Window->Rect.Offset = ParentLayout->Cursor;
    Window->Z = ParentWindow->Z + 1;
    Window->TopLevelParent = ParentWindow->TopLevelParent;

    if (ParentLayout->SkipItems)
    {
        Rr_UIPushEmptyLayout(TitleHash, Window);

        return false;
    }
    else
    {
        return Rr_UIPushWindowLayout(Window, TitleHash, Open);
    }
}

bool Rr_UIBeginWindowEx(char const *Title, bool *Open, Rr_UIWindowFlags Flags)
{
    Rr_UILayout *ParentLayout = Rr_UICurrentLayout();
    Rr_UIWindow *ParentWindow = NULL;

    /* TODO: Resize handle is broken unless we force tabs to auto-resize. */
    if (Flags & RR_UI_WINDOW_FLAGS_TABS_BIT)
    {
        Flags |= RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;
    }

    Rr_UIWindow **WindowRef;
    Rr_UIWindow *Window;

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    if (ParentLayout)
    {
        ParentWindow = ParentLayout->Window;
    }
    WindowRef = RR_FIND_IN_HASH_TRIE(
        &gUIContext->WindowMap,
        TitleHash,
        gUIContext->Arena);
    Window = *WindowRef;
    if (Window == NULL)
    {
        Window = Rr_UICreateWindow(TitleLength, Title, TitleHash, Flags);
        *WindowRef = Window;
    }
    else
    {
        Window->Flags = Flags;
    }

    if (Rr_UIWindowNoTitleBar(Window))
    {
        Window->Collapsed = false;
    }

    bool IsDockedChild =
        ParentWindow && !Window->UndockNextFrame && !Window->Undocked;
    if (IsDockedChild)
    {
        return Rr_UIBeginDockedChildWindow(
            Window,
            TitleHash,
            Open,
            ParentLayout,
            ParentWindow);
    }

    if (Window->CreatedThisFrame)
    {
        Window->Z = gUIContext->TotalWindowCount++;
        Window->Rect.Offset =
            Rr_FloorV2(Rr_V2F(gUIContext->DefaultFont->LineHeight));
    }

    Window->Tab = false;
    Window->Child = false;
    Window->TopLevelParent = Window;
    Window->TabsParent = NULL;

    if (Window->UndockNextFrame)
    {
        Window->Rect.Offset = Window->UndockedOffset;
        Window->UndockNextFrame = false;
        Window->Undocked = true;
        Window->Collapsed = false;
        /* Window->SkipThisFrame = true; */
        Window->Flags |= RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT;
        Rr_UIPutWindowOnTop(Window);
        gUIContext->ClickParent = Window;
        Rr_UIResetDrag();
    }

    if (Window->Undocked)
    {
        Open = &Window->Undocked;
    }

    return Rr_UIPushWindowLayout(Window, TitleHash, Open);
}

bool Rr_UIBeginWindow(char const *Title)
{
    return Rr_UIBeginWindowEx(Title, NULL, 0);
}

static inline bool Rr_UIShouldHightlightWindow(Rr_UIWindow *Window)
{
    bool ClickParent = gUIContext->ClickParent;
    if (ClickParent)
    {
        ClickParent = gUIContext->ClickParent->TopLevelParent == Window;
        /* || gUIContext->ClickParent == Window || */
        /*    gUIContext->ClickParent == Window->TabsParent || */
        /*    gUIContext->ClickParent == Window->SelectedTab; */
    }
    return ClickParent;
}

void Rr_UIEndWindow(void)
{
    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    if (Layout->SkipCompletely)
    {
        Rr_UIPopLayout();

        return;
    }

    Rr_UIWindow *Window = Layout->Window;
    Rr_UIFont *Font = Rr_UICurrentFont();

    Rr_UIAssertNoHorizontal(Layout);

    /* Apply deferred layout properties. */

    if (!Layout->WasCollapsed)
    {
        Window->MaxFlexibleWidgetTitleWidth =
            Layout->DeferredMaxFlexibleWidgetTitleWidth;
        Window->MaxFlexibleWidgetWidth = Layout->DeferredMaxFlexibleWidgetWidth;
        Window->MaxRigidWidth = Layout->DeferredMaxRigidWidth;
        Window->ContentsRect = Layout->DeferredContentsRect;
        Window->ContentsRect.Extent.Y += Layout->WindowPadding.Y;
    }
    Window->SelectedTab = Layout->DeferredSelectedTab;

    Rr_Rect TotalClipRect = Layout->Rect;
    if (!Rr_UIWindowNoBorders(Window))
    {
        TotalClipRect =
            Rr_ResizeRect(&Layout->Rect, gUIContext->DoubleBevelThickness);
    }

    /* Since window wasn't collapsed, current clip rect refers to
     * contents area. Begin new clip rect to draw borders, resize handle
     * and scrolloffs. */

    Rr_UIEndClipRect(Layout);
    Rr_UIBeginClipRect(Layout, &TotalClipRect);

    if (!Layout->WasCollapsed)
    {
        /* NOTE: Flooring these fixed imprecise FillRatio calculation.
         * If the bug ever returns it probably means the fix should be applied
         * somewhere else. */

        Window->ContentsRect.Offset = Rr_FloorV2(Window->ContentsRect.Offset);
        Window->ContentsRect.Extent = Rr_FloorV2(Window->ContentsRect.Extent);

        float ContentsHeight = Window->ContentsRect.Extent.Y;

        float FillRatio = 0.0f;
        Rr_Rect CurrentRect = Rr_UIGetWindowContentsArea(Layout, &FillRatio);

        if (FillRatio > 1.0f)
        {
            Rr_Vec4 *ScrolloffBackground =
                &gUIContext->Colors.ScrolloffBackground;

            float DarkenSize = RR_MIN(Font->LineHeight, ContentsHeight);

            Rr_Rect DarkenRect = CurrentRect;
            DarkenRect.Extent.Y = RR_MIN(CurrentRect.Extent.Y, DarkenSize);

            float TopDarkenAlpha =
                RR_CLAMP(0.0f, Window->VScroll / DarkenSize, 1.0f);
            if (TopDarkenAlpha > 0.0f)
            {
                Rr_UIDrawVerticalGradientQuad(
                    &DarkenRect,
                    &(Rr_Vec4){ ScrolloffBackground->X,
                                ScrolloffBackground->Y,
                                ScrolloffBackground->Z,
                                TopDarkenAlpha },
                    &(Rr_Vec4){ ScrolloffBackground->X,
                                ScrolloffBackground->Y,
                                ScrolloffBackground->Z,
                                0.0f });
            }

            float BottomDarkenAlpha = RR_CLAMP(
                0.0f,
                (ContentsHeight - CurrentRect.Extent.Y - Window->VScroll +
                 Layout->WindowPadding.Y) /
                    DarkenSize,
                1.0f);
            if (BottomDarkenAlpha > 0.0f)
            {
                DarkenRect.Offset.Y =
                    CurrentRect.Offset.Y + CurrentRect.Extent.Y - DarkenSize;

                Rr_UIDrawVerticalGradientQuad(
                    &DarkenRect,
                    &(Rr_Vec4){ ScrolloffBackground->X,
                                ScrolloffBackground->Y,
                                ScrolloffBackground->Z,
                                0.0f },
                    &(Rr_Vec4){ ScrolloffBackground->X,
                                ScrolloffBackground->Y,
                                ScrolloffBackground->Z,
                                BottomDarkenAlpha });
            }
        }

        /* Add resize handle if necessary. */

        if (!Rr_UIWindowNoResize(Window))
        {
            Rr_Vec2 BottomRight =
                Rr_AddV2(Layout->Rect.Offset, Layout->Rect.Extent);
            Rr_Vec2 Positions[] = {
                { BottomRight.X - gUIContext->ResizeHandleSize, BottomRight.Y },
                { BottomRight.X, BottomRight.Y - gUIContext->ResizeHandleSize },
                { BottomRight.X, BottomRight.Y },
            };
            if (Layout->DeferredResizeHandleColor)
            {
                Rr_UIDrawTriangleFilled(
                    Positions,
                    Layout->DeferredResizeHandleColor);
            }
        }
    }

    if (!Rr_UIWindowNoBorders(Window))
    {
        Rr_UIDrawDoubleBevel(
            &TotalClipRect,
            Rr_UIShouldHightlightWindow(Window)
                ? &gUIContext->Colors.SelectedOutline
                : &gUIContext->Colors.Outline,
            gUIContext->DoubleBevelThickness);
    }

    /* NOTE: Forward scroll wheel behavior to the top-level parent. */

    if (!Layout->WasCollapsed || Window->Child)
    {
        Rr_UIScrollBehavior(
            Window,
            &Layout->Rect,
            &Window->TopLevelParent->VScrollTarget);
    }

    /* Apply window extent. */

    if (Window->Child || Layout->DeferredAutoResize)
    {
        Rr_Vec2 Extent = { 0 };

        Extent.Y = Window->ContentsRect.Extent.Y + Layout->WindowPadding.Y;

        /* NOTE: Select between widths occupied by rigid widgets such as
         * buttons and flexible widgets such as input fields. */

        Rr_Vec2 TitleSize = Rr_UICalculateTitleBarSize(Layout);

        Extent.X = RR_MAX(
            Layout->DeferredMaxFlexibleWidgetTitleWidth +
                gUIContext->FlexibleTitleMargin +
                Layout->DeferredMaxFlexibleWidgetWidth,
            Layout->DeferredMaxRigidWidth);
        Extent.X += Layout->WindowPadding.X * 2.0f;
        Extent.X = RR_MAX(Extent.X, TitleSize.X);

        if (!Rr_UIWindowNoTitleBar(Window))
        {
            Extent.Y += gUIContext->TitleBarHeight;
        }

        if (!Rr_UIWindowNoBorders(Window))
        {
            Extent = Rr_AddV2F(Extent, gUIContext->DoubleBevelThickness * 2.0f);
        }

        Rr_UISetWindowExtentChecked(Layout, Extent);
    }

    Layout->VisibleRect = Window->Child ? Rr_UIRectIntersection(
                                              &TotalClipRect,
                                              &Layout->ParentClipRect->Rect)
                                        : TotalClipRect;

    /* Apply window offset.
     * NOTE: Forward drag-to-move behavior to the top-level parent.
     * NOTE: Rr_UIClickDrag() gets called even if the window is
     * non-movable because this function resets widget focus. */

    Rr_UIHash MoveHash =
        Rr_UIGetHash(sizeof("Rr.Move"), "Rr.Move", Rr_UICurrentHash());
    Rr_UIClickResult ClickResult = Rr_UIClickEx(
        Layout,
        &TotalClipRect,
        RR_UI_CLICK_TYPE_DRAG_RELAXED,
        MoveHash,
        (Rr_Rect){ .Offset = Window->TopLevelParent->Rect.Offset });
    if (!Layout->LockOffset)
    {
        if (!Rr_UIWindowNoMove(Window->TopLevelParent) && ClickResult.Moved)
        {
            Rr_Vec2 Delta =
                Rr_SubV2(gUIContext->MousePosition, gUIContext->DragMouseStart);
            Window->TopLevelParent->Rect.Offset =
                Rr_FloorV2(Rr_AddV2(gUIContext->DragValueStart.Offset, Delta));
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

    /* Handle keyboard events. */

    bool EscapeCloses = Rr_UIWindowEscapeCloses(Window);
    if (EscapeCloses && gUIContext->KeyboardInputEvents.Count)
    {
        assert(!Window->Child);
        for (size_t Index = 0; Index < gUIContext->KeyboardInputEvents.Count;
             ++Index)
        {
            Rr_KeyEvent *Event = gUIContext->KeyboardInputEvents.Data + Index;
            if (Event->Down && Event->Keymod == 0 &&
                Event->Scancode == RR_SCANCODE_ESCAPE)
            {
                if (Window == &gUIContext->PopupWindow)
                {
                    Rr_UIClosePopupWindow();
                }
                else if (Layout->Open != NULL)
                {
                    *Layout->Open = false;
                }
                /* Window->Open = false; */
                RR_ZERO_PTR(Event);
                break;
            }
        }
    }

    Rr_UIEndClipRect(Layout);

    /* Pop whatever was pushed in Rr_UIBeginWindowEx(). */

    Rr_UIPopLayout();

    Rr_UILayout *ParentLayout = Rr_UICurrentLayout();
    if (ParentLayout && !ParentLayout->SkipCompletely)
    {
        /* Rr_UIWindow *ParentWindow = ParentLayout->Window; */

        if (Window->Child)
        {
            Rr_Vec2 ExtentThisFrame = Window->Rect.Extent;
            if (Layout->WasCollapsed)
            {
                ExtentThisFrame.Y = gUIContext->TitleBarHeight;

                if (!Rr_UIWindowNoBorders(Window))
                {
                    ExtentThisFrame.Y +=
                        gUIContext->DoubleBevelThickness * 2.0f;
                }
            }
            Rr_UIAdvance(ExtentThisFrame, Rr_V2F(0.0f));
        }

        /* Resume clip rect. */

        Rr_UIBeginClipRect(ParentLayout, &ParentLayout->CurrentClipRect->Rect);
    }
}

void Rr_UIBeginHorizontal(void)
{
    if (Rr_UISkipItems())
    {
        return;
    }

    assert(
        !Rr_UIIsHorizontal() && "Did you forget to call Rr_EndHorizontal()?");
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Layout->HorizontalMaxExtent = Rr_V2F(0.0f);
    Layout->HorizontalX = Layout->Cursor.X;
}

void Rr_UIEndHorizontal(void)
{
    if (Rr_UISkipItems())
    {
        return;
    }

    assert(
        Rr_UIIsHorizontal() && "Did you forget to call Rr_BeginHorizontal()?");
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Layout->Cursor.X = Layout->HorizontalX;
    Layout->HorizontalX = INFINITY;
    Rr_UIAdvance(Layout->HorizontalMaxExtent, Rr_V2F(0.0f));
}

static inline float Rr_UIGetOffsetForTreeDepth(int32_t Depth)
{
    return gUIContext->TopLevelTreeOffset +
           (float)(Depth - 1) * gUIContext->TreeOffset;
}

static inline float Rr_UISetupFlexibleWidget(
    Rr_UILayout *Layout,
    size_t TitleLength,
    char const *Title,
    float DesiredWidgetWidth)
{
    float TitleWidth = Rr_UICalculateTextSize(TitleLength, Title, 0.0f).X;
    Layout->DeferredMaxFlexibleWidgetTitleWidth =
        RR_MAX(Layout->DeferredMaxFlexibleWidgetTitleWidth, TitleWidth);

    Layout->DeferredMaxFlexibleWidgetWidth =
        RR_MAX(Layout->DeferredMaxFlexibleWidgetWidth, DesiredWidgetWidth);

    Rr_UIWindow *Window = Layout->Window;

    if (Layout->DeferredAutoResize && !Layout->LockExtentX)
    {
        DesiredWidgetWidth = RR_MAX(
            Window->MaxRigidWidth - Window->MaxFlexibleWidgetTitleWidth -
                gUIContext->FlexibleTitleMargin,
            RR_MAX(DesiredWidgetWidth, Window->MaxFlexibleWidgetWidth));
    }
    else
    {
        DesiredWidgetWidth = Rr_UIGetAvailableContentsWidth(Layout) -
                             Window->MaxFlexibleWidgetTitleWidth -
                             gUIContext->FlexibleTitleMargin;
    }

    return DesiredWidgetWidth;
}

void Rr_UISetNextTreeExpanded(void)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Layout->TreeExpandCollapseDepth = INT32_MAX - 1;
}

void Rr_UISetNextTreeCollapsed(void)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Layout->TreeExpandCollapseDepth = INT32_MIN + 1;
}

bool Rr_UIBeginTree(char const *Title)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    Rr_UIAssertWindow();
    assert(
        Rr_UIIsHorizontal() == false && "Trees can't be aligned horizontally!");

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;
    Rr_UIFont *Font = Rr_UICurrentFont();

    int32_t CurrentDepth = Layout->TreeDepth;
    bool TopLevel = CurrentDepth == 0;

    Rr_UIPrimitive BevelPrimitive;
    if (TopLevel)
    {
        BevelPrimitive = Rr_UIReserveBevel();
    }

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    bool *Expanded =
        &Rr_UIGetStorage(Window, TitleHash, RR_UI_STORAGE_TYPE_TREE)
             ->Union.TreeExpanded;
    bool WasExpanded = *Expanded;

    float TreeButtonHeight =
        TopLevel ? gUIContext->TitleBarHeight : Font->LineHeight;

    float TreeOffset =
        TopLevel ? gUIContext->TopLevelTreeOffset : gUIContext->TreeOffset;

    float TriangleSize = gUIContext->TitleBarHeight * 0.3f;
    float TriangleOffset = TopLevel ? gUIContext->ButtonPadding.X : 0.0f;
    Rr_Vec2 TriangleCenter = Rr_V2(
        Layout->Cursor.X + TriangleOffset + TriangleSize,
        Layout->Cursor.Y + TreeButtonHeight * 0.5f);

    Rr_UIDrawFitTriangleFilled(
        TriangleCenter,
        TriangleSize,
        WasExpanded ? RR_ANGLE_DEG(90.0f) : 0.0f,
        &gUIContext->Colors.Foreground);

    if (!TopLevel)
    {
        Rr_UITree *ParentTree = &RR_LAST_ARRAY_ELEMENT(&Layout->TreeStack);

        Rr_Vec2 RectOffset = ParentTree->ParentPoint;
        RectOffset.X -= gUIContext->FrameThickness;
        if (CurrentDepth == 1)
        {
            RectOffset.Y += gUIContext->TitleBarHeight * 0.75f;
        }
        else
        {
            RectOffset.Y += TriangleSize * 1.25f;
        }
        Rr_Vec2 RectExtent = { gUIContext->FrameThickness,
                               TriangleCenter.Y - ParentTree->ParentPoint.Y };
        if (CurrentDepth == 1)
        {
            RectExtent.Y -= gUIContext->TitleBarHeight * 0.75f;
        }
        else
        {
            RectExtent.Y -= TriangleSize;
        }
        Rr_UIDrawSolidQuad(
            &(Rr_Rect){ RectOffset, RectExtent },
            &gUIContext->Colors.ForegroundDimmed);

        RectOffset = ParentTree->ParentPoint;
        RectOffset.X -= gUIContext->FrameThickness;
        RectOffset.Y += TriangleCenter.Y - ParentTree->ParentPoint.Y;
        RectExtent = (Rr_Vec2){ TriangleCenter.X - ParentTree->ParentPoint.X,
                                gUIContext->FrameThickness };
        RectExtent.X -= TriangleSize;

        Rr_UIDrawSolidQuad(
            &(Rr_Rect){ RectOffset, RectExtent },
            &gUIContext->Colors.ForegroundDimmed);
    }

    Rr_Vec2 TitlePosition =
        Rr_V2(Layout->Cursor.X + TreeOffset, Layout->Cursor.Y);
    if (TopLevel)
    {
        TitlePosition.Y += gUIContext->TitleBarPadding.Y;
    }
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        false,
        TitlePosition,
        TitleLength,
        Title,
        0,
        &gUIContext->Colors.Foreground);

    Rr_Vec2 TotalExtent;
    TotalExtent.X = TitleSize.X + TreeOffset;
    TotalExtent.Y = TreeButtonHeight;

    Rr_Rect ButtonRect = {
        Layout->Cursor,
        TotalExtent,
    };

    if (TopLevel)
    {
        ButtonRect.Extent.X = Layout->TotalAvailableContentsWidth;
    }

    Rr_UIClickResult ClickResult =
        Rr_UIClickSimple(Layout, &ButtonRect, TitleHash);

    if (ClickResult.ClickCount)
    {
        *Expanded = !*Expanded;
    }

    if (TopLevel)
    {
        ButtonRect.Extent.X = Layout->TotalAvailableContentsWidth;
        Rr_UIBevel(
            BevelPrimitive,
            &ButtonRect,
            ClickResult.Held ? &gUIContext->Colors.ButtonHeld
                             : &gUIContext->Colors.ButtonNormal,
            ClickResult.Held);
    }

    TotalExtent.X += gUIContext->ButtonPadding.X;

    Rr_UIAdvance(TotalExtent, Rr_V2F(0.0f));

    if (Layout->TreeExpandCollapseDepth > 0 &&
        Layout->TreeDepth < Layout->TreeExpandCollapseDepth)
    {
        *Expanded = true;
    }
    else if (
        Layout->TreeExpandCollapseDepth < 0 &&
        Layout->TreeDepth < -Layout->TreeExpandCollapseDepth)
    {
        *Expanded = false;
    }

    if (WasExpanded || *Expanded)
    {
        Layout->TreeDepth++;
        Layout->Cursor.X = Layout->DeferredContentsRect.Offset.X +
                           Rr_UIGetOffsetForTreeDepth(Layout->TreeDepth);

        Rr_UIPushIDHash(TitleHash);

        *RR_PUSH_INTO_ARRAY(&Layout->TreeStack, gUIContext->FrameArena) =
            (Rr_UITree){
                .ParentPoint = TriangleCenter,
            };
    }

    return WasExpanded || *Expanded;
}

void Rr_UIEndTree(void)
{
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    assert(Layout->TreeDepth > 0 && "Did you forget to call Rr_BeginTree()?");

    RR_UNUSED(RR_POP_FROM_ARRAY(&Layout->TreeStack));

    Layout->TreeDepth--;
    Layout->Cursor.X = Layout->DeferredContentsRect.Offset.X +
                       Rr_UIGetOffsetForTreeDepth(Layout->TreeDepth);

    if (Layout->TreeDepth == 0)
    {
        Layout->TreeExpandCollapseDepth = 0;
        Layout->Cursor.X = Layout->DeferredContentsRect.Offset.X;
    }

    Rr_UIPopID();
}

static inline bool Rr_UIApplyWidgetExtent(Rr_Vec2 *OutExtent)
{
    if (gUIContext->WidgetExtentStack.Count)
    {
        *OutExtent = RR_LAST_ARRAY_ELEMENT(&gUIContext->WidgetExtentStack);

        return true;
    }

    return false;
}

void Rr_UIPushWidgetExtent(Rr_Vec2 Extent)
{
    *RR_PUSH_INTO_ARRAY(
        &gUIContext->WidgetExtentStack,
        gUIContext->FrameArena) = Extent;
}

void Rr_UIPopWidgetExtent(void)
{
    RR_UNUSED(RR_POP_FROM_ARRAY(&gUIContext->WidgetExtentStack));
}

void Rr_UISeparator(void)
{
    if (Rr_UISkipItems())
    {
        return;
    }

    Rr_UIAssertWindow();
    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIAssertNoHorizontal(Layout);
    Rr_UIWindow *Window = Layout->Window;

    float AvailableWidth = Rr_UIGetAvailableContentsWidth(Layout);

    Rr_Rect Rect;
    Rect.Extent = (Rr_Vec2){
        AvailableWidth,
        gUIContext->DoubleBevelThickness * 0.5f,
    };
    Rect.Offset = (Rr_Vec2){
        Layout->Cursor.X + AvailableWidth * 0.5f - Rect.Extent.X * 0.5f,
        Layout->Cursor.Y + gUIContext->SeparatorLineHeight * 0.5f -
            Rect.Extent.Y,
    };
    Rr_Vec4 *Color = Rr_UIShouldHightlightWindow(Window)
                         ? &gUIContext->Colors.SelectedOutline
                         : &gUIContext->Colors.Outline;
    Rr_Vec4 ColorLight;
    ColorLight.RGB = Rr_LerpV3(
        Color->RGB,
        gUIContext->Style.BevelIntensityLight,
        RR_UI_VEC3_ONE);
    ColorLight.A = 1.0f;
    Rr_Vec4 ColorDark;
    ColorDark.RGB = Rr_LerpV3(
        Color->RGB,
        gUIContext->Style.BevelIntensityDark,
        RR_UI_VEC3_ZERO);
    ColorDark.A = 1.0f;
    Rr_UIDrawSolidQuad(&Rect, &ColorLight);
    Rect.Offset.Y += Rect.Extent.Y;
    Rr_UIDrawSolidQuad(&Rect, &ColorDark);

    Rr_UIAdvance(
        Rr_V2(gUIContext->SeparatorLineHeight, gUIContext->SeparatorLineHeight),
        Rr_V2(AvailableWidth, gUIContext->SeparatorLineHeight));
}

void Rr_UIImageEx(Rr_Image *Image, Rr_Vec2 Extent, Rr_Vec2 UVMin, Rr_Vec2 UVMax)
{
    if (Rr_UISkipItems())
    {
        return;
    }

    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    Rr_Rect CurrentRect = Layout->CurrentClipRect->Rect;
    Rr_UIClipRect *ClipRect = Rr_UIPushSubClipRect(Layout, &CurrentRect);
    ClipRect->Image = Image;

    Rr_Vec2 Cursor = Layout->Cursor;

    Rr_UIVertex Vertices[4] = {
        {
            .Position = Cursor,
            .UV = UVMin,
            .Color = RR_UI_VEC4_ONE,
        },
        {
            .Position = { Cursor.X + Extent.X, Cursor.Y },
            .UV = Rr_V2(UVMax.X, UVMin.Y),
            .Color = RR_UI_VEC4_ONE,
        },
        {
            .Position = Rr_AddV2(Cursor, Extent),
            .UV = UVMax,
            .Color = RR_UI_VEC4_ONE,
        },
        {
            .Position = { Cursor.X, Cursor.Y + Extent.Y },
            .UV = Rr_V2(UVMin.X, UVMax.Y),
            .Color = RR_UI_VEC4_ONE,
        },
    };

    Rr_UIDrawQuadVertices(Vertices);

    Rr_UIPopSubClipRect(Layout);

    Rr_UIAdvance(Extent, Rr_V2F(0.0f));
}

void Rr_UIText(char const *Text)
{
    if (Rr_UISkipItems())
    {
        return;
    }

    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    Rr_Vec2 TextSize = Rr_UIDrawText(
        false,
        Layout->Cursor,
        SIZE_MAX,
        Text,
        0.0f,
        &gUIContext->Colors.Foreground);

    Rr_UIAdvance(TextSize, Rr_V2F(0.0f));
}

void
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    Rr_UITextF(char const *Format, ...)
{
    if (Rr_UISkipItems())
    {
        return;
    }

    int BufferSize;
    va_list Args;

    va_start(Args, Format);
    BufferSize = vsnprintf(NULL, 0, Format, Args);
    va_end(Args);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    char *Buffer = Rr_AllocNoZero((size_t)BufferSize + 1, Scratch.Arena);

    va_start(Args, Format);
    BufferSize = vsnprintf(Buffer, (size_t)BufferSize + 1, Format, Args);
    va_end(Args);

    Rr_UIText(Buffer);

    Rr_DestroyScratch(Scratch);
}

void Rr_UITextWrapped(char const *Text, float WrapWidth)
{
    if (Rr_UISkipItems())
    {
        return;
    }

    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    Rr_Vec2 TextSize = Rr_UIDrawText(
        false,
        Layout->Cursor,
        SIZE_MAX,
        Text,
        WrapWidth,
        &gUIContext->Colors.Foreground);

    Rr_UIAdvance(TextSize, Rr_V2F(0.0f));
}

void Rr_UILabelText(char const *Title, char const *Text)
{
    if (Rr_UISkipItems())
    {
        return;
    }

    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    size_t TitleLength = strlen(Title);

    float RigidWidth = Rr_UICalculateTextSize(SIZE_MAX, Text, 0.0f).X;

    float FlexibleWidth =
        Rr_UISetupFlexibleWidget(Layout, TitleLength, Title, RigidWidth);

    Rr_Vec2 TextSize = Rr_UIDrawText(
        false,
        Layout->Cursor,
        SIZE_MAX,
        Text,
        0.0f,
        &gUIContext->Colors.Foreground);

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += FlexibleWidth + gUIContext->FlexibleTitleMargin;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0,
        &gUIContext->Colors.Foreground);

    Rr_Vec2 RigidExtent = {
        RigidWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        TextSize.Y,
    };

    Rr_Vec2 FlexibleExtent = {
        FlexibleWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        TextSize.Y,
    };

    Rr_UIAdvance(RigidExtent, FlexibleExtent);
}

bool Rr_UIButton(char const *Text)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    Rr_UIAssertWindow();

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Text, &TitleLength);

    Rr_Vec2 TitleSize = Rr_UICalculateTextSize(TitleLength, Text, 0.0f);

    Rr_Vec2 ButtonSize;
    if (!Rr_UIApplyWidgetExtent(&ButtonSize))
    {
        ButtonSize =
            Rr_AddV2(TitleSize, Rr_MulV2F(gUIContext->ButtonPadding, 2.0f));
    }

    Rr_Vec2 ButtonPosition = Layout->Cursor;
    Rr_UIPrimitive Primitive = Rr_UIReserveBevel();

    Rr_Vec2 TitlePosition = Rr_AddV2(
        ButtonPosition,
        Rr_SubV2(Rr_MulV2F(ButtonSize, 0.5f), Rr_MulV2F(TitleSize, 0.5f)));
    Rr_UIDrawText(
        false,
        TitlePosition,
        TitleLength,
        Text,
        0.0f,
        &gUIContext->Colors.Foreground);

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
        ClickResult.Held ? &gUIContext->Colors.ButtonHeld
                         : &gUIContext->Colors.ButtonNormal,
        ClickResult.Held);

    Rr_UIAdvance(ButtonSize, Rr_V2F(0.0f));

    return ClickResult.ClickCount;
}

bool Rr_UIRadioButton(
    char const *Title,
    int32_t *SelectedOption,
    int32_t ThisOption)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    Rr_UIAssertWindow();
    assert(SelectedOption != NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIFont *Font = Rr_UICurrentFont();

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    float ButtonSize = Font->LineHeight;
    float OuterRadius = ButtonSize * 0.4f;
    float InnerRadius = OuterRadius * 0.6f;
    float OutlineThickness = Font->LineHeight / 24.0f * 0.5f;

    Rr_Vec2 Cursor = Layout->Cursor;

    Rr_Vec2 TitlePosition = Cursor;
    TitlePosition.X += OuterRadius * 2.0f + gUIContext->ButtonPadding.X;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        false,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground);

    Rr_Rect ButtonRect = {
        Cursor,
        Rr_V2(
            TitleSize.X + gUIContext->ButtonPadding.X +
                gUIContext->ButtonPadding.X + OuterRadius * 2.0f,
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

    Rr_Vec4 BaseColor = ClickResult.Held ? gUIContext->Colors.RadioButtonHeld
                                         : gUIContext->Colors.RadioButtonNormal;

    Rr_UIDrawCircleFilled(CircleOffset, OuterRadius, &BaseColor);
    if (Selected)
    {
        Rr_UIDrawCircleFilled(
            CircleOffset,
            InnerRadius,
            &gUIContext->Colors.Foreground);
    }

    Rr_UIDrawCircle(
        CircleOffset,
        OuterRadius - OutlineThickness,
        OutlineThickness,
        &gUIContext->Colors.RadioButtonOutline);

    Rr_UIAdvance(ButtonRect.Extent, Rr_V2F(0.0f));

    return ClickResult.ClickCount;
}

bool Rr_UICheckbox(char const *Title, bool *Checked)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    Rr_UIAssertWindow();
    assert(Checked != NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIFont *Font = Rr_UICurrentFont();

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_Rect CheckboxRect = {
        Layout->Cursor,
        { Font->LineHeight, Font->LineHeight },
    };

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += CheckboxRect.Extent.X + gUIContext->ButtonPadding.X;
    Rr_Vec2 TitleSize = Rr_UIDrawText(
        false,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground);

    Rr_Rect ButtonRect = {
        Layout->Cursor,
        Rr_V2(
            TitleSize.X + gUIContext->ButtonPadding.X * 2.0f + Font->LineHeight,
            TitleSize.Y),
    };

    Rr_UIClickResult ClickResult =
        Rr_UIClickSimple(Layout, &ButtonRect, TitleHash);

    if (ClickResult.ClickCount)
    {
        *Checked = !*Checked;
    }

    Rr_UIDrawBevel(
        &CheckboxRect,
        ClickResult.Held ? &gUIContext->Colors.ButtonHeld
                         : &gUIContext->Colors.ButtonNormal,
        ClickResult.Held);

    if (*Checked)
    {
        CheckboxRect = Rr_ResizeRect(
            &CheckboxRect,
            -CheckboxRect.Extent.X * (1.0f - gUIContext->Style.CheckmarkSize));
        Rr_UIDrawCheckmark(
            CheckboxRect.Offset,
            CheckboxRect.Extent.X,
            &gUIContext->Colors.Foreground);
    }

    Rr_UIAdvance(ButtonRect.Extent, Rr_V2F(0.0f));

    return ClickResult.ClickCount;
}

static inline bool Rr_UIConsumeTextInput(
    size_t UTF8StringLength,
    char const *UTF8String,
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

static inline size_t Rr_UIThisLine(char *Buffer, size_t Cursor)
{
    if (Cursor == 0)
    {
        return 0;
    }
    if (Buffer[Cursor - 1] == '\n')
    {
        return Cursor;
    }
    while (true)
    {
        Cursor--;
        if (Cursor == 0)
        {
            return 0;
        }
        if (Buffer[Cursor - 1] == '\n')
        {
            return Cursor;
        }
    }
    assert(false);
}

static inline size_t Rr_UIPreviousLine(char *Buffer, size_t Cursor)
{
    Cursor = Rr_UIThisLine(Buffer, Cursor);
    if (Cursor == 0)
    {
        return 0;
    }
    Cursor--;
    if (Buffer[Cursor - 1] == '\n')
    {
        return Cursor;
    }

    return Rr_UIThisLine(Buffer, Cursor);
}

static inline size_t Rr_UINextLine(char *Buffer, size_t Cursor)
{
    while (true)
    {
        if (Buffer[Cursor] == '\0')
        {
            return Cursor;
        }
        if (Buffer[Cursor] == '\n')
        {
            return Cursor + 1;
        }
        Cursor++;
    }
    assert(false);
}

static inline size_t Rr_UILineStart(char const *Buffer, size_t Cursor)
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

static inline size_t Rr_UILineEnd(char const *Buffer, size_t Cursor)
{
    return Rr_NextUTF8LFOffset(Buffer, Cursor);
}

static inline void Rr_UISetTextInputMaxCol(char *Buffer, size_t Cursor)
{
    size_t ThisLine = Rr_UIThisLine(Buffer, Cursor);
    size_t CursorMaxCol = Cursor - ThisLine;
    if (CursorMaxCol == 0)
    {
        gUIContext->TextInputCursorCodepointMaxCol = 0;

        return;
    }
    Rr_UTF8Decoder Decoder = {
        .CString = Buffer,
        .CStringParserIndex = ThisLine,
    };
    while (true)
    {
        Rr_UTF8Decode(&Decoder);
        if (Decoder.CStringCodepointIndex == Cursor)
        {
            gUIContext->TextInputCursorCodepointMaxCol =
                Decoder.CodepointCount - 1;

            return;
        }
    }
    assert(false && "Couldn't find codepoint at given cursor!");
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

#ifdef __APPLE__
    const Rr_KeymodFlags DEFAULT_MOD = RR_KEYMOD_SUPER;
#else
    const Rr_KeymodFlags DEFAULT_MOD = RR_KEYMOD_CTRL;
#endif

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

        if (Event->Scancode == RR_SCANCODE_A && Event->Keymod & DEFAULT_MOD)
        {
            NewCursorBegin = 0;
            NewCursorEnd = BufferLength;
            Edited = true;
            ResetCol = true;
        }

        if (Event->Scancode == RR_SCANCODE_C && Event->Keymod & DEFAULT_MOD)
        {
            Rr_Scratch Scratch = Rr_GetScratch(NULL);

            char *ClipboardBuffer = NULL;
            size_t ClipboardLength = 0;
            if (CursorMin != CursorMax)
            {
                ClipboardLength = CursorMax - CursorMin;
                ClipboardBuffer =
                    Rr_AllocNoZero(ClipboardLength, Scratch.Arena);
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
                        Rr_AllocNoZero(ClipboardLength, Scratch.Arena);
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
        if (Event->Scancode == RR_SCANCODE_V && Event->Keymod & DEFAULT_MOD)
        {
            Rr_Scratch Scratch = Rr_GetScratch(NULL);

            char const *ClipboardBuffer = Rr_GetClipboardText(Scratch.Arena);
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

            Rr_DestroyScratch(Scratch);
        }
        if (Event->Scancode == RR_SCANCODE_X && Event->Keymod & DEFAULT_MOD)
        {
            Rr_Scratch Scratch = Rr_GetScratch(NULL);

            char *ClipboardBuffer = NULL;
            size_t ClipboardLength = 0;
            if (CursorMin != CursorMax)
            {
                ClipboardLength = CursorMax - CursorMin;
                ClipboardBuffer =
                    Rr_AllocNoZero(ClipboardLength, Scratch.Arena);
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
                        Rr_AllocNoZero(ClipboardLength, Scratch.Arena);
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
            bool NoAlt = (Event->Keymod & RR_KEYMOD_ALT) == 0;
            if (EnterToConfirm && Event->Keymod == 0)
            {
                Result.Confirmed |= true;
            }
            else if (NoAlt)
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
                size_t PreviousLine = Rr_UIPreviousLine(Buffer, NewCursorEnd);
                size_t ThisLine = Rr_UIThisLine(Buffer, NewCursorEnd);
                if (PreviousLine == ThisLine)
                {
                    NewCursorEnd = 0;
                }
                else
                {
                    Rr_UTF8Decoder Decoder = {
                        .CString = Buffer,
                        .CStringParserIndex = PreviousLine,
                    };
                    while (true)
                    {
                        if (Decoder.CodepointCount ==
                            gUIContext->TextInputCursorCodepointMaxCol)
                        {
                            NewCursorEnd = Decoder.CStringParserIndex;
                            break;
                        }
                        Rr_UTF8Decode(&Decoder);
                        if (Decoder.Codepoint == '\n')
                        {
                            NewCursorEnd = Decoder.CStringCodepointIndex;
                            break;
                        }
                    }
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
                size_t NextLine = Rr_UINextLine(Buffer, NewCursorEnd);
                Rr_UTF8Decoder Decoder = { .CString = Buffer,
                                           .CStringParserIndex = NextLine };
                while (true)
                {
                    if (Decoder.CodepointCount ==
                        gUIContext->TextInputCursorCodepointMaxCol)
                    {
                        NewCursorEnd = Decoder.CStringParserIndex;
                        break;
                    }
                    Rr_UTF8Decode(&Decoder);
                    if (Decoder.Codepoint == '\n' || Decoder.Codepoint == '\0')
                    {
                        NewCursorEnd = Decoder.CStringCodepointIndex;
                        break;
                    }
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
                if ((Event->Keymod & DEFAULT_MOD) == 0)
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
                if ((Event->Keymod & DEFAULT_MOD) == 0)
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
            if (Event->Keymod & DEFAULT_MOD)
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
            if (Event->Keymod & DEFAULT_MOD)
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
                if (CursorMin == 0 && CursorMax == 0)
                {
                }
                else if (CursorMin == 0 && CursorMax == BufferLength)
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
                ResetCol = true;
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
            Rr_UISetTextInputMaxCol(Buffer, NewCursorEnd);
        }
    }

    for (size_t Index = 0; Index < gUIContext->TextInputEvents.Count; ++Index)
    {
        NewCursorBegin = *CursorBegin;
        NewCursorEnd = *CursorEnd;

        /* CursorMin = RR_MIN(NewCursorBegin, NewCursorEnd); */
        /* CursorMax = RR_MAX(NewCursorBegin, NewCursorEnd); */

        char const *CString = gUIContext->TextInputEvents.Data[Index];
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
            Rr_UISetTextInputMaxCol(Buffer, NewCursorEnd);
            Result.Edited |= true;
        }
    }

    RR_CLEAR_ARRAY(&gUIContext->TextInputEvents);
    RR_CLEAR_ARRAY(&gUIContext->KeyboardInputEvents);

    return Result;
}

static inline void Rr_UIApplyInputFieldPlaceholder(
    Rr_Vec2 Offset,
    float FixedWidth,
    bool Focused,
    char const *PlaceholderString,
    bool AutoCenter,
    Rr_Vec2 *BufferPosition,
    Rr_Vec2 *BufferSize)
{
    if (BufferSize->X > -0.0f)
    {
        return;
    }
    if (PlaceholderString != NULL && !Focused)
    {
        if (AutoCenter)
        {
            *BufferSize =
                Rr_UICalculateTextSize(SIZE_MAX, PlaceholderString, 0.0f);
            BufferPosition->X =
                Offset.X + FixedWidth * 0.5f - BufferSize->X * 0.5f;
        }
        *BufferSize = Rr_UIDrawText(
            false,
            *BufferPosition,
            SIZE_MAX,
            PlaceholderString,
            0.0f,
            &gUIContext->Colors.ForegroundDimmed);
    }
    else
    {
        Rr_UIFont *Font = Rr_UICurrentFont();
        const float MIN_FIELD_WIDTH = Font->LineHeight / 2.0f;
        if (BufferSize->Width < MIN_FIELD_WIDTH)
        {
            BufferSize->Width = MIN_FIELD_WIDTH;
        }
    }
}

typedef struct Rr_UIInputFieldResult Rr_UIInputFieldResult;
struct Rr_UIInputFieldResult
{
    Rr_Vec2 Extent;
    bool Edited;
    bool BeganDragging;
    float Dragged;
};

/* NOTE: Generic input field is a building block for other widgets. It doesn't
 * alter the layout on its own. */
static inline Rr_UIInputFieldResult Rr_UIGenericInputField(
    Rr_UIHash Hash,
    Rr_Vec2 Offset,
    size_t BufferCapacity,
    char *Buffer,
    char const *PlaceholderString,
    Rr_UIInputFieldFilterFunc FilterFunc,
    Rr_UIInputFieldFlags Flags,
    float FixedWidth)
{
    if (FixedWidth < 0.0f)
    {
        return (Rr_UIInputFieldResult){
            .Extent = { 0.0f,
                        Rr_UICurrentLineHeight() +
                            gUIContext->InputFieldPadding.Y * 2.0f }
        };
    }

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;

    bool DrawBackground = !(Flags & RR_UI_INPUT_FIELD_FLAGS_NO_BACKGROUND_BIT);
    Rr_UIPrimitive BackgroundBevelPrimitive;
    if (DrawBackground)
    {
        BackgroundBevelPrimitive = Rr_UIReserveBevel();
    }

    bool AutoSelect = Flags & RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT;
    bool Drag = Flags & RR_UI_INPUT_FIELD_FLAGS_DRAG_BIT;

    bool UseFixedWidth = FixedWidth != 0.0f;
    bool AutoCenter =
        (Flags & RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT) && UseFixedWidth;

    bool UsePersistentBuffer =
        Flags & RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT;

    bool Focused = Rr_UIIsFocused(Window, Hash);
    bool WasFocused = !Focused && Rr_UIWasFocused(Window, Hash);

    bool BeganDragging = false;
    float Dragged = 0.0f;
    Rr_Vec2 BufferExtent;
    Rr_Vec2 BufferOffset;
    Rr_Rect FieldRect;
    Rr_UIEditResult EditResult = { 0 };

    if (Drag && !Focused)
    {
        size_t BufferLength = strlen(Buffer);
        BufferOffset = Rr_AddV2(Offset, gUIContext->InputFieldPadding);
        BufferExtent = Rr_UICalculateTextSize(BufferLength, Buffer, 0.0f);

        FieldRect = (Rr_Rect){
            Offset,
            Rr_AddV2(
                BufferExtent,
                Rr_MulV2F(gUIContext->InputFieldPadding, 2.0f)),
        };
        if (UseFixedWidth)
        {
            FieldRect.Extent.X = FixedWidth;
        }

        Rr_Rect ClipRect =
            Rr_ResizeRect(&FieldRect, gUIContext->BevelThickness * -2.0f);
        Rr_UIPushSubClipRect(Layout, &ClipRect);

        if (AutoCenter)
        {
            BufferOffset.X =
                Offset.X + FixedWidth * 0.5f - BufferExtent.X * 0.5f;
        }
        Rr_UIDrawText(
            false,
            BufferOffset,
            BufferLength,
            Buffer,
            0.0f,
            &gUIContext->Colors.Foreground);

        Rr_UIApplyInputFieldPlaceholder(
            Offset,
            FixedWidth,
            Focused,
            PlaceholderString,
            AutoCenter,
            &BufferOffset,
            &BufferExtent);

        Rr_UIClickResult ClickResult = Rr_UIClickEx(
            Layout,
            &FieldRect,
            RR_UI_CLICK_TYPE_DRAG_AND_RELEASE,
            Hash,
            (Rr_Rect){ 0 });

        BeganDragging = ClickResult.MovedFirstTime;

        if (ClickResult.Moved)
        {
            Dragged =
                gUIContext->MousePosition.X - gUIContext->DragMouseStart.X;
        }

        if (ClickResult.ClickCount == 1)
        {
            Rr_UISetFocus(Window, Hash);

            if (AutoSelect && !Focused && !WasFocused)
            {
                /* Select everything on first click. */

                gUIContext->TextInputCursorBegin = 0;
                gUIContext->TextInputCursorEnd = BufferLength;
                gUIContext->TextInputClickID =
                    gUIContext->LeftMouseButton.ClickID;
            }

            if (UsePersistentBuffer && !Focused)
            {
                gUIContext->DeferTextInputBufferCopy = true;
            }

            Rr_UISetTextInputMaxCol(Buffer, gUIContext->TextInputCursorEnd);
            gUIContext->TextInputCursorBlinkTime = Rr_GetTimeMS();
        }

        Focused = ClickResult.ClickCount || ClickResult.Held;

        bool ChangeCursor =
            ClickResult.Hovered &&
            (!gUIContext->DragParent || (gUIContext->DragHash == Hash));
        ChangeCursor |= ClickResult.Held;
        if (ChangeCursor)
        {
            gUIContext->CursorType = RR_CURSOR_TYPE_RESIZE_EW;
        }
    }
    else
    {
        /* NOTE: A bit hacky way to make sure initial memcpy to persistent
         * buffer occurs only once. Defering the copy also protects from issues
         * coming from unfocusing an input field that goes after current one. */
        if (UsePersistentBuffer && Focused &&
            gUIContext->DeferTextInputBufferCopy)
        {
            /* NOTE: May waste quite a bit of memory. */
            if (gUIContext->TextInputBuffer.Capacity < BufferCapacity ||
                !gUIContext->TextInputBuffer.Data)
            {
                gUIContext->TextInputBuffer.Data =
                    Rr_AllocNoZero(BufferCapacity, gUIContext->Arena);
                gUIContext->TextInputBuffer.Capacity = BufferCapacity;
            }
            /* NOTE: It was BufferLength + 1 before. */
            memcpy(gUIContext->TextInputBuffer.Data, Buffer, BufferCapacity);

            gUIContext->DeferTextInputBufferCopy = false;
        }

        size_t NewCursorEnd = gUIContext->TextInputCursorEnd;
        char const *BufferString =
            UsePersistentBuffer && (Focused || WasFocused)
                ? gUIContext->TextInputBuffer.Data
                : Buffer;
        BufferOffset = Rr_AddV2(Offset, gUIContext->InputFieldPadding);
        BufferExtent = Rr_UICalculateTextSize(SIZE_MAX, BufferString, 0.0f);

        FieldRect = (Rr_Rect){
            Offset,
            Rr_AddV2(
                BufferExtent,
                Rr_MulV2F(gUIContext->InputFieldPadding, 2.0f)),
        };
        if (UseFixedWidth)
        {
            FieldRect.Extent.X = FixedWidth;
        }

        Rr_Rect ClipRect =
            Rr_ResizeRect(&FieldRect, gUIContext->BevelThickness * -2.0f);
        Rr_UIPushSubClipRect(Layout, &ClipRect);

        if (AutoCenter)
        {
            BufferOffset.X =
                Offset.X + FixedWidth * 0.5f - BufferExtent.X * 0.5f;
        }
        BufferExtent = Rr_UIDrawInputText(
            BufferString,
            Focused,
            BufferOffset,
            gUIContext->TextInputCursorBegin,
            &NewCursorEnd,
            0.0f,
            &gUIContext->Colors.Foreground);

        Rr_UIApplyInputFieldPlaceholder(
            Offset,
            FixedWidth,
            Focused,
            PlaceholderString,
            AutoCenter,
            &BufferOffset,
            &BufferExtent);

        Rr_UIClickResult ClickResult =
            Rr_UIClickDrag(Layout, &FieldRect, Hash, (Rr_Rect){ 0 });

        if (ClickResult.ClickCount)
        {
            size_t BufferLength = strlen(Buffer);

            if (AutoSelect && !Focused && !WasFocused)
            {
                /* Select everything on first click. */

                gUIContext->TextInputCursorBegin = 0;
                gUIContext->TextInputCursorEnd = BufferLength;
                gUIContext->TextInputClickID =
                    gUIContext->LeftMouseButton.ClickID;
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
                    gUIContext->TextInputCursorEnd =
                        Rr_LastUTF8CharInWordOffset(
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

            Rr_UISetTextInputMaxCol(Buffer, gUIContext->TextInputCursorEnd);
            gUIContext->TextInputCursorBlinkTime = Rr_GetTimeMS();
        }
        else if (Focused && ClickResult.Moved)
        {
            if (!AutoSelect || gUIContext->LeftMouseButton.ClickID >
                                   gUIContext->TextInputClickID)
            {
                gUIContext->TextInputCursorBlinkTime = Rr_GetTimeMS();
                gUIContext->TextInputCursorEnd = NewCursorEnd;
            }
        }

        if (Focused)
        {
            bool EnterToConfirm =
                !(Flags & RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT);
            EditResult = Rr_UIEditUTF8Buffer(
                &gUIContext->TextInputCursorBegin,
                &gUIContext->TextInputCursorEnd,
                BufferCapacity,
                UsePersistentBuffer ? gUIContext->TextInputBuffer.Data : Buffer,
                FilterFunc,
                EnterToConfirm);
            if (EditResult.Confirmed)
            {
                gUIContext->FocusedWidgetParent = NULL;
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

        bool ChangeCursor =
            ClickResult.Hovered &&
            (!gUIContext->DragParent || (gUIContext->DragHash == Hash));
        ChangeCursor |= ClickResult.Held;
        if (ChangeCursor)
        {
            gUIContext->CursorType = RR_CURSOR_TYPE_TEXT;
        }
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

    Rr_UIPopSubClipRect(Layout);

    return (Rr_UIInputFieldResult){
        .Extent = FieldRect.Extent,
        .Edited = EditResult.Edited,
        .BeganDragging = BeganDragging,
        .Dragged = Dragged,
    };
}

typedef enum
{
    RR_UI_SCALAR_TYPE_INT,
    RR_UI_SCALAR_TYPE_UINT,
    RR_UI_SCALAR_TYPE_FLOAT,
    RR_UI_SCALAR_TYPE_DOUBLE,
} Rr_UIScalarType;

static inline bool Rr_UIHexFilter(size_t Length, char const *UTF8String)
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

static inline bool Rr_UIIntegerFilter(size_t Length, char const *UTF8String)
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
    char const *UTF8String)
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

static inline bool Rr_UIFloatFilter(size_t Length, char const *UTF8String)
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
    Rr_UIScalarType ScalarType,
    void *ElementData)
{
    switch (ScalarType)
    {
        case RR_UI_SCALAR_TYPE_INT:
        {
            snprintf(Buffer, BufferCapacity, "%d", *(int *)ElementData);
        }
        break;
        case RR_UI_SCALAR_TYPE_UINT:
        {
            snprintf(
                Buffer,
                BufferCapacity,
                "%u",
                *(unsigned int *)ElementData);
        }
        break;
        case RR_UI_SCALAR_TYPE_FLOAT:
        {
            Rr_UIFormatDouble(BufferCapacity, Buffer, *(float *)ElementData);
        }
        break;
        case RR_UI_SCALAR_TYPE_DOUBLE:
        {
            Rr_UIFormatDouble(BufferCapacity, Buffer, *(double *)ElementData);
        }
        break;
        default:
        {
            RR_LOG_ABORT("Unsupported format type!");
        }
        break;
    }
}

static inline float Rr_UICalculateGenericInputScalarMultiWidth(
    int Cols,
    int Rows,
    void *Data,
    Rr_UIScalarType ScalarType)
{
    char ComponentBuffer[RR_UI_SCALAR_BUFFER_SIZE];

    size_t ComponentSize;
    switch (ScalarType)
    {
        case RR_UI_SCALAR_TYPE_INT:
        {
            ComponentSize = sizeof(int32_t);
        }
        break;
        case RR_UI_SCALAR_TYPE_UINT:
        {
            ComponentSize = sizeof(uint32_t);
        }
        break;
        case RR_UI_SCALAR_TYPE_FLOAT:
        {
            ComponentSize = sizeof(float);
        }
        break;
        case RR_UI_SCALAR_TYPE_DOUBLE:
        {
            ComponentSize = sizeof(double);
        }
        break;
        default:
        {
            RR_LOG_ABORT("Unsupported format type!");
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
                ScalarType,
                ComponentData);

            Rr_Vec2 TextExtent =
                Rr_UICalculateTextSize(SIZE_MAX, ComponentBuffer, 0.0f);
            MaxTextWidth = RR_MAX(MaxTextWidth, TextExtent.X);
        }
    }

    MaxTextWidth += gUIContext->InputFieldPadding.X * 2.0f;

    return MaxTextWidth * (float)Cols +
           gUIContext->ComponentMargin * (float)(Cols - 1);
}

static inline void Rr_UIRecordDragScalar(void *Data, Rr_UIScalarType Type)
{
    switch (Type)
    {
        case RR_UI_SCALAR_TYPE_INT:
        {
            gUIContext->DragScalarValue.Int32 = *(int32_t *)Data;
        }
        break;
        case RR_UI_SCALAR_TYPE_UINT:
        {
            gUIContext->DragScalarValue.UnsignedInt32 = *(uint32_t *)Data;
        }
        break;
        case RR_UI_SCALAR_TYPE_FLOAT:
        {
            gUIContext->DragScalarValue.Float = *(float *)Data;
        }
        break;
        case RR_UI_SCALAR_TYPE_DOUBLE:
        {
            gUIContext->DragScalarValue.Double = *(double *)Data;
        }
        break;
        default:
        {
            RR_LOG_ABORT("Unsupported format type!");
        }
        break;
    }
}

static inline void Rr_UIModifyUnsignedInt(
    uint32_t *UnsignedInt,
    uint32_t Min,
    uint32_t Max,
    float Amount)
{
    uint32_t Abs = (uint32_t)fabsf(Amount);
    uint32_t Old = gUIContext->DragScalarValue.UnsignedInt32;
    if (Amount > 0.0f)
    {
        if (Old + Abs < Old)
        {

            *UnsignedInt = UINT32_MAX;
            return;
        }
        *UnsignedInt = gUIContext->DragScalarValue.UnsignedInt32 + Abs;
    }
    if (Amount < 0.0f)
    {
        if (Old - Abs > Old)
        {
            *UnsignedInt = Min;

            return;
        }
        *UnsignedInt = gUIContext->DragScalarValue.UnsignedInt32 - Abs;
    }
    *UnsignedInt = RR_CLAMP(Min, *UnsignedInt, Max);
}

static inline void Rr_UIModifyDragScalar(
    void *Data,
    Rr_UIScalarType Type,
    float Amount)
{
    switch (Type)
    {
        case RR_UI_SCALAR_TYPE_INT:
        {
            *(int32_t *)Data =
                gUIContext->DragScalarValue.Int32 + (int32_t)Amount;
        }
        break;
        case RR_UI_SCALAR_TYPE_UINT:
        {
            uint32_t *UnsignedInt = (uint32_t *)Data;
            Rr_UIModifyUnsignedInt(UnsignedInt, 0, UINT32_MAX, Amount);
        }
        break;
        case RR_UI_SCALAR_TYPE_FLOAT:
        {
            *(float *)Data = gUIContext->DragScalarValue.Float + Amount;
        }
        break;
        case RR_UI_SCALAR_TYPE_DOUBLE:
        {
            *(double *)Data = gUIContext->DragScalarValue.Double + Amount;
        }
        break;
        default:
        {
            RR_LOG_ABORT("Unsupported format type!");
        }
        break;
    }
}

static inline void Rr_UIModifyDragScalarRange(
    void *Data,
    void const *DataMin,
    void const *DataMax,
    Rr_UIScalarType Type,
    float Amount)
{
    switch (Type)
    {
        case RR_UI_SCALAR_TYPE_INT:
        {
            int32_t Min = *(int32_t const *)DataMin;
            int32_t Max = *(int32_t const *)DataMax;
            int32_t Range = Max - Min;
            int32_t *Int = (int32_t *)Data;
            *Int = gUIContext->DragScalarValue.Int32 +
                   (int32_t)(Amount * (float)Range);
            *Int = RR_CLAMP(Min, *Int, Max);
        }
        break;
        case RR_UI_SCALAR_TYPE_UINT:
        {
            uint32_t Min = *(uint32_t const *)DataMin;
            uint32_t Max = *(uint32_t const *)DataMax;
            uint32_t Range = Max - Min;
            uint32_t *UnsignedInt = (uint32_t *)Data;
            Rr_UIModifyUnsignedInt(
                UnsignedInt,
                Min,
                Max,
                Amount * (float)Range);
        }
        break;
        case RR_UI_SCALAR_TYPE_FLOAT:
        {
            float Min = *(float const *)DataMin;
            float Max = *(float const *)DataMax;
            float Range = Max - Min;
            float *Float = (float *)Data;
            *Float = gUIContext->DragScalarValue.Float + Amount * Range;
            *Float = RR_CLAMP(Min, *Float, Max);
        }
        break;
        case RR_UI_SCALAR_TYPE_DOUBLE:
        {
            double Min = *(double const *)DataMin;
            double Max = *(double const *)DataMax;
            double Range = Max - Min;
            double *Double = (double *)Data;
            *Double = gUIContext->DragScalarValue.Double + Amount * Range;
            *Double = RR_CLAMP(Min, *Double, Max);
        }
        break;
        default:
        {
            RR_LOG_ABORT("Unsupported format type!");
        }
        break;
    }
}

static inline void Rr_UIClampScalarRange(
    void *Data,
    void const *DataMin,
    void const *DataMax,
    Rr_UIScalarType Type)
{
    switch (Type)
    {
        case RR_UI_SCALAR_TYPE_INT:
        {
            int32_t Min = *(int32_t const *)DataMin;
            int32_t Max = *(int32_t const *)DataMax;
            int32_t *Int = (int32_t *)Data;
            *Int = RR_CLAMP(Min, *Int, Max);
        }
        break;
        case RR_UI_SCALAR_TYPE_UINT:
        {
            uint32_t Min = *(uint32_t const *)DataMin;
            uint32_t Max = *(uint32_t const *)DataMax;
            uint32_t *UnsignedInt = (uint32_t *)Data;
            *UnsignedInt = RR_CLAMP(Min, *UnsignedInt, Max);
        }
        break;
        case RR_UI_SCALAR_TYPE_FLOAT:
        {
            float Min = *(float const *)DataMin;
            float Max = *(float const *)DataMax;
            float *Float = (float *)Data;
            *Float = RR_CLAMP(Min, *Float, Max);
        }
        break;
        case RR_UI_SCALAR_TYPE_DOUBLE:
        {
            double Min = *(double const *)DataMin;
            double Max = *(double const *)DataMax;
            double *Double = (double *)Data;
            *Double = RR_CLAMP(Min, *Double, Max);
        }
        break;
        default:
        {
            RR_LOG_ABORT("Unsupported format type!");
        }
        break;
    }
}

static inline Rr_UIInputFieldResult Rr_UIGenericInputScalarMulti(
    Rr_UIHash Hash,
    Rr_Vec2 Offset,
    void *Data,
    void const *DataMin,
    void const *DataMax,
    Rr_UIScalarType ScalarType,
    int Cols,
    int Rows,
    Rr_UIInputFieldFlags Flags,
    float FixedTotalWidth,
    bool DrawBackground)
{
    assert(Cols > 0 && Cols <= 4);
    assert(Rows > 0 && Rows <= 4);

    bool Range = DataMin && DataMax;

    Rr_UIInputFieldFilterFunc FilterFunc;
    size_t ComponentSize;
    switch (ScalarType)
    {
        case RR_UI_SCALAR_TYPE_INT:
        {
            ComponentSize = sizeof(int32_t);
            FilterFunc = Rr_UIIntegerFilter;
        }
        break;
        case RR_UI_SCALAR_TYPE_UINT:
        {
            ComponentSize = sizeof(uint32_t);
            FilterFunc = Rr_UIUnsignedIntegerFilter;
        }
        break;
        case RR_UI_SCALAR_TYPE_FLOAT:
        {
            ComponentSize = sizeof(float);
            FilterFunc = Rr_UIFloatFilter;
        }
        break;
        case RR_UI_SCALAR_TYPE_DOUBLE:
        {
            ComponentSize = sizeof(double);
            FilterFunc = Rr_UIFloatFilter;
        }
        break;
        default:
        {
            RR_LOG_ABORT("Unsupported format type!");
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

    char const *COMPONENT_TITLES[] = {
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
            void *ComponentData = (char *)Data + Index * ComponentSize;
            char const *ComponentMin = NULL;
            char const *ComponentMax = NULL;
            if (Range)
            {
                ComponentMin = (char const *)DataMin + Index * ComponentSize;
                ComponentMax = (char const *)DataMax + Index * ComponentSize;
            }
            char ComponentBuffer[RR_UI_SCALAR_BUFFER_SIZE];

            Rr_UIFormatScalar(
                sizeof(ComponentBuffer),
                ComponentBuffer,
                ScalarType,
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
                SingleFieldWidth);

            if (ComponentResult.BeganDragging)
            {
                Rr_UIRecordDragScalar(ComponentData, ScalarType);
                if (Range)
                {
                    Rr_UIClampScalarRange(
                        ComponentData,
                        ComponentMin,
                        ComponentMax,
                        ScalarType);
                }
            }
            else if (ComponentResult.Dragged != 0.0f)
            {
                if (Range)
                {
                    Rr_UIModifyDragScalarRange(
                        ComponentData,
                        ComponentMin,
                        ComponentMax,
                        ScalarType,
                        ComponentResult.Dragged / SingleFieldWidth);
                }
                else
                {
                    Rr_UIModifyDragScalar(
                        ComponentData,
                        ScalarType,
                        ComponentResult.Dragged);
                }

                Result.Edited = true;
            }
            else
            {
                Result.Edited |= ComponentResult.Edited;

                if (Result.Edited)
                {
                    switch (ScalarType)
                    {
                        case RR_UI_SCALAR_TYPE_INT:
                        {
                            (void)sscanf(
                                ComponentBuffer,
                                "%i",
                                (int *)ComponentData);
                        }
                        break;
                        case RR_UI_SCALAR_TYPE_UINT:
                        {
                            (void)sscanf(
                                ComponentBuffer,
                                "%u",
                                (unsigned int *)ComponentData);
                        }
                        break;
                        case RR_UI_SCALAR_TYPE_FLOAT:
                        {
                            (void)sscanf(
                                ComponentBuffer,
                                "%g",
                                (float *)ComponentData);
                        }
                        break;
                        case RR_UI_SCALAR_TYPE_DOUBLE:
                        {
                            (void)sscanf(
                                ComponentBuffer,
                                "%lg",
                                (double *)ComponentData);
                        }
                        break;
                        default:
                        {
                        }
                        break;
                    }
                    if (Range)
                    {
                        Rr_UIClampScalarRange(
                            ComponentData,
                            ComponentMin,
                            ComponentMax,
                            ScalarType);
                    }
                }
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
    char const *Title,
    void *Data,
    void const *DataMin,
    void const *DataMax,
    int Cols,
    int Rows,
    Rr_UIScalarType ScalarType)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    float RigidWidth = Rr_UICalculateGenericInputScalarMultiWidth(
        Cols,
        Rows,
        Data,
        ScalarType);

    float FlexibleWidth =
        Rr_UISetupFlexibleWidget(Layout, TitleLength, Title, RigidWidth);

    Rr_UIInputFieldResult InputResult = Rr_UIGenericInputScalarMulti(
        TitleHash,
        Layout->Cursor,
        Data,
        DataMin,
        DataMax,
        ScalarType,
        Cols,
        Rows,
        RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_DRAG_BIT,
        FlexibleWidth,
        true);

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += InputResult.Extent.X + gUIContext->FlexibleTitleMargin;
    TitleOffset.Y += gUIContext->InputFieldPadding.Y;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground);

    Rr_Vec2 RigidExtent = {
        RigidWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        InputResult.Extent.Y,
    };

    Rr_Vec2 FlexibleExtent = {
        FlexibleWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        InputResult.Extent.Y,
    };

    Rr_UIAdvance(RigidExtent, FlexibleExtent);

    return InputResult.Edited;
}

bool Rr_UIInputField(
    char const *Title,
    size_t BufferCapacity,
    char *Buffer,
    char const *Placeholder,
    Rr_UIInputFieldFilterFunc FilterFunc,
    Rr_UIInputFieldFlags Flags)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    Rr_UIAssertWindow();
    assert(Title != NULL);
    assert(BufferCapacity);
    assert(Buffer != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    Rr_Vec2 TextSize = Rr_UICalculateTextSize(SIZE_MAX, Buffer, 0.0f);
    if (Placeholder)
    {
        TextSize.X = RR_MAX(
            TextSize.X,
            Rr_UICalculateTextSize(SIZE_MAX, Placeholder, 0.0f).X);
    }

    float RigidWidth = TextSize.X + gUIContext->InputFieldPadding.X * 2.0f;

    float FlexibleWidth =
        Rr_UISetupFlexibleWidget(Layout, TitleLength, Title, RigidWidth);

    Rr_UIInputFieldResult Result = Rr_UIGenericInputField(
        TitleHash,
        Layout->Cursor,
        BufferCapacity,
        Buffer,
        Placeholder,
        FilterFunc,
        Flags,
        FlexibleWidth);

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += Result.Extent.X + gUIContext->FlexibleTitleMargin;
    TitleOffset.Y += gUIContext->InputFieldPadding.Height;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0,
        &gUIContext->Colors.Foreground);

    Rr_Vec2 RigidExtent = {
        RigidWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        Result.Extent.Y,
    };

    Rr_Vec2 FlexibleExtent = {
        FlexibleWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        Result.Extent.Y,
    };

    Rr_UIAdvance(RigidExtent, FlexibleExtent);

    Rr_DestroyScratch(Scratch);

    return Result.Edited;
}

bool Rr_UIInputText(char const *Title, size_t BufferCapacity, char *Buffer)
{
    return Rr_UIInputField(
        Title,
        BufferCapacity,
        Buffer,
        NULL,
        NULL,
        RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT);
}

bool Rr_UIInputFloat(char const *Title, float *Value)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        NULL,
        NULL,
        1,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloatRange(char const *Title, float *Value, float Min, float Max)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        &Min,
        &Max,
        1,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloatZO(char const *Title, float *Value)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        RR_UI_VEC4_ZERO.Elements,
        RR_UI_VEC4_ONE.Elements,
        1,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloatNO(char const *Title, float *Value)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        RR_UI_VEC4_NEG.Elements,
        RR_UI_VEC4_ONE.Elements,
        1,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat2(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        NULL,
        NULL,
        2,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat2Range(
    char const *Title,
    float *Values,
    float const *ValuesMin,
    float const *ValuesMax)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        ValuesMin,
        ValuesMax,
        2,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat2ZO(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        RR_UI_VEC4_ZERO.Elements,
        RR_UI_VEC4_ONE.Elements,
        2,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat2NO(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        RR_UI_VEC4_NEG.Elements,
        RR_UI_VEC4_ONE.Elements,
        2,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat3(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        NULL,
        NULL,
        3,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat3Range(
    char const *Title,
    float *Values,
    float const *MinValues,
    float const *MaxValues)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        MinValues,
        MaxValues,
        3,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat3ZO(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        RR_UI_VEC4_ZERO.Elements,
        RR_UI_VEC4_ONE.Elements,
        3,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat3NO(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        RR_UI_VEC4_NEG.Elements,
        RR_UI_VEC4_ONE.Elements,
        3,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat4(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        NULL,
        NULL,
        4,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat4Range(
    char const *Title,
    float *Values,
    float const *MinValues,
    float const *MaxValues)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        MinValues,
        MaxValues,
        4,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat4ZO(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        RR_UI_VEC4_ZERO.Elements,
        RR_UI_VEC4_ONE.Elements,
        4,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat4NO(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        RR_UI_VEC4_NEG.Elements,
        RR_UI_VEC4_ONE.Elements,
        4,
        1,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat2x2(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        NULL,
        NULL,
        2,
        2,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat3x3(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        NULL,
        NULL,
        3,
        3,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputFloat4x4(char const *Title, float *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        NULL,
        NULL,
        4,
        4,
        RR_UI_SCALAR_TYPE_FLOAT);
}

bool Rr_UIInputInt(char const *Title, int32_t *Value)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        NULL,
        NULL,
        1,
        1,
        RR_UI_SCALAR_TYPE_INT);
}

bool Rr_UIInputIntRange(
    char const *Title,
    int32_t *Value,
    int32_t Min,
    int32_t Max)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        &Min,
        &Max,
        1,
        1,
        RR_UI_SCALAR_TYPE_INT);
}

bool Rr_UIInputInt2(char const *Title, int32_t *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        NULL,
        NULL,
        2,
        1,
        RR_UI_SCALAR_TYPE_INT);
}

bool Rr_UIInputInt3(char const *Title, int32_t *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        NULL,
        NULL,
        3,
        1,
        RR_UI_SCALAR_TYPE_INT);
}

bool Rr_UIInputInt4(char const *Title, int32_t *Values)
{
    return Rr_UIInputScalarMulti(
        Title,
        Values,
        NULL,
        NULL,
        4,
        1,
        RR_UI_SCALAR_TYPE_INT);
}

bool Rr_UIInputUnsignedInt(char const *Title, uint32_t *Value)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        NULL,
        NULL,
        1,
        1,
        RR_UI_SCALAR_TYPE_UINT);
}

bool Rr_UIInputUnsignedIntRange(
    char const *Title,
    uint32_t *Value,
    uint32_t Min,
    uint32_t Max)
{
    return Rr_UIInputScalarMulti(
        Title,
        Value,
        &Min,
        &Max,
        1,
        1,
        RR_UI_SCALAR_TYPE_UINT);
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
        RR_UI_WINDOW_FLAGS_NO_TITLE_BAR_BIT | RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT |
        RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT |
        RR_UI_WINDOW_FLAGS_NO_COLLAPSE_BIT |
        RR_UI_WINDOW_FLAGS_NO_COLLAPSE_BIT |
        RR_UI_WINDOW_FLAGS_ESCAPE_CLOSES_BIT;

    float SVSelectorSize = Rr_UICurrentLineHeight() * 15.0f;
    float SVSelectorActiveSize =
        SVSelectorSize - gUIContext->DoubleBevelThickness * 2.0f;

    Rr_Vec2 WindowPadding = Rr_UICurrentWindowPadding();

    Rr_Vec2 Position = Center;
    Position.X -= (WindowPadding.X + SVSelectorSize) / 2.0f;
    Position.Y -= (WindowPadding.Y + SVSelectorSize) / 2.0f;
    Rr_UISetNextWindowOpenOffset(Position);
    Rr_UIBeginPopupWindow(Hash, POPUP_WINDOW_FLAGS);

    Rr_UIWindow *Window = &gUIContext->PopupWindow;

    Rr_UILayout *Layout = Rr_UICurrentLayout();

    bool HSVChanged = false;

    Rr_UIBeginHorizontal();

    /* Draw saturation and value selector. */

    Rr_Vec2 SVCursor =
        Rr_AddV2(Layout->Cursor, Rr_V2F(gUIContext->DoubleBevelThickness));

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
        Rr_UIPushSubClipRect(Layout, &Layout->CurrentClipRect->Rect);
        Layout->CurrentClipRect->ForceLinearPipeline = true;

        Rr_UIVertex Vertices[4];

        Vertices[0].Color = Rr_V4F(1.0f);
        Vertices[0].Position = SVCursor;
        Vertices[0].UV = Rr_V2F(0.0f);

        Vertices[1].Color =
            Rr_V4(TopRightColor.X, TopRightColor.Y, TopRightColor.Z, 1.0f);
        Vertices[1].Position = SVCursor;
        Vertices[1].Position.X += SVSelectorActiveSize;
        Vertices[1].UV = Rr_V2F(0.0f);

        Vertices[2].Color =
            Rr_V4(TopRightColor.X, TopRightColor.Y, TopRightColor.Z, 1.0f);
        Vertices[2].Position = SVCursor;
        Vertices[2].Position.X += SVSelectorActiveSize;
        Vertices[2].Position.Y += SVSelectorActiveSize;
        Vertices[2].UV = Rr_V2F(0.0f);

        Vertices[3].Color = Rr_V4F(1.0f);
        Vertices[3].Position = SVCursor;
        Vertices[3].Position.Y += SVSelectorActiveSize;
        Vertices[3].UV = Rr_V2F(0.0f);

        Rr_UIDrawQuadVertices(Vertices);

        Vertices[0].Color = Rr_V4F(0.0f);
        Vertices[1].Color = Rr_V4F(0.0f);
        Vertices[2].Color = Rr_V4(0.0f, 0.0f, 0.0f, 1.0f);
        Vertices[3].Color = Rr_V4(0.0f, 0.0f, 0.0f, 1.0f);

        Rr_UIDrawQuadVertices(Vertices);

        Rr_UIPopSubClipRect(Layout);
    }

    Rr_UIDrawDoubleBevel(
        &(Rr_Rect){ Layout->Cursor, Rr_V2F(SVSelectorSize) },
        &gUIContext->Colors.SelectedOutline,
        gUIContext->DoubleBevelThickness);

    float SVSelectorCircleSize = SVSelectorSize * 0.035f;

    Rr_UIHash SVSelectorHash =
        Rr_UIGetHash(sizeof("SVSelector"), "SVSelector", Rr_UICurrentHash());

    Rr_Rect SVSelectorRect = {
        .Offset = Layout->Cursor,
        .Extent = Rr_V2F(SVSelectorSize),
    };

    Rr_UIClickResult ClickResult =
        Rr_UIClickDrag(Layout, &SVSelectorRect, SVSelectorHash, (Rr_Rect){ 0 });

    if (ClickResult.ClickCount || ClickResult.Held)
    {
        Rr_Vec2 Delta = Rr_SubV2(gUIContext->MousePosition, SVCursor);
        Delta = Rr_DivV2F(Delta, SVSelectorActiveSize);
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
        SVCursor,
        Rr_V2(
            StaticHSV.Y * SVSelectorActiveSize,
            (1.0f - StaticHSV.Z) * SVSelectorActiveSize));

    Rr_UIAdvance(Rr_V2F(SVSelectorSize), Rr_V2F(0.0f));

    Rr_UIHash HSelectorHash =
        Rr_UIGetHash(sizeof("HSelector"), "HSelector", Rr_UICurrentHash());

    Rr_Vec4 HColors[6] = {
        Rr_V4(1.0f, 0.0f, 0.0f, 1.0f), Rr_V4(1.0f, 1.0f, 0.0f, 1.0f),
        Rr_V4(0.0f, 1.0f, 0.0f, 1.0f), Rr_V4(0.0f, 1.0f, 1.0f, 1.0f),
        Rr_V4(0.0f, 0.0f, 1.0f, 1.0f), Rr_V4(1.0f, 0.0f, 1.0f, 1.0f),
    };

    float HSelectorWidth = SVSelectorSize * 0.15f;

    Rr_Rect HSelectorRect = {
        .Offset = Rr_AddV2F(Layout->Cursor, gUIContext->DoubleBevelThickness),
        .Extent = Rr_SubV2F(
            Rr_V2(HSelectorWidth, SVSelectorSize),
            gUIContext->DoubleBevelThickness * 2.0f),
    };

    Rr_UIPushSubClipRect(Layout, &Layout->CurrentClipRect->Rect);
    Layout->CurrentClipRect->ForceLinearPipeline = true;
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
    Rr_UIPopSubClipRect(Layout);

    Rr_Rect HBevelRect =
        Rr_ResizeRect(&HSelectorRect, gUIContext->DoubleBevelThickness);
    Rr_UIDrawDoubleBevel(
        &HBevelRect,
        &gUIContext->Colors.SelectedOutline,
        gUIContext->DoubleBevelThickness);

    /* Draw hue handles. */

    float TriangleOutline = 2.0f;
    float TriangleSize = SVSelectorSize * 0.035f;
    Rr_Vec2 LeftTriangleOffset = Rr_V2(
        HSelectorRect.Offset.X + TriangleSize * 0.5f -
            gUIContext->DoubleBevelThickness,
        HSelectorRect.Offset.Y + StaticHSV.X * SVSelectorActiveSize);
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
        HSelectorRect.Offset.X + HSelectorWidth - TriangleSize * 0.5f -
            gUIContext->DoubleBevelThickness,
        HSelectorRect.Offset.Y + StaticHSV.X * SVSelectorActiveSize);
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
        Rr_UIClickDrag(Layout, &HSelectorRect, HSelectorHash, (Rr_Rect){ 0 });

    if (ClickResult.ClickCount || ClickResult.Held)
    {
        float Delta = gUIContext->MousePosition.Y - Layout->Cursor.Y;
        Delta /= SVSelectorActiveSize;
        Delta = RR_CLAMP(0.0f, Delta, 1.0f);

        StaticHSV.X = Delta;

        *(Rr_Vec3 *)Channels = Rr_UIHSVToRGB(&StaticHSV);

        HSVChanged = true;
    }

    Rr_UIAdvance(
        Rr_AddV2F(
            HSelectorRect.Extent,
            gUIContext->DoubleBevelThickness * 2.0f),
        Rr_V2F(0.0f));

    Rr_UIEndHorizontal();

    /* Input fields for different representations. */

    bool RGBChanged = ChannelCount == 3 ? Rr_UIInputFloat3Range(
                                              "RGB###RGB32",
                                              Channels,
                                              RR_UI_VEC4_ZERO.Elements,
                                              RR_UI_VEC4_ONE.Elements)
                                        : Rr_UIInputFloat4Range(
                                              "RGBA###RGBA32",
                                              Channels,
                                              RR_UI_VEC4_ZERO.Elements,
                                              RR_UI_VEC4_ONE.Elements);

    if (RGBChanged)
    {
        HSVChanged = false;
        StaticHSV = Rr_UIRGBToHSV((Rr_Vec3 *)Channels);
    }

    if (Rr_UIInputFloat3Range(
            "HSV###HSV32",
            StaticHSV.Elements,
            RR_UI_VEC4_ZERO.Elements,
            RR_UI_VEC4_ONE.Elements))
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
        (void)sscanf(HexBuffer, "%x", &NewColor);
        if (ChannelCount == 3)
        {
            NewColor <<= 8;
            Rr_Vec3 RGB = Rr_U32ToRGB(NewColor);
            memcpy(Channels, &RGB, sizeof(RGB));
        }
        else if (ChannelCount == 4)
        {
            Rr_Vec4 RGBA = Rr_U32ToRGBA(NewColor);
            memcpy(Channels, &RGBA, sizeof(RGBA));
        }

        HSVChanged = false;
        StaticHSV = Rr_UIRGBToHSV((Rr_Vec3 *)Channels);
    }

    if (HSVChanged)
    {
        *(Rr_Vec3 *)Channels = Rr_UIHSVToRGB(&StaticHSV);
    }

    /* Draw saturation/value circle here so it's on top of everything. */

    float CircleOutlineThickness = 3.0f;
    if (SVSelectorHeld)
    {
        Rr_Vec4 OpaqueColor;
        memcpy(&OpaqueColor, Channels, sizeof(float) * (size_t)ChannelCount);
        OpaqueColor.A = 1.0f;

        Rr_UIPushSubClipRect(Layout, &Layout->CurrentClipRect->Rect);
        Layout->CurrentClipRect->ForceLinearPipeline = true;

        Rr_UIDrawCircleFilled(
            SVSelectorCircleOffset,
            SVSelectorCircleSize,
            &OpaqueColor);

        Rr_UIPopSubClipRect(Layout);
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

    Rr_UIEndWindow();
}

static inline bool Rr_UIInputColorEx(
    char const *Title,
    int ChannelCount,
    float *Channels)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    Rr_UIAssertWindow();
    assert(Title != NULL);
    assert(Channels != NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;
    Rr_UIFont *Font = Rr_UICurrentFont();

    Rr_Vec2 ColorBoxExtent =
        Rr_V2F(Font->LineHeight + gUIContext->InputFieldPadding.Y * 2.0f);

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    float ColorBoxWithMargin = gUIContext->ComponentMargin + ColorBoxExtent.X;

    float RigidWidth = Rr_UICalculateGenericInputScalarMultiWidth(
        ChannelCount,
        1,
        Channels,
        RR_UI_SCALAR_TYPE_FLOAT);
    RigidWidth += ColorBoxWithMargin;

    float FlexibleWidth =
        Rr_UISetupFlexibleWidget(Layout, TitleLength, Title, RigidWidth);

    Rr_UIInputFieldResult InputResult = Rr_UIGenericInputScalarMulti(
        TitleHash,
        Layout->Cursor,
        Channels,
        RR_UI_VEC4_ZERO.Elements,
        RR_UI_VEC4_ONE.Elements,
        RR_UI_SCALAR_TYPE_FLOAT,
        ChannelCount,
        1,
        RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT |
            RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT |
            RR_UI_INPUT_FIELD_FLAGS_DRAG_BIT,
        FlexibleWidth - ColorBoxWithMargin,
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
        Rr_UIShowPopupWindow(Window, TitleHash);
    }

    bool ColorChanged = false;

    if (Rr_UIShouldShowPopupWindow(Window, TitleHash))
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
    if (gUIContext->SRGBSwapchain)
    {
        Rr_UIToLinearColor(&OpaqueColor);
    }
    Rr_UIDrawBevel(
        &ColorBoxRect,
        &OpaqueColor,
        ClickResult.Held && ClickResult.Hovered);
    if (ChannelCount == 4)
    {
        Rr_Rect InnerRect =
            Rr_ResizeRect(&ColorBoxRect, -gUIContext->BevelThickness);
        Rr_UIDrawCheckerQuad(&InnerRect, Font->LineHeight * 0.5f);

        Rr_Vec4 TransparentColor;
        memcpy(
            &TransparentColor,
            Channels,
            sizeof(float) * (size_t)ChannelCount);
        if (gUIContext->SRGBSwapchain)
        {
            Rr_UIToLinearColor(&TransparentColor);
        }

        Rr_UIDrawVerticalGradientQuad(
            &InnerRect,
            &TransparentColor,
            &OpaqueColor);
    }

    Rr_Vec2 TitleOffset = Layout->Cursor;
    TitleOffset.X += InputResult.Extent.X + gUIContext->ComponentMargin +
                     ColorBoxExtent.X + gUIContext->FlexibleTitleMargin;
    TitleOffset.Y += gUIContext->InputFieldPadding.Y;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground);

    Rr_Vec2 RigidExtent = {
        RigidWidth + ColorBoxWithMargin + gUIContext->FlexibleTitleMargin +
            TitleExtent.X,
        InputResult.Extent.Y,
    };

    Rr_Vec2 FlexibleExtent = {
        FlexibleWidth + ColorBoxWithMargin + gUIContext->FlexibleTitleMargin +
            TitleExtent.X,
        InputResult.Extent.Y,
    };

    Rr_UIAdvance(RigidExtent, FlexibleExtent);

    return ColorChanged;
}

bool Rr_UIInputColor3(char const *Title, float *Channels)
{
    return Rr_UIInputColorEx(Title, 3, Channels);
}

bool Rr_UIInputColor4(char const *Title, float *Channels)
{
    return Rr_UIInputColorEx(Title, 4, Channels);
}

bool Rr_UICombobox(
    char const *Title,
    uint32_t OptionCount,
    char const *const *Options,
    uint32_t *SelectedIndex)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    Rr_UIAssertWindow();
    assert(Title != NULL);
    assert(OptionCount > 0);
    assert(Options != NULL);
    assert(SelectedIndex != NULL);

    /* Rr_Scratch Scratch = Rr_GetScratch(NULL); */

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIWindow *Window = Layout->Window;
    Rr_UIFont *Font = Rr_UICurrentFont();

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
        &gUIContext->Colors.Foreground);

    float RigidWidth =
        SelectedTextSize.X + gUIContext->InputFieldPadding.X * 2.0f +
        Font->LineHeight + gUIContext->InputFieldPadding.Y * 2.0f;

    float FlexibleWidth =
        Rr_UISetupFlexibleWidget(Layout, TitleLength, Title, RigidWidth);

    float ButtonHeight =
        SelectedTextSize.Height + gUIContext->InputFieldPadding.Y * 2.0f;

    Rr_Vec2 ButtonExtent = {
        FlexibleWidth,
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
        Rr_UIShowPopupWindow(Window, TitleHash);
    }

    bool OptionChanged = false;
    bool ShouldShowPopupWindow = Rr_UIShouldShowPopupWindow(Window, TitleHash);

    if (ShouldShowPopupWindow)
    {
        Rr_UIWindowFlags const POPUP_WINDOW_FLAGS =
            RR_UI_WINDOW_FLAGS_NO_TITLE_BAR_BIT |
            RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT |
            RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT |
            RR_UI_WINDOW_FLAGS_NO_MOVE_BIT |
            RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT |
            RR_UI_WINDOW_FLAGS_ESCAPE_CLOSES_BIT;

        Rr_Vec2 PopupPosition = ButtonPosition;
        PopupPosition.Y += ButtonExtent.Height + gUIContext->FrameThickness;
        Rr_UIPushWindowPadding(Rr_V2(
            gUIContext->InputFieldPadding.X - gUIContext->DoubleBevelThickness,
            0.0f));
        Rr_UIPushContentsMargin(Rr_V2F(0.0f));
        Rr_UISetNextWindowOffset(PopupPosition);
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
                &gUIContext->Colors.Foreground);
            Rr_Rect OptionButtonRect;
            OptionButtonRect.Offset.Y = PopupLayout->Cursor.Y;
            OptionButtonRect.Offset.X =
                PopupLayout->Cursor.X - gUIContext->InputFieldPadding.Width;
            OptionButtonRect.Extent.Width =
                gUIContext->PopupWindow.Rect.Extent.Width;
            OptionButtonRect.Extent.Height = Font->LineHeight;
            Rr_UIClickResult OptionClickResult =
                Rr_UIClickSimple(PopupLayout, &OptionButtonRect, OptionHash);
            if (OptionClickResult.ClickCount)
            {
                *SelectedIndex = Index;
                Rr_UIClosePopupWindow();
                OptionChanged = true;
            }
            Rr_Vec4 OptionButtonColor;
            if (OptionClickResult.Hovered)
            {
                OptionButtonColor = gUIContext->Colors.ListEntryHovered;
            }
            else
            {
                OptionButtonColor =
                    Index % 2 == 0 ? gUIContext->Colors.ListEntryBackgroundA
                                   : gUIContext->Colors.ListEntryBackgroundB;
            }
            Rr_UISolidQuad(
                OptionButtonQuad.Vertices,
                &OptionButtonRect,
                &OptionButtonColor);
            Rr_UIAdvance(OptionSize, Rr_V2F(0.0f));
        }
        Rr_UIEndWindow();
        Rr_UIPopContentsMargin();
        Rr_UIPopWindowPadding();
    }

    Rr_Rect ButtonRect = {
        ButtonPosition,
        ButtonExtent,
    };

    Rr_Vec4 *SelectedOptionBackground;
    if (ShouldShowPopupWindow)
    {
        SelectedOptionBackground = &gUIContext->Colors.ComboboxButtonActive;
    }
    else
    {
        SelectedOptionBackground =
            ClickResult.Held ? &gUIContext->Colors.ComboboxButtonHeld
                             : &gUIContext->Colors.ComboboxButtonNormal;
    }

    Rr_UIBevel(
        Primitive,
        &ButtonRect,
        SelectedOptionBackground,
        ClickResult.Held);

    /* Add handle. */
    {
        float HandleSize = ButtonExtent.Height;
        Rr_Rect HandleRect = { ButtonRect.Offset, Rr_V2F(HandleSize) };
        HandleRect.Offset.X += ButtonRect.Extent.Width - ButtonExtent.Y;

        Rr_UIDrawBevel(
            &HandleRect,
            ClickResult.Held ? &gUIContext->Colors.ButtonHeld
                             : &gUIContext->Colors.ButtonNormal,
            ClickResult.Held);

        Rr_Vec2 TriangleCenter = Rr_RectCenter(&HandleRect);
        float TriangleSize = gUIContext->TitleBarHeight * 0.3f;
        Rr_UIDrawFitTriangleFilled(
            TriangleCenter,
            TriangleSize,
            !Layout->WasCollapsed ? RR_ANGLE_DEG(90.0f) : 0.0f,
            &gUIContext->Colors.Foreground);
    }

    Rr_Vec2 TitlePosition = Layout->Cursor;
    TitlePosition.X += FlexibleWidth + gUIContext->FlexibleTitleMargin;
    TitlePosition.Y += gUIContext->InputFieldPadding.Y;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitlePosition,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground);

    Rr_Vec2 RigidExtent = {
        RigidWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        Font->LineHeight + gUIContext->InputFieldPadding.Y * 2.0f,
    };

    Rr_Vec2 FlexibleExtent = {
        FlexibleWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        Font->LineHeight + gUIContext->InputFieldPadding.Y * 2.0f,
    };

    Rr_UIAdvance(RigidExtent, FlexibleExtent);

    return OptionChanged;
}

static inline float Rr_UISlider(
    char const *Title,
    float Normalized,
    char const *ValueCString,
    size_t ValueCStringLength,
    float HandleSizeRatio)
{
    Rr_UIAssertWindow();
    assert(Title != NULL);

    Rr_UILayout *Layout = Rr_UICurrentLayout();
    Rr_UIFont *Font = Rr_UICurrentFont();

    size_t TitleLength;
    Rr_UIHash TitleHash = Rr_UIGetTitleHash(Title, &TitleLength);

    /* NOTE: Reserve three handles worth of width just in case. Probably could
     * also use value text width. */
    float RigidWidth = Font->LineHeight * 10.0f;

    float FlexibleWidth =
        Rr_UISetupFlexibleWidget(Layout, TitleLength, Title, RigidWidth);

    Rr_Rect SliderRect = {
        Layout->Cursor,
        {
            FlexibleWidth,
            Font->LineHeight + gUIContext->InputFieldPadding.Y * 2.0f,
        },
    };

    Rr_UIPrimitive BackgroundBevel = Rr_UIReserveBevel();

    float HandleWidth = Font->LineHeight * 0.75f;
    if (HandleSizeRatio != 0.0f)
    {
        HandleWidth = RR_MAX(HandleWidth, FlexibleWidth * HandleSizeRatio);
    }
    HandleWidth = RR_UI_ROUND(HandleWidth);
    Rr_Rect HandleRect = { Layout->Cursor,
                           Rr_V2(HandleWidth, SliderRect.Extent.Y) };
    HandleRect.Offset.X += Normalized * (FlexibleWidth - HandleWidth);
    HandleRect.Offset.X = roundf(HandleRect.Offset.X);
    HandleRect = Rr_ResizeRect(&HandleRect, -gUIContext->BevelThickness);
    Rr_UIDrawBevel(&HandleRect, &gUIContext->Colors.ButtonNormal, false);

    if (ValueCString != NULL)
    {
        Rr_Vec2 ValueSize =
            Rr_UICalculateTextSize(ValueCStringLength, ValueCString, 0.0f);

        float ValueMargin = RR_UI_ROUND(Font->LineHeight * 0.2f);
        Rr_Vec2 ValuePosition = Layout->Cursor;
        ValuePosition.X = HandleRect.Offset.X - ValueSize.Width - ValueMargin;
        bool ShowValue = true;
        if (ValuePosition.X < SliderRect.Offset.X + gUIContext->BevelThickness)
        {
            ValuePosition.X =
                HandleRect.Offset.X + HandleRect.Extent.X + ValueMargin;
            if (ValuePosition.X + ValueSize.Width >
                SliderRect.Offset.X + SliderRect.Extent.Width -
                    gUIContext->BevelThickness)
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
                &gUIContext->Colors.Foreground);
        }
    }

    float HandleDragOffset = gUIContext->MousePosition.X -
                             (HandleRect.Offset.X + HandleWidth / 2.0f);

    Rr_UIClickResult ClickResult = Rr_UIClickDrag(
        Layout,
        &SliderRect,
        TitleHash,
        (Rr_Rect){ .Offset.X = HandleDragOffset });

    Rr_UIBevel(
        BackgroundBevel,
        &SliderRect,
        ClickResult.Held ? &gUIContext->Colors.InputFieldActive
                         : &gUIContext->Colors.InputFieldNormal,
        true);

    if (ClickResult.ClickCount || ClickResult.Moved)
    {
        float SliderMin = Layout->Cursor.X + HandleWidth / 2.0f;
        float SliderMax = SliderMin + FlexibleWidth - HandleWidth;

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
    TitleOffset.X += SliderRect.Extent.X + gUIContext->FlexibleTitleMargin;
    TitleOffset.Y += gUIContext->InputFieldPadding.Y;
    Rr_Vec2 TitleExtent = Rr_UIDrawText(
        false,
        TitleOffset,
        TitleLength,
        Title,
        0.0f,
        &gUIContext->Colors.Foreground);

    Rr_Vec2 RigidExtent = {
        RigidWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        SliderRect.Extent.Y,
    };

    Rr_Vec2 FlexibleExtent = {
        FlexibleWidth + gUIContext->FlexibleTitleMargin + TitleExtent.X,
        SliderRect.Extent.Y,
    };

    Rr_UIAdvance(RigidExtent, FlexibleExtent);

    return Normalized;
}

bool Rr_UISliderInt(char const *Title, int32_t *Value, int32_t Min, int32_t Max)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    assert(Value != NULL);
    assert(Max > Min);

    char Buffer[RR_UI_SCALAR_BUFFER_SIZE];
    int Length = snprintf(Buffer, sizeof(Buffer), "%d", *Value);

    float FloatRange = (float)(Max - Min);
    FloatRange = RR_MAX(1.0f, FloatRange);

    float HandleSizeRatio = 1.0f / (FloatRange + 1);

    int32_t In = *Value;
    int32_t Clamped = RR_CLAMP(Min, *Value, Max);
    float InNormalized = (float)(Clamped - Min) / FloatRange;
    float OutNormalized = Rr_UISlider(
        Title,
        InNormalized,
        Buffer,
        (size_t)Length,
        HandleSizeRatio);
    OutNormalized = roundf(OutNormalized * FloatRange) / FloatRange;
    int32_t Out = (int32_t)(OutNormalized * FloatRange) + Min;
    *Value = Out;
    return In != Out;
}

bool Rr_UISliderUnsignedInt(
    char const *Title,
    uint32_t *Value,
    uint32_t Min,
    uint32_t Max)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    assert(Value != NULL);
    assert(Max > Min);

    char Buffer[RR_UI_SCALAR_BUFFER_SIZE];
    int Length = snprintf(Buffer, sizeof(Buffer), "%d", *Value);

    float FloatRange = (float)(Max - Min);

    uint32_t In = *Value;
    uint32_t Clamped = RR_CLAMP(Min, *Value, Max);
    float InNormalized = (float)(Clamped - Min) / FloatRange;
    float OutNormalized = Rr_UISlider(
        Title,
        InNormalized,
        Buffer,
        (size_t)Length,
        1.0f / FloatRange);
    OutNormalized = roundf(OutNormalized * FloatRange) / FloatRange;
    uint32_t Out = (uint32_t)(OutNormalized * FloatRange) + Min;
    *Value = Out;
    return In != Out;
}

bool Rr_UISliderFloat(char const *Title, float *Value, float Min, float Max)
{
    if (Rr_UISkipItems())
    {
        return false;
    }

    assert(Value != NULL);
    assert(Max > Min);

    char Buffer[RR_UI_SCALAR_BUFFER_SIZE];
    int Length = Rr_UIFormatDouble(sizeof(Buffer), Buffer, *Value);

    float In = *Value;
    float Clamped = RR_CLAMP(Min, *Value, Max);
    float InNormalized = (Clamped - Min) / (Max - Min);
    float OutNormalized =
        Rr_UISlider(Title, InNormalized, Buffer, (size_t)Length, 0.0f);
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

static inline void Rr_UIConvertColorsToSRGB(void)
{
    size_t ColorCount = sizeof(Rr_UIColors) / sizeof(Rr_Vec4);
    Rr_Vec4 *Colors = (Rr_Vec4 *)&gUIContext->Colors;
    for (size_t Index = 0; Index < ColorCount; ++Index)
    {
        Rr_UIToSRGBColor(&Colors[Index]);
    }
}

static inline void Rr_UIConvertColorsToLinear(void)
{
    size_t ColorCount = sizeof(Rr_UIColors) / sizeof(Rr_Vec4);
    Rr_Vec4 *Colors = (Rr_Vec4 *)&gUIContext->Colors;
    for (size_t Index = 0; Index < ColorCount; ++Index)
    {
        Rr_UIToLinearColor(&Colors[Index]);
    }
}

void Rr_UISetDefaultTheme(void)
{
    Rr_UIStyle *Style = Rr_UIGetStyle();
    Rr_UIColors *Colors = Rr_UIGetColors();

    Style->FrameThickness = 0.075000f;
    Style->TitleBarPadding = Rr_V2(0.250000f, 0.025000f);
    Style->WindowPadding = Rr_V2(0.300000f, 0.300000f);
    Style->ContentsMargin = Rr_V2(0.250000f, 0.250000f);
    Style->ComponentMargin = 0.200000f;
    Style->ScrollbarAreaWidth = 0.750000f;
    Style->BevelThickness = 0.050000f;
    Style->DoubleBevelThickness = 0.100000f;
    Style->BevelIntensityLight = 0.300000f;
    Style->BevelIntensityDark = 0.650000f;
    Style->FlexibleTitleMargin = 0.250000f;
    Style->ButtonPadding = Rr_V2(0.250000f, 0.025000f);
    Style->InputFieldPadding = Rr_V2(0.250000f, 0.025000f);
    Style->CheckmarkRatios = Rr_V2(0.350000f, 0.200000f);
    Style->CheckmarkSize = 0.750000f;
    Style->CrossWidth = 0.650000f;
    Style->CrossThickness = 0.135000f;
    Colors->Foreground = Rr_V4(0.899630f, 0.924908f, 0.933333f, 1.000000f);
    Colors->ForegroundDimmed =
        Rr_V4(0.617390f, 0.649893f, 0.660727f, 1.000000f);
    Colors->Background = Rr_V4(0.108806f, 0.145386f, 0.172821f, 1.000000f);
    Colors->ChildBackground = Rr_V4(0.105882f, 0.145098f, 0.172549f, 1.000000f);
    Colors->ScrolloffBackground =
        Rr_V4(0.105882f, 0.145098f, 0.172549f, 1.000000f);
    Colors->Outline = Rr_V4(0.105882f, 0.145098f, 0.172549f, 1.000000f);
    Colors->SelectedOutline = Rr_V4(0.204012f, 0.283247f, 0.338711f, 1.000000f);
    Colors->ListEntryBackgroundA =
        Rr_V4(0.182623f, 0.277109f, 0.338889f, 1.000000f);
    Colors->ListEntryBackgroundB =
        Rr_V4(0.155230f, 0.235543f, 0.288056f, 1.000000f);
    Colors->ListEntryHovered =
        Rr_V4(0.168210f, 0.404396f, 0.555556f, 1.000000f);
    Colors->TitleForeground = Rr_V4(0.928603f, 0.954881f, 0.963641f, 1.000000f);
    Colors->TitleBackground = Rr_V4(0.123951f, 0.467914f, 0.697222f, 1.000000f);
    Colors->TitleBackground2 =
        Rr_V4(0.034855f, 0.159902f, 0.243268f, 1.000000f);
    Colors->TitleBackgroundInactive =
        Rr_V4(0.182623f, 0.277109f, 0.338889f, 1.000000f);
    Colors->TitleCloseButtonBackground =
        Rr_V4(0.839551f, 0.250613f, 0.313724f, 1.000000f);
    Colors->TitleCollapseButtonBackground =
        Rr_V4(0.121569f, 0.466667f, 0.694118f, 1.000000f);
    Colors->ScrollbarBackground =
        Rr_V4(0.053773f, 0.070767f, 0.084195f, 1.000000f);
    Colors->ScrollbarNormal = Rr_V4(0.292805f, 0.334535f, 0.363504f, 1.000000f);
    Colors->ScrollbarHovered =
        Rr_V4(0.408665f, 0.497836f, 0.557879f, 1.000000f);
    Colors->ScrollbarHeld = Rr_V4(0.290196f, 0.333333f, 0.360784f, 1.000000f);
    Colors->ResizeHandleNormal =
        Rr_V4(0.121569f, 0.466667f, 0.694118f, 1.000000f);
    Colors->ResizeHandleHovered =
        Rr_V4(0.104553f, 0.367593f, 0.540961f, 1.000000f);
    Colors->ResizeHandleHeld =
        Rr_V4(0.101961f, 0.364706f, 0.537255f, 1.000000f);
    Colors->ButtonNormal = Rr_V4(0.182623f, 0.277109f, 0.338889f, 1.000000f);
    Colors->ButtonHovered = Rr_V4(0.408665f, 0.497836f, 0.557879f, 1.000000f);
    Colors->ButtonHeld = Rr_V4(0.168210f, 0.404396f, 0.555556f, 1.000000f);
    Colors->ButtonDisabled = Rr_V4(0.070520f, 0.093346f, 0.111383f, 1.000000f);
    Colors->ComboboxButtonNormal =
        Rr_V4(0.086275f, 0.156863f, 0.215686f, 1.000000f);
    Colors->ComboboxButtonHeld =
        Rr_V4(0.164706f, 0.403922f, 0.552941f, 1.000000f);
    Colors->ComboboxButtonActive =
        Rr_V4(0.066667f, 0.250980f, 0.407843f, 1.000000f);
    Colors->RadioButtonNormal =
        Rr_V4(0.180392f, 0.274510f, 0.337255f, 1.000000f);
    Colors->RadioButtonOutline =
        Rr_V4(0.164706f, 0.403922f, 0.552941f, 1.000000f);
    Colors->RadioButtonHeld = Rr_V4(0.164706f, 0.403922f, 0.552941f, 1.000000f);
    Colors->InputFieldNormal =
        Rr_V4(0.089074f, 0.160347f, 0.216667f, 1.000000f);
    Colors->InputFieldActive =
        Rr_V4(0.070324f, 0.252329f, 0.408333f, 1.000000f);
    Colors->SelectedTextBackground =
        Rr_V4(0.433129f, 0.652866f, 0.996207f, 1.000000f);
    Colors->SelectedTextForeground =
        Rr_V4(0.030000f, 0.030000f, 0.030000f, 1.000000f);
}

void Rr_InitUI(void)
{
    assert(gUIContext == NULL);

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gUIContext = Rr_Alloc(sizeof(Rr_UIContext), Arena);
    gUIContext->Arena = Arena;

    gUIContext->NextWindowExtent = Rr_V2F(INFINITY);
    gUIContext->NextWindowMinExtent = Rr_V2F(INFINITY);
    gUIContext->NextWindowMaxExtent = Rr_V2F(INFINITY);
    gUIContext->NextWindowOffset = Rr_V2F(INFINITY);
    gUIContext->NextWindowOpenOffset = Rr_V2F(INFINITY);
    gUIContext->NextWindowPadding = Rr_V2F(INFINITY);

    float DefaultFontSize = 10.0f * Rr_GetDisplayScale();
    Rr_Asset FontAsset = Rr_LoadAsset(RR_BUILTIN_SOURCESERIF4_TTF);
    gUIContext->DefaultFont =
        Rr_UICreateFont(FontAsset.Size, FontAsset.Data, DefaultFontSize);

    Rr_UISetDefaultTheme();

    /* Rr_Binding Bindings[] = { */
    /*     { */
    /*         .Index = 0, */
    /*         .Type = RR_BINDING_TYPE_UNIFORM_BUFFER, */
    /*         .Stages = RR_SHADER_STAGE_VERTEX_BIT |
     * RR_SHADER_STAGE_FRAGMENT_BIT, */
    /*     }, */
    /*     { */
    /*         .Index = 1, */
    /*         .Type = RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, */
    /*         .Stages = RR_SHADER_STAGE_VERTEX_BIT |
     * RR_SHADER_STAGE_FRAGMENT_BIT, */
    /*     }, */
    /* }; */
    /* Rr_BindingSet BindingSets[] = { */
    /*     { */
    /*         RR_ARRAY_COUNT(Bindings), */
    /*         Bindings, */
    /*     }, */
    /* }; */
    /* gUIContext->PipelineLayout = */
    /*     Rr_CreatePipelineLayout(RR_ARRAY_COUNT(BindingSets), BindingSets); */

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

    Rr_ShaderInfo VertexShaderInfo = {
        .SPVSize = VertexShader.Size,
        .SPVData = VertexShader.Data,
        .SpecializationCount = RR_ARRAY_COUNT(Specializations),
        .Specializations = Specializations,
    };

    Rr_ShaderInfo FragmentShaderInfo = {
        .SPVSize = FragmentShader.Size,
        .SPVData = FragmentShader.Data,
    };

    Rr_GraphicsPipelineCreateInfo PipelineInfo = {
        .VertexShaderInfo = &VertexShaderInfo,
        .FragmentShaderInfo = &FragmentShaderInfo,
        .ColorTargetCount = RR_ARRAY_COUNT(ColorTargets),
        .ColorTargets = ColorTargets,
        .VertexInputBindingCount = 1,
        .VertexInputBindings = &VertexInputBinding,
    };

    uint32_t const DONT_CONVERT_TO_SRGB = 0;
    Specializations[0].Data = &DONT_CONVERT_TO_SRGB;
    Rr_SetNextObjectName("Rr.UI.LinearPipeline");
    gUIContext->LinearPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    uint32_t const CONVERT_TO_SRGB = 1;
    Specializations[0].Data = &CONVERT_TO_SRGB;
    Rr_SetNextObjectName("Rr.UI.SRGBPipeline");
    gUIContext->SRGBPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    Rr_SetNextObjectName("Rr.UI.VertexBuffer");
    gUIContext->VertexBuffer = Rr_CreateBuffer(
        RR_MEGABYTES(8),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    Rr_SetNextObjectName("Rr.UI.IndexBuffer");
    gUIContext->IndexBuffer = Rr_CreateBuffer(
        RR_MEGABYTES(8),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    Rr_SetNextObjectName("Rr.UI.UniformBuffer");
    gUIContext->UniformBuffer = Rr_CreateBuffer(
        sizeof(Rr_UIUniformData),
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    Rr_SetNextObjectName("Rr.UI.Sampler");
    gUIContext->Sampler = Rr_CreateSampler(&(Rr_SamplerInfo){
        .MinFilter = RR_FILTER_LINEAR,
        .MagFilter = RR_FILTER_LINEAR,
        .AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    });

    Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
    gUIContext->SRGBSwapchain =
        Rr_IsSRGBFormat(Rr_GetImageFormat(SwapchainImage));
}

void Rr_CleanupUI(void)
{
    assert(gUIContext != NULL);

    Rr_ReleaseBuffer(gUIContext->VertexBuffer);
    Rr_ReleaseBuffer(gUIContext->IndexBuffer);
    Rr_ReleaseBuffer(gUIContext->UniformBuffer);
    Rr_ReleaseSampler(gUIContext->Sampler);
    Rr_ReleaseGraphicsPipeline(gUIContext->LinearPipeline);
    Rr_ReleaseGraphicsPipeline(gUIContext->SRGBPipeline);

    Rr_UIReleaseFont(gUIContext->DefaultFont);

    Rr_DestroyArena(gUIContext->Arena);

    gUIContext = NULL;
}

void Rr_ProcessUIEvent(Rr_Event const *Event)
{
    if (gUIContext == NULL)
    {
        return;
    }

    switch (Event->Type)
    {
        case RR_EVENT_TYPE_FOCUS:
        {
            if (!Event->Focus.Focused)
            {
                Rr_UIConsumeMouseAndKeyboardInput();
            }
        }
        break;
        case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
        {
            Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
            gUIContext->SRGBSwapchain =
                Rr_IsSRGBFormat(Rr_GetImageFormat(SwapchainImage));
        }
        break;
        case RR_EVENT_TYPE_TEXT_INPUT:
        {
            size_t Length = Event->Text.Length;
            char *Text = Rr_AllocNoZero(Length + 1, gUIContext->FrameArena);
            memcpy(Text, Event->Text.CString, Length + 1);
            *RR_PUSH_INTO_ARRAY(
                &gUIContext->TextInputEvents,
                gUIContext->FrameArena) = Text;
        }
        break;
        case RR_EVENT_TYPE_KEY_DOWN:
        case RR_EVENT_TYPE_KEY_REPEAT:
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

void Rr_NewUIFrame(void)
{
    Rr_Arena *FrameArena = gRenderer->Frames[gRenderer->FrameIndex].Arena;
    gUIContext->FrameArena = FrameArena;

    RR_RESET_ARRAY(&gUIContext->LayoutStack, FrameArena);
    RR_RESET_ARRAY(&gUIContext->HashStack, FrameArena);
    RR_RESET_ARRAY(&gUIContext->FontStack, FrameArena);
    RR_RESET_ARRAY(&gUIContext->WindowPaddingStack, FrameArena);
    RR_RESET_ARRAY(&gUIContext->ContentsMarginStack, FrameArena);
    RR_RESET_ARRAY(&gUIContext->WidgetExtentStack, FrameArena);
    RR_RESET_ARRAY(&gUIContext->FormatFloatDecimalPlacesStack, FrameArena);
    RR_RESET_ARRAY(&gUIContext->Vertices, FrameArena);
    RR_RESET_ARRAY(&gUIContext->Indices, FrameArena);

    Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());
    gUIContext->ScreenSize.Width = (float)SwapchainSize.Width;
    gUIContext->ScreenSize.Height = (float)SwapchainSize.Height;
}

void Rr_BeginUI(void)
{
    gUIContext->HoveredWindow = NULL;
    if (Rr_UIPopupWindowActive())
    {
        if (Rr_RectContains(
                &gUIContext->PopupWindow.Rect,
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
        }
    }
    else
    {
        int LastIndex = (int)gUIContext->ActiveLayouts.Count - 1;
        for (int Index = LastIndex; Index >= 0; --Index)
        {
            Rr_UILayout *Layout = gUIContext->ActiveLayouts.Data[Index];
            Rr_UIWindow *Window = Layout->Window;
            if (Rr_RectContains(
                    &Layout->VisibleRect,
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
        Rr_UIResetClickAndDrag();
    }

    RR_CLEAR_ARRAY(&gUIContext->ActiveLayouts);

    Rr_UIRecalculateStyle();
}

static inline int Rr_UIWindowSort(void const *A, void const *B)
{
    Rr_UILayout const *LayoutA = *(Rr_UILayout *const *)A;
    Rr_UILayout const *LayoutB = *(Rr_UILayout *const *)B;

    return LayoutA->Window->Z - LayoutB->Window->Z;
}

static inline void Rr_UIDrawWindow(
    Rr_UIClipRectArray *Array,
    Rr_GraphNode *GraphicsNode,
    Rr_Image **BoundImage)
{
    size_t ClipRectCount = Array->Count;
    for (size_t ClipRectIndex = 0; ClipRectIndex < ClipRectCount;
         ++ClipRectIndex)
    {
        Rr_UIClipRect *ClipRect = Array->Data + ClipRectIndex;

        if (ClipRect->IndexCount == 0)
        {
            continue;
        }

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

        if (IntRect.Extent.Width <= 0 || IntRect.Extent.Height <= 0)
        {
            continue;
        }

        if (ClipRect->ForceLinearPipeline)
        {
            Rr_BindGraphicsPipeline(GraphicsNode, gUIContext->LinearPipeline);
        }

        if (ClipRect->Image != *BoundImage)
        {
            Rr_BindCombinedImage2DSampler(
                GraphicsNode,
                ClipRect->Image,
                gUIContext->Sampler,
                0,
                1);
            *BoundImage = ClipRect->Image;
        }

        Rr_SetScissor(GraphicsNode, &IntRect);

        Rr_DrawIndexed(
            GraphicsNode,
            ClipRect->IndexCount,
            1,
            ClipRect->FirstIndex,
            0,
            0);

        if (ClipRect->ForceLinearPipeline)
        {
            Rr_BindGraphicsPipeline(
                GraphicsNode,
                gUIContext->SRGBSwapchain ? gUIContext->SRGBPipeline
                                          : gUIContext->LinearPipeline);
        }
    }
}

void Rr_EndUI(void)
{
    assert(
        gUIContext->HashStack.Count == 0 &&
        "ID/Hash stack is not empty; did you forget to call Rr_UIPopID()?");

    Rr_UIAssertNoWindow();

    if (gUIContext->ActiveLayouts.Count > 0)
    {
        Rr_BeginFrameSection("Rr.UI.DrawWindows");

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

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
            gUIContext->SRGBSwapchain ? gUIContext->SRGBPipeline
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

        qsort(
            gUIContext->ActiveLayouts.Data,
            gUIContext->ActiveLayouts.Count,
            sizeof(Rr_UILayout *),
            Rr_UIWindowSort);

        gUIContext->HighestWindow =
            RR_LAST_ARRAY_ELEMENT(&gUIContext->ActiveLayouts)->Window;

        Rr_Image2D *BoundImage = NULL;

        for (size_t Index = 0; Index < gUIContext->ActiveLayouts.Count; ++Index)
        {
            Rr_UILayout *Layout = gUIContext->ActiveLayouts.Data[Index];
            Rr_UIWindow *Window = Layout->Window;
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
                Rr_UIDrawWindow(Layout->ClipRects, GraphicsNode, &BoundImage);
            }
            Window->ShownAtLeastOnce = true;
        }

        Rr_EndGraphLabel(Rr_GetGraph(), "Rr.UI");

        Rr_EndFrameSection("Rr.UI.DrawWindows");
    }

    gUIContext->ClickConsumed = false;
    gUIContext->DragConsumed = false;
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
    Rr_SetCursor(gUIContext->CursorType);
    gUIContext->CursorType = RR_CURSOR_TYPE_NORMAL;
    RR_ZERO(gUIContext->TextInputEvents);
    RR_ZERO(gUIContext->KeyboardInputEvents);
}

float Rr_UICurrentFontSize(void)
{
    return Rr_UICurrentFont()->Size;
}

float Rr_UICurrentLineHeight(void)
{
    return Rr_UICurrentFont()->LineHeight;
}

static inline void Rr_UIDebugOverlayArena(Rr_Arena *Arena, char const *Name)
{
    Rr_UITextF(
        "%s:\n"
        "  Commited: %zuKiB\n"
        "  Reserved: %zuMiB",
        Name,
        Arena->Commited / 1024,
        Arena->Reserved / 1024 / 1024);
}

void Rr_UIDebugOverlay(void)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_UIBeginWindowEx("DebugOverlayTabs", NULL, RR_UI_WINDOW_FLAGS_TABS_BIT);
    {
        Rr_UIBeginWindowEx("General", 0, RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT);
        {
            Rr_IntVec2 WindowSize = Rr_GetWindowSize();
            Rr_MouseButtonFlags MouseState = Rr_GetMouseState();
            Rr_Vec2 MousePosition = Rr_GetMousePosition();
            Rr_Vec2 MouseDelta = Rr_GetMousePositionDelta();
            Rr_UITextF(
                "Time: %.2f\n"
                "Window Size: %dx%d\n"
                "Mouse State: %d:%d:%d:%d:%d\n"
                "Mouse Position: %.2f %.2f\n"
                "Mouse Delta: %.2f %.2f\n"
                "Pressed Keys: %d\n"
                "Keymod: %d:%d:%d:%d",
                Rr_GetTimeSeconds(),
                WindowSize.X,
                WindowSize.Y,
                (bool)(MouseState & RR_MOUSE_BUTTON_LEFT_BIT),
                (bool)(MouseState & RR_MOUSE_BUTTON_MIDDLE_BIT),
                (bool)(MouseState & RR_MOUSE_BUTTON_RIGHT_BIT),
                (bool)(MouseState & RR_MOUSE_BUTTON_X1_BIT),
                (bool)(MouseState & RR_MOUSE_BUTTON_X2_BIT),
                MousePosition.X,
                MousePosition.Y,
                MouseDelta.X,
                MouseDelta.Y,
                gPlatform.PressedKeyCount,
                (bool)(gPlatform.Keymod & RR_KEYMOD_CTRL),
                (bool)(gPlatform.Keymod & RR_KEYMOD_SHIFT),
                (bool)(gPlatform.Keymod & RR_KEYMOD_ALT),
                (bool)(gPlatform.Keymod & RR_KEYMOD_SUPER));
            Rr_UISeparator();
            uint32_t PresentModeCount;
            Rr_PresentMode *PresentModes =
                Rr_GetAvailablePresentModes(&PresentModeCount);
            char const **PresentModeStrings = Rr_Alloc(
                PresentModeCount * sizeof(char const *),
                Scratch.Arena);
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
                &Rr_GetFrameTime()->TargetFrameRate);
            Rr_UIInputUnsignedInt(
                "Background Frame Rate",
                &Rr_GetFrameTime()->BackgroundFrameRate);
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
                (double)(Rr_GetFrameSectionTicks("Rr.MainLoop") * 1000) /
                (double)Rr_GetPerformanceFrequency();
            double FrameGraphMS =
                (double)(Rr_GetFrameSectionTicks("Rr.FrameGraph") * 1000) /
                (double)Rr_GetPerformanceFrequency();

            Rr_UITextF(
                "Main Loop: %.3fms\n"
                "Frame Graph: %.3fms",
                MainLoopMS,
                FrameGraphMS);

            if (gRenderer->MainQueue.TimestampsEnabled)
            {
                Rr_UITextF("GPU: %.3fms", gRenderer->LastFrameMS);
            }
            else
            {
                Rr_UITextF("GPU timestamps not supported!");
            }

            Rr_UIBeginHorizontal();

            if (Rr_UIButton("Toggle Fullscreen"))
            {
                Rr_SetWindowFullscreen(!Rr_IsWindowFullscreen());
            }

            if (Rr_UIButton("Quit"))
            {
                Rr_Quit();
            }

            Rr_UIEndHorizontal();
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindowEx("UI", 0, RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT);
        {
            Rr_UITextF(
                "Vertices Capacity: %zu\n"
                "Indices Capacity: %zu",
                gUIContext->Vertices.Capacity,
                gUIContext->Indices.Capacity);

            double DrawWindowsMS =
                (double)(Rr_GetFrameSectionTicks("Rr.UI.DrawWindows") * 1000) /
                (double)Rr_GetPerformanceFrequency();
            double DrawTextMS =
                (double)(Rr_GetFrameSectionTicks("Rr.UI.DrawText") * 1000) /
                (double)Rr_GetPerformanceFrequency();
            double DrawInputTextMS =
                (double)(Rr_GetFrameSectionTicks("Rr.UI.DrawInputText") *
                         1000) /
                (double)Rr_GetPerformanceFrequency();

            Rr_UITextF(
                "DrawWindows: %.3fms\n"
                "DrawText: %.3fms\n"
                "DrawInputText: %.3fms",
                DrawWindowsMS,
                DrawTextMS,
                DrawInputTextMS);

            Rr_UITextF(
                "TextInputCursorBegin: %zu\n"
                "TextInputCursorEnd: %zu\n"
                "TextInputCodepointMaxCol: %zu",
                gUIContext->TextInputCursorBegin,
                gUIContext->TextInputCursorEnd,
                gUIContext->TextInputCursorCodepointMaxCol);

            Rr_UITextF(
                "Hovered Window: %s\n"
                "Click Parent: %s\n"
                "Active Windows: %zu\n"
                "Popup Window Open: %b",
                gUIContext->HoveredWindow ? gUIContext->HoveredWindow->Title
                                          : NULL,
                gUIContext->ClickParent ? gUIContext->ClickParent->Title : NULL,
                gUIContext->ActiveLayouts.Count,
                gUIContext->PopupWindow.Open);

            Rr_UITextF(
                "Drag Parent: %s\n"
                "Drag Hash: %zu\n"
                "Drag Value Start: %.2f %.2f %.2f %.2f",
                gUIContext->DragParent ? gUIContext->DragParent->Title : NULL,
                gUIContext->DragParent ? gUIContext->DragHash : 0,
                gUIContext->DragParent ? gUIContext->DragValueStart.Offset.X
                                       : 0,
                gUIContext->DragParent ? gUIContext->DragValueStart.Offset.Y
                                       : 0,
                gUIContext->DragParent ? gUIContext->DragValueStart.Extent.X
                                       : 0,
                gUIContext->DragParent ? gUIContext->DragValueStart.Extent.Y
                                       : 0);

            Rr_UICheckbox("Visualize Advances", &gUIContext->VisualizeAdvances);
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindowEx("Memory", 0, RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT);
        {
            Rr_ThreadContext *ThreadContext = Rr_GetThreadContext();
            Rr_UIDebugOverlayArena(
                ThreadContext->PermanentArena,
                "Main Thread Permanent");
            Rr_UIDebugOverlayArena(
                ThreadContext->ScratchArenas[0],
                "Main Thread Scratch#0");
            Rr_UIDebugOverlayArena(
                ThreadContext->ScratchArenas[1],
                "Main Thread Scratch#1");
            for (uint32_t Index = 0; Index < RR_FRAME_OVERLAP; ++Index)
            {
                Rr_Frame *Frame = gRenderer->Frames + Index;
                char FrameString[64];
                sprintf(FrameString, "Frame#%d", Index);
                Rr_UIDebugOverlayArena(Frame->Arena, FrameString);
            }
            Rr_UIDebugOverlayArena(gUIContext->Arena, "UI");
        }
        Rr_UIEndWindow();

        Rr_UIBeginWindowEx("Renderer", 0, RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT);
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
                "DescriptorPools: %d",
                gRenderer->DescriptorPoolListCount);
            Rr_UITextF(
                "PipelineLayouts: %zu/%zu",
                gRenderer->PipelineLayoutStorage.Hive.Count,
                gRenderer->PipelineLayoutStorage.Hive.Capacity);
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
                gRenderer->RenderPassMap.Count,
                gRenderer->RenderPassMap.Capacity);
            Rr_UITextF(
                "Framebuffers: %zu/%zu",
                gRenderer->FramebufferMap.Count,
                gRenderer->FramebufferMap.Capacity);
            Rr_UITextF(
                "SwapchainImages: %zu",
                gRenderer->SwapchainImages.Count);
        }
        Rr_UIEndWindow();
    }
    Rr_UIEndWindow();

    Rr_DestroyScratch(Scratch);
}
