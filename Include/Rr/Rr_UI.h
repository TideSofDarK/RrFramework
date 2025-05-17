#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Asset.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Rr_Renderer;
struct Rr_String;

typedef struct Rr_UIContext Rr_UIContext;

typedef struct Rr_Font Rr_Font;

#define RR_TEXT_BUFFER_SIZE (1024 * 1024)
#define RR_TEXT_MAX_COLORS  8
#define RR_TEXT_MAX_GLYPHS  2048

typedef struct Rr_Glyph Rr_Glyph;
struct Rr_Glyph
{
    Rr_Vec4 AtlasBounds;
    Rr_Vec4 PlaneBounds;
};

struct Rr_Font
{
    Rr_Glyph Glyphs[RR_TEXT_MAX_GLYPHS];
    float Advances[RR_TEXT_MAX_GLYPHS];
    struct Rr_Image *Atlas;
    float LineHeight;
    float DefaultSize;
    float Advance;
    float DistanceRange;
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

extern Rr_Font *Rr_CreateFont(
    Rr_UIContext *Context,
    Rr_AssetRef FontPNGRef,
    Rr_AssetRef FontJSONRef);

extern void Rr_DestroyFont(Rr_UIContext *Context, Rr_Font *Font);

typedef enum
{
    RR_UI_TEXT_FLAGS_WRAPPED_BIT = (1 << 0),
} Rr_UITextFlagsBits;
typedef uint32_t Rr_UITextFlags;

Rr_Vec2 Rr_CalculateTextSize(
    Rr_Font *Font,
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
    RR_UI_WINDOW_FLAGS_CLOSE_BIT = (1 << 4),
} Rr_UIWindowFlagsBits;
typedef uint32_t Rr_UIWindowFlags;

typedef enum
{
    RR_UI_INPUT_FIELD_FLAGS_MULTILINE_BIT = (1 << 0),
} Rr_UIInputFieldFlagsBits;
typedef uint32_t Rr_UIInputFieldFlags;

extern void Rr_BeginWindow(const char *Title, Rr_UIWindowFlags Flags);

extern void Rr_EndWindow(void);

extern void Rr_Separator(void);

extern void Rr_LabelEx(const char *Text, Rr_UITextFlags Flags);

extern void Rr_Label(const char *Text);

extern void Rr_LabelF(const char *Format, ...);

extern bool Rr_Button(const char *Text);

extern bool Rr_Checkbox(const char *Text, bool *Checked);

extern bool Rr_InputField(
    size_t BufferSize,
    char *Buffer,
    Rr_UIInputFieldFlags Flags);

extern void Rr_BeginHorizontal(void);

extern void Rr_EndHorizontal(void);

extern void Rr_BeginTabs(const char *Title);

extern bool Rr_Tab(const char *Title);

extern void Rr_EndTabs(void);

extern bool Rr_WantMouseCapture(void);

extern bool Rr_WantKeyboardCapture(void);

extern void Rr_SetFontSize(float Size);

extern void Rr_DebugOverlay(void);

#ifdef __cplusplus
}
#endif
