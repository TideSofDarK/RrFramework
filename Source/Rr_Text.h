#pragma once

#include <Rr/Rr_Math.h>
#include <Rr/Rr_Text.h>
#include "Rr_Vulkan.h"

struct Rr_Image;
struct Rr_Buffer;
struct Rr_Pipeline;

struct Rr_Font
{
    struct Rr_Image *Atlas;
    struct Rr_Buffer *Buffer;
    float *Advances;
    float LineHeight;
    float DefaultSize;
};

typedef enum Rr_TextPipelineDescriptorSet
{
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
} Rr_TextPipelineDescriptorSet;

typedef struct Rr_Glyph Rr_Glyph;
struct Rr_Glyph
{
    uint32_t AtlasXY;
    uint32_t AtlasWH;
    uint32_t PlaneLB;
    uint32_t PlaneRT;
};

typedef struct Rr_TextPerInstanceVertexInput Rr_TextPerInstanceVertexInput;
struct Rr_TextPerInstanceVertexInput
{
    Rr_Vec2 Advance;
    uint32_t Unicode;
};

typedef struct Rr_TextGlobalsLayout Rr_TextGlobalsLayout;
struct Rr_TextGlobalsLayout
{
    float Time;
    float Reserved;
    Rr_Vec2 ScreenSize;
    Rr_Vec4 Palette[RR_TEXT_MAX_COLORS];
};

typedef struct Rr_TextFontLayout Rr_TextFontLayout;
struct Rr_TextFontLayout
{
    float Advance;
    float DistanceRange;
    Rr_Vec2 AtlasSize;
    Rr_Glyph Glyphs[RR_TEXT_MAX_GLYPHS];
};

typedef struct Rr_TextPushConstants Rr_TextPushConstants;
struct Rr_TextPushConstants
{
    Rr_Vec2 PositionScreenSpace;
    float Size;
    uint32_t Flags;
    Rr_Vec4 ReservedB;
    Rr_Vec4 ReservedC;
    Rr_Vec4 ReservedD;
    Rr_Mat4 ReservedE;
};

typedef struct Rr_TextPipeline Rr_TextPipeline;
struct Rr_TextPipeline
{
    VkDescriptorSetLayout DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    struct Rr_Pipeline *Pipeline;
    VkPipelineLayout Layout;
    struct Rr_Buffer *QuadBuffer;
};
