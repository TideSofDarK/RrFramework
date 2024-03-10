#pragma once

#include "RrVulkan.h"

typedef struct SPipelineBuilder SPipelineBuilder;

void PipelineBuilder_Empty(SPipelineBuilder* PipelineBuilder);
void PipelineBuilder_Default(SPipelineBuilder* PipelineBuilder, VkShaderModule VertModule, VkShaderModule FragModule, VkFormat ColorFormat, VkFormat DepthFormat, VkPipelineLayout Layout);
void PipelineBuilder_Depth(SPipelineBuilder* PipelineBuilder);
void PipelineBuilder_AlphaBlend(SPipelineBuilder* PipelineBuilder);
