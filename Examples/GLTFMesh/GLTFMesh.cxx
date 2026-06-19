#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define CGLTF_IMPLEMENTATION
#include "../../Vendor/cgltf/cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <array>
#include <cstring>
#include <vector>

class COrbitCamera
{
    float FieldOfView{ RR_ANGLE_DEG(65) };
    float Pitch{ -RR_ANGLE_DEG(30) };
    float Yaw{ RR_ANGLE_DEG(30) };
    float Distance{ 10.0f };
    Rr_Vec3 Center{};
    Rr_Mat4 Transform{ Rr_M4D(1.0f) };
    Rr_Mat4 ProjMatrix{ Rr_M4D(1.0f) };

public:
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

    void Update(float Aspect)
    {
        auto MouseDelta = Rr_GetMousePositionDelta();
        auto MouseState = Rr_GetMouseState();
        auto LeftButton = MouseState & RR_MOUSE_BUTTON_LEFT_BIT;
        auto RightButton = MouseState & RR_MOUSE_BUTTON_RIGHT_BIT;

        if (LeftButton || RightButton)
        {
            if (RightButton)
            {
                Rr_SetRelativeMouseMode(true);

                auto constexpr SENSITIVITY = 0.05f;
                Distance += MouseDelta.Y * SENSITIVITY;
            }
            if (LeftButton)
            {
                Rr_SetRelativeMouseMode(true);

                if (Rr_IsScancodePressed(RR_SCANCODE_LSHIFT))
                {
                    auto constexpr SENSITIVITY = 0.0125f;
                    auto ForwardVector = GetForwardVector();
                    auto RightVector = GetRightVector();
                    auto UpVector = Rr_Cross(ForwardVector, RightVector);
                    Center += UpVector * MouseDelta.Y * SENSITIVITY;
                    Center -= RightVector * MouseDelta.X * SENSITIVITY;
                }
                else
                {
                    auto constexpr SENSITIVITY = 0.005f;
                    Yaw -= MouseDelta.X * SENSITIVITY;
                    Pitch -= MouseDelta.Y * SENSITIVITY;
                }
            }
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, RR_PI32 * 2.0f);
        Pitch = RR_CLAMP(RR_PI32 * -0.5f, Pitch, RR_PI32 * 0.5f);

        Transform = Rr_TranslateV(Center) * Rr_Rotate_RH(Yaw, Rr_V3(0.0f, 1.0f, 0.0f)) *
                    Rr_Rotate_RH(Pitch, Rr_V3(1.0f, 0.0f, 0.0f)) * Rr_Translate(0.0f, 0.0f, Distance);
        ProjMatrix = Rr_Perspective_RH(FieldOfView, Aspect, 0.1f, 100.0f);
        ProjMatrix.Elements[1][1] *= -1.0f;
    }
};

struct SGPUUniform
{
    Rr_Mat4 View;
    Rr_Mat4 Projection;
    float Time;
};

struct SGPUModel
{
    Rr_Mat4 Model;
    Rr_Vec4 Color;
};

struct SVertex
{
    Rr_Vec3 Position;
    Rr_Vec2 UV;
    Rr_Vec3 Normal;
};

struct SPrimitive
{
    uint32_t IndexCount;
    uint32_t FirstIndex;
    int32_t VertexOffset;
    Rr_Vec4 Color;
};

struct SMesh
{
    std::vector<SPrimitive> Primitives;
};

struct SNode
{
    cgltf_node *GLTFNode;
    Rr_Mat4 Transform;
    Rr_Vec3 AnimatedTranslation;
    Rr_Quat AnimatedRotation;
    Rr_Vec3 AnimatedScale;
    bool Animated;
};

struct SMaterial
{
    Rr_Image2D *ColorTexture{};
};

class CGLTFMeshApp
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Sampler *Sampler{};
    Rr_Image2D *DepthImage{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *ModelBuffer{};
    Rr_Buffer *GeometryBuffer{};
    size_t GeometryBufferIndexOffset;
    cgltf_data *GLTFData{};
    cgltf_scene *GLTFScene{};
    std::vector<SMesh> Meshes;
    std::vector<SMaterial> Materials;
    SGPUUniform GPUUniform{};
    COrbitCamera Camera;
    std::vector<SGPUModel> Models;

    SMesh ParseGLTFMesh(cgltf_mesh *GLTFMesh, std::vector<SVertex> &OutVertices, std::vector<uint16_t> &OutIndices)
    {
        auto Mesh = SMesh{};
        Mesh.Primitives.resize(GLTFMesh->primitives_count);

        for (size_t PrimitiveIndex = 0; PrimitiveIndex < GLTFMesh->primitives_count; ++PrimitiveIndex)
        {
            auto GLTFPrimitive = &GLTFMesh->primitives[PrimitiveIndex];
            auto Primitive = &Mesh.Primitives[PrimitiveIndex];

            auto PositionAccessor = cgltf_find_accessor(GLTFPrimitive, cgltf_attribute_type_position, 0);
            assert(PositionAccessor);
            auto NormalAccessor = cgltf_find_accessor(GLTFPrimitive, cgltf_attribute_type_normal, 0);
            assert(NormalAccessor);
            auto TexCoordAccessor = cgltf_find_accessor(GLTFPrimitive, cgltf_attribute_type_texcoord, 0);
            auto JointsAccessor = cgltf_find_accessor(GLTFPrimitive, cgltf_attribute_type_joints, 0);
            if (JointsAccessor)
            {
                assert(JointsAccessor->type == cgltf_type_vec4);
            }
            auto WeightsAccessor = cgltf_find_accessor(GLTFPrimitive, cgltf_attribute_type_weights, 0);
            if (WeightsAccessor)
            {
                assert(WeightsAccessor->type == cgltf_type_vec4);
            }

            auto VertexCount = PositionAccessor->count;
            auto VertexOffset = OutVertices.size();
            OutVertices.resize(OutVertices.size() + VertexCount);
            for (auto Index = 0; Index < VertexCount; ++Index)
            {
                auto &Vertex = OutVertices.data()[VertexOffset + Index];
                cgltf_accessor_read_float(PositionAccessor, Index, Vertex.Position.Elements, 3);
                cgltf_accessor_read_float(NormalAccessor, Index, Vertex.Normal.Elements, 3);
                if (TexCoordAccessor)
                {
                    Rr_Vec2 TexCoord;
                    cgltf_accessor_read_float(TexCoordAccessor, Index, Vertex.UV.Elements, 2);
                }
            }

            auto IndexAccessor = GLTFPrimitive->indices;
            auto FirstIndex = OutIndices.size();
            OutIndices.resize(OutIndices.size() + IndexAccessor->count);
            cgltf_accessor_unpack_indices(
                IndexAccessor,
                &OutIndices[FirstIndex],
                sizeof(uint16_t),
                IndexAccessor->count);

            Primitive->IndexCount = IndexAccessor->count;
            Primitive->FirstIndex = FirstIndex;
            Primitive->VertexOffset = (int32_t)VertexOffset;
            if (GLTFPrimitive->material && GLTFPrimitive->material->has_pbr_metallic_roughness)
            {
                std::memcpy(
                    Primitive->Color.Elements,
                    GLTFPrimitive->material->pbr_metallic_roughness.base_color_factor,
                    sizeof(Rr_Vec4));
            }
        }

        return Mesh;
    }

    void InitGLTFScene(void)
    {
        auto LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_TOWER_GLB);

        auto Options = cgltf_options{};
        cgltf_data *Data = nullptr;
        auto Result = cgltf_parse(&Options, LoadedAsset.Data, LoadedAsset.Size, &Data);
        assert(Result == cgltf_result_success);
        cgltf_load_buffers(&Options, Data, nullptr);

        assert(Data->scene);
        assert(Data->meshes);

        Materials.resize(Data->materials_count);
        for (auto Index = 0; Index < Data->materials_count; ++Index)
        {
            auto &Material = Data->materials[Index];
            if (Material.has_pbr_metallic_roughness && Material.pbr_metallic_roughness.base_color_texture.texture)
            {
                auto Texture = Material.pbr_metallic_roughness.base_color_texture.texture;

                if (std::strcmp(Texture->image->mime_type, "image/png") != 0 &&
                    std::strcmp(Texture->image->mime_type, "image/jpeg") != 0)
                {
                    assert(false);
                }

                int ImageWidth, ImageHeight, ImageChannels;
                auto ImageData = (std::byte *)stbi_load_from_memory(
                    (stbi_uc const *)Texture->image->buffer_view->buffer->data + Texture->image->buffer_view->offset,
                    (int)Texture->image->buffer_view->size,
                    &ImageWidth,
                    &ImageHeight,
                    &ImageChannels,
                    4);
                assert(ImageData);
                assert(ImageChannels == 4);

                auto ImageDataSize = ImageWidth * ImageHeight * ImageChannels;
                auto StagingBuffer = Rr_CreateBuffer(ImageDataSize, RR_BUFFER_FLAGS_STAGING);
                Rr_ReleaseBuffer(StagingBuffer);
                std::memcpy(Rr_GetMappedBufferData(StagingBuffer), ImageData, ImageDataSize);
                stbi_image_free(ImageData);

                auto ColorTexture = Rr_CreateImage2D(
                    Rr_IntV2(ImageWidth, ImageHeight),
                    RR_IMAGE_FORMAT_R8G8B8A8_SRGB,
                    RR_IMAGE_FLAGS_SAMPLED_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
                Rr_CopyBufferToImage2D(
                    Rr_GetGraph(),
                    StagingBuffer,
                    0,
                    Rr_IntV2(ImageWidth, ImageHeight),
                    ColorTexture,
                    0);
                Materials[Index].ColorTexture = ColorTexture;
            }
        }

        auto Vertices = std::vector<SVertex>{};
        auto Indices = std::vector<uint16_t>{};

        Meshes.reserve(Data->meshes_count);

        for (size_t MeshIndex = 0; MeshIndex < Data->meshes_count; ++MeshIndex)
        {
            cgltf_mesh *Mesh = &Data->meshes[MeshIndex];

            Meshes.push_back(ParseGLTFMesh(Mesh, Vertices, Indices));
        }

        auto VertexDataSize = Vertices.size() * sizeof(SVertex);
        auto IndexDataSize = Indices.size() * sizeof(uint16_t);
        auto TotalSize = VertexDataSize + IndexDataSize;
        auto StagingBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_STAGING);
        Rr_ReleaseBuffer(StagingBuffer);
        auto StagingData = (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
        auto StagingVertices = (SVertex *)StagingData;
        auto StagingIndices = (uint16_t *)(StagingData + VertexDataSize);
        std::memcpy(StagingVertices, Vertices.data(), VertexDataSize);
        std::memcpy(StagingIndices, Indices.data(), IndexDataSize);

        GeometryBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_VERTEX_BIT);
        GeometryBufferIndexOffset = VertexDataSize;
        auto TransferNode = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(TransferNode, TotalSize, StagingBuffer, 0, GeometryBuffer, 0);

        GLTFData = Data;
        GLTFScene = Data->scene;
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

    void DrawNode(Rr_GraphNode *GraphicsNode, cgltf_node *GLTFNode, Rr_Mat4 const &ParentTransform = Rr_M4D(1.0f))
    {
        auto Transform = ParentTransform;
        if (GLTFNode->has_translation)
        {
            auto Translation = Rr_V3(GLTFNode->translation[0], GLTFNode->translation[1], GLTFNode->translation[2]);
            Transform = Transform * Rr_TranslateV(Translation);
        }
        if (GLTFNode->has_rotation)
        {
            auto Quat = Rr_Quat{
                GLTFNode->rotation[0],
                GLTFNode->rotation[1],
                GLTFNode->rotation[2],
                GLTFNode->rotation[3],
            };
            Transform = Transform * Rr_QToM4(Quat);
        }
        if (GLTFNode->has_scale)
        {
            auto Scale = Rr_V3(GLTFNode->scale[0], GLTFNode->scale[1], GLTFNode->scale[2]);
            Transform = Transform * Rr_ScaleV(Scale);
        }

        if (GLTFNode->mesh)
        {
            auto MeshIndex = cgltf_mesh_index(GLTFData, GLTFNode->mesh);
            auto &Mesh = Meshes[MeshIndex];
            for (auto Index = 0; Index < Mesh.Primitives.size(); ++Index)
            {
                auto &GLTFPrimitive = GLTFNode->mesh->primitives[Index];
                auto MaterialIndex = cgltf_material_index(GLTFData, GLTFPrimitive.material);
                Rr_BindCombinedImage2DSampler(GraphicsNode, Materials[MaterialIndex].ColorTexture, Sampler, 3, 0);
                auto &Primitive = Mesh.Primitives[Index];
                Rr_DrawIndexed(
                    GraphicsNode,
                    Primitive.IndexCount,
                    1,
                    Primitive.FirstIndex,
                    Primitive.VertexOffset,
                    Models.size());
                Models.emplace_back(Transform, Primitive.Color);
            }
        }

        for (auto Index = 0; Index < GLTFNode->children_count; ++Index)
        {
            DrawNode(GraphicsNode, GLTFNode->children[Index], Transform);
        }
    }

public:
    CGLTFMeshApp()
    {
        auto SamplerInfo = Rr_SamplerInfo{
            .MagFilter = RR_FILTER_LINEAR,
            .MinFilter = RR_FILTER_LINEAR,
        };
        Sampler = Rr_CreateSampler(&SamplerInfo);

        std::array VertexAttributes = {
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
            Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_FLOAT2 },
            Rr_VertexInputAttribute{ .Location = 2, .Format = RR_FORMAT_FLOAT3 },
        };

        std::array VertexInputBindings = {
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
            },
        };

        auto ColorTargets = std::array{
            Rr_ColorTargetInfo{
                .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
            },
        };

        auto VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFMESH_VERT_SPV);
        auto VertexShaderInfo = Rr_ShaderInfo{
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        auto FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFMESH_FRAG_SPV);
        auto FragmentShaderInfo = Rr_ShaderInfo{
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        auto PipelineInfo = Rr_GraphicsPipelineCreateInfo{
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .VertexInputBindingCount = VertexInputBindings.size(),
            .VertexInputBindings = VertexInputBindings.data(),
            .ColorTargetCount = ColorTargets.size(),
            .ColorTargets = ColorTargets.data(),
            .Rasterizer =
                Rr_Rasterizer{
                    .CullMode = RR_CULL_MODE_NONE,
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

        UniformBuffer = Rr_CreateBuffer(sizeof(GPUUniform), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);

        InitGLTFScene();

        InitDepthImage();

        ModelBuffer = Rr_CreateBuffer(RR_MEBIBYTES(4), RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_DYNAMIC);
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
        if (Rr_UIBeginWindow("GLTFMesh.cxx"))
        {
            Rr_UIText("This example shows using cGLTF to load and draw meshes.");
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto SwapchainImage = Rr_GetSwapchainImage();
        auto SwapchainAspect = Rr_GetImage2DAspect(SwapchainImage);

        Camera.Update(SwapchainAspect);

        GPUUniform.View = Camera.GetViewMatrix();
        GPUUniform.Projection = Camera.GetProjectionMatrix();
        GPUUniform.Time = (float)Rr_GetTimeSeconds();
        memcpy(Rr_GetMappedBufferData(UniformBuffer), &GPUUniform, sizeof(GPUUniform));

        auto ColorTarget = Rr_ColorTarget{
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = { Rr_V4(0.03f, 0.03f, 0.04f, 1.0f) },
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
        Rr_BindIndexBuffer(GraphicsNode, GeometryBuffer, 0, GeometryBufferIndexOffset, RR_INDEX_TYPE_UINT16);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(GPUUniform));
        Rr_BindStorageBuffer(GraphicsNode, ModelBuffer, 1, 0, 0, Rr_GetBufferSize(ModelBuffer));

        for (auto Index = 0; Index < GLTFScene->nodes_count; ++Index)
        {
            DrawNode(GraphicsNode, GLTFScene->nodes[Index]);
        }

        std::memcpy(Rr_GetMappedBufferData(ModelBuffer), Models.data(), sizeof(SGPUModel) * Models.size());
    }

    ~CGLTFMeshApp()
    {
        for (auto &Material : Materials)
        {
            Rr_ReleaseImage(Material.ColorTexture);
        }
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseBuffer(ModelBuffer);
        Rr_ReleaseBuffer(GeometryBuffer);
        Rr_ReleaseImage(DepthImage);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
        Rr_ReleaseSampler(Sampler);
        cgltf_free(GLTFData);
    }
};

int main()
{
    static CGLTFMeshApp *App{};

    auto Config = Rr_Config{
        .WindowTitle = "GLTFMesh",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CGLTFMeshApp(); },
        .EventFunc = [](Rr_Event const *Event) { App->Event(Event); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
