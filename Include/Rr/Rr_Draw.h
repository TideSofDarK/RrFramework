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

typedef struct Rr_DrawTarget Rr_DrawTarget;
typedef struct Rr_RenderingContext Rr_RenderingContext;

typedef struct Rr_Data Rr_Data;
struct Rr_Data
{
    const void* Data;
    usize Size;
};

#define Rr_MakeData(Struct) {&(Struct), sizeof(Struct)}

typedef enum Rr_DrawTextFlags
{
    RR_DRAW_TEXT_FLAGS_NONE_BIT = 0,
    RR_DRAW_TEXT_FLAGS_ANIMATION_BIT = 1
} Rr_DrawTextFlags;

typedef struct Rr_DrawContextInfo Rr_DrawContextInfo;
struct Rr_DrawContextInfo
{
    Rr_DrawTarget* DrawTarget;
    Rr_Image* InitialColor;
    Rr_Image* InitialDepth;
    Rr_GenericPipelineSizes Sizes;
};

Rr_RenderingContext* Rr_CreateDrawContext(Rr_App* App, Rr_DrawContextInfo* Info, const byte* GlobalsData);

void Rr_DrawStaticMesh(
    Rr_RenderingContext* RenderingContext,
    Rr_StaticMesh* StaticMesh,
    Rr_Data DrawData);

void Rr_DrawStaticMeshOverrideMaterials(
    Rr_RenderingContext* RenderingContext,
    Rr_Material** OverrideMaterials,
    usize OverrideMaterialCount,
    Rr_StaticMesh* StaticMesh,
    Rr_Data DrawData);

void Rr_DrawCustomText(
    Rr_RenderingContext* RenderingContext,
    Rr_Font* Font,
    Rr_String* String,
    Rr_Vec2 Position,
    f32 Size,
    Rr_DrawTextFlags Flags);

void Rr_DrawDefaultText(Rr_RenderingContext* RenderingContext, Rr_String* String, Rr_Vec2 Position);

Rr_DrawTarget* Rr_CreateDrawTarget(Rr_App* App, u32 Width, u32 Height);
void Rr_DestroyDrawTarget(Rr_App* App, Rr_DrawTarget* DrawTarget);
Rr_DrawTarget* Rr_GetMainDrawTarget(Rr_App* App);

#ifdef __cplusplus
}
#endif
