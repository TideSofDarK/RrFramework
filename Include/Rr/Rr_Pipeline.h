#pragma once

#include "Rr_App.h"

#include <Rr/Rr_Memory.h>
#include <Rr/Rr_Renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_PipelineLayout Rr_PipelineLayout;
typedef struct Rr_GraphicsPipeline Rr_GraphicsPipeline;

typedef enum
{
    RR_COMPARE_OP_INVALID,
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
    RR_FORMAT_NONE,
    RR_FORMAT_FLOAT,
    RR_FORMAT_UINT,
    RR_FORMAT_VEC2,
    RR_FORMAT_VEC3,
    RR_FORMAT_VEC4,
} Rr_Format;

typedef enum
{
    RR_VERTEX_INPUT_TYPE_VERTEX,
    RR_VERTEX_INPUT_TYPE_INSTANCE,
} Rr_VertexInputType;

typedef struct Rr_VertexInputAttribute Rr_VertexInputAttribute;
struct Rr_VertexInputAttribute
{
    Rr_Format Format;
    Rr_VertexInputType Type;
    uint32_t Location;
};

typedef uint8_t Rr_ColorComponentFlags;

#define RR_COLOR_COMPONENT_R (1u << 0)
#define RR_COLOR_COMPONENT_G (1u << 1)
#define RR_COLOR_COMPONENT_B (1u << 2)
#define RR_COLOR_COMPONENT_A (1u << 3)

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

typedef enum
{
    RR_TEXTURE_FORMAT_TODO,
} Rr_TextureFormat;

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
    Rr_ColorComponentFlags ColorWriteMask;
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
    Rr_VertexInputAttribute *VertexAttributes;
    size_t VertexAttributeCount;
    Rr_Topology Topology;
    Rr_ColorTargetInfo *ColorTargets;
    size_t ColorTargetCount;
    Rr_Rasterizer Rasterizer;
    Rr_DepthStencil DepthStencil;
    Rr_PipelineLayout *Layout;
};

typedef enum
{
    RR_PIPELINE_BINDING_TYPE_COMBINED_SAMPLER,
    RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
    RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER,
} Rr_PipelineBindingType;

typedef struct Rr_PipelineBinding Rr_PipelineBinding;
struct Rr_PipelineBinding
{
    uint32_t Slot;
    uint32_t Count;
    Rr_PipelineBindingType Type;
};

typedef struct Rr_PipelineBindingSet Rr_PipelineBindingSet;
struct Rr_PipelineBindingSet
{
    Rr_PipelineBinding *Bindings;
    size_t BindingCount;
    Rr_ShaderStage Visibility;
};

extern Rr_PipelineLayout *Rr_CreatePipelineLayout(Rr_App *App, Rr_PipelineBindingSet *Sets, size_t SetCount);

extern void Rr_DestroyPipelineLayout(Rr_App *App, Rr_PipelineLayout *PipelineLayout);

extern Rr_GraphicsPipeline *Rr_CreateGraphicsPipeline(Rr_App *App, Rr_PipelineInfo *Info);

extern void Rr_DestroyGraphicsPipeline(Rr_App *App, Rr_GraphicsPipeline *GraphicsPipelin);

#ifdef __cplusplus
}
#endif
