#include "Rr_Pipeline.h"

#include "Rr_App.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_assert.h>

enum Rr_VertexInputBinding
{
    RR_VERTEX_INPUT_BINDING_PER_VERTEX,
    RR_VERTEX_INPUT_BINDING_PER_INSTANCE
};

VkPipeline Rr_BuildPipeline(
    Rr_Renderer* Renderer,
    Rr_PipelineBuilder* PipelineBuilder,
    VkPipelineLayout PipelineLayout)
{
    /* Create shader modules. */
    VkPipelineShaderStageCreateInfo* ShaderStages = Rr_StackAlloc(VkPipelineShaderStageCreateInfo, RR_PIPELINE_SHADER_STAGES);
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
        .attachmentCount = 1,
        .pAttachments = PipelineBuilder->ColorBlendAttachments,
    };

    /* Vertex Input */
    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
    };
    VkVertexInputBindingDescription VertexInputBindingDescriptions[2] = {
        {
            .binding = 0,
            .stride = PipelineBuilder->VertexInput[0].VertexInputStride,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        {
            .binding = 1,
            .stride = PipelineBuilder->VertexInput[1].VertexInputStride,
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        }
    };

    u32 AttributeCount = 0;
    bool bHasPerVertexBinding = false;
    bool bHasPerInstanceBinding = false;
    for (usize Index = 0; Index < RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES; ++Index)
    {
        if (PipelineBuilder->Attributes[Index].format == VK_FORMAT_UNDEFINED)
        {
            break;
        }
        AttributeCount++;
        bHasPerVertexBinding = bHasPerVertexBinding || PipelineBuilder->Attributes[Index].binding == 0;
        bHasPerInstanceBinding = bHasPerInstanceBinding || PipelineBuilder->Attributes[Index].binding == 1;
    }
    if (AttributeCount > 0)
    {
        if (bHasPerVertexBinding)
        {
            VertexInputInfo.vertexBindingDescriptionCount++;
        }
        if (bHasPerInstanceBinding)
        {
            VertexInputInfo.vertexBindingDescriptionCount++;
        }
        VertexInputInfo.vertexAttributeDescriptionCount = AttributeCount;
        VertexInputInfo.pVertexBindingDescriptions = VertexInputBindingDescriptions;
        VertexInputInfo.pVertexAttributeDescriptions = PipelineBuilder->Attributes;
    }

    /* Dynamic States */
    VkDynamicState DynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .pDynamicStates = DynamicStates,
        .dynamicStateCount = SDL_arraysize(DynamicStates)
    };

    /* Create pipeline. */
    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .stageCount = ShaderStageCount,
        .pStages = ShaderStages,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &PipelineBuilder->InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &PipelineBuilder->Rasterizer,
        .pMultisampleState = &PipelineBuilder->Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &PipelineBuilder->DepthStencil,
        .layout = PipelineLayout,
        .pDynamicState = &DynamicStateInfo,
        .renderPass = Renderer->RenderPass
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

    Rr_Free(PipelineBuilder);
    Rr_StackFree(ShaderStages);

    return Pipeline;
}

Rr_PipelineBuilder* Rr_CreatePipelineBuilder(void)
{
    Rr_PipelineBuilder* PipelineBuilder = Rr_Calloc(1, sizeof(Rr_PipelineBuilder));
    *PipelineBuilder = (Rr_PipelineBuilder){
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
    };

    return PipelineBuilder;
}

void Rr_EnableTriangleFan(Rr_PipelineBuilder* PipelineBuilder)
{
    PipelineBuilder->InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
}

static VkFormat GetVulkanFormat(Rr_VertexInputType Type)
{
    switch (Type)
    {
        case RR_VERTEX_INPUT_TYPE_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case RR_VERTEX_INPUT_TYPE_UINT:
            return VK_FORMAT_R32_UINT;
        case RR_VERTEX_INPUT_TYPE_VEC2:
            return VK_FORMAT_R32G32_SFLOAT;
        case RR_VERTEX_INPUT_TYPE_VEC3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case RR_VERTEX_INPUT_TYPE_VEC4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case RR_VERTEX_INPUT_TYPE_NONE:
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

static usize GetVertexInputSize(Rr_VertexInputType Type)
{
    switch (Type)
    {
        case RR_VERTEX_INPUT_TYPE_FLOAT:
            return sizeof(f32);
        case RR_VERTEX_INPUT_TYPE_UINT:
            return sizeof(u32);
        case RR_VERTEX_INPUT_TYPE_VEC2:
            return sizeof(f32) * 2;
        case RR_VERTEX_INPUT_TYPE_VEC3:
            return sizeof(f32) * 3;
        case RR_VERTEX_INPUT_TYPE_VEC4:
            return sizeof(f32) * 4;
        case RR_VERTEX_INPUT_TYPE_NONE:
        default:
            return 0;
    }
}

static void EnableVertexInputAttribute(Rr_PipelineBuilder* PipelineBuilder, Rr_VertexInputAttribute Attribute, usize Binding)
{
    VkFormat Format = GetVulkanFormat(Attribute.Type);
    if (Format == VK_FORMAT_UNDEFINED)
    {
        return;
    }

    usize Location = Attribute.Location;
    if (Location >= RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Exceeding max allowed number of vertex attributes for a pipeline!");
        abort();
    }

    PipelineBuilder->Attributes[Location] = (VkVertexInputAttributeDescription){
        .binding = Binding,
        .location = Location,
        .format = Format,
        .offset = PipelineBuilder->VertexInput[Binding].VertexInputStride,
    };

    PipelineBuilder->VertexInput[Binding].VertexInputStride += GetVertexInputSize(Attribute.Type);
}

void Rr_EnablePerVertexInputAttributes(Rr_PipelineBuilder* PipelineBuilder, Rr_VertexInput* VertexInput)
{
    for (usize Index = 0; Index < RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES; ++Index)
    {
        EnableVertexInputAttribute(PipelineBuilder, VertexInput->Attributes[Index], RR_VERTEX_INPUT_BINDING_PER_VERTEX);
    }
}

void Rr_EnablePerInstanceInputAttributes(Rr_PipelineBuilder* PipelineBuilder, Rr_VertexInput* VertexInput)
{
    for (usize Index = 0; Index < RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES; ++Index)
    {
        EnableVertexInputAttribute(PipelineBuilder, VertexInput->Attributes[Index], RR_VERTEX_INPUT_BINDING_PER_INSTANCE);
    }
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

void Rr_EnableDepthTest(Rr_PipelineBuilder* PipelineBuilder)
{
    PipelineBuilder->DepthStencil.depthTestEnable = VK_TRUE;
    PipelineBuilder->DepthStencil.depthWriteEnable = VK_TRUE;
    PipelineBuilder->DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
}

void Rr_EnableAlphaBlend(Rr_PipelineBuilder* PipelineBuilder)
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

Rr_GenericPipeline* Rr_BuildGenericPipeline(
    Rr_App* App,
    Rr_PipelineBuilder* PipelineBuilder,
    Rr_GenericPipelineSizes Sizes)
{
    SDL_assert(Sizes.Globals < RR_PIPELINE_MAX_GLOBALS_SIZE);
    SDL_assert(Sizes.Material < RR_PIPELINE_MAX_MATERIAL_SIZE);
    SDL_assert(Sizes.Draw < RR_PIPELINE_MAX_DRAW_SIZE);

    Rr_Renderer* Renderer = &App->Renderer;
    Rr_GenericPipeline* Pipeline = Rr_CreateObject(&App->ObjectStorage);
    *Pipeline = (Rr_GenericPipeline){
        .Handle = Rr_BuildPipeline(Renderer, PipelineBuilder, Renderer->GenericPipelineLayout),
        .Sizes = Sizes
    };

    return Pipeline;
}

void Rr_DestroyGenericPipeline(Rr_App* App, Rr_GenericPipeline* Pipeline)
{
    Rr_Renderer* Renderer = &App->Renderer;
    VkDevice Device = Renderer->Device;

    vkDestroyPipeline(Device, Pipeline->Handle, NULL);

    Rr_DestroyObject(&App->ObjectStorage, Pipeline);
}

Rr_GenericPipelineSizes Rr_GetGenericPipelineSizes(Rr_GenericPipeline* Pipeline)
{
    return Pipeline->Sizes;
}
