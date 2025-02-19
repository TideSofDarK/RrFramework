#include "Rr_Pipeline.h"

#include "Rr_App.h"
#include "Rr_Log.h"

#include <assert.h>

static VkRenderPass Rr_GetCompatibleRenderPass(
    Rr_Renderer *Renderer,
    Rr_PipelineInfo *Info)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    bool HasDepth = Info->DepthStencil.EnableDepthWrite;
    size_t AttachmentCount = Info->ColorTargetCount + (HasDepth ? 1 : 0);
    Rr_Attachment *Attachments =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, Rr_Attachment, AttachmentCount);

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
        Renderer,
        &(Rr_RenderPassInfo){
            .AttachmentCount = AttachmentCount,
            .Attachments = Attachments,
        });

    Rr_DestroyScratch(Scratch);

    return RenderPass;
}

Rr_GraphicsPipeline *Rr_CreateGraphicsPipeline(
    Rr_Renderer *Renderer,
    Rr_PipelineInfo *Info)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &Renderer->Device;

    Rr_GraphicsPipeline *Pipeline =
        RR_GET_FREE_LIST_ITEM(&Renderer->GraphicsPipelines, Renderer->Arena);
    Pipeline->Layout = Info->Layout;

    RR_SLICE(VkPipelineShaderStageCreateInfo) ShaderStages = { 0 };

    VkShaderModule VertModule = VK_NULL_HANDLE;
    if(Info->VertexShaderSPV.Pointer != NULL)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .pCode = (uint32_t *)Info->VertexShaderSPV.Pointer,
            .codeSize = Info->VertexShaderSPV.Size,
        };
        Device->CreateShaderModule(
            Device->Handle,
            &ShaderModuleCreateInfo,
            NULL,
            &VertModule);

        *RR_PUSH_SLICE(&ShaderStages, Scratch.Arena) =
            (VkPipelineShaderStageCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .pName = "main",
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = VertModule,
            };
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
        Device->CreateShaderModule(
            Device->Handle,
            &ShaderModuleCreateInfo,
            NULL,
            &FragModule);
        *RR_PUSH_SLICE(&ShaderStages, Scratch.Arena) =
            (VkPipelineShaderStageCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .pName = "main",
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = FragModule,
            };
    }

    RR_SLICE(VkVertexInputBindingDescription) BindingDescriptions = { 0 };
    RR_SLICE(VkVertexInputAttributeDescription) AttributeDescriptions = { 0 };
    for(size_t BindingIndex = 0; BindingIndex < Info->VertexInputBindingCount;
        ++BindingIndex)
    {
        Rr_VertexInputBinding *VertexInputBinding =
            Info->VertexInputBindings + BindingIndex;

        RR_RESERVE_SLICE(
            &AttributeDescriptions,
            AttributeDescriptions.Count + VertexInputBinding->AttributeCount,
            Scratch.Arena);

        for(size_t Index = 0; Index < VertexInputBinding->AttributeCount;
            ++Index)
        {
            Rr_VertexInputAttribute *Attribute =
                VertexInputBinding->Attributes + Index;

            VkVertexInputAttributeDescription *AttributeDescription =
                RR_PUSH_SLICE(&AttributeDescriptions, Scratch.Arena);
            AttributeDescription->location = Attribute->Location;
            AttributeDescription->format =
                Rr_GetVulkanFormat(Attribute->Format);
            AttributeDescription->binding = BindingIndex;
            VkVertexInputBindingDescription *BindingDescription = NULL;
            for(size_t BindingIndex = 0;
                BindingIndex < BindingDescriptions.Count;
                ++BindingIndex)
            {
                if(BindingDescriptions.Data[BindingIndex].binding ==
                   BindingIndex)
                {
                    BindingDescription =
                        BindingDescriptions.Data + BindingIndex;
                    break;
                }
            }
            if(BindingDescription == NULL)
            {
                BindingDescription =
                    RR_PUSH_SLICE(&BindingDescriptions, Scratch.Arena);
                BindingDescription->binding = BindingIndex;
                BindingDescription->inputRate =
                    VertexInputBinding->Rate == RR_VERTEX_INPUT_RATE_INSTANCE
                        ? VK_VERTEX_INPUT_RATE_INSTANCE
                        : VK_VERTEX_INPUT_RATE_VERTEX;
            }
            size_t Size = Rr_GetFormatSize(Attribute->Format);
            AttributeDescription->offset = BindingDescription->stride;
            BindingDescription->stride += Size;
        }
    }

    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexAttributeDescriptionCount = AttributeDescriptions.Count,
        .pVertexAttributeDescriptions = AttributeDescriptions.Data,
        .vertexBindingDescriptionCount = BindingDescriptions.Count,
        .pVertexBindingDescriptions = BindingDescriptions.Data,
    };

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

    VkDynamicState DynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT,
                                       VK_DYNAMIC_STATE_SCISSOR };
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

    RR_SLICE(VkPipelineColorBlendAttachmentState) ColorAttachments = { 0 };
    RR_RESERVE_SLICE(&ColorAttachments, Info->ColorTargetCount, Scratch.Arena);
    for(size_t Index = 0; Index < Info->ColorTargetCount; ++Index)
    {
        VkPipelineColorBlendAttachmentState *Attachment =
            RR_PUSH_SLICE(&ColorAttachments, Scratch.Arena);
        Rr_ColorTargetInfo *ColorTargetInfo = Info->ColorTargets + Index;
        Rr_ColorTargetBlend *Blend = &ColorTargetInfo->Blend;

        VkColorComponentFlags ColorWriteMask = Blend->ColorWriteMask;
        if(Blend->ColorWriteMask == RR_COLOR_COMPONENT_DEFAULT)
        {
            ColorWriteMask = RR_COLOR_COMPONENT_ALL;
        }
        Attachment->blendEnable = Blend->BlendEnable;
        Attachment->srcColorBlendFactor =
            Rr_GetVulkanBlendFactor(Blend->SrcColorBlendFactor);
        Attachment->dstColorBlendFactor =
            Rr_GetVulkanBlendFactor(Blend->DstColorBlendFactor);
        Attachment->colorBlendOp = Rr_GetVulkanBlendOp(Blend->ColorBlendOp);
        Attachment->srcAlphaBlendFactor =
            Rr_GetVulkanBlendFactor(Blend->SrcAlphaBlendFactor);
        Attachment->dstAlphaBlendFactor =
            Rr_GetVulkanBlendFactor(Blend->DstAlphaBlendFactor);
        Attachment->alphaBlendOp = Rr_GetVulkanBlendOp(Blend->AlphaBlendOp);
        Attachment->colorWriteMask = ColorWriteMask;
    }

    Pipeline->ColorAttachmentCount = Info->ColorTargetCount;

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
        .front = Rr_GetVulkanStencilOpState(
            Info->DepthStencil.FrontStencilState,
            &Info->DepthStencil),
        .back = Rr_GetVulkanStencilOpState(
            Info->DepthStencil.BackStencilState,
            &Info->DepthStencil),
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
        .renderPass = Rr_GetCompatibleRenderPass(Renderer, Info),
    };

    Device->CreateGraphicsPipelines(
        Device->Handle,
        VK_NULL_HANDLE,
        1,
        &PipelineInfo,
        NULL,
        &Pipeline->Handle);

    if(VertModule != VK_NULL_HANDLE)
    {
        Device->DestroyShaderModule(Device->Handle, VertModule, NULL);
    }

    if(FragModule != VK_NULL_HANDLE)
    {
        Device->DestroyShaderModule(Device->Handle, FragModule, NULL);
    }

    Rr_DestroyScratch(Scratch);

    return Pipeline;
}

void Rr_DestroyGraphicsPipeline(
    Rr_Renderer *Renderer,
    Rr_GraphicsPipeline *GraphicsPipeline)
{
    Rr_Device *Device = &Renderer->Device;

    Device->DestroyPipeline(Device->Handle, GraphicsPipeline->Handle, NULL);

    RR_RETURN_FREE_LIST_ITEM(&Renderer->GraphicsPipelines, GraphicsPipeline);
}

Rr_PipelineLayout *Rr_CreatePipelineLayout(
    Rr_Renderer *Renderer,
    size_t SetCount,
    Rr_PipelineBindingSet *Sets)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &Renderer->Device;

    Rr_PipelineLayout *PipelineLayout =
        RR_GET_FREE_LIST_ITEM(&Renderer->PipelineLayouts, Renderer->Arena);

    Rr_DescriptorLayoutBuilder DescriptorLayoutBuilder = { 0 };

    for(size_t Index = 0; Index < SetCount; ++Index)
    {
        Rr_PipelineBindingSet *Set = Sets + Index;

        for(size_t BindingIndex = 0; BindingIndex < Set->BindingCount;
            ++BindingIndex)
        {
            Rr_PipelineBinding *Binding = Set->Bindings + BindingIndex;

            assert(Binding->Count > 0);

            if(Binding->Count == 1)
            {
                Rr_AddDescriptor(
                    &DescriptorLayoutBuilder,
                    Binding->Slot,
                    Binding->Type,
                    Set->Stages);
            }
            else
            {
                Rr_AddDescriptorArray(
                    &DescriptorLayoutBuilder,
                    Binding->Slot,
                    Binding->Count,
                    Binding->Type,
                    Set->Stages);
            }
        }

        PipelineLayout->DescriptorSetLayouts[Index] =
            Rr_BuildDescriptorLayout(&DescriptorLayoutBuilder, Device);
        Rr_ClearDescriptors(&DescriptorLayoutBuilder);
    }

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = SetCount,
        .pSetLayouts = PipelineLayout->DescriptorSetLayouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    Device->CreatePipelineLayout(
        Device->Handle,
        &PipelineLayoutCreateInfo,
        NULL,
        &PipelineLayout->Handle);

    Rr_DestroyScratch(Scratch);

    return PipelineLayout;
}

void Rr_DestroyPipelineLayout(
    Rr_Renderer *Renderer,
    Rr_PipelineLayout *PipelineLayout)
{
    Rr_Device *Device = &Renderer->Device;

    Device->DestroyPipelineLayout(Device->Handle, PipelineLayout->Handle, NULL);

    for(size_t Index = 0; Index < RR_MAX_SETS; ++Index)
    {
        if(PipelineLayout->DescriptorSetLayouts[Index] != VK_NULL_HANDLE)
        {
            Device->DestroyDescriptorSetLayout(
                Device->Handle,
                PipelineLayout->DescriptorSetLayouts[Index],
                NULL);
        }
    }

    RR_RETURN_FREE_LIST_ITEM(&Renderer->PipelineLayouts, PipelineLayout);
}
