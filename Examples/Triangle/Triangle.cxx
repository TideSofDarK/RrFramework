#include <Rr/Rr.h>

#include "ExampleAssets.inc"

#include <array>

class CTriangleApp
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Buffer *VertexBuffer{};
    Rr_Buffer *IndexBuffer{};

public:
    CTriangleApp()
    {
        auto VertexAttributes = std::array{
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
            Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_FLOAT3 },
        };

        auto VertexInputBindings = std::array{
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
            },
        };

        auto ColorTarget = Rr_ColorTargetInfo{
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_TRIANGLE_FRAG_SPV);
        auto FragmentShaderInfo = Rr_ShaderInfo{
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        auto PipelineInfo = Rr_GraphicsPipelineCreateInfo{
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .VertexInputBindingCount = VertexInputBindings.size(),
            .VertexInputBindings = VertexInputBindings.data(),
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        auto constexpr VERTEX_DATA = std::array{
            -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
            0.0f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,  1.0f,
        };
        auto constexpr VERTEX_DATA_SIZE = VERTEX_DATA.size() * sizeof(decltype(VERTEX_DATA)::value_type);
        VertexBuffer = Rr_CreateBuffer(sizeof(VERTEX_DATA), RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_STAGING);
        std::memcpy(Rr_GetMappedBufferData(VertexBuffer), VERTEX_DATA.data(), sizeof(VERTEX_DATA));

        auto constexpr INDEX_DATA = std::array{ 2u, 1u, 0u };
        auto constexpr INDEX_DATA_SIZE = INDEX_DATA.size() * sizeof(decltype(INDEX_DATA)::value_type);
        IndexBuffer = Rr_CreateBuffer(INDEX_DATA_SIZE, RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_STAGING);
        std::memcpy(Rr_GetMappedBufferData(IndexBuffer), INDEX_DATA.data(), INDEX_DATA_SIZE);
    }

    void Iterate()
    {
        Rr_UIBeginDebugOverlayTabs();
        if (Rr_UIBeginWindow("Triangle.cxx"))
        {
            Rr_UIText("This example shows drawing a simple triangle.");
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto ColorTarget = Rr_ColorTarget{
            .Image = Rr_GetSwapchainImage(),
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, NULL);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, VertexBuffer, 0, 0);
        Rr_BindIndexBuffer(GraphicsNode, IndexBuffer, 0, 0, RR_INDEX_TYPE_UINT32);
        Rr_DrawIndexed(GraphicsNode, 3, 1, 0, 0, 0);
    }

    ~CTriangleApp()
    {
        Rr_ReleaseBuffer(VertexBuffer);
        Rr_ReleaseBuffer(IndexBuffer);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    }
};

int main()
{
    static CTriangleApp *App{};

    auto Config = Rr_Config{
        .WindowTitle = "Triangle",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CTriangleApp(); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
