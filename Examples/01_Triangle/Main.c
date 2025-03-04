#include <Rr/Rr.h>

#include "ExampleAssets.inc"

static Rr_PipelineLayout *PipelineLayout;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Buffer *VertexBuffer;
static Rr_Buffer *IndexBuffer;

static void Init(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    PipelineLayout = Rr_CreatePipelineLayout(Renderer, 0, NULL);

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
    ColorTargets[0].Blend.ColorWriteMask = RR_COLOR_COMPONENT_ALL;
    ColorTargets[0].Format = Rr_GetSwapchainFormat(Renderer);

    Rr_GraphicsPipelineCreateInfo PipelineInfo = { 0 };
    PipelineInfo.Layout = PipelineLayout;
    PipelineInfo.VertexShaderSPV =
        Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_VERT_SPV);
    PipelineInfo.FragmentShaderSPV =
        Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_FRAG_SPV);
    PipelineInfo.VertexInputBindingCount = RR_ARRAY_COUNT(VertexAttributes);
    PipelineInfo.VertexInputBindings = &VertexInputBinding;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

    float VertexData[] = {
        -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
        0.0f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,  1.0f,
    };

    VertexBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(VertexData),
        RR_BUFFER_FLAGS_VERTEX_BIT);
    Rr_UploadToDeviceBufferImmediate(
        Renderer,
        VertexBuffer,
        RR_MAKE_DATA_ARRAY(VertexData));

    uint32_t IndexData[] = {
        2,
        1,
        0,
    };

    IndexBuffer =
        Rr_CreateBuffer(Renderer, sizeof(IndexData), RR_BUFFER_FLAGS_INDEX_BIT);
    Rr_UploadToDeviceBufferImmediate(
        Renderer,
        IndexBuffer,
        RR_MAKE_DATA_ARRAY(IndexData));
}

static void Iterate(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

    Rr_ColorTarget OffscreenTarget = {
        .Slot = 0,
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = (Rr_ColorClear){ 0 },
    };
    Rr_GraphNode *OffscreenNode = Rr_AddGraphicsNode(
        Renderer,
        "offscreen",
        1,
        &OffscreenTarget,
        &SwapchainImage,
        NULL,
        NULL);
    Rr_BindGraphicsPipeline(OffscreenNode, GraphicsPipeline);
    Rr_BindVertexBuffer(OffscreenNode, VertexBuffer, 0, 0);
    Rr_BindIndexBuffer(OffscreenNode, IndexBuffer, 0, 0, RR_INDEX_TYPE_UINT32);
    Rr_DrawIndexed(OffscreenNode, 3, 1, 0, 0, 0);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);
    Rr_DestroyBuffer(Renderer, VertexBuffer);
    Rr_DestroyBuffer(Renderer, IndexBuffer);
    Rr_DestroyGraphicsPipeline(Renderer, GraphicsPipeline);
    Rr_DestroyPipelineLayout(Renderer, PipelineLayout);
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
