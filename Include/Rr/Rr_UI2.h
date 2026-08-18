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

#ifndef RR_UI2_H
#define RR_UI2_H

#include <Rr/Rr_Math.h>
#include <Rr/Rr_Platform.h>

typedef uint64_t Rr_UIHash;
typedef uint32_t Rr_UIIndex;
typedef struct Rr_UIFont Rr_UIFont;
typedef struct Rr_UIWindow Rr_UIWindow;

typedef enum
{
    RR_UI_COLOR_FG,
    RR_UI_COLOR_FG_DIMMED,
    RR_UI_COLOR_BG,
    RR_UI_COLOR_INPUT_FG,
    RR_UI_COLOR_INPUT_BG,
    RR_UI_COLOR_INPUT_SELECTION_FG,
    RR_UI_COLOR_INPUT_SELECTION_BG,
    RR_UI_COLOR_WHITE,
    RR_UI_COLOR_BLACK,
    RR_UI_COLOR_COUNT = 32,
} Rr_UIColor;

typedef uint32_t Rr_UIColors[RR_UI_COLOR_COUNT];

typedef enum
{
    RR_UI_EXTENT_TYPE_SUM,     /* Sums children extents. */
    RR_UI_EXTENT_TYPE_EM,      /* Absolute extent based on line height. */
    RR_UI_EXTENT_TYPE_PIXEL,   /* Absolute extent in pixels. */
    RR_UI_EXTENT_TYPE_TEXT,    /* Sums text extent and paddings. */
    RR_UI_EXTENT_TYPE_PERCENT, /* A percent of parents extent. */
} Rr_UIExtentType;

typedef enum
{
    RR_UI_AXIS_X,
    RR_UI_AXIS_Y,
    RR_UI_AXIS_COUNT,
} Rr_UIAxis;

typedef struct Rr_UIExtent Rr_UIExtent;
struct Rr_UIExtent
{
    Rr_UIExtentType Type;
    float Value; /* Not used if Type is set to SUM or TEXT. */
    float Rigid; /* How much is NOT dispensable (0 to 1). */
};

typedef struct Rr_UIVertex Rr_UIVertex;
struct Rr_UIVertex
{
    Rr_Vec2 Offset;
    Rr_Vec2 UV;
    uint32_t Color;
    uint32_t ClipIndex;
    uint32_t NoFloor;
    uint32_t Flags;
};

typedef struct Rr_UIPrimitive Rr_UIPrimitive;
struct Rr_UIPrimitive
{
    Rr_UIVertex *Vertices;
    Rr_UIIndex *Indices;
    Rr_UIIndex BaseVertex;
};

typedef void (
    *Rr_UIDrawFunc)(Rr_Rect Rect, uint32_t ClipRectIndex, uintptr_t DrawData);

typedef bool (*Rr_UIInputFieldFilterFunc)(size_t Length, char const *);

typedef struct Rr_UIItem Rr_UIItem;
struct Rr_UIItem
{
    /* Inputs to the layout engine.  */

    Rr_UIExtent Extents[RR_UI_AXIS_COUNT]; /* Extent rules for both axes. */
    bool Scrollable[RR_UI_AXIS_COUNT];     /* Scrolling rules for both axes. */
    Rr_UIAxis Axis;  /* Align children along X or Y axis. */
    Rr_Vec2 Padding; /* Space between the rect of this item and its children. */
    bool Fill; /* Whether to set non-flow axis extent to max among siblings. */

    bool MouseIgnored;   /* Completely ignore mouse (including hovering). */
    bool MouseClickable; /* Full press/click/drag/release support. */
    Rr_CursorType HoveringCursor;
    Rr_CursorType DraggingCursor;

    Rr_UIDrawFunc DrawFunc;
    uintptr_t DrawData; /* Interpretation is up to the draw function. */
    Rr_Vec2 DrawOffset;

    bool DrawText;
    bool InputText;
    bool CenterText;
    Rr_Vec2 TextOffset;
    Rr_UIColor TextColor;
    Rr_UIFont *Font;
    size_t TextLength; /* Can be left zero (strlen will be used). */
    char *Text;        /* Can be left NULL (item name will be used). */

    /* Response to the previous frame input. */

    Rr_Rect Rect; /* Calculated item rect in screen coordinates. */
    Rr_Vec2 DragDelta;
    size_t TextCursor; /* Byte offset to the codepoint under mouse cursor. */
    uint8_t Clicked;   /* Click sequence length (starts with a release). */
    uint8_t Pressed;   /* Press sequence length. */
    bool HasFocus;
    bool Hovering; /* Under mouse cursor. */
    bool Dragging; /* Pressed before. */
    bool Released; /* Pressed before + released anywhere. */
    bool Dragged;  /* Pressed before + mouse moved. */

    /* Persistent item state. */

    Rr_Vec2 Scroll;
    Rr_Vec2 ScrollDamp;

    /* Internals. */

    Rr_UIHash Hash;
    Rr_UIItem *HashChildren[4];
    Rr_UIItem *Parent;
    Rr_UIItem *First; /* First child. */
    Rr_UIItem *Last;  /* Last child. */
    Rr_UIItem *Next;  /* Next sibling. */
    size_t NameLength;
    char *Name;
    Rr_Vec2 Extent;         /* Used during layout calculations. */
    Rr_Vec2 TextExtent;     /* Used during layout calculations. */
    Rr_Vec2 ChildrenExtent; /* Used during layout calculations. */
    uint32_t ClipIndex;
    uint32_t InnerClipIndex;
    uint32_t StartBuildIndex;
    uint32_t EndBuildIndex;
};

typedef enum
{
    RR_UI_POPUP_ANCHOR_RIGHT,
    RR_UI_POPUP_ANCHOR_BOTTOM,
    RR_UI_POPUP_ANCHOR_ABSOLUTE,
} Rr_UIPopupAnchor;

typedef struct Rr_UIPopupInfo Rr_UIPopupInfo;
struct Rr_UIPopupInfo
{
    Rr_UIItem *Parent;
    Rr_Vec2 Offset;
    Rr_UIPopupAnchor Anchor;
};

typedef enum
{
    RR_UI_RESIZE_TYPE_NW,
    RR_UI_RESIZE_TYPE_N,
    RR_UI_RESIZE_TYPE_NE,
    RR_UI_RESIZE_TYPE_E,
    RR_UI_RESIZE_TYPE_W,
    RR_UI_RESIZE_TYPE_SW,
    RR_UI_RESIZE_TYPE_S,
    RR_UI_RESIZE_TYPE_SE,
} Rr_UIResizeType;

struct Rr_UIWindow
{
    char const *Name;

    bool AutoExtent;
    bool NoTitleBar;
    bool NoMove;

    int32_t ZOrder;

    Rr_Rect Rect;
    Rr_Rect DragRect;
    Rr_UIResizeType ResizeType;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Core */

extern Rr_UIExtent RR_CC Rr_UISum(float Rigid);

extern Rr_UIExtent RR_CC Rr_UIEm(float Value, float Rigid);

extern Rr_UIExtent RR_CC Rr_UIPixel(float Value, float Rigid);

extern Rr_UIExtent RR_CC Rr_UIText(float Rigid);

extern Rr_UIExtent RR_CC Rr_UIPercent(float Value, float Rigid);

extern void RR_CC Rr_UIReparentFirst(Rr_UIItem *Item);

extern void RR_CC Rr_UIPush(Rr_UIItem *Item);

extern void RR_CC Rr_UIPop(void);

extern Rr_UIItem *RR_CC Rr_UIGetItemEx(Rr_UIItem *Parent, char const *Name);

extern Rr_UIItem *RR_CC Rr_UIGetItem(char const *Name);

extern Rr_UIItem *RR_CC Rr_UIGetPopup(Rr_UIItem *Item);

extern Rr_UIItem *RR_CC Rr_UIGetHoverPopup(Rr_UIPopupInfo PopupInfo);

extern Rr_UIItem *RR_CC Rr_UIOpenPopup(Rr_UIPopupInfo PopupInfo);

extern void RR_CC Rr_UIClosePopup(Rr_UIItem *Item);

extern void RR_CC Rr_UIClosePopups(void);

/* Toolkit */

extern void RR_CC Rr_UISetDefaultColors(void);

extern void RR_CC
Rr_UIDrawRect(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData);

extern void RR_CC
Rr_UIDrawCheckerRect(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData);

extern void RR_CC
Rr_UIDrawWindowBackground(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData);

extern void RR_CC
Rr_UIDrawTriangle(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData);

extern void RR_CC
Rr_UIDrawBevel(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData);

extern void RR_CC
Rr_UIDrawInset(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData);

extern void RR_CC
Rr_UIDrawCloseCross(Rr_Rect Rect, uint32_t ClipIndex, uintptr_t DrawData);

extern Rr_UIItem *RR_CC Rr_UISpacer(Rr_UIExtent Extent);

extern Rr_UIItem *RR_CC Rr_UIButton(char const *Name);

extern Rr_UIItem *RR_CC
Rr_UIInputFieldV2(char const *Name, size_t BufferLength, char *Buffer);

extern Rr_UIItem *RR_CC Rr_UIPushContextMenu(char const *Name);

extern bool RR_CC Rr_UIContextMenuItem(char const *Name);

extern Rr_UIItem *RR_CC Rr_UIScrollbar(Rr_UIItem *Item, Rr_UIAxis Axis);

extern Rr_UIItem *RR_CC Rr_UIScrollView(bool ScrollableX, bool ScrollableY);

extern Rr_UIWindow *RR_CC Rr_UI2CreateWindow(char const *Name);

extern Rr_UIItem *RR_CC Rr_UIGetWindowItem(Rr_UIWindow *Window);

extern Rr_UIItem *RR_CC Rr_UIInfo(void);

#ifdef __cplusplus
}
#endif

#endif
