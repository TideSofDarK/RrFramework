/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef RR_IMAGE_H
#define RR_IMAGE_H

#include <Rr/Rr_Renderer.h>

typedef struct Rr_Sampler Rr_Sampler;
typedef struct Rr_Image Rr_Image2D;
typedef struct Rr_Image Rr_Image2DArray;
typedef struct Rr_Image Rr_Image3D;
typedef struct Rr_Image Rr_ImageCube;

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

typedef enum Rr_ImageCubeFace
{
    RR_IMAGE_CUBE_FACE_FIRST,
    RR_IMAGE_CUBE_FACE_FRONT = RR_IMAGE_CUBE_FACE_FIRST,
    RR_IMAGE_CUBE_FACE_BACK,
    RR_IMAGE_CUBE_FACE_UP,
    RR_IMAGE_CUBE_FACE_DOWN,
    RR_IMAGE_CUBE_FACE_RIGHT,
    RR_IMAGE_CUBE_FACE_LEFT,
    RR_IMAGE_CUBE_FACE_LAST = RR_IMAGE_CUBE_FACE_LEFT,
    RR_IMAGE_CUBE_FACE_COUNT,
} Rr_ImageCubeFace;

typedef enum
{
    RR_IMAGE_ASPECT_COLOR_BIT = (1 << 0),
    RR_IMAGE_ASPECT_DEPTH_BIT = (1 << 1),
    RR_IMAGE_ASPECT_STENCIL_BIT = (1 << 2),
} Rr_ImageAspect;

typedef enum
{
    RR_IMAGE_FLAGS_STORAGE_BIT = (1 << 0),
    RR_IMAGE_FLAGS_SAMPLED_BIT = (1 << 1),
    RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT = (1 << 2),
    RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT = (1 << 3),
    RR_IMAGE_FLAGS_TRANSFER_BIT = (1 << 4),
    RR_IMAGE_FLAGS_PER_FRAME_BIT = (1 << 5),
    RR_IMAGE_FLAGS_MIP_MAPPED_BIT = (1 << 6),
    RR_IMAGE_FLAGS_MUTABLE_FORMAT_BIT = (1 << 7),
    RR_IMAGE_FLAGS_SAMPLE_COUNT_1 = (1 << 9),
    RR_IMAGE_FLAGS_SAMPLE_COUNT_2 = (1 << 10),
    RR_IMAGE_FLAGS_SAMPLE_COUNT_4 = (1 << 11),
    RR_IMAGE_FLAGS_SAMPLE_COUNT_8 = (1 << 12),
    RR_IMAGE_FLAGS_SAMPLE_COUNT_16 = (1 << 13),
} Rr_ImageFlagsBits;
typedef uint32_t Rr_ImageFlags;

#ifdef __cplusplus
extern "C" {
#endif

extern Rr_Sampler *RR_CC Rr_CreateSampler(Rr_SamplerInfo *Info);

extern void RR_CC Rr_ReleaseSampler(Rr_Sampler *Sampler);

extern Rr_Image2D *RR_CC
Rr_CreateImage2D(Rr_IntVec2 Extent, Rr_ImageFormat Format, Rr_ImageFlags Flags);

extern Rr_Image2DArray *RR_CC Rr_CreateImage2DArray(
    Rr_IntVec2 Extent,
    uint32_t ArrayCount,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags);

extern Rr_Image3D *RR_CC
Rr_CreateImage3D(Rr_IntVec3 Extent, Rr_ImageFormat Format, Rr_ImageFlags Flags);

extern Rr_ImageCube *RR_CC Rr_CreateImageCube(
    Rr_IntVec2 Extent,
    Rr_ImageFormat Format,
    Rr_ImageFlags Flags);

extern void RR_CC Rr_ReleaseImage(struct Rr_Image *Image);

extern Rr_ImageFormat RR_CC Rr_GetImageFormat(struct Rr_Image *Image);

extern Rr_IntVec2 RR_CC Rr_GetImage2DExtent(Rr_Image2D *Image);

extern float RR_CC Rr_GetImage2DAspect(Rr_Image2D *Image);

extern Rr_IntVec3 RR_CC Rr_GetImage3DExtent(Rr_Image3D *Image3D);

#ifdef __cplusplus
}
#endif

#endif
