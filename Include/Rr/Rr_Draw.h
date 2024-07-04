#pragma once

#include "Rr/Rr_App.h"
#include "Rr/Rr_Image.h"
#include "Rr/Rr_Material.h"
#include "Rr/Rr_Math.h"
#include "Rr/Rr_Mesh.h"
#include "Rr/Rr_Pipeline.h"
#include "Rr/Rr_String.h"
#include "Rr/Rr_Text.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_DrawTarget Rr_DrawTarget;
typedef struct Rr_DrawContext Rr_DrawContext;

typedef struct Rr_Data Rr_Data;
struct Rr_Data
{
    void *Data;
    size_t Size;
};

#define Rr_MakeData(Struct)       \
    {                             \
        &(Struct), sizeof(Struct) \
    }

typedef enum Rr_DrawTextFlags
{
    RR_DRAW_TEXT_FLAGS_NONE_BIT = 0,
    RR_DRAW_TEXT_FLAGS_ANIMATION_BIT = 1
} Rr_DrawTextFlags;

typedef struct Rr_DrawContextInfo Rr_DrawContextInfo;
struct Rr_DrawContextInfo
{
    Rr_DrawTarget *DrawTarget;
    Rr_Image *InitialColor;
    Rr_Image *InitialDepth;
    Rr_IntVec4 Viewport;
    Rr_GenericPipeline *BasePipeline;
    Rr_GenericPipeline *OverridePipeline;
    Rr_Bool EnableTextRendering;
};

extern Rr_DrawContext *Rr_CreateDrawContext(
    Rr_App *App,
    Rr_DrawContextInfo *Info,
    char *GlobalsData);

extern void Rr_DrawStaticMesh(
    Rr_DrawContext *DrawContext,
    Rr_StaticMesh *StaticMesh,
    Rr_Data DrawData);

extern void Rr_DrawStaticMeshOverrideMaterials(
    Rr_DrawContext *DrawContext,
    Rr_Material **OverrideMaterials,
    size_t OverrideMaterialCount,
    Rr_StaticMesh *StaticMesh,
    Rr_Data DrawData);

extern void Rr_DrawCustomText(
    Rr_DrawContext *DrawContext,
    Rr_Font *Font,
    Rr_String *String,
    Rr_Vec2 Position,
    float Size,
    Rr_DrawTextFlags Flags);

extern void Rr_DrawDefaultText(
    Rr_DrawContext *DrawContext,
    Rr_String *String,
    Rr_Vec2 Position);

extern Rr_DrawTarget *Rr_CreateDrawTarget(
    Rr_App *App,
    uint32_t Width,
    uint32_t Height);

extern Rr_DrawTarget *Rr_CreateDrawTargetDepthOnly(
    Rr_App *App,
    uint32_t Width,
    uint32_t Height);

extern void Rr_DestroyDrawTarget(Rr_App *App, Rr_DrawTarget *DrawTarget);

extern Rr_DrawTarget *Rr_GetMainDrawTarget(Rr_App *App);

#ifdef __cplusplus
}
#endif
