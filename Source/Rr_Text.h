#pragma once

#include "Rr/Rr_Text.h"
#include "Rr/Rr_Math.h"

#include <volk.h>

struct Rr_Image;
struct Rr_Buffer;

typedef enum Rr_TextPipelineDescriptorSet
{
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_GLOBALS,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_FONT,
    RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT,
} Rr_TextPipelineDescriptorSet;

typedef struct Rr_Glyph
{
    Rr_U32 AtlasXY;
    Rr_U32 AtlasWH;
    Rr_U32 PlaneLB;
    Rr_U32 PlaneRT;
} Rr_Glyph;

typedef struct Rr_TextPerInstanceVertexInput
{
    Rr_Vec2 Advance;
    Rr_U32 Unicode;
} Rr_TextPerInstanceVertexInput;

typedef struct Rr_TextGlobalsLayout
{
    float Time;
    float Reserved;
    Rr_Vec2 ScreenSize;
    Rr_Vec4 Palette[RR_TEXT_MAX_COLORS];
} Rr_TextGlobalsLayout;

typedef struct Rr_TextFontLayout
{
    float Advance;
    float DistanceRange;
    Rr_Vec2 AtlasSize;
    Rr_Glyph Glyphs[RR_TEXT_MAX_GLYPHS];
} Rr_TextFontLayout;

typedef struct Rr_TextPushConstants
{
    Rr_Vec2 PositionScreenSpace;
    Rr_F32 Size;
    Rr_U32 Flags;
    Rr_Vec4 ReservedB;
    Rr_Vec4 ReservedC;
    Rr_Vec4 ReservedD;
    Rr_Mat4 ReservedE;
} Rr_TextPushConstants;

struct Rr_Font
{
    struct Rr_Image* Atlas;
    struct Rr_Buffer* Buffer;
    Rr_F32* Advances;
    Rr_F32 LineHeight;
    Rr_F32 DefaultSize;
};

typedef struct Rr_TextPipeline
{
    VkDescriptorSetLayout DescriptorSetLayouts[RR_TEXT_PIPELINE_DESCRIPTOR_SET_COUNT];
    VkPipeline Handle;
    VkPipelineLayout Layout;
    struct Rr_Buffer* QuadBuffer;
    struct Rr_Buffer* GlobalsBuffers[RR_FRAME_OVERLAP];
    struct Rr_Buffer* TextBuffers[RR_FRAME_OVERLAP];
} Rr_TextPipeline;
