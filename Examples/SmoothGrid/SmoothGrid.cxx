#include "ExampleAssets.inc"

#include <Rr/Rr.h>

constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 100.0f;

struct SCamera
{
    float FOVDegrees = 90.0f;
    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position{};

    Rr_Mat4 Transform = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

    void UpdatePerspective(Rr_IntVec2 Size)
    {
        ProjMatrix = Rr_Perspective_RH(RR_ANGLE_DEG(FOVDegrees), (float)Size.X / (float)Size.Y, NEAR_PLANE, FAR_PLANE);
        ProjMatrix.Elements[1][1] *= -1.0f;
    }

    [[nodiscard]] Rr_Mat4 GetViewMatrix() const
    {
        return Rr_InvGeneral(Transform);
    }

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Transform.Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Transform.Columns[0].XYZ);
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
            Yaw -= MouseDelta.X * Sensitivity;
            Pitch -= MouseDelta.Y * Sensitivity;
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, 360.0f);
        Pitch = RR_CLAMP(-90.0f, Pitch, 90.0f);

        Transform = Rr_TranslateV(Position) * Rr_Rotate_RH(RR_ANGLE_DEG(Yaw), Rr_V3(0.0f, 1.0f, 0.0f)) *
                    Rr_Rotate_RH(RR_ANGLE_DEG(Pitch), Rr_V3(1.0f, 0.0f, 0.0f));
    }
};

struct SGPUUniform
{
    Rr_Mat4 View;
    Rr_Mat4 InvView;
    Rr_Mat4 Projection;
    Rr_Mat4 InvProjection;
    float Near;
    float Far;
    float GridSmall;
    float GridBig;
};

class CSmoothGridApp
{
    static constexpr Rr_ImageFormat DEPTH_FORMAT = RR_IMAGE_FORMAT_D32_SFLOAT;

    Rr_GraphicsPipeline *GraphicsPipeline{};

    Rr_Image2D *DepthImage{};
    Rr_Buffer *UniformBuffer{};

    SCamera Camera;

    void InitPipeline()
    {
        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetImageFormat(Rr_GetSwapchainImage());
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_SMOOTHGRID_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_SMOOTHGRID_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.VertexShaderInfo = &VertexShaderInfo;
        PipelineInfo.FragmentShaderInfo = &FragmentShaderInfo;
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.DepthStencil.EnableDepthTest = true;
        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
        PipelineInfo.DepthStencil.Format = DEPTH_FORMAT;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitUniformBuffer()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    void InitDepthImage()
    {
        Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());

        if (DepthImage != nullptr)
        {
            Rr_IntVec2 DepthImageSize = Rr_GetImage2DExtent(DepthImage);

            if (DepthImageSize.X >= SwapchainSize.X && DepthImageSize.Y >= SwapchainSize.Y)
            {
                return;
            }

            Rr_ReleaseImage(DepthImage);
        }

        DepthImage = Rr_CreateImage2D(
            Rr_IntV2(SwapchainSize.Width, SwapchainSize.Height),
            RR_IMAGE_FORMAT_D32_SFLOAT,
            RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
    }

    void InitCamera()
    {
    }

public:
    CSmoothGridApp()
    {
        InitCamera();
        InitPipeline();
        InitUniformBuffer();
        InitDepthImage();

        Camera.Position = Rr_V3(0.0f, 1.0f, 0.0f);
    }

    void Event(Rr_Event const *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                InitDepthImage();
                InitCamera();
                return;
            }
            default:
                return;
        }
    }

    void Iterate()
    {
        Rr_UIBeginDebugOverlayTabs();
        if (Rr_UIBeginWindow("SmoothGrid.cxx"))
        {
            Rr_UIText("This example shows drawing smooth grid using partial derivatives.");
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto Graph = Rr_GetGraph();

        auto SwapchainImage = Rr_GetSwapchainImage();
        auto SwapchainSize = Rr_GetImage2DExtent(SwapchainImage);

        Camera.UpdatePerspective(Rr_GetImage2DExtent(SwapchainImage));
        Camera.Update();

        auto Uniform = SGPUUniform{
            .View = Camera.GetViewMatrix(),
            .InvView = Camera.Transform,
            .Projection = Camera.ProjMatrix,
            .InvProjection = Rr_InvPerspective_RH(Camera.ProjMatrix),
            .Near = NEAR_PLANE,
            .Far = FAR_PLANE,
            .GridSmall = 1.0f,
            .GridBig = 10.0f,
        };
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &Uniform, sizeof(SGPUUniform));

        auto ColorClear = Rr_ColorClear{
            .Vec4 = { 0.007f, 0.007f, 0.017f, 1.0f },
        };
        auto ColorTarget = Rr_ColorTarget{
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = ColorClear,
        };
        auto DepthTarget = Rr_DepthTarget{
            .Image = DepthImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_DepthClear{ 1.0f, 0 },
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(Graph, 1, &ColorTarget, &DepthTarget);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(SGPUUniform));
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);
    }

    ~CSmoothGridApp()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseImage(DepthImage);
    }
};

int main()
{
    static CSmoothGridApp *App{};

    auto Config = Rr_Config{
        .WindowTitle = "SmoothGrid",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CSmoothGridApp(); },
        .EventFunc = [](Rr_Event const *Event) { App->Event(Event); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
