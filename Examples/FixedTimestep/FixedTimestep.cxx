#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <cmath>

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
    Rr_ReleaseBuffer(StagingBuffer);
    std::memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, Size);

    Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, { Width, Height }, ColorImage, 0);

    return ColorImage;
}

struct CFixedTimestep
{
    struct
    {
        Rr_Mat4 Projection;
        Rr_Mat4 Model;
        float Time;
        float Aspect;
        Rr_Vec2 Padding;
    } GPUUniform;

    struct SState
    {
        Rr_Vec2 ImagePosition;

        SState Lerp(float T, SState const &B) const &
        {
            SState State{
                .ImagePosition = Rr_LerpV2(ImagePosition, T, B.ImagePosition),
            };

            return State;
        }
    };

    float UpdateRate{};
    int64_t TargetDeltaTime{};
    int64_t SystemTime{};
    int64_t SystemTimeStart{};
    int64_t GameTime{};
    int64_t Accumulator{};

    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Buffer *UniformBuffer{};
    Rr_Sampler *Sampler{};
    Rr_Image2D *Image{};
    SState CurrentState{};
    SState PreviousState{};

    void SetUpdateRate(float UpdateRate)
    {
        this->UpdateRate = UpdateRate;
        TargetDeltaTime = (int64_t)((double)Rr_GetPerformanceFrequency() / UpdateRate);
    }

    void InitPipeline()
    {
        Rr_ColorTargetInfo ColorTarget = {
            .Blend = Rr_AlphaBlend(),
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_FIXEDTIMESTEP_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_FIXEDTIMESTEP_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
        };
        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitSampler()
    {
        Rr_SamplerInfo SamplerInfo = {
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
        };
        Sampler = Rr_CreateSampler(&SamplerInfo);
    }

    void InitImage()
    {
        Image = CreateColorImageFromPNG(EXAMPLE_ASSET_VULKAN_PNG);
    }

    void InitUniform()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(GPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT);
    }

    CFixedTimestep()
    {
        InitSampler();
        InitPipeline();
        InitImage();
        InitUniform();
        SystemTime = SystemTimeStart = Rr_GetPerformanceCounter();
        SetUpdateRate(144.0f);
    }

    void Draw(SState const &State, Rr_Graph *Graph)
    {
        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainExtent = Rr_GetImage2DExtent(SwapchainImage);
        Rr_Vec2 SwapchainExtentF = Rr_CastV2(SwapchainExtent);

        GPUUniform.Projection = Rr_Orthographic_RH(
            SwapchainExtent.X / -2.0f,
            SwapchainExtent.X / 2.0f,
            SwapchainExtentF.Y / -2.0f,
            SwapchainExtentF.Y / 2.0f,
            -1.0f,
            1.0f);
        GPUUniform.Model =
            Rr_TranslateV(Rr_V3V(State.ImagePosition, 0)) * Rr_ScaleV(Rr_CastV3(Rr_GetImageExtent(Image)));
        float GameTimeSeconds = (double)GameTime / (double)Rr_GetPerformanceFrequency();
        GPUUniform.Time = GameTimeSeconds;
        GPUUniform.Aspect = Rr_GetImage2DAspect(SwapchainImage);
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &GPUUniform, sizeof(GPUUniform));

        Rr_ColorTarget ColorTarget = {
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_ColorClear{ 0.0f, 0.0f, 0.0f, 1.0f },
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, NULL);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(GPUUniform));
        Rr_BindCombinedImage2DSampler(GraphicsNode, Image, Sampler, 0, 1);
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);
    }

    void FixedIterate()
    {
        static double ImageTime = 0.0f;
        ImageTime += 0.05f;
        CurrentState.ImagePosition.X = std::cos(ImageTime) * Rr_GetImage2DExtent(Image).X;
        CurrentState.ImagePosition.Y = std::sin(ImageTime) * Rr_GetImage2DExtent(Image).Y;
    }

    void Iterate()
    {
        int64_t NewSystemTime = Rr_GetPerformanceCounter();
        int64_t DeltaSystemTime = NewSystemTime - SystemTime;
        if (DeltaSystemTime > TargetDeltaTime * 8)
        {
            DeltaSystemTime = TargetDeltaTime;
        }
        SystemTime = NewSystemTime;

        Accumulator += DeltaSystemTime;

        while (Accumulator >= TargetDeltaTime)
        {
            PreviousState = CurrentState;
            FixedIterate();
            GameTime += TargetDeltaTime;
            Accumulator -= TargetDeltaTime;
        }

        auto Alpha = (float)Accumulator / (float)TargetDeltaTime;
        Draw(PreviousState.Lerp(Alpha, CurrentState), Rr_GetGraph());

        Rr_UIBeginDebugOverlayTabs();
        Rr_UIBeginWindowEx("FixedTimestep.cxx", NULL, 0);
        Rr_UIText(
            "This example shows implementation of fixed "
            "timestep.\nOnly works if your render state can be interpolated.");
        Rr_UIInputFloat("Game Time", &GPUUniform.Time);
        float SystemSeconds = (double)(SystemTime - SystemTimeStart) / (double)Rr_GetPerformanceFrequency();
        Rr_UIInputFloat("System Time", &SystemSeconds);
        if (Rr_UIInputFloat("Update Rate", &UpdateRate))
        {
            UpdateRate = RR_CLAMP(30.0f, UpdateRate, 480.0f);
            SetUpdateRate(UpdateRate);
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();
    }

    ~CFixedTimestep()
    {
        Rr_ReleaseImage(Image);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    }
};

int main()
{
    static CFixedTimestep *App{};

    Rr_Config Config = {
        .WindowTitle = "FixedTimestep",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CFixedTimestep(); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
