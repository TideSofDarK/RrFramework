#pragma once

#include "Rr_Vulkan.h"

typedef struct Rr_TextPipeline
{
    VkDescriptorSetLayout DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    VkPipeline Handle;
    VkPipelineLayout Layout;
    Rr_Buffer* QuadBuffer;
    Rr_Buffer* GlobalsBuffers[RR_FRAME_OVERLAP];
    Rr_Buffer* TextBuffers[RR_FRAME_OVERLAP];
} Rr_TextPipeline;
