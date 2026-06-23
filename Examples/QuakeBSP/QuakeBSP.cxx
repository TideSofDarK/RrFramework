#include <Rr/Rr.h>

#include "ExampleAssets.inc"

#include <array>
#include <cassert>
#include <span>

struct SBBox
{
    Rr_Vec3 Min;
    Rr_Vec3 Max;
};

struct SBBoxShort
{
    int16_t Min;
    int16_t Max;
};

struct SPlane
{
    Rr_Vec3 Normal;
    float Distance;
    int32_t Type;
};

struct SVertex
{
    float X, Y, Z;
};

struct SEdge
{
    uint16_t Vertex0;
    uint16_t Vertex1;
};

struct SFace
{
    uint16_t PlaneIndex;
    uint16_t Side;
    int32_t EdgeIndex;
    uint16_t EdgeCount;
    uint8_t LightType;
    uint8_t LightBase;
    uint8_t Lights[2];
    int32_t Lightmap;
};

struct SNode
{
    int32_t PlaneIndex;
    uint16_t Front;
    uint16_t Back;
    SBBoxShort Box;
    uint16_t FaceIndex;
    uint16_t FaceCount;
};

struct SLeaf
{
    int32_t Type;
    int32_t VisList;
    SBBoxShort Bound;
    uint16_t FaceIndex;
    uint16_t FaceCount;
    uint8_t SndWater;
    uint8_t SndSky;
    uint8_t SndSlime;
    uint8_t SndLava;
};

struct SEntry
{
    int32_t Offset;
    int32_t Size;
};

struct SModel
{
    SBBox Bound;
    Rr_Vec3 Origin;
    int32_t FirstBSPNodeIndex;
    int32_t FirstClipNodeIndex;
    int32_t SecondClipNodeIndex;
    int32_t Unused;
    int32_t LeafCount;
    int32_t FaceIndex;
    int32_t FaceCount;
};

struct SHeader
{
    int32_t Version;
    SEntry Entities;
    SEntry Planes;
    SEntry MipTextures;
    SEntry Vertices;
    SEntry VisList;
    SEntry Nodes;
    SEntry TextureInfo;
    SEntry Faces;
    SEntry Lightmaps;
    SEntry ClipNodes;
    SEntry Leaves;
    SEntry FaceList;
    SEntry Edges;
    SEntry EdgeList;
    SEntry Models;
};

struct SQuakeBSP
{
    std::span<SPlane> Planes;
    std::span<SVertex> Vertices;
    std::span<SNode> Nodes;
    std::span<SFace> Faces;
    std::span<SLeaf> Leaves;
    std::span<SEdge> Edges;
    std::span<SModel> Models;
};

template <typename T>
auto GetSpan(std::byte const *Raw, SEntry const &Entry) -> std::span<T>
{
    return { (T *)(Raw + Entry.Offset), Entry.Size / sizeof(T) };
}

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
        ProjMatrix = Rr_Perspective_RH(FieldOfView, Aspect, 0.1f, 100.0f);
        ProjMatrix.Elements[1][1] *= -1.0f;
    }
};

struct SGPUUniform
{
    Rr_Mat4 Model;
    Rr_Mat4 View;
    Rr_Mat4 Projection;
};

class CQuakeBSPApp
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *GeometryBuffer{};
    size_t ColorsOffset{};
    size_t IndicesOffset{};
    size_t IndexCount{};
    SHeader *Header{};
    SQuakeBSP QuakeBSP;
    CCamera Camera;

public:
    CQuakeBSPApp()
    {
        auto BSPAsset = Rr_LoadAsset(EXAMPLE_ASSET_E1M1_BSP);
        auto Raw = (std::byte *)BSPAsset.Data;
        Header = (SHeader *)Raw;
        assert(Header->Version == 0x1C);
        QuakeBSP = SQuakeBSP{
            .Planes = GetSpan<SPlane>(Raw, Header->Planes),
            .Vertices = GetSpan<SVertex>(Raw, Header->Vertices),
            .Nodes = GetSpan<SNode>(Raw, Header->Nodes),
            .Faces = GetSpan<SFace>(Raw, Header->Faces),
            .Leaves = GetSpan<SLeaf>(Raw, Header->Leaves),
            .Edges = GetSpan<SEdge>(Raw, Header->Edges),
            .Models = GetSpan<SModel>(Raw, Header->Models),
        };

        auto VertexAttributes0 = std::array{
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
        };

        auto VertexAttributes1 = std::array{
            Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_FLOAT3 },
        };

        auto VertexInputBindings = std::array{
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes0.size(),
                .Attributes = VertexAttributes0.data(),
            },
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes1.size(),
                .Attributes = VertexAttributes1.data(),
            },
        };

        auto ColorTarget = Rr_ColorTargetInfo{
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_QUAKEBSP_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_QUAKEBSP_FRAG_SPV);
        auto FragmentShaderInfo = Rr_ShaderInfo{
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        auto PipelineInfo = Rr_GraphicsPipelineCreateInfo{
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .VertexInputBindingCount = VertexInputBindings.size(),
            .VertexInputBindings = VertexInputBindings.data(),
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
            .Rasterizer =
                Rr_Rasterizer{
                    .CullMode = RR_CULL_MODE_BACK,
                },
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        auto constexpr CUBE_POSITIONS = std::array{
            1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
            1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
            1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,
            -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
        };
        auto constexpr CUBE_COLORS = std::array{
            1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        };
        auto constexpr CUBE_INDICES = std::array{
            1,  13, 19, 1,  19, 7,  9, 6, 18, 9, 18, 21, 23, 20, 14, 23, 14, 17,
            16, 4,  10, 16, 10, 22, 5, 2, 8,  5, 8,  11, 15, 12, 0,  15, 0,  3,
        };
        auto PositionsSize = sizeof(decltype(CUBE_POSITIONS)::value_type) * CUBE_POSITIONS.size();
        auto ColorsSize = sizeof(decltype(CUBE_COLORS)::value_type) * CUBE_COLORS.size();
        auto IndicesSize = sizeof(decltype(CUBE_INDICES)::value_type) * CUBE_INDICES.size();
        auto TotalSize = PositionsSize + ColorsSize + IndicesSize;
        auto StagingBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_STAGING);
        Rr_ReleaseBuffer(StagingBuffer);
        auto StagingData = (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
        ColorsOffset = PositionsSize;
        IndicesOffset = PositionsSize + ColorsSize;
        IndexCount = CUBE_INDICES.size();
        std::memcpy(StagingData, CUBE_POSITIONS.data(), PositionsSize);
        std::memcpy(StagingData + ColorsOffset, CUBE_COLORS.data(), ColorsSize);
        std::memcpy(StagingData + IndicesOffset, CUBE_INDICES.data(), IndicesSize);
        GeometryBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_INDEX_BIT);
        auto TransferNode = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(TransferNode, TotalSize, StagingBuffer, 0, GeometryBuffer, 0);

        UniformBuffer = Rr_CreateBuffer(sizeof(SGPUUniform), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);
    }

    void Iterate()
    {
        Rr_UIBeginDebugOverlayTabs();
        if (Rr_UIBeginWindow("QuakeBSP.cxx"))
        {
            Rr_UIText("This example shows rendering a Quake map.");
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto SwapchainImage = Rr_GetSwapchainImage();

        Camera.Update(Rr_GetImage2DAspect(SwapchainImage));

        auto GPUUniform = SGPUUniform{
            .Model = Rr_Rotate_RH(Rr_GetTimeSeconds(), Rr_V3(1.0f, 0.0f, 0.0f)) *
                     Rr_Rotate_RH(Rr_GetTimeSeconds(), Rr_V3(0.0f, 1.0f, 0.0f)),
            .View = Camera.GetViewMatrix(),
            .Projection = Camera.GetProjectionMatrix(),
        };
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &GPUUniform, sizeof(GPUUniform));

        auto ColorTarget = Rr_ColorTarget{
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, GeometryBuffer, 0, 0);
        Rr_BindVertexBuffer(GraphicsNode, GeometryBuffer, 1, ColorsOffset);
        Rr_BindIndexBuffer(GraphicsNode, GeometryBuffer, 0, IndicesOffset, RR_INDEX_TYPE_UINT32);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(GPUUniform));
        Rr_DrawIndexed(GraphicsNode, IndexCount, 1, 0, 0, 0);
    }

    ~CQuakeBSPApp()
    {
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(GeometryBuffer);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    }
};

int main()
{
    static CQuakeBSPApp *App{};

    auto Config = Rr_Config{
        .WindowTitle = "QuakeBSP",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CQuakeBSPApp(); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
