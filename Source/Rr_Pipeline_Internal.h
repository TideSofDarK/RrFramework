#pragma once

#include "Rr_Vulkan.h"

VkPipeline Rr_BuildPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    VkPipelineLayout PipelineLayout);
