#pragma once

#include <cglm/vec3.h>

#include "Rr_Vulkan.h"
#include "Rr_Pipeline.h"
#include "Rr_Buffer.h"
#include "Rr_Defines.h"
#include "Rr_Image.h"

typedef struct Rr_Renderer Rr_Renderer;

typedef enum Rr_TextPipelineDescriptorSet
{
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
} Rr_TextPipelineDescriptorSet;

typedef struct Rr_TextGlobalsLayout
{
    vec2 Reserved;
    vec2 ScreenSize;
    vec4 Reserved2;
} Rr_TextGlobalsLayout;

typedef struct Rr_TextFontLayout
{
    vec3 Reserved;
    f32 AtlasSize;
} Rr_TextFontLayout;

typedef struct Rr_TextPushConstants
{
    vec2 PositionScreenSpace;
    vec2 ReservedA;
    vec4 ReservedB;
    vec4 ReservedC;
    vec4 ReservedD;
    mat4 ReservedE;
} Rr_TextPushConstants;

typedef struct Rr_Font
{
    Rr_Image Atlas;
    Rr_Buffer Buffer;
} Rr_Font;

typedef struct Rr_TextPipeline
{
    VkDescriptorSetLayout DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    VkPipeline Handle;
    VkPipelineLayout Layout;
    Rr_Buffer QuadBuffer;
    Rr_Buffer GlobalsBuffers[RR_FRAME_OVERLAP];
} Rr_TextPipeline;

void Rr_InitTextRenderer(Rr_Renderer* Renderer);
void Rr_CleanupTextRenderer(Rr_Renderer* Renderer);

Rr_Font Rr_CreateFont(Rr_Renderer* Renderer, Rr_Asset* FontPNG, Rr_Asset* FontJSON);
void Rr_DestroyFont(Rr_Renderer* Renderer, Rr_Font* Font);
