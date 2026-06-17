#include <Rr/Rr.h>

#include "ExampleAssets.inc"

static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Buffer *VertexBuffer;
static Rr_Buffer *IndexBuffer;

static void Init(void)
{
    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Format = RR_FORMAT_FLOAT3, .Location = 0 },
        { .Format = RR_FORMAT_FLOAT3, .Location = 1 },
    };

    Rr_VertexInputBinding VertexInputBindings[] = {
        {
            .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
            .AttributeCount = RR_ARRAY_COUNT(VertexAttributes),
            .Attributes = VertexAttributes,
        },
    };

    Rr_ColorTargetInfo ColorTargets[1] = {
        {
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        },
    };

    Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_VERT_SPV);
    Rr_ShaderInfo VertexShaderInfo = {
        .SPVSize = VertexShader.Size,
        .SPVData = VertexShader.Data,
    };

    Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_FRAG_SPV);
    Rr_ShaderInfo FragmentShaderInfo = {
        .SPVSize = FragmentShader.Size,
        .SPVData = FragmentShader.Data,
    };

    Rr_GraphicsPipelineCreateInfo PipelineInfo = { 0 };
    PipelineInfo.VertexShaderInfo = &VertexShaderInfo;
    PipelineInfo.FragmentShaderInfo = &FragmentShaderInfo;
    PipelineInfo.VertexInputBindingCount = RR_ARRAY_COUNT(VertexInputBindings);
    PipelineInfo.VertexInputBindings = VertexInputBindings;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

    float VertexData[] = {
        -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };
    VertexBuffer = Rr_CreateBuffer(
        sizeof(VertexData),
        RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    memcpy(Rr_GetMappedBufferData(VertexBuffer), VertexData, sizeof(VertexData));

    uint32_t IndexData[] = { 2, 1, 0 };
    IndexBuffer = Rr_CreateBuffer(
        sizeof(IndexData),
        RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    memcpy(Rr_GetMappedBufferData(IndexBuffer), IndexData, sizeof(IndexData));
}

static void Iterate(void)
{
    Rr_UIBeginDebugOverlayTabs();
    if(Rr_UIBeginWindow("Triangle.c"))
    {
        Rr_UIText("This example shows drawing a simple triangle.");
    }
    Rr_UIEndWindow();
    Rr_UIEndDebugOverlayTabs();

    Rr_ColorTarget ColorTarget = {
        .Image = Rr_GetSwapchainImage(),
        .LoadOp = RR_LOAD_OP_CLEAR,
        .StoreOp = RR_STORE_OP_STORE,
        .Clear = (Rr_ColorClear){ 0 },
    };
    Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, NULL);
    Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
    Rr_BindVertexBuffer(GraphicsNode, VertexBuffer, 0, 0);
    Rr_BindIndexBuffer(GraphicsNode, IndexBuffer, 0, 0, RR_INDEX_TYPE_UINT32);
    Rr_DrawIndexed(GraphicsNode, 3, 1, 0, 0, 0);
}

static void Cleanup(void)
{
    Rr_ReleaseBuffer(VertexBuffer);
    Rr_ReleaseBuffer(IndexBuffer);
    Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
}

int main(int ArgC, char **ArgV)
{
    Rr_Config Config = {
        .WindowTitle = "Triangle",
        .InitFunc = Init,
        .CleanupFunc = Cleanup,
        .IterateFunc = Iterate,
    };
    Rr_Run(&Config);

    return 0;
}
