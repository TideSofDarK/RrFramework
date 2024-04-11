#pragma once

#include "RrVulkan.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_PipelineBuilder Rr_PipelineBuilder;

Rr_PipelineBuilder Rr_DefaultPipelineBuilder(VkShaderModule VertModule, VkShaderModule FragModule, VkFormat ColorFormat, VkFormat DepthFormat, VkPipelineLayout Layout);
void Rr_EnableDepth(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnableAlphaBlend(Rr_PipelineBuilder* PipelineBuilder);
VkPipeline Rr_BuildPipeline(Rr_Renderer* Renderer, Rr_PipelineBuilder const* PipelineBuilder);
