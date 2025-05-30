#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <print>

#include "../../Vendor/stb/stb_image.h"

using UScancodes = std::array<bool, RR_SCANCODE_COUNT>;

struct SCamera
{
    float Pitch{};
    float Yaw{};
    float Near;
    float Far;
    float Aspect;
    float FOVDegrees;
    Rr_Vec3 Position;

    Rr_Mat4 ViewMatrix = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

    void UpdatePerspective()
    {
        ProjMatrix =
            Rr_Perspective_RH_ZO(RR_ANGLE_DEG(FOVDegrees), Aspect, Near, Far);
    }

    void UpdateView()
    {
        float CosPitch = cosf(Pitch * RR_DEG_TO_RAD);
        float SinPitch = sinf(Pitch * RR_DEG_TO_RAD);
        float CosYaw = cosf(Yaw * RR_DEG_TO_RAD);
        float SinYaw = sinf(Yaw * RR_DEG_TO_RAD);

        Rr_Vec3 XAxis{ CosYaw, 0.0f, -SinYaw };
        Rr_Vec3 YAxis{ SinYaw * SinPitch, CosPitch, CosYaw * SinPitch };
        Rr_Vec3 ZAxis{ SinYaw * CosPitch, -SinPitch, CosPitch * CosYaw };

        ViewMatrix.Columns[0] = { XAxis.X, YAxis.X, ZAxis.X, 0.0f };
        ViewMatrix.Columns[1] = { XAxis.Y, YAxis.Y, ZAxis.Y, 0.0f };
        ViewMatrix.Columns[2] = { XAxis.Z, YAxis.Z, ZAxis.Z, 0.0f };
        ViewMatrix.Columns[3] = { -Rr_Dot(XAxis, Position),
                                  -Rr_Dot(YAxis, Position),
                                  -Rr_Dot(ZAxis, Position),
                                  1.0f };
        ViewMatrix = Rr_VulkanMatrix() * ViewMatrix;
    }

    SCamera(
        Rr_Vec3 Position,
        float FOVDegrees,
        Rr_IntVec2 Size,
        float Near,
        float Far)
        : Position(Position)
        , FOVDegrees(FOVDegrees)
        , Aspect((float)Size.X / (float)Size.Y)
        , Near(Near)
        , Far(Far)
    {
        UpdatePerspective();
        UpdateView();
    }

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
    }

    void Update(const UScancodes &Scancodes)
    {
        float DeltaTime = Rr_GetDeltaSeconds();

        Rr_Vec2 MouseDelta = Rr_GetMousePositionDelta();

        if (Rr_GetMouseState() & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);

            constexpr float CameraSpeed = 5.0f;
            Rr_Vec3 CameraForward = GetForwardVector();
            Rr_Vec3 CameraLeft = GetRightVector();
            if (Scancodes[RR_SCANCODE_W])
            {
                Position -= CameraForward * CameraSpeed * DeltaTime;
            }
            if (Scancodes[RR_SCANCODE_A])
            {
                Position -= CameraLeft * CameraSpeed * DeltaTime;
            }
            if (Scancodes[RR_SCANCODE_S])
            {
                Position += CameraForward * CameraSpeed * DeltaTime;
            }
            if (Scancodes[RR_SCANCODE_D])
            {
                Position += CameraLeft * CameraSpeed * DeltaTime;
            }

            constexpr float Sensitivity = 0.2f;
            Yaw += MouseDelta.X * Sensitivity;
            Pitch += MouseDelta.Y * Sensitivity;
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, 360.0f);
        Pitch = RR_CLAMP(-90.0f, Pitch, 90.0f);

        UpdateView();
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
        Rr_Asset Asset = Rr_LoadAsset(AssetRef);
        int32_t DesiredChannels = 4;
        Data = stbi_load_from_memory(
            (stbi_uc *)Asset.Pointer,
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
    SPNGImage(const SPNGImage &) = default;
    SPNGImage(SPNGImage &&) = delete;
    SPNGImage &operator=(const SPNGImage &) = default;
    SPNGImage &operator=(SPNGImage &&) = delete;
};

struct SCubemap
{
    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    Rr_Buffer *UniformBuffer;
    Rr_Buffer *StagingBuffer;
    Rr_ImageCube *CubemapImage;
    Rr_Sampler *Sampler;

    SCamera Camera;

    UScancodes Scancodes{};

    void InitPipeline()
    {
        std::array Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
            Rr_PipelineBinding{
                1,
                1,
                RR_PIPELINE_BINDING_TYPE_COMBINED_IMAGE_SAMPLER },
        };
        std::array Sets = {
            Rr_PipelineBindingSet{
                Bindings.size(),
                Bindings.data(),
                RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
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
            Rr_LoadAsset(EXAMPLE_ASSET_CUBEMAP_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_CUBEMAP_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitUniformBuffer()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    void InitSampler()
    {
        Rr_SamplerInfo Info = {};
        Sampler = Rr_CreateSampler(&Info);
    }

    void InitCubemapImage()
    {
        SPNGImage Front{ EXAMPLE_ASSET_FRONT_PNG };
        SPNGImage Back{ EXAMPLE_ASSET_BACK_PNG };
        SPNGImage Up{ EXAMPLE_ASSET_UP_PNG };
        SPNGImage Down{ EXAMPLE_ASSET_DOWN_PNG };
        SPNGImage Right{ EXAMPLE_ASSET_RIGHT_PNG };
        SPNGImage Left{ EXAMPLE_ASSET_LEFT_PNG };

        int32_t Width = Up.Width;
        int32_t Height = Up.Height;

        int32_t LayerSize = Width * Height * 4;

        CubemapImage = Rr_CreateCubemap(
            { Width, Height },
            RR_TEXTURE_FORMAT_R8G8B8A8_UNORM,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

        StagingBuffer = Rr_CreateBuffer(
            Width * Height * 4 * 6,
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

        char *StagingData = (char *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(StagingData + (LayerSize * 0), Front.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 1), Back.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 2), Up.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 3), Down.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 4), Right.Data, LayerSize);
        std::memcpy(StagingData + (LayerSize * 5), Left.Data, LayerSize);

        Rr_AddCopyBufferToImageCubeNodeEx(
            Rr_GetGraph(),
            "copy",
            StagingBuffer,
            (LayerSize * 0),
            CubemapImage,
            RR_IMAGE_CUBE_FACE_FIRST,
            RR_IMAGE_CUBE_FACE_LAST,
            0);
    }

    SCubemap()
        : Camera(
              Rr_V3(0.0f, 1.0f, 0.0f),
              90.0f,
              Rr_GetSwapchainSize(),
              0.01f,
              100.0f)
    {
        InitPipeline();
        InitUniformBuffer();
        InitSampler();
        InitCubemapImage();
    }

    void Event(Rr_Event *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_KEY_DOWN:
            case RR_EVENT_TYPE_KEY_UP:
            {
                Scancodes[Event->Key.Scancode] = Event->Key.Down;
                return;
            }
            default:
                return;
        }
    }

    void Iterate()
    {
        Rr_Graph *Graph = Rr_GetGraph();

        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize();

        Camera.Aspect = (float)SwapchainSize.X / (float)SwapchainSize.Y;
        Camera.UpdatePerspective();
        Camera.Update(Scancodes);

        SGPUUniform Uniform = {
            .View = Camera.ViewMatrix,
            .Projection = Camera.ProjMatrix,
        };
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &Uniform,
            sizeof(SGPUUniform));

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

        Rr_ColorClear ColorClear = {};
        ColorClear.Vec4 = { 13.0f / 255.0f,
                            14.0f / 255.0f,
                            28.0f / 255.0f,
                            1.0f };
        Rr_ColorTarget ColorTarget = {
            .Slot = 0,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = ColorClear,
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
            Graph,
            "grid",
            1,
            &ColorTarget,
            &SwapchainImage,
            NULL,
            NULL);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SGPUUniform));
        Rr_BindCombinedCubemapSampler(
            GraphicsNode,
            CubemapImage,
            Sampler,
            0,
            1);
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);

        Rr_UIDebugOverlay();
    }

    ~SCubemap()
    {
        Rr_DestroyGraphicsPipeline(GraphicsPipeline);
        Rr_DestroyPipelineLayout(PipelineLayout);
        Rr_DestroyBuffer(UniformBuffer);
        Rr_DestroyBuffer(StagingBuffer);
        Rr_DestroySampler(Sampler);
        Rr_DestroyCubemap(CubemapImage);
    }
};

static void Init(void *UserData)
{
    new (UserData) SCubemap();
}

static void Event(void *UserData, Rr_Event *Event)
{
    auto SmoothGrid = std::bit_cast<SCubemap *>(UserData);
    SmoothGrid->Event(Event);
}

static void Iterate(void *UserData)
{
    auto SmoothGrid = std::bit_cast<SCubemap *>(UserData);
    SmoothGrid->Iterate();
}

static void Cleanup(void *UserData)
{
    auto SmoothGrid = std::bit_cast<SCubemap *>(UserData);
    SmoothGrid->~SCubemap();
}

int main()
{
    alignas(SCubemap) std::array<std::byte, sizeof(SCubemap)> SmoothGrid;
    Rr_AppConfig Config = {};
    Config.Title = "SmoothGrid";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.smoothgrid";
    Config.InitFunc = Init;
    Config.EventFunc = Event;
    Config.IterateFunc = Iterate;
    Config.CleanupFunc = Cleanup;
    Config.UserData = SmoothGrid.data();
    Rr_Run(&Config);
}
