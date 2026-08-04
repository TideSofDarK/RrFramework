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

#ifndef RR_UI_H
#define RR_UI_H

#include <Rr/Rr_App.h>
#include <Rr/Rr_Asset.h>

#define RR_UI_MIN_FONT_SIZE (6.0f)
#define RR_UI_MAX_FONT_SIZE (48.0f)

struct Rr_RHI;
struct Rr_String;
struct Rr_Graph;
struct Rr_Image;

typedef uint64_t Rr_UIHash;
typedef uint16_t Rr_UIIndex;

typedef struct Rr_UILayout Rr_UILayout;
typedef struct Rr_UIFont Rr_UIFont;

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
    Rr_Vec2 TitleBarPadding;
    Rr_Vec2 TabBarPadding;
    Rr_Vec2 WindowPadding;
    Rr_Vec2 ContentsMargin;
    float ComponentMargin;
    float ScrollbarAreaWidth;
    Rr_Vec2 ScrollbarHandleMargin;
    float TripleBevelThickness;
    float BevelIntensityLight;
    float BevelIntensityGray;
    float BevelIntensityDark;
    float AlignedWidgetTitleMargin;
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

    Rr_Vec4 ListEntryForeground;
    Rr_Vec4 ListEntryBackgroundA;
    Rr_Vec4 ListEntryBackgroundB;
    Rr_Vec4 ListEntryHoveredForeground;
    Rr_Vec4 ListEntryHoveredBackground;

    Rr_Vec4 TitleForeground;
    Rr_Vec4 TitleBackground;
    Rr_Vec4 TitleBackground2;
    Rr_Vec4 TitleBackgroundInactive;
    Rr_Vec4 TitleBackgroundInactive2;
    Rr_Vec4 TitleCloseButtonBackground;

    Rr_Vec4 ScrollbarBackground;
    Rr_Vec4 ScrollbarNormal;
    Rr_Vec4 ScrollbarHovered;
    Rr_Vec4 ScrollbarHeld;

    Rr_Vec4 ButtonNormal;
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

typedef enum
{
    RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT = 1U << 0,
    /* Cannot be collapsed. */
    RR_UI_WINDOW_FLAGS_NO_TITLE_BAR_BIT = 1U << 1,
    RR_UI_WINDOW_FLAGS_NO_HORIZONTAL_SCROLLBAR_BIT = 1U << 2,
    RR_UI_WINDOW_FLAGS_NO_VERTICAL_SCROLLBAR_BIT = 1U << 3,
    RR_UI_WINDOW_FLAGS_NO_MOVE_BIT = 1U << 4,
    RR_UI_WINDOW_FLAGS_NO_BORDERS_BIT = 1U << 5,
    RR_UI_WINDOW_FLAGS_NO_CLOSE_BIT = 1U << 6,
    RR_UI_WINDOW_FLAGS_NO_BACKGROUND_BIT = 1U << 7,
    RR_UI_WINDOW_FLAGS_AUTO_WIDTH_BIT = 1U << 8,
    RR_UI_WINDOW_FLAGS_AUTO_HEIGHT_BIT = 1U << 9,
    RR_UI_WINDOW_FLAGS_POPUP_BIT = 1U << 10,
    RR_UI_WINDOW_FLAGS_MENU_BAR_BIT = 1U << 11,
    RR_UI_WINDOW_FLAGS_ESCAPE_CLOSES_BIT = 1U << 12,
    RR_UI_WINDOW_FLAGS_AUTO_RESIZE =
        RR_UI_WINDOW_FLAGS_AUTO_WIDTH_BIT | RR_UI_WINDOW_FLAGS_AUTO_HEIGHT_BIT,
} Rr_UIWindowFlagsBits;
typedef uint32_t Rr_UIWindowFlags;

typedef enum
{
    RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT = 1U << 0,
    RR_UI_INPUT_FIELD_FLAGS_USE_PERSISTENT_BUFFER_BIT = 1U << 1,
    RR_UI_INPUT_FIELD_FLAGS_AUTO_SELECT_BIT = 1U << 2,
    RR_UI_INPUT_FIELD_FLAGS_AUTO_CENTER_BIT = 1U << 3,
    RR_UI_INPUT_FIELD_FLAGS_DRAG_BIT = 1U << 4,
    RR_UI_INPUT_FIELD_FLAGS_NO_BACKGROUND_BIT = 1U << 5,
} Rr_UIInputFieldFlagsBits;
typedef uint32_t Rr_UIInputFieldFlags;

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*Rr_UIInputFieldFilterFunc)(size_t Length, char const *);

extern Rr_UIFont *RR_CC
Rr_UICreateFont(size_t TTFSize, void const *TTFData, float FontSize);

extern Rr_UIFont *RR_CC Rr_UICreateFontRanges(
    size_t TTFSize,
    void const *TTFData,
    float FontSize,
    size_t CodepointRangeCount,
    Rr_UIRange const *CodepointRanges);

extern void RR_CC Rr_UIReleaseFont(Rr_UIFont *Font);

extern void RR_CC Rr_UIPushFont(Rr_UIFont *Font);

extern void RR_CC Rr_UIPopFont(void);

extern Rr_UIStyle *RR_CC Rr_UIGetStyle(void);

extern Rr_UIColors *RR_CC Rr_UIGetColors(void);

extern Rr_UIPrimitive RR_CC
Rr_UIReservePrimitive(size_t VertexCount, size_t IndexCount);

extern void RR_CC Rr_UIDrawTriangleVertices(Rr_UIVertex const *Vertices);

extern void RR_CC Rr_UIDrawTriangleFilled(
    Rr_Vec2 const *Positions,
    Rr_Vec4 const *Color,
    bool Feather);

extern void RR_CC Rr_UIDrawFitTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 const *Color);

extern void RR_CC Rr_UIDrawEquilateralTriangleFilled(
    Rr_Vec2 Offset,
    float Size,
    float Angle,
    Rr_Vec4 const *Color);

extern void RR_CC Rr_UIDrawCircle(
    Rr_Vec2 Offset,
    float Radius,
    float Thickness,
    Rr_Vec4 const *Color);

extern void RR_CC
Rr_UIDrawCircleFilled(Rr_Vec2 Offset, float Radius, Rr_Vec4 const *Color);

extern void RR_CC Rr_UIDrawQuadVertices(Rr_UIVertex const *Vertices);

extern Rr_Vec2 RR_CC Rr_UIGetCursor(void);

extern void RR_CC Rr_UIAdvance(Rr_Vec2 RigidSize);

extern void RR_CC Rr_UIPushID(char const *IDString);

extern void RR_CC Rr_UIPopID(void);

extern void RR_CC Rr_UIPushWindowPadding(Rr_Vec2 WindowPadding);

extern void RR_CC Rr_UIPopWindowPadding(void);

extern void RR_CC Rr_UIPushContentsMargin(Rr_Vec2 ContentsMargin);

extern void RR_CC Rr_UIPopContentsMargin(void);

extern void RR_CC Rr_UIPushFormatFloatDecimalPlaces(uint32_t Places);

extern void RR_CC Rr_UIPopFormatFloatDecimalPlaces(void);

extern void RR_CC Rr_UISetNextWindowOpenOffset(Rr_Vec2 Offset);

extern void RR_CC Rr_UISetNextWindowOffset(Rr_Vec2 Offset);

extern void RR_CC Rr_UISetNextWindowExtent(Rr_Vec2 Extent);

extern void RR_CC Rr_UISetNextWindowMinExtent(Rr_Vec2 Extent);

extern void RR_CC Rr_UISetNextWindowMaxExtent(Rr_Vec2 Extent);

extern bool RR_CC
Rr_UIBeginWindowEx(char const *Title, bool *Open, Rr_UIWindowFlags Flags);

extern bool RR_CC Rr_UIBeginWindow(char const *Title);

extern void RR_CC Rr_UIEndWindow(void);

extern void RR_CC Rr_UISetNextTreeExpanded(void);

extern void RR_CC Rr_UISetNextTreeCollapsed(void);

extern bool RR_CC Rr_UIBeginTree(char const *Title);

extern void RR_CC Rr_UIEndTree(void);

extern void RR_CC Rr_UIPushWidgetExtent(Rr_Vec2 Extent);

extern void RR_CC Rr_UIPopWidgetExtent(void);

extern void RR_CC Rr_UIPushWidgetWidth(float Width);

extern void RR_CC Rr_UIPopWidgetWidth(void);

extern void RR_CC Rr_UIPushWidgetHeight(float Height);

extern void RR_CC Rr_UIPopWidgetHeight(void);

extern void RR_CC Rr_UISeparator(void);

extern void RR_CC Rr_UIImageEx(
    struct Rr_Image *Image,
    Rr_Vec2 Extent,
    Rr_Vec2 UVMin,
    Rr_Vec2 UVMax);

// extern void RR_CC Rr_UIText(char const *Text);

extern void RR_CC Rr_UITextF(char const *Format, ...);

extern void RR_CC Rr_UITextWrapped(char const *Text);

extern void RR_CC Rr_UILabelText(char const *Title, char const *Text);

extern bool RR_CC Rr_UIButton(char const *Title);

extern bool RR_CC Rr_UIRadioButton(
    char const *Title,
    int32_t *SelectedOption,
    int32_t ThisOption);

extern bool RR_CC Rr_UICheckbox(char const *Title, bool *Checked);

extern bool RR_CC Rr_UIInputField(
    char const *Title,
    size_t BufferCapacity,
    char *Buffer,
    char const *Placeholder,
    Rr_UIInputFieldFilterFunc FilterFunc,
    Rr_UIInputFieldFlags Flags);

extern bool RR_CC
Rr_UIInputText(char const *Title, size_t BufferCapacity, char *Buffer);

extern bool RR_CC Rr_UIInputFloat(char const *Title, float *Value);
extern bool RR_CC
Rr_UIInputFloatRange(char const *Title, float *Value, float Min, float Max);
extern bool RR_CC Rr_UIInputFloatZO(char const *Title, float *Value);
extern bool RR_CC Rr_UIInputFloatNO(char const *Title, float *Value);

extern bool RR_CC Rr_UIInputFloat2(char const *Title, float *Values);
extern bool RR_CC Rr_UIInputFloat2Range(
    char const *Title,
    float *Values,
    float const *MinValues,
    float const *MaxValues);
extern bool RR_CC Rr_UIInputFloat2ZO(char const *Title, float *Values);
extern bool RR_CC Rr_UIInputFloat2NO(char const *Title, float *Values);

extern bool RR_CC Rr_UIInputFloat3(char const *Title, float *Values);
extern bool RR_CC Rr_UIInputFloat3Range(
    char const *Title,
    float *Values,
    float const *MinValues,
    float const *MaxValues);
extern bool RR_CC Rr_UIInputFloat3ZO(char const *Title, float *Values);
extern bool RR_CC Rr_UIInputFloat3NO(char const *Title, float *Values);

extern bool RR_CC Rr_UIInputFloat4(char const *Title, float *Values);
extern bool RR_CC Rr_UIInputFloat4Range(
    char const *Title,
    float *Values,
    float const *MinValues,
    float const *MaxValues);
extern bool RR_CC Rr_UIInputFloat4ZO(char const *Title, float *Values);
extern bool RR_CC Rr_UIInputFloat4NO(char const *Title, float *Values);

extern bool RR_CC Rr_UIInputFloat2x2(char const *Title, float *Values);

extern bool RR_CC Rr_UIInputFloat3x3(char const *Title, float *Values);

extern bool RR_CC Rr_UIInputFloat4x4(char const *Title, float *Values);

extern bool RR_CC Rr_UIInputDouble(char const *Title, double *Value);

extern bool RR_CC Rr_UIInputInt(char const *Title, int32_t *Value);

extern bool RR_CC
Rr_UIInputIntRange(char const *Title, int32_t *Value, int32_t Min, int32_t Max);

extern bool RR_CC Rr_UIInputInt2(char const *Title, int32_t *Values);
extern bool RR_CC Rr_UIInputInt2Range(
    char const *Title,
    int32_t *Values,
    int32_t const *MinValues,
    int32_t const *MaxValues);

extern bool RR_CC Rr_UIInputInt3(char const *Title, int32_t *Values);
extern bool RR_CC Rr_UIInputInt3Range(
    char const *Title,
    int32_t *Values,
    int32_t const *MinValues,
    int32_t const *MaxValues);

extern bool RR_CC Rr_UIInputInt4(char const *Title, int32_t *Values);
extern bool RR_CC Rr_UIInputInt4Range(
    char const *Title,
    int32_t *Values,
    int32_t const *MinValues,
    int32_t const *MaxValues);

extern bool RR_CC Rr_UIInputInt64(char const *Title, int64_t *Value);

extern bool RR_CC Rr_UIInputInt64Range(
    char const *Title,
    int64_t *Value,
    int64_t Min,
    int64_t Max);

extern bool RR_CC Rr_UIInputUnsignedInt(char const *Title, uint32_t *Value);

extern bool RR_CC Rr_UIInputUnsignedIntRange(
    char const *Title,
    uint32_t *Value,
    uint32_t Min,
    uint32_t Max);

extern bool RR_CC Rr_UIInputUnsignedInt64(char const *Title, uint64_t *Value);

extern bool RR_CC Rr_UIInputUnsignedInt64Range(
    char const *Title,
    uint64_t *Value,
    uint64_t Min,
    uint64_t Max);

extern bool RR_CC Rr_UIInputColor3(char const *Title, float *Channels);

extern bool RR_CC Rr_UIInputColor4(char const *Title, float *Channels);

extern bool RR_CC Rr_UICombobox(
    char const *Title,
    uint32_t OptionCount,
    char const *const *Options,
    uint32_t *SelectedIndex);

extern bool RR_CC
Rr_UISliderInt(char const *Title, int32_t *Value, int32_t Min, int32_t Max);

extern bool RR_CC Rr_UISliderUnsignedInt(
    char const *Title,
    uint32_t *Value,
    uint32_t Min,
    uint32_t Max);

extern bool RR_CC
Rr_UISliderFloat(char const *Title, float *Value, float Min, float Max);

extern void RR_CC Rr_UIBeginHorizontal(void);

extern void RR_CC Rr_UIEndHorizontal(void);

extern bool RR_CC Rr_UIWantMouseCapture(void);

extern bool RR_CC Rr_UIWantKeyboardCapture(void);

extern float RR_CC Rr_UICurrentFontSize(void);

extern float RR_CC Rr_UICurrentLineHeight(void);

extern void RR_CC Rr_UISetDefaultTheme(void);

extern void RR_CC Rr_UIDebugOverlay(void);

extern void RR_CC Rr_UIBeginDebugOverlayTabs(void);

extern void RR_CC Rr_UIEndDebugOverlayTabs(void);

#ifdef __cplusplus
}
#endif

#endif
