#pragma once

#include "RrVulkan.h"

typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_PipelineBuilder Rr_PipelineBuilder;

void PipelineBuilder_Empty(Rr_PipelineBuilder* PipelineBuilder);
void PipelineBuilder_Default(Rr_PipelineBuilder* PipelineBuilder, VkShaderModule VertModule, VkShaderModule FragModule, VkFormat ColorFormat, VkFormat DepthFormat, VkPipelineLayout Layout);
void PipelineBuilder_Depth(Rr_PipelineBuilder* PipelineBuilder);
void PipelineBuilder_AlphaBlend(Rr_PipelineBuilder* PipelineBuilder);
VkPipeline Rr_BuildPipeline(Rr_Renderer* Renderer, Rr_PipelineBuilder const* PipelineBuilder);
