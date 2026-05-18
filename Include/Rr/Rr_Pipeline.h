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

#ifndef RR_PIPELINE_H
#define RR_PIPELINE_H

#include <Rr/Rr_App.h>
#include <Rr/Rr_Renderer.h>

#define RR_MAX_BINDINGS 16
#define RR_MAX_SETS     4

typedef struct Rr_PipelineLayout Rr_PipelineLayout;
typedef struct Rr_ComputePipeline Rr_ComputePipeline;
typedef struct Rr_GraphicsPipeline Rr_GraphicsPipeline;

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

typedef struct Rr_Binding Rr_Binding;
struct Rr_Binding
{
    uint32_t Index;
    Rr_BindingType Type;
    Rr_ShaderStage Stages;
    uint32_t Count;
    Rr_ImageFormat ImageFormat;
};

typedef struct Rr_BindingSet Rr_BindingSet;
struct Rr_BindingSet
{
    size_t BindingCount;
    Rr_Binding *Bindings;
};

/* RR_EXTERN Rr_PipelineLayout *Rr_CreatePipelineLayout( */
/*     size_t BindingSetCount, */
/*     Rr_BindingSet const *BindingSets); */

/* RR_EXTERN void Rr_ReleasePipelineLayout(Rr_PipelineLayout *PipelineLayout);
 */

RR_EXTERN Rr_ComputePipeline *Rr_CreateComputePipeline(
    Rr_ShaderInfo const *ShaderInfo);

RR_EXTERN Rr_ComputePipeline *Rr_CreateComputePipelineWithLayout(
    Rr_ShaderInfo const *ShaderInfo,
    Rr_PipelineLayout *PipelineLayout);

RR_EXTERN void Rr_ReleaseComputePipeline(Rr_ComputePipeline *ComputePipeline);

RR_EXTERN Rr_ColorTargetBlend Rr_AlphaBlend(void);

RR_EXTERN Rr_GraphicsPipeline *Rr_CreateGraphicsPipeline(
    Rr_GraphicsPipelineCreateInfo const *CreateInfo);

RR_EXTERN Rr_GraphicsPipeline *Rr_CreateGraphicsPipelineWithLayout(
    Rr_GraphicsPipelineCreateInfo const *CreateInfo,
    Rr_PipelineLayout *PipelineLayout);

RR_EXTERN void Rr_ReleaseGraphicsPipeline(
    Rr_GraphicsPipeline *GraphicsPipeline);

#endif
