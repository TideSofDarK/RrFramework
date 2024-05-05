#pragma once

#include "Rr_Vulkan.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_Frame Rr_Frame;

void Rr_InitRenderer(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_CleanupRenderer(Rr_App* App);
void Rr_Draw(Rr_App* App);
bool Rr_NewFrame(Rr_Renderer* Renderer, SDL_Window* Window);

VkCommandBuffer Rr_BeginImmediate(const Rr_Renderer* Renderer);
void Rr_EndImmediate(const Rr_Renderer* Renderer);

size_t Rr_GetCurrentFrameIndex(Rr_Renderer* Renderer);
Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer);
