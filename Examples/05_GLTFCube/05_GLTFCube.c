#include <Rr/Rr.h>

#include "ExampleAssets.inc"

typedef struct UniformData UniformData;
struct UniformData
{
    Rr_Mat4 Model;
    Rr_Mat4 View;
    Rr_Mat4 Projection;
};

static Rr_LoadThread *LoadThread;
static Rr_GLTFContext *GLTFContext;
static Rr_GLTFAsset *GLTFAsset;
static Rr_Image *ColorAttachment;
static Rr_Image *DepthAttachment;
static Rr_Buffer *UniformBuffer;
static Rr_PipelineLayout *PipelineLayout;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Sampler *NearestSampler;

static bool Loaded;

static void OnLoadComplete(Rr_App *App, void *UserData)
{
    Loaded = true;
}

static void Init(Rr_App *App, void *UserData)
{
    /* Create simple sampler. */

    Rr_SamplerInfo SamplerInfo = { 0 };
    SamplerInfo.MinFilter = RR_FILTER_NEAREST;
    SamplerInfo.MagFilter = RR_FILTER_NEAREST;
    NearestSampler = Rr_CreateSampler(App, &SamplerInfo);

    /* Create graphics pipeline. */

    Rr_PipelineBinding Bindings[] = {
        {
            .Slot = 0,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
        },
        {
            .Slot = 1,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_SAMPLER,
        },
        {
            .Slot = 2,
            .Count = 1,
            .Type = RR_PIPELINE_BINDING_TYPE_SAMPLED_IMAGE,
        },
    };
    Rr_PipelineBindingSet BindingSet = {
        .BindingCount = RR_ARRAY_COUNT(Bindings),
        .Bindings = Bindings,
        .Stages = RR_SHADER_STAGE_FRAGMENT_BIT | RR_SHADER_STAGE_VERTEX_BIT,
    };
    PipelineLayout = Rr_CreatePipelineLayout(App, 1, &BindingSet);

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
    ColorTargets[0].Format = Rr_GetSwapchainFormat(App);

    Rr_PipelineInfo PipelineInfo = { 0 };
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

    GraphicsPipeline = Rr_CreateGraphicsPipeline(App, &PipelineInfo);

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
        App,
        1,
        &VertexInputBinding,
        &GLTFVertexInputBinding,
        RR_ARRAY_COUNT(GLTFTextureMappings),
        GLTFTextureMappings);

    /* Create load thread and load glTF asset. */

    LoadThread = Rr_CreateLoadThread(App);
    Rr_LoadTask Tasks[] = {
        {
            .LoadType = RR_LOAD_TYPE_GLTF_ASSET,
            .AssetRef = EXAMPLE_ASSET_CUBE_GLB,
            .Options = {
                .GLTF = { .GLTFContext = GLTFContext, },
            },
            .Out = { .GLTFAsset = &GLTFAsset },
        },
    };
    Rr_LoadAsync(LoadThread, RR_ARRAY_COUNT(Tasks), Tasks, OnLoadComplete, App);

    /* Create main draw target. */

    ColorAttachment = Rr_CreateImage(
        App,
        (Rr_IntVec3){ 320, 240, 1 },
        Rr_GetSwapchainFormat(App),
        RR_IMAGE_USAGE_COLOR_ATTACHMENT | RR_IMAGE_USAGE_TRANSFER |
            RR_IMAGE_USAGE_SAMPLED,
        false);

    DepthAttachment = Rr_CreateImage(
        App,
        (Rr_IntVec3){ 320, 240, 1 },
        RR_TEXTURE_FORMAT_D32_SFLOAT,
        RR_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT | RR_IMAGE_USAGE_TRANSFER,
        false);

    /* Create uniform buffer. */

    UniformBuffer = Rr_CreateBuffer(
        App,
        sizeof(UniformData),
        RR_BUFFER_FLAGS_UNIFORM_BIT);
}

static void DrawFirstGLTFPrimitive(
    Rr_App *App,
    Rr_GraphImageHandle *ColorAttachmentHandle,
    Rr_GraphImageHandle *DepthAttachmentHandle)
{
    double Time = Rr_GetTimeSeconds(App);

    Rr_GraphBufferHandle UniformBufferHandle =
        Rr_RegisterGraphBuffer(App, UniformBuffer);

    UniformData UniformData = {};
    UniformData.Projection =
        Rr_Perspective_LH_ZO(0.7643276f, 320.0f / 240.0f, 0.5f, 50.0f);
    UniformData.View = Rr_M4D(1.0f);
    UniformData.Model = Rr_MulM4(
        Rr_Translate((Rr_Vec3){ 0.0f, 0.0f, 5.0f }),
        Rr_Rotate_LH(cos(Time), (Rr_Vec3){ 0.0f, 1.0f, 0.0f }));
    UniformData.Model = Rr_MulM4(
        UniformData.Model,
        Rr_Rotate_LH(sin(Time), (Rr_Vec3){ 0.0f, 0.0f, 1.0f }));

    Rr_GraphNode *TransferNode =
        Rr_AddTransferNode(App, "upload_uniform_buffer", &UniformBufferHandle);
    Rr_TransferBufferData(
        App,
        TransferNode,
        RR_MAKE_DATA_STRUCT(UniformData),
        0);

    Rr_ColorTarget OffscreenTarget = {
        .Slot = 0,
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = (Rr_ColorClear){ .Float32 = { 0.1f, 0.1f, 0.1f, 1.0f } },
    };
    Rr_DepthTarget OffscreenDepth = {
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = {
            .Depth = 1.0f,
        },
    };
    Rr_GraphNode *OffscreenNode = Rr_AddGraphicsNode(
        App,
        "offscreen",
        1,
        &OffscreenTarget,
        &(Rr_GraphImageHandle *){ ColorAttachmentHandle },
        &OffscreenDepth,
        DepthAttachmentHandle);

    Rr_GraphBufferHandle GLTFBufferHandle =
        Rr_RegisterGraphBuffer(App, GLTFAsset->Buffer);

    Rr_GLTFPrimitive *GLTFPrimitive = GLTFAsset->Meshes->Primitives;
    Rr_BindGraphicsPipeline(OffscreenNode, GraphicsPipeline);
    Rr_BindVertexBuffer(
        OffscreenNode,
        &GLTFBufferHandle,
        0,
        *GLTFPrimitive->VertexBufferOffsets);
    Rr_BindIndexBuffer(
        OffscreenNode,
        &GLTFBufferHandle,
        0,
        GLTFPrimitive->IndexBufferOffset,
        GLTFPrimitive->IndexType);
    Rr_BindGraphicsUniformBuffer(
        OffscreenNode,
        &UniformBufferHandle,
        0,
        0,
        0,
        sizeof(UniformData));
    Rr_BindSampler(OffscreenNode, NearestSampler, 0, 1);
    Rr_GraphImageHandle ColorTextureHandle = Rr_RegisterGraphImage(App, GLTFAsset->Images[0]);
    Rr_BindSampledImage(OffscreenNode, &ColorTextureHandle, 0, 2);
    Rr_DrawIndexed(OffscreenNode, GLTFPrimitive->IndexCount, 1, 0, 0, 0);
}

static void Iterate(Rr_App *App, void *UserData)
{
    Rr_GraphImageHandle ColorAttachmentHandle =
        Rr_RegisterGraphImage(App, ColorAttachment);
    Rr_GraphImageHandle DepthAttachmentHandle =
        Rr_RegisterGraphImage(App, DepthAttachment);

    if(Loaded)
    {
        DrawFirstGLTFPrimitive(
            App,
            &ColorAttachmentHandle,
            &DepthAttachmentHandle);
    }

    Rr_AddPresentNode(
        App,
        "present",
        &ColorAttachmentHandle,
        NearestSampler,
        RR_PRESENT_MODE_FIT);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_DestroyLoadThread(App, LoadThread);
    Rr_DestroyImage(App, ColorAttachment);
    Rr_DestroyImage(App, DepthAttachment);
    Rr_DestroyBuffer(App, UniformBuffer);
    Rr_DestroyGLTFContext(App, GLTFContext);
    Rr_DestroyGraphicsPipeline(App, GraphicsPipeline);
    Rr_DestroyPipelineLayout(App, PipelineLayout);
    Rr_DestroySampler(App, NearestSampler);
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
