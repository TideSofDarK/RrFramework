#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

class CCamera
{
    float FieldOfView{ RR_ANGLE_DEG(90.0f) };
    float Pitch{};
    float Yaw{};

    Rr_Mat4 Transform = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

public:
    Rr_Mat4 GetProjectionMatrix() const
    {
        return ProjMatrix;
    }

    Rr_Mat4 GetViewMatrix() const
    {
        return Rr_InvGeneral(Transform);
    }

    void Update(float Aspect)
    {
        auto DeltaTime = Rr_GetDeltaSeconds();
        auto MouseDelta = Rr_GetMousePositionDelta();

        if (Rr_GetMouseState() & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);

            auto constexpr SENSITIVITY = 0.005f;
            Yaw -= MouseDelta.X * SENSITIVITY;
            Pitch -= MouseDelta.Y * SENSITIVITY;
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, RR_PI32 * 2.0f);
        Pitch = RR_CLAMP(RR_PI32 * -0.5f, Pitch, RR_PI32 * 0.5f);

        Transform = Rr_Rotate_RH(Yaw, Rr_V3(0.0f, 1.0f, 0.0f)) * Rr_Rotate_RH(Pitch, Rr_V3(1.0f, 0.0f, 0.0f));
        ProjMatrix = Rr_Perspective_RH(FieldOfView, Aspect, 0.1f, 100.0f);
        ProjMatrix.Elements[1][1] *= -1.0f;
    }
};

struct SGPUUniform
{
    Rr_Mat4 View;
    Rr_Mat4 Projection;
};

struct SPNGImage
{
    int32_t Channels;
    int32_t Width;
    int32_t Height;
    void *Data;

    SPNGImage(Rr_AssetRef AssetRef)
    {
        auto Asset = Rr_LoadAsset(AssetRef);
        auto DesiredChannels = 4;
        Data = stbi_load_from_memory(
            (stbi_uc *)Asset.Data,
            (int32_t)Asset.Size,
            (int32_t *)&Width,
            (int32_t *)&Height,
            &Channels,
            DesiredChannels);
    }

    ~SPNGImage()
    {
        stbi_image_free(Data);
    }

    SPNGImage(const SPNGImage &) = delete;
    SPNGImage(SPNGImage &&) = delete;
    SPNGImage &operator=(const SPNGImage &) = delete;
    SPNGImage &operator=(SPNGImage &&) = delete;
};

class CSkyboxApp
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *StagingBuffer{};
    Rr_ImageCube *SkyboxImage{};
    Rr_Sampler *Sampler{};
    CCamera Camera;

public:
    CSkyboxApp()
    {
        auto SamplerInfo = Rr_SamplerInfo{
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
        };
        Sampler = Rr_CreateSampler(&SamplerInfo);

        auto ColorTarget = Rr_ColorTargetInfo{
            .Blend = Rr_AlphaBlend(),
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_SKYBOX_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_SKYBOX_FRAG_SPV);
        auto FragmentShaderInfo = Rr_ShaderInfo{
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        auto PipelineInfo = Rr_GraphicsPipelineCreateInfo{
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
            .Rasterizer =
                Rr_Rasterizer{
                    .CullMode = RR_CULL_MODE_NONE,
                },
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        UniformBuffer = Rr_CreateBuffer(sizeof(SGPUUniform), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);

        auto Right = SPNGImage{ EXAMPLE_ASSET_RIGHT_PNG };
        auto Left = SPNGImage{ EXAMPLE_ASSET_LEFT_PNG };
        auto Up = SPNGImage{ EXAMPLE_ASSET_UP_PNG };
        auto Down = SPNGImage{ EXAMPLE_ASSET_DOWN_PNG };
        auto Front = SPNGImage{ EXAMPLE_ASSET_FRONT_PNG };
        auto Back = SPNGImage{ EXAMPLE_ASSET_BACK_PNG };

        auto Width = Up.Width;
        auto Height = Up.Height;

        auto LayerSize = Width * Height * 4;

        SkyboxImage = Rr_CreateImageCube(
            { Width, Height },
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

        StagingBuffer = Rr_CreateBuffer(Width * Height * 4 * 6, RR_BUFFER_FLAGS_STAGING);

        auto StagingData = (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(StagingData + (LayerSize * 0), Right.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 1), Left.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 2), Up.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 3), Down.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 4), Front.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 5), Back.Data, LayerSize);

        Rr_CopyBufferToImageCubeEx(
            Rr_GetGraph(),
            StagingBuffer,
            (LayerSize * 0),
            { Width, Height },
            SkyboxImage,
            RR_IMAGE_CUBE_FACE_FIRST,
            RR_IMAGE_CUBE_FACE_LAST,
            0);
    }

    void Iterate()
    {
        Rr_UIBeginDebugOverlayTabs();
        Rr_UIBeginWindow("PrerenderedDepth.cxx");
        Rr_UIText("This example shows how to load and sample a cube image.");
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto Graph = Rr_GetGraph();

        auto SwapchainImage = Rr_GetSwapchainImage();

        Camera.Update(Rr_GetImage2DAspect(SwapchainImage));

        auto Uniform = SGPUUniform{
            .View = Camera.GetViewMatrix(),
            .Projection = Camera.GetProjectionMatrix(),
        };
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &Uniform, sizeof(SGPUUniform));

        auto ColorTarget = Rr_ColorTarget{
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = { Rr_V4(13.0f / 255.0f, 14.0f / 255.0f, 28.0f / 255.0f, 1.0f) },
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(SGPUUniform));
        Rr_BindCombinedImageCubeSampler(GraphicsNode, SkyboxImage, Sampler, 0, 1);
        Rr_Draw(GraphicsNode, 3, 1, 0, 0);
    }

    ~CSkyboxApp()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(StagingBuffer);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseImage(SkyboxImage);
    }
};

int main()
{
    static CSkyboxApp *App{};

    auto Config = Rr_Config{
        .WindowTitle = "Skybox",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CSkyboxApp(); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
