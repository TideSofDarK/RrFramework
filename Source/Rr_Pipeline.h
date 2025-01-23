#pragma once

#include "Rr_Vulkan.h"

#include <Rr/Rr_Pipeline.h>

struct Rr_PipelineLayout
{
    VkPipelineLayout Handle;
};

struct Rr_GraphicsPipeline
{
    VkPipeline Handle;
    uint32_t ColorAttachmentCount;
};
