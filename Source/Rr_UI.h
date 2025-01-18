#pragma once

#include <Rr/Rr_String.h>
#include <Rr/Rr_UI.h>

typedef struct Rr_UI Rr_UI;
struct Rr_UI
{
    struct Rr_Arena *Arena;
    struct Rr_UIObject *UIObject;
};

extern Rr_UI *Rr_CreateUI(Rr_App *App);

extern void Rr_DestroyUI(Rr_App *App, Rr_UI *UI);

extern void Rr_ResetUI(Rr_App *App, Rr_UI *UI);

extern void Rr_DrawUI(Rr_App *App, Rr_UI *UI);
