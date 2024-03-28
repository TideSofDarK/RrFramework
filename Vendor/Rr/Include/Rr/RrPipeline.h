#pragma once

#include "RrCore.h"
#include "RrVulkan.h"

typedef struct Rr_Renderer Rr_Renderer;

typedef struct Rr_PipelineCreateInfo
{
    void* VertexSPV;
    size_t VertexSPVLength;
    void* FragmentSPV;
    size_t FragmentSPVLength;

    b8 bBlending;
} Rr_PipelineCreateInfo;

typedef struct Rr_Pipeline
{
    VkPipeline Handle;
} Rr_Pipeline;

Rr_Pipeline Rr_CreatePipeline(Rr_Renderer* Renderer, Rr_PipelineCreateInfo* Info);
