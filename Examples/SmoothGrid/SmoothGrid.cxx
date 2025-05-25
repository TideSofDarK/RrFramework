#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <print>

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
    float Near;
    float Far;
    float GridSmall;
    float GridBig;
};

struct SSmoothGrid
{
    Rr_Renderer *Renderer;

    static const Rr_TextureFormat DEPTH_FORMAT = RR_TEXTURE_FORMAT_D32_SFLOAT;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    Rr_Image *DepthImage;
    Rr_Buffer *UniformBuffer;

    SCamera Camera;

    UScancodes Scancodes{};

    void InitPipeline()
    {
        std::array Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        };
        std::array Sets = {
            Rr_PipelineBindingSet{
                Bindings.size(),
                Bindings.data(),
                RR_SHADER_STAGE_VERTEX_BIT | RR_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        PipelineLayout = Rr_CreatePipelineLayout(
            Renderer,
            (uint32_t)Sets.size(),
            Sets.data());

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat(Renderer);
        ColorTarget.Blend = Rr_AlphaBlend();

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_SMOOTHGRID_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_SMOOTHGRID_FRAG_SPV);
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.DepthStencil.EnableDepthTest = true;
        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;
        PipelineInfo.DepthStencil.Format = DEPTH_FORMAT;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);
    }

    void InitUniformBuffer()
    {
        UniformBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    void InitDepthImage()
    {
        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
        DepthImage = Rr_CreateImage(
            Renderer,
            { SwapchainSize.X, SwapchainSize.Y, 1 },
            DEPTH_FORMAT,
            RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    SSmoothGrid()
        : Renderer(Rr_GetRenderer())
        , Camera(
              Rr_V3(0.0f, 1.0f, 0.0f),
              90.0f,
              Rr_GetSwapchainSize(Renderer),
              0.01f,
              100.0f)
    {
        InitPipeline();
        InitUniformBuffer();
        InitDepthImage();
    }

    void Event(Rr_Event *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                /* TODO: Recreate depth image! */
                return;
            }
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
        Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);

        Camera.Aspect = (float)SwapchainSize.X / (float)SwapchainSize.Y;
        Camera.UpdatePerspective();
        Camera.Update(Scancodes);

        SGPUUniform Uniform = {
            .View = Camera.ViewMatrix,
            .Projection = Camera.ProjMatrix,
            .Near = Camera.Near,
            .Far = Camera.Far,
            .GridSmall = 1.0f,
            .GridBig = 10.0f,
        };
        std::memcpy(
            Rr_GetMappedBufferData(Renderer, UniformBuffer),
            &Uniform,
            sizeof(SGPUUniform));

        Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

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
        Rr_DepthTarget DepthTarget = {
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_DepthClear(1.0f, 0),
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
            Renderer,
            "grid",
            1,
            &ColorTarget,
            &SwapchainImage,
            &DepthTarget,
            DepthImage);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SGPUUniform));
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);

        Rr_UIDebugOverlay();
    }

    ~SSmoothGrid()
    {
        Rr_DestroyGraphicsPipeline(Renderer, GraphicsPipeline);
        Rr_DestroyPipelineLayout(Renderer, PipelineLayout);
        Rr_DestroyBuffer(Renderer, UniformBuffer);
        Rr_DestroyImage(Renderer, DepthImage);
    }
};

static void Init(void *UserData)
{
    new (UserData) SSmoothGrid();
}

static void Event(void *UserData, Rr_Event *Event)
{
    auto SmoothGrid = std::bit_cast<SSmoothGrid *>(UserData);
    SmoothGrid->Event(Event);
}

static void Iterate(void *UserData)
{
    auto SmoothGrid = std::bit_cast<SSmoothGrid *>(UserData);
    SmoothGrid->Iterate();
}

static void Cleanup(void *UserData)
{
    auto SmoothGrid = std::bit_cast<SSmoothGrid *>(UserData);
    SmoothGrid->~SSmoothGrid();
}

int main()
{
    alignas(SSmoothGrid) std::array<std::byte, sizeof(SSmoothGrid)> SmoothGrid;
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
