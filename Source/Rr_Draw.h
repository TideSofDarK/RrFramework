#pragma once

#include "Rr/Rr_Draw.h"
#include "Rr/Rr_String.h"
#include "Rr_Memory.h"

#include <volk.h>

struct Rr_DrawTarget
{
    Rr_Image *ColorImage;
    Rr_Image *DepthImage;
    VkFramebuffer Framebuffer;
};

typedef struct Rr_DrawPrimitiveInfo Rr_DrawPrimitiveInfo;
struct Rr_DrawPrimitiveInfo
{
    Rr_Material *Material;
    struct Rr_Primitive *Primitive;
    uint32_t PerDrawOffset;
};

typedef struct Rr_DrawTextInfo Rr_DrawTextInfo;
struct Rr_DrawTextInfo
{
    Rr_Font *Font;
    Rr_String String;
    Rr_Vec2 Position;
    float Size;
    Rr_DrawTextFlags Flags;
};

typedef RR_SLICE_TYPE(Rr_DrawTextInfo) Rr_DrawTextsSlice;
typedef RR_SLICE_TYPE(Rr_DrawPrimitiveInfo) Rr_DrawPrimitivesSlice;

/* @TODO: Separate generic and builtin stuff! */
struct Rr_GraphPass
{
    Rr_App *App;
    Rr_GraphPassInfo Info;
    Rr_DrawTextsSlice DrawTextsSlice;
    Rr_DrawPrimitivesSlice DrawPrimitivesSlice;
    char GlobalsData[RR_PIPELINE_MAX_GLOBALS_SIZE];
    Rr_Arena *Arena;
};
