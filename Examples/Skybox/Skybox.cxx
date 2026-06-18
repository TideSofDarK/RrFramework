#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

class CCamera
{
    float Near{ 0.01f };
    float Far{ 100.0f };
    float FOVDegrees{ 90.0f };
    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position;

    Rr_Mat4 ViewMatrix{ Rr_M4D(1.0f) };
    Rr_Mat4 ProjMatrix{ Rr_M4D(1.0f) };

public:
    Rr_Mat4 GetProjectionMatrix() const
    {
        return ProjMatrix;
    }

    Rr_Mat4 GetViewMatrix() const
    {
        return ViewMatrix;
    }

    Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
    }

    Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
    }

    void UpdatePerspective(float Aspect)
    {
        ProjMatrix = Rr_Perspective_RH(RR_ANGLE_DEG(FOVDegrees), Aspect, Near, Far);
    }

    void Update()
    {
        auto DeltaTime = Rr_GetDeltaSeconds();

        auto MouseDelta = Rr_GetMousePositionDelta();

        if (Rr_GetMouseState() & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);

            auto constexpr SPEED = 5.0f;
            auto CameraForward = GetForwardVector();
            auto CameraLeft = GetRightVector();
            if (Rr_IsScancodePressed(RR_SCANCODE_W))
            {
                Position -= CameraForward * SPEED * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_A))
            {
                Position -= CameraLeft * SPEED * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_S))
            {
                Position += CameraForward * SPEED * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_D))
            {
                Position += CameraLeft * SPEED * DeltaTime;
            }

            auto constexpr SENSITIVITY = 0.2f;
            Yaw += MouseDelta.X * SENSITIVITY;
            Pitch += MouseDelta.Y * SENSITIVITY;
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, 360.0f);
        Pitch = RR_CLAMP(-90.0f, Pitch, 90.0f);

        auto CosPitch = cosf(Pitch * RR_DEG_TO_RAD);
        auto SinPitch = sinf(Pitch * RR_DEG_TO_RAD);
        auto CosYaw = cosf(Yaw * RR_DEG_TO_RAD);
        auto SinYaw = sinf(Yaw * RR_DEG_TO_RAD);

        auto XAxis = Rr_Vec3{ CosYaw, 0.0f, -SinYaw };
        auto YAxis = Rr_Vec3{ SinYaw * SinPitch, CosPitch, CosYaw * SinPitch };
        auto ZAxis = Rr_Vec3{ SinYaw * CosPitch, -SinPitch, CosPitch * CosYaw };

        ViewMatrix.Columns[0] = { XAxis.X, YAxis.X, ZAxis.X, 0.0f };
        ViewMatrix.Columns[1] = { XAxis.Y, YAxis.Y, ZAxis.Y, 0.0f };
        ViewMatrix.Columns[2] = { XAxis.Z, YAxis.Z, ZAxis.Z, 0.0f };
        ViewMatrix.Columns[3] = { -Rr_Dot(XAxis, Position), -Rr_Dot(YAxis, Position), -Rr_Dot(ZAxis, Position), 1.0f };
        ViewMatrix = Rr_VulkanMatrix() * ViewMatrix;
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

        Camera.UpdatePerspective(Rr_GetImage2DAspect(SwapchainImage));
        Camera.Update();

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
