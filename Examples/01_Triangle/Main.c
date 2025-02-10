#include <Rr/Rr.h>

#include "ExampleAssets.inc"

static Rr_Image *ColorAttachment;
static Rr_PipelineLayout *PipelineLayout;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Buffer *VertexBuffer;
static Rr_Buffer *IndexBuffer;
static Rr_Sampler *NearestSampler;

static void Init(Rr_App *App, void *UserData)
{
    Rr_SamplerInfo SamplerInfo = { 0 };
    SamplerInfo.MinFilter = RR_FILTER_NEAREST;
    SamplerInfo.MagFilter = RR_FILTER_NEAREST;
    SamplerInfo.AddressModeU = RR_SAMPLER_ADDRESS_MODE_REPEAT;
    SamplerInfo.AddressModeV = RR_SAMPLER_ADDRESS_MODE_REPEAT;
    NearestSampler = Rr_CreateSampler(App, &SamplerInfo);

    PipelineLayout = Rr_CreatePipelineLayout(App, 0, NULL);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Format = RR_FORMAT_VEC3, .Type = RR_VERTEX_INPUT_TYPE_VERTEX, .Location = 0 },
        { .Format = RR_FORMAT_VEC3, .Type = RR_VERTEX_INPUT_TYPE_VERTEX, .Location = 1 },
    };

    Rr_ColorTargetInfo ColorTargets[1] = { 0 };
    ColorTargets[0].Blend.ColorWriteMask = RR_COLOR_COMPONENT_ALL;
    ColorTargets[0].Format = Rr_GetSwapchainFormat(App);

    Rr_PipelineInfo PipelineInfo = { 0 };
    PipelineInfo.Layout = PipelineLayout;
    PipelineInfo.VertexShaderSPV = Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_VERT_SPV);
    PipelineInfo.FragmentShaderSPV = Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_FRAG_SPV);
    PipelineInfo.VertexAttributeCount = RR_ARRAY_COUNT(VertexAttributes);
    PipelineInfo.VertexAttributes = VertexAttributes;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(App, &PipelineInfo);

    float VertexData[] = {
        -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };

    VertexBuffer = Rr_CreateBuffer(App, sizeof(VertexData), RR_BUFFER_USAGE_VERTEX_BUFFER_BIT, false);
    Rr_UploadToDeviceBufferImmediate(App, VertexBuffer, RR_MAKE_DATA_ARRAY(VertexData));

    uint32_t IndexData[] = {
        2,
        1,
        0,
    };

    IndexBuffer = Rr_CreateBuffer(App, sizeof(IndexData), RR_BUFFER_USAGE_INDEX_BUFFER_BIT, false);
    Rr_UploadToDeviceBufferImmediate(App, IndexBuffer, RR_MAKE_DATA_ARRAY(IndexData));

    ColorAttachment = Rr_CreateImage(
        App,
        (Rr_IntVec3){ 240, 320, 1 },
        Rr_GetSwapchainFormat(App),
        RR_IMAGE_USAGE_COLOR_ATTACHMENT | RR_IMAGE_USAGE_TRANSFER | RR_IMAGE_USAGE_SAMPLED,
        false);
}

static void Iterate(Rr_App *App, void *UserData)
{
    Rr_GraphImageHandle ColorAttachmentHandle = Rr_RegisterGraphImage(App, ColorAttachment);
    Rr_GraphBufferHandle VertexBufferHandle = Rr_RegisterGraphBuffer(App, VertexBuffer);
    Rr_GraphBufferHandle IndexBufferHandle = Rr_RegisterGraphBuffer(App, IndexBuffer);

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
        &(Rr_GraphImageHandle *){ &ColorAttachmentHandle },
        NULL,
        NULL);
    Rr_BindGraphicsPipeline(OffscreenNode, GraphicsPipeline);
    Rr_BindVertexBuffer(OffscreenNode, &VertexBufferHandle, 0, 0);
    Rr_BindIndexBuffer(OffscreenNode, &IndexBufferHandle, 0, 0, RR_INDEX_TYPE_UINT32);
    Rr_DrawIndexed(OffscreenNode, 3, 1, 0, 0, 0);

    Rr_AddPresentNode(App, "present", &ColorAttachmentHandle, NearestSampler, RR_PRESENT_MODE_FIT);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_DestroyImage(App, ColorAttachment);
    Rr_DestroyBuffer(App, VertexBuffer);
    Rr_DestroyBuffer(App, IndexBuffer);
    Rr_DestroyGraphicsPipeline(App, GraphicsPipeline);
    Rr_DestroyPipelineLayout(App, PipelineLayout);
    Rr_DestroySampler(App, NearestSampler);
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {
        .Title = "01_Triangle",
        .Version = "1.0.0",
        .Package = "com.rr.examples.01_triangle",
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
    };
    Rr_Run(&Config);

    return 0;
}
