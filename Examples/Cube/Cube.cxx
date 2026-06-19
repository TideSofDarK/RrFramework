#include <Rr/Rr.h>

#include "ExampleAssets.inc"

#include <array>

struct SGPUUniform
{
    Rr_Mat4 Model;
    Rr_Mat4 View;
    Rr_Mat4 Projection;
};

class CCubeApp
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *GeometryBuffer{};
    size_t ColorsOffset{};
    size_t IndicesOffset{};
    size_t IndexCount{};

public:
    CCubeApp()
    {
        auto VertexAttributes0 = std::array{
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
        };

        auto VertexAttributes1 = std::array{
            Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_FLOAT3 },
        };

        auto VertexInputBindings = std::array{
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes0.size(),
                .Attributes = VertexAttributes0.data(),
            },
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes1.size(),
                .Attributes = VertexAttributes1.data(),
            },
        };

        auto ColorTarget = Rr_ColorTargetInfo{
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_CUBE_FRAG_SPV);
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
            .Rasterizer =
                Rr_Rasterizer{
                    .CullMode = RR_CULL_MODE_BACK,
                    .FrontFace = RR_FRONT_FACE_CLOCKWISE,
                },
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        auto constexpr CUBE_POSITIONS = std::array{
            1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
            1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
            1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,
            -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
        };
        auto constexpr CUBE_COLORS = std::array{
            1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        };
        auto constexpr CUBE_INDICES = std::array{
            1,  13, 19, 1,  19, 7,  9, 6, 18, 9, 18, 21, 23, 20, 14, 23, 14, 17,
            16, 4,  10, 16, 10, 22, 5, 2, 8,  5, 8,  11, 15, 12, 0,  15, 0,  3,
        };
        auto PositionsSize = sizeof(decltype(CUBE_POSITIONS)::value_type) * CUBE_POSITIONS.size();
        auto ColorsSize = sizeof(decltype(CUBE_COLORS)::value_type) * CUBE_COLORS.size();
        auto IndicesSize = sizeof(decltype(CUBE_INDICES)::value_type) * CUBE_INDICES.size();
        auto TotalSize = PositionsSize + ColorsSize + IndicesSize;
        auto StagingBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_STAGING);
        Rr_ReleaseBuffer(StagingBuffer);
        auto StagingData = (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
        ColorsOffset = PositionsSize;
        IndicesOffset = PositionsSize + ColorsSize;
        IndexCount = CUBE_INDICES.size();
        std::memcpy(StagingData, CUBE_POSITIONS.data(), PositionsSize);
        std::memcpy(StagingData + ColorsOffset, CUBE_COLORS.data(), ColorsSize);
        std::memcpy(StagingData + IndicesOffset, CUBE_INDICES.data(), IndicesSize);
        GeometryBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_INDEX_BIT);
        auto TransferNode = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(TransferNode, TotalSize, StagingBuffer, 0, GeometryBuffer, 0);

        UniformBuffer = Rr_CreateBuffer(sizeof(SGPUUniform), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);
    }

    void Iterate()
    {
        Rr_UIBeginDebugOverlayTabs();
        if (Rr_UIBeginWindow("Cube.cxx"))
        {
            Rr_UIText("This example shows drawing a simple cube.");
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto SwapchainImage = Rr_GetSwapchainImage();

        auto GPUUniform = SGPUUniform{
            .Model = Rr_Rotate_RH(Rr_GetTimeSeconds(), Rr_V3(1.0f, 0.0f, 0.0f)) *
                     Rr_Rotate_RH(Rr_GetTimeSeconds(), Rr_V3(0.0f, 1.0f, 0.0f)),
            .View = Rr_Translate(0.0f, 0.0f, -5.0f),
            .Projection = Rr_Perspective_RH(RR_ANGLE_DEG(50.0f), Rr_GetImage2DAspect(SwapchainImage), 0.1f, 100.0f),
        };
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &GPUUniform, sizeof(GPUUniform));

        auto ColorTarget = Rr_ColorTarget{
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, GeometryBuffer, 0, 0);
        Rr_BindVertexBuffer(GraphicsNode, GeometryBuffer, 1, ColorsOffset);
        Rr_BindIndexBuffer(GraphicsNode, GeometryBuffer, 0, IndicesOffset, RR_INDEX_TYPE_UINT32);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(GPUUniform));
        Rr_DrawIndexed(GraphicsNode, IndexCount, 1, 0, 0, 0);
    }

    ~CCubeApp()
    {
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(GeometryBuffer);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    }
};

int main()
{
    static CCubeApp *App{};

    auto Config = Rr_Config{
        .WindowTitle = "Cube",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CCubeApp(); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
