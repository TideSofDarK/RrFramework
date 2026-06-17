#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define CGLTF_IMPLEMENTATION
#include "../../Vendor/cgltf/cgltf.h"

#include <algorithm>
#include <array>
#include <vector>

struct SOrbitCamera
{
    float FieldOfView{ RR_ANGLE_DEG(70) };
    float Pitch{};
    float Yaw{};
    float Distance{ 10.0f };
    Rr_Vec3 Center{};
    Rr_Mat4 Transform{ Rr_M4D(1.0f) };
    Rr_Mat4 ProjMatrix{ Rr_M4D(1.0f) };

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

    void UpdatePerspective(float Aspect)
    {
        ProjMatrix = Rr_Perspective_RH(FieldOfView, Aspect, 0.1f, 100.0f);
        ProjMatrix.Elements[1][1] *= -1.0f;
    }

    void Update()
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
    Rr_IntVec4 Data;
};

#pragma pack(push, 1)
struct SVertex
{
    Rr_Vec3 Position;
    Rr_Vec3 Normal;
    uint32_t BoneIndices[4];
    Rr_Vec4 BoneWeights;
};
#pragma pack(pop)

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

class CGLTFAnimation
{
    Rr_GraphicsPipeline *GraphicsPipeline{};
    Rr_Image2D *DepthImage{};
    Rr_Buffer *UniformBuffer{};
    Rr_Buffer *ModelBuffer{};
    Rr_Buffer *SkinBuffer{};
    Rr_Buffer *GeometryBuffer{};
    size_t GeometryBufferIndexOffset;
    Rr_Buffer *AnimationBuffer{};
    cgltf_data *CGLTFData{};
    cgltf_scene *CGLTFScene{};
    std::vector<SMesh> Meshes;
    uint32_t AnimationIndex{};
    float AnimationTime{};
    float AnimationStart{};
    float AnimationEnd{};
    SGPUUniform GPUUniform;
    SOrbitCamera Camera;
    std::vector<SNode> Nodes;
    std::vector<Rr_Mat4> Bones;
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
                if (JointsAccessor)
                {
                    cgltf_accessor_read_uint(JointsAccessor, Index, Vertex.BoneIndices, 4);
                }
                if (WeightsAccessor)
                {
                    cgltf_accessor_read_float(WeightsAccessor, Index, Vertex.BoneWeights.Elements, 4);
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
        Rr_Asset LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_ROBOT_GLB);

        cgltf_options Options = {};
        cgltf_data *Data = nullptr;
        cgltf_result Result = cgltf_parse(&Options, LoadedAsset.Data, LoadedAsset.Size, &Data);
        assert(Result == cgltf_result_success);
        cgltf_load_buffers(&Options, Data, nullptr);

        assert(Data->scene);
        assert(Data->meshes);
        assert(Data->skins_count == 1);

        std::vector<SVertex> Vertices;
        std::vector<uint16_t> Indices;

        Meshes.reserve(Data->meshes_count);

        for (size_t MeshIndex = 0; MeshIndex < Data->meshes_count; ++MeshIndex)
        {
            cgltf_mesh *Mesh = &Data->meshes[MeshIndex];

            Meshes.push_back(ParseGLTFMesh(Mesh, Vertices, Indices));
        }

        size_t VertexDataSize = Vertices.size() * sizeof(SVertex);
        size_t IndexDataSize = Indices.size() * sizeof(uint16_t);
        size_t TotalSize = VertexDataSize + IndexDataSize;
        Rr_Buffer *StagingBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_STAGING);
        Rr_ReleaseBuffer(StagingBuffer);
        std::byte *StagingData = (std::byte *)Rr_GetMappedBufferData(StagingBuffer);
        SVertex *StagingVertices = (SVertex *)StagingData;
        uint16_t *StagingIndices = (uint16_t *)(StagingData + VertexDataSize);
        std::memcpy(StagingVertices, Vertices.data(), VertexDataSize);
        std::memcpy(StagingIndices, Indices.data(), IndexDataSize);

        GeometryBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_INDEX_BIT | RR_BUFFER_FLAGS_VERTEX_BIT);
        GeometryBufferIndexOffset = VertexDataSize;
        Rr_TransferNode *Node = Rr_AddTransferNode(Rr_GetGraph());
        Rr_TransferBufferData(Node, TotalSize, StagingBuffer, 0, GeometryBuffer, 0);

        CGLTFData = Data;
        CGLTFScene = Data->scene;

        Nodes.resize(CGLTFData->nodes_count);
    }

    void InitDepthImage(void)
    {
        Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());

        if (DepthImage != nullptr)
        {
            Rr_IntVec2 DepthImageSize = Rr_GetImage2DExtent(DepthImage);

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

    void SetAnimation(uint32_t Index)
    {
        AnimationTime = 0.0f;
        AnimationStart = std::numeric_limits<float>::max();
        AnimationEnd = std::numeric_limits<float>::min();
        auto Animation = &CGLTFData->animations[AnimationIndex];
        for (auto Index = 0; Index < Animation->channels_count; ++Index)
        {
            auto &Channel = Animation->channels[Index];
            auto &Sampler = Channel.sampler;
            assert(Sampler->input->has_min);
            AnimationStart = std::min(Sampler->input->min[0], AnimationStart);
            assert(Sampler->input->has_max);
            AnimationEnd = std::max(Sampler->input->max[0], AnimationEnd);
        }
    }

    Rr_Mat4 NodeTransform(cgltf_node *GLTFNode, Rr_Mat4 const &ParentTransform)
    {
        Rr_Mat4 Transform = ParentTransform;
        if (GLTFNode->has_matrix)
        {
            assert(false);
        }
        else
        {
            if (GLTFNode->has_translation)
            {
                Rr_Vec3 Translation = {
                    GLTFNode->translation[0],
                    GLTFNode->translation[1],
                    GLTFNode->translation[2],
                };
                Transform = Transform * Rr_TranslateV(Translation);
            }
            if (GLTFNode->has_rotation)
            {
                Rr_Quat Quat = {
                    GLTFNode->rotation[0],
                    GLTFNode->rotation[1],
                    GLTFNode->rotation[2],
                    GLTFNode->rotation[3],
                };
                Transform = Transform * Rr_QToM4(Quat);
            }
            if (GLTFNode->has_scale)
            {
                Rr_Vec3 Scale = {
                    GLTFNode->scale[0],
                    GLTFNode->scale[1],
                    GLTFNode->scale[2],
                };
                Transform = Transform * Rr_ScaleV(Scale);
            }
        }

        return Transform;
    }

    void DrawNode(Rr_GraphNode *GraphicsNode, cgltf_node *GLTFNode, Rr_Mat4 const &ParentTransform = Rr_M4D(1.0f))
    {
        Rr_Mat4 Transform = Nodes[cgltf_node_index(CGLTFData, GLTFNode)].Transform;

        if (GLTFNode->mesh)
        {
            auto MeshIndex = cgltf_mesh_index(CGLTFData, GLTFNode->mesh);
            auto &Mesh = Meshes[MeshIndex];
            for (auto Index = 0; Index < Mesh.Primitives.size(); ++Index)
            {
                auto &Primitive = Mesh.Primitives[Index];
                Rr_DrawIndexed(
                    GraphicsNode,
                    Primitive.IndexCount,
                    1,
                    Primitive.FirstIndex,
                    Primitive.VertexOffset,
                    Models.size());
                Models.emplace_back(Transform, Primitive.Color, Rr_IntV4(Bones.size() / 64, 1, 1, 1));
            }
        }

        auto Skin = GLTFNode->skin;
        if (Skin)
        {
            auto FirstJoint = Bones.size();
            Bones.resize(Bones.size() + 64);
            for (auto Index = 0; Index < Skin->joints_count; ++Index)
            {
                auto JointNode = Skin->joints[Index];
                auto JointNodeIndex = cgltf_node_index(CGLTFData, JointNode);

                Rr_Mat4 InverseBind;
                cgltf_accessor_read_float(Skin->inverse_bind_matrices, Index, &InverseBind.Elements[0][0], 16);
                auto BoneMatrix =
                    Rr_MulM4(Rr_InvGeneralM4(Transform), Rr_MulM4(Nodes[JointNodeIndex].Transform, InverseBind));
                Bones[FirstJoint + Index] = BoneMatrix;
            }
        }

        for (auto Index = 0; Index < GLTFNode->children_count; ++Index)
        {
            DrawNode(GraphicsNode, GLTFNode->children[Index], Transform);
        }
    }

    void UpdateAnimation()
    {
        if (AnimationTime > AnimationEnd)
        {
            AnimationTime -= AnimationTime;
        }

        auto Animation = &CGLTFData->animations[AnimationIndex];
        for (auto Index = 0; Index < Animation->channels_count; ++Index)
        {
            auto &Channel = Animation->channels[Index];
            auto Sampler = Channel.sampler;

            auto &Node = Nodes[cgltf_node_index(CGLTFData, Channel.target_node)];
            Node.Animated = true;

            auto KeyframeCount = Sampler->input->count;
            auto KeyframeFromIndex = 0;
            auto KeyframeToIndex = KeyframeCount;
            auto KeyframeFrom = 0.0f;
            auto KeyframeTo = 0.0f;
            for (auto KeyframeIndex = 0; KeyframeIndex < KeyframeCount - 1; ++KeyframeIndex)
            {
                cgltf_accessor_read_float(Sampler->input, KeyframeIndex + 1, &KeyframeTo, 1);
                if (KeyframeTo >= AnimationTime)
                {
                    KeyframeToIndex = KeyframeIndex + 1;

                    break;
                }
            }
            if (KeyframeToIndex == 0)
            {
                KeyframeFromIndex = KeyframeCount - 1;
            }
            KeyframeFromIndex = KeyframeToIndex - 1;
            cgltf_accessor_read_float(Sampler->input, KeyframeFromIndex, &KeyframeFrom, 1);

            auto Alpha = (AnimationTime - KeyframeFrom) / (KeyframeTo - KeyframeFrom);
            if (Sampler->interpolation == cgltf_interpolation_type_step)
            {
                Alpha = std::roundf(Alpha);
            }
            else if (Sampler->interpolation == cgltf_interpolation_type_cubic_spline)
            {
                assert(false);
            }

            if (Channel.target_path == cgltf_animation_path_type_translation)
            {
                Rr_Vec3 TranslationFrom;
                cgltf_accessor_read_float(Sampler->output, KeyframeFromIndex, TranslationFrom.Elements, 3);
                Rr_Vec3 TranslationTo;
                cgltf_accessor_read_float(Sampler->output, KeyframeToIndex, TranslationTo.Elements, 3);
                Node.AnimatedTranslation = Rr_LerpV3(TranslationFrom, Alpha, TranslationTo);
            }
            else if (Channel.target_path == cgltf_animation_path_type_rotation)
            {
                Rr_Quat QuatFrom;
                cgltf_accessor_read_float(Sampler->output, KeyframeFromIndex, QuatFrom.Elements, 4);
                Rr_Quat QuatTo;
                cgltf_accessor_read_float(Sampler->output, KeyframeToIndex, QuatTo.Elements, 4);
                Node.AnimatedRotation = Rr_SLerp(QuatFrom, Alpha, QuatTo);
            }
            else if (Channel.target_path == cgltf_animation_path_type_scale)
            {
                Rr_Vec3 ScaleFrom;
                cgltf_accessor_read_float(Sampler->output, KeyframeFromIndex, ScaleFrom.Elements, 3);
                Rr_Vec3 ScaleTo;
                cgltf_accessor_read_float(Sampler->output, KeyframeToIndex, ScaleTo.Elements, 3);
                Node.AnimatedScale = Rr_LerpV3(ScaleFrom, Alpha, ScaleTo);
            }
        }

        AnimationTime += Rr_GetDeltaSeconds();
    }

    void ProcessNodes(cgltf_node *GLTFNode, Rr_Mat4 const &ParentTransform = Rr_M4D(1.0f))
    {
        auto &Node = Nodes[cgltf_node_index(CGLTFData, GLTFNode)];

        Rr_Mat4 Transform;
        if (Node.Animated)
        {
            Transform = ParentTransform;
            Transform = Transform * Rr_TranslateV(Node.AnimatedTranslation);
            Transform = Transform * Rr_QToM4(Node.AnimatedRotation);
            Transform = Transform * Rr_ScaleV(Node.AnimatedScale);
        }
        else
        {
            Transform = NodeTransform(GLTFNode, ParentTransform);
        }

        Node.GLTFNode = GLTFNode;
        Node.Transform = Transform;

        for (auto Index = 0; Index < GLTFNode->children_count; ++Index)
        {
            ProcessNodes(GLTFNode->children[Index], Transform);
        }
    }

public:
    CGLTFAnimation()
    {
        std::array VertexAttributes = {
            Rr_VertexInputAttribute{ .Location = 0, .Format = RR_FORMAT_FLOAT3 },
            Rr_VertexInputAttribute{ .Location = 1, .Format = RR_FORMAT_FLOAT3 },
            Rr_VertexInputAttribute{ .Location = 2, .Format = RR_FORMAT_UINT4 },
            Rr_VertexInputAttribute{ .Location = 3, .Format = RR_FORMAT_FLOAT4 },
        };

        std::array VertexInputBindings = {
            Rr_VertexInputBinding{
                .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
                .AttributeCount = VertexAttributes.size(),
                .Attributes = VertexAttributes.data(),
            },
        };

        std::array ColorTargets = { Rr_ColorTargetInfo{
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        } };

        Rr_Asset VertexShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFANIMATION_VERT_SPV);
        Rr_ShaderInfo VertexShaderInfo = {
            .SPVSize = VertexShader.Size,
            .SPVData = VertexShader.Data,
        };

        Rr_Asset FragmentShader = Rr_LoadAsset(EXAMPLE_ASSET_GLTFANIMATION_FRAG_SPV);
        Rr_ShaderInfo FragmentShaderInfo = {
            .SPVSize = FragmentShader.Size,
            .SPVData = FragmentShader.Data,
        };

        Rr_GraphicsPipelineCreateInfo PipelineInfo = {
            .VertexShaderInfo = &VertexShaderInfo,
            .FragmentShaderInfo = &FragmentShaderInfo,
            .VertexInputBindingCount = VertexInputBindings.size(),
            .VertexInputBindings = VertexInputBindings.data(),
            .ColorTargetCount = ColorTargets.size(),
            .ColorTargets = ColorTargets.data(),
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

        UniformBuffer = Rr_CreateBuffer(sizeof(GPUUniform), RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_DYNAMIC);

        InitGLTFScene();

        InitDepthImage();

        ModelBuffer = Rr_CreateBuffer(RR_MEBIBYTES(4), RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_DYNAMIC);
        SkinBuffer = Rr_CreateBuffer(RR_MEBIBYTES(4), RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_DYNAMIC);

        SetAnimation(AnimationIndex);
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
        if (Rr_UIBeginWindowEx("GLTFAnimation.cxx", nullptr, RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT))
        {
            Rr_UIText("This example shows using cGLTF to load and draw animated meshes.");
            Rr_UIInputFloat("Animation Time", &AnimationTime);
            auto AnimationNames = std::vector<char const *>{};
            AnimationNames.reserve(CGLTFData->animations_count);
            for (auto Index = 0; Index < CGLTFData->animations_count; ++Index)
            {
                AnimationNames.push_back(CGLTFData->animations[Index].name);
            }
            if (Rr_UICombobox(
                    "Animation",
                    (uint32_t)CGLTFData->animations_count,
                    AnimationNames.data(),
                    &AnimationIndex))
            {
                SetAnimation(AnimationIndex);
            }
        }
        Rr_UIEndWindow();
        Rr_UIEndDebugOverlayTabs();

        auto SwapchainImage = Rr_GetSwapchainImage();
        auto SwapchainSize = Rr_GetImage2DExtent(SwapchainImage);
        auto SwapchainAspect = Rr_GetImage2DAspect(SwapchainImage);

        Camera.UpdatePerspective(SwapchainAspect);
        Camera.Update();

        GPUUniform.View = Camera.GetViewMatrix();
        GPUUniform.Projection = Camera.ProjMatrix;
        GPUUniform.Time = (float)Rr_GetTimeSeconds();
        memcpy(Rr_GetMappedBufferData(UniformBuffer), &GPUUniform, sizeof(GPUUniform));

        Rr_ColorTarget ColorTarget = {
            .Image = SwapchainImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = Rr_ColorClear{ { 0.03f, 0.03f, 0.04f, 1.0f } },
        };
        Rr_DepthTarget DepthTarget = {
            .Image = DepthImage,
            .LoadOp = RR_LOAD_OP_CLEAR,
            .StoreOp = RR_STORE_OP_STORE,
            .Clear = {
                .Depth = 1.0f,
            },
        };
        Rr_GraphNode *GraphicsNode = Rr_AddGraphicsNode(Rr_GetGraph(), 1, &ColorTarget, &DepthTarget);
        Rr_BindGraphicsPipeline(GraphicsNode, GraphicsPipeline);
        Rr_BindVertexBuffer(GraphicsNode, GeometryBuffer, 0, 0);
        Rr_BindIndexBuffer(GraphicsNode, GeometryBuffer, 0, GeometryBufferIndexOffset, RR_INDEX_TYPE_UINT16);
        Rr_BindUniformBuffer(GraphicsNode, UniformBuffer, 0, 0, 0, sizeof(GPUUniform));
        Rr_BindStorageBuffer(GraphicsNode, ModelBuffer, 1, 0, 0, Rr_GetBufferSize(ModelBuffer));
        Rr_BindStorageBuffer(GraphicsNode, SkinBuffer, 2, 0, 0, Rr_GetBufferSize(SkinBuffer));

        Bones.clear();
        Models.clear();

        UpdateAnimation();

        for (auto Index = 0; Index < CGLTFScene->nodes_count; ++Index)
        {
            ProcessNodes(CGLTFScene->nodes[Index]);
        }

        for (auto Index = 0; Index < CGLTFScene->nodes_count; ++Index)
        {
            DrawNode(GraphicsNode, CGLTFScene->nodes[Index]);
        }

        std::memcpy(Rr_GetMappedBufferData(ModelBuffer), Models.data(), sizeof(SGPUModel) * Models.size());
        std::memcpy(Rr_GetMappedBufferData(SkinBuffer), Bones.data(), sizeof(Rr_Mat4) * Bones.size());
    }

    ~CGLTFAnimation()
    {
        Rr_ReleaseBuffer(SkinBuffer);
        Rr_ReleaseBuffer(ModelBuffer);
        Rr_ReleaseBuffer(GeometryBuffer);
        Rr_ReleaseImage(DepthImage);
        Rr_ReleaseBuffer(UniformBuffer);
        Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
    }
};

int main()
{
    static CGLTFAnimation *App{};

    Rr_Config Config = {
        .WindowTitle = "GLTFAnimation",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = []() { App = new CGLTFAnimation(); },
        .EventFunc = [](Rr_Event const *Event) { App->Event(Event); },
        .IterateFunc = []() { App->Iterate(); },
        .CleanupFunc = []() { delete App; },
    };
    Rr_Run(&Config);
}
