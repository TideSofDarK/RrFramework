#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include "../../Vendor/stb/stb_image.h"
#include "../../Vendor/stb/stb_image_write.h"
#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_USE_MINIZ    0
#include "tinyexr.h"

#include <algorithm>
#include <array>
#include <print>

using UScancodes = std::array<bool, RR_SCANCODE_COUNT>;

Rr_Image2D *CreateDepthImageFromEXR(float Near, float Far, Rr_AssetRef AssetRef)
{
    Rr_Asset Asset = Rr_LoadAsset(AssetRef);

    const char *Error;

    EXRVersion Version;
    int32_t Result = ParseEXRVersionFromMemory(
        &Version,
        (unsigned char *)Asset.Pointer,
        Asset.Size);
    if (Result != 0)
    {
        std::println("Error opening EXR file!");
    }

    EXRHeader Header;
    Result = ParseEXRHeaderFromMemory(
        &Header,
        &Version,
        (unsigned char *)Asset.Pointer,
        Asset.Size,
        &Error);
    if (Result != 0)
    {
        std::println("Error opening EXR file: %s", Error);
        std::abort();
    }

    EXRImage Image;
    InitEXRImage(&Image);

    Result = LoadEXRImageFromMemory(
        &Image,
        &Header,
        (unsigned char *)Asset.Pointer,
        Asset.Size,
        &Error);
    if (Result != 0)
    {
        std::println("Error opening EXR file: %s", Error);
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
        float Depth = FarPlusNear / FarMinusNear +
                      ZReciprocal * ((-2.0f * FTimesNear) / (FarMinusNear));
        Depth = (Depth + 1.0f) / 2.0f;
        Depth = std::clamp(Depth, 0.0f, 1.0f);
        *Current = Depth;
    }

    Rr_IntVec2 Extent = {
        .Width = Image.width,
        .Height = Image.height,
    };

    size_t DataSize = Extent.Width * Extent.Height * sizeof(float);

    Rr_Image2D *DepthImage = Rr_CreateImage2D(
        Extent,
        RR_TEXTURE_FORMAT_D32_SFLOAT,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        DataSize,
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    std::memcpy(
        Rr_GetMappedBufferData(StagingBuffer),
        Image.images[0],
        DataSize);

    Rr_CopyBufferToImage2D(
        Rr_GetGraph(),
        "copy",
        StagingBuffer,
        0,
        Extent,
        DepthImage,
        0);

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
    char *Data = (char *)stbi_load_from_memory(
        (stbi_uc *)Asset.Pointer,
        (int32_t)Asset.Size,
        &Width,
        &Height,
        &Channels,
        DesiredChannels);

    Rr_Image2D *ColorImage = Rr_CreateImage2D(
        { Width, Height },
        RR_TEXTURE_FORMAT_R8G8B8A8_SRGB,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

    size_t Size = Width * Height * DesiredChannels;

    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        Size,
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

    std::memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, Size);

    Rr_CopyBufferToImage2D(
        Rr_GetGraph(),
        "copy",
        StagingBuffer,
        0,
        { Width, Height },
        ColorImage,
        0);

    Rr_ReleaseBuffer(StagingBuffer);

    return ColorImage;
}

struct SPrerenderedDepthApp
{
    static const Rr_TextureFormat DEPTH_FORMAT = RR_TEXTURE_FORMAT_D32_SFLOAT;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    Rr_Buffer *UniformBuffer;

    Rr_Image2D *ColorImage;
    Rr_Image2D *DepthImage;

    Rr_Image2D *BackgroundColorImage;
    Rr_Image2D *BackgroundDepthImage;
    Rr_IntVec2 BackgroundExtent;

    UScancodes Scancodes{};

    void InitPipeline()
    {
        std::array Bindings = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_UNIFORM_BUFFER,
                RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{
                Bindings.size(),
                Bindings.data(),
            },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat();
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_PRERENDEREDDEPTH_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_PRERENDEREDDEPTH_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.DepthStencil.EnableDepthTest = true;
        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
        PipelineInfo.DepthStencil.Format = DEPTH_FORMAT;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitBackground()
    {
        BackgroundDepthImage = CreateDepthImageFromEXR(
            0.5f,
            50.0f,
            EXAMPLE_ASSET_PRERENDEREDDEPTH_EXR);
        BackgroundColorImage =
            CreateColorImageFromPNG(EXAMPLE_ASSET_PRERENDEREDDEPTH_PNG);
        BackgroundExtent = Rr_GetImage2DExtent(BackgroundColorImage);
    }

    void InitUniform(float Aspect)
    {
        /* NOTE: Hardcoded values from PrerenderedDepth.blend scene. */

        struct
        {
            Rr_Mat4 View;
            Rr_Mat4 Projection;
            float Near;
            float Far;
        } UniformData;

        UniformData.View = Rr_EulerXYZ({ 90.0f - 63.5593f, -46.6919f, 0.0f }) *
                           Rr_Translate({ -7.35889f, -4.0f, -6.92579f });
        UniformData.Projection =
            Rr_Perspective_RH(RR_ANGLE_DEG(43.7927f), Aspect, 0.5f, 50.0f);
        UniformData.Near = 0.5f;
        UniformData.Far = 50.0f;

        UniformBuffer = Rr_CreateBuffer(
            sizeof(UniformData),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT);

        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &UniformData,
            sizeof(UniformData));
    }

    void InitRenderTarget()
    {
        ColorImage = Rr_CreateImage2D(
            BackgroundExtent,
            RR_TEXTURE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT);
        DepthImage = Rr_CreateImage2D(
            BackgroundExtent,
            DEPTH_FORMAT,
            RR_IMAGE_FLAGS_TRANSFER_BIT |
                RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    void Init()
    {
        InitPipeline();
        InitBackground();
        InitUniform(
            (float)BackgroundExtent.Width / (float)BackgroundExtent.Height);
        InitRenderTarget();
    }

    void Iterate()
    {
        Rr_CopyImage2D(
            Rr_GetGraph(),
            "copy_color",
            BackgroundColorImage,
            { 0 },
            ColorImage,
            { 0 },
            BackgroundExtent,
            0);

        Rr_CopyImage2D(
            Rr_GetGraph(),
            "copy_depth",
            BackgroundDepthImage,
            { 0 },
            DepthImage,
            { 0 },
            BackgroundExtent,
            0);

        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
            .Image = ColorImage,
        };
        Rr_DepthTarget DepthTarget = {
            .LoadOp = RR_LOAD_OP_LOAD,
            .StoreOp = RR_STORE_OP_STORE,
            .Image = DepthImage,
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
            Rr_GetGraph(),
            "graphics",
            1,
            &ColorTarget,
            &DepthTarget);

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainExtent = Rr_GetSwapchainSize();

        Rr_BlitImage2D(
            Rr_GetGraph(),
            "blit",
            ColorImage,
            SwapchainImage,
            { 0, 0, BackgroundExtent.Width, BackgroundExtent.Height },
            { 0, 0, SwapchainExtent.Width, SwapchainExtent.Height },
            RR_IMAGE_ASPECT_COLOR_BIT);

        Rr_UIDebugOverlay();
    }

    void Cleanup()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseImage(BackgroundColorImage);
        Rr_ReleaseImage(BackgroundDepthImage);
        Rr_ReleaseImage(ColorImage);
        Rr_ReleaseImage(DepthImage);
    }
};

int main()
{
    static SPrerenderedDepthApp App;

    Rr_AppConfig Config = {};
    Config.Title = "PrerenderedDepth";
    Config.InitFunc = []() { App.Init(); };
    Config.IterateFunc = []() { App.Iterate(); };
    Config.CleanupFunc = []() { App.Cleanup(); };
    Rr_Run(&Config);
}
