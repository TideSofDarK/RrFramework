#pragma once

#include "Rr/Rr_App.h"
#include "Rr_Asset.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES 5
#define RR_PIPELINE_SHADER_STAGES               2
#define RR_PIPELINE_MAX_COLOR_ATTACHMENTS       2
#define RR_PIPELINE_MAX_GLOBALS_SIZE            512
#define RR_PIPELINE_MAX_MATERIAL_SIZE           256
#define RR_PIPELINE_MAX_PER_DRAW_SIZE           128

typedef struct Rr_GenericPipeline Rr_GenericPipeline;
typedef struct Rr_PipelineBuilder Rr_PipelineBuilder;

typedef enum Rr_PolygonMode
{
    RR_POLYGON_MODE_FILL,
    RR_POLYGON_MODE_LINE
} Rr_PolygonMode;

extern Rr_PipelineBuilder *Rr_CreatePipelineBuilder(void);

extern void Rr_EnableColorAttachment(
    Rr_PipelineBuilder *PipelineBuilder,
    Rr_Bool EnableAlphaBlend);

extern void Rr_EnableVertexStage(
    Rr_PipelineBuilder *PipelineBuilder,
    Rr_Asset *SPVAsset);

extern void Rr_EnableFragmentStage(
    Rr_PipelineBuilder *PipelineBuilder,
    Rr_Asset *SPVAsset);

extern void Rr_EnableRasterizer(
    Rr_PipelineBuilder *PipelineBuilder,
    Rr_PolygonMode PolygonMode);

extern void Rr_EnableDepthTest(Rr_PipelineBuilder *PipelineBuilder);

extern Rr_GenericPipeline *Rr_BuildGenericPipeline(
    Rr_App *App,
    Rr_PipelineBuilder *PipelineBuilder,
    size_t Globals,
    size_t Material,
    size_t PerDraw);

extern void Rr_DestroyGenericPipeline(
    Rr_App *App,
    Rr_GenericPipeline *GenericPipeline);

#ifdef __cplusplus
}
#endif
