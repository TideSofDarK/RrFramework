#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Framework.h"

void Rr_InitRenderer(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_CleanupRenderer(Rr_App* App);
void Rr_Draw(Rr_App* App);
bool Rr_NewFrame(Rr_Renderer* Renderer, SDL_Window* Window);

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer);
void Rr_EndImmediate(Rr_Renderer* Renderer);

size_t Rr_GetCurrentFrameIndex(Rr_Renderer* Renderer);
Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer);
