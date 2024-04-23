#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Pipeline.h"

typedef struct Rr_Renderer Rr_Renderer;

typedef enum Rr_TextPipelineDescriptorSet
{
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_CUSTOM,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
} Rr_TextPipelineDescriptorSet;

typedef struct Rr_TextPipeline
{
    VkDescriptorSetLayout DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    VkPipeline Handle;
    VkPipelineLayout Layout;
} Rr_TextPipeline;

void Rr_CreateTextRenderer(Rr_Renderer* Renderer);
void Rr_DestroyTextRenderer(Rr_Renderer* Renderer);
