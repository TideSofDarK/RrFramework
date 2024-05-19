#pragma once

#include "Rr_App.h"
#include "Rr_String.h"
#include "Rr_Text.h"
#include "Rr_Pipeline.h"
#include "Rr_Material.h"
#include "Rr_Image.h"
#include "Rr_Mesh.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Rr_RenderingContext Rr_RenderingContext;

typedef enum Rr_DrawTextFlags
{
    RR_DRAW_TEXT_FLAGS_NONE_BIT = 0,
    RR_DRAW_TEXT_FLAGS_ANIMATION_BIT = 1
} Rr_DrawTextFlags;

typedef struct Rr_BeginRenderingInfo
{
    Rr_GenericPipeline* Pipeline;
    Rr_Image* InitialColor;
    Rr_Image* InitialDepth;
    Rr_Image* AdditionalAttachment;
    const void* GlobalsData;
} Rr_BeginRenderingInfo;

Rr_RenderingContext* Rr_BeginRendering(Rr_App* App, Rr_BeginRenderingInfo* Info);

void Rr_DrawStaticMesh(
    Rr_RenderingContext* RenderingContext,
    Rr_Material* Material,
    Rr_StaticMesh* StaticMesh,
    const void* DrawData);

void Rr_DrawCustomText(
    Rr_RenderingContext* RenderingContext,
    Rr_Font* Font,
    Rr_String* String,
    Rr_Vec2 Position,
    f32 Size,
    Rr_DrawTextFlags Flags);

void Rr_DrawDefaultText(Rr_RenderingContext* RenderingContext, Rr_String* String, Rr_Vec2 Position);

void Rr_EndRendering(Rr_RenderingContext* RenderingContext);

#ifdef __cplusplus
}
#endif
