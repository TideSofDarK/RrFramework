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

typedef struct Rr_UIRange Rr_UIRange;
struct Rr_UIRange
{
    int32_t First;
    int32_t Last;
};

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
    float FrameThickness;
    Rr_Vec2 TitleBarPadding;
    Rr_Vec2 WindowPadding;
    Rr_Vec2 ContentsMargin;
    float ComponentMargin;
    float ScrollbarAreaWidth;
    float BevelThickness;
    float DoubleBevelThickness;
    float BevelIntensityLight;
    float BevelIntensityDark;
    float FlexibleTitleMargin;
    Rr_Vec2 ButtonPadding;
    Rr_Vec2 InputFieldPadding;
    Rr_Vec2 CheckmarkRatios;
    float CheckmarkSize;
    float CrossWidth;
    float CrossThickness;
};

typedef struct Rr_UIColors Rr_UIColors;
struct Rr_UIColors
{
    Rr_Vec4 Foreground;
    Rr_Vec4 ForegroundDimmed;
    Rr_Vec4 Background;
    Rr_Vec4 ChildBackground;
    Rr_Vec4 ScrolloffBackground;
    Rr_Vec4 Outline;
    Rr_Vec4 SelectedOutline;
    Rr_Vec4 ListEntryBackgroundA;
    Rr_Vec4 ListEntryBackgroundB;
    Rr_Vec4 ListEntryHovered;

    Rr_Vec4 TitleForeground;
    Rr_Vec4 TitleBackground;
    Rr_Vec4 TitleBackground2;
    Rr_Vec4 TitleBackgroundInactive;
    Rr_Vec4 TitleBackgroundTabs;
    Rr_Vec4 TitleCloseButtonBackground;
    Rr_Vec4 TitleCollapseButtonBackground;

    Rr_Vec4 ScrollbarBackground;
    Rr_Vec4 ScrollbarNormal;
    Rr_Vec4 ScrollbarHovered;
    Rr_Vec4 ScrollbarHeld;
    Rr_Vec4 ResizeHandleNormal;
    Rr_Vec4 ResizeHandleHovered;
    Rr_Vec4 ResizeHandleHeld;

    Rr_Vec4 ButtonNormal;
    Rr_Vec4 ButtonHovered;
    Rr_Vec4 ButtonHeld;
    Rr_Vec4 ButtonDisabled;

    Rr_Vec4 ComboboxButtonNormal;
    Rr_Vec4 ComboboxButtonHeld;
    Rr_Vec4 ComboboxButtonActive;

    Rr_Vec4 RadioButtonNormal;
    Rr_Vec4 RadioButtonOutline;
    Rr_Vec4 RadioButtonHeld;

    Rr_Vec4 InputFieldNormal;
    Rr_Vec4 InputFieldActive;
    Rr_Vec4 SelectedTextBackground;
    Rr_Vec4 SelectedTextForeground;
};

extern Rr_UIFont *Rr_UICreateFont(
    size_t TTFSize,
    void const *TTFData,
    float FontSize);

extern Rr_UIFont *Rr_UICreateFontRanges(
    size_t TTFSize,
    void const *TTFData,
    float FontSize,
    size_t CodepointRangeCount,
    Rr_UIRange const *CodepointRanges);

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
    /* Cannot be collapsed. */
    RR_UI_WINDOW_FLAGS_NO_TITLE_BAR_BIT = (1 << 1),
    RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT = (1 << 3),
    RR_UI_WINDOW_FLAGS_NO_MOVE_BIT = (1 << 4),
    RR_UI_WINDOW_FLAGS_NO_BORDERS_BIT = (1 << 5),
    RR_UI_WINDOW_FLAGS_NO_COLLAPSE_BIT = (1 << 6),
    RR_UI_WINDOW_FLAGS_NO_CLOSE_BIT = (1 << 7),
    RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT = (1 << 8),
    RR_UI_WINDOW_FLAGS_ESCAPE_CLOSES_BIT = (1 << 9),
    RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT = (1 << 10),
} Rr_UIWindowFlagsBits;
typedef uint32_t Rr_UIWindowFlags;

typedef enum
{
    RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT = (1 << 0),
    RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT = (1 << 1),
    RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT = (1 << 2),
    RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT = (1 << 3),
    RR_UI_INPUT_FIELD_FLAGS_DRAG_BIT = (1 << 4),
    RR_UI_INPUT_FIELD_FLAGS_NO_BACKGROUND_BIT = (1 << 5),
} Rr_UIInputFieldFlagsBits;
typedef uint32_t Rr_UIInputFieldFlags;

typedef bool (*Rr_UIInputFieldFilterFunc)(size_t Length, char const *);

extern Rr_UIStyle *Rr_UIGetStyle(void);

extern Rr_UIColors *Rr_UIGetColors(void);

extern Rr_UIPrimitive Rr_UIReservePrimitive(
    size_t VertexCount,
    size_t IndexCount);

extern void Rr_UIDrawTriangleVertices(Rr_UIVertex const *Vertices);

extern void Rr_UIDrawTriangleFilled(
    Rr_Vec2 const *Positions,
    Rr_Vec4 const *Color);

extern void Rr_UIDrawFitTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 const *Color);

extern void Rr_UIDrawEquilateralTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 const *Color);

extern void Rr_UIDrawCircle(
    Rr_Vec2 Offset,
    float Radius,
    float Thickness,
    Rr_Vec4 const *Color);

extern void Rr_UIDrawCircleFilled(
    Rr_Vec2 Offset,
    float Radius,
    Rr_Vec4 const *Color);

extern void Rr_UIDrawQuadVertices(Rr_UIVertex const *Vertices);

extern Rr_Vec2 Rr_UIGetCursor(void);

extern void Rr_UIAdvance(Rr_Vec2 RigidSize, Rr_Vec2 FlexibleSize);

extern void Rr_UIPushID(char const *IDString);

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

extern bool Rr_UIBeginWindowEx(
    char const *Title,
    bool *Open,
    Rr_UIWindowFlags Flags);

extern bool Rr_UIBeginWindow(char const *Title);

extern void Rr_UIEndWindow(void);

extern bool Rr_UIBeginTabs(char const *Title);

extern void Rr_UIEndTabs(void);

extern void Rr_UISetNextTreeExpanded(void);

extern void Rr_UISetNextTreeCollapsed(void);

extern bool Rr_UIBeginTree(char const *Title);

extern void Rr_UIEndTree(void);

extern void Rr_UIPushWidgetExtent(Rr_Vec2 Extent);

extern void Rr_UIPopWidgetExtent(void);

extern void Rr_UISeparator(void);

extern void Rr_UITextEx(char const *Text, Rr_UITextFlags Flags);

extern void Rr_UIText(char const *Text);

extern void Rr_UITextF(char const *Format, ...);

extern void Rr_UILabelText(char const *Title, char const *Text);

extern bool Rr_UIButton(char const *Title);

extern bool Rr_UIRadioButton(
    char const *Title,
    int32_t *SelectedOption,
    int32_t ThisOption);

extern bool Rr_UICheckbox(char const *Title, bool *Checked);

extern bool Rr_UIInputField(
    char const *Title,
    size_t BufferCapacity,
    char *Buffer,
    char const *Placeholder,
    Rr_UIInputFieldFilterFunc FilterFunc,
    Rr_UIInputFieldFlags Flags);

extern bool Rr_UIInputText(
    char const *Title,
    size_t BufferCapacity,
    char *Buffer);

extern bool Rr_UIInputFloat(char const *Title, float *Value);
extern bool Rr_UIInputFloatRange(
    char const *Title,
    float *Value,
    float Min,
    float Max);
extern bool Rr_UIInputFloatZO(char const *Title, float *Value);
extern bool Rr_UIInputFloatNO(char const *Title, float *Value);

extern bool Rr_UIInputFloat2(char const *Title, float *Values);
extern bool Rr_UIInputFloat2Range(
    char const *Title,
    float *Values,
    float const *MinValues,
    float const *MaxValues);
extern bool Rr_UIInputFloat2ZO(char const *Title, float *Values);
extern bool Rr_UIInputFloat2NO(char const *Title, float *Values);

extern bool Rr_UIInputFloat3(char const *Title, float *Values);
extern bool Rr_UIInputFloat3Range(
    char const *Title,
    float *Values,
    float const *MinValues,
    float const *MaxValues);
extern bool Rr_UIInputFloat3ZO(char const *Title, float *Values);
extern bool Rr_UIInputFloat3NO(char const *Title, float *Values);

extern bool Rr_UIInputFloat4(char const *Title, float *Values);
extern bool Rr_UIInputFloat4Range(
    char const *Title,
    float *Values,
    float const *MinValues,
    float const *MaxValues);
extern bool Rr_UIInputFloat4ZO(char const *Title, float *Values);
extern bool Rr_UIInputFloat4NO(char const *Title, float *Values);

extern bool Rr_UIInputFloat2x2(char const *Title, float *Values);

extern bool Rr_UIInputFloat3x3(char const *Title, float *Values);

extern bool Rr_UIInputFloat4x4(char const *Title, float *Values);

extern bool Rr_UIInputInt(char const *Title, int32_t *Value);

extern bool Rr_UIInputIntRange(
    char const *Title,
    int32_t *Value,
    int32_t Min,
    int32_t Max);

extern bool Rr_UIInputInt2(char const *Title, int32_t *Values);

extern bool Rr_UIInputInt3(char const *Title, int32_t *Values);

extern bool Rr_UIInputInt4(char const *Title, int32_t *Values);

extern bool Rr_UIInputUnsignedInt(char const *Title, uint32_t *Value);

extern bool Rr_UIInputUnsignedIntRange(
    char const *Title,
    uint32_t *Value,
    uint32_t Min,
    uint32_t Max);

extern bool Rr_UIInputColor3(char const *Title, float *Channels);

extern bool Rr_UIInputColor4(char const *Title, float *Channels);

extern bool Rr_UICombobox(
    char const *Title,
    uint32_t OptionCount,
    char const *const *Options,
    uint32_t *SelectedIndex);

extern bool Rr_UISliderInt(
    char const *Title,
    int32_t *Value,
    int32_t Min,
    int32_t Max);

extern bool Rr_UISliderUnsignedInt(
    char const *Title,
    uint32_t *Value,
    uint32_t Min,
    uint32_t Max);

extern bool Rr_UISliderFloat(
    char const *Title,
    float *Value,
    float Min,
    float Max);

extern void Rr_UIBeginHorizontal(void);

extern void Rr_UIEndHorizontal(void);

extern bool Rr_UIWantMouseCapture(void);

extern bool Rr_UIWantKeyboardCapture(void);

extern float Rr_UICurrentFontSize(void);

extern float Rr_UICurrentLineHeight(void);

extern void Rr_UISetDefaultTheme(void);

extern void Rr_UIDebugOverlay(void);

#ifdef __cplusplus
}
#endif
