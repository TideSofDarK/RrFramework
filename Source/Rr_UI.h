#pragma once

#include <Rr/Rr_Renderer.h>
#include <Rr/Rr_String.h>
#include <Rr/Rr_UI.h>

typedef struct Rr_UI Rr_UI;

extern Rr_UI *Rr_CreateUI(void);

extern void Rr_DestroyUI(Rr_UI *UI);

extern void Rr_BeginUI(Rr_Renderer *Renderer, Rr_UI *UI);

extern void Rr_DrawUI(Rr_Renderer *Renderer, Rr_UI *UI);
