#pragma once

#include <cglm/vec3.h>

#include "Rr_Framework.h"
#include "Rr_Vulkan.h"
#include "Rr_Image.h"

#define RR_TEXT_BUFFER_SIZE (1024 * 1024)
#define RR_TEXT_DEFAULT_SIZE (0.0f)
#define RR_TEXT_MAX_COLORS 8
#define RR_TEXT_MAX_GLYPHS 2048

typedef enum Rr_TextPipelineDescriptorSet
{
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
} Rr_TextPipelineDescriptorSet;

typedef struct Rr_Glyph
{
    u32 AtlasXY;
    u32 AtlasWH;
    u32 PlaneLB;
    u32 PlaneRT;
} Rr_Glyph;

typedef struct Rr_TextPerInstanceVertexInput
{
    vec2 Advance;
    u32 Unicode;
} Rr_TextPerInstanceVertexInput;

typedef struct Rr_TextGlobalsLayout
{
    float Time;
    float Reserved;
    vec2 ScreenSize;
    vec4 Pallete[RR_TEXT_MAX_COLORS];
} Rr_TextGlobalsLayout;

typedef struct Rr_TextFontLayout
{
    float Advance;
    float DistanceRange;
    vec2 AtlasSize;
    Rr_Glyph Glyphs[RR_TEXT_MAX_GLYPHS];
} Rr_TextFontLayout;

typedef struct Rr_TextPushConstants
{
    vec2 PositionScreenSpace;
    f32 Size;
    u32 Flags;
    vec4 ReservedB;
    vec4 ReservedC;
    vec4 ReservedD;
    mat4 ReservedE;
} Rr_TextPushConstants;

struct Rr_Font
{
    Rr_Image* Atlas;
    Rr_Buffer* Buffer;
    f32 Advances[RR_TEXT_MAX_GLYPHS];
    f32 LineHeight;
    f32 DefaultSize;
};

typedef struct Rr_TextPipeline
{
    VkDescriptorSetLayout DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    VkPipeline Handle;
    VkPipelineLayout Layout;
    Rr_Buffer* QuadBuffer;
    Rr_Buffer* GlobalsBuffers[RR_FRAME_OVERLAP];
    Rr_Buffer* TextBuffers[RR_FRAME_OVERLAP];
} Rr_TextPipeline;

void Rr_InitTextRenderer(Rr_Renderer* Renderer);
void Rr_CleanupTextRenderer(Rr_Renderer* Renderer);

Rr_Font Rr_CreateFont(Rr_Renderer* Renderer, Rr_Asset* FontPNG, Rr_Asset* FontJSON);
void Rr_DestroyFont(Rr_Renderer* Renderer, Rr_Font* Font);


