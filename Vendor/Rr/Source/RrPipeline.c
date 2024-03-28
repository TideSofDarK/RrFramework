#include "RrPipeline.h"

#include "RrTypes.h"
#include "RrPipelineBuilder.h"

Rr_Pipeline Rr_CreatePipeline(Rr_Renderer* Renderer, Rr_PipelineCreateInfo* Info)
{
    VkShaderModuleCreateInfo ShaderCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
    };

    ShaderCreateInfo.codeSize = Info->VertexSPVLength;
    ShaderCreateInfo.pCode = Info->VertexSPV;
    VkShaderModule VertModule;
    vkCreateShaderModule(Renderer->Device, &ShaderCreateInfo, NULL, &VertModule);

    ShaderCreateInfo.codeSize = Info->FragmentSPVLength;
    ShaderCreateInfo.pCode = Info->FragmentSPV;
    VkShaderModule FragModule;
    vkCreateShaderModule(Renderer->Device, &ShaderCreateInfo, NULL, &FragModule);

    VkPushConstantRange PushConstantRange = {
        .offset = 0,
        .size = sizeof(Rr_PushConstants3D),
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    VkPipelineLayoutCreateInfo LayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = 1,
        .pSetLayouts = &Renderer->SceneDataLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };
    vkCreatePipelineLayout(Renderer->Device, &LayoutInfo, NULL, &Renderer->MeshPipelineLayout);

    Rr_PipelineBuilder Builder;
    PipelineBuilder_Default(&Builder, VertModule, FragModule, Renderer->DrawTarget.ColorImage.Format, Renderer->DrawTarget.DepthImage.Format, Renderer->MeshPipelineLayout);
    // PipelineBuilder_AlphaBlend(&Builder);
    PipelineBuilder_Depth(&Builder);
    Rr_Pipeline Pipeline = {0};
    Pipeline.Handle = Rr_BuildPipeline(Renderer, &Builder);
    Renderer->MeshPipeline = Pipeline.Handle;

    vkDestroyShaderModule(Renderer->Device, VertModule, NULL);
    vkDestroyShaderModule(Renderer->Device, FragModule, NULL);

    return Pipeline;
}
