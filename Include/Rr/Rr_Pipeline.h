#pragma once

#include "Rr_Buffer.h"
#include "Rr_Asset.h"
#include "Rr_Core.h"

typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;
typedef struct Rr_DescriptorSetLayout Rr_DescriptorSetLayout;
typedef struct Rr_GenericPipelineBuffers Rr_GenericPipelineBuffers;
typedef struct Rr_GenericPipeline Rr_GenericPipeline;
typedef struct Rr_PipelineBuilder Rr_PipelineBuilder;

typedef enum Rr_PolygonMode
{
    RR_POLYGON_MODE_FILL,
    RR_POLYGON_MODE_LINE
} Rr_PolygonMode;

Rr_PipelineBuilder* Rr_CreatePipelineBuilder(void);
void Rr_EnableTriangleFan(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnablePerVertexInputAttribute(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnablePerInstanceInputAttribute(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnableVertexStage(Rr_PipelineBuilder* PipelineBuilder, const Rr_Asset* SPVAsset);
void Rr_EnableFragmentStage(Rr_PipelineBuilder* PipelineBuilder, const Rr_Asset* SPVAsset);
void Rr_EnableAdditionalColorAttachment(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnableRasterizer(Rr_PipelineBuilder* PipelineBuilder, Rr_PolygonMode PolygonMode);
void Rr_EnableDepthTest(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnableAlphaBlend(Rr_PipelineBuilder* PipelineBuilder);
Rr_GenericPipeline* Rr_BuildGenericPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    size_t GlobalsSize,
    size_t MaterialSize,
    size_t DrawSize);
void Rr_DestroyGenericPipeline(Rr_Renderer* Renderer, Rr_GenericPipeline* Pipeline);

Rr_GenericPipelineBuffers* Rr_CreateGenericPipelineBuffers(
    Rr_Renderer* Renderer,
    size_t GlobalsSize,
    size_t MaterialSize,
    size_t DrawSize);
void Rr_DestroyGenericPipelineBuffers(Rr_Renderer* Renderer, Rr_GenericPipelineBuffers* GenericPipelineBuffers);
