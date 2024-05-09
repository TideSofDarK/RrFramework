#pragma once

typedef struct Rr_Frame Rr_Frame;
typedef struct Rr_Renderer Rr_Renderer;

#include "Rr_App.h"
#include "Rr_Pipeline.h"

#include <volk.h>

void Rr_InitRenderer(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_CleanupRenderer(Rr_App* App);
void Rr_Draw(Rr_App* App);
bool Rr_NewFrame(Rr_App* App, void* Window);

Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer);

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer);
void Rr_EndImmediate(Rr_Renderer* Renderer);

VkPipeline Rr_BuildPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    VkPipelineLayout PipelineLayout);

