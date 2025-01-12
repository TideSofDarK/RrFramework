#include "Rr_Pipeline.h"

#include "Rr_App.h"
#include "Rr_Log.h"
#include "Rr_Object.h"

#include <SDL3/SDL_assert.h>

enum Rr_VertexInputBinding
{
    RR_VERTEX_INPUT_BINDING_PER_VERTEX,
    RR_VERTEX_INPUT_BINDING_PER_INSTANCE
};

Rr_Pipeline *Rr_CreatePipeline(Rr_App *App, Rr_PipelineBuilder *PipelineBuilder, VkPipelineLayout PipelineLayout)
{
    Rr_Renderer *Renderer = &App->Renderer;

    Rr_Pipeline *Pipeline = (Rr_Pipeline *)Rr_CreateObject(&App->ObjectStorage);
    Pipeline->ColorAttachmentCount = PipelineBuilder->ColorAttachmentCount;

    /* Create shader modules. */
    VkPipelineShaderStageCreateInfo *ShaderStages =
        Rr_StackAlloc(VkPipelineShaderStageCreateInfo, RR_PIPELINE_SHADER_STAGES);
    uint32_t ShaderStageCount = 0;

    VkShaderModule VertModule = VK_NULL_HANDLE;
    if(PipelineBuilder->VertexShaderSPV.Data != NULL)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .pCode = (uint32_t *)PipelineBuilder->VertexShaderSPV.Data,
            .codeSize = PipelineBuilder->VertexShaderSPV.Length,
        };
        vkCreateShaderModule(Renderer->Device, &ShaderModuleCreateInfo, NULL, &VertModule);
        ShaderStages[ShaderStageCount] = GetShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, VertModule);
        ShaderStageCount++;
    }

    VkShaderModule FragModule = VK_NULL_HANDLE;
    if(PipelineBuilder->FragmentShaderSPV.Data != NULL)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .pCode = (uint32_t *)PipelineBuilder->FragmentShaderSPV.Data,
            .codeSize = PipelineBuilder->FragmentShaderSPV.Length,
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
        .attachmentCount = PipelineBuilder->ColorAttachmentCount,
        .pAttachments = PipelineBuilder->ColorBlendAttachments,
    };

    /* Vertex Input */
    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
    };
    VkVertexInputBindingDescription VertexInputBindingDescriptions[] = {
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

    uint32_t AttributeCount = 0;
    Rr_Bool HasPerVertexBinding = RR_FALSE;
    Rr_Bool HasPerInstanceBinding = RR_FALSE;
    for(size_t Index = 0; Index < RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES; ++Index)
    {
        if(PipelineBuilder->Attributes[Index].format == VK_FORMAT_UNDEFINED)
        {
            break;
        }
        AttributeCount++;
        HasPerVertexBinding = HasPerVertexBinding || PipelineBuilder->Attributes[Index].binding == 0;
        HasPerInstanceBinding = HasPerInstanceBinding || PipelineBuilder->Attributes[Index].binding == 1;
    }
    if(AttributeCount > 0)
    {
        if(HasPerVertexBinding)
        {
            VertexInputInfo.vertexBindingDescriptionCount++;
        }
        if(HasPerInstanceBinding)
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
        .dynamicStateCount = SDL_arraysize(DynamicStates),
    };

    /* Select render pass. */
    switch(PipelineBuilder->ColorAttachmentCount)
    {
        case 0:
            Pipeline->RenderPass = Renderer->RenderPasses.Depth;
            break;
        case 1:
            Pipeline->RenderPass = Renderer->RenderPasses.ColorDepth;
            break;
        default:
            Rr_LogAbort("Unsupported color attachment count!");
            break;
    }

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
        .renderPass = Pipeline->RenderPass,
    };

    vkCreateGraphicsPipelines(Renderer->Device, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &Pipeline->Handle);

    if(VertModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(Renderer->Device, VertModule, NULL);
    }

    if(FragModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(Renderer->Device, FragModule, NULL);
    }

    Rr_Free(PipelineBuilder);
    Rr_StackFree(ShaderStages);

    return Pipeline;
}

void Rr_DestroyPipeline(Rr_App *App, Rr_Pipeline *Pipeline)
{
    Rr_Renderer *Renderer = &App->Renderer;

    vkDestroyPipeline(Renderer->Device, Pipeline->Handle, NULL);

    Rr_DestroyObject(&App->ObjectStorage, Pipeline);
}

Rr_PipelineBuilder *Rr_CreatePipelineBuilder(void)
{
    Rr_PipelineBuilder *PipelineBuilder = Rr_Calloc(1, sizeof(Rr_PipelineBuilder));
    *PipelineBuilder = (Rr_PipelineBuilder){
        .InputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        },
        .Rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        },
        .Multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.0f,
            .pSampleMask = NULL,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        },
        .DepthStencil = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        },
    };

    return PipelineBuilder;
}

void Rr_EnableColorAttachment(Rr_PipelineBuilder *PipelineBuilder, Rr_Bool EnableAlphaBlend)
{
    PipelineBuilder->ColorAttachmentFormats[PipelineBuilder->ColorAttachmentCount] = RR_COLOR_FORMAT;
    if(EnableAlphaBlend)
    {
        PipelineBuilder->ColorBlendAttachments[PipelineBuilder->ColorAttachmentCount] =
            (VkPipelineColorBlendAttachmentState){
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
            };
    }
    else
    {
        PipelineBuilder->ColorBlendAttachments[PipelineBuilder->ColorAttachmentCount] =
            (VkPipelineColorBlendAttachmentState){
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_FALSE,
            };
    }
    PipelineBuilder->ColorAttachmentCount++;
}

void Rr_EnableTriangleFan(Rr_PipelineBuilder *PipelineBuilder)
{
    PipelineBuilder->InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
}

static VkFormat Rr_GetVulkanFormat(Rr_VertexInputType Type)
{
    switch(Type)
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

static size_t Rr_GetVertexInputSize(Rr_VertexInputType Type)
{
    switch(Type)
    {
        case RR_VERTEX_INPUT_TYPE_FLOAT:
            return sizeof(float);
        case RR_VERTEX_INPUT_TYPE_UINT:
            return sizeof(uint32_t);
        case RR_VERTEX_INPUT_TYPE_VEC2:
            return sizeof(float) * 2;
        case RR_VERTEX_INPUT_TYPE_VEC3:
            return sizeof(float) * 3;
        case RR_VERTEX_INPUT_TYPE_VEC4:
            return sizeof(float) * 4;
        case RR_VERTEX_INPUT_TYPE_NONE:
        default:
            return 0;
    }
}

static void Rr_EnableVertexInputAttribute(
    Rr_PipelineBuilder *PipelineBuilder,
    Rr_VertexInputAttribute Attribute,
    size_t Binding)
{
    VkFormat Format = Rr_GetVulkanFormat(Attribute.Type);
    if(Format == VK_FORMAT_UNDEFINED)
    {
        return;
    }

    size_t Location = Attribute.Location;
    if(Location >= RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES)
    {
        Rr_LogAbort("Exceeding max allowed number of vertex attributes for a "
                    "pipeline!");
    }

    PipelineBuilder->Attributes[Location] = (VkVertexInputAttributeDescription){
        .binding = Binding,
        .location = Location,
        .format = Format,
        .offset = PipelineBuilder->VertexInput[Binding].VertexInputStride,
    };

    PipelineBuilder->VertexInput[Binding].VertexInputStride += Rr_GetVertexInputSize(Attribute.Type);
}

void Rr_EnablePerVertexInputAttributes(Rr_PipelineBuilder *PipelineBuilder, Rr_VertexInput *VertexInput)
{
    for(size_t Index = 0; Index < RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES; ++Index)
    {
        Rr_EnableVertexInputAttribute(
            PipelineBuilder,
            VertexInput->Attributes[Index],
            RR_VERTEX_INPUT_BINDING_PER_VERTEX);
    }
}

void Rr_EnablePerInstanceInputAttributes(Rr_PipelineBuilder *PipelineBuilder, Rr_VertexInput *VertexInput)
{
    for(size_t Index = 0; Index < RR_PIPELINE_MAX_VERTEX_INPUT_ATTRIBUTES; ++Index)
    {
        Rr_EnableVertexInputAttribute(
            PipelineBuilder,
            VertexInput->Attributes[Index],
            RR_VERTEX_INPUT_BINDING_PER_INSTANCE);
    }
}

void Rr_EnableVertexStage(Rr_PipelineBuilder *PipelineBuilder, Rr_Asset *SPVAsset)
{
    PipelineBuilder->VertexShaderSPV = *SPVAsset;
}

void Rr_EnableFragmentStage(Rr_PipelineBuilder *PipelineBuilder, Rr_Asset *SPVAsset)
{
    PipelineBuilder->FragmentShaderSPV = *SPVAsset;
}

void Rr_EnableRasterizer(Rr_PipelineBuilder *PipelineBuilder, Rr_PolygonMode PolygonMode)
{
    switch(PolygonMode)
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

void Rr_EnableDepthTest(Rr_PipelineBuilder *PipelineBuilder)
{
    PipelineBuilder->DepthStencil.depthTestEnable = VK_TRUE;
    PipelineBuilder->DepthStencil.depthWriteEnable = VK_TRUE;
    PipelineBuilder->DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
}

Rr_GenericPipeline *Rr_BuildGenericPipeline(
    Rr_App *App,
    Rr_PipelineBuilder *PipelineBuilder,
    size_t Globals,
    size_t Material,
    size_t PerDraw)
{
    SDL_assert(Globals < RR_PIPELINE_MAX_GLOBALS_SIZE);
    SDL_assert(Material < RR_PIPELINE_MAX_MATERIAL_SIZE);
    SDL_assert(PerDraw < RR_PIPELINE_MAX_PER_DRAW_SIZE);

    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);

    Rr_Renderer *Renderer = &App->Renderer;

    Rr_VertexInput VertexInput = {
        .Attributes = {
            {
                .Type = RR_VERTEX_INPUT_TYPE_VEC3,
                .Location = 0,
            },
            {
                .Type = RR_VERTEX_INPUT_TYPE_FLOAT,
                .Location = 1,
            },
            {
                .Type = RR_VERTEX_INPUT_TYPE_VEC3,
                .Location = 2,
            },
            {
                .Type = RR_VERTEX_INPUT_TYPE_FLOAT,
                .Location = 3,
            },
            {
                .Type = RR_VERTEX_INPUT_TYPE_VEC4,
                .Location = 4,
            },
        },
    };
    Rr_EnablePerVertexInputAttributes(PipelineBuilder, &VertexInput);

    Rr_GenericPipeline *Pipeline = Rr_CreateObject(&App->ObjectStorage);
    *Pipeline = (Rr_GenericPipeline){
        .Pipeline = Rr_CreatePipeline(App, PipelineBuilder, Renderer->GenericPipelineLayout),
        .Sizes = { .Globals = Globals, .Material = Material, .PerDraw = PerDraw },
    };

    /* Initialize per-draw buffer descriptor sets.
     * These are dynamic.
     * @TODO: The only thing that doesn't allow me to have single global set is
     * @TODO: varying PerDraw size.
     * @TODO: Pipeline might be deleted but what about the descriptor sets?
     */
    Rr_DescriptorWriter DescriptorWriter = Rr_CreateDescriptorWriter(0, 1, Scratch.Arena);
    for(size_t FrameIndex = 0; FrameIndex < RR_FRAME_OVERLAP; ++FrameIndex)
    {
        Rr_Frame *Frame = &Renderer->Frames[FrameIndex];

        Pipeline->PerDrawDescriptorSets[FrameIndex] = Rr_AllocateDescriptorSet(
            &Renderer->GlobalDescriptorAllocator,
            Renderer->Device,
            Renderer->GenericDescriptorSetLayouts[RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW]);
        Rr_WriteBufferDescriptor(
            &DescriptorWriter,
            0,
            Frame->PerDrawBuffer.Buffer->Handle,
            PerDraw,
            0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            Scratch.Arena);
        Rr_UpdateDescriptorSet(&DescriptorWriter, Renderer->Device, Pipeline->PerDrawDescriptorSets[FrameIndex]);
        Rr_ResetDescriptorWriter(&DescriptorWriter);
    }

    Rr_DestroyArenaScratch(Scratch);

    return Pipeline;
}

void Rr_DestroyGenericPipeline(Rr_App *App, Rr_GenericPipeline *GenericPipeline)
{
    Rr_DestroyPipeline(App, GenericPipeline->Pipeline);
    Rr_DestroyObject(&App->ObjectStorage, GenericPipeline);
}
