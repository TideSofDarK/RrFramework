#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include "../../Vendor/stb/stb_image.h"

#include <array>
#include <cassert>
#include <iostream>

template <size_t ImageCount>
void CreateImagesFromPNGs(
    std::int32_t Width,
    std::int32_t Height,
    const std::array<Rr_AssetRef, ImageCount> &Assets,
    Rr_Image2DArray **OutImage2DArray,
    Rr_Image3D **OutImage3D)
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
            return;
        }

        std::memcpy(StagingData + StagingOffset, Data, LayerSize);
        StagingOffset += LayerSize;

        stbi_image_free(Data);
    }

    Rr_Image2DArray *Image2DArray = Rr_CreateImage2DArray(
        { Width, Height },
        ImageCount,
        RR_TEXTURE_FORMAT_R8G8B8A8_SRGB,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);
    *OutImage2DArray = Image2DArray;
    for (std::uint32_t Index = 0; Index < ImageCount; ++Index)
    {
        Rr_CopyBufferToImage2DArray(
            Rr_GetGraph(),
            StagingBuffer,
            LayerSize * Index,
            { Width, Height },
            Image2DArray,
            Index,
            0);
    }

    Rr_Image3D *Image3D = Rr_CreateImage3D(
        { Width, Height, ImageCount },
        RR_TEXTURE_FORMAT_R8G8B8A8_SRGB,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);
    *OutImage3D = Image3D;
    Rr_CopyBufferToImage3D(
        Rr_GetGraph(),
        StagingBuffer,
        0,
        { Width, Height, (std::int32_t)ImageCount },
        Image3D,
        0);

    Rr_ReleaseBuffer(StagingBuffer);
}

struct SLayeredImageApp
{
    static constexpr std::int32_t IMAGE_WIDTH = 800;
    static constexpr std::int32_t IMAGE_HEIGHT = 600;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *Image2DArrayPipeline;
    Rr_GraphicsPipeline *Image3DPipeline;
    Rr_Sampler *Sampler;
    Rr_Buffer *UniformBuffer;
    Rr_Image2DArray *Image2DArray{};
    Rr_Image3D *Image3D{};

    bool UseImage3D{};

    void InitPipelines()
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
            Rr_LoadAsset(EXAMPLE_ASSET_QUAD_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_IMAGE2DARRAY_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;

        Image2DArrayPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_IMAGE3D_FRAG_SPV);

        Image3DPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitSampler()
    {
        Rr_SamplerInfo SamplerInfo = {};
        SamplerInfo.MagFilter = RR_FILTER_LINEAR;
        SamplerInfo.MinFilter = RR_FILTER_LINEAR;
        SamplerInfo.AddressModeW = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Sampler = Rr_CreateSampler(&SamplerInfo);
    }

    void InitImages()
    {
        CreateImagesFromPNGs(
            IMAGE_WIDTH,
            IMAGE_HEIGHT,
            std::array{
                EXAMPLE_ASSET_IMAGE0_PNG,
                EXAMPLE_ASSET_IMAGE1_PNG,
                EXAMPLE_ASSET_IMAGE2_PNG,
            },
            &Image2DArray,
            &Image3D);

        if (!Image2DArray)
        {
            std::cerr << "Unable to create Image2DArray!\n";
            std::abort();
        }
        if (!Image3D)
        {
            std::cerr << "Unable to create Image3D!\n";
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
        InitPipelines();
        InitImages();
        InitUniformBuffer();
    }

    void Iterate()
    {
        Rr_UIBeginWindow(
            "LayeredImage.cxx",
            NULL,
            RR_UI_WINDOW_FLAGS_AUTO_RESIZE_BIT);
        Rr_UILabel(
            "This example demonstrates sampling of Rr_Image2DArray and "
            "Rr_Image3D objects.");
        Rr_UICheckbox("Use Image3D", &UseImage3D);
        Rr_UIEndWindow();

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainExtent = Rr_GetSwapchainSize();

        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_ColorClear{ 1.0f, 1.0f, 1.0f, 1.0f },
            .Image = SwapchainImage,
        };

        struct
        {
            float Time = (float)Rr_GetTimeSeconds();
            std::uint32_t ImageCount = 3;
        } UniformData;

        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &UniformData,
            sizeof(UniformData));

        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, NULL);

        Rr_Rect ImageRect{ 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT };
        Rr_Rect SwapchainRect{ 0,
                               0,
                               (float)SwapchainExtent.Width,
                               (float)SwapchainExtent.Height };
        Rr_Rect Viewport = Rr_FitRect(&ImageRect, &SwapchainRect);
        Rr_SetViewport(GraphicsNode, &Viewport);

        Rr_BindGraphicsPipeline(
            GraphicsNode,
            UseImage3D ? Image3DPipeline : Image2DArrayPipeline);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, 16);
        if (UseImage3D)
        {
            Rr_BindCombinedImage3DSampler(GraphicsNode, Image3D, Sampler, 0, 1);
        }
        else
        {
            Rr_BindCombinedImage2DArraySampler(
                GraphicsNode,
                Image2DArray,
                Sampler,
                0,
                1);
        }

        Rr_Draw(GraphicsNode, 6, 1, 0, 0);
    }

    void Cleanup()
    {
        Rr_ReleaseGraphicsPipeline(Image2DArrayPipeline);
        Rr_ReleaseGraphicsPipeline(Image3DPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseImage(Image2DArray);
        Rr_ReleaseImage(Image3D);
        Rr_ReleaseSampler(Sampler);
    }
};

int main()
{
    static SLayeredImageApp App;

    Rr_AppConfig Config = {};
    Config.Title = "LayeredImage";
    Config.InitFunc = []() { App.Init(); };
    Config.IterateFunc = []() { App.Iterate(); };
    Config.CleanupFunc = []() { App.Cleanup(); };
    Rr_Run(&Config);
}
