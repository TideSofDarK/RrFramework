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

#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Asset.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Rr_Renderer;
struct Rr_String;

typedef struct Rr_UIContext Rr_UIContext;

typedef struct Rr_UIFont Rr_UIFont;

#define RR_TEXT_BUFFER_SIZE (1024 * 1024)
#define RR_TEXT_MAX_COLORS  8
#define RR_TEXT_MAX_GLYPHS  2048

typedef struct Rr_UIGlyph Rr_UIGlyph;
struct Rr_UIGlyph
{
    Rr_Vec4 AtlasBounds;
    Rr_Vec4 PlaneBounds;
};

struct Rr_UIFont
{
    Rr_UIGlyph Glyphs[RR_TEXT_MAX_GLYPHS];
    float Advances[RR_TEXT_MAX_GLYPHS];
    struct Rr_Image *Atlas;
    float LineHeight;
    float DefaultSize;
    float Advance;
    float DistanceRange;
    float UnderlineY;
    float UnderlineThickness;
};

typedef struct Rr_UIStyle Rr_UIStyle;
struct Rr_UIStyle
{
    Rr_Vec2 TitlePadding;
    Rr_Vec2 ContentsPadding;

    Rr_Vec4 Foreground;
    Rr_Vec4 Background;
    Rr_Vec4 TitleBackground;
    Rr_Vec4 Outline;

    Rr_Vec4 ScrollbarBackground;
    Rr_Vec4 ScrollbarNormal;
    Rr_Vec4 ScrollbarHovered;
    Rr_Vec4 ScrollbarHeld;

    Rr_Vec4 ButtonNormal;
    Rr_Vec4 ButtonHovered;
    Rr_Vec4 ButtonHeld;
    Rr_Vec4 ButtonDisabled;
};

extern Rr_UIFont *Rr_UICreateFont(
    Rr_UIContext *Context,
    Rr_AssetRef FontPNGRef,
    Rr_AssetRef FontJSONRef);

extern void Rr_UIDestroyFont(Rr_UIContext *Context, Rr_UIFont *Font);

typedef enum
{
    RR_UI_TEXT_FLAGS_WRAPPED_BIT = (1 << 0),
} Rr_UITextFlagsBits;
typedef uint32_t Rr_UITextFlags;

Rr_Vec2 Rr_UICalculateTextSize(
    Rr_UIFont *Font,
    float FontSize,
    struct Rr_String *String,
    float AvailableWidth,
    Rr_UITextFlags Flags);

typedef enum
{
    RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT = (1 << 0),
    RR_UI_WINDOW_FLAGS_NO_TITLE_BIT = (1 << 1),
    RR_UI_WINDOW_FLAGS_NO_MINIMIZE_BIT = (1 << 2),
    RR_UI_WINDOW_FLAGS_NO_SCROLLBAR_BIT = (1 << 3),
    RR_UI_WINDOW_FLAGS_NO_MOVE_BIT = (1 << 4),
    RR_UI_WINDOW_FLAGS_NO_BORDER_BIT = (1 << 5),
    RR_UI_WINDOW_FLAGS_CLOSE_BIT = (1 << 6),
    RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT = (1 << 7),
} Rr_UIWindowFlagsBits;
typedef uint32_t Rr_UIWindowFlags;

typedef enum
{
    RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT = (1 << 0),
} Rr_UIInputFieldFlagsBits;
typedef uint32_t Rr_UIInputFieldFlags;

extern Rr_UIStyle *Rr_UIGetStyle(void);

extern void Rr_UISetNextWindowPosition(Rr_Vec2 Position);

extern void Rr_UISetNextWindowSize(Rr_Vec2 Size);

extern void Rr_UISetNextWindowPadding(Rr_Vec2 Padding);

extern bool Rr_UIBeginWindow(const char *Title, bool *Open, Rr_UIWindowFlags Flags);

extern void Rr_UIEndWindow(void);

extern bool Rr_UIFold(const char *Title);

extern void Rr_UISeparator(void);

extern void Rr_UILabelEx(const char *Text, Rr_UITextFlags Flags);

extern void Rr_UILabel(const char *Text);

extern void Rr_UILabelF(const char *Format, ...);

extern bool Rr_UIButton(const char *Title);

extern bool Rr_UICheckbox(const char *Title, bool *Checked);

extern bool Rr_UIInputField(
    size_t BufferSize,
    char *Buffer,
    Rr_UIInputFieldFlags Flags);

extern bool Rr_UICombobox(
    const char *Title,
    uint32_t OptionCount,
    const char **Options,
    uint32_t *SelectedIndex);

extern bool Rr_UIColorPicker(const char *Title, Rr_Vec4 *Color);

extern void Rr_UIBeginHorizontal(void);

extern void Rr_UIEndHorizontal(void);

extern void Rr_UIBeginTabs(const char *Title);

extern bool Rr_UITab(const char *Title);

extern void Rr_UIEndTabs(void);

extern bool Rr_UIWantMouseCapture(void);

extern bool Rr_UIWantKeyboardCapture(void);

extern void Rr_UISetFontSize(float Size);

extern void Rr_UIDebugOverlay(void);

#ifdef __cplusplus
}
#endif
