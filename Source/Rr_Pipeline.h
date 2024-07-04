#pragma once

#include "Rr/Rr_Pipeline.h"

#include <volk.h>

struct Rr_Renderer;

typedef struct Rr_Pipeline Rr_Pipeline;
struct Rr_Pipeline
{
    VkPipeline Handle;
    VkRenderPass RenderPass;
    uint32_t ColorAttachmentCount;
};

struct Rr_GenericPipeline
{
    Rr_Pipeline *Pipeline;
    struct
    {
        size_t Globals;
        size_t Material;
        size_t Draw;
    } Sizes;
    VkDescriptorSet DrawDescriptorSets[RR_FRAME_OVERLAP];
};

struct Rr_PipelineBuilder
{
    struct
    {
        uint32_t VertexInputStride;
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
    size_t PushConstantsSize;

    size_t ColorAttachmentCount;
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
    uint32_t Location;
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

extern Rr_Pipeline *Rr_CreatePipeline(
    struct Rr_App *App,
    Rr_PipelineBuilder *PipelineBuilder,
    VkPipelineLayout PipelineLayout);

extern void Rr_DestroyPipeline(
    struct Rr_App *App,
    Rr_Pipeline *Pipeline);
