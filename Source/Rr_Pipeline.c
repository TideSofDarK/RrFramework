#include "Rr_Pipeline.h"

#include "Rr_App.h"
#include "Rr_Log.h"

#include <assert.h>

static VkStencilOp Rr_GetVulkanStencilOp(Rr_StencilOp StencilOp)
{
    switch(StencilOp)
    {
        case RR_STENCIL_OP_KEEP:
            return VK_STENCIL_OP_KEEP;
        case RR_STENCIL_OP_ZERO:
            return VK_STENCIL_OP_ZERO;
        case RR_STENCIL_OP_REPLACE:
            return VK_STENCIL_OP_REPLACE;
        case RR_STENCIL_OP_INCREMENT_AND_CLAMP:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case RR_STENCIL_OP_DECREMENT_AND_CLAMP:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case RR_STENCIL_OP_INVERT:
            return VK_STENCIL_OP_INVERT;
        case RR_STENCIL_OP_INCREMENT_AND_WRAP:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case RR_STENCIL_OP_DECREMENT_AND_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default:
            return 0;
    }
}

static VkCompareOp Rr_GetVulkanCompareOp(Rr_CompareOp CompareOp)
{
    switch(CompareOp)
    {
        case RR_COMPARE_OP_NEVER:
            return VK_COMPARE_OP_NEVER;
        case RR_COMPARE_OP_LESS:
            return VK_COMPARE_OP_LESS;
        case RR_COMPARE_OP_EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case RR_COMPARE_OP_LESS_OR_EQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case RR_COMPARE_OP_GREATER:
            return VK_COMPARE_OP_GREATER;
        case RR_COMPARE_OP_NOT_EQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        case RR_COMPARE_OP_GREATER_OR_EQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case RR_COMPARE_OP_ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        default:
            return 0;
    }
}

static VkStencilOpState Rr_GetVulkanStencilOpState(Rr_StencilOpState State, Rr_DepthStencil *DepthStencil)
{
    return (VkStencilOpState){
        .compareOp = Rr_GetVulkanCompareOp(State.CompareOp),
        .failOp = Rr_GetVulkanStencilOp(State.FailOp),
        .passOp = Rr_GetVulkanStencilOp(State.PassOp),
        .depthFailOp = Rr_GetVulkanStencilOp(State.DepthFailOp),
        .writeMask = DepthStencil->WriteMask,
        .compareMask = DepthStencil->CompareMask,
        .reference = 0,
    };
}

static VkPolygonMode Rr_GetVulkanPolygonMode(Rr_PolygonMode Mode)
{
    switch(Mode)
    {
        case RR_POLYGON_MODE_LINE:
            return VK_POLYGON_MODE_LINE;
        default:
            return VK_POLYGON_MODE_FILL;
    }
}

static VkCullModeFlagBits Rr_GetVulkanCullMode(Rr_CullMode Mode)
{
    switch(Mode)
    {
        case RR_CULL_MODE_FRONT:
            return VK_CULL_MODE_FRONT_BIT;
        case RR_CULL_MODE_BACK:
            return VK_CULL_MODE_BACK_BIT;
        default:
            return VK_CULL_MODE_NONE;
    }
}

static VkFrontFace Rr_GetVulkanFrontFace(Rr_FrontFace FrontFace)
{
    switch(FrontFace)
    {
        case RR_FRONT_FACE_COUNTER_CLOCKWISE:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default:
            return VK_FRONT_FACE_CLOCKWISE;
    }
}

static VkBlendFactor Rr_GetVulkanBlendFactor(Rr_BlendFactor BlendFactor)
{
    switch(BlendFactor)
    {
        case RR_BLEND_FACTOR_ZERO:
            return VK_BLEND_FACTOR_ZERO;
        case RR_BLEND_FACTOR_ONE:
            return VK_BLEND_FACTOR_ONE;
        case RR_BLEND_FACTOR_SRC_COLOR:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case RR_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case RR_BLEND_FACTOR_DST_COLOR:
            return VK_BLEND_FACTOR_DST_COLOR;
        case RR_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case RR_BLEND_FACTOR_SRC_ALPHA:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case RR_BLEND_FACTOR_DST_ALPHA:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case RR_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case RR_BLEND_FACTOR_CONSTANT_COLOR:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case RR_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case RR_BLEND_FACTOR_SRC_ALPHA_SATURATE:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        default:
            return 0;
    }
}

static VkBlendOp Rr_GetVulkanBlendOp(Rr_BlendOp BlendOp)
{
    switch(BlendOp)
    {
        case RR_BLEND_OP_ADD:
            return VK_BLEND_OP_ADD;
        case RR_BLEND_OP_SUBTRACT:
            return VK_BLEND_OP_SUBTRACT;
        case RR_BLEND_OP_REVERSE_SUBTRACT:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case RR_BLEND_OP_MIN:
            return VK_BLEND_OP_MIN;
        case RR_BLEND_OP_MAX:
            return VK_BLEND_OP_MAX;
        case RR_BLEND_OP_INVALID:
        default:
            return 0;
    }
}

static VkPrimitiveTopology Rr_GetVulkanTopology(Rr_Topology Topology)
{
    switch(Topology)
    {
        case RR_TOPOLOGY_TRIANGLE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case RR_TOPOLOGY_TRIANGLE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case RR_TOPOLOGY_LINE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case RR_TOPOLOGY_LINE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case RR_TOPOLOGY_POINT_LIST:
        default:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
}

static VkFormat Rr_GetVulkanFormat(Rr_Format Format)
{
    switch(Format)
    {
        case RR_FORMAT_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        case RR_FORMAT_UINT:
            return VK_FORMAT_R32_UINT;
        case RR_FORMAT_VEC2:
            return VK_FORMAT_R32G32_SFLOAT;
        case RR_FORMAT_VEC3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case RR_FORMAT_VEC4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case RR_FORMAT_NONE:
        default:
            return VK_FORMAT_UNDEFINED;
    }
}

static size_t Rr_GetFormatSize(Rr_Format Format)
{
    switch(Format)
    {
        case RR_FORMAT_FLOAT:
            return sizeof(float);
        case RR_FORMAT_UINT:
            return sizeof(uint32_t);
        case RR_FORMAT_VEC2:
            return sizeof(float) * 2;
        case RR_FORMAT_VEC3:
            return sizeof(float) * 3;
        case RR_FORMAT_VEC4:
            return sizeof(float) * 4;
        case RR_FORMAT_NONE:
        default:
            return 0;
    }
}

static VkRenderPass Rr_GetCompatibleRenderPass(Rr_App *App, Rr_PipelineInfo *Info)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);

    bool HasDepth = Info->DepthStencil.EnableDepthWrite;
    size_t AttachmentCount = Info->ColorTargetCount + (HasDepth ? 1 : 0);
    Rr_Attachment *Attachments = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, Rr_Attachment, AttachmentCount);

    for(uint32_t Index = 0; Index < Info->ColorTargetCount; ++Index)
    {
        Attachments[Index].LoadOp = RR_LOAD_OP_DONT_CARE;
        Attachments[Index].StoreOp = RR_STORE_OP_DONT_CARE;
    }
    if(HasDepth)
    {
        Attachments[AttachmentCount - 1].LoadOp = RR_LOAD_OP_DONT_CARE;
        Attachments[AttachmentCount - 1].StoreOp = RR_STORE_OP_DONT_CARE;
        Attachments[AttachmentCount - 1].Depth = true;
    }

    VkRenderPass RenderPass = Rr_GetRenderPass(
        App,
        &(Rr_RenderPassInfo){
            .AttachmentCount = AttachmentCount,
            .Attachments = Attachments,
        });

    Rr_DestroyArenaScratch(Scratch);

    return RenderPass;
}

Rr_GraphicsPipeline *Rr_CreateGraphicsPipeline(Rr_App *App, Rr_PipelineInfo *Info)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);

    Rr_Renderer *Renderer = &App->Renderer;

    Rr_GraphicsPipeline *Pipeline = (Rr_GraphicsPipeline *)Rr_CreateObject(App);

    RR_SLICE_TYPE(VkPipelineShaderStageCreateInfo) ShaderStages;

    VkShaderModule VertModule = VK_NULL_HANDLE;
    if(Info->VertexShaderSPV.Pointer != NULL)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .pCode = (uint32_t *)Info->VertexShaderSPV.Pointer,
            .codeSize = Info->VertexShaderSPV.Size,
        };
        vkCreateShaderModule(Renderer->Device, &ShaderModuleCreateInfo, NULL, &VertModule);
        *RR_SLICE_PUSH(&ShaderStages, Scratch.Arena) = GetShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, VertModule);
    }

    VkShaderModule FragModule = VK_NULL_HANDLE;
    if(Info->FragmentShaderSPV.Pointer != NULL)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .pCode = (uint32_t *)Info->FragmentShaderSPV.Pointer,
            .codeSize = Info->FragmentShaderSPV.Size,
        };
        vkCreateShaderModule(Renderer->Device, &ShaderModuleCreateInfo, NULL, &FragModule);
        *RR_SLICE_PUSH(&ShaderStages, Scratch.Arena) = GetShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, FragModule);
    }

    RR_SLICE_TYPE(VkVertexInputBindingDescription) BindingDescriptions;
    RR_SLICE_TYPE(VkVertexInputAttributeDescription) AttributeDescriptions;
    RR_SLICE_RESERVE(&AttributeDescriptions, Info->VertexAttributeCount, Scratch.Arena);
    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexAttributeDescriptionCount = AttributeDescriptions.Count,
        .pVertexAttributeDescriptions = AttributeDescriptions.Data,
        .vertexBindingDescriptionCount = BindingDescriptions.Count,
        .pVertexBindingDescriptions = BindingDescriptions.Data,
    };
    for(size_t Index = 0; Index < Info->VertexAttributeCount; ++Index)
    {
        Rr_VertexInputAttribute *Attribute = Info->VertexAttributes + Index;

        VkVertexInputAttributeDescription *AttributeDescription = RR_SLICE_PUSH(&AttributeDescriptions, Scratch.Arena);
        size_t Binding = Attribute->Type == RR_VERTEX_INPUT_TYPE_INSTANCE ? 1 : 0;
        AttributeDescription->location = Attribute->Location;
        AttributeDescription->format = Rr_GetVulkanFormat(Attribute->Format);
        AttributeDescription->binding = Binding;
        VkVertexInputBindingDescription *BindingDescription = NULL;
        for(size_t BindingIndex = 0; BindingIndex < BindingDescriptions.Count; ++BindingIndex)
        {
            if(BindingDescriptions.Data[BindingIndex].binding == Binding)
            {
                BindingDescription = BindingDescriptions.Data + BindingIndex;
                break;
            }
        }
        if(BindingDescription == NULL)
        {
            BindingDescription = RR_SLICE_PUSH(&BindingDescriptions, Scratch.Arena);
            BindingDescription->binding = Binding;
            BindingDescription->inputRate = Attribute->Type == RR_VERTEX_INPUT_TYPE_INSTANCE
                                                ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                : VK_VERTEX_INPUT_RATE_VERTEX;
        }
        size_t Size = Rr_GetFormatSize(Attribute->Format);
        AttributeDescription->offset = BindingDescription->stride;
        BindingDescription->stride += Size;
    }

    VkPipelineInputAssemblyStateCreateInfo InputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .topology = Rr_GetVulkanTopology(Info->Topology),
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo ViewportInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo Rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthClampEnable = Info->Rasterizer.EnableDepthClip,
        .rasterizerDiscardEnable = false,
        .polygonMode = Rr_GetVulkanPolygonMode(Info->Rasterizer.PolygonMode),
        .cullMode = Rr_GetVulkanCullMode(Info->Rasterizer.CullMode),
        .frontFace = Rr_GetVulkanFrontFace(Info->Rasterizer.FrontFace),
        .depthBiasEnable = Info->Rasterizer.EnableDepthBias,
        .depthBiasConstantFactor = Info->Rasterizer.DepthBiasConstantFactor,
        .depthBiasClamp = Info->Rasterizer.DepthBiasClamp,
        .depthBiasSlopeFactor = Info->Rasterizer.DepthBiasSlopeFactor,
        .lineWidth = 1.0f,
    };

    VkDynamicState DynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .pDynamicStates = DynamicStates,
        .dynamicStateCount = SDL_arraysize(DynamicStates),
    };

    VkPipelineMultisampleStateCreateInfo Multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    RR_SLICE_TYPE(VkPipelineColorBlendAttachmentState) ColorAttachments;
    RR_SLICE_RESERVE(&ColorAttachments, Info->ColorTargetCount, Scratch.Arena);
    for(size_t Index = 0; Index < Info->ColorTargetCount; ++Index)
    {
        VkPipelineColorBlendAttachmentState *Attachment = RR_SLICE_PUSH(&ColorAttachments, Scratch.Arena);
        Rr_ColorTargetInfo *ColorTargetInfo = Info->ColorTargets + Index;
        Rr_ColorTargetBlend *Blend = &ColorTargetInfo->Blend;

        Attachment->blendEnable = Blend->BlendEnable;
        Attachment->srcColorBlendFactor = Rr_GetVulkanBlendFactor(Blend->SrcColorBlendFactor);
        Attachment->dstColorBlendFactor = Rr_GetVulkanBlendFactor(Blend->DstColorBlendFactor);
        Attachment->colorBlendOp = Rr_GetVulkanBlendOp(Blend->ColorBlendOp);
        Attachment->srcAlphaBlendFactor = Rr_GetVulkanBlendFactor(Blend->SrcAlphaBlendFactor);
        Attachment->dstAlphaBlendFactor = Rr_GetVulkanBlendFactor(Blend->DstAlphaBlendFactor);
        Attachment->alphaBlendOp = Rr_GetVulkanBlendOp(Blend->AlphaBlendOp);
        Attachment->colorWriteMask = Blend->ColorWriteMask;
    }

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = ColorAttachments.Count,
        .pAttachments = ColorAttachments.Data,
    };

    VkPipelineDepthStencilStateCreateInfo DepthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = Info->DepthStencil.EnableDepthTest,
        .depthWriteEnable = Info->DepthStencil.EnableDepthWrite,
        .depthCompareOp = Rr_GetVulkanCompareOp(Info->DepthStencil.CompareOp),
        .stencilTestEnable = Info->DepthStencil.EnableStencilTest,
        .front = Rr_GetVulkanStencilOpState(Info->DepthStencil.FrontStencilState, &Info->DepthStencil),
        .back = Rr_GetVulkanStencilOpState(Info->DepthStencil.BackStencilState, &Info->DepthStencil),
    };

    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .stageCount = ShaderStages.Count,
        .pStages = ShaderStages.Data,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &Rasterizer,
        .pMultisampleState = &Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &DepthStencil,
        .layout = Info->Layout->Handle,
        .pDynamicState = &DynamicStateInfo,
        .renderPass = Rr_GetCompatibleRenderPass(App, Info),
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

    Rr_DestroyArenaScratch(Scratch);

    return Pipeline;
}

void Rr_DestroyGraphicsPipeline(Rr_App *App, Rr_GraphicsPipeline *GraphicsPipeline)
{
    Rr_Renderer *Renderer = &App->Renderer;

    vkDestroyPipeline(Renderer->Device, GraphicsPipeline->Handle, NULL);

    Rr_DestroyObject(App, GraphicsPipeline);
}

Rr_PipelineLayout *Rr_CreatePipelineLayout(Rr_App *App, Rr_PipelineBindingSet *Sets, size_t SetCount)
{
    Rr_ArenaScratch Scratch = Rr_GetArenaScratch(NULL);

    Rr_Renderer *Renderer = &App->Renderer;
    Rr_PipelineLayout *PipelineLayout = (Rr_PipelineLayout *)Rr_CreateObject(App);

    Rr_DescriptorLayoutBuilder DescriptorLayoutBuilder = { 0 };

    VkDescriptorSetLayout *SetLayouts = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, VkDescriptorSetLayout, SetCount);

    for(size_t Index = 0; Index < SetCount; ++Index)
    {
        Rr_PipelineBindingSet *Set = Sets + Index;

        for(size_t BindingIndex = 0; BindingIndex < Set->BindingCount; ++BindingIndex)
        {
            Rr_PipelineBinding *Binding = Set->Bindings + BindingIndex;

            assert(Binding->Count > 0);

            if(Binding->Count == 1)
            {
                Rr_AddDescriptor(&DescriptorLayoutBuilder, Binding->Slot, Binding->Type, Set->Visibility);
            }
            else
            {
                Rr_AddDescriptorArray(
                    &DescriptorLayoutBuilder,
                    Binding->Slot,
                    Binding->Count,
                    Binding->Type,
                    Set->Visibility);
            }
        }

        SetLayouts[Index] = Rr_BuildDescriptorLayout(&DescriptorLayoutBuilder, Renderer->Device);
        Rr_ClearDescriptors(&DescriptorLayoutBuilder);
    }

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = SetCount,
        .pSetLayouts = SetLayouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    vkCreatePipelineLayout(Renderer->Device, &PipelineLayoutCreateInfo, NULL, &PipelineLayout->Handle);

    Rr_DestroyArenaScratch(Scratch);

    return PipelineLayout;
}

void Rr_DestroyPipelineLayout(Rr_App *App, Rr_PipelineLayout *PipelineLayout)
{
}
