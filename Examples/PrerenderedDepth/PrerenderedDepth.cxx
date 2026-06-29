#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image_write.h"

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_USE_MINIZ    0
#include "../../Vendor/tinyexr/tinyexr.h"

#include <algorithm>
#include <array>

Rr_Image2D *CreateDepthImageFromEXR(float Near, float Far, Rr_AssetRef AssetRef)
{
    auto Asset = Rr_LoadAsset(AssetRef);

    char const *Error{};

    auto Version = EXRVersion{};
    auto Result = ParseEXRVersionFromMemory(&Version, (uint8_t *)Asset.Data, Asset.Size);
    if (Result != 0)
    {
        std::abort();
    }

    auto Header = EXRHeader{};
    Result = ParseEXRHeaderFromMemory(&Header, &Version, (uint8_t *)Asset.Data, Asset.Size, &Error);
    if (Result != 0)
    {
        std::abort();
    }

    auto Image = EXRImage{};
    InitEXRImage(&Image);
    Result = LoadEXRImageFromMemory(&Image, &Header, (uint8_t *)Asset.Data, Asset.Size, &Error);
    if (Result != 0)
    {
        std::abort();
    }

    /* Calculate depth (https://en.wikipedia.org/wiki/Z-buffering) */

    auto FarPlusNear = Far + Near;
    auto FarMinusNear = Far - Near;
    auto FTimesNear = Far * Near;
    for (auto Index = 0; Index < Image.width * Image.height; Index++)
    {
        auto Current = (float *)Image.images[0] + Index;
        auto ZReciprocal = 1.0f / *Current;
        auto Depth = FarPlusNear / FarMinusNear + ZReciprocal * ((-2.0f * FTimesNear) / (FarMinusNear));
        Depth = (Depth + 1.0f) / 2.0f;
        Depth = std::clamp(Depth, 0.0f, 1.0f);
        *Current = Depth;
    }

    auto Extent = Rr_IntVec2{
        .Width = Image.width,
        .Height = Image.height,
    };

    auto DataSize = Extent.Width * Extent.Height * sizeof(float);

    auto StagingBuffer = Rr_CreateBuffer(DataSize, RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    std::memcpy(Rr_GetMappedBufferData(StagingBuffer), Image.images[0], DataSize);
    Rr_ReleaseBuffer(StagingBuffer);

    auto DepthImage =
        Rr_CreateImage2D(Extent, RR_IMAGE_FORMAT_D32_SFLOAT, RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);
    Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, Extent, DepthImage, 0);

    FreeEXRHeader(&Header);
    FreeEXRImage(&Image);

    return DepthImage;
}

Rr_Image2D *CreateColorImageFromPNG(Rr_AssetRef AssetRef)
{
    int32_t Width, Height, Channels;
    auto Asset = Rr_LoadAsset(AssetRef);
    auto DesiredChannels = 4;
    auto Data = (std::byte *)
        stbi_load_from_memory((stbi_uc *)Asset.Data, (int32_t)Asset.Size, &Width, &Height, &Channels, DesiredChannels);
    assert(Channels == 4);
    auto Size = Channels * Width * Height;

    auto StagingBuffer = Rr_CreateBuffer(Size, RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    Rr_ReleaseBuffer(StagingBuffer);
    std::memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, Size);
    stbi_image_free(Data);

    auto Extent = Rr_IntV2(Width, Height);
    auto ColorImage = Rr_CreateImage2D(
        Extent,
        RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);
    Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, Extent, ColorImage, 0);

    return ColorImage;
}

struct SCube
{
    static float constexpr CubePositions[] = {
        1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00,
        1.00,  -1.00, -1.00, 1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  -1.00, 1.00,
        1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00,
        -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,
        -1.00, 1.00,  1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,
    };
    static uint16_t constexpr CubeIndices[] = {
        1,  13, 19, 1,  19, 7,  9, 6, 18, 9, 18, 21, 23, 20, 14, 23, 14, 17,
        16, 4,  10, 16, 10, 22, 5, 2, 8,  5, 8,  11, 15, 12, 0,  15, 0,  3,
    };

    Rr_Buffer *Buffer{};
    size_t IndexOffset{};
    size_t IndexCount{};

    void Init()
    {
        auto TotalSize = sizeof(CubePositions) + sizeof(CubeIndices);
        auto StagingBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
        Rr_ReleaseBuffer(StagingBuffer);
        auto BufferData = (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(BufferData, CubePositions, sizeof(CubePositions));
        BufferData += sizeof(CubePositions);
        std::memcpy(BufferData, CubeIndices, sizeof(CubeIndices));

        Buffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_INDEX_BIT);
        auto TransferNode = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(TransferNode, TotalSize, StagingBuffer, 0, Buffer, 0);
        IndexOffset = sizeof(CubePositions);
        IndexCount = sizeof(CubeIndices) / sizeof(*CubeIndices);
    }

    void Cleanup()
    {
        Rr_ReleaseBuffer(Buffer);
    }
};

struct SPrerenderedDepthApp
{
    static auto const DEPTH_FORMAT = RR_IMAGE_FORMAT_D32_SFLOAT;

    Rr_GraphicsPipeline *GraphicsPipeline{};
    SCube Cube{};

    Rr_Buffer *UniformBuffer{};
    struct
    {
        Rr_Mat4 Model;
        Rr_Mat4 View;
        Rr_Mat4 Projection;
        float Near;
        float Far;
    } UniformData;

    Rr_Image2D *ColorImage{};
    Rr_Image2D *DepthImage{};

    Rr_Image2D *BackgroundColorImage{};
    Rr_Image2D *BackgroundDepthImage{};
    Rr_IntVec2 BackgroundExtent{};

    void InitPipeline()
    {
        auto VertexAttributes = std::array{
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
        };

        auto VertexInputBindings = std::array{
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
            },
        };

        auto ColorTarget = Rr_ColorTargetInfo{
            .Format = RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_PRERENDEREDDEPTH_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_PRERENDEREDDEPTH_FRAG_SPV);
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
            .DepthStencil =
                Rr_DepthStencil{
                    .Format = DEPTH_FORMAT,
                    .CompareOp = RR_COMPARE_OP_LESS,
                    .EnableDepthTest = true,
                    .EnableDepthWrite = true,
                },
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitBackground()
    {
        BackgroundDepthImage = CreateDepthImageFromEXR(0.5f, 50.0f, EXAMPLE_ASSET_PRERENDEREDDEPTH_EXR);
        BackgroundColorImage = CreateColorImageFromPNG(EXAMPLE_ASSET_PRERENDEREDDEPTH_PNG);
        BackgroundExtent = Rr_GetImage2DExtent(BackgroundColorImage);
    }

    void InitUniform()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(UniformData),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    void InitRenderTarget()
    {
        ColorImage = Rr_CreateImage2D(
            BackgroundExtent,
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT);
        DepthImage = Rr_CreateImage2D(
            BackgroundExtent,
            DEPTH_FORMAT,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    void Init()
    {
        InitPipeline();
        InitBackground();
        InitUniform();
        InitRenderTarget();
        Cube.Init();
    }

    void Iterate()
    {
        static auto Position = Rr_V3(7.35889f, 4.0f, 6.92579f);
        static auto Rotation = Rr_V3(63.5593f - 90.0f, 46.6919f + 90.0f, 0.0f);

        Rr_UIBeginDebugOverlayTabs();
        Rr_UIBeginWindow("PrerenderedDepth.cxx");
        Rr_UIText(
            "This example shows how to implement "
            "prerendered backgrounds.\nCamera is set to exactly match Blender "
            "camera; otherwise, it won't render correctly.");
        Rr_UIInputFloat3("Camera Position", Position.Elements);
        Rr_UIInputFloat3("Camera Rotation", Rotation.Elements);
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        Rr_CopyImage2D(Rr_GetGraph(), BackgroundColorImage, { 0 }, ColorImage, { 0 }, BackgroundExtent, 0);
        Rr_CopyImage2D(Rr_GetGraph(), BackgroundDepthImage, { 0 }, DepthImage, { 0 }, BackgroundExtent, 0);

        auto Time = Rr_GetTimeSeconds() * 1.5f;
        UniformData.Model = Rr_TranslateV(Rr_V3(cosf(Time) * 3.5f - 1.0f, 0.0f, sinf(Time) * 3.0f)) *
                            Rr_Rotate_RH(Time * 3.0f, Rr_V3(0.0f, 1.0f, 0.0f)) * Rr_ScaleV(Rr_V3F(0.5f));
        UniformData.View =
            Rr_RotateAngles(Rr_V3(RR_ANGLE_DEG(Rotation.X), RR_ANGLE_DEG(Rotation.Y), RR_ANGLE_DEG(Rotation.Z))) *
            Rr_TranslateV(Position);
        UniformData.Projection =
            Rr_Perspective_RH(RR_ANGLE_DEG(25.48), (float)BackgroundExtent.X / (float)BackgroundExtent.Y, 0.5f, 50.0f);
        UniformData.Near = 0.5f;
        UniformData.Far = 50.0f;
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &UniformData, sizeof(UniformData));

        auto ColorTarget = Rr_ColorTarget{
            .Image = ColorImage,
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
        };
        auto DepthTarget = Rr_DepthTarget{
            .Image = DepthImage,
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, &DepthTarget);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, Cube.Buffer, 0, 0);
        Rr_BindIndexBuffer(GraphicsNode, Cube.Buffer, 0, Cube.IndexOffset, RR_INDEX_TYPE_UINT16);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, Rr_GetBufferSize(UniformBuffer));
        Rr_DrawIndexed(GraphicsNode, Cube.IndexCount, 1, 0, 0, 0);

        auto SwapchainImage = Rr_GetSwapchainImage();
        auto SwapchainExtent = Rr_GetImage2DExtent(SwapchainImage);

        Rr_BlitImage2DEx(
            Rr_GetGraph(),
            ColorImage,
            0,
            Rr_IntRect{ 0, 0, BackgroundExtent.Width, BackgroundExtent.Height },
            SwapchainImage,
            0,
            Rr_IntRect{ 0, 0, SwapchainExtent.Width, SwapchainExtent.Height },
            RR_IMAGE_ASPECT_COLOR_BIT,
            RR_FILTER_LINEAR);
    }

    void Cleanup()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseImage(BackgroundColorImage);
        Rr_ReleaseImage(BackgroundDepthImage);
        Rr_ReleaseImage(ColorImage);
        Rr_ReleaseImage(DepthImage);
        Cube.Cleanup();
    }
};

int main()
{
    static SPrerenderedDepthApp App;

    auto Config = Rr_Config{
        .WindowTitle = "PrerenderedDepth",
        .InitFunc = []() { App.Init(); },
        .IterateFunc = []() { App.Iterate(); },
        .CleanupFunc = []() { App.Cleanup(); },
    };
    Rr_Run(&Config);
}
