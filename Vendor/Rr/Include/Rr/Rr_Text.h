#pragma once

#include <cglm/vec3.h>

#include "Rr_Vulkan.h"
#include "Rr_Pipeline.h"
#include "Rr_Buffer.h"
#include "Rr_Defines.h"
#include "Rr_Image.h"

#define RR_TEXT_BUFFER_SIZE (1024 * 1024)
#define RR_TEXT_DEFAULT_SIZE (0.0f)
#define RR_TEXT_MAX_COLORS 8

typedef struct Rr_Renderer Rr_Renderer;

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
    Rr_Glyph Glyphs[2048];
} Rr_TextFontLayout;

// typedef struct Rr_TextStringLayout
// {
//     vec2 Reserved;
//     vec2 AtlasSize;
//     Rr_Glyph Glyphs[128];
// } Rr_TextStringLayout;

typedef struct Rr_TextPushConstants
{
    vec2 PositionScreenSpace;
    f32 Size;
    f32 ReservedA;
    vec4 ReservedB;
    vec4 ReservedC;
    vec4 ReservedD;
    mat4 ReservedE;
} Rr_TextPushConstants;

typedef struct Rr_Font
{
    Rr_Image Atlas;
    Rr_Buffer Buffer;
    f32 Advances[2048];
    f32 LineHeight;
    f32 DefaultSize;
} Rr_Font;

typedef struct Rr_TextPipeline
{
    VkDescriptorSetLayout DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    VkPipeline Handle;
    VkPipelineLayout Layout;
    Rr_Buffer QuadBuffer;
    Rr_Buffer GlobalsBuffers[RR_FRAME_OVERLAP];
    Rr_Buffer TextBuffers[RR_FRAME_OVERLAP];
} Rr_TextPipeline;

void Rr_InitTextRenderer(Rr_Renderer* Renderer);
void Rr_CleanupTextRenderer(Rr_Renderer* Renderer);

Rr_Font Rr_CreateFont(Rr_Renderer* Renderer, Rr_Asset* FontPNG, Rr_Asset* FontJSON);
void Rr_DestroyFont(Rr_Renderer* Renderer, Rr_Font* Font);

typedef struct Rr_String
{
    size_t Length;
    const u32* Data;
} Rr_String;

Rr_String Rr_CreateString(const char* CString);
void Rr_DestroyString(Rr_String* String);
