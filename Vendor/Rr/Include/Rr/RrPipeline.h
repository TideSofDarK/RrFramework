#pragma once

#include "RrAsset.h"
#include "RrCore.h"
#include "RrVulkan.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_PipelineBuilder Rr_PipelineBuilder;
typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;
typedef struct Rr_DescriptorSetLayout Rr_DescriptorSetLayout;

typedef struct Rr_Pipeline
{
    VkPipeline Handle;
    VkPipelineLayout Layout;
} Rr_Pipeline;

typedef enum Rr_PolygonMode {
    RR_POLYGON_MODE_FILL,
    RR_POLYGON_MODE_LINE
} Rr_PolygonMode;

Rr_PipelineBuilder Rr_DefaultPipelineBuilder();
void Rr_EnableVertexStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset);
void Rr_EnableFragmentStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset);
void Rr_EnablePushConstants(Rr_PipelineBuilder* PipelineBuilder, size_t Size);
void Rr_EnableAdditionalColorAttachment(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnableRasterizer(Rr_PipelineBuilder* PipelineBuilder, Rr_PolygonMode PolygonMode);
void Rr_EnableDepthTest(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnableAlphaBlend(Rr_PipelineBuilder* PipelineBuilder);
Rr_Pipeline Rr_BuildPipeline(Rr_Renderer* Renderer, Rr_PipelineBuilder* PipelineBuilder, Rr_DescriptorSetLayout* DescriptorSetLayouts, size_t DescriptorSetLayoutCount);
void Rr_DestroyPipeline(Rr_Renderer* Renderer, Rr_Pipeline* Pipeline);
