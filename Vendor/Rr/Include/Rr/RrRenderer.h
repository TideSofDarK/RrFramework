#pragma once

#include "RrTypes.h"
#include "RrAsset.h"

typedef struct Rr_AppConfig Rr_AppConfig;

void Rr_Init(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_Cleanup(Rr_App* App);
void Rr_Draw(Rr_App* App);
b8 Rr_NewFrame(Rr_Renderer* Renderer, SDL_Window* Window);

void Rr_SetMesh(Rr_Renderer* Renderer, Rr_RawMesh* RawMesh);

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer);
void Rr_EndImmediate(Rr_Renderer* Renderer);

Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer);
void* Rr_GetCurrentFrameData(Rr_Renderer* Renderer);

VkPipeline Rr_BuildPipeline(Rr_Renderer* Renderer, Rr_PipelineBuilder const* PipelineBuilder);
