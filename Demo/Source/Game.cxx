#include "Game.hxx"

#include "DemoAssets.inc"

#include <Rr/Rr.h>

#include <array>
#include <cstring>
#include <format>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>

#define MAX_CREEPS 1024

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

struct SCamera
{
    enum class EType
    {
        FP,
        ORBIT,
        TOPDOWN,
    };

    float Pitch{};
    float Yaw{};
    Rr_Vec3 Position{};

    Rr_Vec3 OrbitCenter = { 0.0f, 3.0f, 0.0f };
    float OrbitDistance = 10.0f;
    float OrbitAcceleration = 0.1f;
    Rr_Vec2 OrbitVelocity{};

    float TDHeight = 10.0f;
    float TDPitch = 45.0f * Rr_DegToRad;
    float TDYaw = 45.0f * Rr_DegToRad;
    Rr_Vec3 TDPosition{};

    Rr_Mat4 ViewMatrix = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

    EType Type = EType::TOPDOWN;

    void SetPerspective(float FOVDegrees, float Aspect)
    {
        ProjMatrix =
            Rr_Perspective_LH_ZO(Rr_AngleDeg(FOVDegrees), Aspect, 0.5f, 50.0f);
    }

    [[nodiscard]] Rr_Vec3 GetForwardVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[2].XYZ);
    }

    [[nodiscard]] Rr_Vec3 GetRightVector() const
    {
        return Rr_Norm(Rr_InvGeneral(ViewMatrix).Columns[0].XYZ);
    }

    void Update(Rr_App *App, Rr_InputState *State)
    {
        Rr_KeyStates Keys = State->Keys;

        if(Rr_GetKeyState(Keys, EIA_CAMERA) == RR_KEYSTATE_PRESSED)
        {
            Type = (EType)(((uint32_t)Type + 1) % 3);
        }

        switch(Type)
        {
            case EType::FP:
            {
                Rr_Vec3 CameraForward = GetForwardVector();
                Rr_Vec3 CameraLeft = GetRightVector();
                constexpr float CameraSpeed = 0.1f;
                if(Rr_GetKeyState(Keys, EIA_UP) == RR_KEYSTATE_HELD)
                {
                    Position -= CameraForward * CameraSpeed;
                }
                if(Rr_GetKeyState(Keys, EIA_LEFT) == RR_KEYSTATE_HELD)
                {
                    Position -= CameraLeft * CameraSpeed;
                }
                if(Rr_GetKeyState(Keys, EIA_DOWN) == RR_KEYSTATE_HELD)
                {
                    Position += CameraForward * CameraSpeed;
                }
                if(Rr_GetKeyState(Keys, EIA_RIGHT) == RR_KEYSTATE_HELD)
                {
                    Position += CameraLeft * CameraSpeed;
                }

                if(State->MouseState & RR_MOUSE_BUTTON_RIGHT_MASK)
                {
                    Rr_SetRelativeMouseMode(App, true);
                    constexpr float Sensitivity = 0.2f;
                    Yaw = Rr_WrapMax(
                        Yaw - (State->MousePositionDelta.X * Sensitivity),
                        360.0f);
                    Pitch = Rr_WrapMinMax(
                        Pitch - (State->MousePositionDelta.Y * Sensitivity),
                        -90.0f,
                        90.0f);
                }
                else
                {
                    Rr_SetRelativeMouseMode(App, false);
                }

                float CosPitch = cosf(Pitch * Rr_DegToRad);
                float SinPitch = sinf(Pitch * Rr_DegToRad);
                float CosYaw = cosf(Yaw * Rr_DegToRad);
                float SinYaw = sinf(Yaw * Rr_DegToRad);

                Rr_Vec3 XAxis{ CosYaw, 0.0f, -SinYaw };
                Rr_Vec3 YAxis{ SinYaw * SinPitch, CosPitch, CosYaw * SinPitch };
                Rr_Vec3 ZAxis{ SinYaw * CosPitch,
                               -SinPitch,
                               CosPitch * CosYaw };

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
            break;
            case EType::ORBIT:
            {
                if(State->MouseState & RR_MOUSE_BUTTON_RIGHT_MASK)
                {
                    Rr_SetRelativeMouseMode(App, true);
                    constexpr float Sensitivity = 0.05f;

                    OrbitVelocity.X -=
                        (State->MousePositionDelta.Y * Sensitivity);
                    OrbitVelocity.Y -=
                        (State->MousePositionDelta.X * Sensitivity);
                }
                else
                {
                    Rr_SetRelativeMouseMode(App, false);
                }

                OrbitVelocity.X *= 0.9f;
                OrbitVelocity.Y *= 0.9f;

                Pitch += OrbitVelocity.X;
                Yaw += OrbitVelocity.Y;

                Yaw = Rr_WrapMax(Yaw, 360.0f);
                Pitch = RR_CLAMP(-89.99f, Pitch, 0.0f);

                Rr_Vec3 Eye = { 0.0f, 0.0f, -1.0f };
                Eye = Rr_RotateV3AxisAngle_RH(
                    Eye,
                    { 0.0f, 1.0f, 0.0f },
                    Yaw * Rr_DegToRad);
                Eye = Rr_RotateV3AxisAngle_RH(
                    Eye,
                    Rr_Cross(Eye, { 0.0f, 1.0f, 0.0f }),
                    -Pitch * Rr_DegToRad);

                Eye *= OrbitDistance;
                Eye += OrbitCenter;

                ViewMatrix =
                    Rr_LookAt_RH(Eye, OrbitCenter, { 0.0f, 1.0f, 0.0f });
            }
            break;
            case EType::TOPDOWN:
            {
                if(State->MouseState & RR_MOUSE_BUTTON_RIGHT_MASK)
                {
                    Rr_SetRelativeMouseMode(App, true);
                    constexpr float Sensitivity = 0.1f;
                    TDPosition.X -= (State->MousePositionDelta.X * Sensitivity);
                    TDPosition.Z -= (State->MousePositionDelta.Y * Sensitivity);
                }
                else
                {
                    Rr_SetRelativeMouseMode(App, false);
                }

                ViewMatrix =
                    Rr_Rotate_LH(-TDPitch, { 1.0f, 0.0f, 0.0f }) *
                    Rr_Translate({ TDPosition.X, -TDHeight, TDPosition.Z }) *
                    Rr_Rotate_LH(TDYaw, { 0.0f, 1.0f, 0.0f });
            }
            break;
            default:
            {
                std::abort();
            }
            break;
        }
    }
};

struct SCreep
{
    Rr_Vec3 Position;
    float Speed;
    Rr_Vec3 Velocity;
    uint Flags;
    Rr_Vec3 Color;
    float Padding3;
};

struct SCreepManager
{
    float SpawnRadius = 15.0f;
    float CreepRadius = 1.75f;
    float CreepRadiusSqr = CreepRadius * CreepRadius;
    float MinSpeed = 0.5f;
    float MaxSpeed = 0.8f;
    float SeparationFactor = 0.2f;

    std::vector<SCreep> Creeps;

    Rr_PipelineLayout *PipelineLayout;
    Rr_GraphicsPipeline *GraphicsPipeline;
    Rr_Buffer *StorageBuffer;

    Rr_App *App;

    SCreepManager(Rr_App *App, size_t InitialCount)
        : App(App)
    {
        Rr_Renderer *Renderer = Rr_GetRenderer(App);
        std::array VertexBindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
        };
        std::array Bindings = {
            Rr_PipelineBinding{ 0, 1, RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER },
        };
        std::array Sets = {
            Rr_PipelineBindingSet{
                VertexBindings.size(),
                VertexBindings.data(),
                RR_SHADER_STAGE_VERTEX_BIT,
            },
            Rr_PipelineBindingSet{
                Bindings.size(),
                Bindings.data(),
                RR_SHADER_STAGE_FRAGMENT_BIT | RR_SHADER_STAGE_VERTEX_BIT,
            },
        };
        PipelineLayout =
            Rr_CreatePipelineLayout(Renderer, Sets.size(), Sets.data());

        std::array VertexAttributes = {
            Rr_VertexInputAttribute{ 0, RR_FORMAT_VEC3 },
            Rr_VertexInputAttribute{ 1, RR_FORMAT_VEC2 },
            Rr_VertexInputAttribute{ 2, RR_FORMAT_VEC3 },
        };

        Rr_VertexInputBinding VertexInputBinding = {
            RR_VERTEX_INPUT_RATE_VERTEX,
            VertexAttributes.size(),
            VertexAttributes.data(),
        };

        Rr_ColorTargetInfo ColorTarget = {};
        ColorTarget.Format = Rr_GetSwapchainFormat(Renderer);

        Rr_PipelineInfo PipelineInfo = {};
        PipelineInfo.Layout = PipelineLayout;
        PipelineInfo.VertexShaderSPV = Rr_LoadAsset(DEMO_ASSET_CREEP_VERT_SPV);
        PipelineInfo.FragmentShaderSPV =
            Rr_LoadAsset(DEMO_ASSET_CREEP_FRAG_SPV);
        PipelineInfo.VertexInputBindingCount = 1;
        PipelineInfo.VertexInputBindings = &VertexInputBinding;
        PipelineInfo.ColorTargetCount = 1;
        PipelineInfo.ColorTargets = &ColorTarget;
        PipelineInfo.DepthStencil.EnableDepthTest = true;
        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;

        GraphicsPipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

        StorageBuffer = Rr_CreateBuffer(
            Renderer,
            sizeof(SCreep) * MAX_CREEPS,
            RR_BUFFER_FLAGS_STORAGE_BIT);

        AddCreeps(InitialCount);
    }

    void Cleanup()
    {
        Rr_Renderer *Renderer = Rr_GetRenderer(App);
        Rr_DestroyBuffer(Renderer, StorageBuffer);
        Rr_DestroyGraphicsPipeline(Renderer, GraphicsPipeline);
        Rr_DestroyPipelineLayout(Renderer, PipelineLayout);
    }

    void Update(float DeltaTime)
    {
        // DeltaTime *= 0.01f;
        // if(DeltaTime > 0.1f)
        // return;
        for(size_t Index = 0; Index < Creeps.size(); ++Index)
        {
            SCreep &Creep = Creeps[Index];
            Rr_Vec3 Wanted;
            if(Creep.Flags)
            {
                Wanted =
                    Rr_NormV3(Rr_Vec3{ 0.0f, 4.0f, 0.0f } - Creep.Position);
            }
            else
            {
                Wanted = Rr_NormV3(-Creep.Position);
            }
            Wanted += GetCreepSeparationVelocity(Index);
            Creep.Velocity += Rr_NormV3(Wanted);
            if(Rr_LenV3(Creep.Velocity) > Creep.Speed)
            {
                Creep.Velocity = Rr_NormV3(Creep.Velocity) * Creep.Speed;
            }
            Creep.Position += Creep.Speed * Creep.Velocity * 0.01f;

            if(Creep.Flags == 0 && Rr_LenV3(Creep.Position) < 1.75f)
            {
                float Angle =
                    RR_PI32 * 2.0f * ((float)Index / float(Creeps.size()));
                Creep.Position = {
                    cosf(Angle),
                    0,
                    sinf(Angle),
                };
                Creep.Velocity = Rr_NormV3(-Creep.Position);
                Creep.Position *= SpawnRadius;
            }
        }
    }

    void AddCreeps(size_t Count)
    {
        std::random_device RandomDevice;
        std::mt19937 Gen(RandomDevice());
        std::uniform_real_distribution<> SpeedDist(MinSpeed, MaxSpeed);
        std::uniform_real_distribution<> ColorDist(0.2f, 1.0f);
        std::uniform_real_distribution<> HeightDist(2.0f, 5.0f);

        for(size_t Index = 0; Index < Count; ++Index)
        {
            // float Angle = Distribution(Gen);
            float Angle = RR_PI32 * 2.0f * ((float)Index / float(Count));
            SCreep Creep = {};
            Creep.Speed = SpeedDist(Gen);
            Creep.Position = {
                cosf(Angle),
                0,
                sinf(Angle),
            };
            Creep.Velocity = -Creep.Position;
            Creep.Position *= SpawnRadius;
            Creep.Color = { (float)ColorDist(Gen),
                            (float)ColorDist(Gen),
                            (float)ColorDist(Gen) };

            if((float)ColorDist(Gen) > 0.8f)
            {
                Creep.Flags = 1;
                Creep.Position.Y = (float)HeightDist(Gen);
                Creep.Speed *= 2.0f;
            }

            Creeps.push_back(Creep);
        }
    }

    Rr_Vec3 GetCreepSeparationVelocity(size_t CreepIndex)
    {
        Rr_Vec3 &Position = Creeps[CreepIndex].Position;
        size_t Avoids = 0;
        Rr_Vec3 Result = {};
        for(size_t Index = 0; Index < Creeps.size(); ++Index)
        {
            if(CreepIndex == Index)
            {
                continue;
            }

            Rr_Vec3 A = Position - Creeps[Index].Position;
            float Distance = Rr_LenSqrV3(A);
            if(Distance <= CreepRadius)
            {
                Rr_Vec3 Norm = Rr_NormV3(A);
                Norm /= Distance;
                Result += Norm;
                Avoids++;
            }
        }
        if(Avoids > 0)
        {
            Result /= (float)Avoids;
            Result *= SeparationFactor;
        }
        return Result;
    }

    void Render(
        Rr_GraphNode *Node,
        Rr_GLTFAsset *GLTFAsset,
        Rr_GLTFContext *GLTFContext,
        Rr_Buffer *StagingBuffer,
        char *StagingData,
        size_t *StagingOffset)
    {
        Rr_Renderer *Renderer = Rr_GetRenderer(App);

        /* Upload creeps to storage buffer. */

        size_t StorageSize = sizeof(SCreep) * Creeps.size();

        std::memcpy(StagingData + *StagingOffset, Creeps.data(), StorageSize);

        Rr_GraphNode *TransferNode =
            Rr_AddTransferNode(App, "upload_creep_data");
        Rr_TransferBufferData(
            App,
            TransferNode,
            StorageSize,
            StagingBuffer,
            *StagingOffset,
            StorageBuffer,
            0);

        *StagingOffset = RR_ALIGN_POW2(
            *StagingOffset + StorageSize,
            Rr_GetStorageAlignment(Renderer));

        /* Draw creeps. */

        Rr_BindGraphicsPipeline(Node, GraphicsPipeline);
        Rr_BindGraphicsStorageBuffer(Node, StorageBuffer, 1, 0, 0, StorageSize);

        Rr_BindVertexBuffer(
            Node,
            GLTFAsset->Buffer,
            0,
            GLTFAsset->VertexBufferOffset);
        Rr_BindIndexBuffer(
            Node,
            GLTFAsset->Buffer,
            0,
            GLTFAsset->IndexBufferOffset,
            GLTFAsset->IndexType);

        for(size_t MeshIndex = 0; MeshIndex < GLTFAsset->MeshCount; ++MeshIndex)
        {
            Rr_GLTFMesh *Mesh = GLTFAsset->Meshes + MeshIndex;
            for(size_t Index = 0; Index < Mesh->PrimitiveCount; ++Index)
            {
                Rr_GLTFPrimitive *GLTFPrimitive = Mesh->Primitives + Index;
                Rr_DrawIndexed(
                    Node,
                    GLTFPrimitive->IndexCount,
                    Creeps.size(),
                    GLTFPrimitive->FirstIndex,
                    GLTFPrimitive->VertexOffset,
                    0);
            }
        }
    }
};

struct SUniformData
{
    Rr_Mat4 Model;
    Rr_Mat4 View;
    Rr_Mat4 Projection;
};

static Rr_LoadThread *LoadThread;
static Rr_GLTFContext *GLTFContext;
static Rr_GLTFAsset *TowerAsset;
static Rr_GLTFAsset *ArrowAsset;
static Rr_Image *BlitImage;
static Rr_Image *ColorAttachment;
static Rr_Image *DepthAttachment;
static Rr_Buffer *StagingBuffer;
static Rr_Buffer *UniformBuffer;
static Rr_PipelineLayout *PipelineLayout;
static Rr_GraphicsPipeline *GraphicsPipeline;
static Rr_Sampler *NearestSampler;
static SCamera Camera;
static SCreepManager *CreepManager;

static bool Loaded;

Rr_Vec4 BackgroundColor{ 0.1f, 0.1f, 0.1f, 1.0f };

static void OnLoadComplete(Rr_App *App, void *UserData)
{
    Loaded = true;
}

static void Init(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    /* Create simple sampler. */

    Rr_SamplerInfo SamplerInfo = {};
    SamplerInfo.MinFilter = RR_FILTER_NEAREST;
    SamplerInfo.MagFilter = RR_FILTER_NEAREST;
    NearestSampler = Rr_CreateSampler(Renderer, &SamplerInfo);

    /* Create graphics pipeline. */

    Rr_PipelineBinding BindingsA[] = {
        { 0, 1, RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER },
    };
    Rr_PipelineBinding BindingsB[] = {
        { 0, 1, RR_PIPELINE_BINDING_TYPE_SAMPLER },
        { 1, 1, RR_PIPELINE_BINDING_TYPE_SAMPLED_IMAGE },
    };
    Rr_PipelineBindingSet BindingSets[] = {
        {
            RR_ARRAY_COUNT(BindingsA),
            BindingsA,
            RR_SHADER_STAGE_VERTEX_BIT,
        },
        {
            RR_ARRAY_COUNT(BindingsB),
            BindingsB,
            RR_SHADER_STAGE_FRAGMENT_BIT | RR_SHADER_STAGE_VERTEX_BIT,
        },
    };
    PipelineLayout = Rr_CreatePipelineLayout(
        Renderer,
        RR_ARRAY_COUNT(BindingSets),
        BindingSets);

    Rr_VertexInputAttribute VertexAttributes[] = {
        { 0, RR_FORMAT_VEC3 },
        { 1, RR_FORMAT_VEC2 },
        { 2, RR_FORMAT_VEC3 },
    };

    Rr_VertexInputBinding VertexInputBinding = {
        RR_VERTEX_INPUT_RATE_VERTEX,
        RR_ARRAY_COUNT(VertexAttributes),
        VertexAttributes,
    };

    Rr_ColorTargetInfo ColorTargets[1] = {};
    ColorTargets[0].Format = Rr_GetSwapchainFormat(Renderer);

    Rr_PipelineInfo PipelineInfo = {};
    PipelineInfo.Layout = PipelineLayout;
    PipelineInfo.VertexShaderSPV = Rr_LoadAsset(DEMO_ASSET_TEST_VERT_SPV);
    PipelineInfo.FragmentShaderSPV = Rr_LoadAsset(DEMO_ASSET_TEST_FRAG_SPV);
    PipelineInfo.VertexInputBindingCount = 1;
    PipelineInfo.VertexInputBindings = &VertexInputBinding;
    PipelineInfo.ColorTargetCount = 1;
    PipelineInfo.ColorTargets = ColorTargets;
    PipelineInfo.DepthStencil.EnableDepthTest = true;
    PipelineInfo.DepthStencil.EnableDepthWrite = true;
    PipelineInfo.DepthStencil.CompareOp = RR_COMPARE_OP_LESS;

    GraphicsPipeline = Rr_CreateGraphicsPipeline(Renderer, &PipelineInfo);

    /* Create GLTF context. */

    Rr_GLTFAttributeType GLTFAttributeTypes[] = {
        RR_GLTF_ATTRIBUTE_TYPE_POSITION,
        RR_GLTF_ATTRIBUTE_TYPE_TEXCOORD0,
        RR_GLTF_ATTRIBUTE_TYPE_NORMAL,
    };
    Rr_GLTFVertexInputBinding GLTFVertexInputBinding = {
        RR_ARRAY_COUNT(GLTFAttributeTypes),
        GLTFAttributeTypes,
    };
    Rr_GLTFTextureMapping GLTFTextureMappings[] = {
        { 0, 1, RR_GLTF_TEXTURE_TYPE_COLOR },
    };
    GLTFContext = Rr_CreateGLTFContext(
        Renderer,
        1,
        &VertexInputBinding,
        &GLTFVertexInputBinding,
        RR_ARRAY_COUNT(GLTFTextureMappings),
        GLTFTextureMappings);

    /* Create load thread and load glTF asset. */

    LoadThread = Rr_CreateLoadThread(App);
    Rr_LoadTask Tasks[] = {
        Rr_LoadGLTFAssetTask(DEMO_ASSET_TOWER_GLB, GLTFContext, &TowerAsset),
        Rr_LoadGLTFAssetTask(DEMO_ASSET_ARROW_GLB, GLTFContext, &ArrowAsset),
    };
    Rr_LoadAsync(LoadThread, RR_ARRAY_COUNT(Tasks), Tasks, OnLoadComplete, App);

    /* Create main draw target. */

    BlitImage = Rr_CreateImage(
        Renderer,
        { 128, 128, 1 },
        Rr_GetSwapchainFormat(Renderer),
        RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);

    ColorAttachment = Rr_CreateImage(
        Renderer,
        { 320, 240, 1 },
        Rr_GetSwapchainFormat(Renderer),
        RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT |
            RR_IMAGE_FLAGS_SAMPLED_BIT);

    DepthAttachment = Rr_CreateImage(
        Renderer,
        { 320, 240, 1 },
        RR_TEXTURE_FORMAT_D32_SFLOAT,
        RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT |
            RR_IMAGE_FLAGS_TRANSFER_BIT);

    /* Create uniform buffer. */

    UniformBuffer = Rr_CreateBuffer(
        Renderer,
        sizeof(SUniformData),
        RR_BUFFER_FLAGS_UNIFORM_BIT);

    /* Create staging buffer */

    StagingBuffer = Rr_CreateBuffer(
        Renderer,
        RR_MEGABYTES(16),
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    CreepManager = new SCreepManager(App, 64);
    Camera.SetPerspective(60.0f, Rr_GetImageAspect2D(ColorAttachment));
}

static void Render(
    Rr_App *App,
    Rr_Image *ColorAttachment,
    Rr_Image *DepthAttachment)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    // double Time = Rr_GetTimeSeconds(App);

    SUniformData UniformData = {};
    UniformData.Projection =
        Rr_Perspective_LH_ZO(Rr_AngleDeg(75.0f), 320.0f / 240.0f, 0.5f, 50.0f);
    UniformData.View = Rr_VulkanMatrix() * Camera.ViewMatrix;
    UniformData.Model = Rr_M4D(1.0f);
    // UniformData.Model = Rr_MulM4(
    //     Rr_Translate({ 0.0f, 0.0f, 0.0f }),
    //     Rr_Rotate_LH(fmodf(Time, RR_PI * 2), { 0.0f, 1.0f, 0.0f }));
    // UniformData.Model = Rr_MulM4(
    //     UniformData.Model,
    //     Rr_Rotate_LH(sin(Time), { 0.0f, 0.0f, 1.0f }));

    char *StagingData = (char *)Rr_GetMappedBufferData(Renderer, StagingBuffer);
    std::memcpy(StagingData, &UniformData, sizeof(UniformData));
    size_t StagingOffset =
        RR_ALIGN_POW2(sizeof(UniformData), Rr_GetUniformAlignment(Renderer));

    Rr_GraphNode *TransferNode =
        Rr_AddTransferNode(App, "upload_uniform_buffer");
    Rr_TransferBufferData(
        App,
        TransferNode,
        sizeof(UniformData),
        StagingBuffer,
        0,
        UniformBuffer,
        0);

    Rr_ColorTarget OffscreenTarget = {
        0,
        RR_LOAD_OP_CLEAR,
        RR_STORE_OP_STORE,
        {
            BackgroundColor,
        },
    };
    Rr_DepthTarget OffscreenDepth = {
        RR_LOAD_OP_CLEAR,
        RR_STORE_OP_STORE,
        {
            1.0f,
            0,
        },
    };
    Rr_GraphNode *OffscreenNode = Rr_AddGraphicsNode(
        App,
        "offscreen",
        1,
        &OffscreenTarget,
        &ColorAttachment,
        &OffscreenDepth,
        DepthAttachment);

    if(Loaded)
    {
        Rr_BindGraphicsPipeline(OffscreenNode, GraphicsPipeline);
        Rr_BindGraphicsUniformBuffer(
            OffscreenNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(UniformData));
        Rr_BindSampler(OffscreenNode, NearestSampler, 1, 0);
        Rr_BindSampledImage(OffscreenNode, TowerAsset->Images[0], 1, 1);

        Rr_BindVertexBuffer(
            OffscreenNode,
            TowerAsset->Buffer,
            0,
            TowerAsset->VertexBufferOffset);
        Rr_BindIndexBuffer(
            OffscreenNode,
            TowerAsset->Buffer,
            0,
            TowerAsset->IndexBufferOffset,
            TowerAsset->IndexType);

        for(size_t MeshIndex = 0; MeshIndex < TowerAsset->MeshCount;
            ++MeshIndex)
        {
            Rr_GLTFMesh *Mesh = TowerAsset->Meshes + MeshIndex;
            for(size_t Index = 0; Index < Mesh->PrimitiveCount; ++Index)
            {
                Rr_GLTFPrimitive *GLTFPrimitive = Mesh->Primitives + Index;
                Rr_DrawIndexed(
                    OffscreenNode,
                    GLTFPrimitive->IndexCount,
                    1,
                    GLTFPrimitive->FirstIndex,
                    GLTFPrimitive->VertexOffset,
                    0);
            }
        }

        CreepManager->Render(
            OffscreenNode,
            ArrowAsset,
            GLTFContext,
            StagingBuffer,
            StagingData,
            &StagingOffset);

        Rr_AddBlitNode(
            App,
            "blit1",
            ColorAttachment,
            BlitImage,
            { 0, 0, 320, 240 },
            { 0, 0, 128, 128 },
            RR_BLIT_MODE_COLOR);
        Rr_AddBlitNode(
            App,
            "blit2",
            BlitImage,
            ColorAttachment,
            { 0, 0, 128, 128 },
            { 0, 0, 64, 64 },
            RR_BLIT_MODE_COLOR);
    }
}

static void Iterate(Rr_App *App, void *UserData)
{
    {
        static Rr_InputState InputState;
        Rr_UpdateInputState(
            InputMappings.size(),
            InputMappings.data(),
            &InputState);

        Rr_KeyStates Keys = InputState.Keys;

        Camera.Update(App, &InputState);
        if(Rr_GetKeyState(Keys, EIA_FULLSCREEN) == RR_KEYSTATE_PRESSED)
        {
            Rr_ToggleFullscreen(App);
        }
    }

    CreepManager->Update(Rr_GetDeltaSeconds(App));

    Render(App, ColorAttachment, DepthAttachment);

    Rr_AddPresentNode(
        App,
        "present",
        ColorAttachment,
        NearestSampler,
        BackgroundColor,
        RR_PRESENT_MODE_FIT);
}

static void Cleanup(Rr_App *App, void *UserData)
{
    Rr_Renderer *Renderer = Rr_GetRenderer(App);

    Rr_DestroyLoadThread(App, LoadThread);
    CreepManager->Cleanup();
    Rr_DestroyGLTFContext(GLTFContext);
    Rr_DestroyImage(Renderer, BlitImage);
    Rr_DestroyImage(Renderer, ColorAttachment);
    Rr_DestroyImage(Renderer, DepthAttachment);
    Rr_DestroyBuffer(Renderer, StagingBuffer);
    Rr_DestroyBuffer(Renderer, UniformBuffer);
    Rr_DestroyGraphicsPipeline(Renderer, GraphicsPipeline);
    Rr_DestroyPipelineLayout(Renderer, PipelineLayout);
    Rr_DestroySampler(Renderer, NearestSampler);
}

void RunGame()
{
    Rr_AppConfig Config = {};
    Config.Title = "05_GLTFCube";
    Config.Version = "1.0.0";
    Config.Package = "com.rr.examples.05_gltfcube";
    Config.InitFunc = Init;
    Config.CleanupFunc = Cleanup;
    Config.IterateFunc = Iterate;
    Rr_Run(&Config);
}
