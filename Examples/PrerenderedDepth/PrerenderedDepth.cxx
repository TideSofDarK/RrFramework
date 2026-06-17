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
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    const char *Error{};

    EXRVersion Version;
    int32_t Result = ParseEXRVersionFromMemory(&Version, (unsigned char *)Asset.Data, Asset.Size);
    if (Result != 0)
    {
        std::abort();
    }

    EXRHeader Header;
    Result = ParseEXRHeaderFromMemory(&Header, &Version, (unsigned char *)Asset.Data, Asset.Size, &Error);
    if (Result != 0)
    {
        std::abort();
    }

    EXRImage Image;
    InitEXRImage(&Image);

    Result = LoadEXRImageFromMemory(&Image, &Header, (unsigned char *)Asset.Data, Asset.Size, &Error);
    if (Result != 0)
    {
        std::abort();
    }

    /* Calculate depth (https://en.wikipedia.org/wiki/Z-buffering) */

    float FarPlusNear = Far + Near;
    float FarMinusNear = Far - Near;
    float FTimesNear = Far * Near;
    for (int32_t Index = 0; Index < Image.width * Image.height; Index++)
    {
        float *Current = (float *)Image.images[0] + Index;
        float ZReciprocal = 1.0f / *Current;
        float Depth = FarPlusNear / FarMinusNear + ZReciprocal * ((-2.0f * FTimesNear) / (FarMinusNear));
        Depth = (Depth + 1.0f) / 2.0f;
        Depth = std::clamp(Depth, 0.0f, 1.0f);
        *Current = Depth;
    }

    Rr_IntVec2 Extent = {
        .Width = Image.width,
        .Height = Image.height,
    };

    size_t DataSize = Extent.Width * Extent.Height * sizeof(float);

    Rr_Image2D *DepthImage =
        Rr_CreateImage2D(Extent, RR_IMAGE_FORMAT_D32_SFLOAT, RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(DataSize, RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    std::memcpy(Rr_GetMappedBufferData(StagingBuffer), Image.images[0], DataSize);

    Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, Extent, DepthImage, 0);

    FreeEXRHeader(&Header);
    FreeEXRImage(&Image);

    Rr_ReleaseBuffer(StagingBuffer);

    return DepthImage;
}

Rr_Image2D *CreateColorImageFromPNG(Rr_AssetRef AssetRef)
{
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);
    int32_t DesiredChannels = 4;
    int32_t Width, Height, Channels;
    char *Data = (char *)
        stbi_load_from_memory((stbi_uc *)Asset.Data, (int32_t)Asset.Size, &Width, &Height, &Channels, DesiredChannels);

    Rr_Image2D *ColorImage = Rr_CreateImage2D(
        { Width, Height },
        RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

    size_t Size = Width * Height * DesiredChannels;

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(Size, RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    std::memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, Size);

    Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, { Width, Height }, ColorImage, 0);

    Rr_ReleaseBuffer(StagingBuffer);

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
        size_t TotalSize = sizeof(CubePositions) + sizeof(CubeIndices);
        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
        Rr_ReleaseBuffer(StagingBuffer);
        char *BufferData = (char *)Rr_GetMappedBufferData(StagingBuffer);
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
    static const Rr_ImageFormat DEPTH_FORMAT = RR_IMAGE_FORMAT_D32_SFLOAT;

    Rr_GraphicsPipeline *GraphicsPipeline;
    SCube Cube{};

    Rr_Buffer *UniformBuffer;
    struct
    {
        Rr_Mat4 Model;
        Rr_Mat4 View;
        Rr_Mat4 Projection;
        float Near;
        float Far;
    } UniformData;

    Rr_Image2D *ColorImage;
    Rr_Image2D *DepthImage;

    Rr_Image2D *BackgroundColorImage;
    Rr_Image2D *BackgroundDepthImage;
    Rr_IntVec2 BackgroundExtent;

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

        Rr_ColorTargetInfo ColorTarget = {
            .Format = RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
        };

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_PRERENDEREDDEPTH_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_PRERENDEREDDEPTH_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {
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
        Rr_CopyImage2D(Rr_GetGraph(), BackgroundColorImage, { 0 }, ColorImage, { 0 }, BackgroundExtent, 0);

        Rr_CopyImage2D(Rr_GetGraph(), BackgroundDepthImage, { 0 }, DepthImage, { 0 }, BackgroundExtent, 0);

        float Time = Rr_GetTimeSeconds() * 1.5f;
        UniformData.Model = Rr_TranslateV(Rr_V3(cosf(Time) * 3.5f - 1.0f, 0.0f, sinf(Time) * 3.0f)) *

                            Rr_Rotate_RH(Time * 3.0f, Rr_V3(0.0f, 1.0f, 0.0f)) * Rr_ScaleV(Rr_V3F(0.5f));

        static Rr_Vec3 Position = Rr_V3(7.35889f, 4.0f, 6.92579f);
        static Rr_Vec3 Rotation = Rr_V3(63.5593f - 90.0f, 46.6919f + 90.0f, 0.0f);
        Rr_UIBeginWindow("PrerenderedDepth.cxx");
        Rr_UIText(
            "This example demonstrates how to implement "
            "prerendered backgrounds.\nCamera is set to exactly match Blender "
            "camera; otherwise, it won't render correctly.");
        Rr_UIInputFloat3("Camera Position", Position.Elements);
        Rr_UIInputFloat3("Camera Rotation", Rotation.Elements);
        Rr_UIEndWindow();
        UniformData.View =
            Rr_EulerXYZ(Rr_V3(RR_ANGLE_DEG(Rotation.X), RR_ANGLE_DEG(Rotation.Y), RR_ANGLE_DEG(Rotation.Z))) *
            Rr_TranslateV(Position);
        UniformData.Projection =
            Rr_Perspective_RH(RR_ANGLE_DEG(25.48), (float)BackgroundExtent.X / (float)BackgroundExtent.Y, 0.5f, 50.0f);
        UniformData.Near = 0.5f;
        UniformData.Far = 50.0f;
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &UniformData, sizeof(UniformData));

        Rr_ColorTarget ColorTarget = {
            .Image = ColorImage,
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
        };
        Rr_DepthTarget DepthTarget = {
            .Image = DepthImage,
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, &DepthTarget);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, Cube.Buffer, 0, 0);
        Rr_BindIndexBuffer(GraphicsNode, Cube.Buffer, 0, Cube.IndexOffset, RR_INDEX_TYPE_UINT16);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, Rr_GetBufferSize(UniformBuffer));
        Rr_DrawIndexed(GraphicsNode, Cube.IndexCount, 1, 0, 0, 0);

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainExtent = Rr_GetImage2DExtent(SwapchainImage);

        Rr_BlitImage2D(
            Rr_GetGraph(),
            ColorImage,
            SwapchainImage,
            { 0, 0, BackgroundExtent.Width, BackgroundExtent.Height },
            { 0, 0, SwapchainExtent.Width, SwapchainExtent.Height },
            RR_IMAGE_ASPECT_COLOR_BIT);
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

    Rr_Config Config = {
        .WindowTitle = "PrerenderedDepth",
        .InitFunc = []() { App.Init(); },
        .IterateFunc = []() { App.Iterate(); },
        .CleanupFunc = []() { App.Cleanup(); },
    };
    Rr_Run(&Config);
}
