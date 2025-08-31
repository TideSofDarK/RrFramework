#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include "../../Vendor/stb/stb_image.h"

#include <array>

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

struct SGPUUniform
{
    float Mix;
    float Time;
    float Smoothstep;
};

struct SBrushFadeApp
{
    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;
    Rr_Sampler *Sampler;

    Rr_Buffer *UniformBuffer;

    Rr_Image2D *FadeMaskImage;
    Rr_Image2D *ColorMaskImage;

    void InitPipeline()
    {
        std::array Bindings = {
            Rr_Binding{
                0,
                RR_BINDING_TYPE_UNIFORM_BUFFER,
                RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
            },
            Rr_Binding{
                1,
                RR_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
                2,
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
            Rr_LoadAsset(EXAMPLE_ASSET_BRUSHFADE_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_BRUSHFADE_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitSampler()
    {
        Rr_SamplerInfo SamplerInfo = {};
        Sampler = Rr_CreateSampler(&SamplerInfo);
    }

    void InitImages()
    {
        FadeMaskImage = CreateColorImageFromPNG(EXAMPLE_ASSET_FADEMASK_PNG);
        ColorMaskImage = CreateColorImageFromPNG(EXAMPLE_ASSET_COLORMASK_PNG);
    }

    void InitUniform()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                RR_BUFFER_FLAGS_PER_FRAME_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    }

    void Init()
    {
        InitSampler();
        InitPipeline();
        InitImages();
        InitUniform();
    }

    void Iterate()
    {
        static SGPUUniform Uniform = { 0.3f, 0.0f, 0.01f };
        Rr_UIBeginWindow("BrushFade", NULL, 0);
        Rr_UISliderFloat("Mix", &Uniform.Mix, 0.0f, 1.0f);
        Rr_UISliderFloat("Time", &Uniform.Time, 0.0f, 1.0f);
        Rr_UISliderFloat("Smoothstep", &Uniform.Smoothstep, 0.0f, 1.0f);
        Rr_UIEndWindow();
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &Uniform,
            sizeof(Uniform));

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainExtent = Rr_GetSwapchainSize();

        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_ColorClear{ 1.0f, 1.0f, 1.0f, 1.0f },
            .Image = SwapchainImage,
        };

        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
            Rr_GetGraph(),
            "graphics",
            1,
            &ColorTarget,
            NULL);

        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SGPUUniform));
        std::array Masks = { FadeMaskImage, ColorMaskImage };
        for (std::uint32_t Index = 0; Index < Masks.size(); ++Index)
        {
            Rr_BindCombinedImage2DSamplerAt(
                GraphicsNode,
                Masks[Index],
                Sampler,
                0,
                1,
                Index);
        }
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);
    }

    void Cleanup()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleasePipelineLayout(PipelineLayout);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseImage(FadeMaskImage);
        Rr_ReleaseImage(ColorMaskImage);
        Rr_ReleaseSampler(Sampler);
    }
};

int main()
{
    static SBrushFadeApp App;

    Rr_AppConfig Config = {};
    Config.Title = "BrushFade";
    Config.InitFunc = []() { App.Init(); };
    Config.IterateFunc = []() { App.Iterate(); };
    Config.CleanupFunc = []() { App.Cleanup(); };
    Rr_Run(&Config);
}
