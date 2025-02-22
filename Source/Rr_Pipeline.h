#pragma once

#include <Rr/Rr_Pipeline.h>

#include "Rr_Descriptor.h"
#include "Rr_Vulkan.h"

#include <Rr/Rr_Platform.h>

typedef struct Rr_DescriptorSetLayout Rr_DescriptorSetLayout;
struct Rr_DescriptorSetLayout
{
    Rr_PipelineBindingSet Set;
    VkDescriptorSetLayout Handle;
    uint32_t Hash;
};

struct Rr_PipelineLayout
{
    VkPipelineLayout Handle;
    size_t SetLayoutCount;
    Rr_DescriptorSetLayout *SetLayouts[RR_MAX_SETS];
};

struct Rr_GraphicsPipeline
{
    VkPipeline Handle;
    uint32_t ColorAttachmentCount;
    Rr_PipelineLayout *Layout;
};

extern Rr_DescriptorSetLayout *Rr_GetDescriptorSetLayout(
    Rr_Renderer *Renderer,
    Rr_PipelineBindingSet *Set);
