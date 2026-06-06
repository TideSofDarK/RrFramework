#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define CGLTF_IMPLEMENTATION
#include "../../Vendor/cgltf/cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <webp/decode.h>

#include <array>
#include <format>
#include <functional>
#include <span>
#include <thread>
#include <vector>

Rr_Mat4 constexpr FLIP_Y_MATRIX = { 1.0f, 0.0f,  0.0f, 0.0f,
                                    0.0f, -1.0f, 0.0f, 0.0f, //
                                    0.0f, 0.0f,  1.0f, 0.0f, //
                                    0.0f, 0.0f,  0.0f, 1.0f };

std::array constexpr GENERIC_VERTEX_ATTRIBUTES = {
    Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_VEC3 },
    Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_VEC2 },
    Rr_VertexInputAttribute{ .Location = 2, .Format = RR_FORMAT_VEC3 },
    Rr_VertexInputAttribute{ .Location = 3, .Format = RR_FORMAT_VEC4 },
};
std::array constexpr GENERIC_VERTEX_INPUT_BINDINGS = {
    Rr_VertexInputBinding{
        .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
        .AttributeCount = GENERIC_VERTEX_ATTRIBUTES.size(),
        .Attributes = GENERIC_VERTEX_ATTRIBUTES.data(),
    },
};

float constexpr NEAR_PLANE = 0.1f;
float constexpr FAR_PLANE = 100.0f;

std::size_t constexpr MAX_POINT_LIGHTS = 4;
std::size_t constexpr MAX_SPOT_LIGHTS = 4;

Rr_ImageFormat constexpr DEPTH_FORMAT = RR_IMAGE_FORMAT_D32_SFLOAT;

static Rr_Image2D *LoadImage2D(
    void const *Data,
    size_t Size,
    Rr_IntVec2 Extent,
    Rr_ImageFormat Format,
    bool Mipmaps,
    Rr_Graph *Graph)
{
    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        Size,
        RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_STAGING_BIT);
    Rr_ReleaseBuffer(StagingBuffer);
    memcpy(Rr_GetMappedBufferData(StagingBuffer), Data, Size);

    auto Flags = RR_IMAGE_FLAGS_TRANSFER_BIT | RR_IMAGE_FLAGS_SAMPLED_BIT;
    if (Mipmaps)
    {
        Flags |= RR_IMAGE_FLAGS_MIP_MAPPED_BIT;
    }
    auto Image2D = Rr_CreateImage2D(Extent, Format, Flags);
    Rr_CopyBufferToImage2D(Graph, StagingBuffer, 0, Extent, Image2D, 0);

    return Image2D;
}

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
        ProjMatrix = Rr_Perspective_RH(
                         RR_ANGLE_DEG(FOVDegrees),
                         (float)Size.X / (float)Size.Y,
                         NEAR_PLANE,
                         FAR_PLANE) *
                     FLIP_Y_MATRIX;
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

        Transform = Rr_Translate(Position) *
                    Rr_Rotate_RH(RR_ANGLE_DEG(Yaw), Rr_V3(0.0f, 1.0f, 0.0f)) *
                    Rr_Rotate_RH(RR_ANGLE_DEG(Pitch), Rr_V3(1.0f, 0.0f, 0.0f));
    }
};

class CGLTFScene
{
    enum class ETextureType
    {
        COLOR,
        NORMAL,
        ROUGHNESS_METALLIC
    };

    struct SMaterial
    {
        Rr_Image2D *Color{};
        Rr_Image2D *Normal{};
        Rr_Image2D *RoughnessMetallic{};
    };

    struct SVertex
    {
        Rr_Vec3 Position;
        Rr_Vec2 UV;
        Rr_Vec3 Normal;
        Rr_Vec4 Tangent;
    };

    struct SPrimitive
    {
        uint32_t IndexCount;
        uint32_t FirstIndex;
        int32_t VertexOffset;
        uint32_t MaterialIndex;
    };

    struct SMesh
    {
        std::vector<SPrimitive> Primitives;
    };

    struct alignas(256) SGPUMaterial
    {
        uint32_t AlphaMode;
        float AlphaCutoff;
        float Padding0;
        float Padding1;
    };

    Rr_Sampler *Sampler{};
    Rr_Buffer *MeshBuffer{};
    Rr_Buffer *ModelBuffer{};
    Rr_Buffer *MaterialBuffer{};
    size_t IndexOffset{};
    std::vector<SMesh> Meshes;
    std::vector<SMaterial> Materials;
    std::vector<size_t> MeshesToDraw;

    static Rr_Image2D *LoadTexture(
        cgltf_texture *Texture,
        Rr_ImageFormat Format,
        Rr_Graph *Graph)
    {
        int32_t ImageWidth;
        int32_t ImageHeight;
        int32_t ImageChannels;
        void *Data{};
        if (Texture->has_webp)
        {
            Data = WebPDecodeRGBA(
                (uint8_t const *)
                        Texture->webp_image->buffer_view->buffer->data +
                    Texture->webp_image->buffer_view->offset,
                (size_t)Texture->webp_image->buffer_view->size,
                &ImageWidth,
                &ImageHeight);
        }
        else
        {
            Data = (char *)stbi_load_from_memory(
                (stbi_uc const *)Texture->image->buffer_view->buffer->data +
                    Texture->image->buffer_view->offset,
                (int)Texture->image->buffer_view->size,
                &ImageWidth,
                &ImageHeight,
                &ImageChannels,
                4);
        }
        size_t DataSize = 4 * ImageWidth * ImageHeight;

        return LoadImage2D(
            Data,
            DataSize,
            Rr_IntV2(ImageWidth, ImageHeight),
            Format,
            true,
            Graph);
    }

    template <ETextureType Type>
    static void LoadTextures(
        cgltf_data *Data,
        std::vector<SMaterial> &Materials)
    {
        Rr_InitThreadContext();

        auto Graph = Rr_BeginGraph(RR_QUEUE_TYPE_DEDICATED_TRANSFER);

        for (auto Index = 0; Index < Data->materials_count; ++Index)
        {
            auto &Material = Data->materials[Index];

            if constexpr (Type == ETextureType::COLOR)
            {
                auto Texture =
                    Material.pbr_metallic_roughness.base_color_texture.texture;
                if (!Texture)
                {
                    continue;
                }

                auto Image = LoadTexture(
                    Material.pbr_metallic_roughness.base_color_texture.texture,
                    RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
                    Graph);
                Rr_TransferImageToQueue(Graph, Image, RR_QUEUE_TYPE_MAIN);
                Materials[Index].Color = Image;
            }

            if constexpr (Type == ETextureType::NORMAL)
            {
                auto Texture = Material.normal_texture.texture;
                if (!Texture)
                {
                    continue;
                }

                auto Image = LoadTexture(
                    Material.normal_texture.texture,
                    RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
                    Graph);
                Rr_TransferImageToQueue(Graph, Image, RR_QUEUE_TYPE_MAIN);
                Materials[Index].Normal = Image;
            }

            if constexpr (Type == ETextureType::ROUGHNESS_METALLIC)
            {
                auto Texture = Material.pbr_metallic_roughness
                                   .metallic_roughness_texture.texture;
                if (!Texture)
                {
                    continue;
                }

                auto Image = LoadTexture(
                    Material.pbr_metallic_roughness.metallic_roughness_texture
                        .texture,
                    RR_IMAGE_FORMAT_R8G8B8A8_UNORM,
                    Graph);
                Rr_TransferImageToQueue(Graph, Image, RR_QUEUE_TYPE_MAIN);
                Materials[Index].RoughnessMetallic = Image;
            }
        }

        Rr_EndGraph(Graph);

        Rr_CleanupThreadContext();
    }

public:
    template <uint32_t MaterialSetIndex = UINT32_MAX>
    void Draw(Rr_GraphNode *Node, uint32_t ModelSet, uint32_t ModelBinding)
    {
        Rr_BindVertexBuffer(Node, MeshBuffer, 0, 0);
        Rr_BindIndexBuffer(
            Node,
            MeshBuffer,
            0,
            IndexOffset,
            RR_INDEX_TYPE_UINT16);
        Rr_BindStorageBuffer(
            Node,
            ModelBuffer,
            ModelSet,
            ModelBinding,
            0,
            Rr_GetBufferSize(ModelBuffer));
        uint32_t FirstInstance = 0;
        for (auto const &MeshIndex : MeshesToDraw)
        {
            const auto &Mesh = Meshes[MeshIndex];
            for (auto const &Primitive : Mesh.Primitives)
            {
                if constexpr (MaterialSetIndex != UINT32_MAX)
                {
                    auto const &Material = Materials[Primitive.MaterialIndex];
                    Rr_BindUniformBuffer(
                        Node,
                        MaterialBuffer,
                        MaterialSetIndex,
                        0,
                        Primitive.MaterialIndex * sizeof(SGPUMaterial),
                        sizeof(SGPUMaterial));
                    Rr_BindCombinedImage2DSampler(
                        Node,
                        Material.Color,
                        Sampler,
                        MaterialSetIndex,
                        1);
                    if (!Material.Normal)
                    {
                        continue;
                    }
                    Rr_BindCombinedImage2DSampler(
                        Node,
                        Material.Normal,
                        Sampler,
                        MaterialSetIndex,
                        2);
                    Rr_BindCombinedImage2DSampler(
                        Node,
                        Material.RoughnessMetallic,
                        Sampler,
                        MaterialSetIndex,
                        3);
                }

                Rr_DrawIndexed(
                    Node,
                    Primitive.IndexCount,
                    1,
                    Primitive.FirstIndex,
                    Primitive.VertexOffset,
                    FirstInstance);
            }

            FirstInstance++;
        }
    }

    CGLTFScene()
    {
        Rr_Asset LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_SPONZA_GLB);

        cgltf_options Options = {};
        cgltf_data *Data{};
        cgltf_result Result =
            cgltf_parse(&Options, LoadedAsset.Data, LoadedAsset.Size, &Data);
        if (Result != cgltf_result_success)
        {
            fprintf(stderr, "Failed to load GLTF data!");

            exit(1);
        }
        cgltf_load_buffers(&Options, Data, NULL);

        auto Graph = Rr_GetGraph();

        /* Materials */

        Materials.resize(Data->materials_count);
        auto ColorThread = std::thread(
            LoadTextures<ETextureType::COLOR>,
            Data,
            std::ref(Materials));
        auto NormalThread = std::thread(
            LoadTextures<ETextureType::NORMAL>,
            Data,
            std::ref(Materials));
        auto RoughnessMetallicThread = std::thread(
            LoadTextures<ETextureType::ROUGHNESS_METALLIC>,
            Data,
            std::ref(Materials));

        MaterialBuffer = Rr_CreateBuffer(
            sizeof(SGPUMaterial) * Data->materials_count,
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT |
                RR_BUFFER_FLAGS_MAPPED_BIT);
        auto *GPUMaterials =
            (SGPUMaterial *)Rr_GetMappedBufferData(MaterialBuffer);

        for (auto Index = 0; Index < Data->materials_count; ++Index)
        {
            auto &Material = Data->materials[Index];
            GPUMaterials[Index].AlphaMode = Material.alpha_mode;
            GPUMaterials[Index].AlphaCutoff = Material.alpha_cutoff;
        }

        /* Meshes */

        std::vector<SVertex> Vertices;
        std::vector<uint16_t> Indices;
        Meshes.reserve(Data->meshes_count);
        for (auto &Mesh : std::span{ Data->meshes, Data->meshes_count })
        {
            std::vector<SPrimitive> Primitives;
            Primitives.reserve(Mesh.primitives_count);
            for (auto &Primitive :
                 std::span{ Mesh.primitives, Mesh.primitives_count })
            {
                cgltf_accessor const *PositionAccessor = cgltf_find_accessor(
                    &Primitive,
                    cgltf_attribute_type_position,
                    0);
                cgltf_accessor const *UVAccessor = cgltf_find_accessor(
                    &Primitive,
                    cgltf_attribute_type_texcoord,
                    0);
                cgltf_accessor const *NormalAccessor = cgltf_find_accessor(
                    &Primitive,
                    cgltf_attribute_type_normal,
                    0);
                cgltf_accessor const *TangentAccessor = cgltf_find_accessor(
                    &Primitive,
                    cgltf_attribute_type_tangent,
                    0);
                auto VertexOffset = Vertices.size();
                auto VertexCount = PositionAccessor->count;
                Vertices.resize(Vertices.size() + VertexCount);
                for (size_t Index = 0; Index < VertexCount; ++Index)
                {
                    auto &Vertex = Vertices.data()[VertexOffset + Index];
                    cgltf_accessor_read_float(
                        PositionAccessor,
                        Index,
                        Vertex.Position.Elements,
                        3);
                    cgltf_accessor_read_float(
                        UVAccessor,
                        Index,
                        Vertex.UV.Elements,
                        2);
                    cgltf_accessor_read_float(
                        NormalAccessor,
                        Index,
                        Vertex.Normal.Elements,
                        3);
                    cgltf_accessor_read_float(
                        TangentAccessor,
                        Index,
                        Vertex.Tangent.Elements,
                        4);
                }

                cgltf_accessor const *IndexAccessor = Primitive.indices;
                size_t FirstIndex = Indices.size();
                Indices.resize(Indices.size() + IndexAccessor->count);
                cgltf_accessor_unpack_indices(
                    IndexAccessor,
                    &Indices[FirstIndex],
                    sizeof(uint16_t),
                    IndexAccessor->count);

                Primitives.emplace_back(
                    IndexAccessor->count,
                    FirstIndex,
                    VertexOffset,
                    cgltf_material_index(Data, Primitive.material));
            }

            Meshes.emplace_back(std::move(Primitives));
        }

        /* Nodes */

        std::vector<Rr_Mat4> Models;
        auto Scene = Data->scenes;
        auto TraverseScene = [&](cgltf_node *Node,
                                 Rr_Mat4 const &ParentTransform,
                                 auto &&TraverseScene) -> void {
            Rr_Mat4 Transform = ParentTransform;
            if (Node->has_matrix)
            {
                assert(false);
            }
            else
            {
                if (Node->has_translation)
                {
                    Rr_Vec3 Translation = {
                        Node->translation[0],
                        Node->translation[1],
                        Node->translation[2],
                    };
                    Transform = Transform * Rr_Translate(Translation);
                }
                if (Node->has_rotation)
                {
                    Rr_Quat Quat = {
                        Node->rotation[0],
                        Node->rotation[1],
                        Node->rotation[2],
                        Node->rotation[3],
                    };
                    Transform = Transform * Rr_QToM4(Quat);
                }
                if (Node->has_scale)
                {
                    Rr_Vec3 Scale = {
                        Node->scale[0],
                        Node->scale[1],
                        Node->scale[2],
                    };
                    Transform = Transform * Rr_Scale(Scale);
                }
            }
            Models.emplace_back(Transform);
            MeshesToDraw.emplace_back(cgltf_mesh_index(Data, Node->mesh));
            for (auto Index = 0; Index < Node->children_count; ++Index)
            {
                TraverseScene(Node->children[Index], Transform, TraverseScene);
            }
        };
        for (auto NodeIndex = 0; NodeIndex < Scene->nodes_count; ++NodeIndex)
        {
            auto Node = Scene->nodes[NodeIndex];
            TraverseScene(Node, Rr_M4D(1.0f), TraverseScene);
        }

        size_t VertexDataSize = Vertices.size() * sizeof(SVertex);
        size_t IndexDataSize = Indices.size() * sizeof(uint16_t);
        size_t ModelDataSize = Models.size() * sizeof(Rr_Mat4);
        size_t VertexIndexSize = VertexDataSize + IndexDataSize;
        size_t TotalSize = VertexIndexSize + ModelDataSize;
        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
            TotalSize,
            RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
        Rr_ReleaseBuffer(StagingBuffer);
        std::byte *StagingData =
            (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
        memcpy(StagingData, Vertices.data(), VertexDataSize);
        memcpy(StagingData + VertexDataSize, Indices.data(), IndexDataSize);
        memcpy(StagingData + VertexIndexSize, Models.data(), ModelDataSize);

        auto TransferNode = Rr_AddTransferNode(Graph);

        MeshBuffer = Rr_CreateBuffer(
            VertexIndexSize,
            RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_VERTEX_BIT);
        Rr_TransferBufferData(
            TransferNode,
            VertexIndexSize,
            StagingBuffer,
            0,
            MeshBuffer,
            0);

        ModelBuffer =
            Rr_CreateBuffer(ModelDataSize, RR_BUFFER_FLAGS_STORAGE_BIT);
        Rr_TransferBufferData(
            TransferNode,
            ModelDataSize,
            StagingBuffer,
            VertexIndexSize,
            ModelBuffer,
            0);

        IndexOffset = VertexDataSize;

        ColorThread.join();
        NormalThread.join();
        RoughnessMetallicThread.join();

        cgltf_free(Data);

        for (auto &Material : Materials)
        {
            if (Material.Color)
            {
                Rr_GenerateMipmaps(Graph, Material.Color);
            }
            if (Material.Normal)
            {
                Rr_GenerateMipmaps(Graph, Material.Normal);
            }
            if (Material.RoughnessMetallic)
            {
                Rr_GenerateMipmaps(Graph, Material.RoughnessMetallic);
            }
        }

        Rr_SamplerInfo SamplerInfo = {
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
            .MipmapMode = RR_SAMPLER_MIPMAP_MODE_LINEAR,
        };
        Sampler = Rr_CreateSampler(&SamplerInfo);
    }

    ~CGLTFScene()
    {
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseBuffer(MeshBuffer);
        Rr_ReleaseBuffer(ModelBuffer);
        Rr_ReleaseBuffer(MaterialBuffer);
        for (auto const &Material : Materials)
        {
            Rr_ReleaseImage(Material.Color);
            Rr_ReleaseImage(Material.Normal);
            Rr_ReleaseImage(Material.RoughnessMetallic);
        }
    }
};

class CFullscreenBlit
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Sampler *Sampler{};

public:
    void Blit(Rr_Graph *Graph, Rr_Image2D *SrcImage, Rr_Image2D *DstImage)
    {
        Rr_ColorTarget ColorTarget = {
            .Image = DstImage,
            .LoadOp = RR_LOAD_OP_DONT_CARE,
            .StoreOp = RR_STORE_OP_STORE,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindCombinedImage2DSampler(GraphicsNode, SrcImage, Sampler, 0, 0);
        Rr_Draw(GraphicsNode, 3, 1, 0, 0);
    }

    CFullscreenBlit(Rr_AssetRef FragSPV)
    {
        Rr_SamplerInfo SamplerInfo = {};
        Sampler = Rr_CreateSampler(&SamplerInfo);

        Rr_ColorTargetInfo ColorTarget = {
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        Rr_Asset VertexShader =
            Rr_LoadAsset(EXAMPLE_ASSET_FULLSCREENTRIANGLE_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(FragSPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
            .Rasterizer = Rr_Rasterizer{ .CullMode = RR_CULL_MODE_NONE },
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    ~CFullscreenBlit()
    {
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    }
};

class CSkybox
{
    struct SGPUUniform
    {
        Rr_Mat4 View;
        Rr_Mat4 Projection;
        float Time;
    };

    static float constexpr CUBE_POSITIONS[] = {
        1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00,
        1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00,
        1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,  1.00,
        1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,
        -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00,
        -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00, -1.00,
        -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,  -1.00, 1.00,  1.00,
        -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,  -1.00, -1.00, 1.00,
    };
    static uint16_t constexpr CUBE_INDICES[] = {
        1,  13, 19, 1,  19, 7,  9, 6, 18, 9, 18, 21, 23, 20, 14, 23, 14, 17,
        16, 4,  10, 16, 10, 22, 5, 2, 8,  5, 8,  11, 15, 12, 0,  15, 0,  3,
    };

    static std::array constexpr VERTEX_ATTRIBUTES = {
        Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_VEC3 },
    };

    static std::array constexpr VERTEX_INPUT_BINDINGS = {
        Rr_VertexInputBinding{
            .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
            .AttributeCount = VERTEX_ATTRIBUTES.size(),
            .Attributes = VERTEX_ATTRIBUTES.data(),
        },
    };

    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Sampler *Sampler{};

    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *StagingBuffer{};
    Rr_Buffer *MeshBuffer{};
    size_t IndexOffset{};

    void InitUniformBuffer()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    void InitSampler()
    {
        Rr_SamplerInfo SamplerInfo = {
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
        };
        Sampler = Rr_CreateSampler(&SamplerInfo);
    }

    void InitSkyboxMesh()
    {
        size_t TotalSize = sizeof(CUBE_POSITIONS) + sizeof(CUBE_INDICES);
        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
            TotalSize,
            RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
        Rr_ReleaseBuffer(StagingBuffer);
        char *BufferData = (char *)Rr_GetMappedBufferData(StagingBuffer);
        std::memcpy(BufferData, CUBE_POSITIONS, sizeof(CUBE_POSITIONS));
        BufferData += sizeof(CUBE_POSITIONS);
        std::memcpy(BufferData, CUBE_INDICES, sizeof(CUBE_INDICES));

        MeshBuffer = Rr_CreateBuffer(
            TotalSize,
            RR_BUFFER_FLAGS_VERTEX_BIT | RR_BUFFER_FLAGS_INDEX_BIT);
        auto TransferNode = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(
            TransferNode,
            TotalSize,
            StagingBuffer,
            0,
            MeshBuffer,
            0);
        IndexOffset = sizeof(CUBE_POSITIONS);
    }

public:
    void RecreatePipeline(uint32_t MSAASampleCount)
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);

        Rr_ColorTargetInfo ColorTarget = {
            .Blend = Rr_AlphaBlend(),
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_SKYBOX_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_SKYBOX_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .VertexInputBindingCount = VERTEX_INPUT_BINDINGS.size(),
            .VertexInputBindings = VERTEX_INPUT_BINDINGS.data(),
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
            .Multisampling = Rr_Multisampling{ .SampleCount = MSAASampleCount },
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void Draw(
        Rr_Graph *Graph,
        Rr_Image2D *ColorImage,
        const SCamera &Camera,
        Rr_ImageCube *ImageCube)
    {
        SGPUUniform Uniform = {
            .View = Camera.GetViewMatrix(),
            .Projection = Camera.ProjMatrix,
        };
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &Uniform,
            sizeof(SGPUUniform));

        Rr_ColorTarget ColorTarget = {
            .Image = ColorImage,
            .LoadOp = RR_LOAD_OP_DONT_CARE,
            .StoreOp = RR_STORE_OP_STORE,
        };
        Rr_GraphNode *GraphicsNode =
            Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);

        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, MeshBuffer, 0, 0);
        Rr_BindIndexBuffer(
            GraphicsNode,
            MeshBuffer,
            0,
            IndexOffset,
            RR_INDEX_TYPE_UINT16);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SGPUUniform));
        Rr_BindCombinedImageCubeSampler(GraphicsNode, ImageCube, Sampler, 0, 1);
        Rr_DrawIndexed(GraphicsNode, std::size(CUBE_INDICES), 1, 0, 0, 0);
    }

    CSkybox(uint32_t MSAASampleCount)
    {
        RecreatePipeline(MSAASampleCount);
        InitUniformBuffer();
        InitSampler();
        InitSkyboxMesh();
    }

    ~CSkybox()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseBuffer(MeshBuffer);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(StagingBuffer);
        Rr_ReleaseSampler(Sampler);
    }
};

class CGrid
{
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

    Rr_GraphicsPipeline *GraphicsPipeline{};

    Rr_Buffer *UniformBuffer{};

public:
    void RecreatePipeline(uint32_t SampleCount = 1)
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);

        Rr_ColorTargetInfo ColorTarget = {
            .Blend = Rr_AlphaBlend(),
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        };

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_GRID_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_GRID_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
            .Multisampling = Rr_Multisampling{ .SampleCount = SampleCount },
            .DepthStencil =
                Rr_DepthStencil{
                    .Format = DEPTH_FORMAT,
                    .CompareOp = RR_COMPARE_OP_LESS_OR_EQUAL,
                    .EnableDepthTest = true,
                    .EnableDepthWrite = true,
                },
        };

        GraphicsPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void Draw(
        Rr_GraphNode *GraphicsNode,
        const SCamera &Camera,
        Rr_Image2D *ColorImage,
        Rr_Image2D *DepthImage)
    {
        Rr_BeginNodeLabel(GraphicsNode, "Grid");

        SGPUUniform Uniform = {
            .View = Camera.GetViewMatrix(),
            .InvView = Camera.Transform,
            .Projection = Camera.ProjMatrix,
            .InvProjection = Rr_InvPerspective_RH(Camera.ProjMatrix),
            .Near = NEAR_PLANE,
            .Far = FAR_PLANE,
            .GridSmall = 1.0f,
            .GridBig = 10.0f,
        };
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &Uniform,
            sizeof(SGPUUniform));

        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            0,
            0,
            0,
            sizeof(SGPUUniform));
        Rr_Draw(GraphicsNode, 6, 1, 0, 0);

        Rr_EndNodeLabel(GraphicsNode);
    }

    CGrid(uint32_t MSAASampleCount)
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(SGPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);

        RecreatePipeline(MSAASampleCount);
    }

    ~CGrid()
    {
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseBuffer(UniformBuffer);
    }
};

class CLighting
{
    static constexpr Rr_ImageFormat SHADOW_MAP_DEPTH_FORMAT =
        RR_IMAGE_FORMAT_D32_SFLOAT;
    static constexpr std::int32_t POINT_SHADOW_MAP_SIZE = 1024;
    static constexpr std::int32_t SPOT_SHADOW_MAP_SIZE = 1024;

    struct SGPUPointLight
    {
        Rr_Vec3 Position;
        float Energy;
        Rr_Vec3 Color;
        float Specular;
        float Radius;
        float Intensity;
        float Falloff;
        float ConstantBias;
        float SlopeBias;
        float NormalBias;
        float LightSize;
        float TexelSize;
        float NearPlane;
        float FarPlane;
        Rr_Vec2 Padding;
    };

    struct SGPUSpotLight
    {
        Rr_Mat4 Transform;
        Rr_Mat4 ViewProjection;
        Rr_Vec3 Color;
        float Energy;
        Rr_Vec3 Padding;
        float Specular;
        float Intensity;
        float InnerCone;
        float OuterCone;
        float ConstantBias;
        float SlopeBias;
        float NormalBias;
        float LightSize;
        float TexelSize;
    };

    struct SGPULights
    {
        uint32_t PointLightCount;
        uint32_t SpotLightCount;
        uint32_t Padding0;
        uint32_t Padding1;
        SGPUPointLight PointLights[MAX_POINT_LIGHTS];
        SGPUSpotLight SpotLights[MAX_SPOT_LIGHTS];
    };

    struct SGPUUniform
    {
        Rr_Mat4 ViewProjection;
        Rr_Vec3 LightPosition;
        float FarPlane;
    };

    Rr_GraphicsPipeline *ShadowPipeline{};
    Rr_Sampler *ShadowSampler{};
    Rr_Sampler *RegularSampler{};
    Rr_Buffer *UniformBuffer{};
    size_t LightsBufferOffset{};
    std::vector<Rr_ImageCube *> PointShadowMaps{};
    std::vector<Rr_Image2D *> SpotShadowMaps{};
    Rr_ImageCube *DummyPointShadowMap{};
    Rr_Image2D *DummySpotShadowMap{};

    Rr_Mat4 GetCubeView(Rr_ImageCubeFace Face, Rr_Vec3 Position)
    {
        switch (Face)
        {
            case RR_IMAGE_CUBE_FACE_FRONT:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(1.0f, 0.0f, 0.0f),
                    Rr_V3(0.0f, 1.0f, 0.0f));
            case RR_IMAGE_CUBE_FACE_BACK:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(-1.0f, 0.0f, 0.0f),
                    Rr_V3(0.0f, 1.0f, 0.0f));
            case RR_IMAGE_CUBE_FACE_UP:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(0.0f, 1.0f, 0.0f),
                    Rr_V3(0.0f, 0.0f, -1.0f));
            case RR_IMAGE_CUBE_FACE_DOWN:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(0.0f, -1.0f, 0.0f),
                    Rr_V3(0.0f, 0.0f, 1.0f));
            case RR_IMAGE_CUBE_FACE_RIGHT:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(0.0f, 0.0f, 1.0f),
                    Rr_V3(0.0f, 1.0f, 0.0f));
            case RR_IMAGE_CUBE_FACE_LEFT:
                return Rr_LookAt_RH(
                    Position,
                    Position + Rr_V3(0.0f, 0.0f, -1.0f),
                    Rr_V3(0.0f, 1.0f, 0.0f));
            default:
                std::abort();
        }
    }

public:
    std::vector<SGPUPointLight> PointLights{};
    std::vector<SGPUSpotLight> SpotLights{};
    Rr_ImageCube *VisualizePointShadowMap{};

    auto &AddPointLight()
    {
        SGPUPointLight PointLight = {
            .Energy = 1.0f,
            .Color = Rr_V3(1.0f, 1.0f, 1.0f),
            .Specular = 0.5f,
            .Radius = 2.5f,
            .Intensity = 3.8f,
            .Falloff = 1.4f,
            .ConstantBias = 0.000f,
            .SlopeBias = 0.000f,
            .NormalBias = 0.000f,
            .LightSize = 0.0037f,
            .TexelSize = 1.0f / (float)POINT_SHADOW_MAP_SIZE,
            .NearPlane = NEAR_PLANE,
            .FarPlane = FAR_PLANE,
        };

        Rr_SetNextObjectName("PointShadowMap");
        PointShadowMaps.emplace_back(Rr_CreateImageCube(
            { POINT_SHADOW_MAP_SIZE, POINT_SHADOW_MAP_SIZE },
            SHADOW_MAP_DEPTH_FORMAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT));

        return PointLights.emplace_back(PointLight);
    }

    auto &AddSpotLight()
    {
        SGPUSpotLight SpotLight = {
            .Color = Rr_V3F(1.0f),
            .Energy = 1.0f,
            .Specular = 0.5f,
            .Intensity = 1.0f,
            .InnerCone = 30.0f,
            .OuterCone = 75.0f,
            .ConstantBias = 0.455f,
            .SlopeBias = 5.0f,
            .NormalBias = 0.0091f,
            .LightSize = 0.0037f,
            .TexelSize = 1.0f / (float)SPOT_SHADOW_MAP_SIZE,
        };

        Rr_SetNextObjectName("SpotShadowMap");
        SpotShadowMaps.emplace_back(Rr_CreateImage2D(
            { SPOT_SHADOW_MAP_SIZE, SPOT_SHADOW_MAP_SIZE },
            SHADOW_MAP_DEPTH_FORMAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT));

        return SpotLights.emplace_back(SpotLight);
    }

    void Iterate(
        Rr_Graph *Graph,
        const SCamera &Camera,
        const std::function<void(Rr_GraphNode *Node)> &DrawSceneCallback)
    {
        Rr_BeginGraphLabel(Graph, "ShadowMaps");

        char *UniformData = (char *)Rr_GetMappedBufferData(UniformBuffer);
        std::size_t UniformOffset = 0;

        for (std::size_t Index = 0; Index < PointLights.size(); ++Index)
        {
            SGPUPointLight &Point = PointLights[Index];
            Rr_ImageCube *PointShadowMap = PointShadowMaps[Index];

            const Rr_Mat4 CubeFacePerspective = Rr_Perspective_RH(
                RR_ANGLE_DEG(90.0f),
                1.0f,
                Point.NearPlane,
                Point.FarPlane);

            for (std::uint32_t Face = 0; Face < RR_IMAGE_CUBE_FACE_COUNT;
                 ++Face)
            {
                constexpr Rr_Mat4 FLIP_XY_MATRIX = {
                    -1.0f, 0.0f,  0.0f, 0.0f, //
                    0.0f,  -1.0f, 0.0f, 0.0f, //
                    0.0f,  0.0f,  1.0f, 0.0f, //
                    0.0f,  0.0f,  0.0f, 1.0f, //
                };
                SGPUUniform Uniform = {
                    .ViewProjection =
                        CubeFacePerspective * FLIP_XY_MATRIX *
                        GetCubeView((Rr_ImageCubeFace)Face, Point.Position),
                    .LightPosition = Point.Position,
                    .FarPlane = FAR_PLANE,
                };
                std::memcpy(
                    UniformData + UniformOffset,
                    &Uniform,
                    sizeof(Uniform));

                Rr_DepthTarget DepthTarget = {
                    .Image = PointShadowMap,
                    .ImageLayerIndex = Face,
                    .LoadOp = RR_LOAD_OP_CLEAR,
                    .StoreOp = RR_STORE_OP_STORE,
                    .Clear = Rr_DepthClear(1.0f, 0),
                };
                Rr_SetNextNodeName(
                    Graph,
                    std::format("PointShadowMap#{}", Index).c_str());
                Rr_GraphNode *GraphicsNode =
                    Rr_AddGraphicsNode(Graph, 0, nullptr, &DepthTarget);
                Rr_BindGraphicsPipeline(GraphicsNode, ShadowPipeline);
                Rr_BindUniformBuffer(
                    GraphicsNode,
                    UniformBuffer,
                    0,
                    0,
                    UniformOffset,
                    sizeof(Uniform));
                DrawSceneCallback(GraphicsNode);

                UniformOffset +=
                    RR_ALIGN_POW2(sizeof(Uniform), Rr_GetUniformAlignment());
            }
        }

        for (std::size_t Index = 0; Index < SpotLights.size(); ++Index)
        {
            SGPUSpotLight &Spot = SpotLights[Index];
            Rr_Image2D *SpotShadowMap = SpotShadowMaps[Index];

            Spot.ViewProjection = Rr_Perspective_RH(
                                      RR_ANGLE_DEG(Spot.OuterCone),
                                      1.0f,
                                      0.5f,
                                      FAR_PLANE) *
                                  FLIP_Y_MATRIX * Rr_InvGeneral(Spot.Transform);
            SGPUUniform Uniform = {
                .ViewProjection = Spot.ViewProjection,
                .LightPosition = Spot.Transform.Columns[3].XYZ,
                .FarPlane = FAR_PLANE,
            };
            std::memcpy(UniformData + UniformOffset, &Uniform, sizeof(Uniform));

            Rr_DepthTarget DepthTarget = {
                .Image = SpotShadowMap,
                .LoadOp = RR_LOAD_OP_CLEAR,
                .StoreOp = RR_STORE_OP_STORE,
                .Clear = Rr_DepthClear(1.0f, 0),
            };
            Rr_SetNextNodeName(
                Graph,
                std::format("SpotShadowMap#{}", Index).c_str());
            Rr_GraphNode *GraphicsNode =
                Rr_AddGraphicsNode(Graph, 0, nullptr, &DepthTarget);
            Rr_BindGraphicsPipeline(GraphicsNode, ShadowPipeline);
            Rr_BindUniformBuffer(
                GraphicsNode,
                UniformBuffer,
                0,
                0,
                UniformOffset,
                sizeof(Uniform));
            DrawSceneCallback(GraphicsNode);

            UniformOffset +=
                RR_ALIGN_POW2(sizeof(Uniform), Rr_GetUniformAlignment());
        }

        Rr_EndGraphLabel(Graph, "ShadowMaps");

        LightsBufferOffset = UniformOffset;
        SGPULights *Lights = (SGPULights *)(UniformData + LightsBufferOffset);
        Lights->PointLightCount = (uint32_t)PointLights.size();
        Lights->SpotLightCount = (uint32_t)SpotLights.size();
        std::memcpy(
            Lights->PointLights,
            PointLights.data(),
            sizeof(SGPUPointLight) * PointLights.size());
        std::memcpy(
            Lights->SpotLights,
            SpotLights.data(),
            sizeof(SGPUSpotLight) * SpotLights.size());
    }

    void BindLights(Rr_GraphNode *GraphicsNode, std::uint32_t Set)
    {
        Rr_BindUniformBuffer(
            GraphicsNode,
            UniformBuffer,
            Set,
            0,
            LightsBufferOffset,
            sizeof(SGPULights));
        for (std::size_t Index = 0; Index < MAX_POINT_LIGHTS; ++Index)
        {
            Rr_BindSampledImageCubeAt(
                GraphicsNode,
                Index >= PointLights.size() ? DummyPointShadowMap
                                            : PointShadowMaps[Index],
                Set,
                1,
                Index);
        }
        for (std::uint32_t Index = 0; Index < MAX_SPOT_LIGHTS; ++Index)
        {
            Rr_BindSampledImage2DAt(
                GraphicsNode,
                Index >= SpotShadowMaps.size() ? DummySpotShadowMap
                                               : SpotShadowMaps[Index],
                Set,
                2,
                Index);
        }
        Rr_BindSampler(GraphicsNode, RegularSampler, Set, 3);
        Rr_BindSampler(GraphicsNode, ShadowSampler, Set, 4);
    }

    void UI()
    {
        if (Rr_UIBeginTree("Lights"))
        {
            for (std::uint32_t Index = 0; Index < PointLights.size(); ++Index)
            {
                if (Rr_UIBeginTree(
                        std::format("Point Light #{}", Index).c_str()))
                {
                    auto &PointLight = PointLights[Index];
                    bool Visualize =
                        VisualizePointShadowMap == PointShadowMaps[Index];
                    bool OldVisualize = Visualize;
                    if (Rr_UICheckbox("Visualize Shadow Map", &Visualize))
                    {
                        VisualizePointShadowMap =
                            OldVisualize ? nullptr : PointShadowMaps[Index];
                    }
                    Rr_UIInputFloat3("Position", PointLight.Position.Elements);
                    Rr_UIInputColor3("Color", PointLight.Color.Elements);
                    Rr_UISliderFloat("Radius", &PointLight.Radius, 0.0f, 8.0f);
                    Rr_UISliderFloat(
                        "Intensity",
                        &PointLight.Intensity,
                        0.0f,
                        8.0f);
                    Rr_UISliderFloat(
                        "Falloff",
                        &PointLight.Falloff,
                        0.0f,
                        8.0f);
                    Rr_UISliderFloat(
                        "LightSize",
                        &PointLight.LightSize,
                        0.0001f,
                        0.5f);
                    Rr_UISliderFloat(
                        "ConstantBias",
                        &PointLight.ConstantBias,
                        0.0f,
                        1.0f);
                    Rr_UISliderFloat(
                        "SlopeBias",
                        &PointLight.SlopeBias,
                        0.0f,
                        15.0f);
                    Rr_UISliderFloat(
                        "NormalBias",
                        &PointLight.NormalBias,
                        0.0f,
                        1.0f);

                    Rr_UIEndTree();
                }
            }

            for (std::uint32_t Index = 0; Index < SpotLights.size(); ++Index)
            {
                if (Rr_UIBeginTree(
                        std::format("Spot Light #{}", Index).c_str()))
                {
                    auto &SpotLight = SpotLights[Index];
                    Rr_UIInputColor3("Color", SpotLight.Color.Elements);
                    Rr_UIInputFloat3(
                        "Position",
                        SpotLight.Transform.Columns[3].Elements);
                    Rr_UISliderFloat(
                        "Inner Cone",
                        &SpotLight.InnerCone,
                        0.0f,
                        90.0f);
                    Rr_UISliderFloat(
                        "Outer Cone",
                        &SpotLight.OuterCone,
                        0.0f,
                        90.0f);
                    Rr_UISliderFloat(
                        "Intensity",
                        &SpotLight.Intensity,
                        0.0f,
                        8.0f);
                    Rr_UISliderFloat(
                        "LightSize",
                        &SpotLight.LightSize,
                        0.0001f,
                        0.5f);
                    Rr_UISliderFloat(
                        "ConstantBias",
                        &SpotLight.ConstantBias,
                        0.0f,
                        1.0f);
                    Rr_UISliderFloat(
                        "SlopeBias",
                        &SpotLight.SlopeBias,
                        0.0f,
                        15.0f);
                    Rr_UISliderFloat(
                        "NormalBias",
                        &SpotLight.NormalBias,
                        0.0f,
                        1.0f);

                    Rr_UIEndTree();
                }
            }

            Rr_UIEndTree();
        }
    }

    CLighting()
    {
        Rr_SamplerInfo SamplerInfo = {
            .MagFilter = RR_FILTER_NEAREST,
            .MinFilter = RR_FILTER_NEAREST,
        };
        RegularSampler = Rr_CreateSampler(&SamplerInfo);

        SamplerInfo.CompareEnable = true;
        SamplerInfo.CompareOp = RR_COMPARE_OP_LESS_OR_EQUAL;
        SamplerInfo.MinFilter = RR_FILTER_LINEAR;
        SamplerInfo.MagFilter = RR_FILTER_LINEAR;
        ShadowSampler = Rr_CreateSampler(&SamplerInfo);

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_SHADOWMAP_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader =
            Rr_LoadAsset(EXAMPLE_ASSET_SHADOWMAP_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .VertexInputBindingCount = GENERIC_VERTEX_INPUT_BINDINGS.size(),
            .VertexInputBindings = GENERIC_VERTEX_INPUT_BINDINGS.data(),
            .Rasterizer =
                Rr_Rasterizer{
                    .CullMode = RR_CULL_MODE_BACK,
                    .FrontFace = RR_FRONT_FACE_COUNTER_CLOCKWISE,
                },
            .DepthStencil =
                Rr_DepthStencil{
                    .Format = SHADOW_MAP_DEPTH_FORMAT,
                    .CompareOp = RR_COMPARE_OP_LESS,
                    .EnableDepthTest = true,
                    .EnableDepthWrite = true,
                },
        };

        ShadowPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        UniformBuffer = Rr_CreateBuffer(
            RR_MEGABYTES(1),
            RR_BUFFER_FLAGS_MAPPED_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_UNIFORM_BIT);

        DummyPointShadowMap = Rr_CreateImageCube(
            { 2, 2 },
            SHADOW_MAP_DEPTH_FORMAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);
        DummySpotShadowMap = Rr_CreateImage2D(
            { 2, 2 },
            SHADOW_MAP_DEPTH_FORMAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    ~CLighting()
    {
        Rr_ReleaseSampler(RegularSampler);
        Rr_ReleaseSampler(ShadowSampler);
        Rr_ReleaseGraphicsPipeline(ShadowPipeline);
        Rr_ReleaseBuffer(UniformBuffer);
        for (auto &ShadowMap : PointShadowMaps)
        {
            Rr_ReleaseImage(ShadowMap);
        }
        for (auto &ShadowMap : SpotShadowMaps)
        {
            Rr_ReleaseImage(ShadowMap);
        }
        Rr_ReleaseImage(DummyPointShadowMap);
        Rr_ReleaseImage(DummySpotShadowMap);
    }
};

class CSSAO
{
    Rr_Sampler *Sampler{};
    Rr_GraphicsPipeline *SSAOPipeline{};
    Rr_GraphicsPipeline *BlurPipeline{};
    Rr_Buffer *Buffer{};

    struct
    {
        Rr_Mat4 Projection;
        Rr_Mat4 InvProjection;
        float Bias = 0.5f;
        float Intensity = 0.02f;
        float Scale = 1.0f;
        float KernelRadius = 18.0f;
        float MinRes = 0.0f;
        float CameraNear;
        float CameraFar;
        float DepthRange;
        Rr_Vec2 DepthParams;
        Rr_Vec2 ScreenRes;
    } GPUUniform;

    struct
    {
        Rr_Vec2 InvResDir;
        float Sharpness = 40.0f;
    } GPUUniformBlur;

public:
    void UI()
    {
        if (Rr_UIBeginTree("Scalable Ambient Obscurance (SAO)"))
        {
            Rr_UISliderFloat("Bias", &GPUUniform.Bias, 0.0, 1.0);
            Rr_UISliderFloat("Intensity", &GPUUniform.Intensity, 0.0, 1.0);
            Rr_UISliderFloat("Scale", &GPUUniform.Scale, 0.0, 1.0);
            Rr_UISliderFloat(
                "KernelRadius",
                &GPUUniform.KernelRadius,
                10.0,
                200.0);
            Rr_UISliderFloat("Min Resolution", &GPUUniform.MinRes, 0.0, 1.0);
            Rr_UISliderFloat(
                "Blur Sharpness",
                &GPUUniformBlur.Sharpness,
                1.0f,
                100.0f);
            Rr_UIEndTree();
        }
    }

    void Apply(
        Rr_Graph *Graph,
        Rr_Image2D *TargetImage,
        Rr_Image2D *IntermediateImage,
        Rr_Image2D *NormalDepthImage,
        const SCamera &Camera)
    {
        Rr_BeginGraphLabel(Graph, "AmbientOcclusion");

        char *UniformData = (char *)Rr_GetMappedBufferData(Buffer);
        std::size_t UniformOffset = 0;

        GPUUniform.CameraNear = NEAR_PLANE;
        GPUUniform.CameraFar = FAR_PLANE;
        GPUUniform.DepthRange = FAR_PLANE - NEAR_PLANE;
        GPUUniform.DepthParams = Rr_V2(
            (NEAR_PLANE - FAR_PLANE) / (NEAR_PLANE * FAR_PLANE),
            1.0 / NEAR_PLANE);
        Rr_IntVec2 Extent = Rr_GetImage2DExtent(TargetImage);
        GPUUniform.ScreenRes = Rr_V2(Extent.Width, Extent.Height);
        GPUUniform.Projection = Camera.ProjMatrix;
        GPUUniform.InvProjection = Rr_InvGeneral(Camera.ProjMatrix);
        std::memcpy(
            UniformData + UniformOffset,
            &GPUUniform,
            sizeof(GPUUniform));

        /* Calculate AO and pack it 2x16 along with linear depth. */
        {
            Rr_ColorTarget ColorTarget = {
                .Image = TargetImage,
                .LoadOp = RR_LOAD_OP_DONT_CARE,
                .StoreOp = RR_STORE_OP_STORE,
            };
            Rr_GraphNode *GraphicsNode =
                Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
            Rr_BindGraphicsPipeline(GraphicsNode, SSAOPipeline);
            Rr_BindCombinedImage2DSampler(
                GraphicsNode,
                NormalDepthImage,
                Sampler,
                0,
                0);
            Rr_BindUniformBuffer(
                GraphicsNode,
                Buffer,
                0,
                1,
                UniformOffset,
                sizeof(GPUUniform));
            Rr_Draw(GraphicsNode, 6, 1, 0, 0);
        }

        UniformOffset +=
            RR_ALIGN_POW2(sizeof(GPUUniform), Rr_GetUniformAlignment());

        GPUUniformBlur.InvResDir =
            Rr_V2(1.0f / GPUUniform.ScreenRes.Width, 0.0f);
        std::memcpy(
            UniformData + UniformOffset,
            &GPUUniformBlur,
            sizeof(GPUUniformBlur));

        /* Blur X */
        {
            Rr_ColorTarget ColorTarget = {
                .Image = IntermediateImage,
                .LoadOp = RR_LOAD_OP_DONT_CARE,
                .StoreOp = RR_STORE_OP_STORE,
            };
            Rr_GraphNode *GraphicsNode =
                Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
            Rr_BindGraphicsPipeline(GraphicsNode, BlurPipeline);
            Rr_BindCombinedImage2DSampler(
                GraphicsNode,
                TargetImage,
                Sampler,
                0,
                0);
            Rr_BindUniformBuffer(
                GraphicsNode,
                Buffer,
                0,
                1,
                UniformOffset,
                sizeof(GPUUniformBlur));
            Rr_Draw(GraphicsNode, 6, 1, 0, 0);
        }

        UniformOffset +=
            RR_ALIGN_POW2(sizeof(GPUUniformBlur), Rr_GetUniformAlignment());

        GPUUniformBlur.InvResDir =
            Rr_V2(0.0f, 1.0f / GPUUniform.ScreenRes.Height);
        std::memcpy(
            UniformData + UniformOffset,
            &GPUUniformBlur,
            sizeof(GPUUniformBlur));

        /* Blur Y */
        {
            Rr_ColorTarget ColorTarget = {
                .Image = TargetImage,
                .LoadOp = RR_LOAD_OP_DONT_CARE,
                .StoreOp = RR_STORE_OP_STORE,
            };
            Rr_GraphNode *GraphicsNode =
                Rr_AddGraphicsNode(Graph, 1, &ColorTarget, nullptr);
            Rr_BindGraphicsPipeline(GraphicsNode, BlurPipeline);
            Rr_BindCombinedImage2DSampler(
                GraphicsNode,
                IntermediateImage,
                Sampler,
                0,
                0);
            Rr_BindUniformBuffer(
                GraphicsNode,
                Buffer,
                0,
                1,
                UniformOffset,
                sizeof(GPUUniformBlur));
            Rr_Draw(GraphicsNode, 6, 1, 0, 0);
        }

        Rr_EndGraphLabel(Graph, "AmbientOcclusion");
    }

    CSSAO()
    {
        Rr_SamplerInfo SamplerInfo = {
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
            .AddressModeU = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .AddressModeV = RR_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        };
        Sampler = Rr_CreateSampler(&SamplerInfo);

        Rr_ColorTargetInfo ColorTarget = {
            .Format = RR_IMAGE_FORMAT_R32_SFLOAT,
        };

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_QUAD_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_SSAO_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .ColorTargetCount = 1,
            .ColorTargets = &ColorTarget,
        };

        SSAOPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_SSAOBLUR_FRAG_SPV);
        FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        BlurPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        Buffer = Rr_CreateBuffer(
            RR_KILOBYTES(1),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    ~CSSAO()
    {
        Rr_ReleaseSampler(Sampler);
        Rr_ReleaseGraphicsPipeline(SSAOPipeline);
        Rr_ReleaseGraphicsPipeline(BlurPipeline);
        Rr_ReleaseBuffer(Buffer);
    }
};

#include <cstdio>

class CPBRRenderingApp
{
    struct SGPUUniform
    {
        Rr_Mat4 View;
        Rr_Mat4 Projection;
        Rr_Vec3 CameraPosition;
        float Time;
        Rr_Vec2 Resolution;
        Rr_Vec2 Padding0;
        Rr_Vec4 AmbientColor = Rr_V4(0.04f, 0.04f, 0.04f, 1.0f);
    } GPUUniform;

    Rr_GraphicsPipeline *ForwardPassPipeline{};
    Rr_GraphicsPipeline *NormalDepthPrepassPipeline{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *ModelBuffer{};
    std::uint32_t ModelCount{};
    Rr_Buffer *IndirectBuffer{};
    std::uint32_t DrawCount{};
    Rr_Image2D *ColorImage{};
    Rr_Image2D *ColorImageResolved{};
    Rr_Image2D *NormalDepthImage{};
    Rr_Image2D *NormalDepthImageResolved{};
    Rr_Image2D *DepthImage{};
    Rr_Image2D *AmbientOcclusionImage{};
    Rr_Image2D *AmbientOcclusionIntermediateImage{};
    Rr_Image2D *BRDFImage{};

    static constexpr std::array<const char *, 4> MSAA_OPTIONS = {
        "Disabled",
        "2 Samples",
        "4 Samples",
        "8 Samples",
    };
    std::uint32_t MSAAOptionIndex = 0;

    SCamera Camera;
    CGLTFScene GLTFScene;
    CFullscreenBlit FullscreenBlit;
    CLighting Lighting;
    CSkybox Skybox;
    CGrid Grid;
    CSSAO SSAO;
    bool DrawGrid = true;

    uint32_t GetMSAASampleCount() const
    {
        return 1 << MSAAOptionIndex;
    }

    void RecreatePipelines()
    {
        std::array ColorTargets = {
            Rr_ColorTargetInfo{
                .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
                .Resolve = MSAAOptionIndex > 0,
            },
        };

        Rr_Asset VertexShader =
            Rr_LoadAsset(EXAMPLE_ASSET_FORWARDPASS_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader =
            Rr_LoadAsset(EXAMPLE_ASSET_FORWARDPASS_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .VertexInputBindingCount = GENERIC_VERTEX_INPUT_BINDINGS.size(),
            .VertexInputBindings = GENERIC_VERTEX_INPUT_BINDINGS.data(),
            .ColorTargetCount = ColorTargets.size(),
            .ColorTargets = ColorTargets.data(),
            .Multisampling =
                Rr_Multisampling{ .SampleCount = GetMSAASampleCount() },
            .Rasterizer =
                Rr_Rasterizer{
                    .CullMode = RR_CULL_MODE_BACK,
                    .FrontFace = RR_FRONT_FACE_COUNTER_CLOCKWISE,
                },
            .DepthStencil =
                Rr_DepthStencil{
                    .Format = DEPTH_FORMAT,
                    .CompareOp = RR_COMPARE_OP_LESS_OR_EQUAL,
                    .EnableDepthTest = true,
                    .EnableDepthWrite = false,
                },
        };

        Rr_ReleaseGraphicsPipeline(ForwardPassPipeline);
        ForwardPassPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);

        VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_NORMALDEPTHPREPASS_VERT_SPV);
        VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        FragmentShader =
            Rr_LoadAsset(EXAMPLE_ASSET_NORMALDEPTHPREPASS_FRAG_SPV);
        FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        PipelineInfo.DepthStencil.EnableDepthWrite = true;
        ColorTargets[0] = Rr_ColorTargetInfo{
            .Format = RR_IMAGE_FORMAT_R32G32B32A32_SFLOAT,
        };
        Rr_ReleaseGraphicsPipeline(NormalDepthPrepassPipeline);
        NormalDepthPrepassPipeline = Rr_CreateGraphicsPipeline(&PipelineInfo);
    }

    void InitAttachments()
    {
        Rr_ImageFlags SampleCountFlag = RR_IMAGE_FLAGS_SAMPLE_COUNT_1
                                        << MSAAOptionIndex;
        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(SwapchainImage);
        Rr_ImageFormat SwapchainFormat = Rr_GetImageFormat(SwapchainImage);

        Rr_ReleaseImage(DepthImage);
        DepthImage = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            DEPTH_FORMAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT |
                RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT | SampleCountFlag);

        Rr_ReleaseImage(ColorImage);
        ColorImage = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            SwapchainFormat,
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT |
                RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT | SampleCountFlag);

        Rr_ReleaseImage(NormalDepthImage);
        NormalDepthImage = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            RR_IMAGE_FORMAT_R32G32B32A32_SFLOAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT |
                RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT | SampleCountFlag);

        Rr_ReleaseImage(AmbientOcclusionImage);
        AmbientOcclusionImage = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            RR_IMAGE_FORMAT_R32_SFLOAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT);

        Rr_ReleaseImage(AmbientOcclusionIntermediateImage);
        AmbientOcclusionIntermediateImage = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            RR_IMAGE_FORMAT_R32_SFLOAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT);

        Rr_ReleaseImage(ColorImageResolved);
        ColorImageResolved = nullptr;

        Rr_ReleaseImage(NormalDepthImageResolved);
        NormalDepthImageResolved = nullptr;

        if (GetMSAASampleCount() == 1)
        {
            return;
        }

        ColorImageResolved = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            SwapchainFormat,
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT |
                RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT);

        NormalDepthImageResolved = Rr_CreateImage2D(
            { SwapchainSize.X, SwapchainSize.Y },
            RR_IMAGE_FORMAT_R32G32B32A32_SFLOAT,
            RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT |
                RR_IMAGE_FLAGS_COLOR_ATTACHMENT_BIT);
    }

    void InitCamera()
    {
        Camera.UpdatePerspective(Rr_GetImage2DExtent(Rr_GetSwapchainImage()));
    }

    void InitUniform()
    {
        UniformBuffer = Rr_CreateBuffer(
            sizeof(GPUUniform),
            RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
                RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_PER_FRAME_BIT);
    }

    void UI()
    {
        Rr_UIDebugOverlay();
        Rr_UIBeginWindowEx("PBRRendering.cxx", nullptr, 0);
        {
            if (Rr_UIBeginTree("General"))
            {
                Rr_UIInputColor3(
                    "Ambient Color",
                    GPUUniform.AmbientColor.Elements);
                Rr_UIInputFloat3("Camera Position", Camera.Position.Elements);
                Rr_Vec3 CameraForward = Camera.GetForwardVector();
                Rr_UIInputFloat3("Camera Forward", CameraForward.Elements);
                Rr_UISeparator();
                Rr_UICheckbox("Draw Grid", &DrawGrid);
                if (Rr_UICombobox(
                        "MSAA",
                        MSAA_OPTIONS.size(),
                        MSAA_OPTIONS.data(),
                        &MSAAOptionIndex))
                {
                    Skybox.RecreatePipeline(GetMSAASampleCount());
                    Grid.RecreatePipeline(GetMSAASampleCount());
                    InitAttachments();
                    RecreatePipelines();
                }
                SSAO.UI();
                Rr_UIEndTree();
            }
            Lighting.UI();
        }
        Rr_UIEndWindow();
    }

public:
    void Event(Rr_Event const *Event)
    {
        switch (Event->Type)
        {
            case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
            {
                InitAttachments();
                InitCamera();
            }
            break;
            case RR_EVENT_TYPE_KEY_DOWN:
            {
                if (Event->Key.Scancode == RR_SCANCODE_F11)
                {
                    Rr_SetWindowFullscreen(!Rr_IsWindowFullscreen());
                }
            }
            break;
            default:
            {
            }
            break;
        }
    }

    void Iterate()
    {
        Rr_Graph *Graph = Rr_GetGraph();

        UI();

        Rr_BeginGraphLabel(Graph, "ModernRendering");

        Camera.Update();

        if (Lighting.PointLights.size() > 1)
        {
            Lighting.PointLights[1].Position =
                Rr_V3(std::cos(0.5f + Rr_GetTimeSeconds()) * 6.0f, 5.0f, 0.0f);
        }
        Lighting.Iterate(Graph, Camera, [&](Rr_GraphNode *Node) {
            GLTFScene.Draw<2>(Node, 1, 0);
        });

        Rr_Image2D *SwapchainImage = Rr_GetSwapchainImage();
        Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(SwapchainImage);

        Rr_ClearColorImage2D(
            Graph,
            { Rr_V4(0.005f, 0.007f, 0.015f, 1.0f) },
            ColorImage);

        if (Lighting.VisualizePointShadowMap)
        {
            Skybox.Draw(
                Graph,
                ColorImage,
                Camera,
                Lighting.VisualizePointShadowMap);
        }

        bool UseMSAA = GetMSAASampleCount() > 1;

        GPUUniform.View = Camera.GetViewMatrix();
        GPUUniform.Projection = Camera.ProjMatrix;
        GPUUniform.CameraPosition = Camera.Position;
        GPUUniform.Time = (float)Rr_GetTimeSeconds();
        GPUUniform.Resolution = { (float)SwapchainSize.Width,
                                  (float)SwapchainSize.Height };
        std::memcpy(
            Rr_GetMappedBufferData(UniformBuffer),
            &GPUUniform,
            sizeof(GPUUniform));

        /* Normal/Depth Prepass */
        {
            Rr_BeginGraphLabel(Graph, "NormalDepthPrepass");

            std::array ColorTargets = {
                Rr_ColorTarget{
                    .Image = NormalDepthImage,
                    .LoadOp = RR_LOAD_OP_DONT_CARE,
                    .StoreOp =
                        UseMSAA ? RR_STORE_OP_DONT_CARE : RR_STORE_OP_STORE,
                    .ResolveImage =
                        UseMSAA ? NormalDepthImageResolved : nullptr,
                    .ResolveLoadOp = RR_LOAD_OP_DONT_CARE,
                    .ResolveStoreOp = RR_STORE_OP_STORE,
                },
            };
            Rr_DepthTarget DepthTarget = {
                .Image = DepthImage,
                .LoadOp = RR_LOAD_OP_CLEAR,
                .StoreOp = RR_STORE_OP_STORE,
                .Clear = Rr_DepthClear(1.0f, 0),
            };
            Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
                Graph,
                ColorTargets.size(),
                ColorTargets.data(),
                &DepthTarget);

            Rr_BindGraphicsPipeline(GraphicsNode, NormalDepthPrepassPipeline);
            Rr_BindUniformBuffer(
                GraphicsNode,
                UniformBuffer,
                0,
                0,
                0,
                sizeof(GPUUniform));
            GLTFScene.Draw<3>(GraphicsNode, 2, 0);

            Rr_EndGraphLabel(Graph, "NormalDepthPrepass");
        }

        /* Ambient Occlusion */
        {
            Rr_Image2D *FinalNormalDepthImage =
                UseMSAA ? NormalDepthImageResolved : NormalDepthImage;

            SSAO.Apply(
                Graph,
                AmbientOcclusionImage,
                AmbientOcclusionIntermediateImage,
                FinalNormalDepthImage,
                Camera);
        }

        /* Forward Pass */
        {
            Rr_BeginGraphLabel(Graph, "ForwardPass");

            std::array ColorTargets = {
                Rr_ColorTarget{
                    .Image = ColorImage,
                    .LoadOp = RR_LOAD_OP_LOAD,
                    .StoreOp =
                        UseMSAA ? RR_STORE_OP_DONT_CARE : RR_STORE_OP_STORE,
                    .ResolveImage = UseMSAA ? ColorImageResolved : nullptr,
                    .ResolveLoadOp = RR_LOAD_OP_DONT_CARE,
                    .ResolveStoreOp = RR_STORE_OP_STORE,
                },
            };
            Rr_DepthTarget DepthTarget = {
                .Image = DepthImage,
                .LoadOp = RR_LOAD_OP_LOAD,
                .StoreOp = RR_STORE_OP_DONT_CARE,
            };
            Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(
                Graph,
                ColorTargets.size(),
                ColorTargets.data(),
                &DepthTarget);

            Rr_BindGraphicsPipeline(GraphicsNode, ForwardPassPipeline);
            Rr_BindUniformBuffer(
                GraphicsNode,
                UniformBuffer,
                0,
                0,
                0,
                sizeof(GPUUniform));
            Lighting.BindLights(GraphicsNode, 1);
            Rr_BindSampledImage2D(GraphicsNode, AmbientOcclusionImage, 1, 6);
            Rr_BindSampledImage2D(GraphicsNode, BRDFImage, 1, 7);
            GLTFScene.Draw<3>(GraphicsNode, 2, 0);

            if (DrawGrid)
            {
                Grid.Draw(GraphicsNode, Camera, ColorImage, DepthImage);
            }

            Rr_EndGraphLabel(Graph, "ForwardPass");
        }

        Rr_Image2D *FinalColorImage = UseMSAA ? ColorImageResolved : ColorImage;

        Rr_BeginGraphLabel(Graph, "Compose");

        FullscreenBlit.Blit(Graph, FinalColorImage, SwapchainImage);

        Rr_EndGraphLabel(Graph, "Compose");

        Rr_EndGraphLabel(Graph, "ModernRendering");
    }

    CPBRRenderingApp()
        : FullscreenBlit(EXAMPLE_ASSET_FULLSCREENTRIANGLE_FRAG_SPV)
        , Grid(GetMSAASampleCount())
        , Skybox(GetMSAASampleCount())
        , GLTFScene({})
    {
        {
            auto &PointLight = Lighting.AddPointLight();
            PointLight.Position = Rr_V3(0.0f, 2.0f, 0.0f);
            PointLight.Radius = 4.0f;
            PointLight.Intensity = 5.0f;
            PointLight.Falloff = 0.35f;
        }
        {
            auto &PointLight = Lighting.AddPointLight();
            PointLight.Position = Rr_V3(0.0f, 5.0f, 0.0f);
            PointLight.Radius = 4.0f;
            PointLight.Intensity = 5.0f;
            PointLight.Falloff = 0.35f;
        }
        InitAttachments();
        RecreatePipelines();
        InitUniform();
        InitCamera();
        Camera.Position = Rr_V3(0.0f, 1.0f, 0.0f);
        BRDFImage = LoadImage2D(
            Rr_LoadAsset(EXAMPLE_ASSET_BRDF_RAW).Data,
            Rr_LoadAsset(EXAMPLE_ASSET_BRDF_RAW).Size,
            Rr_IntV2(512, 512),
            RR_IMAGE_FORMAT_R16G16_SFLOAT,
            false,
            Rr_GetGraph());
    }

    ~CPBRRenderingApp()
    {
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(ModelBuffer);
        Rr_ReleaseBuffer(IndirectBuffer);
        Rr_ReleaseGraphicsPipeline(ForwardPassPipeline);
        Rr_ReleaseGraphicsPipeline(NormalDepthPrepassPipeline);
        Rr_ReleaseImage(ColorImage);
        Rr_ReleaseImage(ColorImageResolved);
        Rr_ReleaseImage(NormalDepthImage);
        Rr_ReleaseImage(NormalDepthImageResolved);
        Rr_ReleaseImage(DepthImage);
        Rr_ReleaseImage(AmbientOcclusionImage);
        Rr_ReleaseImage(AmbientOcclusionIntermediateImage);
        Rr_ReleaseImage(BRDFImage);
    }
};

int main()
{
    static CPBRRenderingApp *App{};

    Rr_AppConfig Config = {};
    Config.Title = "PBRRendering";
    Config.WindowFlags |= RR_WINDOW_FLAGS_RESIZE_BIT;
    Config.InitFunc = []() { App = new CPBRRenderingApp(); };
    Config.EventFunc = [](Rr_Event const *Event) { App->Event(Event); };
    Config.IterateFunc = []() { App->Iterate(); };
    Config.CleanupFunc = []() { delete App; };
    Rr_Run(&Config);
}
