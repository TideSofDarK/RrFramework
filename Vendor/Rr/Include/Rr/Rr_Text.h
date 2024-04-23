#pragma once

#include "Rr_Vulkan.h"
#include "Rr_Pipeline.h"
#include "Rr_Buffer.h"
#include "Rr_Defines.h"
#include "Rr_Image.h"

typedef struct Rr_Renderer Rr_Renderer;

typedef enum Rr_TextPipelineDescriptorSet
{
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_CUSTOM,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
} Rr_TextPipelineDescriptorSet;

typedef struct Rr_Font
{
    Rr_Image Atlas;
} Rr_Font;

typedef struct Rr_TextPipeline
{
    VkDescriptorSetLayout DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    VkPipeline Handle;
    VkPipelineLayout Layout;
    Rr_Buffer QuadBuffer;
    Rr_Buffer GlobalsBuffers[RR_FRAME_OVERLAP];
    u32 GlobalsSize;
} Rr_TextPipeline;

void Rr_CreateTextRenderer(Rr_Renderer* Renderer);
void Rr_DestroyTextRenderer(Rr_Renderer* Renderer);

Rr_Font Rr_CreateFont(Rr_Renderer* Renderer, Rr_Asset* FontPNG, Rr_Asset* FontJSON);
void Rr_DestroyFont(Rr_Renderer* Renderer, Rr_Font* Font);