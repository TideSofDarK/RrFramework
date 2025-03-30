#pragma once

#include <Rr/Rr_Renderer.h>
#include <Rr/Rr_String.h>
#include <Rr/Rr_UI.h>

typedef struct Rr_UI Rr_UI;

extern Rr_UI *Rr_CreateUI(Rr_App *App);

extern void Rr_DestroyUI(Rr_App *App, Rr_UI *UI);

extern void Rr_BeginUI(Rr_App *App, Rr_UI *UI);

extern void Rr_EndUI(Rr_App *App, Rr_UI *UI);
