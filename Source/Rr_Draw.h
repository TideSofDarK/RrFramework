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
    Rr_U32 OffsetIntoDrawBuffer;
};

typedef struct Rr_DrawTextInfo Rr_DrawTextInfo;
struct Rr_DrawTextInfo
{
    Rr_Font *Font;
    Rr_String String;
    Rr_Vec2 Position;
    Rr_F32 Size;
    Rr_DrawTextFlags Flags;
};

typedef Rr_SliceType(Rr_DrawTextInfo) Rr_DrawTextsSlice;
typedef Rr_SliceType(Rr_DrawPrimitiveInfo) Rr_DrawPrimitivesSlice;

/* @TODO: Separate generic and builtin stuff! */
struct Rr_DrawContext
{
    Rr_App *App;
    Rr_DrawContextInfo Info;
    Rr_DrawTextsSlice DrawTextsSlice;
    Rr_DrawPrimitivesSlice DrawPrimitivesSlice;
    Rr_Byte GlobalsData[RR_PIPELINE_MAX_GLOBALS_SIZE];
    Rr_Arena *Arena;
};
