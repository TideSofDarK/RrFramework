#include "ExampleAssets.inc"

#include <Rr/Rr.h>

constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 100.0f;

class CCamera
{
    float FieldOfView{ RR_ANGLE_DEG(90.0f) };
    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position{ 0.0f, 1.0f, 0.0f };

    Rr_Mat4 Transform = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

public:
    Rr_Mat4 GetProjectionMatrix() const
    {
        return ProjMatrix;
    }

    Rr_Mat4 GetViewMatrix() const
    {
        return Rr_InvGeneralM4(Transform);
    }

    Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Transform.Columns[2].XYZ);
    }

    Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Transform.Columns[0].XYZ);
    }

    void Update(float Aspect)
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

        Transform = Rr_TranslateV(Position) * Rr_Rotate_RH(Yaw, Rr_V3(0.0f, 1.0f, 0.0f)) *
                    Rr_Rotate_RH(Pitch, Rr_V3(1.0f, 0.0f, 0.0f));
        ProjMatrix = Rr_Perspective_RH(FieldOfView, Aspect, NEAR_PLANE, FAR_PLANE);
        ProjMatrix.Elements[1][1] *= -1.0f;
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

    CCamera Camera;

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
        auto SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());

        if (DepthImage != nullptr)
        {
            auto DepthImageSize = Rr_GetImage2DExtent(DepthImage);

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

public:
    CSmoothGridApp()
    {
        InitPipeline();
        InitUniformBuffer();
        InitDepthImage();
    }

    void Event(Rr_Event const *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                InitDepthImage();

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

        Camera.Update(Rr_GetImage2DAspect(SwapchainImage));

        auto Uniform = SGPUUniform{
            .View = Camera.GetViewMatrix(),
            .InvView = Rr_InvGeneralM4(Camera.GetViewMatrix()),
            .Projection = Camera.GetProjectionMatrix(),
            .InvProjection = Rr_InvPerspective_RH(Camera.GetProjectionMatrix()),
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
