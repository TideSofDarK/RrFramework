#pragma once

#include "RrVulkan.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_PipelineBuilder Rr_PipelineBuilder;

Rr_PipelineBuilder Rr_DefaultPipelineBuilder(VkFormat ColorFormat, VkFormat DepthFormat, VkPipelineLayout Layout);
void Rr_EnableRasterizer(Rr_PipelineBuilder* PipelineBuilder, VkPolygonMode PolygonMode);
void Rr_EnableShaderStages(Rr_PipelineBuilder* PipelineBuilder, VkShaderModule VertModule, VkShaderModule FragModule);
void Rr_EnableDepthTest(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnableAlphaBlend(Rr_PipelineBuilder* PipelineBuilder);
VkPipeline Rr_BuildPipeline(Rr_Renderer* Renderer, Rr_PipelineBuilder const* PipelineBuilder);
