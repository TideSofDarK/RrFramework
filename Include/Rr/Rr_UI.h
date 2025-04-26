#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Asset.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Rr_Renderer;
struct Rr_String;

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
};

extern Rr_Font *Rr_CreateFont(
    struct Rr_Renderer *Renderer,
    Rr_AssetRef FontPNGRef,
    Rr_AssetRef FontJSONRef);

extern void Rr_DestroyFont(struct Rr_Renderer *Renderer, Rr_Font *Font);

Rr_Vec2 Rr_CalculateTextSize(
    Rr_Font *Font,
    float FontSize,
    struct Rr_String *String);

typedef enum
{
    RR_UI_WINDOW_FLAGS_NO_RESIZE_BIT = (1 << 0),
    RR_UI_WINDOW_FLAGS_NO_TITLE_BIT = (1 << 1),
    RR_UI_WINDOW_FLAGS_NO_MINIMIZE_BIT = (1 << 2),
} Rr_UIWindowFlagsBits;
typedef uint32_t Rr_UIWindowFlags;

extern void Rr_BeginWindow(const char *Title, Rr_UIWindowFlags Flags);

extern void Rr_EndWindow(void);

extern void Rr_Separator(void);

extern void Rr_Label(const char *Text);

extern void Rr_LabelF(const char *Format, ...);

extern void Rr_Button(const char *Text);

extern bool Rr_Checkbox(const char *Text, bool *Checked);

extern void Rr_BeginHorizontal(void);

extern void Rr_EndHorizontal(void);

extern bool Rr_WantMouseCapture(void);

extern bool Rr_WantKeyboardCapture(void);

#ifdef __cplusplus
}
#endif
