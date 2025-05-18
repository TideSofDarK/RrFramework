#include "ExampleAssets.inc"

#include "BitonicSorter.hxx"

#include <Rr/Rr.h>

#include <cfloat>
#include <vector>

enum EInputAction
{
    EIA_UP,
    EIA_DOWN,
    EIA_LEFT,
    EIA_RIGHT,
    EIA_FULLSCREEN,
    EIA_DEBUGOVERLAY,
    EIA_TEST,
    EIA_CAMERA,
    EIA_COUNT,
};

struct SCamera
{
    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position{};

    Rr_Mat4 ViewMatrix = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

    float HTanY;
    float HTanX;
    float FocalZ;

    void SetPerspective(
        float FOVDegrees,
        Rr_IntVec2 Size,
        float Near,
        float Far)
    {
        ProjMatrix = Rr_Perspective_LH_ZO(
            RR_ANGLE_DEG(FOVDegrees),
            (float)Size.X / (float)Size.Y,
            Near,
            Far);

        HTanY = tanf(RR_ANGLE_DEG(FOVDegrees) / 2.0);
        HTanX = HTanY / (float)Size.Y * (float)Size.X;
        FocalZ = (float)Size.Y / (2 * HTanY);
    }

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
    }

    void Update(Rr_InputState *State)
    {
        float DeltaTime = Rr_GetDeltaSeconds();

        Rr_KeyStates Keys = State->Keys;

        Rr_Vec3 CameraForward = GetForwardVector();
        Rr_Vec3 CameraLeft = GetRightVector();
        constexpr float CameraSpeed = 5.0f;
        if(Rr_GetKeyState(Keys, EIA_UP) == RR_KEYSTATE_HELD)
        {
            Position += CameraForward * CameraSpeed * DeltaTime;
        }
        if(Rr_GetKeyState(Keys, EIA_LEFT) == RR_KEYSTATE_HELD)
        {
            Position -= CameraLeft * CameraSpeed * DeltaTime;
        }
        if(Rr_GetKeyState(Keys, EIA_DOWN) == RR_KEYSTATE_HELD)
        {
            Position -= CameraForward * CameraSpeed * DeltaTime;
        }
        if(Rr_GetKeyState(Keys, EIA_RIGHT) == RR_KEYSTATE_HELD)
        {
            Position += CameraLeft * CameraSpeed * DeltaTime;
        }

        if(State->MouseState & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);
            constexpr float Sensitivity = 0.2f;
            Yaw = Rr_WrapMax(
                Yaw + (State->MousePositionDelta.X * Sensitivity),
                360.0f);
            Pitch = Rr_WrapMinMax(
                Pitch - (State->MousePositionDelta.Y * Sensitivity),
                -90.0f,
                90.0f);
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        float CosPitch = cosf(Pitch * RR_DEG_TO_RAD);
        float SinPitch = sinf(Pitch * RR_DEG_TO_RAD);
        float CosYaw = cosf(Yaw * RR_DEG_TO_RAD);
        float SinYaw = sinf(Yaw * RR_DEG_TO_RAD);

        Rr_Vec3 XAxis{ CosYaw, 0.0f, -SinYaw };
        Rr_Vec3 YAxis{ SinYaw * SinPitch, CosPitch, CosYaw * SinPitch };
        Rr_Vec3 ZAxis{ SinYaw * CosPitch, -SinPitch, CosPitch * CosYaw };

        ViewMatrix = {
            XAxis.X,
            YAxis.X,
            ZAxis.X,
            0.0f,
            XAxis.Y,
            YAxis.Y,
            ZAxis.Y,
            0.0f,
            XAxis.Z,
            YAxis.Z,
            ZAxis.Z,
            0.0f,
            -Rr_Dot(XAxis, Position),
            -Rr_Dot(YAxis, Position),
            -Rr_Dot(ZAxis, Position),
            1.0f,
        };
    }
};

struct SSplatRenderer
{
    struct SUniformData
    {
        Rr_Mat4 View;
        Rr_Mat4 Projection;
        Rr_Vec3 HFOVFocal;
    };

    struct SGPUSplat
    {
        Rr_Vec4 Position;
        Rr_Quat Quat;
        Rr_Vec4 Scale;
        Rr_Vec4 Color;
    };

    struct SGPUEntry
    {
        uint32_t Index;
        float Z;
    };

    struct SSplat
    {
        Rr_Vec3 Position;
        Rr_Vec3 Scale;
        unsigned char R, G, B, A;
        unsigned char QuatW, QuatX, QuatY, QuatZ;

        Rr_Quat Quat() const
        {
            return Rr_Quat{
                ((float)QuatW - 128.0f) / 128.0f,
                ((float)QuatX - 128.0f) / 128.0f,
                ((float)QuatY - 128.0f) / 128.0f,
                ((float)QuatZ - 128.0f) / 128.0f,
            };
        }

        Rr_Vec4 Color() const
        {
            return Rr_Vec4{
                (float)R / 255.0f,
                (float)G / 255.0f,
                (float)B / 255.0f,
                (float)A / 255.0f,
            };
        }
    };

    Rr_Renderer *Renderer;
    size_t AliveCount;
    size_t AlignedCount;
    SBitonicSorter Sorter;

    std::vector<SGPUSplat> GPUSplats;

    Rr_Buffer *SplatsBuffer{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *EntriesBuffer{};

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;

    SSplatRenderer(Rr_Renderer *Renderer, Rr_Asset Asset)
        : Renderer(Renderer)
        , AliveCount(Asset.Size / 32)
        , AlignedCount(Rr_NextPowerOfTwo(AliveCount))
        , Sorter(Renderer, AliveCount, AlignedCount)
    {
        /* Setup graphics pipeline. */

        std::array Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
            Rr_PipelineBinding{ 1, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
            Rr_PipelineBinding{ 2, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
        };
        std::array BindingSets = {
            Rr_PipelineBindingSet{
                Bindings.size(),
                Bindings.data(),
                RR_SHADER_STAGE_VERTEX_BIT,
            },
        };
        PipelineLayout = Rr_CreatePipelineLayout(
            Renderer,
            BindingSets.size(),
            BindingSets.data());

        Rr_VertexInputBinding VertexInputBinding = {
            RR_VERTEX_INPUT_RATE_VERTEX,
            0,
            nullptr,
        };

        std::array<Rr_ColorTargetInfo, 1> ColorTargets = {};
        ColorTargets[0].Format = Rr_GetSwapchainFormat(Renderer);
        ColorTargets[0].Blend.BlendEnable = true;
        ColorTargets[0].Blend.SrcColorBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
        ColorTargets[0].Blend.DstColorBlendFactor =
            RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ColorTargets[0].Blend.ColorBlendOp = RR_BLEND_OP_ADD;
        ColorTargets[0].Blend.SrcAlphaBlendFactor = RR_BLEND_FACTOR_SRC_ALPHA;
        ColorTargets[0].Blend.DstAlphaBlendFactor =
            RR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ColorTargets[0].Blend.AlphaBlendOp = RR_BLEND_OP_ADD;

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV = Rr_LoadAsset(EXAMPLE_ASSET_GS_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(EXAMPLE_ASSET_GS_FRAG_SPV);
        PipelineInfo.VertexInputBindingCount = 1;
        PipelineInfo.VertexInputBindings = &VertexInputBinding;
        PipelineInfo.ColorTargetCount = ColorTargets.size();
        PipelineInfo.ColorTargets = ColorTargets.data();
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_BACK;
        PipelineInfo.Rasterizer.FrontFace = RR_FRONT_FACE_COUNTER_CLOCKWISE;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

        /* Parse and upload splats. */

        GPUSplats.resize(AlignedCount);

        for(size_t Index = 0; Index < AliveCount; ++Index)
        {
            SSplat *Splat = ((SSplat *)Asset.Pointer) + Index;
            SGPUSplat &GPUSplat = GPUSplats[Index];

            GPUSplat.Position.XYZ = Splat->Position;
            GPUSplat.Scale.XYZ = Splat->Scale;
            GPUSplat.Quat = Rr_NormQ(Splat->Quat());
            GPUSplat.Color = Splat->Color();
        }

        SplatsBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(SGPUSplat) * AlignedCount,
            RR_BUFFER_FLAGS_STORAGE_BIT);
        Rr_UploadToDeviceBufferImmediate(
            Renderer,
            SplatsBuffer,
            RR_MAKE_DATA(sizeof(SGPUSplat) * AlignedCount, GPUSplats.data()));

        EntriesBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(SGPUEntry) * AlignedCount,
            RR_BUFFER_FLAGS_STORAGE_BIT);

        UniformBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(SUniformData),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_PER_FRAME_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    }

    ~SSplatRenderer()
    {
        Rr_DestroyGraphicsPipeline(Renderer, GraphicsPipeline);
        Rr_DestroyPipelineLayout(Renderer, PipelineLayout);
        Rr_DestroyBuffer(Renderer, SplatsBuffer);
        Rr_DestroyBuffer(Renderer, EntriesBuffer);
        Rr_DestroyBuffer(Renderer, UniformBuffer);
    }

    void Render(const SCamera &Camera, Rr_Image *ColorAttachment)
    {
        Sorter.Sort(
            Camera.ProjMatrix * Camera.ViewMatrix,
            sizeof(SGPUSplat) * AlignedCount,
            SplatsBuffer,
            sizeof(SGPUEntry) * AlignedCount,
            EntriesBuffer);

        SUniformData UniformData = {};
        UniformData.Projection = Camera.ProjMatrix;
        UniformData.View = Camera.ViewMatrix;
        UniformData.HFOVFocal = { Camera.HTanX, Camera.HTanY, Camera.FocalZ };

        std::memcpy(
            Rr_GetMappedBufferData(Renderer, UniformBuffer),
            &UniformData,
            sizeof(SUniformData));

        Rr_ColorTarget ColorTarget = {
            0,
            RR_LOAD_OP_CLEAR,
            RR_STORE_OP_STORE,
            {},
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
            Renderer,
            "graphics",
            1,
            &ColorTarget,
            &ColorAttachment,
            nullptr,
            nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(UniformData));
        Rr_BindStorageBuffer(
            GraphicsNode,
            SplatsBuffer,
            0,
            1,
            0,
            sizeof(SGPUSplat) * AliveCount);
        Rr_BindStorageBuffer(
            GraphicsNode,
            EntriesBuffer,
            0,
            2,
            0,
            sizeof(SGPUEntry) * AliveCount);
        Rr_DrawIndirect(GraphicsNode, Sorter.SortList.IndirectBuffer, 0, 1, 0);
    }
};

SCamera Camera;

SSplatRenderer *SplatData;

static void Init(void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer();

    Camera.Position = { 0.0f, -0.5f, -2.5f };

    SplatData =
        new SSplatRenderer(Renderer, Rr_LoadAsset(EXAMPLE_ASSET_PLUSH_SPLAT));
}

static void Iterate(void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer();

    static std::vector<Rr_InputMapping> InputMappings = {
        { RR_SCANCODE_W, RR_SCANCODE_UNKNOWN },
        { RR_SCANCODE_S, RR_SCANCODE_UNKNOWN },
        { RR_SCANCODE_A, RR_SCANCODE_UNKNOWN },
        { RR_SCANCODE_D, RR_SCANCODE_UNKNOWN },
        { RR_SCANCODE_F11, RR_SCANCODE_UNKNOWN },
        { RR_SCANCODE_F1, RR_SCANCODE_UNKNOWN },
        { RR_SCANCODE_F2, RR_SCANCODE_UNKNOWN },
        { RR_SCANCODE_F3, RR_SCANCODE_UNKNOWN },
    };
    static Rr_InputState InputState;
    Rr_UpdateInputState(
        InputMappings.size(),
        InputMappings.data(),
        &InputState);

    Rr_KeyStates Keys = InputState.Keys;

    Rr_IntVec2 SwapchainSize = Rr_GetSwapchainSize(Renderer);
    Camera.SetPerspective(50.0f, SwapchainSize, 0.1f, 200.0f);
    Camera.Update(&InputState);
    if(Rr_GetKeyState(Keys, EIA_FULLSCREEN) == RR_KEYSTATE_PRESSED)
    {
        Rr_ToggleFullscreen();
    }

    Rr_Image *SwapchainImage = Rr_GetSwapchainImage(Renderer);

    SplatData->Render(Camera, SwapchainImage);
}

static void Cleanup(void *UserData)
{
    delete SplatData;
}

int main()
{
    Rr_AppConfig Config = {};
    Config.Title = "GS";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.gs";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);
}
