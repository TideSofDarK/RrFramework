#pragma once

#include "Rr/Rr_Math.h"
#include "Rr/Rr_Text.h"

#include <volk.h>

struct Rr_Image;
struct Rr_Buffer;

struct Rr_Font
{
    struct Rr_Image *Atlas;
    struct Rr_Buffer *Buffer;
    Rr_F32 *Advances;
    Rr_F32 LineHeight;
    Rr_F32 DefaultSize;
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
    Rr_U32 AtlasXY;
    Rr_U32 AtlasWH;
    Rr_U32 PlaneLB;
    Rr_U32 PlaneRT;
};

typedef struct Rr_TextPerInstanceVertexInput Rr_TextPerInstanceVertexInput;
struct Rr_TextPerInstanceVertexInput
{
    Rr_Vec2 Advance;
    Rr_U32 Unicode;
};

typedef struct Rr_TextGlobalsLayout Rr_TextGlobalsLayout;
struct Rr_TextGlobalsLayout
{
    Rr_F32 Time;
    Rr_F32 Reserved;
    Rr_Vec2 ScreenSize;
    Rr_Vec4 Palette[RR_TEXT_MAX_COLORS];
};

typedef struct Rr_TextFontLayout Rr_TextFontLayout;
struct Rr_TextFontLayout
{
    Rr_F32 Advance;
    Rr_F32 DistanceRange;
    Rr_Vec2 AtlasSize;
    Rr_Glyph Glyphs[RR_TEXT_MAX_GLYPHS];
};

typedef struct Rr_TextPushConstants Rr_TextPushConstants;
struct Rr_TextPushConstants
{
    Rr_Vec2 PositionScreenSpace;
    Rr_F32 Size;
    Rr_U32 Flags;
    Rr_Vec4 ReservedB;
    Rr_Vec4 ReservedC;
    Rr_Vec4 ReservedD;
    Rr_Mat4 ReservedE;
};

typedef struct Rr_TextPipeline Rr_TextPipeline;
struct Rr_TextPipeline
{
    VkDescriptorSetLayout
        DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    VkPipeline Handle;
    VkPipelineLayout Layout;
    struct Rr_Buffer *QuadBuffer;
    struct Rr_Buffer *GlobalsBuffers[RR_FRAME_OVERLAP];
    struct Rr_Buffer *TextBuffers[RR_FRAME_OVERLAP];
};
