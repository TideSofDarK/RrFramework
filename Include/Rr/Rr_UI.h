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
struct Rr_Graph;

typedef uint64_t Rr_UIHash;
typedef uint16_t Rr_UIIndex;

typedef struct Rr_UIContext Rr_UIContext;

typedef struct Rr_UIFont Rr_UIFont;

#define RR_UI_MIN_FONT_SIZE (6.0f)
#define RR_UI_MAX_FONT_SIZE (48.0f)

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

typedef struct Rr_UIStyle Rr_UIStyle;
struct Rr_UIStyle
{
    Rr_Vec2 TitlePadding;
    Rr_Vec2 WindowPadding;
    Rr_Vec2 ContentsMargin;
    float ComponentMargin;
    float FlexibleTitleMargin;
    float BevelIntensityLight;
    float BevelIntensityDark;
    Rr_Vec2 ButtonPadding;
    Rr_Vec2 InputFieldPadding;
};

typedef struct Rr_UIColors Rr_UIColors;
struct Rr_UIColors
{
    Rr_Vec4 Foreground;
    Rr_Vec4 ForegroundDimmed;
    Rr_Vec4 Background;
    Rr_Vec4 ChildBackground;
    Rr_Vec4 Outline;

    Rr_Vec4 TitleBackground;
    Rr_Vec4 TitleCloseButtonBackground;
    Rr_Vec4 TitleCollapseButtonBackground;

    Rr_Vec4 ScrollbarBackground;
    Rr_Vec4 ScrollbarNormal;
    Rr_Vec4 ScrollbarHovered;
    Rr_Vec4 ScrollbarHeld;

    Rr_Vec4 ButtonNormal;
    Rr_Vec4 ButtonHovered;
    Rr_Vec4 ButtonHeld;
    Rr_Vec4 ButtonDisabled;

    Rr_Vec4 InputFieldNormal;
    Rr_Vec4 InputFieldActive;
    Rr_Vec4 SelectedTextBackground;
    Rr_Vec4 SelectedTextForeground;
};

extern Rr_UIFont *Rr_UICreateFont(
    size_t TTFSize,
    void const *TTFData,
    float FontSize);

extern void Rr_UIReleaseFont(Rr_UIFont *Font);

extern void Rr_UIPushFont(Rr_UIFont *Font);

extern void Rr_UIPopFont(void);

typedef enum
{
    RR_UI_TEXT_FLAGS_WRAPPED_BIT = (1 << 0),
} Rr_UITextFlagsBits;
typedef uint32_t Rr_UITextFlags;

typedef enum
{
    RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT = (1 << 0),
    RR_UI_WINDOW_FLAGS_NO_TITLE_BIT = (1 << 1),
    RR_UI_WINDOW_FLAGS_NO_MINIMIZE_BIT = (1 << 2),
    RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT = (1 << 3),
    RR_UI_WINDOW_FLAGS_NO_MOVE_BIT = (1 << 4),
    RR_UI_WINDOW_FLAGS_NO_BORDERS_BIT = (1 << 5),
    RR_UI_WINDOW_FLAGS_NO_COLLAPSE_BIT = (1 << 6),
    RR_UI_WINDOW_FLAGS_CLOSE_BIT = (1 << 7),
    RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT = (1 << 8),
} Rr_UIWindowFlagsBits;
typedef uint32_t Rr_UIWindowFlags;

typedef enum
{
    RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT = (1 << 0),
    RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT = (1 << 1),
    RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT = (1 << 2),
    RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT = (1 << 3),
} Rr_UIInputFieldFlagsBits;
typedef uint32_t Rr_UIInputFieldFlags;

typedef bool (*Rr_UIInputFieldFilterFunc)(size_t Length, const char *);

extern Rr_UIStyle *Rr_UIGetStyle(void);

extern Rr_UIColors *Rr_UIGetColors(void);

extern void Rr_UIRandomizeColors(void);

extern Rr_UIPrimitive Rr_UIReservePrimitive(
    size_t VertexCount,
    size_t IndexCount);

extern void Rr_UIDrawTriangleVertices(Rr_UIVertex *Vertices);

extern void Rr_UIDrawTriangleFilled(Rr_Vec2 *Positions, Rr_Vec4 *Color);

extern void Rr_UIDrawFitTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 *Color);

extern void Rr_UIDrawEquilateralTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 *Color);

extern void Rr_UIDrawCircle(
    Rr_Vec2 Offset,
    float Radius,
    float Thickness,
    Rr_Vec4 *Color);

extern void Rr_UIDrawCircleFilled(Rr_Vec2 Offset, float Radius, Rr_Vec4 *Color);

extern void Rr_UIDrawQuadVertices(Rr_UIVertex *Vertices);

extern Rr_Vec2 Rr_UIGetCursor(void);

extern void Rr_UIAdvance(Rr_Vec2 Size);

extern void Rr_UIPushID(const char *IDString);

extern void Rr_UIPopID(void);

extern void Rr_UIPushWindowPadding(Rr_Vec2 WindowPadding);

extern void Rr_UIPopWindowPadding(void);

extern void Rr_UIPushContentsMargin(Rr_Vec2 ContentsMargin);

extern void Rr_UIPopContentsMargin(void);

extern void Rr_UIPushFormatFloatDecimalPlaces(uint32_t Places);

extern void Rr_UIPopFormatFloatDecimalPlaces(void);

extern void Rr_UISetNextWindowOffset(Rr_Vec2 Offset);

extern void Rr_UISetNextWindowOpenOffset(Rr_Vec2 Offset);

extern void Rr_UISetNextWindowExtent(Rr_Vec2 Extent);

extern void Rr_UISetNextWindowCreateCollapsed(bool Collapsed);

extern bool Rr_UIBeginWindow(
    const char *Title,
    bool *Open,
    Rr_UIWindowFlags Flags);

extern void Rr_UIEndWindow(void);

extern bool Rr_UIBeginChild(const char *Title);

extern void Rr_UIEndChild(void);

extern void Rr_UISetNextTreeExpanded(void);

extern void Rr_UISetNextTreeCollapsed(void);

extern bool Rr_UIBeginTree(const char *Title);

extern void Rr_UIEndTree(void);

extern void Rr_UIPushWidgetExtent(Rr_Vec2 Extent);

extern void Rr_UIPopWidgetExtent(void);

extern void Rr_UISeparator(void);

extern void Rr_UITextEx(const char *Text, Rr_UITextFlags Flags);

extern void Rr_UIText(const char *Text);

extern void Rr_UITextF(const char *Format, ...);

extern void Rr_UILabelText(const char *Title, const char *Text);

extern bool Rr_UIButton(const char *Title);

extern bool Rr_UIRadioButton(
    const char *Title,
    int32_t *SelectedOption,
    int32_t ThisOption);

extern bool Rr_UICheckbox(const char *Title, bool *Checked);

extern bool Rr_UIInputField(
    const char *Title,
    size_t BufferCapacity,
    char *Buffer,
    const char *Placeholder,
    Rr_UIInputFieldFilterFunc FilterFunc,
    Rr_UIInputFieldFlags Flags);

extern bool Rr_UIInputText(
    const char *Title,
    size_t BufferCapacity,
    char *Buffer);

extern bool Rr_UIInputFloat(const char *Title, float *Value);

extern bool Rr_UIInputFloat2(const char *Title, float *Values);

extern bool Rr_UIInputFloat3(const char *Title, float *Values);

extern bool Rr_UIInputFloat4(const char *Title, float *Values);

extern bool Rr_UIInputFloat2x2(const char *Title, float *Values);

extern bool Rr_UIInputFloat3x3(const char *Title, float *Values);

extern bool Rr_UIInputFloat4x4(const char *Title, float *Values);

extern bool Rr_UIInputInt(const char *Title, int32_t *Value);

extern bool Rr_UIInputInt2(const char *Title, int32_t *Values);

extern bool Rr_UIInputInt3(const char *Title, int32_t *Values);

extern bool Rr_UIInputInt4(const char *Title, int32_t *Values);

extern bool Rr_UIInputUnsignedInt(const char *Title, uint32_t *Value);

extern bool Rr_UIInputColor3(const char *Title, float *Channels);

extern bool Rr_UIInputColor4(const char *Title, float *Channels);

extern bool Rr_UICombobox(
    const char *Title,
    uint32_t OptionCount,
    const char *const *Options,
    uint32_t *SelectedIndex);

extern bool Rr_UISliderInt(
    const char *Title,
    int32_t *Value,
    int32_t Min,
    int32_t Max);

extern bool Rr_UISliderFloat(
    const char *Title,
    float *Value,
    float Min,
    float Max);

extern void Rr_UIBeginHorizontal(void);

extern void Rr_UIEndHorizontal(void);

extern void Rr_UIBeginTabs(const char *Title);

extern bool Rr_UITab(const char *Title);

extern void Rr_UIEndTabs(void);

extern bool Rr_UIWantMouseCapture(void);

extern bool Rr_UIWantKeyboardCapture(void);

extern float Rr_UICurrentFontSize(void);

extern float Rr_UICurrentLineHeight(void);

extern void Rr_UIPrintColors(void);

extern void Rr_UIDebugOverlay(void);

#ifdef __cplusplus
}
#endif
