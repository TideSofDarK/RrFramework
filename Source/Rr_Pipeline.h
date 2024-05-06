#pragma once

#include "Rr_Framework.h"
#include "Rr_Asset.h"
#include "Rr_Vulkan.h"

struct Rr_GenericPipelineBuffers
{
    Rr_Buffer* Globals;
    Rr_Buffer* Material;
    Rr_Buffer* Draw;
};

struct Rr_GenericPipeline
{
    VkPipeline Handle;

    Rr_GenericPipelineBuffers* Buffers[RR_FRAME_OVERLAP];

    size_t GlobalsSize;
    size_t MaterialSize;
    size_t DrawSize;
};

struct Rr_PipelineBuilder
{
    struct
    {
        u32 VertexInputStride;
    } VertexInput[2];
    VkVertexInputAttributeDescription Attributes[RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES];

    VkPipelineInputAssemblyStateCreateInfo InputAssembly;
    VkPipelineRasterizationStateCreateInfo Rasterizer;
    VkFormat ColorAttachmentFormats[RR_PIPELINE_MAX_COLOR_ATTACHMENTS];
    VkPipelineColorBlendAttachmentState ColorBlendAttachments[RR_PIPELINE_MAX_COLOR_ATTACHMENTS];
    VkPipelineMultisampleStateCreateInfo Multisampling;
    VkPipelineDepthStencilStateCreateInfo DepthStencil;
    VkPipelineRenderingCreateInfo RenderInfo;
    Rr_Asset VertexShaderSPV;
    Rr_Asset FragmentShaderSPV;
    size_t PushConstantsSize;
};

VkPipeline Rr_BuildPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    VkPipelineLayout PipelineLayout);

Rr_GenericPipelineBuffers* Rr_CreateGenericPipelineBuffers(
    Rr_Renderer* Renderer,
    size_t GlobalsSize,
    size_t MaterialSize,
    size_t DrawSize);
void Rr_DestroyGenericPipelineBuffers(Rr_Renderer* Renderer, Rr_GenericPipelineBuffers* GenericPipelineBuffers);


