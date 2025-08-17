/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Rr_Pipeline.h"

#include "Rr_Renderer.h"

#include <assert.h>

#include <xxHash/xxhash.h>

static VkRenderPass Rr_GetCompatibleRenderPass(
    Rr_GraphicsPipelineCreateInfo *Info)
{
    Rr_RenderPassMapKey Key;
    Key.ColorAttachmentCount = Info->ColorTargetCount;
    Key.ResolveAttachmentCount = 0;
    Key.DepthStencil = Info->DepthStencil.EnableDepthTest ||
                       Info->DepthStencil.EnableStencilTest ||
                       Info->DepthStencil.EnableDepthWrite;
    size_t AttachmentCount = Key.ColorAttachmentCount +
                             Key.ResolveAttachmentCount +
                             (size_t)Key.DepthStencil;

    size_t AttachmentIndex = 0;
    size_t Boundary = Info->ColorTargetCount;

    if (Info->ColorTargetCount > 0)
    {
        for (; AttachmentIndex < Boundary; ++AttachmentIndex)
        {
            Key.Attachments[AttachmentIndex].Format = Rr_ToVulkanTextureFormat(
                Info->ColorTargets[AttachmentIndex].Format);
            Key.Attachments[AttachmentIndex].Samples = 1;
            Key.Attachments[AttachmentIndex].LoadOp =
                VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            Key.Attachments[AttachmentIndex].StoreOp =
                VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
    }

    if (Key.DepthStencil)
    {
        Key.Attachments[AttachmentIndex].Format =
            Rr_ToVulkanTextureFormat(Info->DepthStencil.Format);
        Key.Attachments[AttachmentIndex].Samples = 1;
        Key.Attachments[AttachmentIndex].LoadOp =
            VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        Key.Attachments[AttachmentIndex].StoreOp =
            VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    return Rr_GetVulkanRenderPass(&Key);
}

Rr_PipelineLayout *Rr_CreatePipelineLayout(
    uint32_t SetCount,
    Rr_BindingSet *BindingSets)
{
    assert(SetCount <= RR_MAX_SETS);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_PipelineLayoutHiveIterator It = Rr_PushPipelineLayoutIntoHive(
        &gRenderer->PipelineLayouts,
        gRenderer->Arena);
    Rr_PipelineLayout *PipelineLayout = It.Element;

    Rr_UnlockSpinlock(&gRenderer->Lock);

    PipelineLayout->SetLayoutCount = SetCount;

    Rr_Device *Device = &gRenderer->Device;

    VkDescriptorSetLayout Handles[RR_MAX_SETS] = { 0 };

    for (size_t Index = 0; Index < SetCount; ++Index)
    {
        Rr_BindingSet *Set = BindingSets + Index;

        PipelineLayout->SetLayouts[Index] = Rr_GetDescriptorSetLayout(Set);
        Handles[Index] = PipelineLayout->SetLayouts[Index]->Handle;
    }

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = SetCount,
        .pSetLayouts = Handles,
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

void Rr_ReleasePipelineLayout(Rr_PipelineLayout *PipelineLayout)
{
    if (PipelineLayout == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRenderer->Lock);

    *Rr_PushHandleIntoHive(
         &gRenderer->ReleasedPipelineLayouts,
         gRenderer->Arena)
         .Element = PipelineLayout;

    Rr_UnlockSpinlock(&gRenderer->Lock);
}

void Rr_DestroyPipelineLayout(Rr_PipelineLayout *PipelineLayout)
{
    Rr_Device *Device = &gRenderer->Device;

    Device->DestroyPipelineLayout(Device->Handle, PipelineLayout->Handle, NULL);

    Rr_PipelineLayoutHiveIterator It = Rr_GetPipelineLayoutHiveIterator(
        &gRenderer->PipelineLayouts,
        PipelineLayout);
    Rr_RemoveFromPipelineLayoutHive(&gRenderer->PipelineLayouts, &It);

    RR_LOG("Destroyed pipeline layout with address %p", (void *)PipelineLayout);
}

static VkSpecializationInfo *Rr_GetVulkanSpecializationInfo(
    size_t SpecializationCount,
    Rr_PipelineSpecialization *Specializations,
    Rr_Arena *Arena)
{
    VkSpecializationInfo *SpecializationInfo =
        RR_ALLOC_TYPE(Arena, VkSpecializationInfo);
    SpecializationInfo->mapEntryCount = (uint32_t)SpecializationCount;
    VkSpecializationMapEntry *Entries = RR_ALLOC_NO_ZERO(
        Arena,
        sizeof(VkSpecializationMapEntry) * SpecializationCount);
    uintptr_t ArenaPosition = Arena->Position;
    char *DataStart = NULL;
    for (size_t Index = 0; Index < SpecializationCount; ++Index)
    {
        Rr_PipelineSpecialization *Specialization = Specializations + Index;
        char *SpecializationData =
            RR_ALLOC_NO_ZERO(Arena, Specialization->Data.Size);
        if (DataStart == NULL)
        {
            DataStart = SpecializationData;
        }
        memcpy(
            SpecializationData,
            Specialization->Data.Pointer,
            Specialization->Data.Size);
        size_t Offset = SpecializationData - DataStart;
        Entries[Index] = (VkSpecializationMapEntry){
            .constantID = Specialization->ConstantID,
            .size = Specialization->Data.Size,
            .offset = (uint32_t)Offset,
        };
    }
    SpecializationInfo->pMapEntries = Entries;
    SpecializationInfo->pData = DataStart;
    SpecializationInfo->dataSize = Arena->Position - ArenaPosition;

    return SpecializationInfo;
}

Rr_ComputePipeline *Rr_CreateComputePipeline(
    Rr_ComputePipelineCreateInfo *CreateInfo)
{
    assert(CreateInfo);
    assert(CreateInfo->Layout != NULL);
    assert(
        CreateInfo->SpecializationCount == 0 ||
        CreateInfo->Specializations != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_ComputePipelineHiveIterator It = Rr_PushComputePipelineIntoHive(
        &gRenderer->ComputePipelines,
        gRenderer->Arena);
    Rr_ComputePipeline *Pipeline = It.Element;

    Rr_UnlockSpinlock(&gRenderer->Lock);

    atomic_fetch_add_explicit(
        &CreateInfo->Layout->RefCount,
        1,
        memory_order_relaxed);

    Pipeline->Layout = CreateInfo->Layout;

    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = CreateInfo->ShaderSPV.Size,
        .pCode = (uint32_t *)CreateInfo->ShaderSPV.Pointer,
    };

    VkShaderModule ShaderModule;

    Device->CreateShaderModule(
        Device->Handle,
        &ShaderModuleCreateInfo,
        NULL,
        &ShaderModule);

    VkPipelineShaderStageCreateInfo ShaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = ShaderModule,
        .pName = "main",
    };

    if (CreateInfo->SpecializationCount > 0)
    {
        ShaderStageCreateInfo.pSpecializationInfo =
            Rr_GetVulkanSpecializationInfo(
                CreateInfo->SpecializationCount,
                CreateInfo->Specializations,
                Scratch.Arena);
    }

    VkComputePipelineCreateInfo PipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = CreateInfo->Layout->Handle,
        .stage = ShaderStageCreateInfo,
    };

    Device->CreateComputePipelines(
        Device->Handle,
        VK_NULL_HANDLE,
        1,
        &PipelineCreateInfo,
        NULL,
        &Pipeline->Handle);

    Device->DestroyShaderModule(Device->Handle, ShaderModule, NULL);

    Rr_DestroyScratch(Scratch);

    return Pipeline;
}

void Rr_ReleaseComputePipeline(Rr_ComputePipeline *ComputePipeline)
{
    if (ComputePipeline == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRenderer->Lock);

    *Rr_PushHandleIntoHive(
         &gRenderer->ReleasedComputePipelines,
         gRenderer->Arena)
         .Element = ComputePipeline;

    Rr_UnlockSpinlock(&gRenderer->Lock);
}

void Rr_DestroyComputePipeline(Rr_ComputePipeline *ComputePipeline)
{
    Rr_Device *Device = &gRenderer->Device;

    Device->DestroyPipeline(Device->Handle, ComputePipeline->Handle, NULL);

    atomic_fetch_sub_explicit(
        &ComputePipeline->Layout->RefCount,
        1,
        memory_order_relaxed);

    Rr_ComputePipelineHiveIterator It = Rr_GetComputePipelineHiveIterator(
        &gRenderer->ComputePipelines,
        ComputePipeline);
    Rr_RemoveFromComputePipelineHive(&gRenderer->ComputePipelines, &It);

    RR_LOG(
        "Destroyed compute pipeline with address %p",
        (void *)ComputePipeline);
}

Rr_GraphicsPipeline *Rr_CreateGraphicsPipeline(
    Rr_GraphicsPipelineCreateInfo *CreateInfo)
{
    assert(CreateInfo);
    assert(CreateInfo->Layout);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_GraphicsPipelineHiveIterator It = Rr_PushGraphicsPipelineIntoHive(
        &gRenderer->GraphicsPipelines,
        gRenderer->Arena);
    Rr_GraphicsPipeline *Pipeline = It.Element;

    Rr_UnlockSpinlock(&gRenderer->Lock);

    atomic_fetch_add_explicit(
        &CreateInfo->Layout->RefCount,
        1,
        memory_order_relaxed);

    Pipeline->Layout = CreateInfo->Layout;
    Pipeline->HasDepthStencil = CreateInfo->DepthStencil.EnableDepthTest ||
                                CreateInfo->DepthStencil.EnableStencilTest ||
                                CreateInfo->DepthStencil.EnableDepthWrite;

    if (Pipeline->HasDepthStencil)
    {
        assert(CreateInfo->DepthStencil.Format != RR_TEXTURE_FORMAT_UNDEFINED);
    }

    RR_ARRAY(VkPipelineShaderStageCreateInfo) ShaderStages = { 0 };

    VkShaderModule VertModule = VK_NULL_HANDLE;
    if (CreateInfo->VertexShaderSPV.Pointer != NULL)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .codeSize = CreateInfo->VertexShaderSPV.Size,
            .pCode = (uint32_t *)CreateInfo->VertexShaderSPV.Pointer,
        };
        Device->CreateShaderModule(
            Device->Handle,
            &ShaderModuleCreateInfo,
            NULL,
            &VertModule);

        *RR_PUSH_INTO_ARRAY(&ShaderStages, Scratch.Arena) =
            (VkPipelineShaderStageCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .pName = "main",
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = VertModule,
            };
    }

    VkShaderModule FragModule = VK_NULL_HANDLE;
    if (CreateInfo->FragmentShaderSPV.Pointer != NULL)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .codeSize = CreateInfo->FragmentShaderSPV.Size,
            .pCode = (uint32_t *)CreateInfo->FragmentShaderSPV.Pointer,
        };
        Device->CreateShaderModule(
            Device->Handle,
            &ShaderModuleCreateInfo,
            NULL,
            &FragModule);
        *RR_PUSH_INTO_ARRAY(&ShaderStages, Scratch.Arena) =
            (VkPipelineShaderStageCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .pName = "main",
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = FragModule,
            };
    }

    RR_ARRAY(VkVertexInputBindingDescription) BindingDescriptions = { 0 };
    RR_ARRAY(VkVertexInputAttributeDescription) AttributeDescriptions = { 0 };
    for (size_t BindingIndex = 0;
         BindingIndex < CreateInfo->VertexInputBindingCount;
         ++BindingIndex)
    {
        Rr_VertexInputBinding *VertexInputBinding =
            CreateInfo->VertexInputBindings + BindingIndex;

        RR_RESERVE_ARRAY(
            &AttributeDescriptions,
            AttributeDescriptions.Count + VertexInputBinding->AttributeCount,
            Scratch.Arena);

        for (size_t Index = 0; Index < VertexInputBinding->AttributeCount;
             ++Index)
        {
            Rr_VertexInputAttribute *Attribute =
                VertexInputBinding->Attributes + Index;

            VkVertexInputAttributeDescription *AttributeDescription =
                RR_PUSH_INTO_ARRAY(&AttributeDescriptions, Scratch.Arena);
            *AttributeDescription = (VkVertexInputAttributeDescription){
                .location = Attribute->Location,
                .format = Rr_ToVulkanFormat(Attribute->Format),
                .binding = (uint32_t)BindingIndex,
            };

            VkVertexInputBindingDescription *BindingDescription = NULL;
            for (size_t BindingIndex = 0;
                 BindingIndex < BindingDescriptions.Count;
                 ++BindingIndex)
            {
                if (BindingDescriptions.Data[BindingIndex].binding ==
                    BindingIndex)
                {
                    BindingDescription =
                        BindingDescriptions.Data + BindingIndex;
                    break;
                }
            }
            if (BindingDescription == NULL)
            {
                BindingDescription =
                    RR_PUSH_INTO_ARRAY(&BindingDescriptions, Scratch.Arena);
                *BindingDescription = (VkVertexInputBindingDescription){
                    .stride = 0,
                    .binding = (uint32_t)BindingIndex,
                    .inputRate = VertexInputBinding->Rate ==
                                         RR_VERTEX_INPUT_RATE_INSTANCE
                                     ? VK_VERTEX_INPUT_RATE_INSTANCE
                                     : VK_VERTEX_INPUT_RATE_VERTEX,
                };
            }
            size_t Size = Rr_GetFormatSize(Attribute->Format);
            AttributeDescription->offset = BindingDescription->stride;
            BindingDescription->stride += (uint32_t)Size;
        }
    }

    VkPipelineVertexInputStateCreateInfo VertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexAttributeDescriptionCount =
            (uint32_t)AttributeDescriptions.Count,
        .pVertexAttributeDescriptions = AttributeDescriptions.Data,
        .vertexBindingDescriptionCount = (uint32_t)BindingDescriptions.Count,
        .pVertexBindingDescriptions = BindingDescriptions.Data,
    };

    VkPipelineInputAssemblyStateCreateInfo InputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .topology = Rr_ToVulkanPrimitiveTopology(CreateInfo->Topology),
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
        .depthClampEnable = CreateInfo->Rasterizer.EnableDepthClip,
        .rasterizerDiscardEnable = false,
        .polygonMode =
            Rr_ToVulkanPolygonMode(CreateInfo->Rasterizer.PolygonMode),
        .cullMode = Rr_ToVulkanCullMode(CreateInfo->Rasterizer.CullMode),
        .frontFace = Rr_ToVulkanFrontFace(CreateInfo->Rasterizer.FrontFace),
        .depthBiasEnable = CreateInfo->Rasterizer.EnableDepthBias,
        .depthBiasConstantFactor =
            CreateInfo->Rasterizer.DepthBiasConstantFactor,
        .depthBiasClamp = CreateInfo->Rasterizer.DepthBiasClamp,
        .depthBiasSlopeFactor = CreateInfo->Rasterizer.DepthBiasSlopeFactor,
        .lineWidth = 1.0f,
    };

    VkDynamicState DynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT,
                                       VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo DynamicStateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .pDynamicStates = DynamicStates,
        .dynamicStateCount = RR_ARRAY_COUNT(DynamicStates),
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

    RR_ARRAY(VkPipelineColorBlendAttachmentState) ColorAttachments = { 0 };
    RR_RESERVE_ARRAY(
        &ColorAttachments,
        CreateInfo->ColorTargetCount,
        Scratch.Arena);
    for (size_t Index = 0; Index < CreateInfo->ColorTargetCount; ++Index)
    {
        VkPipelineColorBlendAttachmentState *Attachment =
            RR_PUSH_INTO_ARRAY(&ColorAttachments, Scratch.Arena);
        Rr_ColorTargetInfo *ColorTargetInfo = CreateInfo->ColorTargets + Index;
        Rr_ColorTargetBlend *Blend = &ColorTargetInfo->Blend;

        VkColorComponentFlags ColorWriteMask = Blend->ColorWriteMask;
        if (Blend->ColorWriteMask == RR_COLOR_COMPONENT_DEFAULT)
        {
            ColorWriteMask = RR_COLOR_COMPONENT_ALL;
        }
        Attachment->blendEnable = Blend->BlendEnable;
        Attachment->srcColorBlendFactor =
            Rr_ToVulkanBlendFactor(Blend->SrcColorBlendFactor);
        Attachment->dstColorBlendFactor =
            Rr_ToVulkanBlendFactor(Blend->DstColorBlendFactor);
        Attachment->colorBlendOp = Rr_ToVulkanBlendOp(Blend->ColorBlendOp);
        Attachment->srcAlphaBlendFactor =
            Rr_ToVulkanBlendFactor(Blend->SrcAlphaBlendFactor);
        Attachment->dstAlphaBlendFactor =
            Rr_ToVulkanBlendFactor(Blend->DstAlphaBlendFactor);
        Attachment->alphaBlendOp = Rr_ToVulkanBlendOp(Blend->AlphaBlendOp);
        Attachment->colorWriteMask = ColorWriteMask;
    }

    Pipeline->ColorAttachmentCount = (uint32_t)CreateInfo->ColorTargetCount;

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = (uint32_t)ColorAttachments.Count,
        .pAttachments = ColorAttachments.Data,
    };

    VkPipelineDepthStencilStateCreateInfo DepthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = CreateInfo->DepthStencil.EnableDepthTest,
        .depthWriteEnable = CreateInfo->DepthStencil.EnableDepthWrite,
        .depthCompareOp =
            Rr_ToVulkanCompareOp(CreateInfo->DepthStencil.CompareOp),
        .stencilTestEnable = CreateInfo->DepthStencil.EnableStencilTest,
        .front = Rr_ToVulkanStencilOpState(
            CreateInfo->DepthStencil.FrontStencilState,
            &CreateInfo->DepthStencil),
        .back = Rr_ToVulkanStencilOpState(
            CreateInfo->DepthStencil.BackStencilState,
            &CreateInfo->DepthStencil),
    };

    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .stageCount = (uint32_t)ShaderStages.Count,
        .pStages = ShaderStages.Data,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &Rasterizer,
        .pMultisampleState = &Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &DepthStencil,
        .layout = CreateInfo->Layout->Handle,
        .pDynamicState = &DynamicStateInfo,
        .renderPass = Rr_GetCompatibleRenderPass(CreateInfo),
    };

    Device->CreateGraphicsPipelines(
        Device->Handle,
        VK_NULL_HANDLE,
        1,
        &PipelineInfo,
        NULL,
        &Pipeline->Handle);

    if (VertModule != VK_NULL_HANDLE)
    {
        Device->DestroyShaderModule(Device->Handle, VertModule, NULL);
    }

    if (FragModule != VK_NULL_HANDLE)
    {
        Device->DestroyShaderModule(Device->Handle, FragModule, NULL);
    }

    Rr_DestroyScratch(Scratch);

    return Pipeline;
}

void Rr_ReleaseGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipeline)
{
    if (GraphicsPipeline == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRenderer->Lock);

    *Rr_PushHandleIntoHive(
         &gRenderer->ReleasedGraphicsPipelines,
         gRenderer->Arena)
         .Element = GraphicsPipeline;

    Rr_UnlockSpinlock(&gRenderer->Lock);
}

void Rr_DestroyGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipeline)
{
    Rr_Device *Device = &gRenderer->Device;

    Device->DestroyPipeline(Device->Handle, GraphicsPipeline->Handle, NULL);

    atomic_fetch_sub_explicit(
        &GraphicsPipeline->Layout->RefCount,
        1,
        memory_order_relaxed);

    Rr_GraphicsPipelineHiveIterator It = Rr_GetGraphicsPipelineHiveIterator(
        &gRenderer->GraphicsPipelines,
        GraphicsPipeline);
    Rr_RemoveFromGraphicsPipelineHive(&gRenderer->GraphicsPipelines, &It);

    RR_LOG(
        "Destroyed graphics pipeline with address %p",
        (void *)GraphicsPipeline);
}

Rr_DescriptorSetLayout *Rr_GetDescriptorSetLayout(Rr_BindingSet *BindingSet)
{
    uint32_t Hash = XXH32(
        BindingSet->Bindings,
        sizeof(Rr_Binding) * BindingSet->BindingCount,
        0);
    for (size_t LayoutIndex = 0;
         LayoutIndex < gRenderer->DescriptorSetLayouts.Count;
         ++LayoutIndex)
    {
        Rr_DescriptorSetLayout *DescriptorSetLayout =
            gRenderer->DescriptorSetLayouts.Data + LayoutIndex;
        if (DescriptorSetLayout == NULL)
        {
            continue;
        }
        if (DescriptorSetLayout->Hash == Hash &&
            DescriptorSetLayout->Set.BindingCount == BindingSet->BindingCount &&
            DescriptorSetLayout->Set.Stages == BindingSet->Stages)
        {
            bool Fail = false;
            for (size_t BindingIndex = 0;
                 BindingIndex < BindingSet->BindingCount;
                 ++BindingIndex)
            {
                Rr_Binding *BindingA = &BindingSet->Bindings[BindingIndex];
                Rr_Binding *BindingB =
                    &DescriptorSetLayout->Set.Bindings[BindingIndex];
                if (BindingA->Index != BindingB->Index ||
                    BindingA->Count != BindingB->Count ||
                    BindingA->Type != BindingB->Type)
                {
                    Fail = true;
                    break;
                }
            }
            if (!Fail)
            {

                return DescriptorSetLayout;
            }
        }
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    RR_ARRAY(VkDescriptorSetLayoutBinding) Bindings = { 0 };

    for (size_t BindingIndex = 0; BindingIndex < BindingSet->BindingCount;
         ++BindingIndex)
    {
        Rr_Binding *Binding = BindingSet->Bindings + BindingIndex;

        *RR_PUSH_INTO_ARRAY(&Bindings, Scratch.Arena) =
            (VkDescriptorSetLayoutBinding){
                .binding = Binding->Index,
                .descriptorType = Rr_ToVulkanDescriptorType(Binding->Type),
                .descriptorCount = Binding->Count > 0 ? Binding->Count : 1,
                .stageFlags = Rr_ToVulkanShaderStageFlags(BindingSet->Stages),
            };
    }

    VkDescriptorSetLayoutCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = Bindings.Count,
        .pBindings = Bindings.Data,
    };

    Rr_LockSpinlock(&gRenderer->Lock);

    Rr_DescriptorSetLayout *DescriptorSetLayout =
        RR_PUSH_INTO_ARRAY(&gRenderer->DescriptorSetLayouts, gRenderer->Arena);

    DescriptorSetLayout->Hash = Hash;
    DescriptorSetLayout->Set = *BindingSet;
    RR_ALLOC_COPY(
        gRenderer->Arena,
        DescriptorSetLayout->Set.Bindings,
        BindingSet->Bindings,
        sizeof(Rr_Binding) * BindingSet->BindingCount);
    Device->CreateDescriptorSetLayout(
        Device->Handle,
        &CreateInfo,
        NULL,
        &DescriptorSetLayout->Handle);

    Rr_UnlockSpinlock(&gRenderer->Lock);

    Rr_DestroyScratch(Scratch);

    return DescriptorSetLayout;
}
