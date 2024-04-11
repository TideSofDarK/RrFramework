#pragma once

#include "RrAsset.h"
#include "RrCore.h"
#include "RrVulkan.h"

typedef struct Rr_Renderer Rr_Renderer;

typedef struct Rr_PipelineCreateInfo
{
    Rr_Asset VertexShader;
    Rr_Asset FragmentShader;
    size_t PushConstantsSize;
    VkDescriptorSetLayout DescriptorSetLayout;
} Rr_PipelineCreateInfo;

typedef struct Rr_Pipeline
{
    VkPipeline Handle;
    VkPipelineLayout Layout;
} Rr_Pipeline;

Rr_Pipeline Rr_CreatePipeline(Rr_Renderer* Renderer, Rr_PipelineCreateInfo* Info);
void Rr_DestroyPipeline(Rr_Renderer* Renderer, Rr_Pipeline* Pipeline);
