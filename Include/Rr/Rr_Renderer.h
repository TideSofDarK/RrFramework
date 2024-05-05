#pragma once

#include <cglm/vec3.h>
#include <cglm/vec4.h>

#include <SDL3/SDL_atomic.h>

#include "Rr_Image.h"
#include "Rr_Text.h"
#include "Rr_Load.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum Rr_DrawTextFlags
{
    RR_DRAW_TEXT_FLAGS_NONE_BIT = 0,
    RR_DRAW_TEXT_FLAGS_ANIMATION_BIT = 1
} Rr_DrawTextFlags;

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Mutex SDL_Mutex;
typedef struct Rr_AppConfig Rr_AppConfig;
typedef struct Rr_GenericPipeline Rr_GenericPipeline;
typedef struct Rr_Material Rr_Material;
typedef struct Rr_App Rr_App;
typedef struct Rr_MeshBuffers Rr_MeshBuffers;
typedef struct Rr_LoadingContext Rr_LoadingContext;
typedef struct Rr_PendingLoad Rr_PendingLoad;
typedef struct Rr_Frame Rr_Frame;

void Rr_InitRenderer(Rr_App* App);
void Rr_InitImGui(Rr_App* App);
void Rr_CleanupRenderer(Rr_App* App);
void Rr_Draw(Rr_App* App);
bool Rr_NewFrame(Rr_Renderer* Renderer, SDL_Window* Window);

/* Rendering */
typedef struct Rr_BeginRenderingInfo
{
    Rr_GenericPipeline* Pipeline;
    Rr_Image* InitialColor;
    Rr_Image* InitialDepth;
    Rr_Image* AdditionalAttachment;
    const void* GlobalsData;
} Rr_BeginRenderingInfo;
typedef struct Rr_DrawMeshInfo
{
    const Rr_Material* Material;
    const Rr_MeshBuffers* MeshBuffers;
    char DrawData[RR_PIPELINE_MAX_DRAW_SIZE];
} Rr_DrawMeshInfo;
typedef struct Rr_DrawTextInfo
{
    Rr_Font* Font;
    Rr_String String;
    vec2 Position;
    f32 Size;
    Rr_DrawTextFlags Flags;
} Rr_DrawTextInfo;
typedef struct Rr_RenderingContext
{
    Rr_BeginRenderingInfo* Info;
    Rr_Renderer* Renderer;
    Rr_DrawMeshInfo* DrawMeshArray;
    Rr_DrawTextInfo* DrawTextArray;
} Rr_RenderingContext;
Rr_RenderingContext Rr_BeginRendering(Rr_Renderer* Renderer, Rr_BeginRenderingInfo* Info);
void Rr_DrawMesh(
    Rr_RenderingContext* RenderingContext,
    const Rr_Material* Material,
    const Rr_MeshBuffers* MeshBuffers,
    const void* DrawData);
void Rr_DrawCustomText(
    Rr_RenderingContext* RenderingContext,
    Rr_Font* Font,
    Rr_String* String,
    vec2 Position,
    f32 Size,
    Rr_DrawTextFlags Flags);
void Rr_DrawDefaultText(Rr_RenderingContext* RenderingContext, Rr_String* String, vec2 Position);
void Rr_EndRendering(Rr_RenderingContext* RenderingContext);

float Rr_GetAspectRatio(Rr_Renderer* Renderer);
size_t Rr_GetCurrentFrameIndex(Rr_Renderer* Renderer);
Rr_Frame* Rr_GetCurrentFrame(Rr_Renderer* Renderer);
void* Rr_GetCurrentFrameData(Rr_Renderer* Renderer);

#ifdef __cplusplus
}
#endif
