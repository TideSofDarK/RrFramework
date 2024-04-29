#pragma once

#include "Rr_Buffer.h"
#include "Rr_Asset.h"
#include "Rr_Core.h"

#define RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES 5
#define RR_PIPELINE_SHADER_STAGES 2
#define RR_PIPELINE_MAX_COLOR_ATTACHMENTS 2

typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;
typedef struct Rr_DescriptorSetLayout Rr_DescriptorSetLayout;

typedef struct Rr_GenericPipelineBuffers
{
    Rr_Buffer Globals;
    Rr_Buffer Material;
    Rr_Buffer Draw;
} Rr_GenericPipelineBuffers;

typedef struct Rr_GenericPipeline
{
    VkPipeline Handle;

    Rr_GenericPipelineBuffers Buffers[RR_FRAME_OVERLAP];

    size_t GlobalsSize;
    size_t MaterialSize;
    size_t DrawSize;
} Rr_GenericPipeline;

typedef enum Rr_PolygonMode
{
    RR_POLYGON_MODE_FILL,
    RR_POLYGON_MODE_LINE
} Rr_PolygonMode;

typedef struct Rr_PipelineBuilder
{
    struct
    {
        u32 VertexInputStride;
    } VertexInput[2];
    VkVertexInputAttributeDescription Attributes[RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES];
    u32 CurrentVertexInputAttribute;

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
} Rr_PipelineBuilder;

Rr_PipelineBuilder Rr_GetPipelineBuilder(void);
void Rr_EnableTriangleFan(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnablePerVertexInputAttribute(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnablePerInstanceInputAttribute(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnableVertexStage(Rr_PipelineBuilder* PipelineBuilder, const Rr_Asset* SPVAsset);
void Rr_EnableFragmentStage(Rr_PipelineBuilder* PipelineBuilder, const Rr_Asset* SPVAsset);
void Rr_EnableAdditionalColorAttachment(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnableRasterizer(Rr_PipelineBuilder* PipelineBuilder, Rr_PolygonMode PolygonMode);
void Rr_EnableDepthTest(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnableAlphaBlend(Rr_PipelineBuilder* PipelineBuilder);
VkPipeline Rr_BuildPipeline(
    const Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    VkPipelineLayout PipelineLayout);
Rr_GenericPipeline Rr_BuildGenericPipeline(
    const Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    size_t GlobalsSize,
    size_t MaterialSize,
    size_t DrawSize);
void Rr_DestroyGenericPipeline(const Rr_Renderer* Renderer, const Rr_GenericPipeline* Pipeline);

Rr_GenericPipelineBuffers Rr_CreateGenericPipelineBuffers(const Rr_Renderer* Renderer, size_t GlobalsSize, size_t MaterialSize, size_t DrawSize);
void Rr_DestroyGenericPipelineBuffers(const Rr_Renderer* Renderer, const Rr_GenericPipelineBuffers* GenericPipelineBuffers);
