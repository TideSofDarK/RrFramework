#pragma once

#include <Rr/Rr_Renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_Sampler Rr_Sampler;

typedef enum
{
    RR_FILTER_NEAREST = 0,
    RR_FILTER_LINEAR = 1,
} Rr_Filter;

typedef enum
{
    RR_SAMPLER_MIPMAP_MODE_NEAREST = 0,
    RR_SAMPLER_MIPMAP_MODE_LINEAR = 1,
} Rr_SamplerMipmapMode;

typedef enum
{
    RR_SAMPLER_ADDRESS_MODE_REPEAT = 0,
    RR_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT = 1,
    RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE = 2,
    RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER = 3,
    RR_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE = 4,
} Rr_SamplerAddressMode;

typedef enum
{
    RR_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK = 0,
    RR_BORDER_COLOR_INT_TRANSPARENT_BLACK = 1,
    RR_BORDER_COLOR_FLOAT_OPAQUE_BLACK = 2,
    RR_BORDER_COLOR_INT_OPAQUE_BLACK = 3,
    RR_BORDER_COLOR_FLOAT_OPAQUE_WHITE = 4,
    RR_BORDER_COLOR_INT_OPAQUE_WHITE = 5,
} Rr_BorderColor;

typedef struct Rr_SamplerInfo Rr_SamplerInfo;
struct Rr_SamplerInfo
{
    Rr_Filter MagFilter;
    Rr_Filter MinFilter;
    Rr_SamplerMipmapMode MipmapMode;
    Rr_SamplerAddressMode AddressModeU;
    Rr_SamplerAddressMode AddressModeV;
    Rr_SamplerAddressMode AddressModeW;
    float MipLodBias;
    bool AnisotropyEnable;
    float MaxAnisotropy;
    bool CompareEnable;
    Rr_CompareOp CompareOp;
    float MinLod;
    float MaxLod;
    Rr_BorderColor BorderColor;
    bool UnnormalizedCoordinates;
};

extern Rr_Sampler *Rr_CreateSampler(Rr_App *App, Rr_SamplerInfo *Info);

extern void Rr_DestroySampler(Rr_App *App, Rr_Sampler *Sampler);

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
