#include <Rr/Rr.h>

#include "ExampleAssets.inc"

static Rr_GLTFContext *GLTFContext;
static Rr_GLTFAsset *GLTFAsset;
static Rr_Image *ColorAttachment;
static Rr_Buffer *UniformBuffer;
static Rr_PipelineLayout *PipelineLayout;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Sampler *NearestSampler;

static void Init(Rr_App *App, void *UserData)
{
    /* Create simple sampler. */

    Rr_SamplerInfo SamplerInfo = { 0 };
    SamplerInfo.MinFilter = RR_FILTER_NEAREST;
    SamplerInfo.MagFilter = RR_FILTER_NEAREST;
    NearestSampler = Rr_CreateSampler(App, &SamplerInfo);

    /* Create graphics pipeline. */

    Rr_PipelineBinding Binding = {
        .Slot = 0,
        .Count = 1,
        .Type = RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER,
    };
    Rr_PipelineBindingSet BindingSet = {
        .BindingCount = 1,
        .Bindings = &Binding,
        .Stages = RR_SHADER_STAGE_VERTEX_BIT,
    };
    PipelineLayout = Rr_CreatePipelineLayout(App, 1, &BindingSet);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Format = RR_FORMAT_VEC3, .Location = 0 },
        { .Format = RR_FORMAT_VEC3, .Location = 1 },
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
    PipelineInfo.VertexInputBindingCount = RR_ARRAY_COUNT(VertexAttributes);
    PipelineInfo.VertexInputBindings = &VertexInputBinding;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(App, &PipelineInfo);

    /* Create GLTF context. */

    Rr_GLTFAttributeType GLTFAttributeTypes[] = {
        RR_GLTF_ATTRIBUTE_TYPE_POSITION,
        RR_GLTF_ATTRIBUTE_TYPE_NORMAL,
    };
    Rr_GLTFVertexInputBinding GLTFVertexInputBinding = {
        .AttributeCount = RR_ARRAY_COUNT(GLTFAttributeTypes),
        .Attributes = GLTFAttributeTypes,
    };
    GLTFContext = Rr_CreateGLTFContext(
        App,
        1,
        &VertexInputBinding,
        &GLTFVertexInputBinding);

    /* Create main draw target. */

    ColorAttachment = Rr_CreateImage(
        App,
        (Rr_IntVec3){ 240, 320, 1 },
        Rr_GetSwapchainFormat(App),
        RR_IMAGE_USAGE_COLOR_ATTACHMENT | RR_IMAGE_USAGE_TRANSFER |
            RR_IMAGE_USAGE_SAMPLED,
        false);
}

struct UniformData
{
    Rr_Mat4 Model;
    Rr_Mat4 View;
    Rr_Mat4 Projection;
};

static void DrawFirstGLTFMesh(
    Rr_App *App,
    Rr_GraphImageHandle *ColorAttachmentHandle)
{
    Rr_GraphBufferHandle UniformBufferHandle = Rr_RegisterGraphBuffer(App, UniformBuffer);

    UniformData UniformData;

    Rr_GraphNode *TransferNode = Rr_AddTransferNode(App, "upload_uniform_buffer", &UniformBufferHandle);
    Rr_TransferBufferData(App, TransferNode, RR_MAKE_DATA_STRUCT());

    Rr_ColorTarget OffscreenTarget = {
        .Slot = 0,
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .ColorClear = (Rr_ColorClear){ .Float32 = { 0.1f, 0.1f, 0.1f, 1.0f } },
    };
    Rr_GraphNode *OffscreenNode = Rr_AddGraphicsNode(
        App,
        "offscreen",
        1,
        &OffscreenTarget,
        &(Rr_GraphImageHandle *){ ColorAttachmentHandle },
        NULL,
        NULL);
    Rr_BindGraphicsPipeline(OffscreenNode, GraphicsPipeline);
    Rr_BindVertexBuffer(OffscreenNode, &VertexBufferHandle, 0, 0);
    Rr_BindIndexBuffer(
        OffscreenNode,
        &IndexBufferHandle,
        0,
        0,
        RR_INDEX_TYPE_UINT32);
    Rr_DrawIndexed(OffscreenNode, 3, 1, 0, 0, 0);
}

static void Iterate(Rr_App *App, void *UserData)
{
    Rr_GraphImageHandle ColorAttachmentHandle =
        Rr_RegisterGraphImage(App, ColorAttachment);

    DrawFirstGLTFMesh(App, &ColorAttachmentHandle);

    Rr_AddPresentNode(
        App,
        "present",
        &ColorAttachmentHandle,
        NearestSampler,
        RR_PRESENT_MODE_FIT);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_DestroyImage(App, ColorAttachment);
    Rr_DestroyGraphicsPipeline(App, GraphicsPipeline);
    Rr_DestroyPipelineLayout(App, PipelineLayout);
    Rr_DestroySampler(App, NearestSampler);
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {
        .Title = "05_Triangle",
        .Version = "1.0.0",
        .Package = "com.rr.examples.05_gltfcube",
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
    };
    Rr_Run(&Config);

    return 0;
}
