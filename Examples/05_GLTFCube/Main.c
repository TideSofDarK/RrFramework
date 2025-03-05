#include <Rr/Rr.h>

#include "ExampleAssets.inc"

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
static Rr_Image *DepthAttachment;
static Rr_Buffer *StagingBuffer;
static Rr_Buffer *UniformBuffer;
static Rr_PipelineLayout *PipelineLayout;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Sampler *NearestSampler;

static SUniformData UniformData;

static bool Loaded;

static void OnLoadComplete(Rr_App *App, void *UserData)
{
    Loaded = true;
}

static void Init(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    /* Create simple sampler. */

    Rr_SamplerInfo SamplerInfo = { 0 };
    SamplerInfo.MinFilter = RR_FILTER_NEAREST;
    SamplerInfo.MagFilter = RR_FILTER_NEAREST;
    NearestSampler = Rr_CreateSampler(Renderer, &SamplerInfo);

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
    PipelineLayout = Rr_CreatePipelineLayout(Renderer, 1, &BindingSet);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Format = RR_FORMAT_VEC3, .Location = 0 },
        { .Format = RR_FORMAT_VEC2, .Location = 1 },
        { .Format = RR_FORMAT_VEC3, .Location = 2 },
    };

    Rr_VertexInputBinding VertexInputBinding = {
        .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
        .AttributeCount = RR_ARRAY_COUNT(VertexAttributes),
        .Attributes = VertexAttributes,
    };

    Rr_ColorTargetInfo ColorTargets[1] = { 0 };
    ColorTargets[0].Format = Rr_GetSwapchainFormat(Renderer);

    Rr_GraphicsPipelineCreateInfo PipelineInfo = { 0 };
    PipelineInfo.Layout = PipelineLayout;
    PipelineInfo.VertexShaderSPV =
        Rr_LoadAsset(EXAMPLE_ASSET_GLTFCUBE_VERT_SPV);
    PipelineInfo.FragmentShaderSPV =
        Rr_LoadAsset(EXAMPLE_ASSET_GLTFCUBE_FRAG_SPV);
    PipelineInfo.VertexInputBindingCount = 1;
    PipelineInfo.VertexInputBindings = &VertexInputBinding;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;
    PipelineInfo.DepthStencil.EnableDepthTest = true;
    PipelineInfo.DepthStencil.EnableDepthWrite = true;
    PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

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
        Renderer,
        1,
        &VertexInputBinding,
        &GLTFVertexInputBinding,
        RR_ARRAY_COUNT(GLTFTextureMappings),
        GLTFTextureMappings);

    /* Create load thread and load glTF asset. */

    LoadThread = Rr_CreateLoadThread(App);
    Rr_LoadTask Tasks[] = {
        Rr_LoadGLTFAssetTask(EXAMPLE_ASSET_CUBE_GLB, GLTFContext, &GLTFAsset),
    };
    Rr_LoadAsync(LoadThread, RR_ARRAY_COUNT(Tasks), Tasks, OnLoadComplete, App);

    /* Create depth buffer. */

    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);

    DepthAttachment = Rr_CreateImage(
        Renderer,
        (Rr_IntVec3){ SwapchainSize.Width, SwapchainSize.Height, 1 },
        RR_TEXTURE_FORMAT_D32_SFLOAT,
        RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT |
            RR_IMAGE_FLAGS_TRANSFER_BIT);

    /* Create uniform buffer. */

    UniformBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(UniformData),
        RR_BUFFER_FLAGS_UNIFORM_BIT);

    /* Create staging buffer */

    StagingBuffer = Rr_CreateBuffer(
        Renderer,
        RR_MEGABYTES(1),
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    UniformData.Model = Rr_M4D(1.0f);
}

static void DrawFirstGLTFPrimitive(Rr_App *App, Rr_GraphNode *GraphicsNode)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);

    UniformData.Projection = Rr_Perspective_LH_ZO(
        0.7643276f,
        SwapchainSize.Width / (float)SwapchainSize.Height,
        0.5f,
        50.0f);
    UniformData.View = Rr_Translate((Rr_Vec3){ 0.0f, 0.0f, 5.0f });
    UniformData.Model = Rr_MulM4(UniformData.Model, Rr_Rotate_LH(0.005f, (Rr_Vec3){ 0.0f, 1.0f, 0.0f }));

    memcpy(
        Rr_GetMappedBufferData(Renderer, StagingBuffer),
        &UniformData,
        sizeof(UniformData));

    Rr_GraphNode *TransferNode =
        Rr_AddTransferNode(Renderer, "upload_uniform_buffer");
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
    Rr_BindSampledImage(GraphicsNode, GLTFAsset->Images[0], 0, 2);
    Rr_DrawIndexed(GraphicsNode, GLTFPrimitive->IndexCount, 1, 0, 0, 0);
}

static void Iterate(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

    Rr_ColorTarget OffscreenTarget = {
        .Slot = 0,
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = (Rr_ColorClear){ { 0.1f, 0.1f, 0.1f, 1.0f } },
    };
    Rr_DepthTarget OffscreenDepth = {
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = {
            .Depth = 1.0f,
        },
    };
    Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
        Renderer,
        "offscreen",
        1,
        &OffscreenTarget,
        &SwapchainImage,
        &OffscreenDepth,
        DepthAttachment);
    if(Loaded)
    {
        DrawFirstGLTFPrimitive(App, GraphicsNode);
    }
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_DestroyLoadThread(App, LoadThread);
    Rr_DestroyGLTFContext(GLTFContext);
    Rr_DestroyImage(Renderer, DepthAttachment);
    Rr_DestroyBuffer(Renderer, StagingBuffer);
    Rr_DestroyBuffer(Renderer, UniformBuffer);
    Rr_DestroyGraphicsPipeline(Renderer, GraphicsPipeline);
    Rr_DestroyPipelineLayout(Renderer, PipelineLayout);
    Rr_DestroySampler(Renderer, NearestSampler);
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {
        .Title = "05_GLTFCube",
        .Version = "1.0.0",
        .Package = "com.rr.examples.05_gltfcube",
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
    };
    Rr_Run(&Config);

    return 0;
}
