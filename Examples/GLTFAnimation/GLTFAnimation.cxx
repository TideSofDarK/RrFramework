#include "ExampleAssets.inc"

#include <Rr/Rr.h>

#define CGLTF_IMPLEMENTATION
#include "../../Vendor/cgltf/cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../Vendor/stb/stb_image.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

struct SOrbitCamera
{
    float FOVDegrees{ 90.0f };
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
        ProjMatrix = Rr_Perspective_RH(RR_ANGLE_DEG(FOVDegrees), Aspect, 0.1f, 100.0f);
        ProjMatrix.Elements[1][1] *= -1.0f;
    }

    void Update()
    {
        auto MouseDelta = Rr_GetMousePositionDelta();

        if (Rr_GetMouseState() & RR_MOUSE_BUTTON_RIGHT_BIT)
        {
            Rr_SetRelativeMouseMode(true);

            auto constexpr SENSITIVITY = 0.05f;
            Distance += MouseDelta.Y * SENSITIVITY;
        }
        else if (Rr_GetMouseState() & RR_MOUSE_BUTTON_LEFT_BIT)
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
                auto constexpr SENSITIVITY = 0.2f;
                Yaw -= MouseDelta.X * SENSITIVITY;
                Pitch -= MouseDelta.Y * SENSITIVITY;
            }
        }
        else
        {
            Rr_SetRelativeMouseMode(false);
        }

        Yaw = Rr_WrapMax(Yaw, 360.0f);
        Pitch = RR_CLAMP(-90.0f, Pitch, 90.0f);

        Transform = Rr_Translate(Center) * Rr_Rotate_RH(RR_ANGLE_DEG(Yaw), Rr_V3(0.0f, 1.0f, 0.0f)) *
                    Rr_Rotate_RH(RR_ANGLE_DEG(Pitch), Rr_V3(1.0f, 0.0f, 0.0f)) * Rr_TranslateV(0.0f, 0.0f, Distance);
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

static Rr_Image2D *DepthAttachment;
static Rr_Buffer *UniformBuffer;
static Rr_GraphicsPipeline *GraphicsPipeline;
// static Rr_Sampler *Sampler;

static Rr_Buffer *ModelBuffer;
static Rr_Buffer *SkinBuffer;
static Rr_Buffer *GeometryBuffer;
static size_t GeometryBufferIndexOffset;
static Rr_Buffer *AnimationBuffer;
static cgltf_data *CGLTFData;
static cgltf_scene *CGLTFScene;
static std::vector<SMesh> Meshes;
static uint32_t AnimationIndex = 0;
static float AnimationTime = 0.0f;
static float AnimationStart = 0.0f;
static float AnimationEnd = 0.0f;

static SGPUUniform GPUUniform;

static SOrbitCamera Camera;

static SMesh ParseGLTFMesh(cgltf_mesh *GLTFMesh, std::vector<SVertex> &OutVertices, std::vector<uint16_t> &OutIndices)
{
    SMesh Mesh = {};
    Mesh.Primitives.resize(GLTFMesh->primitives_count);

    for (size_t PrimitiveIndex = 0; PrimitiveIndex < GLTFMesh->primitives_count; ++PrimitiveIndex)
    {
        cgltf_primitive *GLTFPrimitive = &GLTFMesh->primitives[PrimitiveIndex];
        SPrimitive *Primitive = &Mesh.Primitives[PrimitiveIndex];

        cgltf_accessor const *PositionAccessor = cgltf_find_accessor(GLTFPrimitive, cgltf_attribute_type_position, 0);
        assert(PositionAccessor);
        cgltf_accessor const *NormalAccessor = cgltf_find_accessor(GLTFPrimitive, cgltf_attribute_type_normal, 0);
        assert(NormalAccessor);
        cgltf_accessor const *JointsAccessor = cgltf_find_accessor(GLTFPrimitive, cgltf_attribute_type_joints, 0);
        if (JointsAccessor)
        {
            assert(JointsAccessor->type == cgltf_type_vec4);
        }
        cgltf_accessor const *WeightsAccessor = cgltf_find_accessor(GLTFPrimitive, cgltf_attribute_type_weights, 0);
        if (WeightsAccessor)
        {
            assert(WeightsAccessor->type == cgltf_type_vec4);
        }

        auto VertexCount = PositionAccessor->count;
        auto VertexOffset = OutVertices.size();
        OutVertices.resize(OutVertices.size() + VertexCount);
        for (size_t Index = 0; Index < VertexCount; ++Index)
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

        cgltf_accessor const *IndexAccessor = GLTFPrimitive->indices;
        size_t FirstIndex = OutIndices.size();
        OutIndices.resize(OutIndices.size() + IndexAccessor->count);
        cgltf_accessor_unpack_indices(IndexAccessor, &OutIndices[FirstIndex], sizeof(uint16_t), IndexAccessor->count);

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

static void InitGLTFScene(void)
{
    Rr_Asset LoadedAsset = Rr_LoadAsset(EXAMPLE_ASSET_ROBOT_GLB);

    cgltf_options Options = {};
    cgltf_data *Data = NULL;
    cgltf_result Result = cgltf_parse(&Options, LoadedAsset.Data, LoadedAsset.Size, &Data);
    assert(Result == cgltf_result_success);
    cgltf_load_buffers(&Options, Data, NULL);

    assert(Data->scene);
    assert(Data->meshes);

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
    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(TotalSize, RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
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
}

static void InitDepthImage(void)
{
    Rr_IntVec2 SwapchainSize = Rr_GetImage2DExtent(Rr_GetSwapchainImage());

    if (DepthAttachment != NULL)
    {
        Rr_IntVec2 DepthImageSize = Rr_GetImage2DExtent(DepthAttachment);

        if (DepthImageSize.X >= SwapchainSize.X && DepthImageSize.Y >= SwapchainSize.Y)
        {
            return;
        }

        Rr_ReleaseImage(DepthAttachment);
    }

    DepthAttachment = Rr_CreateImage2D(
        Rr_IntV2(SwapchainSize.Width, SwapchainSize.Height),
        RR_IMAGE_FORMAT_D32_SFLOAT,
        RR_IMAGE_FLAGS_DEPTH_STENCIL_ATTACHMENT_BIT | RR_IMAGE_FLAGS_TRANSFER_BIT);
}

static void SetAnimation(uint32_t Index)
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

static void Init(void)
{
    Rr_VertexInputAttribute VertexAttributes[] = {
        { .Location = 0, .Format = RR_FORMAT_FLOAT3 },
        { .Location = 1, .Format = RR_FORMAT_FLOAT3 },
        { .Location = 2, .Format = RR_FORMAT_UINT4 },
        { .Location = 3, .Format = RR_FORMAT_FLOAT4 },
    };

    Rr_VertexInputBinding VertexInputBindings[] = {
        {
            .Rate = RR_VERTEX_INPUT_RATE_VERTEX,
            .AttributeCount = std::size(VertexAttributes),
            .Attributes = VertexAttributes,
        },
    };

    Rr_ColorTargetInfo ColorTargets[1] = {
        {
            .Format = Rr_GetImageFormat(Rr_GetSwapchainImage()),
        },
    };

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
        .VertexInputBindingCount = std::size(VertexInputBindings),
        .VertexInputBindings = VertexInputBindings,
        .ColorTargetCount = std::size(ColorTargets),
        .ColorTargets = ColorTargets,
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

    UniformBuffer = Rr_CreateBuffer(
        sizeof(GPUUniform),
        RR_BUFFER_FLAGS_UNIFORM_BIT | RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    InitGLTFScene();

    InitDepthImage();

    ModelBuffer = Rr_CreateBuffer(
        RR_MEBIBYTES(4),
        RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);
    SkinBuffer = Rr_CreateBuffer(
        RR_MEBIBYTES(4),
        RR_BUFFER_FLAGS_STORAGE_BIT | RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT |
            RR_BUFFER_FLAGS_PER_FRAME_BIT);

    SetAnimation(AnimationIndex);
}

static void Event(Rr_Event const *Event)
{
    switch (Event->Type)
    {
        case RR_EVENT_TYPE_SWAPCHAIN_CREATED:
        {
            InitDepthImage();
        }
        default:
            return;
    }
}

static Rr_Mat4 NodeTransform(cgltf_node *GLTFNode, Rr_Mat4 const &ParentTransform)
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
            Transform = Transform * Rr_Translate(Translation);
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
            Transform = Transform * Rr_Scale(Scale);
        }
    }

    return Transform;
}

static void DrawNode(
    Rr_GraphNode *GraphicsNode,
    cgltf_node *GLTFNode,
    std::vector<SGPUModel> &Models,
    std::vector<SNode> &Nodes,
    std::vector<Rr_Mat4> &Bones,
    Rr_Mat4 const &ParentTransform = Rr_M4D(1.0f))
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
        DrawNode(GraphicsNode, GLTFNode->children[Index], Models, Nodes, Bones, Transform);
    }
}

static void UpdateAnimation(std::vector<SNode> &Nodes)
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

static void ProcessNodes(cgltf_node *GLTFNode, std::vector<SNode> &Nodes, Rr_Mat4 const &ParentTransform = Rr_M4D(1.0f))
{
    auto &Node = Nodes[cgltf_node_index(CGLTFData, GLTFNode)];

    Rr_Mat4 Transform;
    if (Node.Animated)
    {
        Transform = ParentTransform;
        Transform = Transform * Rr_Translate(Node.AnimatedTranslation);
        Transform = Transform * Rr_QToM4(Node.AnimatedRotation);
        Transform = Transform * Rr_Scale(Node.AnimatedScale);
    }
    else
    {
        Transform = NodeTransform(GLTFNode, ParentTransform);
    }

    Node.GLTFNode = GLTFNode;
    Node.Transform = Transform;

    for (auto Index = 0; Index < GLTFNode->children_count; ++Index)
    {
        ProcessNodes(GLTFNode->children[Index], Nodes, Transform);
    }
}

static void Iterate(void)
{
    Rr_UIBeginDebugOverlayTabs();
    if (Rr_UIBeginWindowEx("GLTFAnimation.cxx", NULL, RR_UI_WINDOW_FLAGS_UNDOCKABLE_BIT))
    {
        Rr_UIText("This example demonstrates using cGLTF\nto load and draw animated meshes.");
        auto Time = (float)Rr_GetTimeSeconds();
        Rr_UIInputFloat("Time", &Time);
        auto AnimationNames = std::vector<char const *>{};
        AnimationNames.reserve(CGLTFData->animations_count);
        for (auto Index = 0; Index < CGLTFData->animations_count; ++Index)
        {
            AnimationNames.push_back(CGLTFData->animations[Index].name);
        }
        if (Rr_UICombobox("Animation", (uint32_t)CGLTFData->animations_count, AnimationNames.data(), &AnimationIndex))
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
        .Clear = (Rr_ColorClear){ { 0.03f, 0.03f, 0.04f, 1.0f } },
    };
    Rr_DepthTarget DepthTarget = {
        .Image = DepthAttachment,
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

    static std::vector<SNode> Nodes;
    Nodes.clear();
    Nodes.resize(CGLTFData->nodes_count);
    static std::vector<Rr_Mat4> Bones;
    Bones.clear();
    static std::vector<SGPUModel> Models;
    Models.clear();

    assert(CGLTFData->skins_count == 1);

    UpdateAnimation(Nodes);

    for (auto Index = 0; Index < CGLTFScene->nodes_count; ++Index)
    {
        ProcessNodes(CGLTFScene->nodes[Index], Nodes);
    }

    for (auto Index = 0; Index < CGLTFScene->nodes_count; ++Index)
    {
        DrawNode(GraphicsNode, CGLTFScene->nodes[Index], Models, Nodes, Bones);
    }

    std::memcpy(Rr_GetMappedBufferData(ModelBuffer), Models.data(), sizeof(SGPUModel) * Models.size());
    std::memcpy(Rr_GetMappedBufferData(SkinBuffer), Bones.data(), sizeof(Rr_Mat4) * Bones.size());
}

static void Cleanup(void)
{
    Rr_ReleaseBuffer(SkinBuffer);
    Rr_ReleaseBuffer(ModelBuffer);
    Rr_ReleaseBuffer(GeometryBuffer);
    Rr_ReleaseImage(DepthAttachment);
    Rr_ReleaseBuffer(UniformBuffer);
    Rr_ReleaseGraphicsPipeline(GraphicsPipeline);
}

int main(int ArgC, char **ArgV)
{
    Rr_Config Config = {
        .WindowTitle = "GLTFAnimation",
        .WindowFlags = RR_WINDOW_FLAGS_RESIZE_BIT,
        .InitFunc = Init,
        .EventFunc = Event,
        .IterateFunc = Iterate,
        .CleanupFunc = Cleanup,
    };
    Rr_Run(&Config);

    return 0;
}
