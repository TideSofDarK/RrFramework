#pragma once

#include <Rr/Rr_Pipeline.h>

#include "Rr_Vulkan.h"

struct Rr_PipelineLayout
{
    VkPipelineLayout Handle;
    VkDescriptorSetLayout DescriptorSetLayouts[4];
};

struct Rr_GraphicsPipeline
{
    VkPipeline Handle;
    uint32_t ColorAttachmentCount;
    Rr_PipelineLayout *Layout;
};
