#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <array>

Rr_Image2D *LoadImage2D(Rr_AssetRef AssetRef)
{
    int32_t Width, Height, Channels;
    auto Asset = Rr_LoadAsset(AssetRef);
    auto DesiredChannels = 4;
    auto Data = (std::byte *)
        stbi_load_from_memory((stbi_uc *)Asset.Data, (int32_t)Asset.Size, &Width, &Height, &Channels, DesiredChannels);
    assert(Channels = 4);
    auto Size = Channels * Width * Height;

    auto StagingBuffer = Rr_CreateBuffer(Size, RR_BUFFER_FLAGS_STAGING);
    Rr_ReleaseBuffer(StagingBuffer);
    memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, Size);
    stbi_image_free(Data);

    auto Extent = Rr_IntV2(Width, Height);
    auto Image2D = Rr_CreateImage2D(
        Extent,
        RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
        RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_STORAGE_BIT |
            RR_IMAGE_FLAGS_MIP_MAPPED_BIT);
    Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, Extent, Image2D, 0);

    return Image2D;
}

struct SGPUUniform
{
    Rr_Mat4 Projection;
    Rr_Vec2 ImageSize;
};

class CMipGenApp
{
    Rr_ComputePipeline *ComputePipeline{};
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Image2D *Image{};
    Rr_Buffer *UniformBuffer{};
    Rr_Sampler *Sampler{};
    bool UseMips{ true };
    bool UseLanczos3{ true };
    bool InterpolateMips{ true };
    uint32_t LocalSize{};

    void InitSampler()
    {
        if (Sampler)
        {
            Rr_ReleaseSampler(Sampler);
        }

        auto SamplerInfo = Rr_SamplerInfo{
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
            .MipmapMode = InterpolateMips ? RR_SAMPLER_MIPMAP_MODE_LINEAR : RR_SAMPLER_MIPMAP_MODE_NEAREST,
            .AddressModeU = RR_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
            .AddressModeV = RR_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
            .AnisotropyEnable = true,
            .MaxAnisotropy = 8.0f,
        };
        Sampler = Rr_CreateSampler(&SamplerInfo);
    }

    void InitGraphicsPipeline()
    {
        auto ColorTarget = Rr_ColorTargetInfo{
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_MIPGEN_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_MIPGEN_FRAG_SPV);
        auto FragmentShaderInfo = Rr_ShaderInfo{
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        auto PipelineInfo = Rr_GraphicsPipelineCreateInfo{
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitComputePipeline()
    {
        LocalSize = std::sqrt(Rr_GetMaxComputeWorkgroupInvocations());

        auto Specializations = std::array{
            Rr_PipelineSpecialization{
                .ConstantID = 0,
                .Size = sizeof(LocalSize),
                .Data = &LocalSize,
            },
        };

        auto ComputeShader = Rr_LoadAsset(EXAMPLE_ASSET_MIPGEN_COMP_SPV);
        auto ShaderInfo = Rr_ShaderInfo{
            .SPVSize = ComputeShader.Size,
            .SPVData = ComputeShader.Data,
            .SpecializationCount = Specializations.size(),
            .Specializations = Specializations.data(),
        };

        ComputePipeline = Rr_CreateComputePipeline(&ShaderInfo);
    }

    void ComputeMips()
    {
        auto LevelCount = Rr_GetImageLevelCount(Image);
        if (LevelCount == 1)
        {
            return;
        }
        auto ImageExtent = Rr_GetImage2DExtent(Image);
        auto ComputeNode = Rr_AddComputeNode(Rr_GetGraph());
        Rr_BindComputePipeline(ComputeNode, ComputePipeline);
        for (auto LevelIndex = 0; LevelIndex < LevelCount - 1; ++LevelIndex)
        {
            ImageExtent.X /= 2;
            ImageExtent.Y /= 2;
            auto DispatchSize = Rr_IntV3((ImageExtent.X / LocalSize) + 1, (ImageExtent.Y / LocalSize) + 1, 1);
            // Rr_BindStorageImage2DEx(ComputeNode, Image, LevelIndex, 1, 0, 0, 0, false);
            Rr_BindCombinedImage2DSamplerEx(ComputeNode, Image, LevelIndex, 1, Sampler, 0, 0, 0);
            Rr_BindStorageImage2DEx(ComputeNode, Image, LevelIndex + 1, 1, 0, 1, 0, true);
            Rr_Dispatch(ComputeNode, DispatchSize.X, DispatchSize.Y, DispatchSize.Z);
            Rr_ComputeBarrier(ComputeNode);
        }
    }

public:
    CMipGenApp()
    {
        InitSampler();
        InitGraphicsPipeline();
        InitComputePipeline();
        UniformBuffer = Rr_CreateBuffer(sizeof(SGPUUniform), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);
        Image = LoadImage2D(EXAMPLE_ASSET_FABRIC_PNG);
    }

    void Iterate()
    {
        Rr_UIBeginDebugOverlayTabs();
        if (Rr_UIBeginWindow("MipGen.cxx"))
        {
            Rr_UIText("This example shows two ways of generating mip maps.");
            Rr_UICheckbox("Use Mips", &UseMips);
            Rr_UICheckbox("Use Lanczos3", &UseLanczos3);
            if (Rr_UICheckbox("Interpolate Mips", &InterpolateMips))
            {
                InitSampler();
            }
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        if (UseLanczos3)
        {
            ComputeMips();
        }
        else
        {
            Rr_GenerateMipmaps(Rr_GetGraph(), Image);
        }

        auto SwapchainImage = Rr_GetSwapchainImage();
        auto SwapchainSize = Rr_GetImage2DExtent(SwapchainImage);

        auto Animation = std::abs(std::cos(Rr_GetTimeSeconds()));
        auto GPUUniform = SGPUUniform{
            .Projection = Rr_Orthographic_RH(
                -SwapchainSize.X / 2.0f,
                SwapchainSize.X / 2.0f,
                SwapchainSize.Y / 2.0f,
                -SwapchainSize.Y / 2.0f,
                -1.0f,
                1.0f),
            .ImageSize = Rr_CastV2(Rr_GetImage2DExtent(Image)) * Animation,
        };
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &GPUUniform, sizeof(GPUUniform));

        auto ColorTarget = Rr_ColorTarget{
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, NULL);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(GPUUniform));
        if (UseMips)
        {
            /* By default all levels are bound. */

            Rr_BindCombinedImage2DSampler(GraphicsNode, Image, Sampler, 0, 1);
        }
        else
        {
            /* Explicitly bind first level only! */

            Rr_BindCombinedImage2DSamplerEx(GraphicsNode, Image, 0, 1, Sampler, 0, 1, 0);
        }
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);
    }

    ~CMipGenApp()
    {
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseImage(Image);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseComputePipeline(ComputePipeline);
    }
};

int main()
{
    static CMipGenApp *App{};

    auto Config = Rr_Config{
        .WindowTitle = "MipGen",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CMipGenApp(); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
