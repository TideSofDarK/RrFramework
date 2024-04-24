#include "Rr_Pipeline.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_log.h>

#include "Rr_Memory.h"
#include "Rr_Buffer.h"
#include "Rr_Defines.h"
#include "Rr_Helpers.h"
#include "Rr_Vulkan.h"
#include "Rr_Renderer.h"

Rr_PipelineBuilder Rr_GetPipelineBuilder(void)
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

void Rr_EnableVertexInputAttribute(Rr_PipelineBuilder* PipelineBuilder, VkFormat Format)
{
    if (PipelineBuilder->CurrentVertexInputAttribute + 1 >= RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Exceeding max allowed number of vertex attributes for a pipeline!");
        abort();
    }

    PipelineBuilder->VertexInputAttributes[PipelineBuilder->CurrentVertexInputAttribute] = (VkVertexInputAttributeDescription){
        .binding = 0,
        .location = PipelineBuilder->CurrentVertexInputAttribute,
        .format = Format,
        .offset = PipelineBuilder->VertexInputStride,
    };

    if (Format == VK_FORMAT_R32G32_SFLOAT)
    {
        PipelineBuilder->VertexInputStride += sizeof(f32) * 2;
    }
    else if (Format == VK_FORMAT_R32G32B32_SFLOAT)
    {
        PipelineBuilder->VertexInputStride += sizeof(f32) * 3;
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Unknown vertex input format!");
        abort();
    }

    PipelineBuilder->CurrentVertexInputAttribute++;
}

void Rr_EnableVertexStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset)
{
    PipelineBuilder->VertexShaderSPV = *SPVAsset;
}

void Rr_EnableFragmentStage(Rr_PipelineBuilder* PipelineBuilder, Rr_Asset* SPVAsset)
{
    PipelineBuilder->FragmentShaderSPV = *SPVAsset;
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
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };
}

VkPipeline Rr_BuildPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    VkPipelineLayout PipelineLayout)
{
    /* Create shader modules. */
    VkPipelineShaderStageCreateInfo ShaderStages[RR_PIPELINE_SHADER_STAGES];
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
        ShaderStages[ShaderStageCount] = GetShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, VertModule);
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
        ShaderStages[ShaderStageCount] = GetShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, FragModule);
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
    VkVertexInputBindingDescription VertexInputBindingDescription = {
        .binding = 0,
        .stride = PipelineBuilder->VertexInputStride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    if (PipelineBuilder->CurrentVertexInputAttribute > 0)
    {
        VertexInputInfo.vertexBindingDescriptionCount = 1;
        VertexInputInfo.vertexAttributeDescriptionCount = PipelineBuilder->CurrentVertexInputAttribute;
        VertexInputInfo.pVertexBindingDescriptions = &VertexInputBindingDescription;
        VertexInputInfo.pVertexAttributeDescriptions = PipelineBuilder->VertexInputAttributes;
    }

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

    /* Create pipeline. */
    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &PipelineBuilder->RenderInfo,
        .stageCount = ShaderStageCount,
        .pStages = ShaderStages,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &PipelineBuilder->InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &PipelineBuilder->Rasterizer,
        .pMultisampleState = &PipelineBuilder->Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &PipelineBuilder->DepthStencil,
        .layout = Renderer->GenericPipelineLayout,
        .pDynamicState = &DynamicStateInfo
    };

    VkPipeline Pipeline;
    vkCreateGraphicsPipelines(Renderer->Device, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &Pipeline);

    if (VertModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(Renderer->Device, VertModule, NULL);
    }

    if (FragModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(Renderer->Device, FragModule, NULL);
    }

    return Pipeline;
}

Rr_GenericPipeline Rr_BuildGenericPipeline(Rr_Renderer* Renderer, Rr_PipelineBuilder* PipelineBuilder,
    size_t GlobalsSize,
    size_t MaterialSize,
    size_t DrawSize)
{
    Rr_GenericPipeline Pipeline = {
        .GlobalsSize = GlobalsSize,
        .MaterialSize = MaterialSize,
        .DrawSize = DrawSize,
    };

    Pipeline.Handle = Rr_BuildPipeline(Renderer, PipelineBuilder, Renderer->GenericPipelineLayout);

    for (int Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Pipeline.Buffers[Index] = Rr_CreateGenericPipelineBuffers(Renderer, Pipeline.GlobalsSize, Pipeline.MaterialSize, Pipeline.DrawSize);
    }

    return Pipeline;
}

void Rr_DestroyGenericPipeline(Rr_Renderer* Renderer, Rr_GenericPipeline* Pipeline)
{
    VkDevice Device = Renderer->Device;

    for (int Index = 0; Index < RR_FRAME_OVERLAP; Index++)
    {
        Rr_DestroyGenericPipelineBuffers(Renderer, &Pipeline->Buffers[Index]);
    }
    vkDestroyPipeline(Device, Pipeline->Handle, NULL);
}

Rr_GenericPipelineBuffers Rr_CreateGenericPipelineBuffers(Rr_Renderer* Renderer, size_t GlobalsSize, size_t MaterialSize, size_t DrawSize)
{
    Rr_GenericPipelineBuffers Buffers;

    /* Buffers */
    Buffers.Globals = Rr_CreateBuffer(
        Renderer->Allocator,
        GlobalsSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
    Buffers.Material = Rr_CreateBuffer(
        Renderer->Allocator,
        MaterialSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
    const size_t DrawBufferSize = 1 << 20;
    Buffers.Draw = Rr_CreateMappedBuffer(
        Renderer->Allocator,
        DrawBufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    return Buffers;
}

void Rr_DestroyGenericPipelineBuffers(Rr_Renderer* Renderer, Rr_GenericPipelineBuffers* GenericPipelineBuffers)
{
    Rr_DestroyBuffer(&GenericPipelineBuffers->Draw, Renderer->Allocator);
    Rr_DestroyBuffer(&GenericPipelineBuffers->Material, Renderer->Allocator);
    Rr_DestroyBuffer(&GenericPipelineBuffers->Globals, Renderer->Allocator);
}
