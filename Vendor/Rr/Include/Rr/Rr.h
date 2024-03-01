#pragma once

#include "RrTypes.h"
#include "RrArray.h"
#include "RrAsset.h"

void Rr_Init(SRr* Rr, SDL_Window* Window);
void Rr_InitImGui(SRr* Rr, SDL_Window* Window);
void Rr_Cleanup(SRr* Rr);
void Rr_Draw(SRr* Rr);
void Rr_Resize(SRr* Rr, u32 Width, u32 Height);
void Rr_SetMesh(SRr* Rr, SRrRawMesh* RawMesh);
