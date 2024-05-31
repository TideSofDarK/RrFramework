#pragma once

#include "Rr_App.h"
#include "Rr_Pipeline.h"
#include "Rr_Memory.h"

#include <volk.h>

#define RR_MAX_PENDING_LOADS

typedef struct Rr_Frame Rr_Frame;
typedef struct Rr_Renderer Rr_Renderer;

typedef enum Rr_HandleType {
    RR_HANDLE_TYPE_BUFFER,
    RR_HANDLE_TYPE_STATIC_MESH,
    RR_HANDLE_TYPE_IMAGE,
    RR_HANDLE_TYPE_FONT,
    RR_HANDLE_TYPE_MATERIAL,
    RR_HANDLE_TYPE_COUNT,
} Rr_HandleType;

extern void Rr_InitRenderer(Rr_App* App);
extern void Rr_InitImGui(Rr_App* App);
extern void Rr_CleanupRenderer(Rr_App* App);
extern void Rr_Draw(Rr_App* App);
extern bool Rr_NewFrame(Rr_App* App, void* Window);

extern VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer);
extern void Rr_EndImmediate(Rr_Renderer* Renderer);

extern VkPipeline Rr_BuildPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    VkPipelineLayout PipelineLayout);

extern Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer);
extern bool Rr_IsUnifiedQueue(Rr_Renderer* Renderer);
extern VkDeviceSize Rr_GetUniformAlignment(Rr_Renderer* Renderer);
