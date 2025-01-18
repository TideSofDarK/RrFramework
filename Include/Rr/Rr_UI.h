#pragma once

#include <Rr/Rr_App.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void Rr_BeginWindow(Rr_App *App, const char *Title);

extern void Rr_EndWindow(Rr_App *App);

extern void Rr_Label(Rr_App *App, const char *Text);

extern void Rr_Button(Rr_App *App, const char *Text);

extern void Rr_BeginHorizontal(Rr_App *App);

extern void Rr_EndHorizontal(Rr_App *App);

#ifdef __cplusplus
}
#endif
