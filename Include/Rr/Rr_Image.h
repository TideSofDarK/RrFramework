/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <Rr/Rr_Renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    RR_IMAGE_ASPECT_COLOR_BIT = (1 << 0),
    RR_IMAGE_ASPECT_DEPTH_BIT = (1 << 1),
    RR_IMAGE_ASPECT_STENCIL_BIT = (1 << 2),
} Rr_ImageAspect;

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
} Rr_ImageCubeFace;

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

extern Rr_Sampler *Rr_CreateSampler(Rr_SamplerInfo *Info);

extern void Rr_DestroySampler(Rr_Sampler *Sampler);

typedef struct Rr_Image2D Rr_Image2D;
typedef struct Rr_Image2DArray Rr_Image2DArray;
typedef struct Rr_Image3D Rr_Image3D;
typedef struct Rr_Image3DArray Rr_Image3DArray;
typedef struct Rr_ImageCube Rr_ImageCube;

typedef enum
{
    RR_IMAGE_FLAGS_STORAGE_BIT = (1 << 0),
    RR_IMAGE_FLAGS_SAMPLED_BIT = (1 << 1),
    RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT = (1 << 2),
    RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT = (1 << 3),
    RR_IMAGE_FLAGS_TRANSFER_BIT = (1 << 4),
    RR_IMAGE_FLAGS_READBACK_BIT = (1 << 5),
    RR_IMAGE_FLAGS_PER_FRAME_BIT = (1 << 6),
    RR_IMAGE_FLAGS_MIP_MAPPED_BIT = (1 << 7),
} Rr_ImageFlagsBits;
typedef uint32_t Rr_ImageFlags;

extern Rr_Image2D *Rr_CreateImage2D(
    Rr_IntVec2 Extent,
    Rr_TextureFormat Format,
    Rr_ImageFlags Flags);

extern void Rr_DestroyImage2D(Rr_Image2D *Image);

extern Rr_Image3D *Rr_CreateImage3D(
    Rr_IntVec3 Extent,
    Rr_TextureFormat Format,
    Rr_ImageFlags Flags);

extern void Rr_DestroyImage3D(Rr_Image3D *Image);

extern Rr_ImageCube *Rr_CreateCubemap(
    Rr_IntVec2 Extent,
    Rr_TextureFormat Format,
    Rr_ImageFlags Flags);

extern void Rr_DestroyCubemap(Rr_ImageCube *Cubemap);

extern Rr_IntVec2 Rr_GetImage2DExtent(Rr_Image2D *Image);

extern float Rr_GetImage2DAspect(Rr_Image2D *Image);

#ifdef __cplusplus
}
#endif
