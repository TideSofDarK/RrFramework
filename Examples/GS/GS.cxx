#include "ExampleAssets.inc"

#include "BitonicSorter.hxx"

#include <Rr/Rr.h>

#include <array>
#include <cmath>
#include <vector>

using UScancodes = std::array<bool, RR_SCANCODE_COUNT>;

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

    float HTanY;
    float HTanX;
    float FocalZ;

    void UpdatePerspective(Rr_IntVec2 Size)
    {
        ProjMatrix = Rr_Perspective_LH(
            RR_ANGLE_DEG(FOVDegrees),
            (float)Size.X / (float)Size.Y,
            NEAR_PLANE,
            FAR_PLANE);

        HTanY = tanf(RR_ANGLE_DEG(FOVDegrees) / 2.0f);
        HTanX = HTanY / (float)Size.Y * (float)Size.X;
        FocalZ = (float)Size.Y / (2.0f * HTanY);
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
                Position += CameraForward * CameraSpeed * DeltaTime;
            }
            if (Scancodes[RR_SCANCODE_A])
            {
                Position -= CameraLeft * CameraSpeed * DeltaTime;
            }
            if (Scancodes[RR_SCANCODE_S])
            {
                Position -= CameraForward * CameraSpeed * DeltaTime;
            }
            if (Scancodes[RR_SCANCODE_D])
            {
                Position += CameraLeft * CameraSpeed * DeltaTime;
            }

            constexpr float Sensitivity = 0.2f;
            Yaw += MouseDelta.X * Sensitivity;
            Pitch -= MouseDelta.Y * Sensitivity;
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, 360.0f);
        Pitch = RR_CLAMP(-90.0f, Pitch, 90.0f);

        Transform = Rr_Translate(Position) *
                    Rr_Rotate_RH(RR_ANGLE_DEG(Yaw), Rr_V3(0.0f, 1.0f, 0.0f)) *
                    Rr_Rotate_RH(RR_ANGLE_DEG(Pitch), Rr_V3(1.0f, 0.0f, 0.0f));
    }
};

struct SGSApp
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
                std::pow((float)R / 255.0f, 2.2f),
                std::pow((float)G / 255.0f, 2.2f),
                std::pow((float)B / 255.0f, 2.2f),
                (float)A / 255.0f,
            };
        }
    };

    SCamera Camera;

    UScancodes Scancodes{};

    size_t AliveCount{};
    size_t AlignedCount{};
    SBitonicSorter *Sorter{};

    std::vector<SGPUSplat> GPUSplats;

    Rr_Buffer *SplatsBuffer{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *EntriesBuffer{};

    Rr_GraphicsPipeline *GraphicsPipeline{};

    void Render(const SCamera &Camera, Rr_Image2D *ColorAttachment)
    {
        Sorter->Sort(
            Camera.ProjMatrix * Camera.GetViewMatrix(),
            sizeof(SGPUSplat) * AlignedCount,
            SplatsBuffer,
            sizeof(SGPUEntry) * AlignedCount,
            EntriesBuffer);

        SUniformData UniformData = {};
        UniformData.Projection = Camera.ProjMatrix;
        UniformData.View = Camera.GetViewMatrix();
        UniformData.HFOVFocal = { Camera.HTanX, Camera.HTanY, Camera.FocalZ };

        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &UniformData,
            sizeof(SUniformData));

        Rr_ColorTarget ColorTarget = {
            .Image = ColorAttachment,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, nullptr);
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
        Rr_DrawIndirect(GraphicsNode, Sorter->SortList.IndirectBuffer, 0, 1, 0);
    }

    SGSApp()
    {
        Camera.UpdatePerspective(Rr_GetImage2DExtent(Rr_GetSwapchainImage()));
        Camera.Position = { 0.0f, -0.5f, -2.5f };

        Rr_Asset Asset = Rr_LoadAsset(EXAMPLE_ASSET_PLUSH_SPLAT);

        AliveCount = Asset.Size / 32;
        AlignedCount = Rr_NextPowerOfTwo(AliveCount);
        Sorter = new SBitonicSorter(AliveCount, AlignedCount);

        /* Setup graphics pipeline. */

        Rr_VertexInputBinding VertexInputBinding = {
            RR_VERTEX_INPUT_RATE_VERTEX,
            0,
            nullptr,
        };

        std::array<Rr_ColorTargetInfo, 1> ColorTargets = {};
        ColorTargets[0].Format = Rr_GetImageFormat(Rr_GetSwapchainImage());
        ColorTargets[0].Blend = Rr_AlphaBlend();

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_GS_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Pointer,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_GS_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Pointer,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {};
        PipelineInfo.VertexShaderInfo = &VertexShaderInfo;
        PipelineInfo.FragmentShaderInfo = &FragmentShaderInfo;
        PipelineInfo.VertexInputBindingCount = 1;
        PipelineInfo.VertexInputBindings = &VertexInputBinding;
        PipelineInfo.ColorTargetCount = ColorTargets.size();
        PipelineInfo.ColorTargets = ColorTargets.data();
        PipelineInfo.Rasterizer.CullMode = RR_CULL_MODE_BACK;
        PipelineInfo.Rasterizer.FrontFace = RR_FRONT_FACE_COUNTER_CLOCKWISE;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        /* Parse and upload splats. */

        GPUSplats.resize(AlignedCount);

        for (size_t Index = 0; Index < AliveCount; ++Index)
        {
            SSplat *Splat = ((SSplat *)Asset.Pointer) + Index;
            SGPUSplat &GPUSplat = GPUSplats[Index];

            GPUSplat.Position.XYZ = Splat->Position;
            GPUSplat.Scale.XYZ = Splat->Scale;
            GPUSplat.Quat = Rr_NormQ(Splat->Quat());
            GPUSplat.Color = Splat->Color();
        }

        std::uint64_t SplatsDataSize = sizeof(SGPUSplat) * AlignedCount;
        SplatsBuffer =
            Rr_CreateBuffer(SplatsDataSize, RR_BUFFER_FLAGS_STORAGE_BIT);
        {
            Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
                SplatsDataSize,
                RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
            std::memcpy(
                Rr_GetMappedBufferData(StagingBuffer),
                GPUSplats.data(),
                SplatsDataSize);

            Rr_TransferNode *TransferNode = Rr_AddTransferNode(Rr_GetGraph());
            Rr_TransferBufferData(
                TransferNode,
                SplatsDataSize,
                StagingBuffer,
                0,
                SplatsBuffer,
                0);

            Rr_ReleaseBuffer(StagingBuffer);
        }

        EntriesBuffer = Rr_CreateBuffer(
            sizeof(SGPUEntry) * AlignedCount,
            RR_BUFFER_FLAGS_STORAGE_BIT);

        UniformBuffer = Rr_CreateBuffer(
            sizeof(SUniformData),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_PER_FRAME_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    }

    void Event(Rr_Event const *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                Camera.UpdatePerspective(
                    Rr_GetImage2DExtent(Rr_GetSwapchainImage()));
            }
            break;
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
        Camera.Update(Scancodes);

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();

        Render(Camera, SwapchainImage);
    }

    ~SGSApp()
    {
        delete Sorter;
        Rr_ReleaseBuffer(SplatsBuffer);
        Rr_ReleaseBuffer(EntriesBuffer);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    }
};

int main()
{
    static SGSApp *App{};

    Rr_AppConfig Config = {};
    Config.Title = "GS";
    Config.InitFunc = []() { App = new SGSApp(); };
    Config.EventFunc = [](Rr_Event const *Event) { App->Event(Event); };
    Config.IterateFunc = []() { App->Iterate(); };
    Config.CleanupFunc = []() { delete App; };
    Rr_Run(&Config);
}
