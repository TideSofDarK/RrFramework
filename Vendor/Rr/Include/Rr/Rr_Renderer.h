#pragma once

#include "Rr_Types.h"
#include "Rr_Asset.h"

typedef struct Rr_AppConfig Rr_AppConfig;
typedef struct Rr_Pipeline Rr_Pipeline;
typedef struct Rr_Material Rr_Material;
typedef struct Rr_App Rr_App;
typedef struct Rr_MeshBuffers Rr_MeshBuffers;

void Rr_Init(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_Cleanup(Rr_App* App);
void Rr_Draw(Rr_App* App);
b8 Rr_NewFrame(Rr_Renderer* Renderer, SDL_Window* Window);

VkCommandBuffer Rr_BeginImmediate(Rr_Renderer* Renderer);
void Rr_EndImmediate(Rr_Renderer* Renderer);

/* Rendering */
typedef struct Rr_BeginRenderingInfo
{
    Rr_Pipeline* Pipeline;
    Rr_Image* InitialColor;
    Rr_Image* InitialDepth;
    Rr_Image* AdditionalAttachment;
    const void* GlobalsData;
} Rr_BeginRenderingInfo;
typedef struct Rr_DrawMeshInfo
{
    Rr_Material* Material;
    Rr_MeshBuffers* MeshBuffers;
    const void* DrawData;
} Rr_DrawMeshInfo;
typedef struct Rr_RenderingContext
{
    Rr_BeginRenderingInfo* Info;
    Rr_Renderer* Renderer;
    VkCommandBuffer CommandBuffer;
    Rr_DrawMeshInfo* DrawMeshArray;
} Rr_RenderingContext;
Rr_RenderingContext Rr_BeginRendering(Rr_Renderer* Renderer, Rr_BeginRenderingInfo* Info);
void Rr_DrawMesh(Rr_RenderingContext* RenderingContext, Rr_DrawMeshInfo* Info);
void Rr_EndRendering(Rr_RenderingContext* RenderingContext);

static inline float Rr_GetAspectRatio(Rr_Renderer* Renderer)
{
    return (float)Renderer->DrawTarget.ActiveResolution.width / (float)Renderer->DrawTarget.ActiveResolution.height;
}

static inline size_t Rr_GetCurrentFrameIndex(Rr_Renderer* Renderer)
{
    return Renderer->FrameNumber % RR_FRAME_OVERLAP;
}

static inline Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* const Renderer)
{
    return &Renderer->Frames[Renderer->FrameNumber % RR_FRAME_OVERLAP];
}

static inline void* Rr_GetCurrentFrameData(Rr_Renderer* Renderer)
{
    return Renderer->PerFrameDatas + (Renderer->FrameNumber % RR_FRAME_OVERLAP) * Renderer->PerFrameDataSize;
}
