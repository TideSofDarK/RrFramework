#pragma once

#include <Rr/Rr_App.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_UIStyle Rr_UIStyle;
struct Rr_UIStyle
{
    float TitlePadding;
    float ContentsPadding;
    Rr_Vec4 OutlineColor;
    Rr_Vec4 TitleBGColor;
    Rr_Vec4 ContentsBGColor;
};

extern void Rr_BeginWindow(const char *Title);

extern void Rr_EndWindow(void);

extern void Rr_Label(const char *Text);

extern void Rr_Button(const char *Text);

extern void Rr_BeginHorizontal(void);

extern void Rr_EndHorizontal(void);

#ifdef __cplusplus
}
#endif
