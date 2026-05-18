#include <Rr/Rr.h>

#include "ExampleAssets.inc"
#include "Rr/Rr_Pipeline.h"

#include <string.h>

typedef struct SUniformData SUniformData;
struct SUniformData
{
    Rr_Mat4 Model;
    Rr_Mat4 View;
    Rr_Mat4 Projection;
    float Time;
};

static Rr_GLTFContext *GLTFContext;
static Rr_GLTFAsset *GLTFAsset;
static Rr_Image2D *DepthAttachment;
static Rr_Buffer *UniformBuffer;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Sampler *NearestSampler;

static SUniformData UniformData;

#include <stdio.h>

static void InitDepthImage(void)
{
    if (DepthAttachment != NULL)
    {
        Rr_ReleaseImage(DepthAttachment);
    }

    Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());

    Rr_SetNextObjectName("DepthImage");
    DepthAttachment = Rr_CreateImage2D(
        (Rr_IntVec2){ SwapchainSize.Width, SwapchainSize.Height },
        RR_IMAGE_FORMAT_D32_SFLOAT,
        RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT |
            RR_IMAGE_FLAGS_TRANSFER_BIT);
}

static void Init(void)
{
    Rr_SamplerInfo SamplerInfo = { 0 };
    SamplerInfo.MinFilter = RR_FILTER_LINEAR;
    SamplerInfo.MagFilter = RR_FILTER_LINEAR;
    NearestSampler = Rr_CreateSampler(&SamplerInfo);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Location = 0, .Format = RR_FORMAT_VEC3 },
        { .Location = 1, .Format = RR_FORMAT_VEC2 },
        { .Location = 2, .Format = RR_FORMAT_VEC3 },
    };

    Rr_VertexInputBinding VertexInputBindings[] = {
        {
            .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
            .AttributeCount = RR_ARRAY_COUNT(VertexAttributes),
            .Attributes = VertexAttributes,
        },
    };

    Rr_ColorTargetInfo ColorTargets[1] = { 0 };
    ColorTargets[0].Format = Rr_GetImageFormat(Rr_GetSwapchainImage());

    Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFCUBE_VERT_SPV);
    Rr_ShaderInfo VertexShaderInfo = {
        .SPVSize = VertexShader.Size,
        .SPVData = VertexShader.Pointer,
    };

    Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFCUBE_FRAG_SPV);
    Rr_ShaderInfo FragmentShaderInfo = {
        .SPVSize = FragmentShader.Size,
        .SPVData = FragmentShader.Pointer,
    };

    Rr_GraphicsPipelineCreateInfo PipelineInfo = { 0 };
    PipelineInfo.VertexShaderInfo = &VertexShaderInfo;
    PipelineInfo.FragmentShaderInfo = &FragmentShaderInfo;
    PipelineInfo.VertexInputBindingCount = RR_ARRAY_COUNT(VertexInputBindings);
    PipelineInfo.VertexInputBindings = VertexInputBindings;
    PipelineInfo.ColorTargetCount = RR_ARRAY_COUNT(ColorTargets);
    PipelineInfo.ColorTargets = ColorTargets;
    PipelineInfo.DepthStencil.Format = RR_IMAGE_FORMAT_D32_SFLOAT;
    PipelineInfo.DepthStencil.EnableDepthTest = true;
    PipelineInfo.DepthStencil.EnableDepthWrite = true;
    PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
    PipelineInfo.Rasterizer.FrontFace = RR_FRONT_FACE_CLOCKWISE;
    PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_BACK;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    Rr_GLTFAttributeType GLTFAttributeTypes[] = {
        RR_GLTF_ATTRIBUTE_TYPE_POSITION,
        RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0,
        RR_GLTF_ATTRIBUTE_TYPE_NORMAL,
    };
    Rr_GLTFVertexInputBinding GLTFVertexInputBinding = {
        .AttributeTypeCount = RR_ARRAY_COUNT(GLTFAttributeTypes),
        .AttributeTypes = GLTFAttributeTypes,
    };
    Rr_GLTFTextureMapping GLTFTextureMappings[] = {
        {
            .TextureType = RR_GLTF_TEXTURE_TYPE_COLOR,
            .Set = 0,
            .Binding = 1,
        },
    };
    GLTFContext = Rr_CreateGLTFContext(
        RR_ARRAY_COUNT(VertexInputBindings),
        VertexInputBindings,
        &GLTFVertexInputBinding,
        RR_ARRAY_COUNT(GLTFTextureMappings),
        GLTFTextureMappings);

    Rr_Asset LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_GLB);
    GLTFAsset = Rr_CreateGLTFAsset(
        GLTFContext,
        Rr_GetGraph(),
        LoadedAsset.Size,
        LoadedAsset.Pointer);

    UniformBuffer = Rr_CreateBuffer(
        sizeof(UniformData),
        RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);

    UniformData.Model = Rr_M4D(1.0f);
    UniformData.View = Rr_LookAt_RH(
        Rr_V3(0.0f, 0.0f, -5.0f),
        Rr_V3F(0.0f),
        Rr_V3(0.0f, 1.0f, 0.0f));

    InitDepthImage();
}

static void Event(Rr_Event const *Event)
{
    switch (Event->Type)
    {
        case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
        {
            InitDepthImage();
            return;
        }
        default:
            return;
    }
}

static void DrawFirstGLTFPrimitive(Rr_GraphNode *GraphicsNode)
{
    Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());

    UniformData.Projection = Rr_Perspective_RH(
        0.7643276f,
        SwapchainSize.Width / (float)SwapchainSize.Height,
        0.5f,
        50.0f);
    UniformData.Model = Rr_MulM4(
        Rr_Rotate_RH(0.005f, (Rr_Vec3){ 0.0f, 1.0f, 0.0f }),
        UniformData.Model);
    UniformData.Time = (float)Rr_GetTimeSeconds();

    memcpy(
        Rr_GetMappedBufferData(UniformBuffer),
        &UniformData,
        sizeof(UniformData));

    Rr_GLTFPrimitive *GLTFPrimitive = GLTFAsset->Meshes->Primitives;
    Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
    Rr_BindVertexBuffer(
        GraphicsNode,
        GLTFAsset->Buffer,
        0,
        GLTFAsset->VertexBufferOffset);
    Rr_BindIndexBuffer(
        GraphicsNode,
        GLTFAsset->Buffer,
        0,
        GLTFAsset->IndexBufferOffset,
        GLTFAsset->IndexType);
    Rr_BindUniformBuffer(
        GraphicsNode,
        UniformBuffer,
        0,
        0,
        0,
        sizeof(UniformData));
    Rr_BindSampler(GraphicsNode, NearestSampler, 0, 1);
    Rr_BindSampledImage2D(GraphicsNode, GLTFAsset->Images[0], 0, 2);
    Rr_DrawIndexed(GraphicsNode, GLTFPrimitive->IndexCount, 1, 0, 0, 0);
}

static void Iterate(void)
{
    Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

    Rr_ColorTarget ColorTarget = {
        .Image = SwapchainImage,
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = (Rr_ColorClear){ { 0.03f, 0.03f, 0.04f, 1.0f } },
    };
    Rr_DepthTarget DepthTarget = {
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = {
            .Depth = 1.0f,
        },
        .Image = DepthAttachment,
    };
    Rr_GraphNode *GraphicsNode =
        Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, &DepthTarget);

    DrawFirstGLTFPrimitive(GraphicsNode);

    Rr_UIDebugOverlay();
}

static void Cleanup(void)
{
    Rr_ReleaseGLTFContext(GLTFContext);
    Rr_ReleaseImage(DepthAttachment);
    Rr_ReleaseBuffer(UniformBuffer);
    Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    Rr_ReleaseSampler(NearestSampler);
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {
        .Title = "GLTFCube",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = Init,
        .EventFunc = Event,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
    };
    Rr_Run(&Config);

    return 0;
}
