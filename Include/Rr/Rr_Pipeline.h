#pragma once

#include "Rr_App.h"
#include "Rr_Asset.h"

#include <Rr/Rr_Memory.h>

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
typedef struct Rr_GraphicsPipeline Rr_GraphicsPipeline;
typedef struct Rr_PipelineBuilder Rr_PipelineBuilder;

typedef enum Rr_PolygonMode
{
    RR_POLYGON_MODE_FILL,
    RR_POLYGON_MODE_LINE
} Rr_PolygonMode;

typedef enum Rr_VertexInputType
{
    RR_VERTEX_INPUT_TYPE_NONE,
    RR_VERTEX_INPUT_TYPE_FLOAT,
    RR_VERTEX_INPUT_TYPE_UINT,
    RR_VERTEX_INPUT_TYPE_VEC2,
    RR_VERTEX_INPUT_TYPE_VEC3,
    RR_VERTEX_INPUT_TYPE_VEC4,
} Rr_VertexInputType;

typedef struct Rr_VertexInputAttribute Rr_VertexInputAttribute;
struct Rr_VertexInputAttribute
{
    Rr_VertexInputType Type;
    uint32_t Location;
};

typedef struct Rr_VertexInput Rr_VertexInput;
struct Rr_VertexInput
{
    Rr_VertexInputAttribute Attributes[RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES];
};

extern Rr_PipelineBuilder *Rr_CreatePipelineBuilder(Rr_Arena *Arena);

extern void Rr_EnableTriangleFan(Rr_PipelineBuilder *PipelineBuilder);

extern void Rr_EnablePerVertexInputAttributes(Rr_PipelineBuilder *PipelineBuilder, Rr_VertexInput *VertexInput);

extern void Rr_EnablePerInstanceInputAttributes(Rr_PipelineBuilder *PipelineBuilder, Rr_VertexInput *VertexInput);

extern void Rr_EnableColorAttachment(Rr_PipelineBuilder *PipelineBuilder, bool EnableAlphaBlend);

extern void Rr_EnableVertexStage(Rr_PipelineBuilder *PipelineBuilder, Rr_Asset *SPVAsset);

extern void Rr_EnableFragmentStage(Rr_PipelineBuilder *PipelineBuilder, Rr_Asset *SPVAsset);

extern void Rr_EnableRasterizer(Rr_PipelineBuilder *PipelineBuilder, Rr_PolygonMode PolygonMode);

extern void Rr_EnableDepthTest(Rr_PipelineBuilder *PipelineBuilder);

extern Rr_GenericPipeline *Rr_BuildGenericPipeline(
    Rr_App *App,
    Rr_PipelineBuilder *PipelineBuilder,
    size_t Globals,
    size_t Material,
    size_t PerDraw);

extern void Rr_DestroyGenericPipeline(Rr_App *App, Rr_GenericPipeline *GenericPipeline);

extern Rr_GraphicsPipeline *Rr_BuildGraphicsPipeline(Rr_App *App, Rr_PipelineBuilder *PipelineBuilder);

extern void Rr_DestroyGraphicsPipeline(Rr_App *App, Rr_GraphicsPipeline *GraphicsPipelin);

#ifdef __cplusplus
}
#endif
