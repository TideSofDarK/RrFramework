#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include "../../Vendor/stb/stb_image.h"

#include <array>
#include <cassert>
#include <iostream>

template <size_t ImageCount>
Rr_Image3D *CreateColorImage3DFromPNGs(
    std::int32_t Width,
    std::int32_t Height,
    const std::array<Rr_AssetRef, ImageCount> &Assets)
{
    std::uint32_t LayerSize = Width * Height * 4;
    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        LayerSize * ImageCount,
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    char *StagingData = (char *)Rr_GetMappedBufferData(StagingBuffer);
    std::uint32_t StagingOffset{};

    for (auto &AssetRef : Assets)
    {
        auto Asset = Rr_LoadAsset(AssetRef);
        std::int32_t ImageWidth;
        std::int32_t ImageHeight;
        std::int32_t ImageChannels;
        char *Data = (char *)stbi_load_from_memory(
            (stbi_uc *)Asset.Pointer,
            (int32_t)Asset.Size,
            &ImageWidth,
            &ImageHeight,
            &ImageChannels,
            4);

        if (ImageWidth != Width || ImageHeight != Height)
        {
            stbi_image_free(Data);
            Rr_ReleaseBuffer(StagingBuffer);
            return nullptr;
        }

        std::memcpy(StagingData + StagingOffset, Data, LayerSize);
        StagingOffset += LayerSize;

        stbi_image_free(Data);
    }

    Rr_Image3D *Image3D = Rr_CreateImage3D(
        { Width, Height, ImageCount },
        RR_TEXTURE_FORMAT_R8G8B8A8_SRGB,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

    Rr_AddCopyBufferToImage3DNode(
        Rr_GetGraph(),
        "copy",
        StagingBuffer,
        0,
        { Width, Height, (std::int32_t)ImageCount },
        Image3D,
        0);

    Rr_ReleaseBuffer(StagingBuffer);

    return Image3D;
}

struct STransferThreadApp
{
    static constexpr std::int32_t IMAGE_WIDTH = 800;
    static constexpr std::int32_t IMAGE_HEIGHT = 600;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;
    Rr_Sampler *Sampler;
    Rr_Buffer *UniformBuffer;
    Rr_Image3D *Image3D;

    void InitPipeline()
    {
        std::array Bindings = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_UNIFORM_BUFFER,
                RR_SHADER_STAGE_FRAGMENT_BIT,
            },
            Rr_Binding{
                1,
                RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        std::array Sets = {
            Rr_BindingSet{ Bindings.size(), Bindings.data() },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout((uint32_t)Sets.size(), Sets.data());

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat();
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_TRANSFERTHREAD_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_TRANSFERTHREAD_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitSampler()
    {
        Rr_SamplerInfo SamplerInfo = {};
        SamplerInfo.MagFilter = RR_FILTER_LINEAR;
        SamplerInfo.MinFilter = RR_FILTER_LINEAR;
        SamplerInfo.AddressModeW = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Sampler = Rr_CreateSampler(&SamplerInfo);
    }

    void InitImageArray()
    {
        Image3D = CreateColorImage3DFromPNGs(
            IMAGE_WIDTH,
            IMAGE_HEIGHT,
            std::array{
                EXAMPLE_ASSET_IMAGE0_PNG,
                EXAMPLE_ASSET_IMAGE1_PNG,
                EXAMPLE_ASSET_IMAGE2_PNG,
            });
        if (!Image3D)
        {
            std::cerr << "Unable to load images!\n";
            std::abort();
        }
    }

    void InitUniformBuffer()
    {
        UniformBuffer = Rr_CreateBuffer(
            16,
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_UNIFORM_BIT |
                RR_BUFFER_FLAGS_PER_FRAME_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    }

    void Init()
    {
        InitSampler();
        InitPipeline();
        InitImageArray();
        InitUniformBuffer();
    }

    void Iterate()
    {
        Rr_UIDebugOverlay();

        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_ColorClear{ 1.0f, 1.0f, 1.0f, 1.0f },
        };

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainExtent = Rr_GetSwapchainSize();

        struct
        {
            float Time = (float)Rr_GetTimeSeconds();
            std::uint32_t ImageCount = 3;
        } UniformData;

        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &UniformData,
            sizeof(UniformData));

        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
            Rr_GetGraph(),
            "graphics",
            1,
            &ColorTarget,
            &SwapchainImage,
            NULL,
            NULL);

        Rr_Rect ImageRect{ 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT };
        Rr_Rect SwapchainRect{ 0,
                               0,
                               (float)SwapchainExtent.Width,
                               (float)SwapchainExtent.Height };
        Rr_Rect Viewport = Rr_FitRect(&ImageRect, &SwapchainRect);
        Rr_SetViewport(GraphicsNode, &Viewport);

        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, 16);
        Rr_BindCombinedImage3DSampler(GraphicsNode, Image3D, Sampler, 0, 1);

        Rr_Draw(GraphicsNode, 6, 1, 0, 0);
    }

    void Cleanup()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseImage(Image3D);
        Rr_ReleaseSampler(Sampler);
    }
};

int main()
{
    static STransferThreadApp App;

    Rr_AppConfig Config = {};
    Config.Title = "TransferThread";
    Config.InitFunc = []() { App.Init(); };
    Config.IterateFunc = []() { App.Iterate(); };
    Config.CleanupFunc = []() { App.Cleanup(); };
    Rr_Run(&Config);
}
