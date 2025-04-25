#pragma once

#include <Rr/Rr_Platform.h>
#include <Rr/Rr_Renderer.h>
#include <Rr/Rr_String.h>
#include <Rr/Rr_UI.h>

typedef struct Rr_UIContext Rr_UIContext;

extern Rr_UIContext *Rr_CreateUIContext(Rr_App *App);

extern void Rr_DestroyUIContext(Rr_App *App, Rr_UIContext *UI);

extern void Rr_ProcessUIEvent(Rr_App *App, Rr_Event *Event);

extern void Rr_BeginUI(Rr_App *App, Rr_UIContext *UI);

extern void Rr_EndUI(Rr_App *App);
