#pragma once

#include <Rr/Rr_App.h>
#include <Rr/Rr_Memory.h>
#include <Rr/Rr_Renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_PipelineLayout Rr_PipelineLayout;
typedef struct Rr_GraphicsPipeline Rr_GraphicsPipeline;

typedef enum
{
    RR_STENCIL_OP_INVALID,
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
    Rr_VertexInputAttribute *Attributes;
};

typedef enum
{
    RR_BLEND_FACTOR_INVALID,
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
    RR_BLEND_OP_INVALID,
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
    Rr_TextureFormat Format;
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
    bool EnableDepthClip;
};

typedef struct Rr_DepthStencil Rr_DepthStencil;
struct Rr_DepthStencil
{
    Rr_CompareOp CompareOp;
    Rr_StencilOpState BackStencilState;
    Rr_StencilOpState FrontStencilState;
    uint8_t CompareMask;
    uint8_t WriteMask;
    bool EnableDepthTest;
    bool EnableDepthWrite;
    bool EnableStencilTest;
};

typedef struct Rr_PipelineInfo Rr_PipelineInfo;
struct Rr_PipelineInfo
{
    Rr_Data VertexShaderSPV;
    Rr_Data FragmentShaderSPV;
    size_t VertexInputBindingCount;
    Rr_VertexInputBinding *VertexInputBindings;
    Rr_Topology Topology;
    size_t ColorTargetCount;
    Rr_ColorTargetInfo *ColorTargets;
    Rr_Rasterizer Rasterizer;
    Rr_DepthStencil DepthStencil;
    Rr_PipelineLayout *Layout;
};

typedef enum
{
    RR_PIPELINE_BINDING_TYPE_INVALID,
    RR_PIPELINE_BINDING_TYPE_SAMPLER,
    RR_PIPELINE_BINDING_TYPE_SAMPLED_IMAGE,
    RR_PIPELINE_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
    RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
    RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER,
} Rr_PipelineBindingType;

typedef struct Rr_PipelineBinding Rr_PipelineBinding;
struct Rr_PipelineBinding
{
    uint32_t Binding;
    uint32_t Count;
    Rr_PipelineBindingType Type;
};

typedef struct Rr_PipelineBindingSet Rr_PipelineBindingSet;
struct Rr_PipelineBindingSet
{
    size_t BindingCount;
    Rr_PipelineBinding *Bindings;
    Rr_ShaderStage Stages;
};

extern Rr_PipelineLayout *Rr_CreatePipelineLayout(
    Rr_Renderer *Renderer,
    size_t SetCount,
    Rr_PipelineBindingSet *Sets);

extern void Rr_DestroyPipelineLayout(
    Rr_Renderer *Renderer,
    Rr_PipelineLayout *PipelineLayout);

extern Rr_GraphicsPipeline *Rr_CreateGraphicsPipeline(
    Rr_Renderer *Renderer,
    Rr_PipelineInfo *Info);

extern void Rr_DestroyGraphicsPipeline(
    Rr_Renderer *Renderer,
    Rr_GraphicsPipeline *GraphicsPipelin);

#ifdef __cplusplus
}
#endif
