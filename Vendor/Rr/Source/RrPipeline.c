#include "RrPipeline.h"

#include "RrTypes.h"
#include "RrPipelineBuilder.h"

Rr_Pipeline Rr_CreatePipeline(Rr_Renderer* Renderer, Rr_PipelineCreateInfo* Info)
{
    Rr_Pipeline Pipeline = { 0 };

    VkShaderModuleCreateInfo ShaderCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
    };

    ShaderCreateInfo.codeSize = Info->VertexShader.Length;
    ShaderCreateInfo.pCode = (u32*)Info->VertexShader.Data;
    VkShaderModule VertModule;
    vkCreateShaderModule(Renderer->Device, &ShaderCreateInfo, NULL, &VertModule);

    ShaderCreateInfo.codeSize = Info->FragmentShader.Length;
    ShaderCreateInfo.pCode = (u32*)Info->FragmentShader.Data;
    VkShaderModule FragModule;
    vkCreateShaderModule(Renderer->Device, &ShaderCreateInfo, NULL, &FragModule);

    VkPushConstantRange PushConstantRange = {
        .offset = 0,
        .size = Info->PushConstantsSize,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkPipelineLayoutCreateInfo LayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = 1,
        .pSetLayouts = &Info->DescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };
    vkCreatePipelineLayout(Renderer->Device, &LayoutInfo, NULL, &Pipeline.Layout);

    Rr_PipelineBuilder Builder = Rr_DefaultPipelineBuilder(VertModule, FragModule, Renderer->DrawTarget.ColorImage.Format, Renderer->DrawTarget.DepthImage.Format, Pipeline.Layout);
    // PipelineBuilder_AlphaBlend(&Builder);
    Rr_EnableDepth(&Builder);
    Pipeline.Handle = Rr_BuildPipeline(Renderer, &Builder);

    vkDestroyShaderModule(Renderer->Device, VertModule, NULL);
    vkDestroyShaderModule(Renderer->Device, FragModule, NULL);

    return Pipeline;
}

void Rr_DestroyPipeline(Rr_Renderer* Renderer, Rr_Pipeline* Pipeline)
{
    VkDevice Device = Renderer->Device;
    vkDestroyPipelineLayout(Device, Pipeline->Layout, NULL);
    vkDestroyPipeline(Device, Pipeline->Handle, NULL);
}
