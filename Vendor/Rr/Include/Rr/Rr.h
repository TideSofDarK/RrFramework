#pragma once

#include "RrTypes.h"
#include "RrAsset.h"

void Rr_Init(SRr* Rr, SDL_Window* Window);
void Rr_InitImGui(SRr* Rr, SDL_Window* Window);
void Rr_Cleanup(SRr* Rr);
void Rr_Draw(SRr* Rr);
void Rr_NewFrame(SRr* Rr, SDL_Window* Window);
void Rr_SetMesh(SRr* Rr, SRrRawMesh* RawMesh);
