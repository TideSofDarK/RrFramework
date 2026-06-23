#include <Rr/Rr.h>

#include "ExampleAssets.inc"

#include <array>
#include <cassert>
#include <span>
#include <vector>

struct SBBox
{
    Rr_Vec3 Min;
    Rr_Vec3 Max;
};

struct SBBoxShort
{
    int16_t Min[3];
    int16_t Max[3];
};

struct SPlane
{
    Rr_Vec3 Normal;
    float Distance;
    int32_t Type;
};

using SVertex = Rr_Vec3;

struct SEdge
{
    uint16_t Vertex0;
    uint16_t Vertex1;
};

struct SFace
{
    uint16_t PlaneIndex;
    uint16_t Side;
    int32_t EdgeListIndex;
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
    int32_t VisListIndex;
    SBBoxShort Bound;
    uint16_t FaceListIndex;
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
    std::span<SPlane const> Planes;
    std::span<SVertex const> Vertices;
    std::span<uint8_t const> VisList;
    std::span<SNode const> Nodes;
    std::span<SFace const> Faces;
    std::span<uint16_t const> FaceList;
    std::span<SLeaf const> Leaves;
    std::span<SEdge const> Edges;
    std::span<int32_t const> EdgeList;
    std::span<SModel const> Models;
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

    Rr_Mat4 Transform = Rr_M4D(1.0f);
    Rr_Mat4 ProjMatrix = Rr_M4D(1.0f);

public:
    Rr_Vec3 Position{ 540.0f, 260.0f, 100.0f };

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

            auto Speed = 500.0f;
            if (Rr_IsScancodePressed(RR_SCANCODE_LSHIFT))
            {
                Speed *= 2.0f;
            }
            auto CameraForward = GetForwardVector();
            auto CameraLeft = GetRightVector();
            if (Rr_IsScancodePressed(RR_SCANCODE_W))
            {
                Position += CameraForward * Speed * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_A))
            {
                Position -= CameraLeft * Speed * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_S))
            {
                Position -= CameraForward * Speed * DeltaTime;
            }
            if (Rr_IsScancodePressed(RR_SCANCODE_D))
            {
                Position += CameraLeft * Speed * DeltaTime;
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

        auto constexpr SWAP_YZ = Rr_Mat4{
            1, 0, 0, 0, //
            0, 0, 1, 0, //
            0, 1, 0, 0, //
            0, 0, 0, 1  //
        };
        Transform = Rr_TranslateV(Position) * Rr_Rotate_LH(-Yaw, Rr_V3(0.0f, 0.0f, 1.0f)) *
                    Rr_Rotate_LH(-Pitch, Rr_V3(1.0f, 0.0f, 0.0f)) * SWAP_YZ;
        ProjMatrix = Rr_Perspective_LH(FieldOfView, Aspect, 0.1f, 100000.0f);
        ProjMatrix[1][1] *= -1.0f;
    }
};

struct SGPUUniform
{
    Rr_Mat4 Model;
    Rr_Mat4 View;
    Rr_Mat4 Projection;
};

struct SGPUVertex
{
    Rr_Vec3 Position;
    Rr_Vec3 Normal;
};

class CQuakeBSPApp
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Image2D *DepthImage{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *GeometryBuffer{};
    SHeader *Header{};
    SQuakeBSP QuakeBSP;
    CCamera Camera;
    uint16_t CameraLeafIndex{};
    std::vector<SGPUVertex> GPUVertices{};
    std::vector<size_t> VisibleFaces{};

    void GetFaceTriangles(SFace const &Face, std::vector<SGPUVertex> &OutVertices)
    {
        auto &Plane = QuakeBSP.Planes[Face.PlaneIndex];
        SVertex First;
        for (int EdgeListOffset = 0; EdgeListOffset < Face.EdgeCount; ++EdgeListOffset)
        {
            auto EdgeIndex = QuakeBSP.EdgeList[Face.EdgeListIndex + EdgeListOffset];
            auto &Edge = QuakeBSP.Edges[EdgeIndex < 0 ? -EdgeIndex : EdgeIndex];
            auto Vertex0 = QuakeBSP.Vertices[Edge.Vertex0];
            auto Vertex1 = QuakeBSP.Vertices[Edge.Vertex1];
            if (EdgeIndex < 0)
            {
                std::swap(Vertex0, Vertex1);
            }
            if (EdgeListOffset == 0)
            {
                First = Vertex0;
            }
            else
            {
                OutVertices.emplace_back(First, Plane.Normal);
                OutVertices.emplace_back(Vertex0, Plane.Normal);
                OutVertices.emplace_back(Vertex1, Plane.Normal);
            }
        }
    }

    SLeaf const &FindCameraLeaf(SModel const &Map)
    {
        auto NodeIndex = (uint16_t)Map.FirstBSPNodeIndex;
        while ((NodeIndex & (1 << 15)) == 0)
        {
            auto &Node = QuakeBSP.Nodes[NodeIndex];
            auto &Plane = QuakeBSP.Planes[Node.PlaneIndex];

            auto Distance = Rr_DotV3(Camera.Position, Plane.Normal) - Plane.Distance;

            if (Distance >= 0.0f)
            {
                NodeIndex = Node.Front;
            }
            else
            {
                NodeIndex = Node.Back;
            }
        }

        CameraLeafIndex = (uint16_t)~NodeIndex;

        return QuakeBSP.Leaves[CameraLeafIndex];
    }

    void InitDepthImage(void)
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
    CQuakeBSPApp()
    {
        auto BSPAsset = Rr_LoadAsset(EXAMPLE_ASSET_E1M1_BSP);
        auto Raw = (std::byte *)std::memcpy(std::malloc(BSPAsset.Size), BSPAsset.Data, BSPAsset.Size);
        Header = (SHeader *)Raw;
        assert(Header->Version >= 0x1C);
        QuakeBSP = SQuakeBSP{
            .Planes = GetSpan<SPlane>(Raw, Header->Planes),
            .Vertices = GetSpan<SVertex>(Raw, Header->Vertices),
            .VisList = GetSpan<uint8_t>(Raw, Header->VisList),
            .Nodes = GetSpan<SNode>(Raw, Header->Nodes),
            .Faces = GetSpan<SFace>(Raw, Header->Faces),
            .FaceList = GetSpan<uint16_t>(Raw, Header->FaceList),
            .Leaves = GetSpan<SLeaf>(Raw, Header->Leaves),
            .Edges = GetSpan<SEdge>(Raw, Header->Edges),
            .EdgeList = GetSpan<int32_t>(Raw, Header->EdgeList),
            .Models = GetSpan<SModel>(Raw, Header->Models),
        };

        auto VertexAttributes = std::array{
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
            Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_FLOAT3 },
        };

        auto VertexInputBindings = std::array{
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
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
                    .FrontFace = RR_FRONT_FACE_CLOCKWISE,
                },
            .DepthStencil =
                Rr_DepthStencil{
                    .Format = RR_IMAGE_FORMAT_D32_SFLOAT,
                    .CompareOp = RR_COMPARE_OP_LESS,
                    .EnableDepthTest = true,
                    .EnableDepthWrite = true,
                },
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        UniformBuffer = Rr_CreateBuffer(sizeof(SGPUUniform), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);
        GeometryBuffer = Rr_CreateBuffer(RR_MEBIBYTES(4), RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_DYNAMIC);

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
        if (Rr_UIBeginWindow("QuakeBSP.cxx"))
        {
            Rr_UIText("This example shows rendering a Quake map.");
            Rr_UIInputFloat3("Camera Position", Camera.Position.Elements);
            Rr_UITextF("Camera Leaf Index: %d\nVisible Face Count: %zu", CameraLeafIndex, VisibleFaces.size());
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto SwapchainImage = Rr_GetSwapchainImage();

        Camera.Update(Rr_GetImage2DAspect(SwapchainImage));

        GPUVertices.clear();
        VisibleFaces.clear();
        auto &Map = QuakeBSP.Models[0];
        auto &Leaf = FindCameraLeaf(Map);
        auto VisListIndex = Leaf.VisListIndex;
        for (auto Index = 1; Index < Map.LeafCount; ++VisListIndex)
        {
            if (QuakeBSP.VisList[VisListIndex] == 0)
            {
                ++VisListIndex;
                Index += 8 * QuakeBSP.VisList[VisListIndex];
            }
            else
            {
                for (uint8_t Bit = 1; Bit != 0; Bit *= 2, ++Index)
                {
                    if (QuakeBSP.VisList[VisListIndex] & Bit)
                    {
                        auto &VisibleLeaf = QuakeBSP.Leaves[Index];
                        auto First = VisibleLeaf.FaceListIndex;
                        auto Last = First + VisibleLeaf.FaceCount;
                        for (auto FaceListIndex = First; FaceListIndex < Last; ++FaceListIndex)
                        {
                            VisibleFaces.emplace_back(QuakeBSP.FaceList[FaceListIndex]);
                        }
                    }
                }
            }
        }
        for (auto FaceIndex : VisibleFaces)
        {
            GetFaceTriangles(QuakeBSP.Faces[FaceIndex], GPUVertices);
        }
        std::memcpy(
            Rr_GetMappedBufferData(GeometryBuffer),
            GPUVertices.data(),
            sizeof(SGPUVertex) * GPUVertices.size());

        auto GPUUniform = SGPUUniform{
            .Model = Rr_M4D(1.0f),
            .View = Camera.GetViewMatrix(),
            .Projection = Camera.GetProjectionMatrix(),
        };
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &GPUUniform, sizeof(GPUUniform));

        auto ColorTarget = Rr_ColorTarget{
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
        };
        auto DepthTarget = Rr_DepthTarget{
            .Image = DepthImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_DONT_CARE,
            .Clear = Rr_DepthClear{ 1.0f, 0 },
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, &DepthTarget);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, GeometryBuffer, 0, 0);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(GPUUniform));
        Rr_Draw(GraphicsNode, GPUVertices.size(), 1, 0, 0);
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
        .EventFunc = [](Rr_Event const *Event) { App->Event(Event); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
