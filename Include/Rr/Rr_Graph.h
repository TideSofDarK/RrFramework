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
typedef struct Rr_Graph Rr_Graph;

/*
 * Passes
 */

typedef struct Rr_DrawPass Rr_DrawPass;

typedef enum Rr_PassType
{
    RR_PASS_TYPE_BUILTIN,
    RR_PASS_TYPE_DRAW,
    RR_PASS_TYPE_BLIT,
    RR_PASS_TYPE_PRESENT,
} Rr_PassType;

typedef struct Rr_DrawPassInfo Rr_DrawPassInfo;
struct Rr_DrawPassInfo
{
    const char *Name;
    Rr_DrawTarget *DrawTarget;
    Rr_Image *InitialColor;
    Rr_Image *InitialDepth;
    Rr_IntVec4 Viewport;
    Rr_GenericPipeline *BasePipeline;
    Rr_GenericPipeline *OverridePipeline;
    Rr_Bool EnableTextRendering;
};

extern Rr_DrawPass *Rr_AddDrawPass(
    Rr_App *App,
    Rr_DrawPassInfo *Info,
    char *GlobalsData);

/*
 * Builtin Commands
 */

typedef enum Rr_DrawTextFlags
{
    RR_DRAW_TEXT_FLAGS_NONE_BIT = 0,
    RR_DRAW_TEXT_FLAGS_ANIMATION_BIT = 1
} Rr_DrawTextFlags;

extern void Rr_DrawCustomText(
    Rr_App *App,
    Rr_Font *Font,
    Rr_String *String,
    Rr_Vec2 Position,
    float Size,
    Rr_DrawTextFlags Flags);

extern void Rr_DrawDefaultText(
    Rr_App *App,
    Rr_String *String,
    Rr_Vec2 Position);

/*
 * Draw Commands
 */

extern void Rr_DrawStaticMesh(
    Rr_App *App,
    Rr_DrawPass *Pass,
    Rr_StaticMesh *StaticMesh,
    Rr_Data PerDrawData);

extern void Rr_DrawStaticMeshOverrideMaterials(
    Rr_App *App,
    Rr_DrawPass *Pass,
    Rr_Material **OverrideMaterials,
    size_t OverrideMaterialCount,
    Rr_StaticMesh *StaticMesh,
    Rr_Data PerDrawData);

/*
 * Draw Target
 */

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
