#pragma once

#include "Rr/Rr_Pipeline.h"

#include <volk.h>

struct Rr_Renderer;

struct Rr_GenericPipeline
{
    VkPipeline Handle;
    Rr_GenericPipelineSizes Sizes;
};

struct Rr_PipelineBuilder
{
    struct
    {
        Rr_U32 VertexInputStride;
    } VertexInput[2];
    VkVertexInputAttributeDescription
        Attributes[RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES];

    VkPipelineInputAssemblyStateCreateInfo InputAssembly;
    VkPipelineRasterizationStateCreateInfo Rasterizer;
    VkFormat ColorAttachmentFormats[RR_PIPELINE_MAX_COLOR_ATTACHMENTS];
    VkPipelineColorBlendAttachmentState
        ColorBlendAttachments[RR_PIPELINE_MAX_COLOR_ATTACHMENTS];
    VkPipelineMultisampleStateCreateInfo Multisampling;
    VkPipelineDepthStencilStateCreateInfo DepthStencil;
    Rr_Asset VertexShaderSPV;
    Rr_Asset FragmentShaderSPV;
    Rr_USize PushConstantsSize;

    Rr_USize ColorAttachmentCount;
};

typedef enum Rr_VertexInputType
{
    RR_VERTEX_INPUT_TYPE_NONE,
    RR_VERTEX_INPUT_TYPE_FLOAT,
    RR_VERTEX_INPUT_TYPE_UINT,
    RR_VERTEX_INPUT_TYPE_VEC2,
    RR_VERTEX_INPUT_TYPE_VEC3,
    RR_VERTEX_INPUT_TYPE_VEC4,
} Rr_VertexInputType;

typedef struct Rr_VertexInputAttribute Rr_VertexInputAttribute;
struct Rr_VertexInputAttribute
{
    Rr_VertexInputType Type;
    Rr_U32 Location;
};

typedef struct Rr_VertexInput Rr_VertexInput;
struct Rr_VertexInput
{
    Rr_VertexInputAttribute Attributes[RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES];
};

extern void Rr_EnableTriangleFan(Rr_PipelineBuilder *PipelineBuilder);

extern void Rr_EnablePerVertexInputAttributes(
    Rr_PipelineBuilder *PipelineBuilder,
    Rr_VertexInput *VertexInput);

extern void Rr_EnablePerInstanceInputAttributes(
    Rr_PipelineBuilder *PipelineBuilder,
    Rr_VertexInput *VertexInput);

extern VkPipeline Rr_BuildPipeline(
    struct Rr_Renderer *Renderer,
    Rr_PipelineBuilder *PipelineBuilder,
    VkPipelineLayout PipelineLayout);
