#pragma once

#include "RrTypes.h"
#include "RrAsset.h"
#include "RrCore.h"
#include "RrVulkan.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_PipelineBuilder Rr_PipelineBuilder;
typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;
typedef struct Rr_DescriptorSetLayout Rr_DescriptorSetLayout;

typedef struct Rr_GenericPipelineBuffers
{
    Rr_Buffer Globals;
    Rr_Buffer Material;
    Rr_Buffer Draw;
} Rr_GenericPipelineBuffers;

typedef struct Rr_Pipeline
{
    VkPipeline Handle;
    VkPipelineLayout Layout;
} Rr_Pipeline;

typedef enum Rr_PolygonMode {
    RR_POLYGON_MODE_FILL,
    RR_POLYGON_MODE_LINE
} Rr_PolygonMode;

Rr_PipelineBuilder Rr_GenericPipelineBuilder(void);
void Rr_EnableVertexStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset);
void Rr_EnableFragmentStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset);
void Rr_EnableAdditionalColorAttachment(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnableRasterizer(Rr_PipelineBuilder* PipelineBuilder, Rr_PolygonMode PolygonMode);
void Rr_EnableDepthTest(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnableAlphaBlend(Rr_PipelineBuilder* PipelineBuilder);
Rr_Pipeline Rr_BuildGenericPipeline(Rr_Renderer* Renderer, Rr_PipelineBuilder* PipelineBuilder);
void Rr_DestroyPipeline(Rr_Renderer* Renderer, Rr_Pipeline* Pipeline);

Rr_GenericPipelineBuffers Rr_CreateGenericPipelineBuffers(Rr_Renderer* Renderer, size_t GlobalSize, size_t MaterialSize);
void Rr_DestroyGenericPipelineBuffers(Rr_Renderer* Renderer, Rr_GenericPipelineBuffers* GenericPipelineBuffers);
