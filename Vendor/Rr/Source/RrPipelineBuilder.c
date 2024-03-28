#include "RrPipelineBuilder.h"

#include "RrTypes.h"
#include "RrHelpers.h"

void PipelineBuilder_Empty(Rr_PipelineBuilder* PipelineBuilder)
{
    *PipelineBuilder = (Rr_PipelineBuilder){
        .InputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO },
        .Rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO },
        .ColorBlendAttachment = { 0 },
        .Multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.0f,
            .pSampleMask = NULL,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE },
        .PipelineLayout = VK_NULL_HANDLE,
        .DepthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO },
        .RenderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO }
    };
}

void PipelineBuilder_Default(Rr_PipelineBuilder* PipelineBuilder, VkShaderModule VertModule, VkShaderModule FragModule, VkFormat ColorFormat, VkFormat DepthFormat, VkPipelineLayout Layout)
{
    PipelineBuilder_Empty(PipelineBuilder);

    PipelineBuilder->PipelineLayout = Layout;

    PipelineBuilder->ShaderStages[0] = GetShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, VertModule);
    PipelineBuilder->ShaderStages[1] = GetShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, FragModule);
    PipelineBuilder->ShaderStageCount = 2;

    PipelineBuilder->InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PipelineBuilder->InputAssembly.primitiveRestartEnable = VK_FALSE;

    PipelineBuilder->Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    PipelineBuilder->Rasterizer.lineWidth = 1.0f;
    PipelineBuilder->Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    PipelineBuilder->Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    PipelineBuilder->ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    PipelineBuilder->ColorBlendAttachment.blendEnable = VK_FALSE;

    PipelineBuilder->ColorAttachmentFormat = ColorFormat;
    PipelineBuilder->RenderInfo.pColorAttachmentFormats = &PipelineBuilder->ColorAttachmentFormat;
    PipelineBuilder->RenderInfo.colorAttachmentCount = 1;
    PipelineBuilder->RenderInfo.depthAttachmentFormat = DepthFormat;
}

void PipelineBuilder_Depth(Rr_PipelineBuilder* const PipelineBuilder)
{
    PipelineBuilder->DepthStencil.depthTestEnable = VK_TRUE;
    PipelineBuilder->DepthStencil.depthWriteEnable = VK_TRUE;
    PipelineBuilder->DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    PipelineBuilder->DepthStencil.depthBoundsTestEnable = VK_FALSE;
    PipelineBuilder->DepthStencil.stencilTestEnable = VK_FALSE;
    PipelineBuilder->DepthStencil.front = (VkStencilOpState){ 0 };
    PipelineBuilder->DepthStencil.back = (VkStencilOpState){ 0 };
    PipelineBuilder->DepthStencil.minDepthBounds = 0.0f;
    PipelineBuilder->DepthStencil.maxDepthBounds = 1.0f;
}

void PipelineBuilder_AlphaBlend(Rr_PipelineBuilder* const PipelineBuilder)
{
    PipelineBuilder->ColorBlendAttachment = (VkPipelineColorBlendAttachmentState){
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };
}

VkPipeline Rr_BuildPipeline(Rr_Renderer* const Renderer, Rr_PipelineBuilder const* const PipelineBuilder)
{
    VkPipelineViewportStateCreateInfo ViewportInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &PipelineBuilder->ColorBlendAttachment,
    };

    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
    };

    VkDynamicState DynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .pDynamicStates = DynamicState,
        .dynamicStateCount = SDL_arraysize(DynamicState)
    };

    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &PipelineBuilder->RenderInfo,
        .stageCount = PipelineBuilder->ShaderStageCount,
        .pStages = PipelineBuilder->ShaderStages,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &PipelineBuilder->InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &PipelineBuilder->Rasterizer,
        .pMultisampleState = &PipelineBuilder->Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &PipelineBuilder->DepthStencil,
        .layout = PipelineBuilder->PipelineLayout,
        .pDynamicState = &DynamicStateInfo
    };

    VkPipeline Pipeline;
    VK_ASSERT(vkCreateGraphicsPipelines(Renderer->Device, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &Pipeline))

    return Pipeline;
}
