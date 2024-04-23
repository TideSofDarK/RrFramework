#include "Rr_Text.h"

#include "Rr_Renderer.h"
#include "Rr_Asset.h"

static void InitBuiltinFont(Rr_Renderer* Renderer)
{
    Rr_ExternAsset(BuiltinFontPNG);
    Rr_ExternAsset(BuiltinFontJSON);
}

static void InitGenericPipelineLayout(Rr_Renderer* Renderer)
{
    VkDevice Device = Renderer->Device;
    Rr_TextPipeline* TextPipeline = &Renderer->TextPipeline;

    /* Descriptor Set Layouts */
    Rr_DescriptorLayoutBuilder DescriptorLayoutBuilder = { 0 };
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS] =
        Rr_BuildDescriptorLayout(
            &DescriptorLayoutBuilder,
            Device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    Rr_ClearDescriptors(&DescriptorLayoutBuilder);
    Rr_AddDescriptor(&DescriptorLayoutBuilder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    TextPipeline->DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_CUSTOM] =
        Rr_BuildDescriptorLayout(
            &DescriptorLayoutBuilder,
            Device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    /* Pipeline Layout */
    VkPushConstantRange PushConstantRange = {
        .offset = 0,
        .size = 128,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkPipelineLayoutCreateInfo LayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
        .pSetLayouts = TextPipeline->DescriptorSetLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &PushConstantRange,
    };
    vkCreatePipelineLayout(Device, &LayoutInfo, NULL, &TextPipeline->Layout);

    /* Pipeline */

}

void Rr_CreateTextRenderer(Rr_Renderer* Renderer)
{
    InitBuiltinFont(Renderer);
    InitGenericPipelineLayout(Renderer);
}

void Rr_DestroyTextRenderer(Rr_Renderer* Renderer)
{
    Rr_TextPipeline* TextPipeline = &Renderer->TextPipeline;
    VkDevice Device = Renderer->Device;
    vkDestroyPipeline(Device, TextPipeline->Handle, NULL);
    vkDestroyPipelineLayout(Device, TextPipeline->Layout, NULL);
    for (int Index = 0; Index < RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT; ++Index)
    {
        vkDestroyDescriptorSetLayout(Device, TextPipeline->DescriptorSetLayouts[Index], NULL);
    }
}
