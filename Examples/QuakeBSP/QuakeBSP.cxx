#include <Rr/Rr.h>

#include "ExampleAssets.inc"

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <span>
#include <unordered_set>
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
    uint16_t SurfaceIndex;
    uint8_t Lights[4];
    int32_t LightmapOffset;
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

struct SSurface
{
    Rr_Vec3 VectorS;
    float DistanceS;
    Rr_Vec3 VectorT;
    float DistanceT;
    uint32_t MipTexIndex;
    uint32_t Animated;
};

struct SMipHeader
{
    int32_t Count;
    int32_t Offsets[];
};

struct SMipTex
{
    char Name[16];
    uint32_t Width;
    uint32_t Height;
    uint32_t Offset1;
    uint32_t Offset2;
    uint32_t Offset4;
    uint32_t Offset8;
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
    SEntry Surfaces;
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
    SMipHeader const *MipHeader;
    std::span<SVertex const> Vertices;
    std::span<uint8_t const> VisList;
    std::span<SNode const> Nodes;
    std::span<SSurface const> Surfaces;
    std::span<SFace const> Faces;
    std::span<uint8_t const> Lightmaps;
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
    float Time;
    float Anim0;
    float Anim1;
    float Anim2;
};

struct SGPUVertex
{
    Rr_Vec3 Position;
    uint32_t SurfaceIndex;
    Rr_Vec3 Normal;
    uint32_t TextureIndex;
    uint32_t FaceIndex;
};

struct SGPUSurface
{
    Rr_Vec3 VectorX;
    float DistanceX;
    Rr_Vec3 VectorY;
    float DistanceY;
    int32_t Sky;
    int32_t Water;
    int32_t Unused1;
    int32_t Unused2;
};

struct SGPUFace
{
    Rr_IntVec4 Lights;
    Rr_IntVec2 LightmapSize;
    int32_t DataOffset;
    int32_t Unused0;
    Rr_Vec2 MinUV;
    Rr_Vec2 MaxUV;
    Rr_Vec2 MidPolyUV;
    float Unused1;
    float Unused2;
};

class CQuakeBSPApp
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Image2D *ColorImage{};
    Rr_Image2D *DepthImage{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *VertexBuffer{};
    Rr_Buffer *IndexBuffer{};
    Rr_Buffer *TextureBuffer{};
    Rr_Buffer *SurfaceBuffer{};
    Rr_Buffer *LightmapBuffer{};
    Rr_Buffer *FacesBuffer{};
    SHeader *Header{};
    SQuakeBSP QuakeBSP;
    Rr_Image2D *ColormapImage{};
    Rr_Image2D *AtlasImage{};
    CCamera Camera;
    uint16_t CameraLeafIndex{};
    std::vector<SGPUVertex> Vertices{};
    std::vector<uint16_t> Indices{};
    std::unordered_set<uint32_t> VisibleFaces{};
    bool LowResMode{};

    void GetFaceTriangles(uint32_t FaceIndex)
    {
        auto &Map = QuakeBSP.Models[0];
        auto &Face = QuakeBSP.Faces[FaceIndex];
        auto &Surface = QuakeBSP.Surfaces[Face.SurfaceIndex];
        auto &Plane = QuakeBSP.Planes[Face.PlaneIndex];
        auto FirstVertex = Vertices.size();
        for (auto Index = 0; Index < Face.EdgeCount; ++Index)
        {
            auto EdgeIndex = QuakeBSP.EdgeList[Face.EdgeListIndex + Index];
            auto &Edge = QuakeBSP.Edges[EdgeIndex >= 0 ? EdgeIndex : -EdgeIndex];
            auto Vertex = QuakeBSP.Vertices[EdgeIndex >= 0 ? Edge.Vertex0 : Edge.Vertex1];
            Vertices.emplace_back(
                Vertex,
                (uint32_t)Face.SurfaceIndex,
                Plane.Normal,
                (uint32_t)Surface.MipTexIndex,
                FaceIndex);
        }
        for (auto Index = FirstVertex + 2; Index < Vertices.size(); ++Index)
        {
            Indices.emplace_back((uint16_t)FirstVertex);
            Indices.emplace_back((uint16_t)Index);
            Indices.emplace_back((uint16_t)Index - 1);
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

    void InitBSP()
    {
        auto BSPAsset = Rr_LoadAsset(EXAMPLE_ASSET_START_BSP);
        auto Raw = (std::byte *)std::memcpy(std::malloc(BSPAsset.Size), BSPAsset.Data, BSPAsset.Size);
        Header = (SHeader *)Raw;
        assert(Header->Version >= 0x1C);
        QuakeBSP = SQuakeBSP{
            .Planes = GetSpan<SPlane>(Raw, Header->Planes),
            .MipHeader = (SMipHeader *)(Raw + Header->MipTextures.Offset),
            .Vertices = GetSpan<SVertex>(Raw, Header->Vertices),
            .VisList = GetSpan<uint8_t>(Raw, Header->VisList),
            .Nodes = GetSpan<SNode>(Raw, Header->Nodes),
            .Surfaces = GetSpan<SSurface>(Raw, Header->Surfaces),
            .Faces = GetSpan<SFace>(Raw, Header->Faces),
            .Lightmaps = GetSpan<uint8_t>(Raw, Header->Lightmaps),
            .FaceList = GetSpan<uint16_t>(Raw, Header->FaceList),
            .Leaves = GetSpan<SLeaf>(Raw, Header->Leaves),
            .Edges = GetSpan<SEdge>(Raw, Header->Edges),
            .EdgeList = GetSpan<int32_t>(Raw, Header->EdgeList),
            .Models = GetSpan<SModel>(Raw, Header->Models),
        };
    }

    void InitFaces()
    {
        auto &Map = QuakeBSP.Models[0];
        auto FirstFace = Map.FaceIndex;
        auto LastFace = Map.FaceIndex + Map.FaceCount - 1;
        auto Faces = std::vector<SGPUFace>{};
        Faces.resize(Map.FaceIndex + Map.FaceCount);
        for (auto Index = FirstFace; Index <= LastFace; ++Index)
        {
            auto &Face = QuakeBSP.Faces[Index];
            auto &Surface = QuakeBSP.Surfaces[Face.SurfaceIndex];
            auto &Plane = QuakeBSP.Planes[Face.PlaneIndex];

            if (Index == 3544)
            {
                int g = 23 + 23;
            }

            std::vector<Rr_Vec2> VertexTexCoords{};
            VertexTexCoords.resize(Face.EdgeCount);
            for (auto EdgeListIndex = 0; EdgeListIndex < Face.EdgeCount; ++EdgeListIndex)
            {
                auto EdgeIndex = QuakeBSP.EdgeList[Face.EdgeListIndex + EdgeListIndex];
                auto &Edge = QuakeBSP.Edges[EdgeIndex >= 0 ? EdgeIndex : -EdgeIndex];
                auto Vertex = QuakeBSP.Vertices[EdgeIndex >= 0 ? Edge.Vertex0 : Edge.Vertex1];
                VertexTexCoords[EdgeListIndex] = Rr_V2(
                    Rr_DotV3(Vertex, Surface.VectorS) + Surface.DistanceS,
                    Rr_DotV3(Vertex, Surface.VectorT) + Surface.DistanceT);
            }

            Rr_Vec2 MinTexCoord = Rr_V2F(9999999);
            Rr_Vec2 MaxTexCoord = Rr_V2F(-9999999);
            for (auto VertexTexCoord : VertexTexCoords)
            {
                MinTexCoord.X = std::min(std::floor(VertexTexCoord.X), MinTexCoord.X);
                MinTexCoord.Y = std::min(std::floor(VertexTexCoord.Y), MinTexCoord.Y);
                MaxTexCoord.X = std::max(std::ceil(VertexTexCoord.X), MaxTexCoord.X);
                MaxTexCoord.Y = std::max(std::ceil(VertexTexCoord.Y), MaxTexCoord.Y);
            }

            if (Face.LightmapOffset == -1)
            {
                Faces[Index] = SGPUFace{
                    .Lights = Rr_IntV4(Face.Lights[0], Face.Lights[1], Face.Lights[2], Face.Lights[3]),
                    .DataOffset = -1,
                };

                continue;
            }

            auto LightmapSize = Rr_V2(
                std::ceil(MaxTexCoord.X / 16.0f) - std::floor(MinTexCoord.X / 16.0f) + 1,
                std::ceil(MaxTexCoord.Y / 16.0f) - std::floor(MinTexCoord.Y / 16.0f) + 1);

            Faces[Index] = SGPUFace{
                .Lights = Rr_IntV4(Face.Lights[0], Face.Lights[1], Face.Lights[2], Face.Lights[3]),
                .LightmapSize = Rr_CastIntV2(LightmapSize),
                .DataOffset = Face.LightmapOffset,
                .MinUV = MinTexCoord,
                .MaxUV = MaxTexCoord,
                .MidPolyUV = Rr_DivV2F(Rr_AddV2(MinTexCoord, MaxTexCoord), 2.0f),
            };
        }
        FacesBuffer =
            Rr_CreateBuffer(sizeof(SGPUFace) * Faces.size(), RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_STAGING);
        std::memcpy(Rr_GetMappedBufferData(FacesBuffer), Faces.data(), sizeof(SGPUFace) * Faces.size());

        auto LightmapData = std::vector<float>{};
        LightmapData.reserve(QuakeBSP.Lightmaps.size());
        for (auto const &Lightmap : QuakeBSP.Lightmaps)
        {
            LightmapData.emplace_back((float)Lightmap / 255.0f);
        }
        LightmapBuffer =
            Rr_CreateBuffer(sizeof(float) * LightmapData.size(), RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_STAGING);
        std::memcpy(Rr_GetMappedBufferData(LightmapBuffer), LightmapData.data(), sizeof(float) * LightmapData.size());
    }

    void InitColormap()
    {
        int32_t Width, Height, Channels;
        auto Asset = Rr_LoadAsset(EXAMPLE_ASSET_COLORMAP_PNG);
        auto DesiredChannels = 4;
        auto Data = (std::byte *)stbi_load_from_memory(
            (stbi_uc *)Asset.Data,
            (int32_t)Asset.Size,
            &Width,
            &Height,
            &Channels,
            DesiredChannels);
        assert(Channels = 4);
        auto Size = Channels * Width * Height;

        auto StagingBuffer = Rr_CreateBuffer(Size, RR_BUFFER_FLAGS_STAGING);
        Rr_ReleaseBuffer(StagingBuffer);
        memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, Size);
        stbi_image_free(Data);

        auto Extent = Rr_IntV2(Width, Height);
        ColormapImage = Rr_CreateImage2D(
            Extent,
            RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
            RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_STORAGE_BIT);
        Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, Extent, ColormapImage, 0);
    }

    void InitAtlas()
    {
        auto AtlasWidth = 1024;
        auto AtlasHeight = 1024;
        auto AtlasChannels = 1;

        auto StagingSize = AtlasWidth * AtlasHeight * AtlasChannels;
        auto StagingBuffer = Rr_CreateBuffer(StagingSize, RR_BUFFER_FLAGS_STAGING);
        Rr_ReleaseBuffer(StagingBuffer);
        auto StagingData = (uint8_t *)Rr_GetMappedBufferData(StagingBuffer);

        auto CurrentX = 0;
        auto CurrentY = 0;
        auto MaxHeight = 0u;

        auto GPUTextures = std::vector<Rr_IntVec4>{};
        for (auto Index = 0; Index < QuakeBSP.MipHeader->Count; ++Index)
        {
            auto Offset = QuakeBSP.MipHeader->Offsets[Index];
            auto MipTexRaw = (std::byte *)QuakeBSP.MipHeader + Offset;
            auto MipTex = (SMipTex *)MipTexRaw;
            auto Mip0ColorIndices = (uint8_t *)MipTexRaw + MipTex->Offset1;

            if (CurrentX + MipTex->Width >= AtlasWidth)
            {
                CurrentY += MaxHeight;
                MaxHeight = 0;
                CurrentX = 0;
            }

            assert(CurrentY + MipTex->Height < AtlasHeight);

            for (auto Y = 0; Y < MipTex->Height; ++Y)
            {
                for (auto X = 0; X < MipTex->Width; ++X)
                {
                    auto MipIndex = Y * MipTex->Width + X;
                    auto MipPixelColorIndex = Mip0ColorIndices[MipIndex];

                    auto AtlasIndex = (CurrentY + Y) * AtlasWidth + CurrentX + X;
                    StagingData[AtlasIndex] = MipPixelColorIndex;
                }
            }

            GPUTextures.emplace_back(Rr_IntV4(CurrentX, CurrentY, MipTex->Width, MipTex->Height));

            MaxHeight = std::max(MaxHeight, MipTex->Height);
            CurrentX += MipTex->Width;
        }
        AtlasImage = Rr_CreateImage2D(
            Rr_IntV2(AtlasWidth, AtlasHeight),
            RR_IMAGE_FORMAT_R8_UINT,
            RR_IMAGE_FLAGS_STORAGE_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
        Rr_CopyBufferToImage2D(Rr_GetGraph(), StagingBuffer, 0, Rr_IntV2(AtlasWidth, AtlasHeight), AtlasImage, 0);
        TextureBuffer = Rr_CreateBuffer(
            sizeof(Rr_IntVec4) * GPUTextures.size(),
            RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_STAGING);
        std::memcpy(Rr_GetMappedBufferData(TextureBuffer), GPUTextures.data(), sizeof(Rr_IntVec4) * GPUTextures.size());

        auto GPUSurfaces = std::vector<SGPUSurface>{};
        for (auto &Surface : QuakeBSP.Surfaces)
        {
            auto Offset = QuakeBSP.MipHeader->Offsets[Surface.MipTexIndex];
            auto MipTexRaw = (std::byte *)QuakeBSP.MipHeader + Offset;
            auto MipTex = (SMipTex *)MipTexRaw;
            auto Sky = !std::strncmp("sky", MipTex->Name, 3);
            auto Water = MipTex->Name[0] == '*';
            GPUSurfaces.emplace_back(
                SGPUSurface{
                    .VectorX = Surface.VectorS,
                    .DistanceX = Surface.DistanceS,
                    .VectorY = Surface.VectorT,
                    .DistanceY = Surface.DistanceT,
                    .Sky = Sky,
                    .Water = Water,
                });
        }
        SurfaceBuffer = Rr_CreateBuffer(
            sizeof(SGPUSurface) * GPUSurfaces.size(),
            RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_STAGING);
        std::memcpy(
            Rr_GetMappedBufferData(SurfaceBuffer),
            GPUSurfaces.data(),
            sizeof(SGPUSurface) * GPUSurfaces.size());
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
    CQuakeBSPApp()
    {
        InitBSP();
        InitColormap();
        InitAtlas();
        InitFaces();

        auto VertexAttributes = std::array{
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
            Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_UINT },
            Rr_VertexInputAttribute{ .Location = 2, .Format = RR_FORMAT_FLOAT3 },
            Rr_VertexInputAttribute{ .Location = 3, .Format = RR_FORMAT_UINT },
            Rr_VertexInputAttribute{ .Location = 4, .Format = RR_FORMAT_UINT },
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
                    .FrontFace = RR_FRONT_FACE_COUNTER_CLOCKWISE,
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
        VertexBuffer = Rr_CreateBuffer(RR_MEBIBYTES(2), RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_DYNAMIC);
        IndexBuffer = Rr_CreateBuffer(RR_MEBIBYTES(2), RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_DYNAMIC);
        ColorImage = Rr_CreateImage2D(
            Rr_IntV2(320, 240),
            Rr_GetImageFormat(Rr_GetSwapchainImage()),
            RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);

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
            Rr_UICheckbox("Low Resolution Mode", &LowResMode);
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto Graph = Rr_GetGraph();

        auto SwapchainImage = Rr_GetSwapchainImage();
        auto SwapchainSize = Rr_GetImage2DExtent(SwapchainImage);

        Camera.Update(Rr_GetImage2DAspect(SwapchainImage));

        Vertices.clear();
        Indices.clear();
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
                            VisibleFaces.insert(QuakeBSP.FaceList[FaceListIndex]);
                        }
                    }
                }
            }
        }
        for (auto FaceIndex : VisibleFaces)
        {
            GetFaceTriangles(FaceIndex);
        }
        std::memcpy(Rr_GetMappedBufferData(VertexBuffer), Vertices.data(), sizeof(SGPUVertex) * Vertices.size());
        std::memcpy(Rr_GetMappedBufferData(IndexBuffer), Indices.data(), sizeof(uint16_t) * Indices.size());

        auto GPUUniform = SGPUUniform{
            .Model = Rr_M4D(1.0f),
            .View = Camera.GetViewMatrix(),
            .Projection = Camera.GetProjectionMatrix(),
            .Time = (float)Rr_GetTimeSeconds(),
        };
        std::memcpy(Rr_GetMappedBufferData(UniformBuffer), &GPUUniform, sizeof(GPUUniform));

        auto ColorTarget = Rr_ColorTarget{
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
        };
        if (LowResMode)
        {
            ColorTarget.Image = ColorImage;
        }
        auto DepthTarget = Rr_DepthTarget{
            .Image = DepthImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_DONT_CARE,
            .Clear = Rr_DepthClear{ 1.0f, 0 },
        };
        auto GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, &DepthTarget);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, VertexBuffer, 0, 0);
        Rr_BindIndexBuffer(GraphicsNode, IndexBuffer, 0, 0, RR_INDEX_TYPE_UINT16);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(GPUUniform));
        Rr_BindStorageBuffer(GraphicsNode, SurfaceBuffer, 0, 1, 0, Rr_GetBufferSize(SurfaceBuffer));
        Rr_BindStorageBuffer(GraphicsNode, TextureBuffer, 0, 2, 0, Rr_GetBufferSize(TextureBuffer));
        Rr_BindStorageBuffer(GraphicsNode, LightmapBuffer, 0, 3, 0, Rr_GetBufferSize(LightmapBuffer));
        Rr_BindStorageBuffer(GraphicsNode, FacesBuffer, 0, 4, 0, Rr_GetBufferSize(FacesBuffer));
        Rr_BindStorageImage2D(GraphicsNode, ColormapImage, 1, 0);
        Rr_BindStorageImage2D(GraphicsNode, AtlasImage, 1, 1);
        Rr_DrawIndexed(GraphicsNode, Indices.size(), 1, 0, 0, 0);

        if (!LowResMode)
        {
            return;
        }

        auto SrcRect = Rr_IntRect{ 0, 0, 320, 240 };
        auto DstRect = Rr_IntRect{ 0, 0, SwapchainSize.X, SwapchainSize.Y };
        auto DstRectFit = Rr_FitIntRect(&SrcRect, &DstRect);
        Rr_BlitImage2DEx(
            Graph,
            ColorImage,
            0,
            SrcRect,
            SwapchainImage,
            0,
            DstRectFit,
            RR_IMAGE_ASPECT_COLOR_BIT,
            RR_FILTER_NEAREST);
    }

    ~CQuakeBSPApp()
    {
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(VertexBuffer);
        Rr_ReleaseBuffer(IndexBuffer);
        Rr_ReleaseBuffer(SurfaceBuffer);
        Rr_ReleaseBuffer(TextureBuffer);
        Rr_ReleaseBuffer(LightmapBuffer);
        Rr_ReleaseBuffer(FacesBuffer);
        Rr_ReleaseImage(ColormapImage);
        Rr_ReleaseImage(AtlasImage);
        Rr_ReleaseImage(ColorImage);
        Rr_ReleaseImage(DepthImage);
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
