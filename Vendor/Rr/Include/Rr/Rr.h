#pragma once

#include "RrTypes.h"
#include "RrAsset.h"

void Rr_Init(SRr* Rr, SDL_Window* Window);
void Rr_InitImGui(SRr* Rr, SDL_Window* Window);
void Rr_Cleanup(SRr* Rr);
void Rr_Draw(SRr* Rr);
b8 Rr_NewFrame(SRr* Rr, SDL_Window* Window);

void Rr_SetMesh(SRr* Rr, SRrRawMesh* RawMesh);

VkCommandBuffer Rr_BeginImmediate(SRr* Rr);
void Rr_EndImmediate(SRr* Rr);

SRrFrame* Rr_GetCurrentFrame(SRr* Rr);

VkPipeline Rr_BuildPipeline(SRr* Rr, SPipelineBuilder const* PipelineBuilder);
