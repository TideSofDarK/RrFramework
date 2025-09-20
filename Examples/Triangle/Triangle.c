#include <Rr/Rr.h>

#include "ExampleAssets.inc"

static Rr_PipelineLayout *PipelineLayout;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Buffer *VertexBuffer;
static Rr_Buffer *IndexBuffer;

static void Init(void)
{
    PipelineLayout = Rr_CreatePipelineLayout(0, NULL);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Format = RR_FORMAT_VEC3, .Location = 0 },
        { .Format = RR_FORMAT_VEC3, .Location = 1 },
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
        Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_VERT_SPV);
    PipelineInfo.FragmentShaderSPV =
        Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_FRAG_SPV);
    PipelineInfo.VertexInputBindingCount = RR_ARRAY_COUNT(VertexInputBindings);
    PipelineInfo.VertexInputBindings = VertexInputBindings;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    float VertexData[] = {
        -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
        0.0f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,  1.0f,
    };

    VertexBuffer = Rr_CreateBuffer(
        sizeof(VertexData),
        RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
            RR_BUFFER_FLAGS_MAPPED_BIT);
    memcpy(
        Rr_GetMappedBufferData(VertexBuffer),
        VertexData,
        sizeof(VertexData));

    uint32_t IndexData[] = {
        2,
        1,
        0,
    };

    IndexBuffer = Rr_CreateBuffer(
        sizeof(IndexData),
        RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
            RR_BUFFER_FLAGS_MAPPED_BIT);
    memcpy(Rr_GetMappedBufferData(IndexBuffer), IndexData, sizeof(IndexData));
}

static void Iterate(void)
{
    Rr_ColorTarget ColorTarget = {
        .Image = Rr_GetSwapchainImage(),
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = (Rr_ColorClear){ 0 },
    };
    Rr_GraphNode *OffscreenNode =
        Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, NULL);
    Rr_BindGraphicsPipeline(OffscreenNode, GraphicsPipeline);
    Rr_BindVertexBuffer(OffscreenNode, VertexBuffer, 0, 0);
    Rr_BindIndexBuffer(OffscreenNode, IndexBuffer, 0, 0, RR_INDEX_TYPE_UINT32);
    Rr_DrawIndexed(OffscreenNode, 3, 1, 0, 0, 0);

    Rr_UIDebugOverlay();
}

static void Cleanup(void)
{
    Rr_ReleaseBuffer(VertexBuffer);
    Rr_ReleaseBuffer(IndexBuffer);
    Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    Rr_ReleasePipelineLayout(PipelineLayout);
}

int main(int ArgC, char **ArgV)
{
    Rr_AppConfig Config = {
        .Title = "Triangle",
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
    };
    Rr_Run(&Config);

    return 0;
}
