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

#ifndef RR_RHI_H
#define RR_RHI_H

#include <Rr/Rr_Arena.h>
#include <Rr/Rr_Math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RR_FRAME_OVERLAP          2
#define RR_MAX_COLOR_ATTACHMENTS  4
#define RR_MAX_OBJECT_NAME_LENGTH 32
#define RR_MAX_BINDINGS           16
#define RR_MAX_SETS               4

struct Rr_Graph;

/*
 * Common
 */

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
    RR_QUEUE_TYPE_MAIN,
    RR_QUEUE_TYPE_DEDICATED_TRANSFER,
    RR_QUEUE_TYPE_ASYNC_COMPUTE,
} Rr_QueueType;

extern bool RR_CC Rr_HasQueue(Rr_QueueType QueueType);

extern bool RR_CC Rr_IsIntegratedGPU(void);

extern size_t RR_CC Rr_GetMaxUniformRange(void);

extern size_t RR_CC Rr_GetUniformAlignment(void);

extern size_t RR_CC Rr_GetStorageAlignment(void);

extern size_t RR_CC Rr_GetMaxComputeSharedMemorySize(void);

extern size_t RR_CC Rr_GetMaxComputeWorkgroupInvocations(void);

/*
 * Buffer
 */

typedef struct Rr_Buffer Rr_Buffer;

typedef enum
{
    RR_BUFFER_FLAGS_UNIFORM_BIT = 1U << 0,
    RR_BUFFER_FLAGS_STORAGE_BIT = 1U << 1,
    RR_BUFFER_FLAGS_VERTEX_BIT = 1U << 2,
    RR_BUFFER_FLAGS_INDEX_BIT = 1U << 3,
    RR_BUFFER_FLAGS_INDIRECT_BIT = 1U << 4,
    RR_BUFFER_FLAGS_MAPPED_BIT = 1U << 5, /* Mapped on creation. */
    RR_BUFFER_FLAGS_PER_FRAME_BIT = 1U << 6,
    RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT = 1U << 7,
    RR_BUFFER_FLAGS_STAGING_BIT = 1U << 8,
    RR_BUFFER_FLAGS_READBACK_BIT = 1U << 9,
    RR_BUFFER_FLAGS_DYNAMIC = RR_BUFFER_FLAGS_STAGING_BIT |
                              RR_BUFFER_FLAGS_PER_FRAME_BIT |
                              RR_BUFFER_FLAGS_MAPPED_BIT,
    RR_BUFFER_FLAGS_STAGING =
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT
} Rr_BufferFlagsBits;
typedef uint32_t Rr_BufferFlags;

extern Rr_Buffer *RR_CC Rr_CreateBuffer(uint64_t Size, Rr_BufferFlags Flags);

extern size_t Rr_GetBufferSize(Rr_Buffer *Buffer);

extern void RR_CC Rr_ReleaseBuffer(Rr_Buffer *Buffer);

extern void *RR_CC Rr_GetMappedBufferData(Rr_Buffer *Buffer);

extern void *RR_CC Rr_MapBuffer(Rr_Buffer *Buffer);

extern void RR_CC Rr_UnmapBuffer(Rr_Buffer *Buffer);

extern void RR_CC
Rr_FlushBufferRange(Rr_Buffer *Buffer, uint64_t Offset, uint64_t Size);

/*
 * Image
 */

struct Rr_Image;

typedef struct Rr_Image Rr_Image2D;
typedef struct Rr_Image Rr_Image2DArray;
typedef struct Rr_Image Rr_Image3D;
typedef struct Rr_Image Rr_ImageCube;

typedef enum
{
    RR_COLOR_COMPONENT_DEFAULT = 0,
    RR_COLOR_COMPONENT_R = 1U << 0,
    RR_COLOR_COMPONENT_G = 1U << 1,
    RR_COLOR_COMPONENT_B = 1U << 2,
    RR_COLOR_COMPONENT_A = 1U << 3,
    RR_COLOR_COMPONENT_ALL = RR_COLOR_COMPONENT_R | RR_COLOR_COMPONENT_G |
                             RR_COLOR_COMPONENT_B | RR_COLOR_COMPONENT_A,
} Rr_ColorComponent;

typedef enum
{
    RR_IMAGE_FORMAT_UNDEFINED,
    /* R8 */
    RR_IMAGE_FORMAT_R8_UNORM,
    RR_IMAGE_FORMAT_R8_SNORM,
    RR_IMAGE_FORMAT_R8_UINT,
    RR_IMAGE_FORMAT_R8_SINT,
    RR_IMAGE_FORMAT_R8_SRGB,
    /* R8G8 */
    RR_IMAGE_FORMAT_R8G8_UNORM,
    RR_IMAGE_FORMAT_R8G8_SNORM,
    RR_IMAGE_FORMAT_R8G8_UINT,
    RR_IMAGE_FORMAT_R8G8_SINT,
    RR_IMAGE_FORMAT_R8G8_SRGB,
    /* R8G8B8A8 */
    RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
    RR_IMAGE_FORMAT_R8G8B8A8_SNORM,
    RR_IMAGE_FORMAT_R8G8B8A8_UINT,
    RR_IMAGE_FORMAT_R8G8B8A8_SINT,
    RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
    /* B8G8R8A8 */
    RR_IMAGE_FORMAT_B8G8R8A8_UNORM,
    RR_IMAGE_FORMAT_B8G8R8A8_SNORM,
    RR_IMAGE_FORMAT_B8G8R8A8_UINT,
    RR_IMAGE_FORMAT_B8G8R8A8_SINT,
    RR_IMAGE_FORMAT_B8G8R8A8_SRGB,
    /* R16 */
    RR_IMAGE_FORMAT_R16_UNORM,
    RR_IMAGE_FORMAT_R16_SNORM,
    RR_IMAGE_FORMAT_R16_UINT,
    RR_IMAGE_FORMAT_R16_SINT,
    RR_IMAGE_FORMAT_R16_SFLOAT,
    /* R16G16 */
    RR_IMAGE_FORMAT_R16G16_UNORM,
    RR_IMAGE_FORMAT_R16G16_SNORM,
    RR_IMAGE_FORMAT_R16G16_UINT,
    RR_IMAGE_FORMAT_R16G16_SINT,
    RR_IMAGE_FORMAT_R16G16_SFLOAT,
    /* R16G16B16A16 */
    RR_IMAGE_FORMAT_R16G16B16A16_UNORM,
    RR_IMAGE_FORMAT_R16G16B16A16_SNORM,
    RR_IMAGE_FORMAT_R16G16B16A16_UINT,
    RR_IMAGE_FORMAT_R16G16B16A16_SINT,
    RR_IMAGE_FORMAT_R16G16B16A16_SFLOAT,
    /* R32 */
    RR_IMAGE_FORMAT_R32_UINT,
    RR_IMAGE_FORMAT_R32_SINT,
    RR_IMAGE_FORMAT_R32_SFLOAT,
    /* R32G32 */
    RR_IMAGE_FORMAT_R32G32_UINT,
    RR_IMAGE_FORMAT_R32G32_SINT,
    RR_IMAGE_FORMAT_R32G32_SFLOAT,
    /* R32G32B32A32 */
    RR_IMAGE_FORMAT_R32G32B32A32_UINT,
    RR_IMAGE_FORMAT_R32G32B32A32_SINT,
    RR_IMAGE_FORMAT_R32G32B32A32_SFLOAT,
    /* */
    RR_IMAGE_FORMAT_D16_UNORM,
    RR_IMAGE_FORMAT_D32_SFLOAT,
    RR_IMAGE_FORMAT_D24_UNORM_S8_UINT,
    RR_IMAGE_FORMAT_D32_SFLOAT_S8_UINT,
} Rr_ImageFormat;

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
    RR_IMAGE_ASPECT_COLOR_BIT = 1U << 0,
    RR_IMAGE_ASPECT_DEPTH_BIT = 1U << 1,
    RR_IMAGE_ASPECT_STENCIL_BIT = 1U << 2,
} Rr_ImageAspect;

typedef enum
{
    RR_IMAGE_FLAGS_STORAGE_BIT = 1U << 0,
    RR_IMAGE_FLAGS_SAMPLED_BIT = 1U << 1,
    RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT = 1U << 2,
    RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT = 1U << 3,
    RR_IMAGE_FLAGS_TRANSFER_BIT = 1U << 4,
    RR_IMAGE_FLAGS_MIP_MAPPED_BIT = 1U << 5,
    RR_IMAGE_FLAGS_SAMPLE_COUNT_1 = 1U << 9,
    RR_IMAGE_FLAGS_SAMPLE_COUNT_2 = 1U << 10,
    RR_IMAGE_FLAGS_SAMPLE_COUNT_4 = 1U << 11,
    RR_IMAGE_FLAGS_SAMPLE_COUNT_8 = 1U << 12,
    RR_IMAGE_FLAGS_SAMPLE_COUNT_16 = 1U << 13,
} Rr_ImageFlagsBits;
typedef uint32_t Rr_ImageFlags;

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

extern uint32_t RR_CC Rr_GetImageLevelCount(struct Rr_Image *Image);

extern Rr_ImageFormat RR_CC Rr_GetImageFormat(struct Rr_Image *Image);

extern Rr_IntVec2 RR_CC Rr_GetImage2DExtent(Rr_Image2D *Image);

extern float RR_CC Rr_GetImage2DAspect(Rr_Image2D *Image);

extern Rr_IntVec3 RR_CC Rr_GetImageExtent(struct Rr_Image *Image);

extern bool RR_CC Rr_IsDepthStencilFormat(Rr_ImageFormat Format);

extern bool RR_CC Rr_IsDepthFormat(Rr_ImageFormat Format);

extern bool RR_CC Rr_IsSRGBFormat(Rr_ImageFormat Format);

extern char const *RR_CC Rr_GetImageFormatString(Rr_ImageFormat Format);

extern char const *const *RR_CC Rr_GetImageFormatStrings(void);

/*
 * Sampler
 */

typedef struct Rr_Sampler Rr_Sampler;

typedef enum
{
    RR_FILTER_NEAREST,
    RR_FILTER_LINEAR,
} Rr_Filter;

typedef enum
{
    RR_SAMPLER_MIPMAP_MODE_NEAREST,
    RR_SAMPLER_MIPMAP_MODE_LINEAR,
} Rr_SamplerMipmapMode;

typedef enum
{
    RR_SAMPLER_ADDRESS_MODE_REPEAT,
    RR_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    RR_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
} Rr_SamplerAddressMode;

typedef enum
{
    RR_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    RR_BORDER_COLOR_INT_TRANSPARENT_BLACK,
    RR_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    RR_BORDER_COLOR_INT_OPAQUE_BLACK,
    RR_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    RR_BORDER_COLOR_INT_OPAQUE_WHITE,
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
    Rr_BorderColor BorderColor;
    bool UnnormalizedCoordinates;
};

extern Rr_Sampler *RR_CC Rr_CreateSampler(Rr_SamplerInfo const *Info);

extern void RR_CC Rr_ReleaseSampler(Rr_Sampler *Sampler);

/*
 * Pipelines
 */

typedef struct Rr_PipelineLayout Rr_PipelineLayout;

typedef struct Rr_ComputePipeline Rr_ComputePipeline;

typedef struct Rr_GraphicsPipeline Rr_GraphicsPipeline;

typedef enum
{
    RR_FORMAT_UNDEFINED,
    RR_FORMAT_INT,
    RR_FORMAT_INT2,
    RR_FORMAT_INT3,
    RR_FORMAT_INT4,
    RR_FORMAT_UINT,
    RR_FORMAT_UINT2,
    RR_FORMAT_UINT3,
    RR_FORMAT_UINT4,
    RR_FORMAT_FLOAT,
    RR_FORMAT_FLOAT2,
    RR_FORMAT_FLOAT3,
    RR_FORMAT_FLOAT4,
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
    RR_SHADER_STAGE_VERTEX_BIT = 1U << 0,
    RR_SHADER_STAGE_FRAGMENT_BIT = 1U << 1,
    RR_SHADER_STAGE_COMPUTE_BIT = 1U << 2,
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

typedef enum
{
    RR_STENCIL_OP_KEEP,
    RR_STENCIL_OP_ZERO,
    RR_STENCIL_OP_REPLACE,
    RR_STENCIL_OP_INCREMENT_AND_CLAMP,
    RR_STENCIL_OP_DECREMENT_AND_CLAMP,
    RR_STENCIL_OP_INVERT,
    RR_STENCIL_OP_INCREMENT_AND_WRAP,
    RR_STENCIL_OP_DECREMENT_AND_WRAP,
} Rr_StencilOp;

typedef struct Rr_StencilOpState Rr_StencilOpState;
struct Rr_StencilOpState
{
    Rr_StencilOp FailOp;
    Rr_StencilOp PassOp;
    Rr_StencilOp DepthFailOp;
    Rr_CompareOp CompareOp;
};

typedef enum
{
    RR_POLYGON_MODE_FILL,
    RR_POLYGON_MODE_LINE
} Rr_PolygonMode;

typedef enum
{
    RR_CULL_MODE_NONE,
    RR_CULL_MODE_FRONT,
    RR_CULL_MODE_BACK
} Rr_CullMode;

typedef enum
{
    RR_FRONT_FACE_COUNTER_CLOCKWISE,
    RR_FRONT_FACE_CLOCKWISE
} Rr_FrontFace;

typedef enum
{
    RR_TOPOLOGY_TRIANGLE_LIST,
    RR_TOPOLOGY_TRIANGLE_STRIP,
    RR_TOPOLOGY_LINE_LIST,
    RR_TOPOLOGY_LINE_STRIP,
    RR_TOPOLOGY_POINT_LIST,
} Rr_Topology;

typedef enum
{
    RR_VERTEX_INPUT_RATE_VERTEX,
    RR_VERTEX_INPUT_RATE_INSTANCE,
} Rr_VertexInputRate;

typedef struct Rr_VertexInputAttribute Rr_VertexInputAttribute;
struct Rr_VertexInputAttribute
{
    uint32_t Location;
    Rr_Format Format;
};

typedef struct Rr_VertexInputBinding Rr_VertexInputBinding;
struct Rr_VertexInputBinding
{
    Rr_VertexInputRate Rate;
    size_t AttributeCount;
    Rr_VertexInputAttribute const *Attributes;
};

typedef enum
{
    RR_BLEND_FACTOR_ZERO,
    RR_BLEND_FACTOR_ONE,
    RR_BLEND_FACTOR_SRC_COLOR,
    RR_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    RR_BLEND_FACTOR_DST_COLOR,
    RR_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    RR_BLEND_FACTOR_SRC_ALPHA,
    RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    RR_BLEND_FACTOR_DST_ALPHA,
    RR_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    RR_BLEND_FACTOR_CONSTANT_COLOR,
    RR_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    RR_BLEND_FACTOR_SRC_ALPHA_SATURATE,
} Rr_BlendFactor;

typedef enum
{
    RR_BLEND_OP_ADD,
    RR_BLEND_OP_SUBTRACT,
    RR_BLEND_OP_REVERSE_SUBTRACT,
    RR_BLEND_OP_MIN,
    RR_BLEND_OP_MAX,
} Rr_BlendOp;

typedef struct Rr_ColorTargetBlend Rr_ColorTargetBlend;
struct Rr_ColorTargetBlend
{
    bool BlendEnable;
    Rr_BlendFactor SrcColorBlendFactor;
    Rr_BlendFactor DstColorBlendFactor;
    Rr_BlendOp ColorBlendOp;
    Rr_BlendFactor SrcAlphaBlendFactor;
    Rr_BlendFactor DstAlphaBlendFactor;
    Rr_BlendOp AlphaBlendOp;
    Rr_ColorComponent ColorWriteMask;
};

typedef struct Rr_ColorTargetInfo Rr_ColorTargetInfo;
struct Rr_ColorTargetInfo
{
    Rr_ColorTargetBlend Blend;
    Rr_ImageFormat Format;
    bool Resolve;
};

typedef struct Rr_Rasterizer Rr_Rasterizer;
struct Rr_Rasterizer
{
    Rr_PolygonMode PolygonMode;
    Rr_CullMode CullMode;
    Rr_FrontFace FrontFace;
    float DepthBiasConstantFactor;
    float DepthBiasClamp;
    float DepthBiasSlopeFactor;
    bool EnableDepthBias;
};

typedef struct Rr_DepthStencil Rr_DepthStencil;
struct Rr_DepthStencil
{
    Rr_ImageFormat Format;
    Rr_CompareOp CompareOp;
    Rr_StencilOpState BackStencilState;
    Rr_StencilOpState FrontStencilState;
    uint8_t CompareMask;
    uint8_t WriteMask;
    bool EnableDepthTest;
    bool EnableDepthWrite;
    bool EnableStencilTest;
};

typedef struct Rr_PipelineSpecialization Rr_PipelineSpecialization;
struct Rr_PipelineSpecialization
{
    uint32_t ConstantID;
    uint32_t Size;
    void const *Data;
};

typedef struct Rr_Multisampling Rr_Multisampling;
struct Rr_Multisampling
{
    uint32_t SampleCount;
};

typedef struct Rr_ShaderInfo Rr_ShaderInfo;
struct Rr_ShaderInfo
{
    size_t SPVSize;
    void const *SPVData;
    char const *EntryPoint;
    size_t SpecializationCount;
    Rr_PipelineSpecialization const *Specializations;
};

typedef struct Rr_GraphicsPipelineCreateInfo Rr_GraphicsPipelineCreateInfo;
struct Rr_GraphicsPipelineCreateInfo
{
    Rr_ShaderInfo const *VertexShaderInfo;
    Rr_ShaderInfo const *FragmentShaderInfo;
    size_t VertexInputBindingCount;
    Rr_VertexInputBinding const *VertexInputBindings;
    Rr_Topology Topology;
    size_t ColorTargetCount;
    Rr_ColorTargetInfo const *ColorTargets;
    Rr_Multisampling Multisampling;
    Rr_Rasterizer Rasterizer;
    Rr_DepthStencil DepthStencil;
};

typedef enum
{
    RR_BINDING_TYPE_INVALID,
    RR_BINDING_TYPE_SAMPLER,
    RR_BINDING_TYPE_SAMPLED_IMAGE,
    RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
    RR_BINDING_TYPE_UNIFORM_BUFFER,
    RR_BINDING_TYPE_STORAGE_BUFFER,
    RR_BINDING_TYPE_STORAGE_IMAGE,
} Rr_BindingType;

typedef enum
{
    RR_BINDING_FLAGS_NON_WRITABLE_BIT = 1U << 0,
    RR_BINDING_FLAGS_NON_READABLE_BIT = 1U << 1,
} Rr_BindingFlagsBits;
typedef uint32_t Rr_BindingFlags;

typedef struct Rr_Binding Rr_Binding;
struct Rr_Binding
{
    uint32_t Index;
    Rr_BindingType Type;
    Rr_ShaderStage Stages;
    uint32_t Count;
    Rr_ImageFormat ImageFormat;
    Rr_BindingFlags Flags;
};

typedef struct Rr_BindingSet Rr_BindingSet;
struct Rr_BindingSet
{
    size_t BindingCount;
    Rr_Binding *Bindings;
};

extern Rr_ComputePipeline *RR_CC
Rr_CreateComputePipeline(Rr_ShaderInfo const *ShaderInfo);

extern Rr_ComputePipeline *RR_CC Rr_CreateComputePipelineWithLayout(
    Rr_ShaderInfo const *ShaderInfo,
    Rr_PipelineLayout *PipelineLayout);

extern void RR_CC
Rr_ReleaseComputePipeline(Rr_ComputePipeline *ComputePipeline);

extern Rr_GraphicsPipeline *RR_CC
Rr_CreateGraphicsPipeline(Rr_GraphicsPipelineCreateInfo const *CreateInfo);

extern Rr_GraphicsPipeline *RR_CC Rr_CreateGraphicsPipelineWithLayout(
    Rr_GraphicsPipelineCreateInfo const *CreateInfo,
    Rr_PipelineLayout *PipelineLayout);

extern void RR_CC
Rr_ReleaseGraphicsPipeline(Rr_GraphicsPipeline *GraphicsPipeline);

extern Rr_ColorTargetBlend RR_CC Rr_AlphaBlend(void);

extern Rr_ColorTargetBlend RR_CC Rr_PremultipliedAlphaBlend(void);

/*
 * Swapchain and Presentation
 */

typedef enum
{
    RR_PRESENT_MODE_UNDEFINED,
    RR_PRESENT_MODE_FIFO,
    RR_PRESENT_MODE_FIFO_RELAXED,
    RR_PRESENT_MODE_IMMEDIATE,
    RR_PRESENT_MODE_MAILBOX,
} Rr_PresentMode;

extern struct Rr_Image *RR_CC Rr_GetSwapchainImage(void);

extern void RR_CC
Rr_GetAvailableSwapchainFormats(uint32_t *Count, Rr_ImageFormat *OutFormats);

extern void RR_CC Rr_SetSwapchainFormat(Rr_ImageFormat Format);

extern void RR_CC
Rr_GetAvailablePresentModes(uint32_t *Count, Rr_PresentMode *OutPresentModes);

extern Rr_PresentMode RR_CC Rr_GetPresentMode(void);

extern char const *RR_CC Rr_GetPresentModeString(Rr_PresentMode PresentMode);

extern char const *const *RR_CC Rr_GetPresentModeStrings(void);

extern void RR_CC Rr_SetPresentMode(Rr_PresentMode PresentMode);

/*
 * Debug
 */

extern void RR_CC Rr_SetNextObjectName(char const *Name);

extern void RR_CC Rr_SetNextObjectNameF(char const *Format, ...);

#ifdef __cplusplus
}
#endif

#endif
