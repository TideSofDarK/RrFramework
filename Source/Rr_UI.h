#pragma once

#include <Rr/Rr_Platform.h>
#include <Rr/Rr_Renderer.h>
#include <Rr/Rr_String.h>
#include <Rr/Rr_UI.h>

extern Rr_UIContext *Rr_CreateUIContext(Rr_App *App);

extern void Rr_DestroyUIContext(Rr_App *App, Rr_UIContext *UI);

extern void Rr_ProcessUIEvent(Rr_App *App, Rr_Event *Event);

extern void Rr_BeginUI(Rr_UIContext *UI);

extern void Rr_EndUI(void);
