#pragma once

#include "RrTypes.h"
#include "RrAsset.h"

typedef struct Rr_AppConfig Rr_AppConfig;
typedef struct Rr_Pipeline Rr_Pipeline;

void Rr_Init(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_Cleanup(Rr_App* App);
void Rr_Draw(Rr_App* App);
b8 Rr_NewFrame(Rr_Renderer* Renderer, SDL_Window* Window);

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer);
void Rr_EndImmediate(Rr_Renderer* Renderer);

Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer);
void* Rr_GetCurrentFrameData(Rr_Renderer* Renderer);

void Rr_BeginRendering(Rr_Renderer* Renderer, Rr_Pipeline* Pipeline);
void Rr_EndRendering(Rr_Renderer* Renderer);

static inline float Rr_GetAspectRatio(Rr_Renderer* Renderer)
{
    return (float)Renderer->DrawTarget.ActiveResolution.width / (float)Renderer->DrawTarget.ActiveResolution.height;
}
