#pragma once

#include <Rr/Rr_Renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Image Rr_Image;

typedef enum
{
    RR_IMAGE_USAGE_STORAGE = (1 << 0),
    RR_IMAGE_USAGE_SAMPLED = (1 << 1),
    RR_IMAGE_USAGE_COLOR_ATTACHMENT = (1 << 2),
    RR_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT = (1 << 3),
    RR_IMAGE_USAGE_TRANSFER = (1 << 4),
} Rr_ImageUsageBits;
typedef uint32_t Rr_ImageUsage;

extern Rr_Image *Rr_CreateImage(
    Rr_App *App,
    Rr_IntVec3 Extent,
    Rr_TextureFormat Format,
    Rr_ImageUsage Usage,
    bool MipMapped);

extern void Rr_DestroyImage(Rr_App *App, Rr_Image *Image);

extern Rr_Image *Rr_GetDummyColorTexture(Rr_App *App);

extern Rr_Image *Rr_GetDummyNormalTexture(Rr_App *App);

#ifdef __cplusplus
}
#endif
