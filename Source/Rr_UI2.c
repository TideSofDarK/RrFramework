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

#include "Rr_UI2.h"

#include "Rr_BuiltinAssets.inc"

#ifdef RR_GNU_OR_CLANG
#pragma GCC diagnostic ignored "-Wmissing-format-attribute"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_UI
#include "Rr_LogMacro.h"

#include "Rr_Arena.h"
#include "Rr_Hash.h"
#include "Rr_Memory.h"
#include "Rr_RHI_Vulkan.h"

#include <Rr/Rr_Graph.h>
#include <Rr/Rr_Platform.h>
#include <Rr/Rr_System.h>
#include <Rr/Rr_Utility.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb/stb_truetype.h>

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#define RR_UI_FONT_OVERSAMPLING  2
#define RR_UI_SCALAR_BUFFER_SIZE 32
#define RR_UI_MIN_FONT_SIZE      6.0f
#define RR_UI_MAX_FONT_SIZE      48.0f

/* TODO: DPI-based values. */

static float const RR_UI_BEVEL_THICKNESS = 2.0f;
static float const RR_UI_WINDOW_BORDER = 1.0f;
static float const RR_UI_WINDOW_HANDLES = 4.0f;
static float const RR_UI_WINDOW_CONTENTS_PADDING = 4.0f;

typedef struct Rr_UIUniform Rr_UIUniform;
struct Rr_UIUniform
{
    Rr_Vec4 CanvasExtent;
    uint32_t IndexCount;
    uint32_t Reserved1;
    uint32_t Reserved2;
    uint32_t Reserved3;
};

typedef struct Rr_UIMouseButton Rr_UIMouseButton;
struct Rr_UIMouseButton
{
    Rr_UIItem *Item;
    Rr_UIItem *ReleasedItem;
    Rr_Vec2 ItemDragStart;
    uint32_t Clicks;
    bool Down;
    bool Held;
    bool Up;
};

typedef struct Rr_UIRootTrie Rr_UIRootTrie;
struct Rr_UIRootTrie
{
    Rr_UIHash Hash;
    Rr_UIItem Root;
    Rr_UIRootTrie *Children[4];
};

typedef struct Rr_UI Rr_UI;
struct Rr_UI
{
    Rr_UIColors Colors;
    // Rr_UI2Style Style;

    float DisplayScale;
    // Rr_UI2Style CalculatedStyle;

    RR_ARRAY(Rr_UIFont *) Fonts;
    Rr_UIFont *DefaultFont;

    bool SRGBSwapchain;
    Rr_GraphicsPipeline *DefaultPipeline;
    // Rr_GraphicsPipeline *LinearPipeline;
    // Rr_GraphicsPipeline *SRGBPipeline;
    Rr_Buffer *UniformBuffer;
    Rr_Buffer *ClipRectStagingBuffer;
    uint32_t ClipRectCount;
    Rr_Rect *ClipRects;
    Rr_Buffer *VertexStagingBuffer;
    uint32_t VertexCount;
    Rr_UIVertex *Vertices;
    Rr_Buffer *IndexStagingBuffer;
    uint32_t IndexCount;
    Rr_UIIndex *Indices;
    Rr_Buffer *DeviceLocalBuffer;
    Rr_Sampler *NearestSampler;
    Rr_Sampler *LinearSampler;

    Rr_UIMouseButton LeftMouseButton;
    Rr_Vec2 MouseWheel;
    Rr_Vec2 MousePosition;
    bool MouseMoved;

    Rr_UIItem *HoveredItem;
    Rr_UIItem *FocusedNonPopupRoot;
    Rr_UIItem *FocusedItem;
    size_t TextInputCursorBegin;
    size_t TextInputCursorEnd;
    size_t TextInputCursorCodepointMaxCol;
    size_t TextInputCursorBlinkTimeNS;

    RR_ARRAY(char const *) TextInputEvents;
    RR_ARRAY(Rr_KeyEvent) KeyboardInputEvents;

    Rr_UIRootTrie *Roots;

    Rr_UIItem *ImplicitItem;

    RR_ARRAY(Rr_UIPopupInfo) PopupStack;

    RR_ARRAY(Rr_UIItem *) ParentStack;

    /* Toolkit */

    RR_ARRAY(Rr_UIWindow *) Windows;
    Rr_Rect DragValueStart;

    uint32_t BuildIndex;

    Rr_Arena *FrameArena;
    Rr_Arena *Arena;
};

static Rr_UI *gUI;

typedef struct Rr_UIRange Rr_UIRange;
struct Rr_UIRange
{
    uint32_t First;
    uint32_t Last;
};

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
    Rr_IntVec2 ImageExtent;
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

static inline Rr_Vec2 Rr_UICalculateTextExtent(
    Rr_UIFont *Font,
    size_t TextLength,
    char const *Text)
{
    float LineHeight = Font->LineHeight;
    float MaxX = 0.0f;
    Rr_Vec2 Offset = Rr_V2F(0.0f);

    Rr_UTF8Decoder Decoder = { .CString = Text };
    while (Decoder.CStringParserIndex < TextLength)
    {
        Rr_UTF8Decode(&Decoder);
        uint32_t Codepoint = Decoder.Codepoint;

        if (Codepoint == '\0')
        {
            break;
        }

        Rr_UIGlyph *Glyph = Rr_UIGetGlyphForCodepoint(Font, Codepoint);
        if (!Glyph)
        {
            /* TODO: Handle missing glyphs. */

            Glyph = Rr_UIGetGlyphForCodepoint(Font, ' ');
        }

        if (Codepoint == '\n')
        {
            Offset.X = 0.0f;
            Offset.Y += LineHeight;

            continue;
        }

        if (Codepoint == ' ')
        {
            Offset.X += Glyph->XAdvance;

            continue;
        }

        Offset.X += Glyph->XAdvance;
        MaxX = RR_MAX(MaxX, Offset.X);
    }

    return Rr_RoundV2(Rr_V2(MaxX, Offset.Y + LineHeight));
}

static inline Rr_Vec2 Rr_UICalculateInputTextExtent(
    Rr_UIFont *Font,
    size_t TextLength,
    char const *Text)
{
    float LineHeight = Font->LineHeight;
    float MaxX = 0.0f;
    Rr_Vec2 Offset = Rr_V2F(0.0f);

    Rr_UTF8Decoder Decoder = { .CString = Text };
    while (Decoder.CStringParserIndex < TextLength)
    {
        Rr_UTF8Decode(&Decoder);
        uint32_t Codepoint = Decoder.Codepoint;

        if (Codepoint == '\0')
        {
            break;
        }

        bool DrawLineBreakAsSpace = false;
        if (Codepoint == '\n')
        {
            Codepoint = ' ';
            DrawLineBreakAsSpace = true;
        }

        Rr_UIGlyph *Glyph = Rr_UIGetGlyphForCodepoint(Font, Codepoint);
        if (!Glyph)
        {
            /* TODO: Handle missing glyphs. */

            Glyph = Rr_UIGetGlyphForCodepoint(Font, ' ');
        }

        Offset.X += Glyph->XAdvance;
        MaxX = RR_MAX(MaxX, Offset.X);
        if (DrawLineBreakAsSpace)
        {
            Offset.X = 0.0f;
            Offset.Y += LineHeight;
        }
    }

    return Rr_RoundV2(Rr_V2(MaxX, Offset.Y + LineHeight));
}

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

    Rr_UIFont *Font = Rr_AllocNoZero(AllocationSize, gUI->Arena);
    *RR_PUSH_INTO_ARRAY(&gUI->Fonts, gUI->Arena) = Font;
    Font->AllocationSize = AllocationSize;
    Font->RangeCount = CodepointRangeCount;
    Font->Ranges = (Rr_UIFontRange *)(Font + 1);
    Rr_SetNextObjectNameF("Rr.UI.Font#%d", FontIndex);
    Font->ImageExtent = ATLAS_EXTENT;
    Font->Image = Rr_CreateImage2D(
        ATLAS_EXTENT,
        RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
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

        uint32_t NumChars = CodepointRange->Last - CodepointRange->First;

        FontRange->First = CodepointRange->First;
        FontRange->Last = CodepointRange->Last;
        FontRange->Glyphs = (void *)((char *)Font + GlyphsOffset);

        PackRanges[Index] = (stbtt_pack_range){
            .first_unicode_codepoint_in_range = (int)CodepointRange->First,
            .num_chars = (int)NumChars,
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

    size_t AtlasBufferSize =
        (size_t)(Font->ImageExtent.X * Font->ImageExtent.Y) * sizeof(uint32_t);
    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        AtlasBufferSize,
        RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT);
    Rr_ReleaseBuffer(StagingBuffer);
    uint32_t *StagingData = Rr_MapBuffer(StagingBuffer);
    for (int32_t Y = 0; Y < Font->ImageExtent.Y; ++Y)
    {
        for (int32_t X = 0; X < Font->ImageExtent.X; ++X)
        {
            uint8_t Grayscale = GrayscaleBuffer[Y * Font->ImageExtent.X + X];
            StagingData[Y * Font->ImageExtent.X + X] =
                ((uint32_t)Grayscale << 24) | 0x00FFFFFF;
        }
    }
    StagingData[0] = 0xFFFFFFFF; /* Opaque pixel at [0,0]. */
    Rr_FlushBufferRange(StagingBuffer, 0, AtlasBufferSize);
    Rr_UnmapBuffer(StagingBuffer);
    Rr_CopyBufferToImage2D(
        Rr_GetGraph(),
        StagingBuffer,
        0,
        Font->ImageExtent,
        Font->Image,
        0);

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
    Rr_ReleaseImage(Font->Image);
}

static inline Rr_UIHash Rr_UIGetHash(
    size_t Size,
    void const *Data,
    Rr_UIHash Seed)
{
    return Rr_Hash64WithSeed(Size, Data, ~Seed);
}

static inline Rr_UIHash Rr_UIGetNameHash(
    char const *Name,
    Rr_UIHash Seed,
    size_t *OutLength)
{
    size_t TotalLength = strlen(Name);
    char const *ExplicitID = strstr(Name, "###");
    if (ExplicitID)
    {
        ExplicitID += 3;
        assert(
            (ExplicitID < (Name + TotalLength)) &&
            "Empty ID after ### sentinel!");

        size_t IDLength = TotalLength - (size_t)(ExplicitID - Name);

        if (OutLength)
        {
            *OutLength = TotalLength - IDLength - 3;
        }

        return Rr_UIGetHash(IDLength, ExplicitID, Seed);
    }
    else
    {
        if (OutLength)
        {
            *OutLength = TotalLength;
        }

        return Rr_UIGetHash(TotalLength, Name, Seed);
    }
}

static inline uint32_t Rr_UIPushClipRect(Rr_Rect Rect)
{
    uint32_t Index = gUI->ClipRectCount;
    gUI->ClipRects[Index] = Rect;
    gUI->ClipRectCount++;
    assert(
        gUI->ClipRectCount <
        (Rr_GetBufferSize(gUI->ClipRectStagingBuffer) / sizeof(Rr_Vec4)));

    return Index;
}

static inline Rr_UIItem *Rr_UIRoot(Rr_UIItem *Item)
{
    if (!Item)
    {
        return NULL;
    }

    while (Item->Parent)
    {
        Item = Item->Parent;
    }

    return Item;
}

static inline Rr_UIItem *Rr_UINonPopupRoot(Rr_UIItem *Item)
{
    Rr_UIItem *Root = Rr_UIRoot(Item);
    for (size_t Index = 0; Index < gUI->PopupStack.Count; ++Index)
    {
        Rr_UIItem *PopupParent = gUI->PopupStack.Data[Index].Parent;
        Rr_UIItem *Popup = Rr_UIGetPopup(PopupParent);
        if (Root == Popup)
        {
            return Rr_UIRoot(gUI->PopupStack.Data[0].Parent);
        }
    }

    return Root;
}

static inline bool Rr_UIChildOfPopup(Rr_UIItem *Item)
{
    Rr_UIItem *Root = Rr_UIRoot(Item);

    return Root != Rr_UINonPopupRoot(Root);
}

static inline float Rr_UIMaxScroll(Rr_UIItem *Item, Rr_UIAxis Axis)
{
    float ChildrenExtent = Item->ChildrenExtent.Elements[Axis];
    float TextExtent = Item->TextExtent.Elements[Axis];
    float MaxExtent = RR_MAX(ChildrenExtent, TextExtent);
    float MaxScroll = MaxExtent - Item->Extent.Elements[Axis];
    MaxScroll += Item->Padding.Elements[Axis] * 2.0f;
    MaxScroll = RR_MAX(0.0f, MaxScroll);

    return MaxScroll;
}

static inline void Rr_UIPropagateScroll(Rr_UIItem *Item, Rr_UIAxis Axis)
{
    float Amount = gUI->MouseWheel.Elements[Axis];
    if (Amount == 0.0f)
    {
        return;
    }
    if (Item->Scrollable[Axis])
    {
        float MaxScroll = Rr_UIMaxScroll(Item, Axis);
        float Scroll = Item->Scroll.Elements[Axis];
        if (MaxScroll > 0.0f)
        {
            if (Amount != 0.0f)
            {
                Scroll += Amount * gUI->DefaultFont->LineHeight;
            }
            Scroll = RR_CLAMP(0.0f, Scroll, MaxScroll);
            Item->Scroll.Elements[Axis] = Scroll;

            return;
        }
    }

    Item->Scroll.Elements[Axis] = 0.0f;

    if (Item->Parent)
    {
        Rr_UIPropagateScroll(Item->Parent, Axis);
    }
}

static inline void Rr_UIUpdateMouseResponse(Rr_UIItem *Item)
{
    Rr_UIMouseButton *Button = &gUI->LeftMouseButton;

    /* NOTE: TextCursor is written in Rr_UIDrawItem()! */

    Item->HasFocus = gUI->FocusedItem == Item;
    Item->Hovering = gUI->HoveredItem == Item;
    Item->Dragging = Button->Item == Item;
    Item->DragDelta = Rr_V2F(0.0f);
    Item->Clicked = 0;
    Item->Pressed = 0;
    Item->Released = Button->ReleasedItem == Item;
    Item->Dragged = gUI->MouseMoved && (Item->Dragging || Item->Released);

    if (Item->Dragging || Item->Dragged)
    {
        Item->DragDelta = Rr_SubV2(gUI->MousePosition, Button->ItemDragStart);
    }

    if (Item->Dragging)
    {
        if (Button->Down && Item->MouseClickable)
        {
            if (Button->Clicks > 1)
            {
                Item->Clicked = (uint8_t)Button->Clicks;
            }
            Item->Pressed = (uint8_t)Button->Clicks;
        }
    }

    if (Item->Released && Item->Hovering && Button->Clicks == 1)
    {
        Item->Clicked = 1;
    }
}

static inline Rr_UIItem *Rr_UIResetItem(Rr_UIItem *Item)
{
    if (Item->StartBuildIndex == gUI->BuildIndex)
    {
        return Item;
    }

    Item->StartBuildIndex = gUI->BuildIndex;
    // Item->Parent = NULL;
    Item->First = NULL;
    Item->Last = NULL;
    Item->Next = NULL;

    Rr_UIUpdateMouseResponse(Item);

    return Item;
}

static inline Rr_UIItem *Rr_UILookupItem(Rr_UIItem **ItemRef, Rr_UIHash Hash)
{
    for (; *ItemRef; Hash <<= 2)
    {
        if (Hash == (*ItemRef)->Hash)
        {
            return Rr_UIResetItem(*ItemRef);
        }
        static int const SHIFT = (sizeof(Rr_UIHash) * CHAR_BIT) - 2;
        ItemRef = &(*ItemRef)->HashChildren[Hash >> SHIFT];
    }

    *ItemRef = Rr_Alloc(sizeof(Rr_UIItem), gUI->Arena);
    (*ItemRef)->Hash = Hash;

    return Rr_UIResetItem(*ItemRef);
}

static inline Rr_UIItem *Rr_UILookupRoot(void *Key)
{
    Rr_UIRootTrie **Ref = &gUI->Roots;
    uintptr_t Hash = (uintptr_t)Key;
    for (; *Ref; Hash <<= 2)
    {
        if (Hash == (*Ref)->Hash)
        {
            return Rr_UIResetItem(&(*Ref)->Root);
        }
        static int const SHIFT = (sizeof(uintptr_t) * CHAR_BIT) - 2;
        Ref = &(*Ref)->Children[Hash >> SHIFT];
    }

    *Ref = Rr_Alloc(sizeof(Rr_UIRootTrie), gUI->Arena);
    (*Ref)->Hash = Hash;
    (*Ref)->Root.Hash = Hash;

    return Rr_UIResetItem(&(*Ref)->Root);
}

static inline void Rr_UIInitPipelines(void)
{
    Rr_ImageFormat Format = Rr_GetImageFormat(Rr_GetSwapchainImage());
    bool SRGBSwapchain = Rr_IsSRGBFormat(Format);
    if (gUI->DefaultPipeline && SRGBSwapchain == gUI->SRGBSwapchain)
    {
        return;
    }
    gUI->SRGBSwapchain = SRGBSwapchain;

    Rr_ColorTargetInfo ColorTargets[] = {
        {
            .Format = Format,
            .Blend = Rr_AlphaBlend(),
        },
    };

    // Rr_VertexInputAttribute VertexInputAttributes[] = {
    //     {
    //         .Location = 0,
    //         .Format = RR_FORMAT_FLOAT4,
    //     },
    //     {
    //         .Location = 1,
    //         .Format = RR_FORMAT_FLOAT2,
    //     },
    //     {
    //         .Location = 2,
    //         .Format = RR_FORMAT_FLOAT2,
    //     },
    //     {
    //         .Location = 3,
    //         .Format = RR_FORMAT_INT4,
    //     },
    // };

    // Rr_VertexInputBinding VertexInputBinding = {
    //     .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
    //     .AttributeCount = RR_ARRAY_COUNT(VertexInputAttributes),
    //     .Attributes = VertexInputAttributes,
    // };

    Rr_Asset VertexShader = Rr_LoadAsset(RR_BUILTIN_UI2_VERT_SPV);
    Rr_Asset FragmentShader = Rr_LoadAsset(RR_BUILTIN_UI2_FRAG_SPV);

    // Rr_PipelineSpecialization Specializations[1] = {
    //     {
    //         .ConstantID = 0,
    //         .Size = sizeof(uint32_t),
    //     },
    // };

    Rr_ShaderInfo VertexShaderInfo = {
        .SPVSize = VertexShader.Size,
        .SPVData = VertexShader.Data,
        // .SpecializationCount = RR_ARRAY_COUNT(Specializations),
        // .Specializations = Specializations,
    };

    Rr_ShaderInfo FragmentShaderInfo = {
        .SPVSize = FragmentShader.Size,
        .SPVData = FragmentShader.Data,
        // .SpecializationCount = RR_ARRAY_COUNT(Specializations),
        // .Specializations = Specializations,
    };

    Rr_GraphicsPipelineCreateInfo PipelineInfo = {
        .VertexShaderInfo = &VertexShaderInfo,
        .FragmentShaderInfo = &FragmentShaderInfo,
        .ColorTargetCount = RR_ARRAY_COUNT(ColorTargets),
        .ColorTargets = ColorTargets,
        // .VertexInputBindingCount = 1,
        // .VertexInputBindings = &VertexInputBinding,
    };

    // uint32_t const DONT_CONVERT_TO_SRGB = 0;
    // ColorTargets[0].Blend = Rr_AlphaBlend();
    // Specializations[0].Data = &DONT_CONVERT_TO_SRGB;
    // Rr_SetNextObjectName("Rr.UI.LinearPipeline");
    // if (gUI->LinearPipeline)
    // {
    //     Rr_ReleaseGraphicsPipeline(gUI->LinearPipeline);
    // }
    // gUI->LinearPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    // uint32_t const CONVERT_TO_SRGB = 1;
    // ColorTargets[0].Blend = Rr_PremultipliedAlphaBlend();
    // // ColorTargets[0].Blend = Rr_AlphaBlend();
    // Specializations[0].Data = &CONVERT_TO_SRGB;
    // Rr_SetNextObjectName("Rr.UI.SRGBPipeline");
    // if (gUI->SRGBPipeline)
    // {
    //     Rr_ReleaseGraphicsPipeline(gUI->SRGBPipeline);
    // }
    // gUI->SRGBPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    if (gUI->DefaultPipeline)
    {
        Rr_ReleaseGraphicsPipeline(gUI->DefaultPipeline);
    }
    Rr_SetNextObjectName("Rr.UI.SRGBPipeline");
    gUI->DefaultPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
}

void Rr_InitUI2(void)
{
    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gUI = Rr_Alloc(sizeof(Rr_UI), Arena);
    gUI->Arena = Arena;

    float DefaultFontSize = 10.0f * Rr_GetDisplayScale();
    Rr_Asset FontAsset = Rr_LoadAsset(RR_BUILTIN_SOURCESERIF4_TTF);
    // gUI->DefaultFont =
    //     Rr_UICreateFont(FontAsset.Size, FontAsset.Data, DefaultFontSize);
    gUI->DefaultFont = Rr_UICreateFontEx(
        FontAsset.Size,
        FontAsset.Data,
        DefaultFontSize,
        RR_ARRAY_COUNT(RR_UI_DEFAULT_RANGES),
        RR_UI_DEFAULT_RANGES);

    Rr_UISetDefaultColors();
    Rr_UIInitPipelines();

    Rr_SetNextObjectName("Rr.UI.UniformBuffer");
    gUI->UniformBuffer = Rr_CreateBuffer(
        256,
        RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);

    Rr_SetNextObjectName("Rr.UI.ClipRectStagingBuffer");
    gUI->ClipRectStagingBuffer = Rr_CreateBuffer(
        RR_MEBIBYTES(4),
        RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_MAPPED_BIT);

    Rr_SetNextObjectName("Rr.UI.VertexStagingBuffer");
    gUI->VertexStagingBuffer = Rr_CreateBuffer(
        RR_MEBIBYTES(4),
        RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_MAPPED_BIT);

    Rr_SetNextObjectName("Rr.UI.IndexStagingBuffer");
    gUI->IndexStagingBuffer = Rr_CreateBuffer(
        RR_MEBIBYTES(4),
        RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
            RR_BUFFER_FLAGS_MAPPED_BIT);

    Rr_SetNextObjectName("Rr.UI.DeviceLocalBuffer");
    gUI->DeviceLocalBuffer = Rr_CreateBuffer(
        RR_MEBIBYTES(8),
        RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_VERTEX_BIT |
            RR_BUFFER_FLAGS_INDEX_BIT);

    Rr_SetNextObjectName("Rr.UI.NearestSampler");
    gUI->NearestSampler = Rr_CreateSampler(&(Rr_SamplerInfo){
        .MinFilter = RR_FILTER_LINEAR,
        .MagFilter = RR_FILTER_LINEAR,
        .AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    });

    Rr_SetNextObjectName("Rr.UI.LinearSampler");
    gUI->LinearSampler = Rr_CreateSampler(&(Rr_SamplerInfo){
        .MinFilter = RR_FILTER_LINEAR,
        .MagFilter = RR_FILTER_LINEAR,
        .AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    });
}

void Rr_CleanupUI2(void)
{
    Rr_ReleaseBuffer(gUI->UniformBuffer);
    Rr_ReleaseBuffer(gUI->ClipRectStagingBuffer);
    Rr_ReleaseBuffer(gUI->VertexStagingBuffer);
    Rr_ReleaseBuffer(gUI->IndexStagingBuffer);
    Rr_ReleaseBuffer(gUI->DeviceLocalBuffer);
    Rr_ReleaseSampler(gUI->LinearSampler);
    Rr_ReleaseSampler(gUI->NearestSampler);
    Rr_ReleaseGraphicsPipeline(gUI->DefaultPipeline);

    Rr_UIReleaseFont(gUI->DefaultFont);

    Rr_DestroyArena(gUI->Arena);

    gUI = NULL;
}

static inline Rr_UIWindow *Rr_UIIsWindowRoot(Rr_UIItem *Root)
{
    for (size_t Index = 0; Index < gUI->Windows.Count; ++Index)
    {
        Rr_UIWindow *Window = gUI->Windows.Data[Index];

        if (Root == Rr_UILookupRoot(Window))
        {
            return Window;
        }
    }

    return NULL;
}

static inline void Rr_UIHandleLeftMouseButtonDown(
    Rr_MouseButtonEvent const *Event)
{
    gUI->LeftMouseButton.Clicks = Event->Clicks;
    gUI->LeftMouseButton.Down = true;
    gUI->LeftMouseButton.Up = false;
    gUI->LeftMouseButton.Held = true;

    /* Update focused/dragged item. */

    if (gUI->HoveredItem && gUI->HoveredItem->MouseClickable)
    {
        gUI->LeftMouseButton.ItemDragStart = Event->Position;
        gUI->LeftMouseButton.Item = gUI->HoveredItem;
        gUI->FocusedItem = gUI->HoveredItem;
    }
    else
    {
        gUI->LeftMouseButton.Item = NULL;
        gUI->FocusedItem = NULL;
    }

    gUI->FocusedNonPopupRoot = Rr_UINonPopupRoot(gUI->HoveredItem);

    /* Close popups if clicked outside. */

    if (gUI->PopupStack.Count)
    {
        bool ChildOfPopup = Rr_UIChildOfPopup(gUI->HoveredItem);
        if (!ChildOfPopup)
        {
            Rr_UIClosePopups();
        }
    }
}

static inline void Rr_UIHandleLeftMouseButtonUp(void)
{
    gUI->LeftMouseButton.Down = false;
    gUI->LeftMouseButton.Up = true;
    gUI->LeftMouseButton.Held = false;
    gUI->LeftMouseButton.ReleasedItem = gUI->LeftMouseButton.Item;
    gUI->LeftMouseButton.Item = NULL;
}

void Rr_ProcessUI2Event(Rr_Event const *Event)
{
    switch (Event->Type)
    {
        case RR_EVENT_TYPE_FOCUS:
        {
            if (!Event->Focus.Focused)
            {
                gUI->FocusedItem = NULL;
                RR_ZERO(gUI->LeftMouseButton);
                RR_ZERO(gUI->KeyboardInputEvents);
            }
        }
        break;
        case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
        {
            Rr_UIInitPipelines();
        }
        break;
        case RR_EVENT_TYPE_TEXT_INPUT:
        {
            size_t Length = Event->Text.Length;
            char *Text = Rr_AllocNoZero(Length + 1, gUI->FrameArena);
            memcpy(Text, Event->Text.CString, Length + 1);
            *RR_PUSH_INTO_ARRAY(&gUI->TextInputEvents, gUI->FrameArena) = Text;
        }
        break;
        case RR_EVENT_TYPE_KEY_DOWN:
        case RR_EVENT_TYPE_KEY_REPEAT:
        case RR_EVENT_TYPE_KEY_UP:
        {
            *RR_PUSH_INTO_ARRAY(&gUI->KeyboardInputEvents, gUI->FrameArena) =
                Event->Key;
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_DOWN:
        {
            if (Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                Rr_UIHandleLeftMouseButtonDown(&Event->MouseButton);
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_BUTTON_UP:
        {
            if (Event->MouseButton.Button == RR_MOUSE_BUTTON_LEFT)
            {
                Rr_UIHandleLeftMouseButtonUp();
            }
        }
        break;
        case RR_EVENT_TYPE_MOUSE_MOTION:
        {
            gUI->MouseMoved = true;
            gUI->MousePosition = Event->MouseMotion.Position;
        }
        break;
        case RR_EVENT_TYPE_MOUSE_WHEEL:
        {
            gUI->MouseWheel = Rr_SubV2(gUI->MouseWheel, Event->Wheel.Amount);

            if (gUI->HoveredItem)
            {
                for (Rr_UIAxis Axis = 0; Axis < 2; ++Axis)
                {
                    Rr_UIPropagateScroll(gUI->HoveredItem, Axis);
                }
            }
        }
        break;
        default:
            break;
    }
}

static bool Rr_UINullIfOutdated(Rr_UIItem **ItemRef)
{
    Rr_UIItem *Item = *ItemRef;
    if (Item && Item->EndBuildIndex != gUI->BuildIndex)
    {
        *ItemRef = NULL;

        return true;
    }

    return false;
}

void Rr_BeginUI2(void)
{
    Rr_Arena *FrameArena = gRHI->Frames[gRHI->FrameIndex].Arena;
    gUI->FrameArena = FrameArena;

    gUI->DisplayScale = Rr_GetDisplayScale();

    Rr_UINullIfOutdated(&gUI->HoveredItem);
    Rr_UINullIfOutdated(&gUI->FocusedNonPopupRoot);
    Rr_UINullIfOutdated(&gUI->FocusedItem);
    Rr_UINullIfOutdated(&gUI->LeftMouseButton.Item);
    for (size_t Index = 0; Index < gUI->PopupStack.Count; ++Index)
    {
        if (Rr_UINullIfOutdated(&gUI->PopupStack.Data[Index].Parent))
        {
            gUI->PopupStack.Count = Index;

            break;
        }
    }

    gUI->BuildIndex++;
    gUI->ImplicitItem = Rr_UILookupRoot(gUI);
}

static inline void Rr_UICalculateFixedItems(Rr_UIItem *Item)
{
    /* Traversal order doesn't matter. */

    Item->Extent = Rr_V2F(0.0f);

    bool NeedTextExtent =
        Item->Extents[RR_UI_AXIS_X].Type == RR_UI_EXTENT_TYPE_TEXT ||
        Item->Extents[RR_UI_AXIS_Y].Type == RR_UI_EXTENT_TYPE_TEXT ||
        (Item->DrawText &&
         (Item->Scrollable[RR_UI_AXIS_X] || Item->Scrollable[RR_UI_AXIS_Y] ||
          Item->CenterText));
    if (NeedTextExtent)
    {
        size_t TextLength;
        char const *Text;
        if (Item->Text)
        {
            if (!Item->TextLength)
            {
                Item->TextLength = strlen(Item->Text);
            }

            TextLength = Item->TextLength;
            Text = Item->Text;
        }
        else
        {
            TextLength = Item->NameLength;
            Text = Item->Name;
        }

        Rr_UIFont *Font = gUI->DefaultFont;
        if (Item->Font)
        {
            Font = Item->Font;
        }

        Rr_Vec2 TextExtent;
        if (Item->InputText)
        {
            TextExtent = Rr_UICalculateInputTextExtent(Font, TextLength, Text);
        }
        else
        {
            TextExtent = Rr_UICalculateTextExtent(Font, TextLength, Text);
        }
        Item->TextExtent = TextExtent;
    }
    else
    {
        Item->TextExtent = Rr_V2F(0.0f);
    }

    Rr_Vec2 Padding = Rr_MulV2F(Item->Padding, 2.0f);
    for (Rr_UIAxis Axis = 0; Axis < RR_UI_AXIS_COUNT; ++Axis)
    {
        Rr_UIExtent *Extent = &Item->Extents[Axis];
        if (Extent->Type == RR_UI_EXTENT_TYPE_EM)
        {
            float LineHeight = gUI->DefaultFont->LineHeight;
            Item->Extent.Elements[Axis] = LineHeight * Extent->Value;
            Item->Extent.Elements[Axis] += Padding.Elements[Axis];
        }

        if (Extent->Type == RR_UI_EXTENT_TYPE_PIXEL)
        {
            Item->Extent.Elements[Axis] = Extent->Value;
            Item->Extent.Elements[Axis] += Padding.Elements[Axis];
        }

        if (Extent->Type == RR_UI_EXTENT_TYPE_TEXT)
        {
            Item->Extent.Elements[Axis] = Item->TextExtent.Elements[Axis];
            Item->Extent.Elements[Axis] += Padding.Elements[Axis];
        }
    }

    Rr_UIItem *Child = Item->First;
    while (Child)
    {
        Rr_UICalculateFixedItems(Child);

        Child = Child->Next;
    }
}

static inline bool Rr_UIIsFixedItem(Rr_UIItem *Item, Rr_UIAxis Axis)
{
    return Item->Extents[Axis].Type == RR_UI_EXTENT_TYPE_PIXEL ||
           Item->Extents[Axis].Type == RR_UI_EXTENT_TYPE_EM ||
           Item->Extents[Axis].Type == RR_UI_EXTENT_TYPE_TEXT;
}

static inline void Rr_UICalculatePercentItems(Rr_UIItem *Item)
{
    /* Pre-order traversal. */

    for (Rr_UIAxis Axis = 0; Axis < RR_UI_AXIS_COUNT; ++Axis)
    {
        Rr_UIExtent *Extent = &Item->Extents[Axis];
        if (Extent->Type != RR_UI_EXTENT_TYPE_PERCENT)
        {
            continue;
        }

        /* Rr_UIItem *FixedParent = Item->Parent; */
        /* while (!Rr_UIIsFixedItem(FixedParent, Axis)) */
        /* { */
        /*     FixedParent = FixedParent->Parent; */
        /* } */

        /* float Percent = FixedParent->Extent.Elements[Axis]; */
        /* Percent -= FixedParent->Padding.Elements[Axis] * 2.0f; */
        /* Percent *= Extent->Value; */
        /* Percent = RR_MAX(0.0f, Percent); */
        /* Item->Extent.Elements[Axis] = Percent; */

        float Percent = Item->Parent->Extent.Elements[Axis];
        Percent -= Item->Parent->Padding.Elements[Axis] * 2.0f;
        Percent *= Extent->Value;
        Percent = RR_MAX(0.0f, Percent);
        Item->Extent.Elements[Axis] = Percent;
    }

    Rr_UIItem *Child = Item->First;
    while (Child)
    {
        Rr_UICalculatePercentItems(Child);

        Child = Child->Next;
    }
}

static inline float Rr_UICalculateChildrenExtent(
    Rr_UIItem *Item,
    Rr_UIAxis Axis,
    float *OutDispensable)
{
    bool AxisMatch = Axis == Item->Axis;
    float Total = 0.0f;
    float Dispensable = 0.0f;
    Rr_UIItem *Child = Item->First;
    while (Child)
    {
        float Extent = Child->Extent.Elements[Axis];
        if (AxisMatch)
        {
            Total += Extent;

            float Dispense = 1.0f - Child->Extents[Axis].Rigid;
            Dispensable += Extent * Dispense;
        }
        else
        {
            Total = RR_MAX(Total, Extent);
        }

        Child = Child->Next;
    }

    if (OutDispensable)
    {
        *OutDispensable = Dispensable;
    }

    return Total;
}

static inline void Rr_UICalculateSumItems(Rr_UIItem *Item)
{
    /* Post-order traversal. */

    Rr_UIItem *Child = Item->First;
    while (Child)
    {
        Rr_UICalculateSumItems(Child);

        Child = Child->Next;
    }

    for (Rr_UIAxis Axis = 0; Axis < RR_UI_AXIS_COUNT; ++Axis)
    {
        Rr_UIExtent *Extent = &Item->Extents[Axis];
        if (Extent->Type != RR_UI_EXTENT_TYPE_SUM)
        {
            continue;
        }

        float ChildrenExtent = Rr_UICalculateChildrenExtent(Item, Axis, NULL);
        float Padding = Item->Padding.Elements[Axis] * 2.0f;
        Item->Extent.Elements[Axis] = ChildrenExtent;
        Item->Extent.Elements[Axis] += Padding;
    }
}

static inline void Rr_UICalculateViolations(Rr_UIItem *Item)
{
    /* Pre-order traversal. */

    float DispenseRatio = 0.0f;
    float NonFlowMax = 0.0f;
    for (Rr_UIAxis Axis = 0; Axis < RR_UI_AXIS_COUNT; ++Axis)
    {
        float Dispensable = 0.0f;
        float ChildrenExtent =
            Rr_UICalculateChildrenExtent(Item, Axis, &Dispensable);
        float Available = Item->Extent.Elements[Axis];
        Available -= Item->Padding.Elements[Axis] * 2.0f;
        if (Item->Scrollable[Axis])
        {
            /* Do nothing. */
        }
        else if (Item->Axis == Axis)
        {
            float Error = ChildrenExtent - Available;
            if (Error > 0.0f)
            {
                DispenseRatio = Error / Dispensable;
                DispenseRatio = RR_CLAMP(0.0f, DispenseRatio, 1.0f);
                ChildrenExtent -= Dispensable * DispenseRatio;
            }
        }
        else
        {
            NonFlowMax = Available;
            ChildrenExtent = RR_MIN(ChildrenExtent, NonFlowMax);
        }
        Item->ChildrenExtent.Elements[Axis] = ChildrenExtent;
    }

    Rr_UIItem *Child = Item->First;
    while (Child)
    {
        for (Rr_UIAxis Axis = 0; Axis < RR_UI_AXIS_COUNT; ++Axis)
        {
            if (Item->Scrollable[Axis])
            {
                continue;
            }
            float ChildExtent = Child->Extent.Elements[Axis];
            if (Item->Axis == Axis)
            {
                float Dispense = 1.0f - Child->Extents[Axis].Rigid;
                Dispense *= DispenseRatio;
                Dispense *= ChildExtent;
                ChildExtent -= Dispense;
            }
            else if (Child->Fill)
            {
                ChildExtent = NonFlowMax;
            }
            else
            {
                ChildExtent = RR_MIN(ChildExtent, NonFlowMax);
            }
            ChildExtent = RR_MAX(0.0f, ChildExtent);
            Child->Extent.Elements[Axis] = ChildExtent;
        }

        /* if (Child->Fill) */
        {
            /* NOTE: I'm not entirely sure about this but it seems we need
             * another percent pass here. It started as an attempt to support
             * 100% spacers within Fill == true items and it (sort of) works
             * with context menus but more testing required. */

            /* NOTE: While working on a scrollbar item I figured it would be
             * useful to recalculate these unconditionally because a hierarchy
             * of percent items inconveniently used the same large base size. */

            Rr_UIItem *Child2 = Child->First;
            while (Child2)
            {
                Rr_UICalculatePercentItems(Child2);
                Rr_UICalculateSumItems(Child2);

                Child2 = Child2->Next;
            }
        }

        Rr_UICalculateViolations(Child);

        Child = Child->Next;
    }
}

static inline void Rr_UICalculateScroll(Rr_UIItem *Item)
{
    for (Rr_UIAxis Axis = 0; Axis < 2; ++Axis)
    {
        if (!Item->Scrollable[Axis])
        {
            Item->Scroll.Elements[Axis] = 0.0f;
            Item->ScrollDamp.Elements[Axis] = 0.0f;

            continue;
        }

        float MaxScroll = Rr_UIMaxScroll(Item, Axis);
        float Scroll = Item->Scroll.Elements[Axis];
        Item->Scroll.Elements[Axis] = RR_CLAMP(0.0f, Scroll, MaxScroll);
    }

    float Alpha = (float)(15.0 * Rr_GetDeltaSeconds());
    Item->ScrollDamp = Rr_DampV2(Item->ScrollDamp, Alpha, Item->Scroll);
}

static inline void Rr_UICalculateRects(Rr_UIItem *Item, Rr_Vec2 Offset)
{
    /* Pre-order traversal. */

    Item->Rect.Offset = Offset;
    Item->Rect.Extent = Item->Extent;
    Item->EndBuildIndex = gUI->BuildIndex;

    /* Rr_Vec4 RectSIMD; */
    /* memcpy(&RectSIMD, &Item->Rect, sizeof(Rr_Rect)); */
    /* RectSIMD = Rr_RoundV4(RectSIMD); */
    /* memcpy(&Item->Rect, &RectSIMD, sizeof(Rr_Rect)); */

    /* Create clip rects. */

    Rr_Rect ClipRect = Item->Rect;
    if (Item->Parent)
    {
        Rr_Rect *ParentRect = &gUI->ClipRects[Item->Parent->InnerClipIndex];
        ClipRect = Rr_UIRectIntersection(ParentRect, &ClipRect);
    }
    Item->ClipIndex = Rr_UIPushClipRect(ClipRect);
    if (Item->Padding.X != 0.0f || Item->Padding.Y != 0.0f)
    {
        Rr_Vec2 NegativePadding = Rr_MulV2F(Item->Padding, -1.0f);
        Rr_Rect InnerClipRect = Rr_ResizeRectV2(&Item->Rect, NegativePadding);
        InnerClipRect = Rr_UIRectIntersection(&ClipRect, &InnerClipRect);
        Item->InnerClipIndex = Rr_UIPushClipRect(InnerClipRect);
    }
    else
    {
        Item->InnerClipIndex = Item->ClipIndex;
    }

    /* Setup offset for children. */

    Rr_UICalculateScroll(Item);

    Offset = Rr_AddV2(Item->Rect.Offset, Item->Padding);
    Offset = Rr_SubV2(Offset, Item->ScrollDamp);

    Rr_UIItem *Child = Item->First;
    while (Child)
    {
        Rr_UICalculateRects(Child, Offset);

        float ChildExtent = Child->Extent.Elements[Item->Axis];
        Offset.Elements[Item->Axis] += ChildExtent;

        Child = Child->Next;
    }
}

static inline Rr_UIVertex *Rr_UIGetVertices(uint32_t Count)
{
    Rr_UIVertex *Vertices = &gUI->Vertices[gUI->VertexCount];
    gUI->VertexCount += Count;
    assert(
        gUI->VertexCount <
        (Rr_GetBufferSize(gUI->VertexStagingBuffer) / sizeof(Rr_UIVertex)));

    return Vertices;
}

static inline Rr_UIIndex *Rr_UIGetIndices(uint32_t Count)
{
    Rr_UIIndex *Indices = &gUI->Indices[gUI->IndexCount];
    gUI->IndexCount += Count;
    assert(
        gUI->IndexCount <
        (Rr_GetBufferSize(gUI->IndexStagingBuffer) / sizeof(Rr_UIIndex)));

    return Indices;
}

static inline Rr_UIPrimitive Rr_UI2ReservePrimitive(
    uint32_t VertexCount,
    uint32_t IndexCount)
{
    Rr_UIPrimitive Primitive;
    Primitive.BaseVertex = gUI->VertexCount;
    Primitive.Vertices = Rr_UIGetVertices(VertexCount);
    Primitive.Indices = Rr_UIGetIndices(IndexCount);

    return Primitive;
}

static inline void Rr_UIFeatherConvexPrimitive(
    Rr_UIPrimitive *SourcePrimitive,
    uint32_t VertexCount,
    uint32_t ClipIndex,
    float Amount)
{
    Rr_UIVertex *Vertices = SourcePrimitive->Vertices;

    Rr_UIPrimitive Primitive =
        Rr_UI2ReservePrimitive(VertexCount, VertexCount * 6);

    for (uint32_t Index = 0; Index < VertexCount; ++Index)
    {
        uint32_t NextIndex = (Index + 1) % VertexCount;

        Rr_UIVertex Previous =
            Vertices[(Index + VertexCount - 1) % VertexCount];
        Rr_UIVertex Current = Vertices[Index];
        Rr_UIVertex Next = Vertices[NextIndex];

        Rr_Vec2 Normal0 = Rr_SubV2(Previous.Offset, Current.Offset);
        Rr_Vec2 Normal1 = Rr_SubV2(Next.Offset, Current.Offset);
        float Normal0Length = Rr_LenV2(Normal0);
        float Normal1Length = Rr_LenV2(Normal1);
        Rr_Vec2 Offset = Rr_MulV2F(
            Rr_DivV2F(
                Rr_AddV2(
                    Rr_MulV2F(Normal0, Normal1Length),
                    Rr_MulV2F(Normal1, Normal0Length)),
                (Normal0.X * Normal1.Y - Normal0.Y * Normal1.X)),
            Amount);
        Rr_Vec2 Position = Rr_AddV2(Current.Offset, Offset);

        Primitive.Vertices[Index] = (Rr_UIVertex){
            .Offset = Position,
            .UV = Rr_V2F(0.0f),
            .Color = Current.Color & 0xFFFFFF00,
            .ClipIndex = ClipIndex,
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
}

static inline Rr_UIPrimitive Rr_UIDrawQuadEx(Rr_UIVertex VerticesToCopy[4])
{
    static uint32_t const RR_UI_QUAD_VERTEX_COUNT = 4;
    static uint32_t const RR_UI_QUAD_INDEX_COUNT = 6;

    Rr_UIIndex BaseVertex = gUI->VertexCount;
    Rr_UIVertex *Vertices = Rr_UIGetVertices(RR_UI_QUAD_VERTEX_COUNT);
    Rr_UIIndex *Indices = Rr_UIGetIndices(RR_UI_QUAD_INDEX_COUNT);

    for (size_t Index = 0; Index < RR_UI_QUAD_VERTEX_COUNT; ++Index)
    {
        Vertices[Index] = VerticesToCopy[Index];
    }

    static Rr_UIIndex const QUAD_INDICES[] = { 0, 1, 2, 3, 0, 2 };

    for (size_t Index = 0; Index < RR_UI_QUAD_INDEX_COUNT; ++Index)
    {
        Indices[Index] = BaseVertex + QUAD_INDICES[Index];
    }

    return (Rr_UIPrimitive){ Vertices, Indices, BaseVertex };
}

static inline Rr_UIPrimitive Rr_UIDrawSolidRect(
    Rr_Rect const *Rect,
    uint32_t ClipIndex,
    uint32_t Color,
    uint32_t NoFloor,
    uint32_t Flags)
{
    Rr_UIVertex Vertices[4];
    Vertices[0] = (Rr_UIVertex){
        .Offset = Rect->Offset,
        .Color = Color,
        .ClipIndex = ClipIndex,
        .NoFloor = NoFloor,
        .Flags = Flags,
    };
    Vertices[1] = (Rr_UIVertex){
        .Offset = Rr_V2(Rect->Offset.X + Rect->Extent.Width, Rect->Offset.Y),
        .Color = Color,
        .ClipIndex = ClipIndex,
        .NoFloor = NoFloor,
        .Flags = Flags,
    };
    Vertices[2] = (Rr_UIVertex){
        .Offset = Rr_AddV2(Rect->Offset, Rect->Extent),
        .Color = Color,
        .ClipIndex = ClipIndex,
        .NoFloor = NoFloor,
        .Flags = Flags,
    };
    Vertices[3] = (Rr_UIVertex){
        .Offset = Rr_V2(Rect->Offset.X, Rect->Offset.Y + Rect->Extent.Height),
        .Color = Color,
        .ClipIndex = ClipIndex,
        .NoFloor = NoFloor,
        .Flags = Flags,
    };

    return Rr_UIDrawQuadEx(Vertices);
}

static inline void Rr_UIDrawGlyph(
    Rr_UIGlyph *Glyph,
    Rr_Vec2 Offset,
    uint32_t ClipIndex,
    uint32_t Color)
{
    Rr_Vec2 UVs[4] = {
        { Glyph->UVMin.X, Glyph->UVMin.Y },
        { Glyph->UVMax.X, Glyph->UVMin.Y },
        { Glyph->UVMin.X, Glyph->UVMax.Y },
        { Glyph->UVMax.X, Glyph->UVMax.Y },
    };
    Rr_Rect Rect = {
        Rr_AddV2(Offset, Glyph->Offset),
        Glyph->Extent,
    };
    Rr_UIVertex Vertices[4];
    Vertices[0] = (Rr_UIVertex){
        .Offset = Rect.Offset,
        .UV = UVs[0],
        .Color = Color,
        .ClipIndex = ClipIndex,
        .NoFloor = 1,
    };
    Vertices[1] = (Rr_UIVertex){
        .Offset = Rr_V2(Rect.Offset.X + Rect.Extent.X, Rect.Offset.Y),
        .UV = UVs[1],
        .Color = Color,
        .ClipIndex = ClipIndex,
        .NoFloor = 1,
    };
    Vertices[2] = (Rr_UIVertex){
        .Offset = Rr_AddV2(Rect.Offset, Rect.Extent),
        .UV = UVs[3],
        .Color = Color,
        .ClipIndex = ClipIndex,
        .NoFloor = 1,
    };
    Vertices[3] = (Rr_UIVertex){
        .Offset = Rr_V2(Rect.Offset.X, Rect.Offset.Y + Rect.Extent.Y),
        .UV = UVs[2],
        .Color = Color,
        .ClipIndex = ClipIndex,
        .NoFloor = 1,
    };

    Rr_UIDrawQuadEx(Vertices);
}

static inline void Rr_UIDrawText(
    Rr_Vec2 Offset,
    uint32_t ClipIndex,
    uint32_t Color,
    size_t TextLength,
    char const *Text)
{
    Rr_UIFont *Font = gUI->DefaultFont;
    float LineHeight = Font->LineHeight;
    float MaxX = 0.0f;
    Rr_Vec2 StartOffset = Offset;

    Rr_UTF8Decoder Decoder = { .CString = Text };
    while (Decoder.CStringParserIndex < TextLength)
    {
        Rr_UTF8Decode(&Decoder);
        uint32_t Codepoint = Decoder.Codepoint;

        if (Codepoint == '\0')
        {
            break;
        }

        Rr_UIGlyph *Glyph = Rr_UIGetGlyphForCodepoint(Font, Codepoint);

        if (Codepoint == '\n')
        {
            Offset.X = StartOffset.X;
            Offset.Y += LineHeight;

            continue;
        }

        if (Codepoint == ' ')
        {
            Offset.X += Glyph->XAdvance;

            continue;
        }

        Rr_UIDrawGlyph(Glyph, Offset, ClipIndex, Color);

        Offset.X += Glyph->XAdvance;
        MaxX = RR_MAX(MaxX, Offset.X);
    }
}

static inline void Rr_UIDrawInputTextCursor(
    Rr_Vec2 Offset,
    uint32_t ClipIndex,
    float LineHeight,
    uint32_t Color)
{
    uint64_t TimeDelta = Rr_GetTimeNS() - gUI->TextInputCursorBlinkTimeNS;
    if ((TimeDelta / 500000000) % 2 == 0)
    {
        Rr_Rect CursorRect = {
            .Offset = Offset,
            .Extent = Rr_V2(2.0f, LineHeight),
        };
        Rr_UIDrawSolidRect(&CursorRect, ClipIndex, Color, 0, 0);
    }
}

static inline size_t Rr_UIDrawInputText(
    Rr_Vec2 Offset,
    uint32_t ClipIndex,
    bool HasFocus,
    size_t BufferLength,
    char *Buffer)
{
    Rr_UIFont *Font = gUI->DefaultFont;
    Rr_Vec2 OffsetStart = Offset;
    float LineHeight = Font->LineHeight;
    Rr_Vec2 MousePosition = gUI->MousePosition;
    MousePosition.Y -= LineHeight * 0.5f;

    Rr_Vec2 MouseDistance = Rr_V2F(FLT_MAX);

    size_t NewCursorEnd = gUI->TextInputCursorEnd;
    size_t OldCursorMin = RR_MIN(gUI->TextInputCursorBegin, NewCursorEnd);
    size_t OldCursorMax = RR_MAX(gUI->TextInputCursorBegin, NewCursorEnd);

    Rr_UTF8Decoder Decoder = { .CString = Buffer };
    while (Decoder.CStringParserIndex < BufferLength)
    {
        Rr_UTF8Decode(&Decoder);
        uint32_t Codepoint = Decoder.Codepoint;
        size_t CStringIndex = Decoder.CStringCodepointIndex;

        bool DrawLineBreakAsSpace = false;
        if (Codepoint == '\n')
        {
            Codepoint = ' ';
            DrawLineBreakAsSpace = true;
        }

        Rr_Vec2 Distance = Rr_SubV2(Offset, MousePosition);
        Distance.X = fabsf(Distance.X);
        Distance.Y = fabsf(Distance.Y);
        if (Distance.Y <= MouseDistance.Y)
        {
            if (Distance.X < MouseDistance.X || Distance.Y < MouseDistance.Y)
            {
                NewCursorEnd = CStringIndex;
                MouseDistance.X = Distance.X;
            }
            MouseDistance.Y = Distance.Y;
        }

        Rr_UIGlyph *Glyph = Rr_UIGetGlyphForCodepoint(Font, Codepoint);
        if (!Glyph)
        {
            /* TODO: Handle missing glyphs. */

            Glyph = Rr_UIGetGlyphForCodepoint(Font, ' ');
        }

        bool GlyphSelected =
            HasFocus &&
            ((OldCursorMin != OldCursorMax) &&
             (CStringIndex >= OldCursorMin && CStringIndex < OldCursorMax));
        if (GlyphSelected)
        {
            Rr_Rect GlyphRect = {
                Offset,
                Rr_V2(Glyph->XAdvance, Font->LineHeight),
            };
            uint32_t GlyphRectColor =
                gUI->Colors[RR_UI_COLOR_INPUT_SELECTION_BG];
            Rr_UIDrawSolidRect(&GlyphRect, ClipIndex, GlyphRectColor, 0, 0);
        }
        if (HasFocus && gUI->TextInputCursorEnd == CStringIndex)
        {
            uint32_t CursorColor = gUI->Colors[RR_UI_COLOR_FG];
            Rr_UIDrawInputTextCursor(
                Offset,
                ClipIndex,
                LineHeight,
                CursorColor);
        }
        uint32_t GlyphColor = GlyphSelected
                                  ? gUI->Colors[RR_UI_COLOR_INPUT_SELECTION_FG]
                                  : gUI->Colors[RR_UI_COLOR_FG];
        Rr_UIDrawGlyph(Glyph, Offset, ClipIndex, GlyphColor);
        if (Codepoint == '\0')
        {
            break;
        }

        if (DrawLineBreakAsSpace)
        {
            Offset.X = OffsetStart.X;
            Offset.Y += LineHeight;
        }
        else
        {
            Offset.X += Glyph->XAdvance;
        }
    }

    return NewCursorEnd;
}

static inline Rr_Vec2 Rr_UIGetTextOffset(Rr_UIItem *Item)
{
    Rr_Vec2 TextOffset;
    if (Item->CenterText)
    {
        TextOffset = Rr_SubV2(Item->Rect.Extent, Item->TextExtent);
        TextOffset = Rr_MulV2F(TextOffset, 0.5f);
    }
    else
    {
        TextOffset = Item->Padding;
    }
    TextOffset = Rr_AddV2(TextOffset, Item->Rect.Offset);
    TextOffset = Rr_AddV2(TextOffset, Item->TextOffset);
    TextOffset = Rr_SubV2(TextOffset, Item->ScrollDamp);

    if (Item->CenterText)
    {
        Rr_Vec2 DefaultOffset = Rr_AddV2(Item->Rect.Offset, Item->Padding);
        TextOffset = Rr_MaxV2(TextOffset, DefaultOffset);
    }

    return TextOffset;
}

static inline void Rr_UIDrawItem(Rr_UIItem *Item)
{
    /* Pre-order traversal. */

    bool Hovering = Rr_RectContains(&Item->Rect, gUI->MousePosition);
    if (!Item->MouseIgnored && Hovering)
    {
        gUI->HoveredItem = Item;
    }

    if (Item->DrawFunc)
    {
        Rr_Rect DrawRect = Item->Rect;
        DrawRect.Offset = Rr_AddV2(DrawRect.Offset, Item->DrawOffset);
        Item->DrawFunc(DrawRect, Item->ClipIndex, Item->DrawData);
    }

    if (Item->InputText && Item->Text)
    {
        Rr_Vec2 TextOffset = Rr_UIGetTextOffset(Item);
        Item->TextCursor = Rr_UIDrawInputText(
            TextOffset,
            Item->InnerClipIndex,
            gUI->FocusedItem == Item,
            Item->TextLength,
            Item->Text);
    }
    else if (Item->DrawText)
    {
        size_t TextLength;
        char const *Text;
        if (Item->Text)
        {
            TextLength = Item->TextLength;
            Text = Item->Text;
        }
        else
        {
            TextLength = Item->NameLength;
            Text = Item->Name;
        }

        Rr_Vec2 TextOffset = Rr_UIGetTextOffset(Item);
        uint32_t ClipIndex = Item->InnerClipIndex;
        uint32_t Color = gUI->Colors[Item->TextColor];
        Rr_UIDrawText(TextOffset, ClipIndex, Color, TextLength, Text);
    }

#if 0
    Rr_UIDrawSolidRect(&Item->Rect, Item->ClipIndex, (uint32_t)Item->Hash);
#endif

    Rr_UIItem *Child = Item->First;
    while (Child)
    {
        Rr_UIDrawItem(Child);

        Child = Child->Next;
    }
}

static inline void Rr_UIProcessRootItem(Rr_UIItem *Item, Rr_Rect Rect)
{
    Item->Axis = RR_UI_AXIS_Y;
    for (Rr_UIAxis Axis = 0; Axis < 2; ++Axis)
    {
        float Extent = Rect.Extent.Elements[Axis];
        if (Extent != 0.0f)
        {
            Item->Extents[Axis].Type = RR_UI_EXTENT_TYPE_PIXEL;
            Item->Extents[Axis].Value = Extent;
        }
        else
        {
            Item->Extents[Axis].Type = RR_UI_EXTENT_TYPE_SUM;
            Item->Extents[Axis].Value = Extent;
        }
    }

    Rr_UICalculateFixedItems(Item);
    Rr_UICalculatePercentItems(Item);
    Rr_UICalculateSumItems(Item);
    Rr_UICalculateViolations(Item);
    Rr_UICalculateRects(Item, Rect.Offset);
    Rr_UIDrawItem(Item);
}

static inline int Rr_UIWindowSort(void const *A, void const *B)
{
    Rr_UIWindow const *WindowA = *(Rr_UIWindow *const *)A;
    Rr_UIWindow const *WindowB = *(Rr_UIWindow *const *)B;

    return WindowA->ZOrder - WindowB->ZOrder;
}

static inline void Rr_UIProcessWindows(void)
{
    size_t WindowCount = gUI->Windows.Count;
    Rr_UIWindow **Windows = gUI->Windows.Data;
    qsort(Windows, WindowCount, sizeof(Rr_UIWindow *), Rr_UIWindowSort);
    for (size_t Index = 0; Index < WindowCount; ++Index)
    {
        Rr_UIWindow *Window = Windows[Index];
        Window->ZOrder = (int32_t)Index;
        Rr_UIItem *Item = Rr_UILookupRoot(Window);
        Rr_Rect Rect = Window->Rect;
        Rect = Rr_ResizeRect(&Rect, RR_UI_WINDOW_HANDLES);
        Rect = Rr_ResizeRect(&Rect, RR_UI_WINDOW_BORDER);
        Rr_UIProcessRootItem(Item, Rect);
    }
}

static inline void Rr_UIProcessPopups(void)
{
    size_t PopupCount = gUI->PopupStack.Count;
    Rr_UIPopupInfo *PopupInfos = gUI->PopupStack.Data;
    for (size_t Index = 0; Index < PopupCount; ++Index)
    {
        Rr_UIPopupInfo *PopupInfo = &PopupInfos[Index];
        Rr_UIItem *Parent = PopupInfo->Parent;
        Rr_UIItem *PopupRoot = Rr_UIGetPopup(Parent);
        Rr_Vec2 Offset = Parent->Rect.Offset;
        if (PopupInfo->Anchor == RR_UI_POPUP_ANCHOR_RIGHT)
        {
            Offset.X += Parent->Rect.Extent.X;
        }
        if (PopupInfo->Anchor == RR_UI_POPUP_ANCHOR_BOTTOM)
        {
            Offset.Y += Parent->Rect.Extent.Y;
        }
        Offset = Rr_AddV2(Offset, PopupInfo->Offset);
        Rr_Rect Rect = { Offset, Rr_V2F(0.0f) };
        Rr_UIProcessRootItem(PopupRoot, Rect);
    }
}

void Rr_EndUI2(void)
{
    Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
    Rr_IntVec2 SwapchainExtent = Rr_GetImage2DExtent(SwapchainImage);
    Rr_Rect SwapchainRect = { Rr_V2F(0.0f), Rr_CastV2(SwapchainExtent) };

    /* Cleanup. */

    gUI->ClipRectCount = 0;
    gUI->ClipRects = Rr_GetMappedBufferData(gUI->ClipRectStagingBuffer);
    gUI->VertexCount = 0;
    gUI->Vertices = Rr_GetMappedBufferData(gUI->VertexStagingBuffer);
    gUI->IndexCount = 0;
    gUI->Indices = Rr_GetMappedBufferData(gUI->IndexStagingBuffer);
    gUI->LeftMouseButton.Down = false;
    gUI->LeftMouseButton.Up = false;
    gUI->LeftMouseButton.ReleasedItem = NULL;
    gUI->MouseWheel = Rr_V2F(0.0f);
    gUI->MouseMoved = false;
    gUI->HoveredItem = NULL;
    RR_ZERO(gUI->ParentStack);

    /* Layout. */

    Rr_UIProcessRootItem(gUI->ImplicitItem, SwapchainRect);
    Rr_UIProcessWindows();
    Rr_UIProcessPopups();

    /* Cursor. */

    if (gUI->LeftMouseButton.Item)
    {
        Rr_CursorType Type = gUI->LeftMouseButton.Item->DraggingCursor;
        Rr_SetCursor(Type);
    }
    else if (gUI->HoveredItem)
    {
        Rr_CursorType Type = gUI->HoveredItem->HoveringCursor;
        Rr_SetCursor(Type);
    }

    /* Draw. */

    Rr_Graph *Graph = Rr_GetGraph();

    Rr_UIUniform Uniform = {
        .CanvasExtent.XY = Rr_CastV2(SwapchainExtent),
        .IndexCount = gUI->IndexCount,
    };
    char *MappedUniformData = Rr_GetMappedBufferData(gUI->UniformBuffer);
    memcpy(MappedUniformData, &Uniform, sizeof(Uniform));

    size_t ClipRectsSize = sizeof(Rr_Rect) * gUI->ClipRectCount;
    size_t VerticesSize = sizeof(Rr_UIVertex) * gUI->VertexCount;
    size_t IndicesSize = sizeof(Rr_UIIndex) * gUI->IndexCount;

    size_t ClipRectsOffset = 0;
    size_t VerticesOffset = RR_ALIGN_POW2(ClipRectsSize, 256);
    size_t IndicesOffset = VerticesOffset + RR_ALIGN_POW2(VerticesSize, 256);

    // Rr_FlushBufferRange(gUI->ClipRectStagingBuffer, 0, ClipRectsSize);
    // Rr_FlushBufferRange(gUI->VertexStagingBuffer, 0, VerticesSize);
    // Rr_FlushBufferRange(gUI->IndexStagingBuffer, 0, IndicesSize);

    Rr_TransferNode *TransferNode = Rr_AddTransferNode(Graph);
    Rr_TransferBufferData(
        TransferNode,
        ClipRectsSize,
        gUI->ClipRectStagingBuffer,
        0,
        gUI->DeviceLocalBuffer,
        ClipRectsOffset);
    Rr_TransferBufferData(
        TransferNode,
        VerticesSize,
        gUI->VertexStagingBuffer,
        0,
        gUI->DeviceLocalBuffer,
        VerticesOffset);
    Rr_TransferBufferData(
        TransferNode,
        IndicesSize,
        gUI->IndexStagingBuffer,
        0,
        gUI->DeviceLocalBuffer,
        IndicesOffset);

    Rr_ColorTarget ColorTarget = {
        .Image = SwapchainImage,
        .LoadOp = RR_LOAD_OP_LOAD,
        .StoreOp = RR_STORE_OP_STORE,
    };
    Rr_GraphNode *GraphicsNode =
        Rr_AddGraphicsNode(Graph, 1, &ColorTarget, NULL);
    Rr_BindGraphicsPipeline(GraphicsNode, gUI->DefaultPipeline);
    Rr_BindUniformBuffer(
        GraphicsNode,
        gUI->UniformBuffer,
        0,
        0,
        0,
        sizeof(Uniform));
    // Rr_BindVertexBuffer(
    //     GraphicsNode,
    //     gUI->DeviceLocalBuffer,
    //     0,
    //     VerticesOffset);
    // Rr_BindIndexBuffer(
    //     GraphicsNode,
    //     gUI->DeviceLocalBuffer,
    //     0,
    //     IndicesOffset,
    //     RR_INDEX_TYPE_UINT32);
    Rr_BindStorageBuffer(
        GraphicsNode,
        gUI->DeviceLocalBuffer,
        0,
        1,
        ClipRectsOffset,
        ClipRectsSize);
    Rr_BindStorageBuffer(
        GraphicsNode,
        gUI->DeviceLocalBuffer,
        0,
        2,
        VerticesOffset,
        VerticesSize);
    Rr_BindStorageBuffer(
        GraphicsNode,
        gUI->DeviceLocalBuffer,
        0,
        3,
        IndicesOffset,
        IndicesSize);
    Rr_BindCombinedImage2DSampler(
        GraphicsNode,
        gUI->DefaultFont->Image,
        gUI->LinearSampler,
        1,
        0);
    // Rr_DrawIndexed(GraphicsNode, (uint32_t)gUI->IndexCount, 1, 0, 0, 0);
    Rr_Draw(GraphicsNode, (uint32_t)gUI->IndexCount, 1, 0, 0);
}

Rr_UIExtent Rr_UISum(float Rigid)
{
    return (Rr_UIExtent){
        .Type = RR_UI_EXTENT_TYPE_SUM,
        .Rigid = Rigid,
    };
}

Rr_UIExtent Rr_UIEm(float Value, float Rigid)
{
    return (Rr_UIExtent){
        .Type = RR_UI_EXTENT_TYPE_EM,
        .Value = Value,
        .Rigid = Rigid,
    };
}

Rr_UIExtent Rr_UIPixel(float Value, float Rigid)
{
    return (Rr_UIExtent){
        .Type = RR_UI_EXTENT_TYPE_PIXEL,
        .Value = Value,
        .Rigid = Rigid,
    };
}

Rr_UIExtent Rr_UIText(float Rigid)
{
    return (Rr_UIExtent){
        .Type = RR_UI_EXTENT_TYPE_TEXT,
        .Rigid = Rigid,
    };
}

Rr_UIExtent Rr_UIPercent(float Value, float Rigid)
{
    return (Rr_UIExtent){
        .Type = RR_UI_EXTENT_TYPE_PERCENT,
        .Value = Value,
        .Rigid = Rigid,
    };
}

void Rr_UIPush(Rr_UIItem *Item)
{
    *RR_PUSH_INTO_ARRAY(&gUI->ParentStack, gUI->FrameArena) = Item;
}

void Rr_UIPop(void)
{
    if (!gUI->ParentStack.Count)
    {
        return;
    }

    RR_UNUSED(RR_POP_FROM_ARRAY(&gUI->ParentStack));
}

Rr_UIItem *Rr_UIGetItemEx(Rr_UIItem *Parent, char const *Name)
{
    size_t NameLength;
    Rr_UIHash NameHash = 0;
    Rr_UIItem *Item = NULL;
    if (Name)
    {
        NameHash = Rr_UIGetNameHash(Name, Parent->Hash, &NameLength);
        Item = Rr_UILookupItem(&Parent, NameHash);
        Item->NameLength = NameLength;
        Item->Name = Rr_AllocCopy(Name, NameLength + 1, gUI->FrameArena);
        Item->Name[NameLength] = '\0';
    }
    else /* Transient flows (i.e. spacers) */
    {
        Item = Rr_Alloc(sizeof(Rr_UIItem), gUI->FrameArena);
    }

    Item->Parent = Parent;

    if (!Parent->First)
    {
        Parent->First = Item;
    }
    if (Parent->Last)
    {
        Parent->Last->Next = Item;
    }
    Parent->Last = Item;

    return Item;
}

Rr_UIItem *Rr_UIGetItem(char const *Name)
{
    Rr_UIItem *Parent;
    if (!gUI->ParentStack.Count)
    {
        Parent = gUI->ImplicitItem;
    }
    else
    {
        Parent = RR_LAST_ARRAY_ELEMENT(&gUI->ParentStack);
    }

    return Rr_UIGetItemEx(Parent, Name);
}

Rr_UIItem *Rr_UIGetPopup(Rr_UIItem *Item)
{
    for (size_t Index = 0; Index < gUI->PopupStack.Count; ++Index)
    {
        if (Item == gUI->PopupStack.Data[Index].Parent)
        {
            return Rr_UILookupRoot(Item);
        }
    }

    return NULL;
}

Rr_UIItem *Rr_UIGetHoverPopup(Rr_UIPopupInfo PopupInfo)
{
    if (PopupInfo.Parent->Hovering)
    {
        return Rr_UIOpenPopup(PopupInfo);
    }

    Rr_UIItem *Popup = Rr_UIGetPopup(PopupInfo.Parent);
    if (Popup && gUI->HoveredItem)
    {
        Rr_UIItem *ParentRoot = Rr_UIRoot(PopupInfo.Parent);
        Rr_UIItem *HoveredRoot = Rr_UIRoot(gUI->HoveredItem);
        if (ParentRoot == HoveredRoot)
        {
            Rr_UIClosePopup(PopupInfo.Parent);

            return NULL;
        }
    }

    return Popup;
}

Rr_UIItem *Rr_UIOpenPopup(Rr_UIPopupInfo PopupInfo)
{
    if (!PopupInfo.Parent->Hash)
    {
        return NULL;
    }

    size_t Count = gUI->PopupStack.Count;
    if (!Count)
    {
        *RR_PUSH_INTO_ARRAY(&gUI->PopupStack, gUI->Arena) = PopupInfo;

        return Rr_UIGetPopup(PopupInfo.Parent);
    }

    Rr_UIItem *Root = Rr_UIRoot(PopupInfo.Parent);
    for (size_t Index = Count - 1; Index != SIZE_MAX; --Index)
    {
        Rr_UIItem *Parent = gUI->PopupStack.Data[Index].Parent;
        Rr_UIItem *Popup = Rr_UIGetPopup(Parent);

        if (Parent == PopupInfo.Parent)
        {
            /* Already opened. */

            gUI->PopupStack.Data[Index].Anchor = PopupInfo.Anchor;
            gUI->PopupStack.Data[Index].Offset = PopupInfo.Offset;
            gUI->PopupStack.Count = Index + 1;

            return Popup;
        }

        if (Root == Popup)
        {
            break;
        }

        gUI->PopupStack.Count--;
    }

    *RR_PUSH_INTO_ARRAY(&gUI->PopupStack, gUI->Arena) = PopupInfo;

    return Rr_UIGetPopup(PopupInfo.Parent);
}

void Rr_UIClosePopup(Rr_UIItem *Item)
{
    size_t Count = gUI->PopupStack.Count;
    if (!Count)
    {
        return;
    }

    for (size_t Index = Count - 1; Index != SIZE_MAX; --Index)
    {
        Rr_UIItem *Parent = gUI->PopupStack.Data[Index].Parent;

        if (Parent == Item)
        {
            gUI->PopupStack.Count = Index;

            return;
        }
    }
}

void Rr_UIClosePopups(void)
{
    gUI->PopupStack.Count = 0;
}

/* Toolkit */

void Rr_UISetDefaultColors(void)
{
    uint32_t *Colors = gUI->Colors;

    Colors[RR_UI_COLOR_FG] = 0x000000FF;
    Colors[RR_UI_COLOR_FG_DIMMED] = 0x010101FF;
    Colors[RR_UI_COLOR_BG] = 0xAAAAAAFF;
    Colors[RR_UI_COLOR_INPUT_FG] = 0x000000FF;
    Colors[RR_UI_COLOR_INPUT_BG] = 0xFFFFFFFF;
    Colors[RR_UI_COLOR_INPUT_SELECTION_FG] = 0xFFFFFFFF;
    Colors[RR_UI_COLOR_INPUT_SELECTION_BG] = 0x0000FFFF;
    Colors[RR_UI_COLOR_WHITE] = 0xFFFFFFFF;
    Colors[RR_UI_COLOR_BLACK] = 0x000000FF;
}

void Rr_UIDrawBevelEx(
    Rr_Rect Rect,
    uint32_t ClipIndex,
    float Border,
    uint32_t LightColor,
    uint32_t DarkColor,
    uint32_t CenterColor,
    uint32_t FillColor)
{
    static uint32_t const RR_UI_BEVEL_VERTEX_COUNT = 20;
    static uint32_t const RR_UI_BEVEL_INDEX_COUNT = 54;

    uint32_t BaseVertex = (uint32_t)gUI->VertexCount;
    Rr_UIVertex *Vertices = &gUI->Vertices[gUI->VertexCount];
    gUI->VertexCount += RR_UI_BEVEL_VERTEX_COUNT;
    Rr_UIIndex *Indices = &gUI->Indices[gUI->IndexCount];
    gUI->IndexCount += RR_UI_BEVEL_INDEX_COUNT;

    float HalfBorder = Border * 0.5f;

    Rr_Vec2 Offset = Rect.Offset;
    Rr_Vec2 Extent = Rect.Extent;

    Vertices[0] = (Rr_UIVertex){
        .Color = LightColor,
    };
    Vertices[1] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X, 0.0f),
        .Color = LightColor,
    };
    Vertices[2] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X, 0.0f),
        .Color = DarkColor,
    };
    Vertices[3] = (Rr_UIVertex){
        .Offset = Rr_V2F(1.0f),
        .Color = LightColor,
    };
    Vertices[4] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - 1.0f, 1.0f),
        .Color = LightColor,
    };
    Vertices[5] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - 1.0f, 1.0f),
        .Color = DarkColor,
    };
    Vertices[6] = (Rr_UIVertex){
        .Offset = Rr_V2F(HalfBorder),
        .Color = CenterColor,
    };
    Vertices[7] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - Border, HalfBorder),
        .Color = CenterColor,
    };
    Vertices[8] = (Rr_UIVertex){
        .Offset = Rr_V2(HalfBorder, Extent.Y - Border),
        .Color = CenterColor,
    };
    Vertices[9] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - Border, Extent.Y - Border),
        .Color = CenterColor,
    };
    Vertices[10] = (Rr_UIVertex){
        .Offset = Rr_V2(1.0f, Extent.Y - 1.0f),
        .Color = LightColor,
    };
    Vertices[11] = (Rr_UIVertex){
        .Offset = Rr_V2(1.0f, Extent.Y - 1.0f),
        .Color = DarkColor,
    };
    Vertices[12] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - 1.0f, Extent.Y - 1.0f),
        .Color = DarkColor,
    };
    Vertices[13] = (Rr_UIVertex){
        .Offset = Rr_V2(0.0f, Extent.Y),
        .Color = LightColor,
    };
    Vertices[14] = (Rr_UIVertex){
        .Offset = Rr_V2(0.0f, Extent.Y),
        .Color = DarkColor,
    };
    Vertices[15] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X, Extent.Y),
        .Color = DarkColor,
    };
    Vertices[16] = (Rr_UIVertex){
        .Offset = Rr_V2F(HalfBorder),
        .Color = FillColor,
    };
    Vertices[17] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - Border, HalfBorder),
        .Color = FillColor,
    };
    Vertices[18] = (Rr_UIVertex){
        .Offset = Rr_V2(HalfBorder, Extent.Y - Border),
        .Color = FillColor,
    };
    Vertices[19] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - Border, Extent.Y - Border),
        .Color = FillColor,
    };

    for (size_t Index = 0; Index < RR_UI_BEVEL_VERTEX_COUNT; ++Index)
    {
        /* Rounding bevel offsets helps with fractional scale values. */
        Vertices[Index].Offset = Rr_RoundV2(Vertices[Index].Offset);
        Vertices[Index].Offset = Rr_AddV2(Vertices[Index].Offset, Offset);
        Vertices[Index].ClipIndex = ClipIndex;
    }

    static Rr_UIIndex const BEVEL_INDICES[] = {
        0,  1,  4,  0,  4,  3,  5, 2, 15, 5, 15, 12, 0,  3,  10, 0,  10, 13,
        14, 11, 12, 14, 12, 15, 3, 7, 6,  3, 4,  7,  7,  5,  12, 7,  12, 9,
        8,  9,  12, 8,  12, 11, 3, 6, 8,  3, 8,  10, 16, 17, 19, 16, 19, 18
    };

    for (size_t Index = 0; Index < RR_UI_BEVEL_INDEX_COUNT; ++Index)
    {
        Indices[Index] = BaseVertex + BEVEL_INDICES[Index];
    }
}

void Rr_UIDrawRect(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData)
{
    Rr_UIDrawSolidRect(&Rect, ClipIndex, (uint32_t)DrawData, 0, 0);
}

void Rr_UIDrawCheckerRect(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData)
{
    Rr_UIDrawSolidRect(&Rect, ClipIndex, (uint32_t)DrawData, 0, 1);
}

void Rr_UIDrawWindowBackground(
    Rr_Rect Rect,
    uint32_t ClipIndex,
    uintptr_t DrawData)
{
    Rect = Rr_ResizeRect(&Rect, -RR_UI_WINDOW_HANDLES + RR_UI_WINDOW_BORDER);
    Rr_UIDrawSolidRect(&Rect, ClipIndex, gUI->Colors[RR_UI_COLOR_BLACK], 0, 0);
    Rect = Rr_ResizeRect(&Rect, -RR_UI_WINDOW_BORDER);
    Rr_UIDrawSolidRect(&Rect, ClipIndex, gUI->Colors[RR_UI_COLOR_BG], 0, 0);
}

void Rr_UIDrawWindowTitleBar(
    Rr_Rect Rect,
    uint32_t ClipIndex,
    uintptr_t DrawData)
{
    Rect.Extent.X += RR_UI_WINDOW_BORDER; /* Merge right border. */
    uint32_t LightColor = 0xFFFFFFFF;
    uint32_t DarkColor = 0x000000FF;
    uint32_t CenterColor = 0xAAAAAAFF;
    uint32_t FillColor = 0xAAAAAAFF;
    if (DrawData) /* Has focus. */
    {
        LightColor = 0xAAAAAAFF;
        DarkColor = 0x000000FF;
        CenterColor = 0xAAAAAAFF;
        FillColor = 0x000000FF;
    }
    Rr_UIDrawBevelEx(
        Rect,
        ClipIndex,
        RR_UI_BEVEL_THICKNESS,
        LightColor,
        DarkColor,
        CenterColor,
        FillColor);
}

void Rr_UIDrawCloseCross(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData)
{
    uint32_t Color = (uint32_t)DrawData;
    float Angle = RR_ANGLE_DEG(45.0f);
    float Length = Rect.Extent.X * 0.5f;
    float Thickness = 0.2f;
    for (int Axis = 0; Axis < 2; ++Axis)
    {
        Rr_Rect BarRect;
        BarRect.Offset.X = Length * -0.5f;
        BarRect.Offset.Y = Thickness * -0.5f;
        BarRect.Extent.X = Length;
        BarRect.Extent.Y = Thickness;
        Rr_UIPrimitive Primitive =
            Rr_UIDrawSolidRect(&BarRect, ClipIndex, Color, 1, 0);
        Rr_UIVertex *Vertices = Primitive.Vertices;
        for (int Index = 0; Index < 4; ++Index)
        {
            Rr_Vec2 Offset = Vertices[Index].Offset;
            Offset = Rr_RotateV2(Offset, Angle);
            Offset = Rr_AddV2(Offset, Rect.Offset);
            Offset = Rr_AddV2(Offset, Rr_MulV2F(Rect.Extent, 0.5f));
            Vertices[Index].Offset = Offset;
        }
        Rr_UIFeatherConvexPrimitive(&Primitive, 4, ClipIndex, 1.5f);
        Angle += RR_ANGLE_DEG(90.0f);
    }
}

Rr_UIItem *Rr_UISpacer(Rr_UIExtent Extent)
{
    Rr_UIItem *Item = Rr_UIGetItem(NULL);
    Rr_UIAxis ParentAxis = Item->Parent->Axis;
    Rr_UIAxis NonAlignedAxis;
    if (ParentAxis == RR_UI_AXIS_X)
    {
        NonAlignedAxis = RR_UI_AXIS_Y;
    }
    else
    {
        NonAlignedAxis = RR_UI_AXIS_X;
    }
    Item->Extents[NonAlignedAxis].Type = RR_UI_EXTENT_TYPE_SUM;
    Item->Extents[ParentAxis] = Extent;
    Item->MouseIgnored = true;

    return Item;
}

Rr_UIItem *Rr_UIButton(char const *Name)
{
    Rr_UIItem *Item = Rr_UIGetItem(Name);
    Item->Extents[RR_UI_AXIS_X] = Rr_UIText(1.0f);
    Item->Extents[RR_UI_AXIS_Y] = Rr_UIText(1.0f);
    Item->DrawText = true;
    Item->CenterText = true;
    Item->MouseClickable = true;
    Item->DrawData = gUI->Colors[RR_UI_COLOR_BG];
    if (Item->Hovering && (Item->Dragging || Item->Pressed))
    {
        Item->TextOffset = Rr_V2F(1.0f);
        Item->DrawFunc = Rr_UIDrawInset;
    }
    else
    {
        Item->TextOffset = Rr_V2F(0.0f);
        Item->DrawFunc = Rr_UIDrawBevel;
    }

    return Item;
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
    /* Consider selected range since it will be replaced by input string. */
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
        gUI->TextInputCursorCodepointMaxCol = 0;

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
            gUI->TextInputCursorCodepointMaxCol = Decoder.CodepointCount - 1;

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
    if (gUI->TextInputEvents.Count == 0 && gUI->KeyboardInputEvents.Count == 0)
    {
        return Result;
    }

    uint64_t TimeNS = Rr_GetTimeNS();
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

    for (size_t Index = 0; Index < gUI->KeyboardInputEvents.Count; ++Index)
    {
        Rr_KeyEvent *Event = gUI->KeyboardInputEvents.Data + Index;

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
                            gUI->TextInputCursorCodepointMaxCol)
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
                        gUI->TextInputCursorCodepointMaxCol)
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

        if (Event->Scancode == RR_SCANCODE_BACKSPACE && BufferLength > 0)
        {
            if (CursorMin == 0 && CursorMax == 0)
            {
                /* Cursor is at the beginning. */
            }
            else if (CursorMin == 0 && CursorMax == BufferLength)
            {
                /* Whole buffer is selected. */

                Buffer[0] = '\0';
                NewCursorEnd = NewCursorBegin = 0;
                BufferLength = 0;
            }
            else if (CursorMin != CursorMax)
            {
                /* A range is selected. */

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
                /* No range is selected. */

                if (Event->Keymod & RR_KEYMOD_CTRL)
                {
                    CursorMin = Rr_PreviousUTF8WordOffset(Buffer, CursorMin);
                }
                else
                {
                    CursorMin =
                        Rr_PreviousUTF8CodepointOffset(Buffer, CursorMin);
                }

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

        if (Event->Scancode == RR_SCANCODE_DELETE && BufferLength > 0)
        {
            if (CursorMin == BufferLength && CursorMax == BufferLength)
            {
                /* Cursor is at the end. */
            }
            else if (CursorMin == 0 && CursorMax == BufferLength)
            {
                /* Whole buffer is selected.
                 * NOTE: Same behavior as BACKSPACE. */

                Buffer[0] = '\0';
                NewCursorEnd = NewCursorBegin = 0;
                BufferLength = 0;
            }
            else if (CursorMin != CursorMax)
            {
                /* A range is selected.
                 * NOTE: Same behavior as BACKSPACE. */

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
                /* No range is selected. */

                if (Event->Keymod & RR_KEYMOD_CTRL)
                {
                    CursorMax = Rr_NextUTF8WordOffset(Buffer, CursorMin);
                }
                else
                {
                    CursorMax = Rr_NextUTF8CodepointOffset(Buffer, CursorMin);
                }

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

        if (Edited)
        {
            *CursorBegin = NewCursorBegin;
            *CursorEnd = NewCursorEnd;
            gUI->TextInputCursorBlinkTimeNS = TimeNS;
            Result.Edited |= true;
        }

        if (ResetCol)
        {
            Rr_UISetTextInputMaxCol(Buffer, NewCursorEnd);
        }
    }

    for (size_t Index = 0; Index < gUI->TextInputEvents.Count; ++Index)
    {
        NewCursorBegin = *CursorBegin;
        NewCursorEnd = *CursorEnd;

        /* CursorMin = RR_MIN(NewCursorBegin, NewCursorEnd); */
        /* CursorMax = RR_MAX(NewCursorBegin, NewCursorEnd); */

        char const *CString = gUI->TextInputEvents.Data[Index];
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
            gUI->TextInputCursorBlinkTimeNS = TimeNS;
            Rr_UISetTextInputMaxCol(Buffer, NewCursorEnd);
            Result.Edited |= true;
        }
    }

    RR_ZERO(gUI->TextInputEvents);
    RR_ZERO(gUI->KeyboardInputEvents);

    return Result;
}

Rr_UIItem *Rr_UIInputFieldV2(
    char const *Name,
    size_t BufferLength,
    char *Buffer)
{
    Rr_UIItem *Item = Rr_UIGetItem(Name);
    Item->Extents[RR_UI_AXIS_X] = Rr_UIText(1.0f);
    Item->Extents[RR_UI_AXIS_Y] = Rr_UIText(1.0f);
    Item->MouseClickable = true;
    Item->HoveringCursor = RR_CURSOR_TYPE_TEXT;
    Item->DraggingCursor = RR_CURSOR_TYPE_TEXT;
    Item->DrawFunc = Rr_UIDrawInset;
    Item->DrawData = gUI->Colors[RR_UI_COLOR_INPUT_BG];

    Item->DrawText = true;
    Item->InputText = true;
    Item->TextLength = BufferLength;
    Item->Text = Buffer;

    if (Item->Pressed)
    {
        uint32_t Cycles = (Item->Pressed - 1u) % 3u;
        if (Cycles == 0)
        {
            gUI->TextInputCursorBegin = Item->TextCursor;
            gUI->TextInputCursorEnd = Item->TextCursor;
        }
        if (Cycles == 1)
        {
            if (!(Item->TextCursor > 0 && Buffer[Item->TextCursor - 1] == ' '))
            {
                gUI->TextInputCursorBegin =
                    Rr_PreviousUTF8WordOffset(Buffer, Item->TextCursor);
            }
            gUI->TextInputCursorEnd =
                Rr_LastUTF8CharInWordOffset(Buffer, gUI->TextInputCursorBegin);

            gUI->TextInputCursorEnd = RR_CLAMP(
                gUI->TextInputCursorBegin,
                gUI->TextInputCursorEnd,
                BufferLength);
        }
        if (Cycles == 2)
        {
            gUI->TextInputCursorBegin =
                Rr_UILineStart(Buffer, Item->TextCursor);
            gUI->TextInputCursorEnd = Rr_UILineEnd(Buffer, Item->TextCursor);
        }
        gUI->TextInputCursorBlinkTimeNS = Rr_GetTimeNS();
        Rr_UISetTextInputMaxCol(Buffer, gUI->TextInputCursorEnd);
    }
    else if (Item->Dragged)
    {
        gUI->TextInputCursorEnd = Item->TextCursor;
        gUI->TextInputCursorBlinkTimeNS = Rr_GetTimeNS();
        Rr_UISetTextInputMaxCol(Buffer, gUI->TextInputCursorEnd);
    }
    if (Item->HasFocus)
    {
        // bool EnterToConfirm =
        //     !(Flags & RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT);
        bool EnterToConfirm = false;
        Rr_UIEditResult EditResult = Rr_UIEditUTF8Buffer(
            &gUI->TextInputCursorBegin,
            &gUI->TextInputCursorEnd,
            BufferLength,
            Buffer,
            NULL,
            EnterToConfirm);
        if (EditResult.Confirmed)
        {
            // gUI->FocusedWidgetParent = NULL;
        }
    }

    return Item;
}

static inline Rr_UIItem *Rr_UIContextMenuBase(char const *Name)
{
    Rr_UIItem *Item = Rr_UIGetItem(Name);
    Item->Extents[RR_UI_AXIS_X] = Rr_UISum(1.0f);
    Item->Extents[RR_UI_AXIS_Y] = Rr_UISum(1.0f);
    Item->Padding = Rr_V2(0.0f, 1.0f);
    Item->Fill = true;
    Item->MouseClickable = true;
    Item->DrawFunc = Rr_UIDrawBevel;
    if (Item->Hovering)
    {
        Item->DrawData = gUI->Colors[RR_UI_COLOR_WHITE];
    }
    else
    {
        Item->DrawData = gUI->Colors[RR_UI_COLOR_BG];
    }

    Rr_UIPush(Item);

    Rr_UISpacer(Rr_UIEm(0.25f, 1.0f));

    Rr_UIItem *Text = Rr_UIGetItem(NULL);
    Text->Extents[RR_UI_AXIS_X] = Rr_UIText(1.0f);
    Text->Extents[RR_UI_AXIS_Y] = Rr_UIText(1.0f);
    Text->MouseIgnored = true;
    Text->DrawText = true;
    Text->TextColor = RR_UI_COLOR_FG;
    Text->TextOffset = Rr_V2F(0.0f);
    Text->TextLength = strlen(Name);
    Text->Text = Rr_AllocCopy(Name, Text->TextLength + 1, gUI->FrameArena);

    Rr_UIPop();

    return Item;
}

typedef union Rr_UIDrawTriangleUnion Rr_UIDrawTriangleUnion;
union Rr_UIDrawTriangleUnion
{
    struct
    {
        uint32_t Color;
        float Angle;
    } Params;
    uintptr_t DrawData;
};

static inline Rr_UIItem *Rr_UITriangle(
    Rr_UIItem *Parent,
    Rr_UIExtent Extent,
    float Angle)
{
    Rr_UIDrawTriangleUnion Union = {
        .Params.Color = gUI->Colors[RR_UI_COLOR_BLACK],
        .Params.Angle = Angle,
    };

    Rr_UIItem *Triangle = Rr_UIGetItemEx(Parent, NULL);
    Triangle->Extents[RR_UI_AXIS_X] = Extent;
    Triangle->Extents[RR_UI_AXIS_Y] = Extent;
    Triangle->MouseIgnored = true;
    Triangle->DrawFunc = Rr_UIDrawTriangle;
    Triangle->DrawData = Union.DrawData;

    return Triangle;
}

Rr_UIItem *Rr_UIPushContextMenu(char const *Name)
{
    Rr_UIItem *Item = Rr_UIContextMenuBase(Name);

    Rr_UIPush(Item);

    Rr_UISpacer(Rr_UIPercent(1.0f, 0.0f));

    Rr_UIPop();

    Rr_UITriangle(Item, Rr_UIEm(1.0f, 1.0f), 0.0f);

    Rr_UIPopupInfo PopupInfo = {
        .Parent = Item,
        .Anchor = RR_UI_POPUP_ANCHOR_RIGHT,
    };
    Rr_UIItem *Popup = Rr_UIGetHoverPopup(PopupInfo);
    if (Popup)
    {
        Rr_UIPush(Popup);
    }

    return Popup;
}

bool Rr_UIContextMenuItem(char const *Name)
{
    Rr_UIItem *Item = Rr_UIContextMenuBase(Name);

    Rr_UIPush(Item);

    Rr_UISpacer(Rr_UIEm(0.25f, 1.0f));

    Rr_UIPop();

    if (Item->Clicked)
    {
        Rr_UIClosePopups();
    }

    return Item->Clicked;
}

Rr_UIItem *Rr_UIScrollbar(char const *Name, Rr_UIItem *Item, Rr_UIAxis Axis)
{
    float const SIZE = 0.85f;

    Rr_UIAxis NonAxis = Axis == RR_UI_AXIS_X ? RR_UI_AXIS_Y : RR_UI_AXIS_X;
    float MaxScroll = Rr_UIMaxScroll(Item, Axis);
    float ScrollRatio = 1.0f;
    float ItemRatio = 1.0f;
    if (MaxScroll > 0.0f)
    {
        ScrollRatio = Item->Scroll.Elements[Axis] / MaxScroll;
        ScrollRatio = RR_CLAMP(0.0f, ScrollRatio, 1.0f);
        ItemRatio = Item->Extent.Elements[Axis] / MaxScroll;
        ItemRatio = RR_CLAMP(0.0f, ItemRatio, 1.0f);
    }

    Rr_UIItem *Bar = Rr_UIGetItem(Name);
    Bar->Extents[Axis] = Rr_UIPercent(1.0f, 0.0f);
    Bar->Extents[NonAxis] = Rr_UIEm(SIZE, 1.0f);
    Bar->Axis = Axis;
    Bar->Padding = Rr_V2F(1.0f);

    Rr_UIPush(Bar);

    Rr_UIItem *HandleBG = Rr_UIGetItem("__HandleBG");
    HandleBG->Extents[RR_UI_AXIS_X] = Rr_UIPercent(1.0f, 0.0f);
    HandleBG->Extents[RR_UI_AXIS_Y] = Rr_UIPercent(1.0f, 0.0f);
    HandleBG->DrawFunc = Rr_UIDrawCheckerRect;
    HandleBG->DrawData = 0x555555FF;
    HandleBG->MouseClickable = true;

    Rr_UIPush(HandleBG);

    Rr_UISpacer(Rr_UIPercent((1.0f - ItemRatio) * ScrollRatio, 1.0f));

    Rr_UIItem *Handle = Rr_UIGetItem("__Handle");
    Handle->Extents[Axis] = Rr_UIPercent(ItemRatio, 1.0f);
    Handle->Extents[NonAxis] = Rr_UIPercent(1.0f, 0.0f);
    Handle->DrawFunc = Rr_UIDrawBevel;
    Handle->DrawData = gUI->Colors[RR_UI_COLOR_BG];
    Handle->MouseClickable = true;

    Rr_UIPop();

    Rr_UISpacer(Rr_UIPixel(1.0f, 1.0f));

    Rr_UIItem *DecButton = Rr_UIButton("__DecButton");
    DecButton->Extents[RR_UI_AXIS_X] = Rr_UIEm(SIZE, 1.0f);
    DecButton->Extents[RR_UI_AXIS_Y] = Rr_UIEm(SIZE, 1.0f);
    DecButton->DrawText = false;

    Rr_UITriangle(DecButton, Rr_UIPercent(1.0f, 1.0f), 180.0f);

    Rr_UISpacer(Rr_UIPixel(1.0f, 1.0f));

    Rr_UIItem *IncButton = Rr_UIButton("__IncButton");
    IncButton->Extents[RR_UI_AXIS_X] = Rr_UIEm(SIZE, 1.0f);
    IncButton->Extents[RR_UI_AXIS_Y] = Rr_UIEm(SIZE, 1.0f);
    IncButton->DrawText = false;

    Rr_UITriangle(IncButton, Rr_UIPercent(1.0f, 1.0f), 0.0f);

    Rr_UIPop();

    float HandleBGOffset = HandleBG->Rect.Offset.Elements[Axis];
    float HandleBGSize = HandleBG->Rect.Extent.Elements[Axis];
    float HandleSize = Handle->Rect.Extent.Elements[Axis];

    if (Handle->Pressed)
    {
        gUI->DragValueStart.Offset.X = Item->Scroll.Elements[Axis];
    }

    if (HandleBG->Pressed)
    {
        /* Snap handle center to the cursor. */

        float Mouse = gUI->MousePosition.Elements[Axis];
        Mouse -= HandleSize * 0.5f;
        Mouse -= HandleBGOffset;
        Mouse /= HandleBGSize - HandleSize;
        float Scroll = Mouse * MaxScroll;
        Scroll = RR_CLAMP(0.0f, Scroll, MaxScroll);
        Item->Scroll.Elements[Axis] = Scroll;
        Item->ScrollDamp.Elements[Axis] = Scroll;

        gUI->DragValueStart.Offset.X = Item->Scroll.Elements[Axis];
    }

    if (Handle->Dragging || HandleBG->Dragging)
    {
        float HandleRatio = 1.0f;
        if (MaxScroll > 0.0f)
        {
            HandleRatio = (HandleBGSize - HandleSize) / MaxScroll;
        }

        float DragDelta;
        if (Handle->Dragging)
        {
            DragDelta = Handle->DragDelta.Elements[Axis];
        }
        if (HandleBG->Dragging)
        {
            DragDelta = HandleBG->DragDelta.Elements[Axis];
        }
        DragDelta /= HandleRatio;

        float Scroll = gUI->DragValueStart.Offset.X + DragDelta;
        Scroll = RR_CLAMP(0.0f, Scroll, MaxScroll);
        Item->Scroll.Elements[Axis] = Scroll;
        Item->ScrollDamp.Elements[Axis] = Scroll;
    }

    /* NOTE: Technically, using bar size as page size base is not correct. */

    if (DecButton->Clicked)
    {
        float HalfPage = Bar->Extent.Elements[Axis] * 0.5f;
        float Scroll = Item->Scroll.Elements[Axis] - HalfPage;
        Scroll = RR_CLAMP(0.0f, Scroll, MaxScroll);
        Item->Scroll.Elements[Axis] = Scroll;
    }

    if (IncButton->Clicked)
    {
        float HalfPage = Bar->Extent.Elements[Axis] * 0.5f;
        float Scroll = Item->Scroll.Elements[Axis] + HalfPage;
        Scroll = RR_CLAMP(0.0f, Scroll, MaxScroll);
        Item->Scroll.Elements[Axis] = Scroll;
    }

    return Bar;
}

Rr_UIWindow *Rr_UI2CreateWindow(char const *Name)
{
    Rr_UIWindow *Window = Rr_Alloc(sizeof(Rr_UIWindow), gUI->Arena);
    Window->Name = Rr_AllocCopy(Name, strlen(Name) + 1, gUI->Arena);
    Window->Rect.Offset = Rr_V2F(15.0f);
    Window->Rect.Extent = Rr_V2F(300.0f);
    *RR_PUSH_INTO_ARRAY(&gUI->Windows, gUI->Arena) = Window;

    return Window;
}

static inline void Rr_UIDrawTriangleEx(
    Rr_Rect Rect,
    uint32_t ClipIndex,
    Rr_UIDrawTriangleUnion Union)
{
    Rr_UIPrimitive Primitive = Rr_UI2ReservePrimitive(3, 3);

    float const FEATHER = 1.5f;

    float Size = RR_MIN(Rect.Extent.X, Rect.Extent.Y);
    Rr_Vec2 OriginalOffset = Rect.Offset;
    Rect = Rr_FitRect(Rect.Extent, Rr_V2(Size, Size));
    Rect.Offset = Rr_AddV2(Rect.Offset, OriginalOffset);
    Rect = Rr_ResizeRect(&Rect, -Size * 0.25f);
    Rect = Rr_ResizeRect(&Rect, -FEATHER);

    /* if (FlipX) */
    /* { */
    /*     Rect.Offset = Rr_AddV2(Rect.Offset, Rect.Extent); */
    /*     Rect.Extent = Rr_MulV2F(Rect.Extent, -1.0f); */
    /* } */

    Primitive.Vertices[0] = (Rr_UIVertex){
        .Offset = Rr_V2F(0.0f),
        .Color = Union.Params.Color,
        .ClipIndex = ClipIndex,
        .NoFloor = 1,
    };
    Primitive.Vertices[1] = (Rr_UIVertex){
        .Offset = Rr_V2(Rect.Extent.X, Rect.Extent.Y * 0.5f),
        .Color = Union.Params.Color,
        .ClipIndex = ClipIndex,
        .NoFloor = 1,
    };
    Primitive.Vertices[2] = (Rr_UIVertex){
        .Offset = Rr_V2(0.0f, Rect.Extent.Y),
        .Color = Union.Params.Color,
        .ClipIndex = ClipIndex,
        .NoFloor = 1,
    };

    for (int Index = 0; Index < 3; ++Index)
    {
        Rr_Vec2 Offset = Primitive.Vertices[Index].Offset;
        Offset = Rr_SubV2(Offset, Rr_MulV2F(Rect.Extent, 0.5f));
        Offset = Rr_RotateV2(Offset, RR_ANGLE_DEG(Union.Params.Angle));
        Offset = Rr_AddV2(Offset, Rr_MulV2F(Rect.Extent, 0.5f));
        Offset = Rr_AddV2(Offset, Rect.Offset);
        Primitive.Vertices[Index].Offset = Offset;
    }

    Primitive.Indices[0] = Primitive.BaseVertex;
    Primitive.Indices[1] = Primitive.BaseVertex + 1;
    Primitive.Indices[2] = Primitive.BaseVertex + 2;

    Rr_UIFeatherConvexPrimitive(&Primitive, 3, ClipIndex, FEATHER);
}

void Rr_UIDrawTriangle(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData)
{
    Rr_UIDrawTriangleUnion Union = { .DrawData = DrawData };
    Rr_UIDrawTriangleEx(Rect, ClipIndex, Union);
}

void Rr_UIDrawBevel(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData)
{
    uint32_t FillColor = (uint32_t)DrawData;
    Rr_UIDrawBevelEx(
        Rect,
        ClipIndex,
        RR_UI_BEVEL_THICKNESS,
        0xFFFFFFFF,
        0x000000FF,
        FillColor,
        FillColor);
}

void Rr_UIDrawInset(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData)
{
    static uint32_t const RR_UI_INSET_VERTEX_COUNT = 16;
    static uint32_t const RR_UI_INSET_INDEX_COUNT = 30;

    Rr_UIIndex BaseVertex = (Rr_UIIndex)gUI->VertexCount;
    Rr_UIVertex *Vertices = &gUI->Vertices[gUI->VertexCount];
    gUI->VertexCount += RR_UI_INSET_VERTEX_COUNT;
    Rr_UIIndex *Indices = &gUI->Indices[gUI->IndexCount];
    gUI->IndexCount += RR_UI_INSET_INDEX_COUNT;

    float Thickness = RR_UI_BEVEL_THICKNESS;

    uint32_t FillColor = (uint32_t)DrawData;
    uint32_t LightColor = 0xFFFFFFFF;
    uint32_t DarkColor = 0x000000FF;
    uint32_t HalfColor = 0x808080FF;

    Rr_Vec2 Offset = Rect.Offset;
    Rr_Vec2 Extent = Rect.Extent;

    Vertices[0] = (Rr_UIVertex){
        .Color = gUI->Colors[RR_UI_COLOR_BG],
    };
    Vertices[1] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X, 0.0f),
        .Color = gUI->Colors[RR_UI_COLOR_BG],
    };
    Vertices[2] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X, 0.0f),
        .Color = LightColor,
    };
    Vertices[3] = (Rr_UIVertex){
        .Offset = Rr_V2F(Thickness),
        .Color = DarkColor,
    };
    Vertices[4] = (Rr_UIVertex){
        .Offset = Rr_V2F(Thickness),
        .Color = FillColor,
    };
    Vertices[5] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - Thickness, Thickness),
        .Color = DarkColor,
    };
    Vertices[6] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - Thickness, Thickness),
        .Color = HalfColor, //
    };
    Vertices[7] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - Thickness, Thickness),
        .Color = FillColor,
    };
    Vertices[8] = (Rr_UIVertex){
        .Offset = Rr_V2(Thickness, Extent.Y - Thickness),
        .Color = DarkColor,
    };
    Vertices[9] = (Rr_UIVertex){
        .Offset = Rr_V2(Thickness, Extent.Y - Thickness),
        .Color = HalfColor, //
    };
    Vertices[10] = (Rr_UIVertex){
        .Offset = Rr_V2(Thickness, Extent.Y - Thickness),
        .Color = FillColor,
    };
    Vertices[11] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - Thickness, Extent.Y - Thickness),
        .Color = HalfColor, //
    };
    Vertices[12] = (Rr_UIVertex){
        .Offset = Rr_V2(Extent.X - Thickness, Extent.Y - Thickness),
        .Color = FillColor,
    };
    Vertices[13] = (Rr_UIVertex){
        .Offset = Rr_V2(0.0f, Extent.Y),
        .Color = gUI->Colors[RR_UI_COLOR_BG],
    };
    Vertices[14] = (Rr_UIVertex){
        .Offset = Rr_V2(0.0f, Extent.Y),
        .Color = LightColor,
    };
    Vertices[15] = (Rr_UIVertex){
        .Offset = Extent,
        .Color = LightColor,
    };

    for (size_t Index = 0; Index < RR_UI_INSET_VERTEX_COUNT; ++Index)
    {
        Vertices[Index].Offset = Rr_AddV2(Vertices[Index].Offset, Offset);
        // Vertices[Index].Offset = Rr_RoundV2(Vertices[Index].Offset);
        Vertices[Index].ClipIndex = ClipIndex;
    }

    static Rr_UIIndex const INSET_INDICES[] = { 0,  1,  5,  0,  5,  3, 0,  3,
                                                8,  0,  8,  13, 6,  2, 15, 6,
                                                15, 11, 9,  11, 15, 9, 15, 14,
                                                4,  7,  12, 4,  12, 10 };

    for (size_t Index = 0; Index < RR_UI_INSET_INDEX_COUNT; ++Index)
    {
        Indices[Index] = BaseVertex + INSET_INDICES[Index];
    }
}

static inline Rr_UIItem *Rr_UITitleBarButton(char const *Name)
{
    Rr_UIItem *ButtonRoot = Rr_UIGetItem(Name);
    ButtonRoot->Extents[RR_UI_AXIS_X] = Rr_UIEm(1.0f, 1.0f);
    ButtonRoot->Extents[RR_UI_AXIS_Y] = Rr_UIPercent(1.0f, 1.0f);
    ButtonRoot->Axis = RR_UI_AXIS_Y;
    ButtonRoot->Fill = true;
    ButtonRoot->MouseIgnored = true;

    Rr_UIPush(ButtonRoot);

    Rr_UISpacer(Rr_UIPercent(1.0f, 0.0f));

    Rr_UIItem *Button = Rr_UIButton(Name);
    Button->Extents[RR_UI_AXIS_X] = Rr_UIEm(0.85f, 1.0f);
    Button->Extents[RR_UI_AXIS_Y] = Rr_UIEm(0.85f, 1.0f);
    Button->DrawText = false;

    Rr_UISpacer(Rr_UIPercent(1.0f, 0.0f));

    Rr_UIPop();

    return Button;
}

static inline void Rr_UIAddWindowTitleBar(Rr_UIWindow *Window)
{
    bool HasFocus = gUI->FocusedNonPopupRoot == Rr_UILookupRoot(Window);

    Rr_UIItem *Bar = Rr_UIGetItem("Rr.UI.TitleBar");
    Bar->Extents[RR_UI_AXIS_X] = Rr_UIPercent(1.0f, 1.0f);
    Bar->Extents[RR_UI_AXIS_Y] = Rr_UIEm(1.2f, 1.0f);
    Bar->Axis = RR_UI_AXIS_Y;
    Bar->MouseClickable = true;
    Bar->DrawData = HasFocus;
    Bar->DrawFunc = Rr_UIDrawWindowTitleBar;

    if (Bar->Pressed)
    {
        Window->DragRect.Offset = Window->Rect.Offset;
    }
    else if (Bar->Dragged)
    {
        Rr_Vec2 Offset = Rr_AddV2(Window->DragRect.Offset, Bar->DragDelta);
        Window->Rect.Offset = Offset;
    }

    Rr_UIPush(Bar);

    Rr_UIItem *Buttons = Rr_UIGetItem("Buttons");
    Buttons->Extents[RR_UI_AXIS_X] = Rr_UIPercent(1.0f, 1.0f);
    Buttons->Extents[RR_UI_AXIS_Y] = Rr_UIPercent(1.0f, 1.0f);
    Buttons->Padding = Rr_V2F(0.0f);
    Buttons->MouseIgnored = true;
    Buttons->DrawText = true;
    Buttons->CenterText = true;
    Buttons->TextLength = strlen(Window->Name);
    Buttons->Text =
        Rr_AllocCopy(Window->Name, Buttons->TextLength + 1, gUI->FrameArena);
    Buttons->Text[Buttons->TextLength] = '\0';
    if (HasFocus)
    {
        Buttons->TextColor = RR_UI_COLOR_WHITE;
    }
    else
    {
        Buttons->TextColor = RR_UI_COLOR_FG;
    }

    Rr_UIPush(Buttons);

    Rr_UISpacer(Rr_UIPercent(1.0f, 0.0f));

    Rr_UIItem *Close = Rr_UITitleBarButton("Close");
    Rr_UIItem *Cross = Rr_UIGetItemEx(Close, "CloseCross");
    Cross->Extents[RR_UI_AXIS_X] = Rr_UIPercent(1.0f, 1.0f);
    Cross->Extents[RR_UI_AXIS_Y] = Rr_UIPercent(1.0f, 1.0f);
    Cross->MouseIgnored = true;
    Cross->DrawFunc = Rr_UIDrawCloseCross;
    Cross->DrawData = gUI->Colors[RR_UI_COLOR_FG];

    Rr_UIPop();

    Rr_UIPop();
}

static inline Rr_UIResizeType Rr_UIGetResizeType(Rr_Rect Rect)
{
    float Width = gUI->DefaultFont->LineHeight;
    Rr_Vec2 Offset = Rr_SubV2(gUI->MousePosition, Rect.Offset);
    if (Offset.X > Rect.Extent.X - Width)
    {
        if (Offset.Y < Width)
        {
            return RR_UI_RESIZE_TYPE_NE;
        }
        else if (Offset.Y > Rect.Extent.Y - Width)
        {
            return RR_UI_RESIZE_TYPE_SE;
        }
    }
    if (Offset.X < Width)
    {
        if (Offset.Y < Width)
        {
            return RR_UI_RESIZE_TYPE_NW;
        }
        else if (Offset.Y > Rect.Extent.Y - Width)
        {
            return RR_UI_RESIZE_TYPE_SW;
        }
    }
    if (Offset.X > Rect.Extent.X - Width)
    {
        return RR_UI_RESIZE_TYPE_E;
    }
    if (Offset.X < Width)
    {
        return RR_UI_RESIZE_TYPE_W;
    }
    if (Offset.Y > Rect.Extent.Y - Width)
    {
        return RR_UI_RESIZE_TYPE_S;
    }
    if (Offset.Y < Width)
    {
        return RR_UI_RESIZE_TYPE_N;
    }

    return RR_UI_RESIZE_TYPE_SE;
}

static inline Rr_CursorType Rr_UIGetResizeCursorType(Rr_UIResizeType ResizeType)
{
    switch (ResizeType)
    {
        case RR_UI_RESIZE_TYPE_NW:
        case RR_UI_RESIZE_TYPE_SE:
        {
            return RR_CURSOR_TYPE_RESIZE_NWSE;
        }
        break;
        case RR_UI_RESIZE_TYPE_NE:
        case RR_UI_RESIZE_TYPE_SW:
        {
            return RR_CURSOR_TYPE_RESIZE_NESW;
        }
        break;
        case RR_UI_RESIZE_TYPE_N:
        case RR_UI_RESIZE_TYPE_S:
        {
            return RR_CURSOR_TYPE_RESIZE_NS;
        }
        break;
        case RR_UI_RESIZE_TYPE_E:
        case RR_UI_RESIZE_TYPE_W:
        {
            return RR_CURSOR_TYPE_RESIZE_EW;
        }
        break;
        default:
        {
            return RR_CURSOR_TYPE_NORMAL;
        }
        break;
    }
}

static inline void Rr_UIHandleWindowResize(Rr_UIWindow *Window, Rr_UIItem *Root)
{
    Root->Padding = Rr_V2F(RR_UI_WINDOW_HANDLES);
    Root->MouseClickable = true;

    Rr_UIResizeType ResizeType = Rr_UIGetResizeType(Root->Rect);

    if (Root->Pressed)
    {
        Window->DragRect = Window->Rect;
        Window->ResizeType = ResizeType;
    }

    Root->HoveringCursor = Rr_UIGetResizeCursorType(ResizeType);
    Root->DraggingCursor = Rr_UIGetResizeCursorType(Window->ResizeType);

    if (!Root->Dragged)
    {
        return;
    }

    Rr_Vec2 Offset = Rr_AddV2(Window->DragRect.Offset, Root->DragDelta);
    Rr_Vec2 Extent = Rr_SubV2(Window->DragRect.Extent, Root->DragDelta);
    Rr_Vec2 AltExtent = Rr_AddV2(Window->DragRect.Extent, Root->DragDelta);
    switch (Window->ResizeType)
    {
        case RR_UI_RESIZE_TYPE_NW:
        {
            Window->Rect.Offset = Offset;
            Window->Rect.Extent = Extent;
        }
        break;
        case RR_UI_RESIZE_TYPE_SE:
        {
            Window->Rect.Extent = AltExtent;
        }
        break;
        case RR_UI_RESIZE_TYPE_NE:
        {
            Window->Rect.Offset.Y = Offset.Y;
            Window->Rect.Extent.Y = Extent.Y;
            Window->Rect.Extent.X = AltExtent.X;
        }
        break;
        case RR_UI_RESIZE_TYPE_SW:
        {
            Window->Rect.Offset.X = Offset.X;
            Window->Rect.Extent.X = Extent.X;
            Window->Rect.Extent.Y = AltExtent.Y;
        }
        break;
        case RR_UI_RESIZE_TYPE_N:
        {
            Window->Rect.Offset.Y = Offset.Y;
            Window->Rect.Extent.Y = Extent.Y;
        }
        break;
        case RR_UI_RESIZE_TYPE_S:
        {
            Window->Rect.Extent.Y = AltExtent.Y;
        }
        break;
        case RR_UI_RESIZE_TYPE_E:
        {
            Window->Rect.Extent.X = AltExtent.X;
        }
        break;
        case RR_UI_RESIZE_TYPE_W:
        {
            Window->Rect.Offset.X = Offset.X;
            Window->Rect.Extent.X = Extent.X;
        }
        break;
        default:
        {
        }
        break;
    }

    return;
}

Rr_UIItem *Rr_UIGetWindowItem(Rr_UIWindow *Window)
{
    Rr_UIItem *Root = Rr_UILookupRoot(Window);
    Root->Axis = RR_UI_AXIS_Y;
    Root->Padding = Rr_V2F(RR_UI_WINDOW_BORDER + RR_UI_WINDOW_HANDLES);
    Root->DrawFunc = Rr_UIDrawWindowBackground;

    if (gUI->FocusedNonPopupRoot == Root)
    {
        Window->ZOrder = INT32_MIN;
    }

    if (!Window->AutoExtent)
    {
        Rr_UIHandleWindowResize(Window, Root);
    }

    Rr_UIPush(Root);

    Rr_UIAddWindowTitleBar(Window);

    Rr_UIItem *Contents = Rr_UIGetItem("Rr.UI.Window.Contents");
    Contents->Axis = RR_UI_AXIS_Y;
    Contents->Extents[RR_UI_AXIS_X] = Rr_UIPercent(1.0f, 0.0f);
    Contents->Extents[RR_UI_AXIS_Y] = Rr_UIPercent(1.0f, 0.0f);
    Contents->Scrollable[RR_UI_AXIS_X] = true;
    Contents->Scrollable[RR_UI_AXIS_Y] = true;
    Contents->Padding = Rr_V2F(RR_UI_WINDOW_CONTENTS_PADDING);

    Rr_UIScrollbar("Rr.UI.Window.XScrollbar", Contents, RR_UI_AXIS_X);

    Rr_UIPop();

    return Contents;
}

Rr_UIItem *Rr_UIInfo(void)
{
    char *Buffer = Rr_AllocNoZero(256, gUI->FrameArena);
    size_t Length = (size_t)snprintf(
        Buffer,
        256,
        "Arena: %zu/%zu\n"
        "ClipRects: %d\n"
        "Vertices: %d\n"
        "Indices: %d\n"
        "Focused item: %s\n"
        "Hovered item: %s\n"
        "Dragging item: %s",
        gUI->Arena->Commited,
        gUI->Arena->Reserved,
        gUI->ClipRectCount,
        gUI->VertexCount,
        gUI->IndexCount,
        gUI->FocusedItem ? gUI->FocusedItem->Name : NULL,
        gUI->HoveredItem ? gUI->HoveredItem->Name : NULL,
        gUI->LeftMouseButton.Item ? gUI->LeftMouseButton.Item->Name : NULL);

    Rr_UIItem *Item = Rr_UIGetItem("Rr.UI.Info");
    Item->Extents[RR_UI_AXIS_X] = Rr_UIText(1.0f);
    Item->Extents[RR_UI_AXIS_Y] = Rr_UIText(1.0f);
    Item->Padding = Rr_V2F(4.0f);
    Item->DrawText = true;
    Item->Text = Buffer;
    Item->TextLength = Length;
    Item->DrawFunc = Rr_UIDrawBevel;
    Item->DrawData = gUI->Colors[RR_UI_COLOR_BG];

    return Item;
}
