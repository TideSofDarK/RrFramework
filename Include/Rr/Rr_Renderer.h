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

#ifndef RR_RENDERER_H
#define RR_RENDERER_H

#include <Rr/Rr_App.h>

struct Rr_Graph;
struct Rr_Image;

#define RR_FRAME_OVERLAP          2
#define RR_MAX_COLOR_ATTACHMENTS  4
#define RR_MAX_OBJECT_NAME_LENGTH 32

typedef struct Rr_Renderer Rr_Renderer;

typedef enum
{
    RR_PRESENT_MODE_FIFO,
    RR_PRESENT_MODE_FIFO_RELAXED,
    RR_PRESENT_MODE_IMMEDIATE,
    RR_PRESENT_MODE_MAILBOX,
} Rr_PresentMode;

static const char *RR_PRESENT_MODES[] = {
    "FIFO",
    "FIFO_RELAXED",
    "IMMEDIATE",
    "MAILBOX",
};

typedef enum
{
    RR_FORMAT_UNDEFINED,
    RR_FORMAT_FLOAT,
    RR_FORMAT_UINT,
    RR_FORMAT_VEC2,
    RR_FORMAT_VEC3,
    RR_FORMAT_VEC4,
} Rr_Format;

typedef enum
{
    RR_INDEX_TYPE_INVALID,
    RR_INDEX_TYPE_UINT8,
    RR_INDEX_TYPE_UINT16,
    RR_INDEX_TYPE_UINT32,
} Rr_IndexType;

typedef enum
{
    RR_LOAD_OP_LOAD,
    RR_LOAD_OP_CLEAR,
    RR_LOAD_OP_DONT_CARE,
} Rr_LoadOp;

typedef enum
{
    RR_STORE_OP_STORE,
    RR_STORE_OP_DONT_CARE,
} Rr_StoreOp;

typedef enum
{
    RR_COMPARE_OP_NEVER,
    RR_COMPARE_OP_LESS,
    RR_COMPARE_OP_EQUAL,
    RR_COMPARE_OP_LESS_OR_EQUAL,
    RR_COMPARE_OP_GREATER,
    RR_COMPARE_OP_NOT_EQUAL,
    RR_COMPARE_OP_GREATER_OR_EQUAL,
    RR_COMPARE_OP_ALWAYS,
} Rr_CompareOp;

typedef enum
{
    RR_COLOR_COMPONENT_DEFAULT = 0,
    RR_COLOR_COMPONENT_R = (1 << 0),
    RR_COLOR_COMPONENT_G = (1 << 1),
    RR_COLOR_COMPONENT_B = (1 << 2),
    RR_COLOR_COMPONENT_A = (1 << 3),
    RR_COLOR_COMPONENT_ALL = RR_COLOR_COMPONENT_R | RR_COLOR_COMPONENT_G |
                             RR_COLOR_COMPONENT_B | RR_COLOR_COMPONENT_A,
} Rr_ColorComponent;

typedef enum
{
    RR_IMAGE_FORMAT_UNDEFINED,
    RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
    RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
    RR_IMAGE_FORMAT_B8G8R8A8_UNORM,
    RR_IMAGE_FORMAT_B8G8R8A8_SRGB,
    RR_IMAGE_FORMAT_A8B8G8R8_UNORM_PACK32,
    RR_IMAGE_FORMAT_A8B8G8R8_SRGB_PACK32,
    RR_IMAGE_FORMAT_D16_UNORM,
    RR_IMAGE_FORMAT_D32_SFLOAT,
    RR_IMAGE_FORMAT_D24_UNORM_S8_UINT,
    RR_IMAGE_FORMAT_D32_SFLOAT_S8_UINT,
    RR_IMAGE_FORMAT_R8G8B8A8_UINT,
    RR_IMAGE_FORMAT_R8G8B8A8_SINT,
    RR_IMAGE_FORMAT_R32_UINT,
    RR_IMAGE_FORMAT_R32_SINT,
    RR_IMAGE_FORMAT_R32_SFLOAT,
    RR_IMAGE_FORMAT_R32G32_SFLOAT,
    RR_IMAGE_FORMAT_R32G32B32A32_SFLOAT,
} Rr_ImageFormat;

typedef enum
{
    RR_SHADER_STAGE_VERTEX_BIT = (1 << 0),
    RR_SHADER_STAGE_FRAGMENT_BIT = (1 << 1),
    RR_SHADER_STAGE_COMPUTE_BIT = (1 << 2),
} Rr_ShaderStageBits;
typedef uint32_t Rr_ShaderStage;

typedef union Rr_ColorClear Rr_ColorClear;
union Rr_ColorClear
{
    Rr_Vec4 Vec4;
    Rr_IntVec4 IntVec4;
};

typedef struct Rr_ColorTarget Rr_ColorTarget;
struct Rr_ColorTarget
{
    struct Rr_Image *Image;
    uint32_t ImageLayerIndex;
    Rr_LoadOp LoadOp;
    Rr_StoreOp StoreOp;
    Rr_ColorClear Clear;
    struct Rr_Image *ResolveImage;
    uint32_t ResolveImageLayerIndex;
    Rr_LoadOp ResolveLoadOp;
    Rr_StoreOp ResolveStoreOp;
    Rr_ColorClear ResolveClear;
};

typedef struct Rr_DepthClear Rr_DepthClear;
struct Rr_DepthClear
{
    float Depth;
    uint32_t Stencil;
};

typedef struct Rr_DepthTarget Rr_DepthTarget;
struct Rr_DepthTarget
{
    struct Rr_Image *Image;
    uint32_t ImageLayerIndex;
    Rr_LoadOp LoadOp;
    Rr_StoreOp StoreOp;
    Rr_DepthClear Clear;
};

typedef struct Rr_DrawIndirectCommand Rr_DrawIndirectCommand;
struct Rr_DrawIndirectCommand
{
    uint32_t VertexCount;
    uint32_t InstanceCount;
    uint32_t FirstVertex;
    uint32_t FirstInstance;
};

typedef struct Rr_DrawIndexedIndirectCommand Rr_DrawIndexedIndirectCommand;
struct Rr_DrawIndexedIndirectCommand
{
    uint32_t IndexCount;
    uint32_t InstanceCount;
    uint32_t FirstIndex;
    int32_t VertexOffset;
    uint32_t FirstInstance;
};

RR_EXTERN struct Rr_Image *Rr_GetSwapchainImage(void);

RR_EXTERN Rr_PresentMode *Rr_GetAvailablePresentModes(uint32_t *Count);

RR_EXTERN Rr_PresentMode Rr_GetPresentMode(void);

RR_EXTERN const char *Rr_GetPresentModeString(Rr_PresentMode PresentMode);

RR_EXTERN bool Rr_SetPresentMode(Rr_PresentMode PresentMode);

RR_EXTERN size_t Rr_GetUniformAlignment(void);

RR_EXTERN size_t Rr_GetStorageAlignment(void);

RR_EXTERN size_t Rr_GetMaxComputeSharedMemorySize(void);

RR_EXTERN size_t Rr_GetMaxComputeWorkgroupInvocations(void);

RR_EXTERN bool Rr_IsSRGBFormat(Rr_ImageFormat Format);

RR_EXTERN void Rr_SetNextObjectName(const char *Name);

RR_EXTERN void Rr_SetNextObjectNameF(const char *Format, ...);

#endif
