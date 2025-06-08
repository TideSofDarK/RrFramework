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
};

static Rr_LoadThread *LoadThread;
static Rr_GLTFContext *GLTFContext;
static Rr_GLTFAsset *GLTFAsset;
static Rr_Image2D *DepthAttachment;
static Rr_Buffer *StagingBuffer;
static Rr_Buffer *UniformBuffer;
static Rr_PipelineLayout *PipelineLayout;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Sampler *NearestSampler;

static SUniformData UniformData;

static bool Loaded;

static void OnLoadComplete(void *UserData)
{
    Loaded = true;
}

static void InitDepthImage(void)
{
    if (DepthAttachment != NULL)
    {
        Rr_ReleaseImage(DepthAttachment);
    }

    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();

    DepthAttachment = Rr_CreateImage2D(
        (Rr_IntVec2){ SwapchainSize.Width, SwapchainSize.Height },
        RR_TEXTURE_FORMAT_D32_SFLOAT,
        RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT |
            RR_IMAGE_FLAGS_TRANSFER_BIT);
}

static void Init(void *UserData)
{
    /* Create simple sampler. */

    Rr_SamplerInfo SamplerInfo = { 0 };
    SamplerInfo.MinFilter = RR_FILTER_NEAREST;
    SamplerInfo.MagFilter = RR_FILTER_NEAREST;
    NearestSampler = Rr_CreateSampler(&SamplerInfo);

    /* Create graphics pipeline. */

    Rr_PipelineBinding Bindings[] = {
        {
            .Binding = 0,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
        },
        {
            .Binding = 1,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_SAMPLER,
        },
        {
            .Binding = 2,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_SAMPLED_IMAGE,
        },
    };
    Rr_PipelineBindingSet BindingSet = {
        .BindingCount = RR_ARRAY_COUNT(Bindings),
        .Bindings = Bindings,
        .Stages = RR_SHADER_STAGE_FRAGMENT_BIT | RR_SHADER_STAGE_VERTEX_BIT,
    };
    PipelineLayout = Rr_CreatePipelineLayout(1, &BindingSet);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Format = RR_FORMAT_VEC3, .Location = 0 },
        { .Format = RR_FORMAT_VEC2, .Location = 1 },
        { .Format = RR_FORMAT_VEC3, .Location = 2 },
    };

    Rr_VertexInputBinding VertexInputBindings[] = {
        {
            .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
            .AttributeCount = RR_ARRAY_COUNT(VertexAttributes),
            .Attributes = VertexAttributes,
        },
    };

    Rr_ColorTargetInfo ColorTargets[1] = { 0 };
    ColorTargets[0].Format = Rr_GetSwapchainFormat();

    Rr_GraphicsPipelineCreateInfo PipelineInfo = { 0 };
    PipelineInfo.Layout = PipelineLayout;
    PipelineInfo.VertexShaderSPV =
        Rr_LoadAsset(EXAMPLE_ASSET_GLTFCUBE_VERT_SPV);
    PipelineInfo.FragmentShaderSPV =
        Rr_LoadAsset(EXAMPLE_ASSET_GLTFCUBE_FRAG_SPV);
    PipelineInfo.VertexInputBindingCount = RR_ARRAY_COUNT(VertexInputBindings);
    PipelineInfo.VertexInputBindings = VertexInputBindings;
    PipelineInfo.ColorTargetCount = RR_ARRAY_COUNT(ColorTargets);
    PipelineInfo.ColorTargets = ColorTargets;
    PipelineInfo.DepthStencil.Format = RR_TEXTURE_FORMAT_D32_SFLOAT;
    PipelineInfo.DepthStencil.EnableDepthTest = true;
    PipelineInfo.DepthStencil.EnableDepthWrite = true;
    PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
    PipelineInfo.Rasterizer.FrontFace = RR_FRONT_FACE_COUNTER_CLOCKWISE;
    PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_BACK;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    /* Create GLTF context. */

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

    /* Create load thread and load glTF asset. */

    LoadThread = Rr_CreateLoadThread();
    Rr_LoadTask Tasks[] = {
        Rr_LoadGLTFAssetTask(EXAMPLE_ASSET_CUBE_GLB, GLTFContext, &GLTFAsset),
    };
    Rr_LoadAsync(
        LoadThread,
        RR_ARRAY_COUNT(Tasks),
        Tasks,
        OnLoadComplete,
        NULL);

    /* Create uniform buffer. */

    UniformBuffer =
        Rr_CreateBuffer(sizeof(UniformData), RR_BUFFER_FLAGS_UNIFORM_BIT);

    /* Create staging buffer */

    StagingBuffer = Rr_CreateBuffer(
        RR_MEGABYTES(1),
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    UniformData.Model = Rr_M4D(1.0f);

    InitDepthImage();
}

static void Event(void *UserData, Rr_Event *Event)
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
    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();

    UniformData.Projection = Rr_Perspective_LH(
        0.7643276f,
        SwapchainSize.Width / (float)SwapchainSize.Height,
        0.5f,
        50.0f);
    UniformData.View = Rr_Translate((Rr_Vec3){ 0.0f, 0.0f, 5.0f });
    UniformData.Model = Rr_MulM4(
        UniformData.Model,
        Rr_Rotate_LH(0.005f, (Rr_Vec3){ 0.0f, 1.0f, 0.0f }));

    memcpy(
        Rr_GetMappedBufferData(StagingBuffer),
        &UniformData,
        sizeof(UniformData));

    Rr_GraphNode *TransferNode =
        Rr_AddTransferNode(Rr_GetGraph(), "upload_uniform_buffer");
    Rr_TransferBufferData(
        TransferNode,
        sizeof(UniformData),
        StagingBuffer,
        0,
        UniformBuffer,
        0);

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

    Rr_UIDebugOverlay();
}

static void Iterate(void *UserData)
{
    Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

    Rr_ColorTarget ColorTarget = {
        .Slot = 0,
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = (Rr_ColorClear){ { 0.1f, 0.1f, 0.1f, 1.0f } },
    };
    Rr_DepthTarget DepthTarget = {
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = {
            .Depth = 1.0f,
        },
    };
    Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
        Rr_GetGraph(),
        "graphics",
        1,
        &ColorTarget,
        &SwapchainImage,
        &DepthTarget,
        DepthAttachment);
    if (Loaded)
    {
        DrawFirstGLTFPrimitive(GraphicsNode);
    }
}

static void Cleanup(void *UserData)
{
    Rr_DestroyLoadThread(LoadThread);
    Rr_ReleaseGLTFContext(GLTFContext);
    Rr_ReleaseImage(DepthAttachment);
    Rr_ReleaseBuffer(StagingBuffer);
    Rr_ReleaseBuffer(UniformBuffer);
    Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    Rr_ReleasePipelineLayout(PipelineLayout);
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
