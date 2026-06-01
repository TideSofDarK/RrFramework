#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

struct SCube
{
    static float constexpr CubePositions[] = {
        1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00,
        1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00,
        1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,
        1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,
        -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00,
        -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00,
        -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,
        -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,
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
        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
            TotalSize,
            RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
        Rr_ReleaseBuffer(StagingBuffer);
        char *BufferData = (char *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(BufferData, CubePositions, sizeof(CubePositions));
        BufferData += sizeof(CubePositions);
        std::memcpy(BufferData, CubeIndices, sizeof(CubeIndices));

        Buffer = Rr_CreateBuffer(
            TotalSize,
            RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_INDEX_BIT);
        auto TransferNode = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(
            TransferNode,
            TotalSize,
            StagingBuffer,
            0,
            Buffer,
            0);
        IndexOffset = sizeof(CubePositions);
        IndexCount = sizeof(CubeIndices) / sizeof(*CubeIndices);
    }

    void Cleanup()
    {
        Rr_ReleaseBuffer(Buffer);
    }
};

struct SCamera
{
    float Near = 0.01f;
    float Far = 100.0f;
    float FOVDegrees = 90.0f;
    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position;

    Rr_Mat4 ViewMatrix = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

    void UpdatePerspective(float Aspect)
    {
        ProjMatrix =
            Rr_Perspective_RH(RR_ANGLE_DEG(FOVDegrees), Aspect, Near, Far);
    }

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
    }

    void Update()
    {
        float DeltaTime = Rr_GetDeltaSeconds();

        Rr_Vec2 MouseDelta = Rr_GetMousePositionDelta();

        if (Rr_GetMouseState() & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);

            constexpr float CameraSpeed = 5.0f;
            Rr_Vec3 CameraForward = GetForwardVector();
            Rr_Vec3 CameraLeft = GetRightVector();
            if (Rr_IsScancodePressed(RR_SCANCODE_W))
            {
                Position -= CameraForward * CameraSpeed * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_A))
            {
                Position -= CameraLeft * CameraSpeed * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_S))
            {
                Position += CameraForward * CameraSpeed * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_D))
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

    SPNGImage(const SPNGImage &) = delete;
    SPNGImage(SPNGImage &&) = delete;
    SPNGImage &operator=(const SPNGImage &) = delete;
    SPNGImage &operator=(SPNGImage &&) = delete;
};

struct SSkyboxApp
{
    Rr_GraphicsPipeline *GraphicsPipeline;

    Rr_Buffer *UniformBuffer;
    Rr_Buffer *StagingBuffer;
    Rr_ImageCube *SkyboxImage;
    Rr_Sampler *Sampler;

    SCamera Camera;
    SCube Cube;

    void InitPipeline()
    {
        std::array VertexAttributes = {
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_VEC3 },
        };

        std::array VertexInputBindings = {
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
            },
        };

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetImageFormat(Rr_GetSwapchainImage());
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_SKYBOX_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Pointer,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_SKYBOX_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Pointer,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.VertexShaderInfo = &VertexShaderInfo;
        PipelineInfo.FragmentShaderInfo = &FragmentShaderInfo;
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_NONE;
        PipelineInfo.VertexInputBindingCount = VertexInputBindings.size();
        PipelineInfo.VertexInputBindings = VertexInputBindings.data();

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
        SPNGImage Right{ EXAMPLE_ASSET_RIGHT_PNG };
        SPNGImage Left{ EXAMPLE_ASSET_LEFT_PNG };
        SPNGImage Up{ EXAMPLE_ASSET_UP_PNG };
        SPNGImage Down{ EXAMPLE_ASSET_DOWN_PNG };
        SPNGImage Front{ EXAMPLE_ASSET_FRONT_PNG };
        SPNGImage Back{ EXAMPLE_ASSET_BACK_PNG };

        int32_t Width = Up.Width;
        int32_t Height = Up.Height;

        int32_t LayerSize = Width * Height * 4;

        SkyboxImage = Rr_CreateImageCube(
            { Width, Height },
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT);

        StagingBuffer = Rr_CreateBuffer(
            Width * Height * 4 * 6,
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);

        char *StagingData = (char *)Rr_GetMappedBufferData(StagingBuffer);
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

    void InitCamera()
    {
        Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());
        float Aspect = (float)SwapchainSize.Width / SwapchainSize.Height;
        Camera.UpdatePerspective(Aspect);
    }

    void Init()
    {
        InitCamera();
        InitPipeline();
        InitUniformBuffer();
        InitSampler();
        InitCubemapImage();
        Cube.Init();
    }

    void Event(Rr_Event const *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                InitCamera();
                return;
            }
            break;
            default:
                return;
        }
    }

    void Iterate()
    {
        Rr_Graph *Graph = Rr_GetGraph();

        Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());

        Camera.Update();

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
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = ColorClear,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, Cube.Buffer, 0, 0);
        Rr_BindIndexBuffer(
            GraphicsNode,
            Cube.Buffer,
            0,
            Cube.IndexOffset,
            RR_INDEX_TYPE_UINT16);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SGPUUniform));
        Rr_BindCombinedImageCubeSampler(
            GraphicsNode,
            SkyboxImage,
            Sampler,
            0,
            1);
        Rr_DrawIndexed(GraphicsNode, Cube.IndexCount, 1, 0, 0, 0);
    }

    void Cleanup()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(StagingBuffer);
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseImage(SkyboxImage);
        Cube.Cleanup();
    }
};

int main()
{
    static SSkyboxApp App;

    Rr_AppConfig Config = {};
    Config.Title = "Skybox";
    Config.WindowFlags |= RR_WINDOW_FLAGS_RESIZE_BIT;
    Config.InitFunc = []() { App.Init(); };
    Config.EventFunc = [](Rr_Event const *Event) { App.Event(Event); };
    Config.IterateFunc = []() { App.Iterate(); };
    Config.CleanupFunc = []() { App.Cleanup(); };
    Rr_Run(&Config);
}
