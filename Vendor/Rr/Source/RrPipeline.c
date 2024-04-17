#include "RrPipeline.h"

#include "RrMemory.h"
#include "RrDefines.h"
#include "RrHelpers.h"
#include "RrVulkan.h"
#include "RrArray.h"
#include "RrTypes.h"

Rr_PipelineBuilder Rr_DefaultPipelineBuilder()
{
    Rr_PipelineBuilder PipelineBuilder = {
        .InputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        },
        .ColorAttachmentFormats = { RR_COLOR_FORMAT },
        .ColorBlendAttachments = {
            (VkPipelineColorBlendAttachmentState){
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_FALSE,
            },
        },
        .Rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO },
        .Multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .sampleShadingEnable = VK_FALSE, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, .minSampleShading = 1.0f, .pSampleMask = NULL, .alphaToCoverageEnable = VK_FALSE, .alphaToOneEnable = VK_FALSE },
        .DepthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO },
        .RenderInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .depthAttachmentFormat = RR_DEPTH_FORMAT,
        },
    };

    return PipelineBuilder;
}

void Rr_EnableVertexStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset)
{
    PipelineBuilder->VertexShaderSPV = *SPVAsset;
}

void Rr_EnableFragmentStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset)
{
    PipelineBuilder->FragmentShaderSPV = *SPVAsset;
}

void Rr_EnablePushConstants(Rr_PipelineBuilder* PipelineBuilder, size_t Size)
{
    PipelineBuilder->PushConstantsSize = Size;
}

void Rr_EnableAdditionalColorAttachment(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format)
{
    PipelineBuilder->ColorAttachmentFormats[1] = Format;

    PipelineBuilder->RenderInfo.colorAttachmentCount = 2;
}

void Rr_EnableRasterizer(Rr_PipelineBuilder* PipelineBuilder, Rr_PolygonMode PolygonMode)
{
    switch (PolygonMode)
    {
        case RR_POLYGON_MODE_LINE:
        {
            PipelineBuilder->Rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
            break;
        }
        default:
        {
            PipelineBuilder->Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            break;
        }
    }
    PipelineBuilder->Rasterizer.lineWidth = 1.0f;
    PipelineBuilder->Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    PipelineBuilder->Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

void Rr_EnableDepthTest(Rr_PipelineBuilder* const PipelineBuilder)
{
    PipelineBuilder->DepthStencil.depthTestEnable = VK_TRUE;
    PipelineBuilder->DepthStencil.depthWriteEnable = VK_TRUE;
    PipelineBuilder->DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
}

void Rr_EnableAlphaBlend(Rr_PipelineBuilder* const PipelineBuilder)
{
    PipelineBuilder->ColorBlendAttachments[0] = (VkPipelineColorBlendAttachmentState){
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

Rr_Pipeline Rr_BuildPipeline(Rr_Renderer* const Renderer, Rr_PipelineBuilder* const PipelineBuilder, Rr_DescriptorSetLayout* DescriptorSetLayouts, size_t DescriptorSetLayoutCount)
{
    Rr_Pipeline Pipeline = { 0 };

    /* Create shader modules. */
    int ShaderStageCount = 0;

    VkShaderModule VertModule = VK_NULL_HANDLE;
    if (PipelineBuilder->VertexShaderSPV.Data != NULL)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .pCode = (u32*)PipelineBuilder->VertexShaderSPV.Data,
            .codeSize = PipelineBuilder->VertexShaderSPV.Length
        };
        vkCreateShaderModule(Renderer->Device, &ShaderModuleCreateInfo, NULL, &VertModule);
        PipelineBuilder->ShaderStages[ShaderStageCount] = GetShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, VertModule);
        ShaderStageCount++;
    }

    VkShaderModule FragModule = VK_NULL_HANDLE;
    if (PipelineBuilder->FragmentShaderSPV.Data != NULL)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .pCode = (u32*)PipelineBuilder->FragmentShaderSPV.Data,
            .codeSize = PipelineBuilder->FragmentShaderSPV.Length
        };
        vkCreateShaderModule(Renderer->Device, &ShaderModuleCreateInfo, NULL, &FragModule);
        PipelineBuilder->ShaderStages[ShaderStageCount] = GetShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, FragModule);
        ShaderStageCount++;
    }

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
        .attachmentCount = PipelineBuilder->RenderInfo.colorAttachmentCount,
        .pAttachments = PipelineBuilder->ColorBlendAttachments,
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

    if (PipelineBuilder->RenderInfo.colorAttachmentCount > 1)
    {
        PipelineBuilder->ColorBlendAttachments[1] = PipelineBuilder->ColorBlendAttachments[0];
    }
    PipelineBuilder->RenderInfo.pColorAttachmentFormats = PipelineBuilder->ColorAttachmentFormats;

    /* Create layout. */
    VkPushConstantRange PushConstantRange = {
        .offset = 0,
        .size = PipelineBuilder->PushConstantsSize,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayout* DescriptorSetLayoutsVK = Rr_StackAlloc(VkDescriptorSetLayout, DescriptorSetLayoutCount);
    for (int Index = 0; Index < DescriptorSetLayoutCount; ++Index)
    {
        DescriptorSetLayoutsVK[Index] = DescriptorSetLayouts[Index].Layout;
    }
    VkPipelineLayoutCreateInfo LayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = DescriptorSetLayoutCount,
        .pSetLayouts = DescriptorSetLayoutsVK,
        .pushConstantRangeCount = PipelineBuilder->PushConstantsSize > 0 ? 1 : 0,
        .pPushConstantRanges = &PushConstantRange,
    };
    vkCreatePipelineLayout(Renderer->Device, &LayoutInfo, NULL, &Pipeline.Layout);

    /* Create pipeline. */
    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &PipelineBuilder->RenderInfo,
        .stageCount = ShaderStageCount,
        .pStages = PipelineBuilder->ShaderStages,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &PipelineBuilder->InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &PipelineBuilder->Rasterizer,
        .pMultisampleState = &PipelineBuilder->Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &PipelineBuilder->DepthStencil,
        .layout = Pipeline.Layout,
        .pDynamicState = &DynamicStateInfo
    };

    vkCreateGraphicsPipelines(Renderer->Device, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &Pipeline.Handle);

    if (VertModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(Renderer->Device, VertModule, NULL);
    }

    if (FragModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(Renderer->Device, FragModule, NULL);
    }

    Rr_StackFree(DescriptorSetLayoutsVK);

    return Pipeline;
}

void Rr_DestroyPipeline(Rr_Renderer* Renderer, Rr_Pipeline* Pipeline)
{
    VkDevice Device = Renderer->Device;

    vkDestroyPipelineLayout(Device, Pipeline->Layout, NULL);
    vkDestroyPipeline(Device, Pipeline->Handle, NULL);
}
