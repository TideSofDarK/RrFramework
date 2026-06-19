#include "ExampleAssets.inc"

#include "BitonicSorter.hxx"

#include <Rr/Rr.h>

#include <array>
#include <cmath>
#include <vector>

constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 100.0f;

class CCamera
{
    float FieldOfView{ RR_ANGLE_DEG(90.0f) };
    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position{ 0.0f, -0.5f, -2.5f };

    Rr_Mat4 Transform = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

public:
    float HTanY;
    float HTanX;
    float FocalZ;

    Rr_Mat4 GetProjectionMatrix() const
    {
        return ProjMatrix;
    }

    Rr_Mat4 GetViewMatrix() const
    {
        return Rr_InvGeneral(Transform);
    }

    Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Transform.Columns[2].XYZ);
    }

    Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Transform.Columns[0].XYZ);
    }

    void Update(Rr_IntVec2 Size)
    {
        auto DeltaTime = Rr_GetDeltaSeconds();
        auto MouseDelta = Rr_GetMousePositionDelta();

        if (Rr_GetMouseState() & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);

            auto constexpr SPEED = 5.0f;
            Rr_Vec3 CameraForward = GetForwardVector();
            Rr_Vec3 CameraLeft = GetRightVector();
            if (Rr_IsScancodePressed(RR_SCANCODE_W))
            {
                Position += CameraForward * SPEED * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_A))
            {
                Position -= CameraLeft * SPEED * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_S))
            {
                Position -= CameraForward * SPEED * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_D))
            {
                Position += CameraLeft * SPEED * DeltaTime;
            }

            auto constexpr SENSITIVITY = 0.005f;
            Yaw += MouseDelta.X * SENSITIVITY;
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
        ProjMatrix = Rr_Perspective_LH(FieldOfView, (float)Size.X / (float)Size.Y, NEAR_PLANE, FAR_PLANE);

        HTanY = tanf(FieldOfView / 2.0f);
        HTanX = HTanY / (float)Size.Y * (float)Size.X;
        FocalZ = (float)Size.Y / (2.0f * HTanY);
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

    CCamera Camera;

    size_t AliveCount{};
    size_t AlignedCount{};
    SBitonicSorter *Sorter{};

    std::vector<SGPUSplat> GPUSplats;

    Rr_Buffer *SplatsBuffer{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *EntriesBuffer{};

    Rr_GraphicsPipeline *GraphicsPipeline{};

    SGSApp()
    {
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
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_GS_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
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
            SSplat *Splat = ((SSplat *)Asset.Data) + Index;
            SGPUSplat &GPUSplat = GPUSplats[Index];

            GPUSplat.Position.XYZ = Splat->Position;
            GPUSplat.Scale.XYZ = Splat->Scale;
            GPUSplat.Quat = Rr_NormQ(Splat->Quat());
            GPUSplat.Color = Splat->Color();
        }

        std::uint64_t SplatsDataSize = sizeof(SGPUSplat) * AlignedCount;
        SplatsBuffer = Rr_CreateBuffer(SplatsDataSize, RR_BUFFER_FLAGS_STORAGE_BIT);
        {
            Rr_Buffer *StagingBuffer =
                Rr_CreateBuffer(SplatsDataSize, RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
            std::memcpy(Rr_GetMappedBufferData(StagingBuffer), GPUSplats.data(), SplatsDataSize);

            Rr_TransferNode *TransferNode = Rr_AddTransferNode(Rr_GetGraph());
            Rr_TransferBufferData(TransferNode, SplatsDataSize, StagingBuffer, 0, SplatsBuffer, 0);

            Rr_ReleaseBuffer(StagingBuffer);
        }

        EntriesBuffer = Rr_CreateBuffer(sizeof(SGPUEntry) * AlignedCount, RR_BUFFER_FLAGS_STORAGE_BIT);

        UniformBuffer = Rr_CreateBuffer(
            sizeof(SUniformData),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT);
    }

    void Iterate()
    {
        Rr_UIBeginDebugOverlayTabs();
        Rr_UIBeginWindow("GS.cxx");
        Rr_UIText("This example shows how to implement 3D Gaussian splatting.");
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto SwapchainImage = Rr_GetSwapchainImage();
        auto SwapchainSize = Rr_GetImage2DExtent(SwapchainImage);

        Camera.Update(SwapchainSize);

        Sorter->Sort(
            Camera.GetProjectionMatrix() * Camera.GetViewMatrix(),
            sizeof(SGPUSplat) * AlignedCount,
            SplatsBuffer,
            sizeof(SGPUEntry) * AlignedCount,
            EntriesBuffer);

        SUniformData UniformData = {};
        UniformData.Projection = Camera.GetProjectionMatrix();
        UniformData.View = Camera.GetViewMatrix();
        UniformData.HFOVFocal = { Camera.HTanX, Camera.HTanY, Camera.FocalZ };

        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &UniformData, sizeof(SUniformData));

        Rr_ColorTarget ColorTarget = {
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(UniformData));
        Rr_BindStorageBuffer(GraphicsNode, SplatsBuffer, 0, 1, 0, sizeof(SGPUSplat) * AliveCount);
        Rr_BindStorageBuffer(GraphicsNode, EntriesBuffer, 0, 2, 0, sizeof(SGPUEntry) * AliveCount);
        Rr_DrawIndirect(GraphicsNode, Sorter->SortList.IndirectBuffer, 0, 1, 0);
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

    auto Config = Rr_Config{
        .WindowTitle = "GS",
        .InitFunc = []() { App = new SGSApp(); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
