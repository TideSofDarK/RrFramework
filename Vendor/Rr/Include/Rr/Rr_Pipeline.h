#pragma once

#include "Rr_Buffer.h"
#include "Rr_Asset.h"
#include "Rr_Core.h"

typedef struct Rr_Renderer Rr_Renderer;
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
void Rr_EnableVertexStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset);
void Rr_EnableFragmentStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset);
void Rr_EnableAdditionalColorAttachment(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format);
void Rr_EnableRasterizer(Rr_PipelineBuilder* PipelineBuilder, Rr_PolygonMode PolygonMode);
void Rr_EnableDepthTest(Rr_PipelineBuilder* PipelineBuilder);
void Rr_EnableAlphaBlend(Rr_PipelineBuilder* PipelineBuilder);
VkPipeline Rr_BuildPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    VkPipelineLayout PipelineLayout);
Rr_GenericPipeline Rr_BuildGenericPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    size_t GlobalsSize,
    size_t MaterialSize,
    size_t DrawSize);
void Rr_DestroyGenericPipeline(Rr_Renderer* Renderer, Rr_GenericPipeline* Pipeline);

Rr_GenericPipelineBuffers Rr_CreateGenericPipelineBuffers(Rr_Renderer* Renderer, size_t GlobalsSize, size_t MaterialSize, size_t DrawSize);
void Rr_DestroyGenericPipelineBuffers(Rr_Renderer* Renderer, Rr_GenericPipelineBuffers* GenericPipelineBuffers);
