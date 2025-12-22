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

#if defined(__x86_64__) && !defined(__APPLE__)
#include <xxHash/xxh_x86dispatch.h>
#else
#include <xxHash/xxhash.h>
#endif

static VkRenderPass Rr_GetCompatibleRenderPass(
    uint32_t ColorTargetCount,
    Rr_ColorTargetInfo const *ColorTargets,
    Rr_DepthStencil const *DepthStencil,
    uint32_t SampleCount)
{
    Rr_RenderPassMapKey Key = {
        .ColorAttachmentCount = (uint8_t)ColorTargetCount,
        .DepthStencil = DepthStencil->EnableDepthTest ||
                        DepthStencil->EnableStencilTest ||
                        DepthStencil->EnableDepthWrite,
    };

    uint32_t ResolveAttachmentIndex = ColorTargetCount;

    for (uint32_t Index = 0; Index < ColorTargetCount; ++Index)
    {
        Rr_ColorTargetInfo const *Info = &ColorTargets[Index];

        Key.Attachments[Index].Format = Rr_ToVulkanImageFormat(Info->Format);
        Key.Attachments[Index].Samples = SampleCount;
        Key.Attachments[Index].LoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        Key.Attachments[Index].StoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        if (!Info->Resolve)
        {
            continue;
        }

        Key.ResolveMask |= (uint8_t)(1 << Index);
        Key.ResolveAttachmentCount++;

        Key.Attachments[ResolveAttachmentIndex].Format =
            Rr_ToVulkanImageFormat(Info->Format);
        Key.Attachments[ResolveAttachmentIndex].Samples = 1;
        Key.Attachments[ResolveAttachmentIndex].LoadOp =
            VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        Key.Attachments[ResolveAttachmentIndex].StoreOp =
            VK_ATTACHMENT_STORE_OP_DONT_CARE;

        ResolveAttachmentIndex++;
    }

    if (Key.DepthStencil)
    {
        Key.Attachments[ResolveAttachmentIndex].Format =
            Rr_ToVulkanImageFormat(DepthStencil->Format);
        Key.Attachments[ResolveAttachmentIndex].Samples = SampleCount;
        Key.Attachments[ResolveAttachmentIndex].LoadOp =
            VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        Key.Attachments[ResolveAttachmentIndex].StoreOp =
            VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    return Rr_GetVulkanRenderPass(&Key);
}

Rr_PipelineLayout *Rr_CreatePipelineLayout(
    size_t BindingSetCount,
    Rr_BindingSet const *BindingSets)
{
    assert(BindingSetCount <= RR_MAX_SETS);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    Rr_LockSpinlock(&gRenderer->PipelineLayoutsLock);

    Rr_PipelineLayout *PipelineLayout = Rr_PushPipelineLayoutIntoHiveLocked(
                                            &gRenderer->PipelineLayouts,
                                            gRenderer->Arena,
                                            &gRenderer->Lock)
                                            .Element;

    Rr_UnlockSpinlock(&gRenderer->PipelineLayoutsLock);

    *PipelineLayout = (Rr_PipelineLayout){
        .SetLayoutCount = (uint32_t)BindingSetCount,
    };

    Rr_ConsumeNextObjectName(PipelineLayout->Name);

    Rr_DescriptorSetLayoutKey Keys[RR_MAX_SETS] = { 0 };
    VkDescriptorSetLayout Handles[RR_MAX_SETS] = { 0 };

    for (size_t SetIndex = 0; SetIndex < BindingSetCount; ++SetIndex)
    {
        Rr_BindingSet const *Set = BindingSets + SetIndex;

        assert(Set->BindingCount < RR_MAX_BINDINGS);

        for (uint32_t Index = 0; Index < Set->BindingCount; ++Index)
        {
            Rr_Binding const *Binding = Set->Bindings + Index;

            assert(Binding->Type != RR_BINDING_TYPE_INVALID);
            assert(Binding->Stages != 0);
            assert(Binding->Index < RR_MAX_BINDINGS);

            Rr_PackedBinding *PackedBinding =
                &Keys[SetIndex].Bindings[Binding->Index];

            PackedBinding->Index = (uint8_t)Binding->Index;
            PackedBinding->Type =
                (uint8_t)Rr_ToVulkanDescriptorType(Binding->Type);
            PackedBinding->Stages =
                (uint8_t)Rr_ToVulkanShaderStageFlags(Binding->Stages);
            /* NOTE: Allow to omit explicitly setting Count to 1. */
            PackedBinding->Count =
                Binding->Count == 0 ? 1 : (uint8_t)Binding->Count;
            PackedBinding->ImageFormat =
                (uint8_t)Rr_ToVulkanImageFormat(Binding->ImageFormat);

            Keys[SetIndex].TotalBindingCount++;
        }

        Rr_DescriptorSetLayout *DescriptorSetLayout =
            Rr_GetDescriptorSetLayout(&Keys[SetIndex]);
        PipelineLayout->SetLayouts[SetIndex] = DescriptorSetLayout;
        Handles[SetIndex] = DescriptorSetLayout->Handle;
    }

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = PipelineLayout->SetLayoutCount,
        .pSetLayouts = Handles,
    };

    VkResult Result = Device->CreatePipelineLayout(
        Device->Handle,
        &PipelineLayoutCreateInfo,
        NULL,
        &PipelineLayout->Handle);
    assert(Result == VK_SUCCESS);

    Rr_SetVulkanObjectName(
        VK_OBJECT_TYPE_PIPELINE_LAYOUT,
        (uint64_t)PipelineLayout->Handle,
        PipelineLayout->Name);

    Rr_DestroyScratch(Scratch);

    return PipelineLayout;
}

void Rr_ReleasePipelineLayout(Rr_PipelineLayout *PipelineLayout)
{
    if (PipelineLayout == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRenderer->ReleasedPipelineLayoutsLock);

    *Rr_PushHandleIntoHiveLocked(
         &gRenderer->ReleasedPipelineLayouts,
         gRenderer->Arena,
         &gRenderer->Lock)
         .Element = PipelineLayout;

    Rr_UnlockSpinlock(&gRenderer->ReleasedPipelineLayoutsLock);
}

void Rr_DestroyPipelineLayout(Rr_PipelineLayout *PipelineLayout)
{
    assert(PipelineLayout && PipelineLayout->Handle != VK_NULL_HANDLE);

    Rr_PrintDestroyMessage(
        "Rr_PipelineLayout",
        PipelineLayout->Name,
        PipelineLayout);

    Rr_Device *Device = &gRenderer->Device;

    Device->DestroyPipelineLayout(Device->Handle, PipelineLayout->Handle, NULL);

    Rr_LockSpinlock(&gRenderer->PipelineLayoutsLock);

    Rr_PipelineLayoutHiveIterator It = Rr_GetPipelineLayoutHiveIterator(
        &gRenderer->PipelineLayouts,
        PipelineLayout);
    Rr_RemoveFromPipelineLayoutHive(&gRenderer->PipelineLayouts, &It);

    Rr_UnlockSpinlock(&gRenderer->PipelineLayoutsLock);
}

static VkSpecializationInfo *Rr_GetVulkanSpecializationInfo(
    size_t SpecializationCount,
    Rr_PipelineSpecialization const *Specializations,
    Rr_Arena *Arena)
{
    VkSpecializationInfo *SpecializationInfo =
        RR_ALLOC_TYPE(VkSpecializationInfo, Arena);
    SpecializationInfo->mapEntryCount = (uint32_t)SpecializationCount;
    VkSpecializationMapEntry *Entries = RR_ALLOC_NO_ZERO(
        sizeof(VkSpecializationMapEntry) * SpecializationCount,
        Arena);
    uintptr_t ArenaPosition = Arena->Position;
    char *DataStart = NULL;
    for (size_t Index = 0; Index < SpecializationCount; ++Index)
    {
        Rr_PipelineSpecialization const *Specialization =
            Specializations + Index;
        char *SpecializationData =
            RR_ALLOC_NO_ZERO(Specialization->Size, Arena);
        if (DataStart == NULL)
        {
            DataStart = SpecializationData;
        }
        memcpy(SpecializationData, Specialization->Data, Specialization->Size);
        Entries[Index] = (VkSpecializationMapEntry){
            .constantID = Specialization->ConstantID,
            .size = Specialization->Size,
            .offset = (uint32_t)(SpecializationData - DataStart),
        };
    }
    SpecializationInfo->pMapEntries = Entries;
    SpecializationInfo->pData = DataStart;
    SpecializationInfo->dataSize = Arena->Position - ArenaPosition;

    return SpecializationInfo;
}

Rr_ComputePipeline *Rr_CreateComputePipeline(
    Rr_ShaderInfo const *ShaderInfo,
    Rr_PipelineLayout *PipelineLayout)
{
    assert(ShaderInfo);
    assert(PipelineLayout);
    assert(
        ShaderInfo->SpecializationCount == 0 ||
        ShaderInfo->Specializations != NULL);

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = ShaderInfo->SPVSize,
        .pCode = (uint32_t *)ShaderInfo->SPVData,
    };

    VkShaderModule ShaderModule = VK_NULL_HANDLE;
    Device->CreateShaderModule(
        Device->Handle,
        &ShaderModuleCreateInfo,
        NULL,
        &ShaderModule);

    VkPipelineShaderStageCreateInfo ShaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = ShaderModule,
        .pName = ShaderInfo->EntryPoint ? ShaderInfo->EntryPoint : "main",
    };

    if (ShaderInfo->SpecializationCount)
    {
        ShaderStageCreateInfo.pSpecializationInfo =
            Rr_GetVulkanSpecializationInfo(
                ShaderInfo->SpecializationCount,
                ShaderInfo->Specializations,
                Scratch.Arena);
    }

    VkComputePipelineCreateInfo PipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = PipelineLayout->Handle,
        .stage = ShaderStageCreateInfo,
    };

    VkPipeline Handle = VK_NULL_HANDLE;
    VkResult Result = Device->CreateComputePipelines(
        Device->Handle,
        VK_NULL_HANDLE,
        1,
        &PipelineCreateInfo,
        NULL,
        &Handle);

    Rr_ComputePipeline *ComputePipeline = NULL;

    if (Result == VK_SUCCESS)
    {
        Rr_LockSpinlock(&gRenderer->ComputePipelinesLock);

        ComputePipeline = Rr_PushComputePipelineIntoHiveLocked(
                              &gRenderer->ComputePipelines,
                              gRenderer->Arena,
                              &gRenderer->Lock)
                              .Element;

        Rr_UnlockSpinlock(&gRenderer->ComputePipelinesLock);

        Rr_IncrementAtomicRelaxed(&PipelineLayout->RefCount);

        *ComputePipeline = (Rr_ComputePipeline){
            .Layout = PipelineLayout,
            .Handle = Handle,
        };

        Rr_ConsumeNextObjectName(ComputePipeline->Name);

        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_PIPELINE,
            (uint64_t)ComputePipeline->Handle,
            ComputePipeline->Name);
    }
    else
    {
        /* TODO: Set error etc... */
    }

    if (ShaderModule != VK_NULL_HANDLE)
    {
        Device->DestroyShaderModule(Device->Handle, ShaderModule, NULL);
    }

    Rr_DestroyScratch(Scratch);

    return ComputePipeline;
}

void Rr_ReleaseComputePipeline(Rr_ComputePipeline *ComputePipeline)
{
    if (ComputePipeline == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRenderer->ReleasedComputePipelinesLock);

    *Rr_PushHandleIntoHiveLocked(
         &gRenderer->ReleasedComputePipelines,
         gRenderer->Arena,
         &gRenderer->Lock)
         .Element = ComputePipeline;

    Rr_UnlockSpinlock(&gRenderer->ReleasedComputePipelinesLock);
}

void Rr_DestroyComputePipeline(Rr_ComputePipeline *ComputePipeline)
{
    assert(ComputePipeline && ComputePipeline->Handle != VK_NULL_HANDLE);

    Rr_PrintDestroyMessage(
        "Rr_ComputePipeline",
        ComputePipeline->Name,
        ComputePipeline);

    Rr_Device *Device = &gRenderer->Device;

    Device->DestroyPipeline(Device->Handle, ComputePipeline->Handle, NULL);

    Rr_DecrementAtomicRelaxed(&ComputePipeline->Layout->RefCount);

    Rr_LockSpinlock(&gRenderer->ComputePipelinesLock);

    Rr_ComputePipelineHiveIterator It = Rr_GetComputePipelineHiveIterator(
        &gRenderer->ComputePipelines,
        ComputePipeline);
    Rr_RemoveFromComputePipelineHive(&gRenderer->ComputePipelines, &It);

    Rr_UnlockSpinlock(&gRenderer->ComputePipelinesLock);
}

Rr_ColorTargetBlend Rr_AlphaBlend(void)
{
    Rr_ColorTargetBlend Blend;
    Blend.BlendEnable = true;
    Blend.SrcColorBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
    Blend.DstColorBlendFactor = RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    Blend.ColorBlendOp = RR_BLEND_OP_ADD;
    Blend.SrcAlphaBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
    Blend.DstAlphaBlendFactor = RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    Blend.AlphaBlendOp = RR_BLEND_OP_ADD;
    Blend.ColorWriteMask = RR_COLOR_COMPONENT_DEFAULT;
    return Blend;
}

Rr_GraphicsPipeline *Rr_CreateGraphicsPipeline(
    Rr_GraphicsPipelineCreateInfo const *CreateInfo,
    Rr_PipelineLayout *PipelineLayout)
{
    assert(CreateInfo);
    assert(PipelineLayout);

    bool HasDepthStencil = CreateInfo->DepthStencil.EnableDepthTest ||
                           CreateInfo->DepthStencil.EnableStencilTest ||
                           CreateInfo->DepthStencil.EnableDepthWrite;
    if (HasDepthStencil)
    {
        assert(CreateInfo->DepthStencil.Format != RR_IMAGE_FORMAT_UNDEFINED);
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    RR_ARRAY(VkPipelineShaderStageCreateInfo) ShaderStages = { 0 };

    VkShaderModule VertModule = VK_NULL_HANDLE;
    if (CreateInfo->VertexShaderInfo)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = CreateInfo->VertexShaderInfo->SPVSize,
            .pCode = (uint32_t *)CreateInfo->VertexShaderInfo->SPVData,
        };
        Device->CreateShaderModule(
            Device->Handle,
            &ShaderModuleCreateInfo,
            NULL,
            &VertModule);

        VkPipelineShaderStageCreateInfo *PipelineShaderStageCreateInfo =
            RR_PUSH_INTO_ARRAY(&ShaderStages, Scratch.Arena);
        *PipelineShaderStageCreateInfo = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = CreateInfo->VertexShaderInfo->EntryPoint
                         ? CreateInfo->VertexShaderInfo->EntryPoint
                         : "main",
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = VertModule,
        };
        if (CreateInfo->VertexShaderInfo->SpecializationCount)
        {
            PipelineShaderStageCreateInfo->pSpecializationInfo =
                Rr_GetVulkanSpecializationInfo(
                    CreateInfo->VertexShaderInfo->SpecializationCount,
                    CreateInfo->VertexShaderInfo->Specializations,
                    Scratch.Arena);
        }
    }

    VkShaderModule FragModule = VK_NULL_HANDLE;
    if (CreateInfo->FragmentShaderInfo)
    {
        VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .codeSize = CreateInfo->FragmentShaderInfo->SPVSize,
            .pCode = (uint32_t *)CreateInfo->FragmentShaderInfo->SPVData,
        };
        Device->CreateShaderModule(
            Device->Handle,
            &ShaderModuleCreateInfo,
            NULL,
            &FragModule);

        VkPipelineShaderStageCreateInfo *PipelineShaderStageCreateInfo =
            RR_PUSH_INTO_ARRAY(&ShaderStages, Scratch.Arena);
        *PipelineShaderStageCreateInfo = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .pName = CreateInfo->FragmentShaderInfo->EntryPoint
                         ? CreateInfo->FragmentShaderInfo->EntryPoint
                         : "main",
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = FragModule,
        };
        if (CreateInfo->FragmentShaderInfo->SpecializationCount)
        {
            PipelineShaderStageCreateInfo->pSpecializationInfo =
                Rr_GetVulkanSpecializationInfo(
                    CreateInfo->FragmentShaderInfo->SpecializationCount,
                    CreateInfo->FragmentShaderInfo->Specializations,
                    Scratch.Arena);
        }
    }

    RR_ARRAY(VkVertexInputBindingDescription) BindingDescriptions = { 0 };
    RR_ARRAY(VkVertexInputAttributeDescription) AttributeDescriptions = { 0 };
    for (size_t BindingIndex = 0;
         BindingIndex < CreateInfo->VertexInputBindingCount;
         ++BindingIndex)
    {
        Rr_VertexInputBinding const *VertexInputBinding =
            CreateInfo->VertexInputBindings + BindingIndex;

        RR_RESERVE_ARRAY(
            &AttributeDescriptions,
            AttributeDescriptions.Count + VertexInputBinding->AttributeCount,
            Scratch.Arena);

        for (size_t AttributeIndex = 0;
             AttributeIndex < VertexInputBinding->AttributeCount;
             ++AttributeIndex)
        {
            Rr_VertexInputAttribute const *Attribute =
                VertexInputBinding->Attributes + AttributeIndex;

            VkVertexInputAttributeDescription *AttributeDescription =
                RR_PUSH_INTO_ARRAY(&AttributeDescriptions, Scratch.Arena);
            *AttributeDescription = (VkVertexInputAttributeDescription){
                .location = Attribute->Location,
                .format = Rr_ToVulkanFormat(Attribute->Format),
                .binding = (uint32_t)BindingIndex,
            };

            VkVertexInputBindingDescription *BindingDescription = NULL;
            for (size_t Index = 0; Index < BindingDescriptions.Count; ++Index)
            {
                if (BindingDescriptions.Data[Index].binding == Index)
                {
                    BindingDescription = BindingDescriptions.Data + Index;
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
        .vertexAttributeDescriptionCount =
            (uint32_t)AttributeDescriptions.Count,
        .pVertexAttributeDescriptions = AttributeDescriptions.Data,
        .vertexBindingDescriptionCount = (uint32_t)BindingDescriptions.Count,
        .pVertexBindingDescriptions = BindingDescriptions.Data,
    };

    VkPipelineInputAssemblyStateCreateInfo InputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = Rr_ToVulkanPrimitiveTopology(CreateInfo->Topology),
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo ViewportInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo Rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .flags = 0,
        .depthClampEnable = false,
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
        .pDynamicStates = DynamicStates,
        .dynamicStateCount = RR_ARRAY_COUNT(DynamicStates),
    };

    VkPipelineMultisampleStateCreateInfo Multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    if (RR_IS_POW2(CreateInfo->Multisampling.SampleCount) &&
        CreateInfo->Multisampling.SampleCount != 0)
    {
        Multisampling.rasterizationSamples =
            CreateInfo->Multisampling.SampleCount;
    }
    else
    {
        Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    }

    RR_ARRAY(VkPipelineColorBlendAttachmentState) ColorAttachments = { 0 };
    RR_RESERVE_ARRAY(
        &ColorAttachments,
        CreateInfo->ColorTargetCount,
        Scratch.Arena);
    for (size_t Index = 0; Index < CreateInfo->ColorTargetCount; ++Index)
    {
        VkPipelineColorBlendAttachmentState *Attachment =
            RR_PUSH_INTO_ARRAY(&ColorAttachments, Scratch.Arena);
        Rr_ColorTargetInfo const *ColorTargetInfo =
            CreateInfo->ColorTargets + Index;
        Rr_ColorTargetBlend const *Blend = &ColorTargetInfo->Blend;

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
            &CreateInfo->DepthStencil.FrontStencilState,
            &CreateInfo->DepthStencil),
        .back = Rr_ToVulkanStencilOpState(
            &CreateInfo->DepthStencil.BackStencilState,
            &CreateInfo->DepthStencil),
    };

    VkGraphicsPipelineCreateInfo PipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = (uint32_t)ShaderStages.Count,
        .pStages = ShaderStages.Data,
        .pVertexInputState = &VertexInputInfo,
        .pInputAssemblyState = &InputAssembly,
        .pViewportState = &ViewportInfo,
        .pRasterizationState = &Rasterizer,
        .pMultisampleState = &Multisampling,
        .pColorBlendState = &ColorBlendInfo,
        .pDepthStencilState = &DepthStencil,
        .layout = PipelineLayout->Handle,
        .pDynamicState = &DynamicStateInfo,
        .renderPass = Rr_GetCompatibleRenderPass(
            (uint32_t)CreateInfo->ColorTargetCount,
            CreateInfo->ColorTargets,
            &CreateInfo->DepthStencil,
            Multisampling.rasterizationSamples),
    };

    VkPipeline Handle = VK_NULL_HANDLE;
    VkResult Result = Device->CreateGraphicsPipelines(
        Device->Handle,
        VK_NULL_HANDLE,
        1,
        &PipelineInfo,
        NULL,
        &Handle);

    Rr_GraphicsPipeline *GraphicsPipeline = NULL;

    if (Result == VK_SUCCESS)
    {
        Rr_LockSpinlock(&gRenderer->GraphicsPipelinesLock);

        GraphicsPipeline = Rr_PushGraphicsPipelineIntoHiveLocked(
                               &gRenderer->GraphicsPipelines,
                               gRenderer->Arena,
                               &gRenderer->Lock)
                               .Element;

        Rr_UnlockSpinlock(&gRenderer->GraphicsPipelinesLock);

        Rr_IncrementAtomicRelaxed(&PipelineLayout->RefCount);

        *GraphicsPipeline = (Rr_GraphicsPipeline){
            .Layout = PipelineLayout,
            .HasDepthStencil = HasDepthStencil,
            .Handle = Handle,
            .ColorAttachmentCount = (uint32_t)CreateInfo->ColorTargetCount,
        };

        Rr_ConsumeNextObjectName(GraphicsPipeline->Name);

        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_PIPELINE,
            (uint64_t)GraphicsPipeline->Handle,
            GraphicsPipeline->Name);
    }
    else
    {
        /* TODO: Set error etc... */
    }

    if (VertModule != VK_NULL_HANDLE)
    {
        Device->DestroyShaderModule(Device->Handle, VertModule, NULL);
    }

    if (FragModule != VK_NULL_HANDLE)
    {
        Device->DestroyShaderModule(Device->Handle, FragModule, NULL);
    }

    Rr_DestroyScratch(Scratch);

    return GraphicsPipeline;
}

void Rr_ReleaseGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipeline)
{
    if (GraphicsPipeline == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRenderer->ReleasedGraphicsPipelinesLock);

    *Rr_PushHandleIntoHiveLocked(
         &gRenderer->ReleasedGraphicsPipelines,
         gRenderer->Arena,
         &gRenderer->Lock)
         .Element = GraphicsPipeline;

    Rr_UnlockSpinlock(&gRenderer->ReleasedGraphicsPipelinesLock);
}

void Rr_DestroyGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipeline)
{
    assert(GraphicsPipeline && GraphicsPipeline->Handle != VK_NULL_HANDLE);

    Rr_PrintDestroyMessage(
        "Rr_GraphicsPipeline",
        GraphicsPipeline->Name,
        GraphicsPipeline);

    Rr_Device *Device = &gRenderer->Device;

    Device->DestroyPipeline(Device->Handle, GraphicsPipeline->Handle, NULL);

    Rr_DecrementAtomicRelaxed(&GraphicsPipeline->Layout->RefCount);

    Rr_LockSpinlock(&gRenderer->GraphicsPipelinesLock);

    Rr_GraphicsPipelineHiveIterator It = Rr_GetGraphicsPipelineHiveIterator(
        &gRenderer->GraphicsPipelines,
        GraphicsPipeline);
    Rr_RemoveFromGraphicsPipelineHive(&gRenderer->GraphicsPipelines, &It);

    Rr_UnlockSpinlock(&gRenderer->GraphicsPipelinesLock);
}

Rr_DescriptorSetLayout *Rr_GetDescriptorSetLayout(
    Rr_DescriptorSetLayoutKey *Key)
{
    VkDescriptorSetLayout *HandleRef = NULL;

    Rr_LockSpinlock(&gRenderer->DescriptorSetLayoutStorageLock);

    size_t HashSize = sizeof(Rr_DescriptorSetLayoutKey);

    Rr_DescriptorSetLayout **MapRef =
        &gRenderer->DescriptorSetLayoutStorage.Map;
    for (uint64_t Hash = XXH64(Key, HashSize, 0); *MapRef; Hash <<= 2)
    {
        if ((*MapRef)->Handle == VK_NULL_HANDLE)
        {
            (*MapRef)->Key = *Key;
            HandleRef = &(*MapRef)->Handle;

            goto Found;
        }
        else if (memcmp(Key, &(*MapRef)->Key, HashSize) == 0)
        {
            HandleRef = &(*MapRef)->Handle;

            goto Found;
        }
        MapRef = &(*MapRef)->Children[Hash >> 62];
    }
    *MapRef = Rr_PushDescriptorSetLayoutIntoHiveLocked(
                  &gRenderer->DescriptorSetLayoutStorage.Hive,
                  gRenderer->Arena,
                  &gRenderer->Lock)
                  .Element;
    (*MapRef)->Key = *Key;
    (*MapRef)->Handle = VK_NULL_HANDLE;
    RR_ZERO((*MapRef)->Children);
    HandleRef = &(*MapRef)->Handle;

Found:

    Rr_UnlockSpinlock(&gRenderer->DescriptorSetLayoutStorageLock);

    if (*HandleRef != VK_NULL_HANDLE)
    {
        return *MapRef;
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_Device *Device = &gRenderer->Device;

    RR_ARRAY(VkDescriptorSetLayoutBinding) VkBindings = { 0 };

    for (size_t BindingIndex = 0; BindingIndex < RR_MAX_BINDINGS;
         ++BindingIndex)
    {
        Rr_PackedBinding *Binding = Key->Bindings + BindingIndex;

        if (Binding->Count == 0)
        {
            continue;
        }

        *RR_PUSH_INTO_ARRAY(&VkBindings, Scratch.Arena) =
            (VkDescriptorSetLayoutBinding){
                .binding = Binding->Index,
                .descriptorType = Binding->Type,
                .descriptorCount = Binding->Count,
                .stageFlags = Binding->Stages,
            };
    }

    VkDescriptorSetLayoutCreateInfo CreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (uint32_t)VkBindings.Count,
        .pBindings = VkBindings.Data,
    };

    Device->CreateDescriptorSetLayout(
        Device->Handle,
        &CreateInfo,
        NULL,
        HandleRef);

    Rr_DestroyScratch(Scratch);

    return *MapRef;
}

/* Rr_PipelineSpecialization */

RR_BEGIN_SERIALIZE_FUNCTION(
    Rr_SerializePipelineSpecialization,
    Rr_PipelineSpecialization)
{
    RR_SERIALIZE_PTR(Struct_->Size, Struct_->Data);
}
RR_END_SERIALIZE_FUNCTION()

RR_BEGIN_DESERIALIZE_FUNCTION(
    Rr_DeserializePipelineSpecialization,
    Rr_PipelineSpecialization)
{
    RR_DESERIALIZE_PTR(Struct_->Data);
}
RR_END_DESERIALIZE_FUNCTION()

/* Rr_ShaderInfo */

RR_BEGIN_SERIALIZE_FUNCTION(Rr_SerializeShaderInfo, Rr_ShaderInfo)
{
    RR_SERIALIZE_PTR(Struct_->SPVSize, Struct_->SPVData);
    if (Struct_->EntryPoint)
    {
        RR_SERIALIZE_PTR(strlen(Struct_->EntryPoint) + 1, Struct_->EntryPoint);
    }
    Rr_SerializePipelineSpecialization(RR_SERIALIZE_ARGS(
        Struct_->SpecializationCount,
        Struct_->Specializations));
}
RR_END_SERIALIZE_FUNCTION()

RR_BEGIN_DESERIALIZE_FUNCTION(Rr_DeserializeShaderInfo, Rr_ShaderInfo)
{
    RR_DESERIALIZE_PTR(Struct_->SPVData);
    RR_DESERIALIZE_PTR(Struct_->EntryPoint);
    Rr_DeserializePipelineSpecialization(RR_DESERIALIZE_ARGS(
        Struct_->SpecializationCount,
        Struct_->Specializations));
}
RR_END_DESERIALIZE_FUNCTION()

/* Rr_VertexInputBinding */

RR_BEGIN_SERIALIZE_FUNCTION(
    Rr_SerializeVertexInputBinding,
    Rr_VertexInputBinding)
{
    RR_SERIALIZE_PTR(
        sizeof(Rr_VertexInputAttribute) * Struct_->AttributeCount,
        Struct_->Attributes);
}
RR_END_SERIALIZE_FUNCTION()

RR_BEGIN_DESERIALIZE_FUNCTION(
    Rr_DeserializeVertexInputBinding,
    Rr_VertexInputBinding)
{
    RR_DESERIALIZE_PTR(Struct_->Attributes);
}
RR_END_DESERIALIZE_FUNCTION()

/* Rr_GraphicsPipelineCreateInfo */

RR_BEGIN_SERIALIZE_FUNCTION(
    Rr_SerializeGraphicsPipelineCreateInfo,
    Rr_GraphicsPipelineCreateInfo)
{
    Rr_SerializeShaderInfo(RR_SERIALIZE_ARGS(1, Struct_->VertexShaderInfo));
    Rr_SerializeShaderInfo(RR_SERIALIZE_ARGS(1, Struct_->FragmentShaderInfo));
    Rr_SerializeVertexInputBinding(RR_SERIALIZE_ARGS(
        Struct_->VertexInputBindingCount,
        Struct_->VertexInputBindings));
    RR_SERIALIZE_PTR(
        sizeof(Rr_ColorTargetInfo) * Struct_->ColorTargetCount,
        Struct_->ColorTargets);
}
RR_END_SERIALIZE_FUNCTION()

RR_BEGIN_DESERIALIZE_FUNCTION(
    Rr_DeserializeGraphicsPipelineCreateInfo,
    Rr_GraphicsPipelineCreateInfo)
{
    Rr_DeserializeShaderInfo(RR_DESERIALIZE_ARGS(1, Struct_->VertexShaderInfo));
    Rr_DeserializeShaderInfo(
        RR_DESERIALIZE_ARGS(1, Struct_->FragmentShaderInfo));
    Rr_DeserializeVertexInputBinding(RR_DESERIALIZE_ARGS(
        Struct_->VertexInputBindingCount,
        Struct_->VertexInputBindings));
    RR_DESERIALIZE_PTR(Struct_->ColorTargets);
}
RR_END_DESERIALIZE_FUNCTION()
